module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A C4 model is a set of typed elements (people, systems, containers,
// components, boundaries, deployment nodes) and directed relations between
// their aliases, optionally nested in brace-delimited container blocks. The
// terminal cannot draw boxes or routed arrows, so the projection is a bounded
// native indented outline preserving every element and relation as text.

constexpr int kC4MaxDepth = 32;
constexpr int kC4MaxStatements = 512;
constexpr int kC4MaxArgs = 8;
constexpr int kC4WrapWidth = 64;
constexpr int kC4MaxRows = 1024;

const std::array<std::string_view, 7> kRelations{
    "Rel", "Rel_Back", "Rel_U", "Rel_D", "Rel_L", "Rel_R", "BiRel"};

// Element keywords that may open a nested brace block.
const std::array<std::string_view, 8> kBlockOpeners{
    "Boundary", "System_Boundary", "Container_Boundary", "Enterprise_Boundary",
    "Deployment_Node", "Node", "Node_L", "Node_R"};

// The full closed element vocabulary; anything else is an unknown call and
// fails closed.
const std::array<std::string_view, 22> kElements{
    "Person", "Person_Ext", "System", "System_Ext", "SystemDb", "SystemDb_Ext",
    "SystemQueue", "SystemQueue_Ext", "Container", "Container_Ext", "ContainerDb",
    "ContainerDb_Ext", "ContainerQueue", "ContainerQueue_Ext", "Component",
    "Component_Ext", "ComponentDb", "ComponentDb_Ext", "ComponentQueue",
    "ComponentQueue_Ext", "Boundary", "Deployment_Node"};

bool c4_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view c4_body(std::string_view source) {
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

bool is_any(std::string_view token, const std::array<std::string_view, 7>& set) {
    return std::ranges::find(set, token) != set.end();
}
bool is_element_keyword(std::string_view token) {
    if (std::ranges::find(kElements, token) != kElements.end()) return true;
    // Node aliases and the boundary spellings share the Node_*/Boundary roots;
    // accept the closed opener set explicitly plus any listed element.
    return std::ranges::find(kBlockOpeners, token) != kBlockOpeners.end();
}
bool is_block_opener(std::string_view token) {
    return std::ranges::find(kBlockOpeners, token) != kBlockOpeners.end();
}

bool ident_head(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool ident_tail(char c) { return ident_head(c) || (c >= '0' && c <= '9'); }
bool is_identifier(std::string_view s) {
    return !s.empty() && s.size() <= 64 && ident_head(s.front()) &&
           std::ranges::all_of(s.substr(1), ident_tail);
}

// A non-rendered upstream attribute, e.g. $link="..." or $sprite=browser. Only
// the $identifier= prefix shape is validated; the value is ignored.
bool is_attribute(std::string_view s) {
    if (s.size() < 2 || s.front() != '$') return false;
    std::size_t i = 1;
    if (i >= s.size() || !ident_head(s[i])) return false;
    while (i < s.size() && ident_tail(s[i])) ++i;
    return i < s.size() && s[i] == '=';
}

// Split a call's argument list on commas outside double quotes; strips outer
// whitespace from each argument.
bool split_args(std::string_view args, std::vector<std::string>& out) {
    std::string current;
    bool in_quote = false;
    for (char c : args) {
        if (c == '"') {
            in_quote = !in_quote;
            current.push_back(c);
        } else if (c == ',' && !in_quote) {
            if (out.size() >= kC4MaxArgs) return false;
            out.push_back(trim_copy(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (in_quote) return false;  // unterminated string
    if (out.size() >= kC4MaxArgs) return false;
    out.push_back(trim_copy(current));
    return true;
}

// Unquote a "..." argument and validate it; returns the inner text.
bool unquote(std::string_view arg, std::string& value) {
    if (arg.size() < 2 || arg.front() != '"' || arg.back() != '"') return false;
    value.assign(arg.substr(1, arg.size() - 2));
    if (value.find('"') != std::string::npos) return false;  // no escaped quotes in corpus
    return value.size() <= 256 && unicode_text(value);
}

// Element display names are normally quoted, but the corpus also has a bare
// identifier spelling (Person(customer, Customer, ...)). Accept either.
bool name_arg(std::string_view arg, std::string& value) {
    if (unquote(arg, value)) return true;
    if (is_identifier(arg)) {
        value.assign(arg);
        return true;
    }
    return false;
}

bool c4_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kC4MaxRows || !work.spend(8)) return false;
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
        if (word_cells > kC4WrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kC4WrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

}  // namespace

std::optional<std::vector<std::string>> c4_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!c4_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = c4_body(source);
    // No color/layout contract: a frontmatter theme/look/config key or an
    // in-body Update*Style/UpdateLayoutConfig statement keeps the whole source
    // on ordinary fallback. Key-based so prose mentioning "style" is not a gate.
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
                    if (content.starts_with("UpdateElementStyle") ||
                        content.starts_with("UpdateRelStyle") ||
                        content.starts_with("UpdateLayoutConfig"))
                        visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    static const std::array<std::string_view, 5> kHeaders{
        "C4Context", "C4Container", "C4Component", "C4Deployment", "C4Dynamic"};
    bool saw_header = false;
    int depth = 0;
    int statement_count = 0;
    std::vector<std::string> title_rows;
    std::vector<std::string> out;
    // Single protected emission path shared by the title and every statement
    // row, so the row cap, 240-cell width and Work fee cannot be bypassed
    // (review MINOR-1). The row cap counts the combined final output so the
    // title and statement vectors cannot each fill the cap independently.
    auto push_guarded = [&](std::vector<std::string>& target, const std::string& row) -> bool {
        if (title_rows.size() + out.size() >= kC4MaxRows ||
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
        std::string_view content = line.substr(trimmed_pos);
        if (content.starts_with("%%")) continue;  // Mermaid comment line

        if (!saw_header) {
            if (std::ranges::find(kHeaders, content) == kHeaders.end()) return {};
            saw_header = true;
            continue;
        }
        if (content.starts_with("title ")) {
            // Stash and reject a duplicate, like the other greenfield
            // renderers; wrap now through the same guarded path as statements.
            if (!title_rows.empty()) return {};
            const auto title_text = trim_copy(content.substr(6));
            if (title_text.empty() || title_text.size() > 256 ||
                !unicode_text(title_text))
                return {};
            std::vector<std::string> title_wrapped;
            if (!c4_wrap(title_text, work, title_wrapped)) return {};
            for (const auto& row : title_wrapped)
                if (!push_guarded(title_rows, row)) return {};
            continue;
        }
        if (content == "}") {
            if (depth == 0) return {};  // unmatched close
            --depth;
            continue;
        }

        // Function-call statement: keyword( ... ) optionally followed by {,
        // with or without whitespace before the brace ("...){}" or "...) {").
        const auto paren = content.find('(');
        if (paren == std::string_view::npos) return {};  // unknown statement
        const std::string_view keyword = content.substr(0, paren);
        const auto close_paren = content.rfind(')');
        if (close_paren == std::string_view::npos || close_paren <= paren) return {};
        bool opens_block = false;
        {
            std::string_view trailing = content.substr(close_paren + 1);
            if (!trailing.empty()) {
                std::size_t i = 0;
                while (i < trailing.size() && trailing[i] == ' ') ++i;
                if (i >= trailing.size() || trailing[i] != '{' || i + 1 != trailing.size())
                    return {};  // only an optional single { is legal after )
                opens_block = true;
            }
        }
        const std::string_view arg_text = content.substr(paren + 1, close_paren - paren - 1);

        std::vector<std::string> args;
        if (!split_args(arg_text, args) || args.empty()) return {};
        if (++statement_count > kC4MaxStatements) return {};

        std::string outline;
        if (is_any(keyword, kRelations)) {
            if (opens_block) return {};  // relations never open a block
            // Rel(from, to, "label"[, "tech"][, $attr...])
            if (args.size() < 3) return {};
            if (!is_identifier(args[0]) || !is_identifier(args[1])) return {};
            std::string label;
            if (!unquote(args[2], label)) return {};
            outline = std::string{keyword} + " " + std::string{args[0]} + " -> " +
                      std::string{args[1]} + ": " + label;
            for (std::size_t i = 3; i < args.size(); ++i) {
                std::string quoted;
                if (is_attribute(args[i])) continue;  // non-rendered
                // A relation's technology/description must be quoted; a bare
                // token here is an upstream syntax error (review NIT-3).
                if (!unquote(args[i], quoted)) return {};
                outline += " (" + quoted + ")";
            }
        } else if (is_element_keyword(keyword)) {
            if (opens_block && !is_block_opener(keyword)) return {};
            // Element(alias, Name[, "Tech"[, "Desc"]][, $attr...]); Name may
            // be quoted or a bare identifier in the corpus.
            if (args.size() < 2 || !is_identifier(args[0])) return {};
            std::string name;
            if (!name_arg(args[1], name)) return {};
            outline = std::string{keyword} + " " + std::string{args[0]} + " - " + name;
            for (std::size_t i = 2; i < args.size(); ++i) {
                std::string quoted;
                if (is_attribute(args[i])) continue;  // non-rendered
                if (!unquote(args[i], quoted)) return {};
                outline += " | " + quoted;
            }
        } else {
            return {};  // unknown call target
        }

        const std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
        std::vector<std::string> wrapped;
        if (!c4_wrap(outline, work, wrapped)) return {};
        for (std::size_t i = 0; i < wrapped.size(); ++i) {
            const std::string prefix = indent + (i == 0 ? "- " : "  ");
            if (!push_guarded(out, prefix + wrapped[i])) return {};
        }
        if (opens_block) {
            if (depth >= kC4MaxDepth) return {};
            ++depth;
        }
    }

    if (!saw_header || depth != 0) return {};
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
