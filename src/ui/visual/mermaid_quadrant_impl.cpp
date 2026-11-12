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

    const std::string_view body = quadrant_body(source);
    // No geometry/color contract: a frontmatter theme/look/config key, an
    // %%{init}%% directive (axis position, size, font, fill), a classDef/:::
    // style, or an inline point style keeps the whole source on fallback.
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
    return out;
}

}  // namespace loom::ui::mermaid::detail
