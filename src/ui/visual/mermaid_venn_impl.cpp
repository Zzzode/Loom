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
    // style assignment, classDef or ::: tag keeps the whole source on fallback.
    // Key-based so the words "style"/"theme" in prose or a name are not gates.
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
                    if (content.starts_with("style ") || content.starts_with("classDef") ||
                        content.find(":::") != std::string_view::npos)
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    std::string title;
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
            // A text node is always exactly one level under a set/union.
            if (depth != 1) return {};
            std::string_view quoted = trim_view(content.substr(5));
            std::string text_value;
            if (!name_token(quoted, text_value)) return {};
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

        if (++statement_count > kVennMaxStatements) return {};
        const auto sp = content.find(' ');
        if (sp == std::string_view::npos) return {};  // unknown bare statement
        const std::string_view keyword = content.substr(0, sp);
        std::string_view rest = content.substr(sp + 1);

        // Grammar role invariant (corpus: sets/unions are always at the base
        // level, text one level under them). A nested set/union is invalid.
        if ((keyword == "set" || keyword == "union") && depth != 0) return {};

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
    return out;
}

}  // namespace loom::ui::mermaid::detail
