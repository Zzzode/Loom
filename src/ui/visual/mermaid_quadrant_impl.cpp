module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A quadrant chart declares axis labels, four quadrant labels and named data
// points with [x, y] coordinates. The terminal cannot plot points on axes, so
// the projection is a bounded native outline transcribing every axis,
// quadrant, point name and coordinate verbatim — no geometry, no range math.

constexpr int kQuadrantMaxPoints = 256;
constexpr int kQuadrantMaxStatements = 512;
constexpr int kQuadrantMaxNameBytes = 256;
constexpr int kQuadrantMaxCoordBytes = 24;
constexpr int kQuadrantWrapWidth = 64;
constexpr int kQuadrantMaxRows = 1024;

bool quadrant_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view quadrant_body(std::string_view source) {
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

// A non-negative decimal coordinate token: digits with at most one dot. The
// token is transcribed verbatim; the structural projection does no range math.
bool is_coordinate(std::string_view token) {
    token = trim_view(token);
    if (token.empty() || token.size() > kQuadrantMaxCoordBytes) return false;
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

bool quadrant_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kQuadrantMaxRows || !work.spend(8)) return false;
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
        if (word_cells > kQuadrantWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kQuadrantWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

// D-450: the leading body `%%{init: {...}}%%` directive is a closed grammar.
// The projection draws no axes, points or colors, so the quadrantChart sizing
// / axis-position knobs and the quadrant theme colors are inert; each admitted
// key is disclosed in a bounded footer. Anything outside the closed tables, a
// duplicate key or section, a nested value or a second directive keeps the
// whole source on fallback.
constexpr std::string_view kQuadrantNoticeHeader = "Quadrant native projection v1";
// Bounded by the closed tables: prefix 11 + namespace 15 + longest key 35 +
// "=" + longest value 7 + suffix 41 = 110 bytes; the cap is 128.
constexpr std::size_t kQuadrantNoticeLimit = 128;
// "point " — the fixed prefix of a per-point disclosure line; the pre-check
// and the truncation budget count it exactly once.
constexpr std::size_t kQuadrantNoticePrefix = 6u;

constexpr std::array<std::string_view, 16> kQuadrantTuningKeys{
    "chartWidth", "chartHeight", "titlePadding", "titleFontSize", "quadrantPadding",
    "quadrantTextTopPadding", "quadrantLabelFontSize", "quadrantInternalBorderStrokeWidth",
    "quadrantExternalBorderStrokeWidth", "xAxisLabelPadding", "xAxisLabelFontSize",
    "yAxisLabelPadding", "yAxisLabelFontSize", "pointTextPadding", "pointLabelFontSize",
    "pointRadius"};

constexpr std::array<std::string_view, 15> kQuadrantThemeKeys{
    "quadrant1Fill", "quadrant2Fill", "quadrant3Fill", "quadrant4Fill", "quadrant1TextFill",
    "quadrant2TextFill", "quadrant3TextFill", "quadrant4TextFill", "quadrantPointFill",
    "quadrantPointTextFill", "quadrantXAxisTextFill", "quadrantYAxisTextFill",
    "quadrantInternalBorderStrokeFill", "quadrantExternalBorderStrokeFill", "quadrantTitleFill"};

// Split an object interior on its top-level commas; quoted spans and nested
// braces are tracked so a value comma or object separator never splits.
std::optional<std::vector<std::string_view>> quadrant_split_top(std::string_view text) {
    std::vector<std::string_view> parts;
    int depth = 0;
    char quote = 0;
    std::size_t start = 0;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char c = text[at];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == '{') {
            ++depth;
            continue;
        }
        if (c == '}') {
            if (depth == 0) return std::nullopt;
            --depth;
            continue;
        }
        if (c == ',' && depth == 0) {
            parts.push_back(text.substr(start, at - start));
            start = at + 1;
        }
    }
    if (quote != 0 || depth != 0) return std::nullopt;
    parts.push_back(text.substr(start));
    return parts;
}

bool quadrant_key(std::string_view text, std::string_view& out) {
    text = trim_view(text);
    if (text.size() >= 2u && text.front() == '"' && text.back() == '"')
        text = text.substr(1u, text.size() - 2u);
    if (text.empty() || text.find('"') != std::string_view::npos ||
        text.find('\'') != std::string_view::npos)
        return false;
    out = text;
    return true;
}

