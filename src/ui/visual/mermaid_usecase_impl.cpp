module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A usecase-beta diagram declares actors, use cases and system boundaries,
// attaches notes and @{...} metadata, and connects them with the UML edge
// vocabulary. The terminal has no placement/routing/color contract, so the
// projection is a bounded native outline that validates every logical
// statement and transcribes it verbatim — physical line breaks inside a
// quoted label are joined into one statement row. No layout, no color.

constexpr int kUsecaseMaxStatements = 512;
constexpr int kUsecaseMaxIdentBytes = 128;
constexpr int kUsecaseMaxTextBytes = 512;
constexpr int kUsecaseMaxBoundaryDepth = 16;
constexpr int kUsecaseMaxRows = 1024;

bool usecase_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view usecase_body(std::string_view source) {
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

bool is_identifier(std::string_view token) {
    token = trim_view(token);
    if (token.empty() || token.size() > kUsecaseMaxIdentBytes) return false;
    if (!((token[0] >= 'a' && token[0] <= 'z') || (token[0] >= 'A' && token[0] <= 'Z') ||
          token[0] == '_'))
        return false;
    return std::ranges::all_of(token, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

bool is_direction(std::string_view value) {
    return value == "LR" || value == "RL" || value == "TB" || value == "BT";
}

// Replace the inside of every double-quoted span with spaces so shape checks
// never see label text; the original string is still emitted verbatim.
std::string blank_quotes(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool in_quote = false;
    for (char c : text) {
        if (c == '"') {
            in_quote = !in_quote;
            out.push_back(' ');
        } else {
            out.push_back(in_quote ? ' ' : c);
        }
    }
    return out;
}

// Validate only the flat @{...} block beginning at `open` and return the
// position just past its closing brace, or npos. It does not constrain what
// follows the brace (a <<stereotype>> or &lt;..&gt; suffix is parsed by the
// caller's declaration loop).
std::size_t flat_annotation_end(std::string_view code, std::size_t open) {
    if (code.compare(open, 2, "@{") != 0) return std::string_view::npos;
    const auto close = code.find('}', open + 2);
    if (close == std::string_view::npos) return std::string_view::npos;
    const std::string_view inside = code.substr(open + 2, close - open - 2);
    if (trim_view(inside).empty() || inside.find('{') != std::string_view::npos ||
        inside.find('}') != std::string_view::npos)
        return std::string_view::npos;
    return close + 1;
}

// An edge carries one of the UML connectors (quotes already blanked).
bool is_edge(std::string_view code) {
    static constexpr std::string_view kConnectors[] = {
        "--|>", "..>", "-->", "<--", "--o", "o--", "--x", "x--", "---", "--"};
    for (const auto c : kConnectors)
        if (code.find(c) != std::string_view::npos) return true;
    return false;
}

// Validate an edge by structural whitelist: after removing the UML connector
// runs, the include/extend keywords and every identifier run, only balanced
// structural punctuation (endpoint labels/brackets/annotations and the
// dependency colon) may remain. An unknown letter therefore fails closed.
bool validate_edge(std::string_view code) {
    if (!is_edge(code)) return false;
    // A semicolon never appears in a corpus edge (semicolons only occur inside
    // quoted labels, which blank_quotes has already removed).
    if (code.find(';') != std::string_view::npos) return false;
    // A dependency arrow "..>" must carry ": include" or ": extend".
    if (code.find("..>") != std::string_view::npos &&
        code.find(": include") == std::string_view::npos &&
        code.find(": extend") == std::string_view::npos)
        return false;
    std::string rest{code};
    // Connector runs (longest first): arrows, association markers and dots.
    // A labeled edge legitimately contains two dash runs ("-- "label" -->"),
    // so the count is not used as an acceptance condition; a chained edge is
    // still a verbatim text outline with no incorrect transcription.
    static constexpr std::string_view kConnectorPatterns[] = {
        "--|>", "..>", "--->", "---->", "-->", "<--", "--o", "o--", "--x",
        "x--", "---", "--"};
    for (const auto pat : kConnectorPatterns) {
        for (auto pos = rest.find(pat); pos != std::string::npos;
             pos = rest.find(pat, pos))
            rest.replace(pos, pat.size(), pat.size(), ' ');
    }
    bool saw_identifier = false;
    std::size_t identifier_runs = 0;
    for (std::size_t pos = 0; pos < rest.size();) {
        while (pos < rest.size() && rest[pos] == ' ') ++pos;
        if (pos >= rest.size()) break;
        const std::size_t start = pos;
        while (pos < rest.size() &&
               (std::isalnum(static_cast<unsigned char>(rest[pos])) || rest[pos] == '_'))
            ++pos;
        if (pos == start) {
            ++pos;  // punctuation, whitelisted below
            continue;
        }
        const std::string_view word = std::string_view{rest}.substr(start, pos - start);
        if (word != "include" && word != "extend") {
            if (!is_identifier(word)) return false;
            saw_identifier = true;
            ++identifier_runs;
        }
    }
    if (!saw_identifier || identifier_runs < 2) {
        return false;
    }
    // Whatever is left may only be structural punctuation, whitespace or the
    // identifier letters already validated by the scan above.
    for (char c : rest) {
        if (c == ' ' || std::isalnum(static_cast<unsigned char>(c)) || c == '_') continue;
        const bool structural =
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '@' || c == ':' || c == ',' || c == '.' || c == '&' ||
            c == '#' || c == '<' || c == '>' || c == '|' || c == '-';
        if (!structural) {
            return false;
        }
    }
    return true;
}

// Declarations: actor / use case / boundary name, always an identifier here
// because callers pass quote-blanked code (an anonymous quoted use case is
// recognized on the raw statement before this runs), optionally followed by
// ("..."), [...], a flat @{...} block and/or a <<stereotype>> / entity
// suffix.
bool validate_declaration(std::string_view code) {
    std::size_t pos = 0;
    auto skip_spaces = [&] { while (pos < code.size() && code[pos] == ' ') ++pos; };
    skip_spaces();
    {
        const std::size_t start = pos;
        while (pos < code.size() && code[pos] != ' ' && code[pos] != '(' &&
               code[pos] != '[' && code[pos] != '@' && code[pos] != '<')
            ++pos;
        if (!is_identifier(code.substr(start, pos - start))) return false;
    }
    int closed_paren = 0, closed_bracket = 0;
    while (pos < code.size()) {
        skip_spaces();
        if (pos >= code.size()) break;
        const char c = code[pos];
        if (c == '(') {
            const auto close = code.find(')', pos + 1);
            if (close == std::string_view::npos || close == pos + 1) return false;
            pos = close + 1;
            ++closed_paren;
        } else if (c == '[') {
            const auto close = code.find(']', pos + 1);
            if (close == std::string_view::npos || close == pos + 1) return false;
            pos = close + 1;
            ++closed_bracket;
        } else if (c == '@') {
            const auto after = flat_annotation_end(code, pos);
            if (after == std::string_view::npos) return false;
            pos = after;
        } else if (c == '<') {
            // A <<stereotype>> (or a literal &lt;&lt;...&gt;&gt; entity).
            if (code.find("<<", pos) == pos) {
                const auto end = code.find(">>", pos + 2);
                if (end == std::string_view::npos) return false;
                pos = end + 2;
            } else {
                return false;
            }
        } else if (code.find("&lt;", pos) == pos) {
            const auto end = code.find("&gt;&gt;", pos);
            if (end == std::string_view::npos) return false;
            pos = end + 8;
        } else {
            return false;  // unconsumed trailing token
        }
    }
    return closed_paren <= 1 && closed_bracket <= 1;
}

// note for ID "..." — quotes are blanked in `code`, so the shape is
// `note for ID   `.
bool validate_note(std::string_view code) {
    const auto for_pos = code.find(" for ");
    if (for_pos == std::string_view::npos) return false;
    const auto id = trim_view(code.substr(for_pos + 5));
    return is_identifier(id);
}

}  // namespace

// D-447: the closed `config.usecase` tuning namespace and the scalar/block
// accessibility no-ops. The usecase projection is a statement outline; the
// tuning leaves only shape upstream SVG geometry, so each is admitted from its
// corpus-backed exact value and disclosed. `accTitle`/`accDescr` are
// accessibility metadata and are validated no-ops, like the state family's.
constexpr std::size_t kUsecaseNoticeLineLimit = 96u;
constexpr std::string_view kUsecaseNoticeHeader = "Usecase native projection v1";

struct UsecaseLeaf {
    std::string_view key, value;
};

constexpr std::array<UsecaseLeaf, 9> kUsecaseLeaves{{
    {"actorFontSize", "16"},
    {"actorFontFamily", "Arial"},
    {"actorFontWeight", "bold"},
    {"usecaseFontSize", "14"},
    {"usecaseFontFamily", "Georgia"},
    {"nodeSpacing", "60"},
    {"rankSpacing", "70"},
    {"diagramPadding", "24"},
    {"useMaxWidth", "false"},
}};

bool usecase_frontmatter(std::string_view source, std::vector<std::string>& notices,
                         Work& work) {
    if (!source.starts_with("---\n")) return true;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return true;  // unclosed: historical passthrough
    const std::string_view frontmatter = source.substr(4, close - 4);
    bool in_config = false, saw_config = false, saw_description = false, folded = false,
         namespace_seen = false, payload = false;
    std::size_t block_indent = 0;
    std::array<bool, kUsecaseLeaves.size()> seen{};
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
        if (folded && in_config == false && line.front() == ' ') {
            const auto indent = line.find_first_not_of(' ');
            if (block_indent == 0) block_indent = indent;
            if (indent != block_indent || !unicode_text(line.substr(indent))) return false;
            payload = true;
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (key_start != 0) {
            if (!in_config) return false;
            const std::string_view leaf = line.substr(key_start);
            if (leaf.starts_with("#")) {  // YAML comment
                if (eol == std::string_view::npos) break;
                continue;
            }
            const auto colon = leaf.find(':');
            if (colon == std::string_view::npos) return false;
            const std::string_view key = leaf.substr(0, colon);
            std::string_view value = leaf.substr(colon + 1u);
            const auto first = value.find_first_not_of(' ');
            const auto last = value.find_last_not_of(' ');
            value = first == std::string_view::npos ? std::string_view{}
                                                     : value.substr(first, last - first + 1u);
            if (key_start == 2u && key == "usecase" && value.empty()) {
                if (namespace_seen) return false;
                namespace_seen = true;
            } else if (key_start == 4u && namespace_seen) {
                bool matched = false;
                for (std::size_t i = 0; i < kUsecaseLeaves.size(); ++i) {
                    if (kUsecaseLeaves[i].key != key) continue;
                    if (seen[i] || kUsecaseLeaves[i].value != value) return false;
                    seen[i] = true;
                    matched = true;
                    break;
                }
                if (!matched) return false;
            } else {
                return false;
            }
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (line.starts_with("#")) {  // YAML comment
            if (eol == std::string_view::npos) break;
            continue;
        }
        const auto colon = line.find(':');
        const std::string_view key = line.substr(0, colon);
        if (key == "config") {
            if (saw_config) return false;
            saw_config = true;
            in_config = true;
        } else if (key == "description") {
            if (saw_description) return false;
            saw_description = true;
            folded = line.substr(colon + 1u) == " >-";
            in_config = false;
        } else {
            return false;
        }
        if (eol == std::string_view::npos) break;
    }
    if (!saw_description) return false;
    if (folded && !payload) return false;
    bool any = false;
    for (const auto flag : seen) any |= flag;
    if (saw_config && !any) return false;
    if (namespace_seen && !any) return false;
    if (any) {
        std::size_t bytes = 0;
        auto push = [&](std::string_view line) -> bool {
            if (line.size() > kUsecaseNoticeLineLimit || !work.spend(line.size() * 4u + 32u))
                return false;
            notices.emplace_back(line);
            return true;
        };
        if (!push(kUsecaseNoticeHeader)) return false;
        for (std::size_t i = 0; i < kUsecaseLeaves.size(); ++i) {
            if (!seen[i]) continue;
            std::string line = "config.usecase.";
            line += kUsecaseLeaves[i].key;
            line += '=';
            line += kUsecaseLeaves[i].value;
            line += " -> SVG layout tuning; not applied";
            if (!push(line)) return false;
            bytes += line.size();
        }
        if (!work.spend(bytes)) return false;
    }
    return true;
}

// D-447: a non-empty, bounded, unicode payload with a matched quote pair
// removed; the value itself is never projected.
bool usecase_accessibility_value(std::string_view raw, Work& work) {
    auto value = trim_view(raw);
    if (!work.spend(value.size() * 2u + 32u)) return false;
    if (value.size() >= 2u &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = trim_view(value.substr(1u, value.size() - 2u));
    return !value.empty() && value.size() <= static_cast<std::size_t>(kUsecaseMaxTextBytes) &&
           value.find_first_of("\r\n\t") == std::string_view::npos && unicode_text(value);
}

std::optional<std::vector<std::string>> usecase_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!usecase_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = usecase_body(source);
    // D-447: the frontmatter envelope is the closed description + `config.usecase`
    // shape; its admitted leaves become the disclosure footer.
    std::vector<std::string> notices;
    if (!usecase_frontmatter(source, notices, work)) return {};
    // No color contract: a %%{init}%% directive or a classDef/:::/style
    // statement keeps the whole source on fallback. Key-based so prose words
    // are not false gates.
    {
        bool visual_gate = false;
        {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos
                                                       : eol - pos);
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

    // Join physical lines into logical statements across quoted line breaks.
    // A line whose first non-space token is "%%" is a comment: when we are not
    // already inside an open quote, its quotes are inert (a quoted string in a
    // comment must not swallow following statements), so the line closes the
    // current statement instead of contributing to the quote count.
    std::vector<std::string> statements;
    {
        std::string current;
        int quotes = 0;
        for (std::size_t line_start = 0; line_start <= body.size();) {
            std::size_t line_end = body.find('\n', line_start);
            std::string_view line = body.substr(
                line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                                : line_end - line_start);
            line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            const bool outside_quote = quotes % 2 == 0;
            const bool comment_line = trim_view(line).starts_with("%%");
            if (outside_quote && comment_line) {
                if (!current.empty()) {
                    const std::string stmt = trim_copy(current);
                    if (!stmt.empty()) statements.push_back(stmt);
                    current.clear();
                    quotes = 0;
                }
                if (!line.empty()) statements.emplace_back(trim_copy(line));
                continue;
            }
            if (!current.empty()) current.push_back(' ');
            current.append(line.data(), line.size());
            quotes += static_cast<int>(std::count(line.begin(), line.end(), '"'));
            if (quotes % 2 == 0) {
                const std::string stmt = trim_copy(current);
                if (!stmt.empty()) statements.push_back(stmt);
                current.clear();
                quotes = 0;
            }
        }
        if (quotes % 2 != 0) return {};  // unclosed quote
    }
    if (statements.empty() || statements.front() != "usecase-beta") return {};

    bool saw_direction = false;
    bool saw_acc_title = false, saw_acc_descr = false, in_descr_block = false;
    int boundary_depth = 0;
    std::vector<std::string> out;
    auto push_guarded = [&](const std::string& row) -> bool {
        if (out.size() >= kUsecaseMaxRows || ftxui::string_width(row) > kWidthLimit ||
            row.size() > static_cast<std::size_t>(kUsecaseMaxTextBytes) * 4 ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(row);
        return true;
    };

    for (std::size_t i = 1; i < statements.size(); ++i) {
        const std::string& stmt = statements[i];
        if (static_cast<int>(i) > kUsecaseMaxStatements) return {};
        if (stmt.starts_with("%%")) continue;  // comment
        const std::string code = blank_quotes(stmt);
        const std::string_view cv = code;

        if (in_descr_block) {
            // D-447: block-form accDescr payload is validated opaque text.
            if (stmt == "}") {
                in_descr_block = false;
                continue;
            }
            if (stmt.empty() ||
                stmt.size() > static_cast<std::size_t>(kUsecaseMaxTextBytes) ||
                !unicode_text(stmt))
                return {};
            continue;
        }
        if (cv.starts_with("accTitle")) {
            // D-447: scalar accessibility metadata is a validated no-op.
            if (saw_acc_title) return {};
            const auto rest = trim_view(cv.substr(8u));
            if (!rest.starts_with(':')) return {};
            // The value comes from the raw statement: blank_quotes() erases
            // quoted spans, and `accTitle: "..."` is the canonical spelling.
            const std::string_view raw{stmt};
            if (!usecase_accessibility_value(trim_view(raw.substr(8u)).substr(1u), work))
                return {};
            saw_acc_title = true;
            continue;
        }
        if (cv.starts_with("accDescr")) {
            if (saw_acc_descr) return {};
            const auto rest = trim_view(cv.substr(8u));
            if (rest == "{") {
                saw_acc_descr = true;
                in_descr_block = true;
                continue;
            }
            if (!rest.starts_with(':')) return {};
            const std::string_view raw{stmt};
            if (!usecase_accessibility_value(trim_view(raw.substr(8u)).substr(1u), work))
                return {};
            saw_acc_descr = true;
            continue;
        }
        if (cv.starts_with("direction ")) {
            if (saw_direction || !is_direction(trim_view(cv.substr(10)))) return {};
            saw_direction = true;
        } else if (cv.starts_with("systemBoundary")) {
            if (boundary_depth >= kUsecaseMaxBoundaryDepth) return {};
            if (!validate_declaration(trim_view(cv.substr(15)))) return {};
            ++boundary_depth;
        } else if (cv == "end") {
            if (boundary_depth == 0) return {};
            --boundary_depth;
        } else if (cv.starts_with("note ")) {
            if (!validate_note(cv)) return {};
        } else if (cv.starts_with("actor ")) {
            if (!validate_declaration(trim_view(cv.substr(6)))) return {};
        } else if (is_edge(cv)) {
            if (!validate_edge(cv)) return {};
        } else if (trim_view(stmt).starts_with('"')) {
            // An anonymous use case is a quoted string on its own; the join
            // step already proved the quotes are balanced and closed.
            if (trim_view(stmt).find('"', 1) == std::string_view::npos) return {};
        } else {
            // A use case declaration, an edge annotation (id@{...}) or an
            // anonymous quoted use case.
            if (!validate_declaration(cv)) return {};
        }
        if (!push_guarded(stmt)) return {};
    }

    if (in_descr_block) return {};       // unclosed accDescr block
    if (boundary_depth != 0) return {};  // unclosed system boundary
    if (out.empty()) {
        if (!push_guarded("usecase-beta")) return {};  // header-only frame
    } else {
        std::vector<std::string> combined;
        combined.reserve(out.size() + 1);
        combined.push_back("usecase-beta");
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    if (!notices.empty()) {
        // D-447: append the closed-envelope disclosure after the outline.
        if (!push_guarded("")) return {};
        for (const auto& line : notices)
            if (!push_guarded(line)) return {};
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
