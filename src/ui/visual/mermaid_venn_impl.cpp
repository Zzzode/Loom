module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A venn diagram declares named sets, their pairwise/three-way unions and
// optional text nodes nested inside a set or union. The terminal cannot draw
// circles or overlap geometry, so the projection is a bounded native indented
// outline preserving every set, union, size, label and text node as text.

constexpr int kVennMaxDepth = 16;
constexpr int kVennMaxStatements = 256;
constexpr int kVennMaxUnionNames = 4;
constexpr int kVennWrapWidth = 64;
constexpr int kVennMaxRows = 512;

bool venn_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view venn_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return source;
    return source.substr(close + 5);
}

std::string trim_copy(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return std::string(value);
}

// View-only trim; never binds a temporary, so it is safe to reassign a
// string_view parameter inside the validators.
std::string_view trim_view(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return value;
}

bool ident_head(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool ident_tail(char c) { return ident_head(c) || (c >= '0' && c <= '9'); }
bool is_identifier(std::string_view s) {
    return !s.empty() && s.size() <= 64 && ident_head(s.front()) &&
           std::ranges::all_of(s.substr(1), ident_tail);
}

// A set/union name is a bare identifier or a quoted string; fills `out` with
// the transcribed name.
bool name_token(std::string_view token, std::string& out) {
    token = trim_view(token);
    if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
        out.assign(token.substr(1, token.size() - 2));
        return out.find('"') == std::string::npos && !out.empty() &&
               out.size() <= 256 && unicode_text(out);
    }
    if (is_identifier(token)) {
        out.assign(token);
        return true;
    }
    return false;
}

// A non-negative integer size token.
bool is_size(std::string_view s, std::string& out) {
    s = trim_view(s);
    if (s.empty() || s.size() > 18) return false;
    if (!std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; })) return false;
    out.assign(s);
    return true;
}

// Split "A, B,C" on commas outside quotes.
bool split_names(std::string_view text, std::vector<std::string>& names) {
    std::size_t start = 0;
    bool in_quote = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '"') in_quote = !in_quote;
        if (text[i] == ',' && !in_quote) {
            std::string name;
            if (!name_token(text.substr(start, i - start), name)) return false;
            names.push_back(std::move(name));
            start = i + 1;
        }
    }
    std::string last;
    if (!name_token(text.substr(start), last)) return false;
    names.push_back(std::move(last));
    return !names.empty() && names.size() <= kVennMaxUnionNames;
}

// Parse an optional trailing " :size" or ["Label"] suffix off a declaration,
// filling size/label; at most one suffix is allowed.
bool parse_suffix(std::string_view tail, std::string& size, std::string& label) {
    tail = trim_view(tail);
    if (tail.empty()) return true;
    if (tail.front() == ':') {
        if (!size.empty() || !label.empty()) return false;
        return is_size(tail.substr(1), size);
    }
    if (tail.front() == '[' && tail.back() == ']') {
        if (!size.empty() || !label.empty()) return false;
        std::string_view inner = trim_view(tail.substr(1, tail.size() - 2));
        if (inner.size() < 2 || inner.front() != '"' || inner.back() != '"') return false;
        label.assign(inner.substr(1, inner.size() - 2));
        return label.find('"') == std::string::npos && !label.empty() &&
               label.size() <= 256 && unicode_text(label);
    }
    return false;  // unknown trailing token
}

bool venn_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kVennMaxRows || !work.spend(8)) return false;
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
        if (word_cells > kVennWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kVennWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

// RFC 0043: a column-0 `config:` block whose sole leaf is an indent-2 `theme`
// inside the closed corpus table is accepted and disclosed through a bounded
// footer; every other frontmatter shape keeps the previous whole-source
// fallback. The renderer never reads `theme`, so the footer states that the
// palette was not applied.
constexpr std::size_t kVennNoticeLineLimit = 96u;
constexpr std::string_view kVennNoticeHeader = "Venn native projection v1";
constexpr std::array<std::string_view, 3> kVennThemeTable{
    {"dark", "redux-color", "redux-dark-color"}};

// Trim, strip a trailing YAML comment outside quotes, then dequote a matched
// pair; an unmatched quote is a mismatch, never a value. TU-local copy of the
// agentflow helper (RFC 0041) so no shared module is widened.
std::optional<std::string> venn_scalar(std::string_view raw, Work& work) {
    const auto trim_view = [](std::string_view value) {
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string_view::npos) return std::string_view{};
        const auto last = value.find_last_not_of(" \t");
        return value.substr(first, last - first + 1u);
    };
    auto value = trim_view(raw);
    char quote = 0;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const char c = value[n];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '#' && (n == 0u || value[n - 1u] == ' ' || value[n - 1u] == '\t')) {
            value = trim_view(value.substr(0, n));
            break;
        }
    }
    if (!work.spend(value.size() + 1u)) return std::nullopt;
    if (value.size() >= 2u &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1u, value.size() - 2u);
    return std::string(value);
}

