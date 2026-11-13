module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A railroad diagram is a set of grammar rules. The corpus carries four
// dialects behind four headers: railroad-beta (IR: nested function calls),
// railroad-abnf-beta, railroad-ebnf-beta and railroad-peg-beta (textual
// rule notations). The terminal has no track geometry or color contract, so
// the projection is a bounded native outline: each rule's notation is
// validated by a recursive-descent parser and the physical lines are
// transcribed verbatim with their indentation preserved. No geometry, no
// color.

constexpr int kRailroadMaxRules = 256;
constexpr int kRailroadMaxRows = 1024;
constexpr int kRailroadMaxTextBytes = 512;
constexpr int kRailroadMaxDepth = 64;

enum class Dialect { Ir, Abnf, Ebnf, Peg };

bool railroad_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// A closing fence is "---" optionally followed by spaces/tabs.
std::string_view railroad_body(std::string_view source) {
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

std::string_view trim_spaces(std::string_view value) {
    while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    while (!value.empty() && value.back() == ' ') value.remove_suffix(1);
    return value;
}

bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

// A cursor over one joined rule. Whitespace (spaces and the newlines that
// join a multi-line IR rule) is insignificant outside string literals.
struct Cursor {
    std::string_view s;
    std::size_t i = 0;
    Dialect dialect;
};

void skip_ws(Cursor& c) {
    while (c.i < c.s.size() &&
           (c.s[c.i] == ' ' || c.s[c.i] == '\n' || c.s[c.i] == '\t'))
        ++c.i;
}
bool at_end(Cursor& c) {
    skip_ws(c);
    return c.i >= c.s.size();
}
bool accept_char(Cursor& c, char ch) {
    skip_ws(c);
    if (c.i < c.s.size() && c.s[c.i] == ch) {
        ++c.i;
        return true;
    }
    return false;
}
bool accept_keyword(Cursor& c, std::string_view keyword) {
    skip_ws(c);
    if (c.s.substr(c.i).starts_with(keyword)) {
        c.i += keyword.size();
        return true;
    }
    return false;
}
bool parse_ident(Cursor& c, std::string_view& token) {
    skip_ws(c);
    const std::size_t start = c.i;
    if (c.i >= c.s.size() || !is_ident_start(c.s[c.i])) return false;
    while (c.i < c.s.size() && is_ident_char(c.s[c.i])) ++c.i;
    token = c.s.substr(start, c.i - start);
    return token.size() <= static_cast<std::size_t>(kRailroadMaxTextBytes);
}

// A quoted literal: a non-empty single-line printable-ASCII interior with
// no embedded quote. Newlines, C0 control bytes (NUL/ESC/...), DEL and
// non-ASCII bytes are rejected rather than emitted as invisible cells.
bool parse_string(Cursor& c) {
    skip_ws(c);
    if (c.i >= c.s.size() || c.s[c.i] != '"') return false;
    ++c.i;
    const std::size_t begin = c.i;
    while (c.i < c.s.size() && c.s[c.i] != '"') {
        const auto u = static_cast<unsigned char>(c.s[c.i]);
        if (u < 0x20 || u > 0x7E) return false;  // controls, DEL and non-ASCII
        ++c.i;
    }
    if (c.i >= c.s.size() || c.i == begin) return false;
    if (c.i - begin > static_cast<std::size_t>(kRailroadMaxTextBytes)) return false;
    ++c.i;  // closing quote
    return true;
}

// ---- IR (railroad-beta): a closed set of seven nested function calls ----

bool is_ir_function(std::string_view name) {
    return name == "terminal" || name == "nonterminal" || name == "choice" ||
           name == "sequence" || name == "optional" || name == "oneOrMore" ||
           name == "zeroOrMore";
}

bool parse_ir_call(Cursor& c, int depth);

bool parse_ir_arg(Cursor& c, int depth) {
    skip_ws(c);
    if (c.i < c.s.size() && c.s[c.i] == '"') return parse_string(c);
    return parse_ir_call(c, depth);
}

bool parse_ir_call(Cursor& c, int depth) {
    if (depth > kRailroadMaxDepth) return false;
    std::string_view name;
    if (!parse_ident(c, name) || !is_ir_function(name)) return false;
    if (!accept_char(c, '(')) return false;
    skip_ws(c);
    if (c.i < c.s.size() && c.s[c.i] != ')') {
        if (!parse_ir_arg(c, depth + 1)) return false;
        skip_ws(c);
        while (c.i < c.s.size() && c.s[c.i] == ',') {
            ++c.i;
            if (!parse_ir_arg(c, depth + 1)) return false;
            skip_ws(c);
        }
    }
    return accept_char(c, ')');
}

bool parse_ir_rule(std::string_view joined) {
    Cursor c{joined, 0, Dialect::Ir};
    std::string_view lhs;
    if (!parse_ident(c, lhs) || !accept_char(c, '=')) return false;
    if (!parse_ir_call(c, 0)) return false;
    if (!accept_char(c, ';')) return false;
    return at_end(c);
}

// ---- Textual dialects (ABNF / EBNF / PEG) ----

bool parse_text_choice(Cursor& c, int depth);

bool at_item_start(Cursor& c) {
    if (c.i >= c.s.size()) return false;
    const char ch = c.s[c.i];
    if (ch == '"' || is_ident_start(ch) || ch == '(') return true;
    if (ch == '{' && c.dialect == Dialect::Ebnf) return true;
    if (ch == '*' && c.dialect == Dialect::Abnf) return true;
    return false;
}

bool parse_text_atom(Cursor& c, int depth) {
    if (depth > kRailroadMaxDepth) return false;
    skip_ws(c);
    if (c.i >= c.s.size()) return false;
    const char ch = c.s[c.i];
    if (ch == '"') return parse_string(c);
    if (is_ident_start(ch)) {
        std::string_view token;
        return parse_ident(c, token);
    }
    if (ch == '(') {
        ++c.i;
        if (!parse_text_choice(c, depth + 1)) return false;
        return accept_char(c, ')');
    }
    if (ch == '{' && c.dialect == Dialect::Ebnf) {
        ++c.i;
        if (!parse_text_choice(c, depth + 1)) return false;
        return accept_char(c, '}');
    }
    return false;
}

// EBNF/PEG repetition is a suffix; ABNF instead prefixes a group with '*'.
bool parse_text_postfix(Cursor& c, int depth) {
    if (c.dialect == Dialect::Abnf) return parse_text_atom(c, depth);
    if (!parse_text_atom(c, depth)) return false;
    while (true) {
        skip_ws(c);
        if (c.i < c.s.size() && (c.s[c.i] == '*' || c.s[c.i] == '+')) {
            ++c.i;
            continue;
        }
        break;
    }
    return true;
}

// ABNF (RFC 5234) repetition is a prefix on a group, "*(" ... ")", never a
// postfix operator; the corpus's only ABNF fixture uses this prefix form, so
// a postfix "*" (an EBNF/PEG convention) is deliberately not accepted here.
bool parse_abnf_repetition(Cursor& c, int depth) {
    skip_ws(c);
    if (c.i >= c.s.size() || c.s[c.i] != '*') return parse_text_atom(c, depth);
    ++c.i;
    if (!accept_char(c, '(')) return false;
    if (!parse_text_choice(c, depth + 1)) return false;
    return accept_char(c, ')');
}

bool parse_text_item(Cursor& c, int depth) {
    return c.dialect == Dialect::Abnf ? parse_abnf_repetition(c, depth)
                                      : parse_text_postfix(c, depth);
}

// A sequence is implicit adjacency; EBNF additionally allows an optional
// comma between items. A dangling comma is an error.
bool parse_text_sequence(Cursor& c, int depth) {
    if (!parse_text_item(c, depth)) return false;
    while (true) {
        skip_ws(c);
        bool comma = false;
        if (c.dialect == Dialect::Ebnf && c.i < c.s.size() && c.s[c.i] == ',') {
            ++c.i;
            comma = true;
        }
        skip_ws(c);
        if (!at_item_start(c)) return !comma;
        if (!parse_text_item(c, depth)) return false;
    }
}

bool parse_text_choice(Cursor& c, int depth) {
    if (!parse_text_sequence(c, depth)) return false;
    const char choice_op = c.dialect == Dialect::Ebnf ? '|' : '/';
    while (true) {
        skip_ws(c);
        if (c.i < c.s.size() && c.s[c.i] == choice_op) {
            ++c.i;
            if (!parse_text_sequence(c, depth)) return false;
            continue;
        }
        break;
    }
    return true;
}

bool parse_text_rule(std::string_view joined, Dialect dialect) {
    Cursor c{joined, 0, dialect};
    std::string_view lhs;
    if (!parse_ident(c, lhs)) return false;
    if (dialect == Dialect::Peg) {
        if (!accept_keyword(c, "<-")) return false;
    } else if (!accept_char(c, '=')) {
        return false;
    }
    if (!parse_text_choice(c, 0)) return false;
    if (!accept_char(c, ';')) return false;
    return at_end(c);
}

bool is_railroad_header(std::string_view header, Dialect& dialect) {
    if (header == "railroad-beta") {
        dialect = Dialect::Ir;
        return true;
    }
    if (header == "railroad-abnf-beta") {
        dialect = Dialect::Abnf;
        return true;
    }
    if (header == "railroad-ebnf-beta") {
        dialect = Dialect::Ebnf;
        return true;
    }
    if (header == "railroad-peg-beta") {
        dialect = Dialect::Peg;
        return true;
    }
    return false;
}

}  // namespace

