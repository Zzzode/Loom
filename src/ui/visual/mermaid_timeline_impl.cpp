module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A timeline is a sequence of named time periods, each holding one or more
// events, optionally grouped into sections. The horizontal/vertical (TD)
// orientation distinction has no text-table equivalent, so the projection is
// a faithful linear structural listing rather than an SVG geometry claim.

constexpr int kTimelineMaxSections = 32;
constexpr int kTimelineMaxPeriods = 256;
constexpr int kTimelineMaxEventsPerPeriod = 16;  // corpus max under colon split is 9
constexpr int kTimelineWrapWidth = 64;
constexpr int kTimelineMaxRows = 512;

struct TimelinePeriod {
    std::string name;
    std::vector<std::string> events;
};

struct TimelineSection {
    std::string name;  // empty for the implicit pre-section block
    std::vector<TimelinePeriod> periods;
};

bool timeline_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view timeline_body(std::string_view source) {
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

// Case-insensitive marker match at a fixed position.
bool iequals_at(std::string_view text, std::size_t at, std::string_view marker) {
    if (at + marker.size() > text.size()) return false;
    for (std::size_t i = 0; i < marker.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[at + i])) !=
            std::tolower(static_cast<unsigned char>(marker[i])))
            return false;
    }
    return true;
}

// Replace <br>/<br/>/<br /> with a space, case-insensitively like upstream
// HTML tag matching, so an event wraps as plain text rather than leaking
// markup. Markers are probed longest-first so "<br>" never eats the prefix
// of "<br/>".
std::string sanitize_event(std::string text) {
    static const std::array<std::string_view, 3> kBreaks{"<br/>", "<br />", "<br>"};
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        std::size_t matched = 0;
        if (text[i] == '<') {
            for (const auto marker : kBreaks) {
                if (iequals_at(text, i, marker)) {
                    matched = marker.size();
                    break;
                }
            }
        }
        if (matched != 0) {
            out.push_back(' ');
            i += matched;
        } else {
            out.push_back(text[i]);
            ++i;
        }
    }
    return out;
}

bool timeline_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kTimelineMaxRows || !work.spend(8)) return false;
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
        if (word_cells > kTimelineWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kTimelineWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}

}  // namespace

std::optional<std::vector<std::string>> timeline_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!timeline_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = timeline_body(source);
    // No color contract: a theme/look frontmatter leaf or an in-body themed
    // init directive keeps the whole source on ordinary fallback. Key-based,
    // so a period/event containing the word "theme" is never falsely rejected.
    // This gate intentionally mirrors journey_layout's visual contract
    // exactly (same frontmatter key scan and in-body directive scan); the two
    // renderers must not drift apart here. An independent corpus re-scan
    // (generator is_themed) matches this gate for all 30 timeline originals.
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

    std::vector<TimelineSection> sections;
    sections.push_back({"", {}});  // implicit block before the first section
    std::string title;
    bool saw_header = false;

    std::size_t line_start = 0;
    while (line_start <= body.size()) {
        std::size_t line_end = body.find('\n', line_start);
        std::string_view line = body.substr(
            line_start, line_end == std::string_view::npos ? std::string_view::npos
                                                            : line_end - line_start);
        line_start = line_end == std::string_view::npos ? body.size() + 1 : line_end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        while (!line.empty() && line.front() == ' ') line.remove_prefix(1);
        if (line.empty()) continue;
        if (line.starts_with("%%")) continue;
        if (line.starts_with("accTitle:") || line.starts_with("accDescr:")) continue;

        if (!saw_header) {
            if (line != "timeline" && line != "timeline TD") return {};
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
            // The vector already holds the implicit pre-section block, so this
            // admits at most 31 named sections under the 32-entry cap; the
            // corpus maximum is 3 and overflow fails closed (review NIT 1).
            if (sections.size() >= kTimelineMaxSections) return {};
            std::string name = trim_copy(line.substr(8));
            if (name.empty() || name.size() > 256 || !unicode_text(name)) return {};
            sections.push_back({std::move(name), {}});
            continue;
        }

        const auto first_colon = line.find(':');
        if (first_colon == std::string_view::npos) return {};  // unknown statement
        auto& section = sections.back();
        // Append one or more colon-separated, sanitized events to a period.
        // Both period lines and continuation lines split uniformly on ':',
        // matching the upstream timeline grammar.
        const auto append_events = [&](TimelinePeriod& period,
                                       std::string_view field) -> bool {
            while (true) {
                const auto next = field.find(':');
                std::string event = sanitize_event(trim_copy(field.substr(0, next)));
                if (!event.empty()) {
                    if (event.size() > 256 || !unicode_text(event)) return false;
                    if (period.events.size() >= kTimelineMaxEventsPerPeriod) return false;
                    period.events.push_back(std::move(event));
                }
                if (next == std::string_view::npos) break;
                field = field.substr(next + 1);
            }
            return true;
        };
        const bool continuation = line.starts_with(':');
        if (continuation) {
            // A leading colon appends event(s) to the most recent period.
            if (section.periods.empty()) return {};
            if (!append_events(section.periods.back(), line.substr(1))) return {};
            continue;
        }

        std::string period_name = trim_copy(line.substr(0, first_colon));
        if (period_name.empty() || period_name.size() > 128 || !unicode_text(period_name))
            return {};
        if (section.periods.size() >= kTimelineMaxPeriods) return {};
        TimelinePeriod period{period_name, {}};
        if (!append_events(period, line.substr(first_colon + 1))) return {};
        if (period.events.empty()) return {};
        section.periods.push_back(std::move(period));
    }

    if (!saw_header) return {};

    std::vector<std::string> out;
    auto emit = [&](std::string row) -> bool {
        if (out.size() >= kTimelineMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(std::move(row));
        return true;
    };

    if (!title.empty() && !emit(title)) return {};
    for (const auto& section : sections) {
        if (section.periods.empty()) continue;
        if (!emit("")) return {};
        if (!section.name.empty() && !emit("Section: " + section.name)) return {};
        for (const auto& period : section.periods) {
            if (!emit("- " + period.name)) return {};
            for (const auto& event : period.events) {
                std::vector<std::string> wrapped;
                if (!timeline_wrap(event, work, wrapped)) return {};
                for (const auto& row : wrapped)
                    if (!emit("    * " + row)) return {};
            }
        }
    }
    if (out.empty()) return {};
    return out;
}

}  // namespace loom::ui::mermaid::detail
