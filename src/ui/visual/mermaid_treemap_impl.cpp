module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A treemap is an indentation-defined tree of quoted named nodes: containers
// carry no value, leaves carry one numeric value. The terminal cannot draw the
// packed rectangle geometry, so the projection is a faithful bounded indented
// outline preserving the hierarchy and every source value verbatim. There are
// no edges or per-node geometry claims.

constexpr int kTreemapMaxDepth = 32;       // corpus maximum is 4
constexpr int kTreemapMaxNodes = 512;
constexpr int kTreemapMaxNameBytes = 256;  // corpus maximum is 100
constexpr int kTreemapMaxValueBytes = 32;
constexpr int kTreemapWrapWidth = 64;
constexpr int kTreemapMaxRows = 1024;

struct TreemapNode {
    int depth;
    std::string name;
    std::string value;  // empty for a container
};

bool treemap_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view treemap_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return source;
    return source.substr(close + 5);
}

// A non-negative integer or decimal: digits, optionally one decimal point
// followed by digits. The source token is transcribed, never reinterpreted.
bool is_leaf_value(std::string_view token) {
    if (token.empty() || token.size() > kTreemapMaxValueBytes) return false;
    bool seen_dot = false;
    for (char c : token) {
        if (c == '.') {
            if (seen_dot) return false;
            seen_dot = true;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    return token.front() != '.' && token.back() != '.';
}

bool treemap_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kTreemapMaxRows || !work.spend(8)) return false;
        out.push_back(std::move(current));
        current.clear();
        cells = 0;
        return true;
    };
    while (byte < label.size()) {
        const std::size_t start = byte;
        while (byte < label.size() && label[byte] != ' ') ++byte;
        std::string word = label.substr(start, byte - start);
        while (byte < label.size() && label[byte] == ' ') ++byte;
        const int word_cells = ftxui::string_width(word);
        if (word_cells < 1) return false;
        if (word_cells > kTreemapWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kTreemapWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

}  // namespace

std::optional<std::vector<std::string>> treemap_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!treemap_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = treemap_body(source);
    // No color/text-format contract. A frontmatter theme/look/config leaf
    // (the only treemap config is the valueFormat number formatter, which
    // changes visible leaf text) or an in-body classDef / ::: style keeps the
    // whole source on ordinary fallback. Key-based, so the word "theme" inside
    // a description or node name is never a false gate.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            const auto close = source.find("\n---\n", 4);
            if (close != std::string_view::npos) {
                const std::string_view frontmatter(source.data(), close);
                for (std::size_t pos = 0;;) {
                    const auto eol = frontmatter.find('\n', pos);
                    std::string_view line = frontmatter.substr(
                        pos, eol == std::string_view::npos ? std::string_view::npos
                                                           : eol - pos);
                    pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
                    const auto key_start = line.find_first_not_of(' ');
                    if (key_start != std::string_view::npos) {
                        std::string_view key = line.substr(key_start);
                        const auto colon = key.find(':');
                        if (colon != std::string_view::npos) key = key.substr(0, colon);
                        if (key == "theme" || key == "look" || key == "config")
                            visual_gate = true;
                    }
                    if (eol == std::string_view::npos) break;
                }
            }
        }
        if (!visual_gate) {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos
                                                       : eol - pos);
                pos = eol == std::string_view::npos ? body.size() : eol + 1;
                const auto trimmed = line.find_first_not_of(' ');
                if (trimmed != std::string_view::npos) {
                    const std::string_view content = line.substr(trimmed);
                    if (content.starts_with("classDef") || content.find(":::") != std::string_view::npos ||
                        (content.starts_with("%%{") && line.find("theme") != std::string_view::npos))
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    std::vector<TreemapNode> nodes;
    int base_indent = -1;
    int previous_depth = -1;
    // A node carrying a value is a leaf; no node may be nested deeper than the
    // most recent leaf (review M1). Track each leaf's depth on a stack so a
    // sibling at the same level stays valid after a leaf closes.
    std::vector<int> leaf_depths;

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                           : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line.find_first_not_of(' ') == std::string_view::npos) continue;
        const auto trimmed_pos = line.find_first_not_of(' ');
        const std::string_view content = line.substr(trimmed_pos);
        if (content.starts_with("%%")) continue;  // comments

        if (!saw_header) {
            if (content != "treemap" && content != "treemap-beta") return {};
            saw_header = true;
            continue;
        }

        // Structural node: leading even spaces then a quoted name.
        const int indent = static_cast<int>(trimmed_pos);
        if (base_indent < 0) base_indent = indent;
        if (indent < base_indent || (indent - base_indent) % 2 != 0) return {};
        const int depth = (indent - base_indent) / 2;
        if (depth > kTreemapMaxDepth) return {};
        if (previous_depth >= 0 && depth > previous_depth + 1) return {};  // no skipped level
        previous_depth = depth;

        if (content.empty() || content.front() != '"') return {};
        const auto closing = content.find('"', 1);
        if (closing == std::string_view::npos || closing == 1) return {};
        std::string name(content.substr(1, closing - 1));
        if (name.size() > kTreemapMaxNameBytes || !unicode_text(name)) return {};
        std::string_view remainder = content.substr(closing + 1);
        while (!remainder.empty() && remainder.front() == ' ') remainder.remove_prefix(1);

        std::string value;
        if (!remainder.empty()) {
            if (remainder.front() != ':') return {};  // unknown trailing token
            std::string_view raw = remainder.substr(1);
            while (!raw.empty() && raw.front() == ' ') raw.remove_prefix(1);
            while (!raw.empty() && raw.back() == ' ') raw.remove_suffix(1);
            if (!is_leaf_value(raw)) return {};
            value = std::string{raw};
        }

        if (nodes.size() >= kTreemapMaxNodes) return {};
        // Leaf role check (review M1): a value node cannot have descendants.
        // Close leaves at the current level or above; a remaining open leaf at
        // a shallower depth means this node would be nested inside that leaf.
        while (!leaf_depths.empty() && leaf_depths.back() >= depth) leaf_depths.pop_back();
        if (!leaf_depths.empty() && leaf_depths.back() < depth) return {};
        if (!value.empty()) leaf_depths.push_back(depth);
        nodes.push_back({depth, std::move(name), std::move(value)});
    }

    if (!saw_header || nodes.empty()) return {};

    std::vector<std::string> out;
    auto emit = [&](std::string row) -> bool {
        if (out.size() >= kTreemapMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(std::move(row));
        return true;
    };

    for (const auto& node : nodes) {
        const std::string indent(static_cast<std::size_t>(node.depth) * 2, ' ');
        const std::string label =
            node.value.empty() ? node.name : node.name + ": " + node.value;
        std::vector<std::string> wrapped;
        if (!treemap_wrap(label, work, wrapped)) return {};
        for (std::size_t i = 0; i < wrapped.size(); ++i) {
            const std::string prefix = indent + (i == 0 ? "- " : "  ");
            if (!emit(prefix + wrapped[i])) return {};
        }
    }
    if (out.empty()) return {};
    return out;
}

}  // namespace loom::ui::mermaid::detail