std::optional<std::vector<std::string>> railroad_layout(std::string_view source,
                                                        Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!railroad_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = railroad_body(source);
    // No track/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive or a classDef/:::/style statement keeps the whole
    // source on fallback. Trim the key before comparing so the YAML-legal
    // "config : 42" spacing shape still gates.
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
                    if (colon != std::string_view::npos)
                        key = trim_spaces(key.substr(0, colon));
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
    Dialect dialect = Dialect::Ir;
    int rule_count = 0;
    bool rule_open = false;
    std::vector<std::string> buffered;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kRailroadMaxRows || ftxui::string_width(row) > kWidthLimit ||
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
            if (first_non_space != 0 || !is_railroad_header(content, dialect)) return {};
            saw_header = true;
            continue;
        }

        // A continuation line (any indentation) is legal only inside an open
        // multi-line IR rule; the textual dialects and single-line IR rules
        // keep every rule on a column-0 line. The ';' terminator, not the
        // column, marks a rule boundary, so a column-0 line may either open a
        // rule or close a multi-line one.
        if (first_non_space > 0) {
            if (dialect != Dialect::Ir || !rule_open) return {};
        } else if (!rule_open) {
            if (++rule_count > kRailroadMaxRules) return {};
        }
        rule_open = true;
        buffered.emplace_back(line);

        if (content.ends_with(';')) {
            std::string joined;
            for (const auto& piece : buffered) {
                if (!joined.empty()) joined.push_back('\n');
                joined += piece;
            }
            const bool valid = dialect == Dialect::Ir
                                   ? parse_ir_rule(joined)
                                   : parse_text_rule(joined, dialect);
            if (!valid) return {};
            for (const auto& piece : buffered)
                if (!push_guarded(piece)) return {};  // verbatim, indent preserved
            buffered.clear();
            rule_open = false;
        }
    }

    if (!saw_header || rule_open) return {};  // missing header or unterminated rule
    if (out.empty()) {
        std::string header_only;
        for (std::size_t n = 0; n < body.size(); ++n) {
            if (body[n] == '\n') break;
            header_only.push_back(body[n]);
        }
        if (!push_guarded(header_only)) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        const std::string header_text = [&] {
            const auto nl = body.find('\n');
            return std::string{body.substr(0, nl == std::string_view::npos ? body.size() : nl)};
        }();
        combined.push_back(header_text);
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