// A quoted or bare scalar; no nested structure and no quote or control bytes.
bool quadrant_scalar(std::string_view text, std::string_view& out) {
    text = trim_view(text);
    if (text.size() >= 2u && text.front() == '"' && text.back() == '"')
        text = text.substr(1u, text.size() - 2u);
    if (text.empty() || text.find('"') != std::string_view::npos ||
        text.find('\'') != std::string_view::npos ||
        text.find_first_of("{}[]") != std::string_view::npos)
        return false;
    for (const char c : text)
        if (static_cast<unsigned char>(c) < 0x20) return false;
    out = text;
    return true;
}

bool quadrant_uint(std::string_view value) {
    return !value.empty() && value.size() <= 9u &&
           std::ranges::all_of(value, [](char c) { return c >= '0' && c <= '9'; });
}

bool quadrant_color(std::string_view value) {
    if (value.size() != 7u || value.front() != '#') return false;
    return std::ranges::all_of(value.substr(1u), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

// D-457: a class name or a `:::class` annotation is an identifier.
bool quadrant_identifier(std::string_view value) {
    if (value.empty() || value.size() > 64u) return false;
    if (!((value.front() >= 'a' && value.front() <= 'z') ||
          (value.front() >= 'A' && value.front() <= 'Z') || value.front() == '_'))
        return false;
    for (const char c : value)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
            return false;
    return true;
}

// D-457: the closed point-style property list shared by `classDef` and the
// inline per-point suffix (`radius: 20`, `color: #ff0000`,
// `stroke-color: #ff00ff`, `stroke-width: 3px`), whitespace-tolerant around
// ',' and ':' and closed in both key and value shape. The terminal plots point
// labels only, so every property is inert, validated, dropped and disclosed.
bool quadrant_style_entry(std::string_view key, std::string_view value) {
    if (key == "radius") return quadrant_uint(value) && value.size() <= 3u;
    if (key == "color" || key == "stroke-color") return quadrant_color(value);
    if (key == "stroke-width")
        return value.size() >= 3u && value.size() <= 5u && value.ends_with("px") &&
               std::ranges::all_of(value.substr(0u, value.size() - 2u),
                                   [](char c) { return c >= '0' && c <= '9'; });
    return false;
}

std::optional<std::vector<std::pair<std::string, std::string>>> quadrant_style_list(
    std::string_view text) {
    std::vector<std::pair<std::string, std::string>> props;
    std::size_t pos = 0;
    while (true) {
        while (pos < text.size() && text[pos] == ' ') ++pos;
        if (pos >= text.size()) break;
        const auto colon = text.find(':', pos);
        if (colon == std::string_view::npos) return std::nullopt;
        const std::string_view key = trim_view(text.substr(pos, colon - pos));
        const auto comma = text.find(',', colon + 1);
        const std::string_view value =
            trim_view(text.substr(colon + 1, comma == std::string_view::npos
                                                 ? std::string_view::npos
                                                 : comma - colon - 1));
        if (key.empty() || !quadrant_style_entry(key, value)) return std::nullopt;
        for (const auto& [seen, ignored] : props)
            if (seen == key) return std::nullopt;
        props.emplace_back(std::string{key}, std::string{value});
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    if (props.empty()) return std::nullopt;
    return props;
}

bool quadrant_object_interior(std::string_view text, std::string_view& interior) {
    text = trim_view(text);
    if (text.size() < 2u || text.front() != '{' || text.back() != '}') return false;
    interior = text.substr(1u, text.size() - 2u);
    return true;
}

bool quadrant_tuning_object(std::string_view text, std::vector<std::string>& notices,
                            Work& work) {
    std::string_view interior;
    if (!quadrant_object_interior(text, interior)) return false;
    auto entries = quadrant_split_top(interior);
    if (!entries) return false;
    std::vector<std::string> seen;
    for (const std::string_view entry : *entries) {
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) return false;
        std::string_view key;
        if (!quadrant_key(entry.substr(0u, colon), key)) return false;
        const auto key_text = std::string{key};
        if (std::ranges::find(seen, key_text) != seen.end()) return false;
        seen.push_back(key_text);
        std::string_view value;
        if (!quadrant_scalar(entry.substr(colon + 1u), value)) return false;
        bool ok = false;
        if (key == "xAxisPosition")
            ok = value == "top" || value == "bottom";
        else if (key == "yAxisPosition")
            ok = value == "left" || value == "right";
        else
            ok = std::ranges::find(kQuadrantTuningKeys, key) != kQuadrantTuningKeys.end() &&
                 quadrant_uint(value);
        if (!ok) return false;
        if (!work.spend(key.size() + value.size() * 2u + 64u)) return false;
        std::string line = "%%{init}%% quadrantChart." + key_text + "=" + std::string{value} +
                           " -> SVG layout/axis tuning; not applied";
        if (line.size() > kQuadrantNoticeLimit) return false;
        notices.push_back(std::move(line));
    }
    return true;
}

bool quadrant_theme_object(std::string_view text, std::vector<std::string>& notices, Work& work) {
    std::string_view interior;
    if (!quadrant_object_interior(text, interior)) return false;
    auto entries = quadrant_split_top(interior);
    if (!entries) return false;
    std::vector<std::string> seen;
    for (const std::string_view entry : *entries) {
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) return false;
        std::string_view key;
        if (!quadrant_key(entry.substr(0u, colon), key)) return false;
        if (std::ranges::find(kQuadrantThemeKeys, key) == kQuadrantThemeKeys.end()) return false;
        const auto key_text = std::string{key};
        if (std::ranges::find(seen, key_text) != seen.end()) return false;
        seen.push_back(key_text);
        std::string_view value;
        if (!quadrant_scalar(entry.substr(colon + 1u), value) || !quadrant_color(value))
            return false;
        if (!work.spend(key.size() + value.size() * 2u + 64u)) return false;
        std::string line = "%%{init}%% themeVariables." + key_text + "=" + std::string{value} +
                           " -> colour palette; not applied";
        if (line.size() > kQuadrantNoticeLimit) return false;
        notices.push_back(std::move(line));
    }
    return true;
}

// True when `line` is an admitted directive. Every admitted key appends one
// notice; the empty `{}` form is a no-op.
bool quadrant_init_directive(std::string_view line, std::vector<std::string>& notices,
                             Work& work) {
    line = trim_view(line);
    if (!line.starts_with("%%{") || !line.ends_with("}%%") || line.size() < 8u) return false;
    const std::string_view inner = line.substr(3u, line.size() - 6u);
    if (!work.spend(inner.size() * 4u + 256u)) return false;
    const std::string_view directive = trim_view(inner);
    if (directive == "init") return true;  // bare no-op directive
    if (!directive.starts_with("init:")) return false;
    std::string_view payload = trim_view(directive.substr(5u));
    std::string_view interior;
    if (!quadrant_object_interior(payload, interior)) return false;
    if (trim_view(interior).empty()) return true;
    auto entries = quadrant_split_top(interior);
    if (!entries) return false;
    bool saw_tuning = false, saw_theme = false;
    for (const std::string_view entry : *entries) {
        const auto colon = entry.find(':');
        if (colon == std::string_view::npos) return false;
        std::string_view key;
        if (!quadrant_key(entry.substr(0u, colon), key)) return false;
        const std::string_view value_text = entry.substr(colon + 1u);
        if (key == "quadrantChart") {
            if (saw_tuning) return false;
            saw_tuning = true;
            if (!quadrant_tuning_object(value_text, notices, work)) return false;
        } else if (key == "themeVariables") {
            if (saw_theme) return false;
            saw_theme = true;
            if (!quadrant_theme_object(value_text, notices, work)) return false;
        } else {
            return false;
        }
    }
    return true;
}

// Truncate `text` to at most `budget` bytes on a UTF-8 codepoint boundary: a
// sequence cut mid-way is dropped whole, so the disclosure never carries an
// invalid byte sequence.
void quadrant_truncate_utf8(std::string& text, std::size_t budget) {
    if (text.size() <= budget) return;
    text.resize(budget);
    std::size_t at = text.size();
    while (at > 0 && (static_cast<unsigned char>(text[at - 1]) & 0xC0u) == 0x80u) --at;
    if (at == 0) {
        text.clear();
        return;
    }
    const auto lead = static_cast<unsigned char>(text[at - 1]);
    const std::size_t expected =
        lead >= 0xF0u ? 4u : lead >= 0xE0u ? 3u : lead >= 0xC0u ? 2u : 1u;
    if (text.size() - (at - 1) < expected) text.erase(at - 1);
}

}  // namespace

