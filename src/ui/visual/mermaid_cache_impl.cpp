// mermaid_cache_impl.cpp - impl unit for loom.ui.visual.mermaid (RFC 0031
// E6b). Bodies of the exported per-instance LayoutRowsCache. The cache is
// the only caller added around the detail layout seam; it stores immutable
// rows and optional packed semantic roles, never ftxui::Element.
module;

#include <ftxui/dom/node.hpp>

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
        detail::LayoutPayload payload; // empty rows AND plane = rejection sentinel
        std::size_t style_bytes = 0;
        std::size_t bytes = 0;
        [[nodiscard]] bool matches(std::uint64_t h, std::string_view src, bool ascii) const
            noexcept {
            return hash == h && use_ascii == ascii && source == src;
        }
    };

    std::size_t max_entries;
    std::size_t max_bytes;
    std::size_t total_bytes = 0;
    std::size_t total_style_bytes = 0;
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
    detail::LayoutPayload lookup(std::string_view source, bool use_ascii);
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
    impl_->total_style_bytes = other.impl_->total_style_bytes;
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

detail::LayoutPayload LayoutRowsCache::Impl::lookup(std::string_view source, bool use_ascii) {
    const std::uint64_t hash = layout_cache_hash(source, use_ascii);
    if (const auto found = index.find(hash);
        found != index.end() &&
        found->second->matches(hash, source, use_ascii)) {
        // Exact key match (source bytes + ascii bit); move to the MRU end and
        // hand out a copy of the immutable rows. An empty vector reproduces
        // the cached rejection through the caller's finish path.
        lru.splice(lru.end(), lru, found->second);
        ++hit_count;
        return found->second->payload;
    }
    ++miss_count;

    // Fresh layout through the detail seam. Nullopt is normalized to the
    // present-but-empty rejection sentinel before retention.
    auto fresh = detail::layout_mermaid_payload(source, use_ascii).value_or(detail::LayoutPayload{});
    const std::size_t fresh_bytes = row_bytes(fresh.rows);
    const std::size_t fresh_styles = fresh.styles.size() * sizeof(std::uint16_t);

    // A hash slot occupied by a DIFFERENT source (64-bit collision) cannot
    // retain the fresh entry without changing the index shape; the current
    // call is still served correctly, just uncached.
    const bool slot_occupied = [&] {
        const auto slot = index.find(hash);
        return slot != index.end() &&
               !slot->second->matches(hash, source, use_ascii);
    }();

    // Make room LRU-first until the fresh entry fits; an entry that alone
    // exceeds the byte cap (or a zero entry capacity) is served but not
    // retained, so a single oversized diagram can never evict the working
    // set on every call.
    while (!slot_occupied && !lru.empty() &&
           (lru.size() >= max_entries ||
            fresh_bytes > max_bytes - total_bytes)) {
        auto victim = lru.begin();
        total_bytes -= victim->bytes;
        total_style_bytes -= victim->style_bytes;
        index.erase(victim->hash);
        lru.erase(victim);
    }
    if (slot_occupied || max_entries == 0u || fresh_bytes > max_bytes ||
        fresh_styles > std::numeric_limits<std::size_t>::max() - total_style_bytes)
        return fresh;

    lru.push_back(
        Impl::Entry{std::string(source), hash, use_ascii, std::move(fresh), fresh_styles, fresh_bytes});
    auto inserted = std::prev(lru.end());
    index.emplace(hash, inserted);
    total_bytes += fresh_bytes;
    total_style_bytes += fresh_styles;
    return inserted->payload;
}

std::optional<std::vector<std::string>> LayoutRowsCache::get_or_layout(
    std::string_view source, bool use_ascii) {
    return impl_->lookup(source, use_ascii).rows;
}

std::optional<ftxui::Element> LayoutRowsCache::render(
    std::string_view source, bool use_ascii, ftxui::Element fallback) {
    if (!fallback) return {};
    return detail::finish_mermaid_payload(impl_->lookup(source, use_ascii), std::move(fallback));
}

void LayoutRowsCache::clear() {
    impl_->lru.clear();
    impl_->index.clear();
    impl_->total_bytes = 0;
    impl_->total_style_bytes = 0;
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
