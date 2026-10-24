// mermaid_cache_impl.cpp - impl unit for loom.ui.visual.mermaid (RFC 0031
// E6b). Bodies of the exported per-instance LayoutRowsCache. The cache is
// the only caller added around the detail layout seam; it stores immutable
// string rows, never ftxui::Element.
module;

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid {
namespace {

// FNV-1a (64-bit) over the EXACT source bytes (byte-for-byte, including
// CRLF; no trim/normalize), with the use_ascii bit folded in as a trailing
// 0/1 byte. Width and theme deliberately do not participate: row layout is
// width/theme independent (width flips happen inside MermaidNode, dim is
// applied by the caller).
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] std::uint64_t layout_cache_hash(std::string_view source, bool use_ascii) {
    std::uint64_t hash = kFnvOffsetBasis;
    for (const char raw : source) {
        hash ^= static_cast<unsigned char>(raw);
        hash *= kFnvPrime;
    }
    hash ^= static_cast<unsigned char>(use_ascii ? 1u : 0u);
    hash *= kFnvPrime;
    return hash;
}

[[nodiscard]] std::size_t row_bytes(const std::vector<std::string>& rows) {
    std::size_t total = 0;
    for (const auto& row : rows) {
        if (total > std::numeric_limits<std::size_t>::max() - row.size())
            return std::numeric_limits<std::size_t>::max();
        total += row.size();
    }
    return total;
}

} // namespace

struct LayoutRowsCache::Impl {
    struct Entry {
        std::string source;             // exact retained key bytes
        std::uint64_t hash = 0;         // FNV-1a of source + ascii bit
        bool use_ascii = false;
        std::vector<std::string> rows;  // present-but-empty = rejection sentinel
        std::size_t bytes = 0;
        [[nodiscard]] bool matches(std::uint64_t h, std::string_view src, bool ascii) const
            noexcept {
            return hash == h && use_ascii == ascii && source == src;
        }
    };

    std::size_t max_entries;
    std::size_t max_bytes;
    std::size_t total_bytes = 0;
    std::size_t hit_count = 0;
    std::size_t miss_count = 0;
    // Back of the list is most-recently-used; the front is evicted first.
    // The hash index points at LRU entries; a hash-index hit is accepted
    // only after an exact (source bytes, use_ascii) comparison, so a 64-bit
    // collision can never serve one source's rows for another's.
    std::list<Entry> lru;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> index;

    Impl(std::size_t entries, std::size_t bytes)
        : max_entries(entries), max_bytes(bytes) {}
};

LayoutRowsCache::LayoutRowsCache(std::size_t max_entries, std::size_t max_bytes)
    : impl_(std::make_unique<Impl>(max_entries, max_bytes)) {}

LayoutRowsCache::~LayoutRowsCache() = default;

// Deep copy: the unordered_map holds iterators INTO lru, so a memberwise
// copy would alias the source list. Walk LRU order, copy the nodes, then
// rebuild the hash index against the fresh iterators.
LayoutRowsCache::LayoutRowsCache(const LayoutRowsCache& other)
    : impl_(std::make_unique<Impl>(other.impl_->max_entries,
                                   other.impl_->max_bytes)) {
    for (const auto& node : other.impl_->lru) impl_->lru.push_back(node);
    for (auto node = impl_->lru.begin(); node != impl_->lru.end(); ++node)
        impl_->index.emplace(node->hash, node);
    impl_->total_bytes = other.impl_->total_bytes;
    impl_->hit_count = other.impl_->hit_count;
    impl_->miss_count = other.impl_->miss_count;
}

LayoutRowsCache& LayoutRowsCache::operator=(const LayoutRowsCache& other) {
    if (this != &other) {
        LayoutRowsCache copied(other);
        impl_.swap(copied.impl_);
    }
    return *this;
}

LayoutRowsCache::LayoutRowsCache(LayoutRowsCache&&) noexcept = default;
LayoutRowsCache& LayoutRowsCache::operator=(LayoutRowsCache&&) noexcept = default;

std::optional<std::vector<std::string>> LayoutRowsCache::get_or_layout(
    std::string_view source, bool use_ascii) {
    const std::uint64_t hash = layout_cache_hash(source, use_ascii);
    if (const auto found = impl_->index.find(hash);
        found != impl_->index.end() &&
        found->second->matches(hash, source, use_ascii)) {
        // Exact key match (source bytes + ascii bit); move to the MRU end and
        // hand out a copy of the immutable rows. An empty vector reproduces
        // the cached rejection through the caller's finish path.
        impl_->lru.splice(impl_->lru.end(), impl_->lru, found->second);
        ++impl_->hit_count;
        return found->second->rows;
    }
    ++impl_->miss_count;

    // Fresh layout through the detail seam. Nullopt is normalized to the
    // present-but-empty rejection sentinel before retention.
    std::vector<std::string> fresh =
        detail::layout_mermaid_rows(source, use_ascii).value_or(std::vector<std::string>{});
    const std::size_t fresh_bytes = row_bytes(fresh);

    // A hash slot occupied by a DIFFERENT source (64-bit collision) cannot
    // retain the fresh entry without changing the index shape; the current
    // call is still served correctly, just uncached.
    const bool slot_occupied = [&] {
        const auto slot = impl_->index.find(hash);
        return slot != impl_->index.end() &&
               !slot->second->matches(hash, source, use_ascii);
    }();

    // Make room LRU-first until the fresh entry fits; an entry that alone
    // exceeds the byte cap (or a zero entry capacity) is served but not
    // retained, so a single oversized diagram can never evict the working
    // set on every call.
    while (!slot_occupied && !impl_->lru.empty() &&
           (impl_->lru.size() >= impl_->max_entries ||
            impl_->total_bytes + fresh_bytes > impl_->max_bytes)) {
        auto victim = impl_->lru.begin();
        impl_->total_bytes -= victim->bytes;
        impl_->index.erase(victim->hash);
        impl_->lru.erase(victim);
    }
    if (slot_occupied || impl_->max_entries == 0u || fresh_bytes > impl_->max_bytes)
        return fresh;

    impl_->lru.push_back(
        Impl::Entry{std::string(source), hash, use_ascii, std::move(fresh), fresh_bytes});
    auto inserted = std::prev(impl_->lru.end());
    impl_->index.emplace(hash, inserted);
    impl_->total_bytes += fresh_bytes;
    return inserted->rows;
}

void LayoutRowsCache::clear() {
    impl_->lru.clear();
    impl_->index.clear();
    impl_->total_bytes = 0;
    impl_->hit_count = 0;
    impl_->miss_count = 0;
}

std::size_t LayoutRowsCache::size() const noexcept { return impl_->index.size(); }
std::size_t LayoutRowsCache::byte_size() const noexcept { return impl_->total_bytes; }
std::size_t LayoutRowsCache::entry_capacity() const noexcept { return impl_->max_entries; }
std::size_t LayoutRowsCache::byte_capacity() const noexcept { return impl_->max_bytes; }
std::size_t LayoutRowsCache::hits() const noexcept { return impl_->hit_count; }
std::size_t LayoutRowsCache::misses() const noexcept { return impl_->miss_count; }

} // namespace loom::ui::mermaid