std::optional<std::vector<std::string>> quadrant_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!quadrant_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    std::string_view body = quadrant_body(source);
    // D-450: consume an admitted leading `%%{init: {...}}%%` directive before
    // the visual gate. A directive anywhere else, a second one, or one that
    // fails the closed grammar keeps the whole source on fallback.
    std::vector<std::string> notices;
    {
        const auto eol = body.find('\n');
        std::string_view first =
            body.substr(0u, eol == std::string_view::npos ? body.size() : eol);
        if (!first.empty() && first.back() == '\r') first.remove_suffix(1);
        const auto first_trimmed = trim_view(first);
        if (first_trimmed.starts_with("%%{init")) {
            if (!quadrant_init_directive(first_trimmed, notices, work)) return {};
            body = eol == std::string_view::npos ? std::string_view{} : body.substr(eol + 1u);
        }
    }
    // No geometry/color contract: a frontmatter theme/look/config key or a
    // non-leading %%{init}%% directive keeps the whole source on fallback.
    // D-457 admits the body `classDef` statements and the per-point `:::class`
    // / inline style suffixes, validated and dropped by the loop below.
    // Key/shape-based so prose words are not false gates.
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
                    if (content.starts_with("%%{init")) visual_gate = true;
                }
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }

    bool saw_header = false;
    std::string title;
    int statement_count = 0;
    int point_count = 0;
    std::vector<std::string> title_rows;
    std::vector<std::string> out;
    // One guarded push for the title and every statement row (rule 22).
    auto push_guarded = [&](std::vector<std::string>& target, const std::string& row) -> bool {
        if (title_rows.size() + out.size() >= kQuadrantMaxRows ||
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
        const std::string_view content = line.substr(trimmed_pos);
        if (content.starts_with("%%")) continue;

        if (!saw_header) {
            if (content != "quadrantChart") return {};
            saw_header = true;
            continue;
        }
        if (++statement_count > kQuadrantMaxStatements) return {};

        if (content.starts_with("title ")) {
            if (!title.empty()) return {};  // duplicate title
            if (content.find(":::") != std::string_view::npos) return {};
            title = trim_copy(content.substr(6));
            if (title.empty() || title.size() > kQuadrantMaxNameBytes ||
                !unicode_text(title))
                return {};
            std::vector<std::string> wrapped;
            if (!quadrant_wrap(title, work, wrapped)) return {};
            for (const auto& row : wrapped)
                if (!push_guarded(title_rows, row)) return {};
            continue;
        }

        if (content.starts_with("x-axis ") || content.starts_with("y-axis ")) {
            if (content.find(":::") != std::string_view::npos) return {};
            const std::string axis(content.substr(0, 6));
            const std::string label = trim_copy(content.substr(7));
            if (label.empty() || label.size() > kQuadrantMaxNameBytes ||
                !unicode_text(label))
                return {};
            std::vector<std::string> wrapped;
            if (!quadrant_wrap(axis + ": " + label, work, wrapped)) return {};
            for (const auto& row : wrapped)
                if (!push_guarded(out, row)) return {};
            continue;
        }

        if (content.starts_with("quadrant-")) {
            // quadrant-1 .. quadrant-4 followed by a label.
            if (content.find(":::") != std::string_view::npos) return {};
            const auto space = content.find(' ');
            if (space == std::string_view::npos) return {};
            const std::string_view number = content.substr(9, space - 9);
            if (number != "1" && number != "2" && number != "3" && number != "4")
                return {};
            const std::string label = trim_copy(content.substr(space + 1));
            if (label.empty() || label.size() > kQuadrantMaxNameBytes ||
                !unicode_text(label))
                return {};
            std::vector<std::string> wrapped;
            if (!quadrant_wrap("Q" + std::string{number} + ": " + label, work, wrapped))
                return {};
            for (const auto& row : wrapped)
                if (!push_guarded(out, row)) return {};
            continue;
        }

        // D-457: a body `classDef <name> <closed props>` statement is inert
        // styling. It is validated, dropped from the outline and disclosed.
        if (content.starts_with("classDef ")) {
            const std::string_view rest = trim_view(content.substr(9u));
            const auto space = rest.find(' ');
            if (space == std::string_view::npos) return {};
            const std::string_view name = rest.substr(0, space);
            if (!quadrant_identifier(name)) return {};
            if (!quadrant_style_list(rest.substr(space + 1))) return {};
            const std::string line =
                "body classDef=" + std::string{name} + " -> colour/radius; not applied";
            if (line.size() > kQuadrantNoticeLimit || !work.spend(line.size() * 2u + 32u))
                return {};
            notices.push_back(line);
            continue;
        }

        // Data point: "<name>[: :::class]: [x, y][ <closed style list>]".
        const auto bracket_open = content.find('[');
        const auto bracket_close = content.find(']');
        if (bracket_open == std::string_view::npos ||
            bracket_close == std::string_view::npos || bracket_close < bracket_open)
            return {};
        std::string_view prefix = trim_view(content.substr(0, bracket_open));
        if (prefix.empty() || prefix.back() != ':') return {};
        prefix.remove_suffix(1);
        std::string_view name = trim_view(prefix);
        std::string_view point_class;
        const auto class_mark = name.rfind(":::");
        if (class_mark != std::string_view::npos) {
            point_class = name.substr(class_mark + 3u);
            if (class_mark == 0u || !quadrant_identifier(point_class)) return {};
            name = trim_view(name.substr(0, class_mark));
            if (name.find(":::") != std::string_view::npos) return {};
        }
        // The name is the single field before the `:` separator; an inner
        // `:` (e.g. `A: B: [0.1, 0.2]`) is outside the admitted grammar.
        if (name.empty() || name.size() > kQuadrantMaxNameBytes ||
            name.find(':') != std::string_view::npos || !unicode_text(std::string{name}))
            return {};
        const std::string_view coords =
            content.substr(bracket_open + 1u, bracket_close - bracket_open - 1u);
        const auto comma = coords.find(',');
        if (comma == std::string_view::npos) return {};
        const std::string_view x = trim_view(coords.substr(0, comma));
        const std::string_view y = trim_view(coords.substr(comma + 1u));
        if (!is_coordinate(x) || !is_coordinate(y)) return {};
        const std::string_view style_text = trim_view(content.substr(bracket_close + 1u));
        std::optional<std::vector<std::pair<std::string, std::string>>> styles;
        if (!style_text.empty()) {
            styles = quadrant_style_list(style_text);
            if (!styles) return {};
        }
        if (++point_count > kQuadrantMaxPoints) return {};
        if (!point_class.empty() || styles) {
            std::string tail = " -> ";
            if (!point_class.empty()) tail += "class=" + std::string{point_class};
            if (!point_class.empty() && styles) tail += ", ";
            if (styles) tail += "styles=" + std::to_string(styles->size());
            tail += "; not applied";
            std::string disclosed{name};
            if (kQuadrantNoticePrefix + disclosed.size() + tail.size() >
                kQuadrantNoticeLimit) {
                // Truncate on a codepoint boundary so a long free-text name
                // never turns an otherwise admitted row into a fallback.
                const std::size_t marker = 3u;  // "..."
                if (tail.size() + kQuadrantNoticePrefix + marker >= kQuadrantNoticeLimit)
                    return {};
                const std::size_t budget =
                    kQuadrantNoticeLimit - tail.size() - kQuadrantNoticePrefix - marker;
                quadrant_truncate_utf8(disclosed, budget);
                disclosed += "...";
            }
            const std::string line = "point " + disclosed + tail;
            if (line.size() > kQuadrantNoticeLimit || !work.spend(line.size() * 2u + 32u))
                return {};
            notices.push_back(line);
        }

        const std::string outline =
            std::string{name} + " (" + std::string{x} + ", " + std::string{y} + ")";
        std::vector<std::string> wrapped;
        if (!quadrant_wrap(outline, work, wrapped)) return {};
        for (const auto& row : wrapped)
            if (!push_guarded(out, row)) return {};
    }

    if (!saw_header) return {};
    // A header-only chart is a valid empty frame in the official corpus. Its
    // only statement is the keyword, so transcribe that one token verbatim
    // rather than inventing prose or dropping the source onto fallback.
    if (title_rows.empty() && out.empty()) {
        if (!push_guarded(out, "quadrantChart")) return {};
    }
    if (!title_rows.empty()) {
        std::vector<std::string> combined;
        combined.reserve(title_rows.size() + out.size());
        combined.insert(combined.end(), title_rows.begin(), title_rows.end());
        combined.insert(combined.end(), out.begin(), out.end());
        out = std::move(combined);
    }
    if (out.empty()) return {};
    // D-450 disclosure footer: header plus one line per admitted key, below
    // the art and above nothing else.
    if (!notices.empty()) {
        if (!push_guarded(out, std::string{})) return {};
        if (!push_guarded(out, std::string{kQuadrantNoticeHeader})) return {};
        for (const auto& notice : notices)
            if (!push_guarded(out, notice)) return {};
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
