module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A treeView-beta diagram is an indentation tree: every node line sits on a
// 2+4k space grid and carries a quoted or bare label with an optional icon(...)
// and/or "## description" suffix. The terminal has no icon renderer or tree
// guide-line/color contract, so the projection is a bounded native outline
// that validates the grid and suffix shapes and transcribes each node line
// verbatim, preserving its source indentation. No icons, no color.

constexpr int kTreeMaxNodes = 512;
constexpr int kTreeMaxLabelBytes = 512;
constexpr int kTreeMaxRows = 1024;
constexpr int kTreeBaseIndent = 2;
constexpr int kTreeIndentStep = 4;

bool tree_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs (one corpus
// fixture uses "---      "). Returns the body after the fence, or the whole
// source when there is no frontmatter block.
std::string_view tree_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    std::size_t search = 4;
    while (true) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                                : nl - search);
        std::size_t i = 0;
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i == line.size()) {
                if (nl == std::string_view::npos) return std::string_view{};
                return source.substr(nl + 1);
            }
        }
        if (nl == std::string_view::npos) return source;
        search = nl + 1;
    }
}

// Validate one node content (indent already stripped): a leading quoted label
// or a bare label, with balanced icon(...)/## suffixes. Quotes, when used,
// form exactly one leading "..." pair; parentheses balance; a "##" may only
// appear as the description marker " ## ".
bool validate_node(std::string_view content) {
    if (content.empty() || content.size() > static_cast<std::size_t>(kTreeMaxLabelBytes))
        return false;
    if (!unicode_text(std::string{content})) return false;

    std::size_t pos = 0;
    bool quoted_lead = false;
    if (content[0] == '"') {
        const auto close = content.find('"', 1);
        if (close == std::string_view::npos || close == 1) return false;  // unclosed/empty
        pos = close + 1;
        quoted_lead = true;
    } else if (content.find('"') != std::string_view::npos) {
        return false;  // quotes only appear as a leading label pair
    }

    int parens = 0;
    bool seen_icon = false;
    bool seen_description = false;
    while (pos < content.size()) {
        const char c = content[pos];
        if (c == ' ') {
            ++pos;
            continue;
        }
        // Description marker: "##" preceded by a space, then free text.
        if (content.compare(pos, 2, "##") == 0) {
            if (seen_description || pos == 0 || content[pos - 1] != ' ') return false;
            seen_description = true;
            pos += 2;
            continue;
        }
        // icon(...) suffix with a non-empty, balanced argument. The branch
        // consumes the whole parenthesized pair atomically, so it does not
        // touch `parens` (which only tracks stray, non-icon parentheses).
        if (content.compare(pos, 5, "icon(") == 0) {
            if (seen_icon || seen_description) return false;
            const auto close = content.find(')', pos + 5);
            if (close == std::string_view::npos || close == pos + 5) return false;
            seen_icon = true;
            pos = close + 1;
            continue;
        }
        if (c == '(' || c == ')') {
            parens += c == '(' ? 1 : -1;
            if (parens < 0) return false;
            ++pos;
            continue;
        }
        // After a closing quoted label only the suffix tokens above may
        // follow; a bare character or stray quote there is malformed. A bare
        // (unquoted) label may still contain arbitrary label characters.
        if (quoted_lead) return false;
        // Bare label characters (including '/', '.', emoji, '_', '-').
        ++pos;
    }
    return parens == 0;
}

}  // namespace

std::optional<std::vector<std::string>> treeview_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!tree_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = tree_body(source);
    // No icon/color/layout contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. The fence splitter tolerates trailing whitespace so
    // a "---      " close still exposes its frontmatter to the gate.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            std::size_t search = 4;
            while (search < source.size()) {
                const auto nl = source.find('\n', search);
                const std::string_view line =
                    source.substr(search, nl == std::string_view::npos
                                                      ? std::string_view::npos
                                                      : nl - search);
                std::size_t i = 0;
                bool is_fence = false;
                if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
                    i = 3;
                    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
                    is_fence = i == line.size();
                }
                const auto key_start = line.find_first_not_of(' ');
                if (!is_fence && key_start != std::string_view::npos) {
                    std::string_view key = line.substr(key_start);
                    const auto colon = key.find(':');
                    if (colon != std::string_view::npos) key = key.substr(0, colon);
                    if (key == "theme" || key == "look" || key == "config")
                        visual_gate = true;
                }
                if (is_fence) break;
                if (nl == std::string_view::npos) break;
                search = nl + 1;
            }
        }
        if (!visual_gate) {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
                pos = eol == std::string_view::npos ? body.size() : eol + 1;
                const auto trimmed = line.find_first_not_of(' ');
                if (trimmed != std::string_view::npos) {
                    const std::string_view content = line.substr(trimmed);
                    if (content.starts_with("%%{init") ||
                        content.starts_with("classDef") ||
                        content.starts_with("style ") ||
                        content.find(":::") != std::string_view::npos)
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    int node_count = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kTreeMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(row);
        return true;
    };

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                            : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        const auto first_non_space = line.find_first_not_of(' ');
        if (first_non_space == std::string_view::npos) continue;  // blank line
        const std::string_view content = line.substr(first_non_space);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (content != "treeView-beta") return {};
            saw_header = true;
            continue;
        }
        // Indent grid: 2 + 4k spaces (tabs were already rejected).
        if (first_non_space < static_cast<std::size_t>(kTreeBaseIndent) ||
            (first_non_space - kTreeBaseIndent) % kTreeIndentStep != 0)
            return {};
        if (++node_count > kTreeMaxNodes) return {};
        if (!validate_node(content)) return {};
        if (!push_guarded(std::string{line})) return {};  // verbatim, indent preserved
    }

    if (!saw_header) return {};
    if (out.empty()) {
        if (!push_guarded("treeView-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("treeView-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
