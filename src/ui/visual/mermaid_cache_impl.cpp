// mermaid_cache_impl.cpp - impl unit for loom.ui.visual.mermaid (RFC 0031
// E6b). Bodies of the exported per-instance LayoutRowsCache. The cache is
// the only caller added around the detail layout seam; it stores immutable
// owning text/semantic rows, never ftxui::Element.
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

}  // namespace

struct LayoutRowsCache::Impl {
    struct Entry {
        std::string source;      // exact retained key bytes
        std::uint64_t hash = 0;  // FNV-1a of source + ascii bit
        bool use_ascii = false;
        std::vector<std::string> rows;  // present-but-empty = rejection sentinel
        std::optional<detail::StyledRows> styles;
        std::size_t layout_work = 0;
        std::vector<std::uint16_t> packed_styles{};
        // Metered packed family (ADR 0032 ER; D-035 batch 1 Block): on a hit
        // the Element tree is rebuilt and ElementWork is charged afresh;
        // payload_prepaid holds the ER post-gate bookkeeping remainder.
        detail::PackedMeter packed_meter = detail::PackedMeter::Off;
        std::size_t payload_prepaid = 0u;
        std::size_t bytes = 0;

        [[nodiscard]] bool matches(std::uint64_t h, std::string_view src,
                                   bool ascii) const noexcept {
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

    std::optional<detail::NativeLayout> resolve(std::string_view source, bool use_ascii,
                                                detail::Work& work);

    Impl(std::size_t entries, std::size_t bytes)
        : max_entries(entries)
        , max_bytes(bytes) {}
};

LayoutRowsCache::LayoutRowsCache(std::size_t max_entries, std::size_t max_bytes)
    : impl_(std::make_unique<Impl>(max_entries, max_bytes)) {}

LayoutRowsCache::~LayoutRowsCache() = default;

// Deep copy: the unordered_map holds iterators INTO lru, so a memberwise
// copy would alias the source list. Walk LRU order, copy the nodes, then
// rebuild the hash index against the fresh iterators.
LayoutRowsCache::LayoutRowsCache(const LayoutRowsCache& other)
    : impl_(std::make_unique<Impl>(other.impl_->max_entries, other.impl_->max_bytes)) {
    for (const auto& node : other.impl_->lru)
        impl_->lru.push_back(node);
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

std::optional<detail::NativeLayout>
LayoutRowsCache::Impl::resolve(std::string_view source, bool use_ascii, detail::Work& work) {
    // Preview layouts must never enter or reuse the normal cache namespace.
    if (detail::flow_style_preview_active())
        return detail::layout_mermaid_content(source, use_ascii, work);
    const std::uint64_t hash = layout_cache_hash(source, use_ascii);
    if (const auto found = index.find(hash);
        found != index.end() && found->second->matches(hash, source, use_ascii)) {
        const auto& entry = *found->second;
        if ((entry.styles || !entry.packed_styles.empty()) && !work.spend(entry.layout_work))
            return {};
        lru.splice(lru.end(), lru, found->second);
        ++hit_count;
        return detail::NativeLayout{entry.rows,
                                    entry.styles,
                                    entry.layout_work,
                                    entry.packed_styles,
                                    entry.packed_meter,
                                    entry.payload_prepaid};
    }
    ++miss_count;
    auto computed = detail::layout_mermaid_content(source, use_ascii, work);
    // Exhaustion is a call-local outcome, never a semantic rejection cache entry.
    if (!computed && work.remaining == 0u) return {};
    auto fresh = computed ? std::move(*computed) : detail::NativeLayout{};
    const auto measured = (fresh.styles || !fresh.packed_styles.empty())
                              ? detail::styled_payload_bytes(fresh, source.size())
                              : std::optional<std::size_t>{row_bytes(fresh.rows)};
    if (!measured) return fresh;
    const auto fresh_bytes = *measured;
    const auto slot = index.find(hash);
    const bool collision = slot != index.end() && !slot->second->matches(hash, source, use_ascii);
    // Oversized entries never evict the working set. The caller still owns
    // its complete paid payload and can finish it without retaining a copy.
    if (collision || max_entries == 0u || fresh_bytes > max_bytes) return fresh;
    while (!lru.empty() && (lru.size() >= max_entries || fresh_bytes > max_bytes - total_bytes)) {
        const auto victim = lru.begin();
        total_bytes -= victim->bytes;
        index.erase(victim->hash);
        lru.erase(victim);
    }
    try {
        Entry entry{std::string(source), hash,           use_ascii,
                    fresh.rows,          fresh.styles,   fresh.measured_layout_work,
                    fresh.packed_styles, fresh.packed_meter, fresh.payload_prepaid,
                    fresh_bytes};
        lru.push_back(std::move(entry));
        auto inserted = std::prev(lru.end());
        try {
            index.emplace(hash, inserted);
        } catch (...) {
            lru.erase(inserted);
            throw;
        }
        total_bytes += fresh_bytes;
    } catch (const std::bad_alloc&) {
        return fresh;
    } catch (const std::length_error&) {
        return fresh;
    }
    return fresh;
}

std::optional<std::vector<std::string>> LayoutRowsCache::get_or_layout(std::string_view source,
                                                                       bool use_ascii) {
    detail::Work work;
    try {
        auto result = impl_->resolve(source, use_ascii, work);
        if (!result) return {};
        return std::move(result->rows);
    } catch (const std::bad_alloc&) {
        return {};
    } catch (const std::length_error&) {
        return {};
    }
}

std::optional<ftxui::Element> LayoutRowsCache::render(std::string_view source, bool use_ascii,
                                                      ftxui::Element fallback) {
    detail::Work work;
    return render_with_work(source, use_ascii, std::move(fallback), work, nullptr);
}

std::optional<ftxui::Element>
testing::CacheBudgetAccess::render(LayoutRowsCache& cache, std::string_view source, bool use_ascii,
                                   ftxui::Element fallback, Work& work, FinalEmissionTrace* trace) {
    return cache.render_with_work(source, use_ascii, std::move(fallback), work, trace);
}

std::optional<ftxui::Element>
LayoutRowsCache::render_with_work(std::string_view source, bool use_ascii, ftxui::Element fallback,
                                  testing::Work& work, testing::FinalEmissionTrace* trace) {
    // Preserve a caller-injected ElementWork meter (ADR 0032 F4) across the
    // per-call observation reset.
    testing::Work* injected_element_work = trace ? trace->element_work : nullptr;
    if (trace) *trace = {};
    if (trace) trace->element_work = injected_element_work;
    if (!fallback) return {};
    try {
        return detail::finish_native_layout(impl_->resolve(source, use_ascii, work),
                                            std::move(fallback), work, trace);
    } catch (const std::bad_alloc&) {
        return {};
    } catch (const std::length_error&) {
        return {};
    }
}

void LayoutRowsCache::clear() {
    impl_->lru.clear();
    impl_->index.clear();
    impl_->total_bytes = 0;
    impl_->hit_count = 0;
    impl_->miss_count = 0;
}

std::size_t LayoutRowsCache::size() const noexcept {
    return impl_->index.size();
}

std::size_t LayoutRowsCache::byte_size() const noexcept {
    return impl_->total_bytes;
}

std::size_t LayoutRowsCache::entry_capacity() const noexcept {
    return impl_->max_entries;
}

std::size_t LayoutRowsCache::byte_capacity() const noexcept {
    return impl_->max_bytes;
}

std::size_t LayoutRowsCache::hits() const noexcept {
    return impl_->hit_count;
}

std::size_t LayoutRowsCache::misses() const noexcept {
    return impl_->miss_count;
}

}  // namespace loom::ui::mermaid
