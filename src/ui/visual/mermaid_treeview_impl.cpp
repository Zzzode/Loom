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

// D-457/D-466: a node line may carry one `:::class` annotation. It follows the
// label (either as the trailing annotation or immediately before an
// `icon(...)`/`## description` suffix). The terminal draws no highlight colour,
// so the suffix is validated against an identifier, dropped from the
// transcribed row and disclosed once in the footer. A suffix without the
// separating space, a second `:::` or a non-identifier name is malformed.
std::optional<std::string> tree_strip_class(std::string_view content, bool& had_class) {
    const auto at = content.rfind(":::");
    if (at == std::string_view::npos) return std::string{content};
    if (at == 0 || content[at - 1] != ' ') return std::nullopt;
    if (content.rfind(":::", at - 1) != std::string_view::npos) return std::nullopt;
    const std::string_view rest = content.substr(at + 3);
    const auto space = rest.find(' ');
    const std::string_view name = space == std::string_view::npos ? rest : rest.substr(0, space);
    if (name.empty() || name.size() > 64u) return std::nullopt;
    if (!((name.front() >= 'a' && name.front() <= 'z') ||
          (name.front() >= 'A' && name.front() <= 'Z') || name.front() == '_'))
        return std::nullopt;
    for (const char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_'))
            return std::nullopt;
    std::string_view head = content.substr(0, at - 1);
    while (!head.empty() && head.back() == ' ') head.remove_suffix(1);
    while (!head.empty() && head.front() == ' ') head.remove_prefix(1);
    // The annotation attaches to a bare or quoted label only: it must not
    // follow an icon(...) or `## description` suffix, where the tail is free
    // text and dropping it would silently lose authored content.
    if (head.find("icon(") != std::string_view::npos ||
        head.find("##") != std::string_view::npos)
        return std::nullopt;
    if (space == std::string_view::npos) {
        had_class = true;
        return std::string{head};
    }
    const std::string_view tail = rest.substr(space);
    if (tail.find_first_not_of(' ') == std::string_view::npos) {
        had_class = true;
        return std::string{head};
    }
    // A class placed before the suffix tokens may only be followed by them.
    if (!(tail.starts_with(" icon(") || tail.starts_with(" ## "))) return std::nullopt;
    had_class = true;
    std::string result{head};
    result += tail;
    return result;
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
        // icon(...) suffix with a balanced argument; the empty `icon()` form
        // suppresses the default icon upstream and is inert here (icons are
        // never drawn). The branch consumes the whole parenthesized pair
        // atomically, so it does not touch `parens` (which only tracks stray,
        // non-icon parentheses).
        if (content.compare(pos, 5, "icon(") == 0) {
            if (seen_icon || seen_description) return false;
            const auto close = content.find(')', pos + 5);
            if (close == std::string_view::npos) return false;
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

// D-433: the closed `config` namespace. The terminal has no icon renderer, no
// colour and its own indentation grid, so every admitted leaf is inert and is
// disclosed under `TreeView native projection v1`. The parser is tri-state:
// `accepted` separates "no admitted leaf" (plain source, description-only
// frontmatter, config-free twin) from "not an admitted frontmatter".
struct TreeNamespace {
    bool accepted = false;
    std::optional<std::string> show_icons;
    std::optional<std::string> icon_pack;
    std::optional<std::string> row_indent;
    std::optional<std::string> line_thickness;
    std::optional<std::string> font_size;
    std::optional<std::string> label_colour;
    std::optional<std::string> line_colour;
    // D-434: closed icon maps; entries stay in source order.
    std::vector<std::pair<std::string, std::string>> filename_icons;
    std::vector<std::pair<std::string, std::string>> extension_icons;
};

bool tree_key_word(std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_' || c == '.'))  // '.' for icon-map entry keys
            return false;
    return true;
}

// A bare scalar word: lowercase letters, digits, '-' and '_' (icon pack names).
bool tree_word(std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    return true;
}

bool tree_uint_value(std::string_view value) {
    if (value.empty() || value.size() > 5 || value.front() == '0') return false;
    for (const char c : value)
        if (c < '0' || c > '9') return false;
    return std::stoi(std::string{value}) <= 99999;
}

// A matching outer quote pair (single or double) is stripped; the remainder is
// returned, or the value unchanged when unquoted. A lone or mismatched quote
// makes the value invalid.
std::optional<std::string> tree_unquote(std::string_view value) {
    if (value.empty()) return std::nullopt;
    if (value.front() != '\'' && value.front() != '"') return std::string{value};
    if (value.size() < 2 || value.back() != value.front()) return std::nullopt;
    const std::string_view inner = value.substr(1, value.size() - 2);
    if (inner.find(value.front()) != std::string_view::npos) return std::nullopt;
    return std::string{inner};
}

bool tree_px_value(std::string_view value) {
    auto unquoted = tree_unquote(value);
    if (!unquoted) return false;
    const std::string& text = *unquoted;
    if (text.size() < 3 || text.size() > 5 || !text.ends_with("px")) return false;
    for (std::size_t i = 0; i + 2 < text.size(); ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    return text[0] != '0';
}

// D-434: a map entry key is a file name or extension token.
bool tree_entry_key(std::string_view key) {
    if (key.empty() || key.size() > 64) return false;
    for (const char c : key)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_'))
            return false;
    return true;
}

// An icon reference: `none` or a bare pack name with an optional `:name`.
bool tree_icon_ref(std::string_view value) {
    if (value.empty() || value.size() > 64) return false;
    const auto colon = value.find(':');
    const std::string_view pack = colon == std::string_view::npos ? value : value.substr(0, colon);
    const std::string_view name =
        colon == std::string_view::npos ? std::string_view{} : value.substr(colon + 1u);
    if (!tree_word(pack)) return false;
    return colon == std::string_view::npos || (!name.empty() && tree_word(name));
}

bool tree_hex_value(std::string_view value) {
    auto unquoted = tree_unquote(value);
    if (!unquoted || unquoted->size() != 7 || unquoted->front() != '#') return false;
    for (std::size_t i = 1; i < 7; ++i) {
        const char c = (*unquoted)[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

TreeNamespace tree_namespace(std::string_view source) {
    TreeNamespace result;
    if (!source.starts_with("---\n")) {
        result.accepted = true;  // a plain source carries no frontmatter to admit
        return result;
    }
    // The closing fence is "---" optionally followed by spaces/tabs (the
    // corpus has one "---      " close), the same rule as tree_body.
    std::size_t close_start = 0;
    for (std::size_t search = 4;;) {
        const auto nl = source.find('\n', search);
        const std::string_view line =
            source.substr(search, nl == std::string_view::npos ? std::string_view::npos
                                                               : nl - search);
        if (line.size() >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            std::size_t i = 3;
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
            if (i == line.size()) {
                if (nl == std::string_view::npos) return result;  // unterminated
                close_start = search;
                break;
            }
        }
        if (nl == std::string_view::npos) return result;
        search = nl + 1;
    }
    const std::string_view frontmatter = source.substr(0, close_start);
    std::vector<std::pair<std::size_t, std::string_view>> stack;  // (indent, path)
    bool description_seen = false, config_seen = false;
    bool tree_view_seen = false, theme_vars_seen = false, theme_tree_seen = false;
    bool filename_map_seen = false, extension_map_seen = false;
    int leaves = 0;
    bool first = true;
    for (std::size_t pos = 0; pos <= frontmatter.size();) {
        const auto nl = frontmatter.find('\n', pos);
        const std::string_view line =
            frontmatter.substr(pos, nl == std::string_view::npos ? std::string_view::npos
                                                                 : nl - pos);
        const bool last = nl == std::string_view::npos;
        pos = last ? frontmatter.size() + 1u : nl + 1u;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {
            if (last) break;
            continue;
        }
        const std::string_view content = line.substr(key_start);
        if (first) {
            if (key_start != 0u || content != "---") return result;
            first = false;
            if (last) break;
            continue;
        }
        if (key_start == 0u) {
            if (content == "description:" || content.starts_with("description: ")) {
                if (description_seen || config_seen) return result;
                description_seen = true;
                stack.clear();
            } else if (content == "config:") {
                if (config_seen) return result;
                config_seen = true;
                stack.assign(1, {0u, "config"});
            } else {
                return result;
            }
            if (last) break;
            continue;
        }
        if (!config_seen) {  // a description payload line
            if (last) break;
            continue;
        }
        while (!stack.empty() && stack.back().first >= key_start) stack.pop_back();
        if (stack.empty()) return result;
        const std::string_view parent = stack.back().second;
        const auto colon = content.find(':');
        if (colon == std::string_view::npos || colon == 0u) return result;
        const std::string_view key = content.substr(0, colon);
        if (!tree_key_word(key)) return result;
        std::string_view value = content.substr(colon + 1u);
        while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
        while (!value.empty() && value.back() == ' ') value.remove_suffix(1);
        if (!value.empty()) {
            std::optional<std::string>* slot = nullptr;
            bool ok = false;
            if (parent == "config.treeView") {
                if (key == "showIcons") {
                    slot = &result.show_icons;
                    ok = value == "true" || value == "false";
                } else if (key == "defaultIconPack") {
                    slot = &result.icon_pack;
                    ok = tree_word(value);
                } else if (key == "rowIndent") {
                    slot = &result.row_indent;
                    ok = tree_uint_value(value);
                } else if (key == "lineThickness") {
                    slot = &result.line_thickness;
                    ok = tree_uint_value(value);
                }
            } else if (parent == "config.treeView.filenameIcons" ||
                       parent == "config.treeView.extensionIcons") {
                auto unquoted = tree_unquote(value);
                if (!tree_entry_key(key) || !unquoted || !tree_icon_ref(*unquoted))
                    return result;
                auto& map = parent == "config.treeView.filenameIcons"
                                ? result.filename_icons
                                : result.extension_icons;
                if (map.size() >= 16u) return result;
                for (const auto& entry : map)
                    if (entry.first == key) return result;
                map.emplace_back(std::string{key}, std::move(*unquoted));
                ++leaves;
                continue;
            } else if (parent == "config.themeVariables.treeView") {
                if (key == "labelFontSize") {
                    slot = &result.font_size;
                    ok = tree_px_value(value);
                } else if (key == "labelColor") {
                    slot = &result.label_colour;
                    ok = tree_hex_value(value);
                } else if (key == "lineColor") {
                    slot = &result.line_colour;
                    ok = tree_hex_value(value);
                }
            }
            if (slot == nullptr || !ok || slot->has_value()) return result;
            if (slot == &result.font_size || slot == &result.label_colour ||
                slot == &result.line_colour) {
                auto unquoted = tree_unquote(value);
                if (!unquoted) return result;
                *slot = std::move(*unquoted);
            } else {
                *slot = std::string{value};
            }
            ++leaves;
        } else if (parent == "config" && key == "treeView") {
            if (tree_view_seen) return result;
            tree_view_seen = true;
            stack.push_back({key_start, "config.treeView"});
        } else if (parent == "config" && key == "themeVariables") {
            if (theme_vars_seen) return result;
            theme_vars_seen = true;
            stack.push_back({key_start, "config.themeVariables"});
        } else if (parent == "config.treeView" && key == "filenameIcons") {
            if (filename_map_seen) return result;
            filename_map_seen = true;
            stack.push_back({key_start, "config.treeView.filenameIcons"});
        } else if (parent == "config.treeView" && key == "extensionIcons") {
            if (extension_map_seen) return result;
            extension_map_seen = true;
            stack.push_back({key_start, "config.treeView.extensionIcons"});
        } else if (parent == "config.themeVariables" && key == "treeView") {
            if (theme_tree_seen) return result;
            theme_tree_seen = true;
            stack.push_back({key_start, "config.themeVariables.treeView"});
        } else {
            return result;
        }
        if (last) break;
    }
    if (config_seen && leaves == 0) return result;
    result.accepted = true;
    return result;
}

// One notice per admitted leaf, in a fixed order independent of source order.
std::vector<std::string> tree_namespace_notices(const TreeNamespace& ns) {
    std::vector<std::string> out;
    if (ns.show_icons)
        out.push_back("config.treeView.showIcons=" + *ns.show_icons +
                      " -> icons are not drawn; not applied");
    if (ns.icon_pack)
        out.push_back("config.treeView.defaultIconPack=" + *ns.icon_pack +
                      " -> icon pack resolution; not applied");
    if (ns.row_indent)
        out.push_back("config.treeView.rowIndent=" + *ns.row_indent +
                      " -> row indentation; not applied");
    if (ns.line_thickness)
        out.push_back("config.treeView.lineThickness=" + *ns.line_thickness +
                      " -> branch line weight; not applied");
    for (const auto& [key, value] : ns.filename_icons)
        out.push_back("config.treeView.filenameIcons[" + key + "]=" + value +
                      " -> icon mapping; not applied");
    for (const auto& [key, value] : ns.extension_icons)
        out.push_back("config.treeView.extensionIcons[" + key + "]=" + value +
                      " -> icon mapping; not applied");
    if (ns.font_size)
        out.push_back("config.themeVariables.treeView.labelFontSize=" + *ns.font_size +
                      " -> label font size; not applied");
    if (ns.label_colour)
        out.push_back("config.themeVariables.treeView.labelColor=" + *ns.label_colour +
                      " -> label colour; not applied");
    if (ns.line_colour)
        out.push_back("config.themeVariables.treeView.lineColor=" + *ns.line_colour +
                      " -> branch line colour; not applied");
    return out;
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
    // D-433: the closed `config` namespace owns the admitted frontmatter shape;
    // an in-body %%{init}%% directive or a classDef/style statement keeps the
    // whole source on ordinary fallback. A `:::class` annotation is admitted
    // per node by D-457 (validated and dropped). Key-based so prose mentioning
    // "config" is not a gate.
    const TreeNamespace ns = tree_namespace(source);
    if (!ns.accepted) return {};
    {
        bool visual_gate = false;
        for (std::size_t pos = 0;;) {
            const auto eol = body.find('\n', pos);
            std::string_view line = body.substr(
                pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
            pos = eol == std::string_view::npos ? body.size() : eol + 1;
            const auto trimmed = line.find_first_not_of(' ');
            if (trimmed != std::string_view::npos) {
                const std::string_view content = line.substr(trimmed);
                if (content.starts_with("%%{init") || content.starts_with("classDef") ||
                    content.starts_with("style "))
                    visual_gate = true;
            }
            if (eol == std::string_view::npos) break;
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    bool had_class = false;
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
        const auto stripped = tree_strip_class(content, had_class);
        if (!stripped) return {};
        if (!validate_node(*stripped)) return {};
        // Verbatim row, indent preserved; the class annotation is dropped.
        if (!push_guarded(std::string{line.substr(0, first_non_space)} +
                          std::string{*stripped}))
            return {};
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
    auto notices = tree_namespace_notices(ns);
    if (had_class)
        notices.push_back("body class annotations -> highlight colour; not applied");
    if (!notices.empty()) {
        std::size_t extra = 2 + notices.size();
        for (const auto& notice : notices) extra += notice.size();
        if (out.size() + 2 + notices.size() > static_cast<std::size_t>(kTreeMaxRows) ||
            !work.spend(extra * 4u + 64u))
            return {};
        out.push_back("");
        out.push_back("TreeView native projection v1");
        for (const auto& notice : notices) out.push_back(notice);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
