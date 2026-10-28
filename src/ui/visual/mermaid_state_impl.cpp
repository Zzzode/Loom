module;

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {

// State descriptions and block Notes share the original physical-line guard.
bool state_source_lines(std::string_view source, bool& checked, Work& work) {
    if (!work.spend(4u * sizeof(std::string_view) + 8u * sizeof(bool) + 32u)) return false;
    if (checked) return true;
    if (source.size() > kSourceLimit ||
        !work.spend(source.size() * 2u + 8u * sizeof(std::size_t) + 64u))
        return false;
    for (std::size_t n = 0; n < source.size(); ++n)
        if (source[n] == '\r' && (n + 1u == source.size() || source[n + 1u] != '\n')) return false;
    checked = true;
    return true;
}

std::optional<std::string> state_description_label(std::string_view raw, Work& work) {
    if (raw.size() > kSourceLimit ||
        !work.spend(raw.size() * (32u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view)) +
                    256u))
        return {};
    const auto value = trim_relation(raw);
    if (value.empty() || value.size() > 256u || value.front() == ':' ||
        value.find_first_of("{}") != std::string_view::npos)
        return {};
    // The reused atom-safety decoder can search suffixes quadratically.
    if (!work.spend(value.size() * (value.size() + 1u))) return {};
    // Quotes in a description are data; only its alias owner removes STR quotes.
    auto text = relation_literal_label(value);
    if (!text || text->size() > 256u || text->find('\n') != std::string::npos) return {};
    return text;
}

std::optional<StateNoteBlock> state_note_block(std::string_view original, std::size_t header,
                                               Work& work) {
    if (original.size() > kSourceLimit || header > original.size() ||
        !work.spend(original.size() * 8u + 16u * sizeof(std::size_t) +
                    8u * sizeof(std::string_view) + 256u))
        return {};
    const auto newline = original.find('\n', header);
    if (newline == std::string_view::npos) return {};
    const auto begin = newline + 1u;
    std::size_t cursor = begin, close = original.size(), after = 0, total = 0, rows = 0;
    while (cursor < original.size()) {
        if (!work.spend(64u)) return {};
        const auto next = original.find('\n', cursor);
        const auto end = next == std::string_view::npos ? original.size() : next;
        const auto value = trim_relation(original.substr(cursor, end - cursor));
        if (value.size() >= 8u && er_ascii_equal(value.substr(0, 8), "end note")) {
            const auto ch = value.size() == 8u ? 0u : static_cast<unsigned char>(value[8]);
            const bool word = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                              (ch >= '0' && ch <= '9') || ch == '_';
            if (value.size() == 8u || !word) {
                // A boundary marker with residue cannot silently lose its tail.
                if (value.size() != 8u) return {};
                close = cursor;
                after = next == std::string_view::npos ? original.size() : next + 1u;
                break;
            }
        }
        if (value.empty() || value.find("%%") != std::string_view::npos || er_metadata_space(value))
            return {};
        // Only boundary whitespace is closed; internal Unicode/data stay literal.
        auto scalar = value.size() - 1u;
        std::size_t continuation = 0;
        while (scalar > 0 && (static_cast<unsigned char>(value[scalar]) & 0xc0u) == 0x80u) {
            if (!work.spend() || ++continuation > 3u) return {};
            --scalar;
        }
        if (er_metadata_space(value.substr(scalar))) return {};
        const std::size_t separator = rows ? 1u : 0u;
        if (separator > kSourceLimit - total || value.size() > kSourceLimit - total - separator)
            return {};
        total += separator + value.size();
        ++rows;
        cursor = next == std::string_view::npos ? original.size() : next + 1u;
    }
    if (!after || !rows || close == original.size()) return {};
    // Prepay exact join storage and reused decoder worst suffix scans before
    // allocation/decoding. The original source bound makes multiplication safe.
    if (!work.spend(total * (total + 1u) +
                    total * (32u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view)) +
                    (total + 1u) * 3u + 8u * sizeof(StateNoteBlock) + 256u))
        return {};
    std::string joined;
    joined.reserve(total);
    cursor = begin;
    while (cursor < close) {
        if (!work.spend(16u)) return {};
        const auto next = original.find('\n', cursor);
        if (next == std::string_view::npos || next > close) return {};
        const auto value = trim_relation(original.substr(cursor, next - cursor));
        if (!joined.empty()) joined += '\n';
        joined.append(value);
        cursor = next + 1u;
    }
    if (joined.size() != total) return {};
    auto text = relation_literal_label(joined);
    if (!text) return {};
    return StateNoteBlock{std::move(*text), after};
}
}  // namespace loom::ui::mermaid::detail