bool venn_notice_line(std::vector<std::string>& notices, std::string_view line, Work& work) {
    if (line.size() > kVennNoticeLineLimit ||
        !work.spend(line.size() * 4u + sizeof(std::string) + 32u))
        return false;
    notices.emplace_back(line);
    return true;
}

// D-453: the in-body `style <targets> <key>:<value>[, ...]` statement is a
// closed colour shape. The venn projection paints its own labelled outline and
// never reads fill/colour, so each statement is inert and disclosed once.
constexpr std::array<std::string_view, 4> kVennStyleProps{
    {"fill", "color", "stroke", "stroke-width"}};

bool venn_style_identifier(std::string_view token) {
    if (token.empty() || token.size() > 64u) return false;
    const auto head_ok = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    if (!head_ok(token.front())) return false;
    return std::ranges::all_of(token, [&](char c) {
        return head_ok(c) || (c >= '0' && c <= '9') || c == '_';
    });
}

bool venn_style_value(std::string_view value) {
    if (value.empty() || value.size() > 32u) return false;
    for (const char c : value) {
        const auto u = static_cast<unsigned char>(c);
        const bool ok = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'z') ||
                        (u >= 'A' && u <= 'Z') || c == '#' || c == '.' || c == '-' ||
                        c == '%';
        if (!ok) return false;
    }
    return true;
}

