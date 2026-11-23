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

// Inline point styling appears after the coordinate bracket, e.g.
// "[0.3, 0.6] radius: 20" or "[..] stroke-color  : #ff00ff". Shape/key-based:
// a key must start on a non-identifier boundary and end at an optional-space
// colon, so prose mentioning "color" never trips the gate. Longer keys first;
// the '-' boundary check also blocks "color" matching inside "stroke-color".
bool has_point_style_keyword(std::string_view suffix) {
    static constexpr std::string_view kKeys[] = {"stroke-color", "stroke-width", "radius",
                                                  "color"};
    for (const std::string_view key : kKeys) {
        std::size_t pos = 0;
        while ((pos = suffix.find(key, pos)) != std::string_view::npos) {
            const char before = pos == 0 ? ' ' : suffix[pos - 1];
            if (!((before >= 'a' && before <= 'z') || (before >= 'A' && before <= 'Z') ||
                  (before >= '0' && before <= '9') || before == '_' || before == '-')) {
                std::size_t k = pos + key.size();
                while (k < suffix.size() && suffix[k] == ' ') ++k;
                if (k < suffix.size() && suffix[k] == ':') return true;
            }
            pos += key.size();
        }
    }
    return false;
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
        notices.push_back("%%{init}%% quadrantChart." + key_text + "=" + std::string{value} +
                          " -> SVG layout/axis tuning; not applied");
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
        notices.push_back("%%{init}%% themeVariables." + key_text + "=" + std::string{value} +
                          " -> colour palette; not applied");
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
    // No geometry/color contract: a frontmatter theme/look/config key, any
    // other %%{init}%% directive (axis position, size, font, fill), a
    // classDef/::: style, or an inline point style keeps the whole source on
    // fallback. Key/shape-based so prose words are not false gates.
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
                    if (content.starts_with("%%{init") ||
                        content.starts_with("classDef") ||
                        content.find(":::") != std::string_view::npos)
                        visual_gate = true;
                    // Inline per-point style suffix after the coordinate pair.
                    const auto bracket = content.find(']');
                    if (!visual_gate && bracket != std::string_view::npos &&
                        has_point_style_keyword(content.substr(bracket + 1)))
                        visual_gate = true;
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

        // Data point: "<name>: [x, y]" with nothing but the coordinate pair.
        const auto colon = content.find(':');
        if (colon == std::string_view::npos) return {};  // unknown statement
        const std::string name = trim_copy(content.substr(0, colon));
        if (name.empty() || name.size() > kQuadrantMaxNameBytes || !unicode_text(name))
            return {};
        std::string_view tail = trim_view(content.substr(colon + 1));
        if (tail.size() < 2 || tail.front() != '[' || tail.back() != ']') return {};
        const auto comma = tail.find(',');
        if (comma == std::string_view::npos) return {};
        const std::string_view x = trim_view(tail.substr(1, comma - 1));
        const std::string_view y = trim_view(tail.substr(comma + 1, tail.size() - comma - 2));
        if (!is_coordinate(x) || !is_coordinate(y)) return {};
        if (++point_count > kQuadrantMaxPoints) return {};

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
