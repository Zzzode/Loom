module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A user-journey diagram is a structured table: a title, a sequence of
// sections, and tasks each carrying a 0..5 score and an optional actor list.
// There are no edges, shapes or date math, so the projection is bounded text
// layout. Every limit is checked against the single prepaid Work meter.

constexpr int kJourneyMaxSections = 32;
constexpr int kJourneyMaxTasksPerSection = 128;
constexpr int kJourneyMaxActors = 8;
constexpr int kJourneyWrapWidth = 64;
constexpr int kJourneyMaxRows = 512;

struct JourneyTask {
    std::string label;
    int score = 0;
    std::vector<std::string> actors;
};

struct JourneySection {
    std::string name;
    std::vector<JourneyTask> tasks;
};

bool journey_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

// Strip a YAML frontmatter block (mirrors the other renderers' body split) and
// return the diagram body. A missing frontmatter leaves the source unchanged.
std::string_view journey_body(std::string_view source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string_view::npos) return source;
    return source.substr(close + 5);
}

std::string trim_copy(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return std::string(value);
}

// Word-wrap a label to the bounded width, charging every emitted row.
bool journey_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kJourneyMaxRows || !work.spend(8)) return false;
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
        if (word_cells > kJourneyWrapWidth) return false;  // no hard mid-word split
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kJourneyWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

}  // namespace

std::optional<std::vector<std::string>> journey_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!journey_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = journey_body(source);
    // The terminal has no color/theme contract, so a themed journey stays on
    // ordinary whole-source fallback. Theme can arrive as a frontmatter
    // config leaf (theme/look) or as an in-body %%{init}%% directive. Both
    // checks are key-based, not whole-text substring scans, so a task label
    // that happens to contain the word "theme" is never falsely rejected.
    {
        bool visual_gate = false;
        if (source.starts_with("---\n")) {
            const auto close = source.find("\n---\n", 4);
            if (close != std::string_view::npos) {
                const std::string_view frontmatter = source.substr(0, close);
                for (std::size_t pos = 0;;) {
                    const auto eol = frontmatter.find('\n', pos);
                    std::string_view line = frontmatter.substr(
                        pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
                    pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1;
                    const auto key_start = line.find_first_not_of(' ');
                    if (key_start != std::string_view::npos) {
                        std::string_view key = line.substr(key_start);
                        const auto colon = key.find(':');
                        if (colon != std::string_view::npos) key = key.substr(0, colon);
                        if (key == "theme" || key == "look") visual_gate = true;
                    }
                    if (eol == std::string_view::npos) break;
                }
            }
        }
        if (!visual_gate) {
            for (std::size_t pos = 0;;) {
                const auto eol = body.find('\n', pos);
                std::string_view line = body.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
                pos = eol == std::string_view::npos ? body.size() : eol + 1;
                const auto trimmed = line.find_first_not_of(' ');
                if (trimmed != std::string_view::npos &&
                    line.substr(trimmed).starts_with("%%{") &&
                    line.find("theme") != std::string_view::npos)
                    visual_gate = true;
                if (eol == std::string_view::npos) break;
            }
        }
        if (visual_gate) return {};
    }
    std::vector<JourneySection> sections;
    std::string title;
    bool saw_header = false;

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                            : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);  // CRLF tolerance
        while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
        if (line.empty()) continue;
        if (line.starts_with("%%")) continue;  // comments and init directives
        if (line.starts_with("accTitle:") || line.starts_with("accDescr:")) continue;
        if (line.starts_with("size:")) continue;

        if (!saw_header) {
            if (line != "journey") return {};
            saw_header = true;
            continue;
        }
        if (line.starts_with("title ")) {
            if (!title.empty()) return {};
            title = trim_copy(line.substr(6));
            if (title.empty() || title.size() > 256 || !unicode_text(title)) return {};
            continue;
        }
        if (line.starts_with("section ")) {
            if (sections.size() >= kJourneyMaxSections) return {};
            std::string section_name = trim_copy(line.substr(8));
            if (section_name.empty() || section_name.size() > 256 ||
                !unicode_text(section_name))
                return {};
            sections.push_back({std::move(section_name), {}});
            continue;
        }
        // Task line: Label : score [: actor, actor]
        const auto first = line.find(':');
        if (first == std::string_view::npos) return {};  // unknown statement
        if (sections.empty()) return {};                  // a task needs a section
        JourneyTask task;
        task.label = trim_copy(line.substr(0, first));
        if (task.label.empty() || task.label.size() > 256 || !unicode_text(task.label))
            return {};
        std::string_view rest = line.substr(first + 1);
        const auto second = rest.find(':');
        std::string_view score_field = second == std::string_view::npos ? rest : rest.substr(0, second);
        std::string score_text = trim_copy(score_field);
        if (score_text.empty() || !std::ranges::all_of(score_text, [](char c) {
                return c >= '0' && c <= '9';
            }))
            return {};
        // Non-throwing parse: an over-long digit run must fail closed instead
        // of escaping std::stoi's std::out_of_range through the public renderer.
        int parsed_score = 0;
        const auto parse_result = std::from_chars(
            score_text.data(), score_text.data() + score_text.size(), parsed_score);
        if (parse_result.ec != std::errc{} ||
            parse_result.ptr != score_text.data() + score_text.size() ||
            parsed_score < 0 || parsed_score > 5)
            return {};
        task.score = parsed_score;
        if (second != std::string_view::npos) {
            std::string_view actors_field = rest.substr(second + 1);
            std::size_t a_start = 0;
            while (a_start <= actors_field.size()) {
                std::size_t a_end = actors_field.find(',', a_start);
                std::string_view part = actors_field.substr(
                    a_start, a_end == std::string_view::npos ? std::string_view::npos
                                                              : a_end - a_start);
                a_start = a_end == std::string_view::npos ? actors_field.size() + 1 : a_end + 1;
                std::string actor = trim_copy(part);
                if (actor.empty()) continue;
                if (task.actors.size() >= kJourneyMaxActors || actor.size() > 128 ||
                    !unicode_text(actor))
                    return {};
                task.actors.push_back(std::move(actor));
            }
        }
        if (sections.back().tasks.size() >= kJourneyMaxTasksPerSection) return {};
        sections.back().tasks.push_back(std::move(task));
    }

    if (!saw_header) return {};

    std::vector<std::string> out;
    auto emit = [&](std::string row) -> bool {
        if (out.size() >= kJourneyMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(std::move(row));
        return true;
    };

    if (!title.empty() && !emit(title)) return {};
    for (const auto& section : sections) {
        if (!emit("")) return {};
        if (!emit("Section: " + section.name)) return {};
        for (const auto& task : section.tasks) {
            std::vector<std::string> wrapped;
            if (!journey_wrap(task.label, work, wrapped)) return {};
            for (std::size_t i = 0; i < wrapped.size(); ++i) {
                const std::string prefix = i == 0 ? "  - " : "    ";
                if (!emit(prefix + wrapped[i])) return {};
            }
            if (!emit("    score: " + std::to_string(task.score))) return {};
            if (!task.actors.empty()) {
                std::string joined = "    actors: ";
                for (std::size_t i = 0; i < task.actors.size(); ++i) {
                    if (i) joined += ", ";
                    joined += task.actors[i];
                }
                if (!emit(std::move(joined))) return {};
            }
        }
    }
    if (out.empty()) return {};
    return out;
}

}  // namespace loom::ui::mermaid::detail