// `style A,B fill:#ffe66d` -> one disclosure line. Targets and properties are
// both closed: identifiers for the former, the property table for the latter.
bool venn_style_statement(std::string_view content, std::vector<std::string>& notices,
                          std::vector<std::string>& seen_statements, Work& work) {
    std::string_view rest = content.substr(6u);  // after "style "
    const auto space = rest.find(' ');
    if (space == std::string_view::npos) return false;
    const std::string_view targets_text = rest.substr(0u, space);
    std::string_view props_text = rest.substr(space + 1u);
    while (!props_text.empty() && props_text.front() == ' ') props_text.remove_prefix(1);
    while (!props_text.empty() && props_text.back() == ' ') props_text.remove_suffix(1);
    if (props_text.empty()) return false;
    std::string targets;
    for (std::size_t pos = 0;;) {
        const auto comma = targets_text.find(',', pos);
        std::string_view name = targets_text.substr(
            pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
        if (!venn_style_identifier(name)) return false;
        if (!targets.empty()) targets.push_back(',');
        targets.append(name.data(), name.size());
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    std::string keys;
    // The disclosure omits values, so duplicates are keyed on the normalized
    // statement (targets plus each ordered key=value pair), not on the rendered
    // line: `style A fill:#fff` twice is a duplicate, `fill:#fff` plus
    // `fill:#000` is not.
    std::string dedupe = targets + "|";
    std::vector<std::string> seen;
    for (std::size_t pos = 0;;) {
        const auto comma = props_text.find(',', pos);
        std::string_view entry = props_text.substr(
            pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
        while (!entry.empty() && entry.front() == ' ') entry.remove_prefix(1);
        while (!entry.empty() && entry.back() == ' ') entry.remove_suffix(1);
        if (entry.empty()) return false;
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) return false;
        const std::string_view key = entry.substr(0u, colon);
        const std::string_view value = entry.substr(colon + 1u);
        if (std::ranges::find(kVennStyleProps, key) == kVennStyleProps.end() ||
            !venn_style_value(value))
            return false;
        const std::string key_text{key};
        if (std::ranges::find(seen, key_text) != seen.end()) return false;
        seen.push_back(key_text);
        if (!keys.empty()) keys.push_back(',');
        keys += key_text;
        dedupe += key_text;
        dedupe += '=';
        dedupe.append(value.data(), value.size());
        dedupe += ';';
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    if (std::ranges::find(seen_statements, dedupe) != seen_statements.end()) return false;
    seen_statements.push_back(std::move(dedupe));
    std::string line = "style " + targets + " [" + keys + "] -> SVG colour; not applied";
    return venn_notice_line(notices, line, work);
}

// Returns true when the whole source must fall back. Otherwise `notices` holds
// the bounded disclosure footer (empty when no theme leaf was admitted).
bool venn_frontmatter_gated(std::string_view source, std::vector<std::string>& notices,
                            Work& work) {
    if (!source.starts_with("---\n")) return false;
    // The venn fence rule is exact "\n---\n"; an unclosed envelope does not
    // gate, exactly as before.
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return false;
    const std::string_view frontmatter = source.substr(4, close - 4);
    bool in_config = false;
    bool saw_config = false;
    std::optional<std::string> theme;
    // D-443: the sketch-style leaf trio. The venn projection paints its own
    // circles and never reads these keys, so each value comes from its closed
    // corpus table and is disclosed.
    std::optional<std::string> look, seed, font;
    for (std::size_t pos = 0;;) {
        const auto eol = frontmatter.find('\n', pos);
        const std::string_view line = frontmatter.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
        const auto key_start = line.find_first_not_of(' ');
        if (key_start == std::string_view::npos) {  // blank line
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (key_start != 0) {
            const std::string_view leaf = line.substr(key_start);
            if (in_config) {
                if (!leaf.starts_with('#')) {  // YAML comment
                    const auto colon = leaf.find(':');
                    if (colon == std::string_view::npos || key_start != 2u) return true;
                    const std::string_view key = leaf.substr(0, colon);
                    auto value = venn_scalar(leaf.substr(colon + 1u), work);
                    if (!value) return true;
                    if (key == "theme") {
                        if (theme ||
                            std::ranges::find(kVennThemeTable, *value) == kVennThemeTable.end())
                            return true;
                        theme = std::move(*value);
                    } else if (key == "look") {
                        // Closed corpus table: only the hand-drawn look occurs.
                        if (look || *value != "handDrawn") return true;
                        look = std::move(*value);
                    } else if (key == "handDrawnSeed") {
                        // D-442's seed rule: a positive decimal of at most nine
                        // digits; zero, signs, decimals and empty stay rejected.
                        if (seed || value->empty() || value->size() > 9u ||
                            value->find_first_not_of("0123456789") != std::string_view::npos ||
                            value->find_first_not_of('0') == std::string_view::npos)
                            return true;
                        seed = std::move(*value);
                    } else if (key == "fontFamily") {
                        if (font || *value != "courier") return true;
                        font = std::move(*value);
                    } else {
                        return true;
                    }
                }
            } else {
                // Outside a config block the previous flat scan still gates
                // every indented visual key; other indented lines are ignored.
                const auto colon = leaf.find(':');
                const auto key = leaf.substr(0, colon);
                if (key == "theme" || key == "look" || key == "config") return true;
            }
            if (eol == std::string_view::npos) break;
            continue;
        }
        const std::string_view column0 = line;
        if (column0.starts_with('#')) {  // YAML comment
            if (eol == std::string_view::npos) break;
            continue;
        }
        const auto colon = column0.find(':');
        const auto key = column0.substr(0, colon);
        if (key == "config") {
            if (saw_config) return true;
            saw_config = true;
            in_config = true;
        } else {
            // `description` and every other column-0 key keep the previous
            // passthrough and close an open config block; a bare visual key
            // still gates.
            if (key == "theme" || key == "look") return true;
            in_config = false;
        }
        if (eol == std::string_view::npos) break;
    }
    if (saw_config && !theme && !look && !seed && !font) return true;
    if (!theme && !look && !seed && !font) return false;
    if (!venn_notice_line(notices, kVennNoticeHeader, work)) return true;
    if (theme) {
        std::string line = "config.theme=";
        line += *theme;
        line += " -> colour palette; not applied";
        if (!venn_notice_line(notices, line, work)) return true;
    }
    // D-443 appends the look/seed/font lines after the theme leaf so no
    // already-drawn frame's footer changes.
    if (look) {
        const std::string line = "config.look=" + *look + " -> source node style; not applied";
        if (!venn_notice_line(notices, line, work)) return true;
    }
    if (seed) {
        const std::string line =
            "config.handDrawnSeed=" + *seed + " -> sketch jitter seed; not applied";
        if (!venn_notice_line(notices, line, work)) return true;
    }
    if (font) {
        const std::string line = "config.fontFamily=\"courier\" -> terminal font; not applied";
        if (!venn_notice_line(notices, line, work)) return true;
    }
    return false;
}

}  // namespace

std::optional<std::vector<std::string>> venn_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!venn_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = venn_body(source);
    // No color/layout contract: a frontmatter theme/look/config key, an in-body
    // classDef or ::: tag keeps the whole source on fallback; D-453 admits the
    // closed `style` statement (inert and disclosed).
    // Key-based so the words "style"/"theme" in prose or a name are not gates.
    // The single admitted exception is RFC 0043's config-block theme leaf.
    std::vector<std::string> notices;
    if (venn_frontmatter_gated(source, notices, work)) return {};
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        std::string_view line = body.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? body.size() : eol + 1;
        const auto trimmed = line.find_first_not_of(' ');
        if (trimmed != std::string_view::npos) {
            const std::string_view content = line.substr(trimmed);
            // D-453 admits the closed `style` statement; classDef/::: stay
            // whole-source gates.
            if (content.starts_with("classDef") ||
                content.find(":::") != std::string_view::npos)
                return {};
        }
        if (eol == std::string_view::npos) break;
    }

    bool saw_header = false;
    std::string title;
    // D-453 flat text form: `text` lines directly after a base-level set/union.
    bool flat_parent = false;
    std::vector<std::string> seen_statements;
    int base_indent = -1;
    int previous_depth = -1;
    int statement_count = 0;
    std::vector<std::string> title_rows;
    std::vector<std::string> out;
    // Single guarded push shared by the title and every statement row, so the
    // row cap, 240-cell width and Work fee cannot be bypassed (review M2,
    // rule 22). The cap counts the combined final output.
    auto push_guarded = [&](std::vector<std::string>& target, const std::string& row) -> bool {
        if (title_rows.size() + out.size() >= kVennMaxRows ||
            ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        target.push_back(row);
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
        const auto trimmed_pos = line.find_first_not_of(' ');
        if (trimmed_pos == std::string_view::npos) continue;
        const int indent = static_cast<int>(trimmed_pos);
        const std::string_view content = line.substr(trimmed_pos);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (content != "venn-beta") return {};
            saw_header = true;
            continue;
        }
        if (content.starts_with("title ")) {
            if (!title.empty()) return {};  // duplicate title
            title = trim_copy(content.substr(6));
            if (title.empty() || title.size() > 256 || !unicode_text(title)) return {};
            std::vector<std::string> title_wrapped;
            if (!venn_wrap(title, work, title_wrapped)) return {};
            for (const auto& row : title_wrapped)
                if (!push_guarded(title_rows, row)) return {};
            continue;
        }

        if (base_indent < 0) base_indent = indent;
        if (indent < base_indent || (indent - base_indent) % 2 != 0) return {};
        const int depth = (indent - base_indent) / 2;
        if (depth > kVennMaxDepth) return {};
        if (previous_depth >= 0 && depth > previous_depth + 1) return {};  // no skipped level

        if (content.starts_with("text ")) {
            // A text node is one level under a set/union, or at the base level
            // directly after a base-level set/union (the flat corpus form).
            if (!(depth == 1 || (depth == 0 && flat_parent))) return {};
            std::string_view quoted = trim_view(content.substr(5));
            std::string text_value;
            // D-453: `text id["Label"]` carries an id used only for styling;
            // the label is what renders.
            const auto bracket = quoted.find('[');
            if (bracket != std::string_view::npos && quoted.front() != '"' &&
                quoted.front() != '\'') {
                const std::string_view id = trim_view(quoted.substr(0u, bracket));
                if (!venn_style_identifier(id)) return {};
                std::string_view rest_label = trim_view(quoted.substr(bracket));
                if (rest_label.size() < 4u || rest_label.front() != '[' ||
                    rest_label.back() != ']')
                    return {};
                rest_label = trim_view(rest_label.substr(1u, rest_label.size() - 2u));
                if (!name_token(rest_label, text_value)) return {};
                quoted = rest_label;
            } else if (!name_token(quoted, text_value)) {
                return {};
            }
            std::vector<std::string> wrapped;
            if (!venn_wrap(text_value, work, wrapped)) return {};
            const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
            for (std::size_t i = 0; i < wrapped.size(); ++i) {
                const std::string prefix = pad + (i == 0 ? "- text " : "  ");
                if (!push_guarded(out, prefix + wrapped[i])) return {};
            }
            previous_depth = depth;
            continue;
        }

        if (content.starts_with("style ")) {
            if (!venn_style_statement(content, notices, seen_statements, work)) return {};
            flat_parent = false;
            continue;
        }

        if (++statement_count > kVennMaxStatements) return {};
        const auto sp = content.find(' ');
        if (sp == std::string_view::npos) return {};  // unknown bare statement
        const std::string_view keyword = content.substr(0, sp);
        std::string_view rest = content.substr(sp + 1);

        // Grammar role invariant (corpus: sets/unions are always at the base
        // level, text one level under them or flat after them). A nested
        // set/union is invalid.
        if ((keyword == "set" || keyword == "union") && depth != 0) return {};
        flat_parent = depth == 0 && (keyword == "set" || keyword == "union");

        std::string outline;
        if (keyword == "set") {
            // set <name>[:size] | ["Label"]
            const auto bracket = rest.find('[');
            const auto colon = rest.find(':');
            std::string_view name_part = rest;
            std::string_view suffix;
            if (bracket != std::string_view::npos &&
                (colon == std::string_view::npos || bracket < colon)) {
                name_part = rest.substr(0, bracket);
                suffix = rest.substr(bracket);
            } else if (colon != std::string_view::npos) {
                name_part = rest.substr(0, colon);
                suffix = rest.substr(colon);
            }
            std::string name;
            if (!name_token(name_part, name)) return {};
            std::string size, label;
            if (!parse_suffix(suffix, size, label)) return {};
            outline = "set " + name;
            if (!label.empty()) outline += " | " + label;
            if (!size.empty()) outline += " | size " + size;
        } else if (keyword == "union") {
            // union n, n[, n][:size] | ["Label"]
            const auto bracket = rest.find('[');
            const auto colon = rest.find(':');
            std::string_view names_part = rest;
            std::string_view suffix;
            if (bracket != std::string_view::npos &&
                (colon == std::string_view::npos || bracket < colon)) {
                names_part = rest.substr(0, bracket);
                suffix = rest.substr(bracket);
            } else if (colon != std::string_view::npos) {
                names_part = rest.substr(0, colon);
                suffix = rest.substr(colon);
            }
            std::vector<std::string> names;
            if (!split_names(names_part, names) || names.size() < 2) return {};
            std::string size, label;
            if (!parse_suffix(suffix, size, label)) return {};
            outline = "union";
            for (const auto& name : names) outline += " " + name + " +";
            outline.erase(outline.size() - 2);  // drop trailing " +"
            if (!label.empty()) outline += " | " + label;
            if (!size.empty()) outline += " | size " + size;
        } else {
            return {};  // unknown statement
        }

        const std::string pad(static_cast<std::size_t>(depth) * 2, ' ');
        std::vector<std::string> wrapped;
        if (!venn_wrap(outline, work, wrapped)) return {};
        for (std::size_t i = 0; i < wrapped.size(); ++i) {
            const std::string prefix = pad + (i == 0 ? "- " : "  ");
            if (!push_guarded(out, prefix + wrapped[i])) return {};
        }
        previous_depth = depth;
    }

    if (!saw_header) return {};
    // Prepend the already-validated title rows without re-charging Work.
    if (!title_rows.empty()) {
        std::vector<std::string> combined;
        combined.reserve(title_rows.size() + out.size());
        combined.insert(combined.end(), title_rows.begin(), title_rows.end());
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    if (out.empty()) return {};
    // RFC 0043: disclose the validated, not-applied theme leaf below the art,
    // after the title combine so the cap counts the same rows it always did.
    if (!notices.empty()) {
        // The frontmatter parser pushes the header when a theme/look leaf is
        // admitted; a style-only footer needs it prepended here.
        if (notices.front() != kVennNoticeHeader) {
            if (!work.spend(sizeof(std::string) + 64u)) return {};
            notices.insert(notices.begin(), std::string{kVennNoticeHeader});
        }
        std::size_t bytes = 0;
        for (const auto& notice : notices) bytes += notice.size();
        if (out.size() + 1u + notices.size() > kVennMaxRows ||
            !work.spend((1u + notices.size()) * (4u * sizeof(std::string) + 256u) +
                        bytes * 2u + 256u))
            return {};
        out.emplace_back();
        for (const auto& notice : notices) out.push_back(notice);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
