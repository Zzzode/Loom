module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// A gantt diagram is a schedule: sections of tasks carrying status tags,
// optional ids, absolute/epoch/clock start points, durations, and after/until
// dependencies. The terminal has no time axis, so the projection is a faithful
// structural listing: tokens are strictly shape-validated and transcribed, but
// no calendar math, exclude resolution or dependency scheduling is performed.

constexpr int kGanttMaxSections = 32;
constexpr int kGanttMaxTasksPerSection = 128;
constexpr int kGanttMaxFields = 16;
constexpr int kGanttMaxIds = 8;
constexpr int kGanttWrapWidth = 64;
constexpr int kGanttMaxRows = 512;

struct GanttTask {
    std::string label;
    std::vector<std::string> tags;
    std::vector<std::string> ids;
    std::string start;  // point text, "after ...", or empty for chained
    std::string span;   // duration, "end <point>", "until ...", "milestone", or empty
};

struct GanttSection {
    std::string name;  // empty for the implicit pre-section block
    std::vector<GanttTask> tasks;
};

bool gantt_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    if (count * rate > std::numeric_limits<std::size_t>::max() - total) return false;
    total += count * rate;
    return true;
}

std::string_view gantt_body(std::string_view source) {
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

char lower_ascii(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool iequals(std::string_view text, std::string_view expected) {
    return text.size() == expected.size() &&
           std::ranges::equal(text, expected, [](char a, char b) {
               return lower_ascii(a) == lower_ascii(b);
           });
}

// Case-insensitive "<keyword> " prefix used by the after/until scheduling
// fields; the trailing space separates the keyword from its dependency ids.
bool starts_keyword(std::string_view field, std::string_view keyword) {
    return field.size() > keyword.size() &&
           iequals(field.substr(0, keyword.size()), keyword) &&
           field[keyword.size()] == ' ';
}

// Upstream break tags accept horizontal whitespace and an optional slash:
// <br>, <br/>, <br />, and the corpus's tab form <br\t/>. Replace any such
// marker (case-insensitive) with a space so the only tabs left are real
// structural tabs that the later scan rejects.
std::string sanitize_breaks(std::string text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        std::size_t matched = 0;
        if (text[i] == '<' && i + 2 < text.size() && lower_ascii(text[i + 1]) == 'b' &&
            lower_ascii(text[i + 2]) == 'r') {
            std::size_t j = i + 3;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t')) ++j;
            if (j < text.size() && text[j] == '/') ++j;
            while (j < text.size() && (text[j] == ' ' || text[j] == '\t')) ++j;
            if (j < text.size() && text[j] == '>') matched = j - i + 1;
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

bool all_digits(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

// A duration is digits, optionally one decimal point, then ms/s/m/h/d/w.
bool is_duration(std::string_view s) {
    const auto unit = [](char c) {
        return c == 'm' || c == 's' || c == 'h' || c == 'd' || c == 'w';
    };
    if (s.size() < 2 || !unit(s.back())) return false;
    std::string_view number = s.substr(0, s.size() - 1);
    if (s.size() >= 3 && s.substr(s.size() - 2) == "ms") number = s.substr(0, s.size() - 2);
    if (number.empty()) return false;
    bool seen_dot = false;
    for (char c : number) {
        if (c == '.') {
            if (seen_dot) return false;
            seen_dot = true;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    return number.front() != '.' && number.back() != '.';
}

bool two_digits(std::string_view s) {
    return s.size() == 2 && all_digits(s);
}

// HH:mm or HH:mm:ss. Only the two-digit field shape is validated, not the
// numeric range (no calendar interpretation is performed by design).
bool is_clock(std::string_view s) {
    const auto first_colon = s.find(':');
    if (first_colon == std::string_view::npos) return false;
    const auto hh = s.substr(0, first_colon);
    const auto rest = s.substr(first_colon + 1);
    if (!two_digits(hh)) return false;
    const auto second_colon = rest.find(':');
    if (second_colon == std::string_view::npos) return two_digits(rest);
    return two_digits(rest.substr(0, second_colon)) &&
           two_digits(rest.substr(second_colon + 1));
}

// Strict YYYY-MM-DD, optionally followed by a space and HH:mm:ss. The exact
// 4-digit year and zero-padded 2-digit month/day shapes are what reject the
// two corpus malformed-date fixtures (3-digit year, 1-digit day) without any
// calendar interpretation.
bool is_calendar(std::string_view s) {
    if (s.size() < 10 || s[4] != '-' || s[7] != '-') return false;
    if (!all_digits(s.substr(0, 4)) || !two_digits(s.substr(5, 2)) ||
        !two_digits(s.substr(8, 2)))
        return false;
    const int month = (s[5] - '0') * 10 + (s[6] - '0');
    const int day = (s[8] - '0') * 10 + (s[9] - '0');
    if (month < 1 || month > 12 || day < 1 || day > 31) return false;
    if (s.size() == 10) return true;
    if (s[10] != ' ') return false;
    return is_clock(s.substr(11));
}

// Epoch/ordinal values used under dateFormat x/X/SSS/ss/D are plain numbers.
bool is_point(std::string_view s) {
    return is_calendar(s) || is_clock(s) || all_digits(s);
}

bool ident_head(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool ident_tail(char c) { return ident_head(c) || (c >= '0' && c <= '9') || c == '-'; }

bool is_identifier(std::string_view s) {
    if (s.empty() || s.size() > 64) return false;
    if (!ident_head(s.front())) return false;
    return std::ranges::all_of(s.substr(1), ident_tail);
}

bool is_tag(std::string_view s) {
    return iequals(s, "done") || iequals(s, "active") || iequals(s, "crit") ||
           iequals(s, "milestone");
}

// Split "after a b c" / "until x y" into the keyword and its id list; every
// dependency token must be an identifier. Dependencies are transcribed, never
// resolved.
bool dependency_field(std::string_view field, std::string_view keyword,
                      std::vector<std::string>& ids) {
    if (field.size() <= keyword.size() ||
        !iequals(field.substr(0, keyword.size()), keyword) ||
        field[keyword.size()] != ' ')
        return false;
    std::string_view rest = field.substr(keyword.size() + 1);
    while (!rest.empty()) {
        const auto space = rest.find(' ');
        const auto token = rest.substr(0, space);
        if (!is_identifier(token) || ids.size() >= kGanttMaxIds) return false;
        ids.emplace_back(token);
        rest = space == std::string_view::npos ? std::string_view{} : rest.substr(space + 1);
    }
    return !ids.empty();
}

bool gantt_wrap(const std::string& label, Work& work, std::vector<std::string>& out) {
    std::size_t byte = 0;
    std::string current;
    int cells = 0;
    auto flush = [&]() -> bool {
        if (current.empty()) return true;
        if (out.size() >= kGanttMaxRows || !work.spend(8)) return false;
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
        if (word_cells > kGanttWrapWidth) return false;
        const int gap = cells == 0 ? 0 : 1;
        if (cells + gap + word_cells > kGanttWrapWidth) {
            if (!flush()) return false;
        }
        if (!current.empty()) current.push_back(' ');
        current += word;
        cells += word_cells + gap;
    }
    return flush();
}


// RFC 0040: the shared "no color contract" gate becomes a validating one. A
// `theme`/`look` key at indent 2 inside the single `config:` block is validated
// against this family's closed corpus table and disclosed through a footer
// below the art; a visual key anywhere else (top level, inside a
// `description:` payload, or at indent 4 or deeper) keeps the whole source, as
// does an in-body %%{...theme...}%% initializer. Every other frontmatter line
// is ignored (the three renderers' long-standing pass-through contract).
constexpr std::size_t kJourneyFamNoticeLineLimit = 96u;
constexpr std::string_view kFamilyNoticeHeader = "Gantt native projection v1";
constexpr std::array<std::string_view, 0> kFamilyLookTable{};
constexpr std::array<std::string_view, 8> kFamilyThemeTable{"dark", "default", "forest", "neutral", "redux", "redux-dark", "redux-color", "redux-dark-color"};

std::optional<std::string> journey_fam_scalar(std::string_view raw, Work& work) {
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

bool journey_fam_notice_line(std::vector<std::string>& notices, std::string_view line, Work& work) {
    if (line.size() > kJourneyFamNoticeLineLimit ||
        !work.spend(line.size() * 4u + sizeof(std::string) + 32u))
        return false;
    notices.emplace_back(line);
    return true;
}

// nullopt: the whole source stays whole-source. Otherwise the bounded
// disclosure footer (possibly empty), emitted below the art by the caller.
std::optional<std::vector<std::string>> journey_fam_visual_config(
    std::string_view source, std::string_view header,
    std::span<const std::string_view> looks, std::span<const std::string_view> themes,
    Work& work) {
    const auto matches = [](std::span<const std::string_view> table,
                            const std::string& value) {
        return std::ranges::find(table, value) != table.end();
    };
    std::string look_value, theme_value;
    enum : unsigned { Look = 1u, Theme = 2u };
    unsigned fields = 0u;
    std::string_view body = source;
    if (source.starts_with("---\n")) {
        const auto close = source.find("\n---\n", 4);
        if (close != std::string_view::npos) {
            body = source.substr(close + 5u);
            bool in_config = false;
            const std::string_view frontmatter = source.substr(4u, close - 4u);
            for (std::size_t pos = 0;;) {
                const auto eol = frontmatter.find('\n', pos);
                std::string_view line = frontmatter.substr(
                    pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
                pos = eol == std::string_view::npos ? frontmatter.size() : eol + 1u;
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1u);
                const auto indent = line.find_first_not_of(' ');
                if (indent != std::string_view::npos) {
                    const bool column0 = indent == 0u;
                    const auto content = line.substr(indent);
                    const auto colon = content.find(':');
                    if (column0) {
                        if (!content.starts_with('#')) {
                            const auto key =
                                colon == std::string_view::npos ? content
                                                                : content.substr(0, colon);
                            // A visual key outside the config block (top level
                            // or a description payload opener) keeps the whole
                            // source, exactly as before RFC 0040.
                            if (colon != std::string_view::npos &&
                                (key == "theme" || key == "look"))
                                return std::nullopt;
                            in_config = colon != std::string_view::npos && key == "config";
                        }
                    } else if (colon != std::string_view::npos &&
                               (content.substr(0, colon) == "theme" ||
                                content.substr(0, colon) == "look")) {
                        // Admitted only at indent 2 inside the config block; a
                        // payload line or a namespaced key stays a gate.
                        if (!in_config || indent != 2u) return std::nullopt;
                        const auto value = journey_fam_scalar(content.substr(colon + 1u), work);
                        if (!value) return std::nullopt;
                        if (content.substr(0, colon) == "look") {
                            if ((fields & Look) || !matches(looks, *value)) return std::nullopt;
                            look_value = *value;
                            fields |= Look;
                        } else {
                            if ((fields & Theme) || !matches(themes, *value)) return std::nullopt;
                            theme_value = *value;
                            fields |= Theme;
                        }
                    }
                }
                if (eol == std::string_view::npos) break;
            }
        }
    }
    // The in-body themed initializer stays a whole-source gate (unchanged).
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        const std::string_view line = body.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? body.size() : eol + 1u;
        const auto trimmed = line.find_first_not_of(" \t");
        if (trimmed != std::string_view::npos && line.substr(trimmed).starts_with("%%{") &&
            line.find("theme") != std::string_view::npos)
            return std::nullopt;
        if (eol == std::string_view::npos) break;
    }
    std::vector<std::string> notices;
    if (fields != 0u) {
        if (!journey_fam_notice_line(notices, header, work)) return std::nullopt;
        if ((fields & Look) &&
            !journey_fam_notice_line(notices,
                                     "config.look=" + look_value +
                                         " -> source node style; not applied",
                                     work))
            return std::nullopt;
        if ((fields & Theme) &&
            !journey_fam_notice_line(notices,
                                     "config.theme=" + theme_value +
                                         " -> colour palette; not applied",
                                     work))
            return std::nullopt;
    }
    return notices;
}

}  // namespace

std::optional<std::vector<std::string>> gantt_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!gantt_term(fee, source.size(), 8) || !work.spend(fee)) return {};

    // Break-tag sanitizing runs first so its internal whitespace/tab spellings
    // do not trip the structural tab scan; every other tab and any bare CR is
    // rejected.
    const std::string sanitized = sanitize_breaks(std::string{source});
    for (std::size_t i = 0; i < sanitized.size(); ++i) {
        if (sanitized[i] == '\t' ||
            (sanitized[i] == '\r' &&
             (i + 1 == sanitized.size() || sanitized[i + 1] != '\n')))
            return {};
    }

    const std::string_view body = gantt_body(sanitized);
    const auto visual = journey_fam_visual_config(
        sanitized, kFamilyNoticeHeader, kFamilyLookTable, kFamilyThemeTable, work);
    if (!visual) return {};
    const std::vector<std::string>& notices = *visual;

    std::vector<GanttSection> sections;
    sections.push_back({"", {}});
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
            if (line != "gantt") return {};
            saw_header = true;
            continue;
        }

        // Non-terminal directives describe SVG axis/schedule behavior the
        // terminal does not reproduce; their values are bounded text only.
        static const std::array<std::string_view, 8> kDirectives{
            "dateFormat", "axisFormat", "tickInterval", "excludes", "includes",
            "todayMarker", "weekend", "weekday"};
        bool handled_directive = false;
        for (const auto directive : kDirectives) {
            if (line.size() > directive.size() && line.starts_with(directive) &&
                (line[directive.size()] == ' ' || line[directive.size()] == '\t')) {
                const auto value = trim_copy(line.substr(directive.size()));
                if (value.size() > 256 || !unicode_text(value)) return {};
                handled_directive = true;
                break;
            }
        }
        if (handled_directive) continue;
        if (line.starts_with("title ")) {
            if (!title.empty()) return {};
            title = trim_copy(line.substr(6));
            if (title.empty() || title.size() > 256 || !unicode_text(title)) return {};
            continue;
        }
        if (line.starts_with("section ")) {
            // The vector already holds the implicit pre-section block, so this
            // admits at most 31 named sections under the 32-entry cap; the
            // corpus maximum is small and overflow fails closed (review N3).
            if (sections.size() >= kGanttMaxSections) return {};
            std::string name = trim_copy(line.substr(8));
            if (name.empty() || name.size() > 256 || !unicode_text(name)) return {};
            sections.push_back({std::move(name), {}});
            continue;
        }

        // Task line: label : comma-separated fields.
        const auto first_colon = line.find(':');
        if (first_colon == std::string_view::npos) return {};  // unknown statement
        GanttTask task;
        task.label = trim_copy(line.substr(0, first_colon));
        if (task.label.empty() || task.label.size() > 256 || !unicode_text(task.label))
            return {};

        std::vector<std::string> fields;
        std::string_view tail = line.substr(first_colon + 1);
        while (true) {
            const auto comma = tail.find(',');
            fields.emplace_back(trim_copy(tail.substr(0, comma)));
            if (fields.size() > kGanttMaxFields) return {};
            if (comma == std::string_view::npos) break;
            tail = tail.substr(comma + 1);
        }

        std::size_t index = 0;
        const auto next = [&]() -> std::string_view {
            return index < fields.size() ? std::string_view{fields[index]} : std::string_view{};
        };

        // 1. Leading closed-vocabulary status tags.
        bool is_milestone = false;
        while (index < fields.size() && is_tag(fields[index])) {
            if (iequals(fields[index], "milestone")) is_milestone = true;
            task.tags.push_back(fields[index]);
            ++index;
        }
        // 2. Leading ids: a bare identifier or the :idref spelling. At most two
        //    lead in the corpus (vertical-marker form); the cap is eight,
        //    checked before insertion to match the dependency-id limit.
        while (index < fields.size()) {
            std::string_view field = fields[index];
            if (!field.empty() && field.front() == ':') field.remove_prefix(1);
            if (!is_identifier(field)) break;
            if (task.ids.size() >= kGanttMaxIds) return {};
            task.ids.emplace_back(field);
            ++index;
        }

        const std::size_t schedule_start = index;
        const std::size_t schedule_count = fields.size() - schedule_start;
        if (schedule_count == 0) return {};

        // 3. Scheduling tail, one of the measured corpus shapes.
        const std::string_view first = next();
        if (starts_keyword(first, "after")) {
            std::vector<std::string> deps;
            if (!dependency_field(first, "after", deps)) return {};
            task.start = "after " + [&] {
                std::string joined;
                for (std::size_t i = 0; i < deps.size(); ++i) {
                    if (i) joined += ' ';
                    joined += deps[i];
                }
                return joined;
            }();
            ++index;
            if (index == fields.size()) {
                // after-only tasks carry an implicit duration upstream; keep
                // the dependency as the whole scheduling description.
            } else if (starts_keyword(fields[index], "until")) {
                std::vector<std::string> until_ids;
                if (!dependency_field(fields[index], "until", until_ids)) return {};
                std::string joined = "until";
                for (const auto& id : until_ids) {
                    joined += ' ';
                    joined += id;
                }
                task.span = std::move(joined);
                ++index;
            } else if (is_duration(fields[index])) {
                task.span = fields[index];
                ++index;
            } else {
                return {};
            }
        } else if (starts_keyword(first, "until")) {
            std::vector<std::string> until_ids;
            if (!dependency_field(first, "until", until_ids)) return {};
            task.span = "until";
            for (const auto& id : until_ids) {
                task.span += ' ';
                task.span += id;
            }
            ++index;
        } else if (schedule_count == 1) {
            // A lone field must be a duration (implicit chained task).
            if (!is_duration(first)) return {};
            task.span = std::string{first};
            ++index;
        } else {
            // Point start plus a span: another point (explicit end date) or a
            // duration; a milestone may carry an empty trailing field or zero.
            if (!is_point(first)) return {};
            task.start = std::string{first};
            ++index;
            const std::string_view second = next();
            if (second.empty()) {
                // A point followed by an empty trailing field is the zero-span
                // vertical-marker form (corpus: "Milestone : vert, 01,"),
                // which carries no milestone tag.
                task.span = is_milestone ? "milestone" : "marker";
                ++index;
            } else if (is_duration(second) || is_point(second)) {
                // A milestone is zero-span by definition, so its literal "0"
                // endpoint transcribes as the milestone marker rather than an
                // explicit end date (review M1).
                task.span = is_milestone ? "milestone"
                        : is_point(second) ? "end " + std::string{second}
                                           : std::string{second};
                ++index;
            } else {
                return {};
            }
        }
        if (index != fields.size()) return {};  // unconsumed fields = unknown shape
        if (is_milestone && task.span.empty()) task.span = "milestone";

        for (const auto& emitted : task.tags)
            if (!unicode_text(emitted)) return {};
        for (const auto& emitted : task.ids)
            if (emitted.size() > 128 || !unicode_text(emitted)) return {};
        if (task.start.size() > 256 || task.span.size() > 256 ||
            !unicode_text(task.start) || !unicode_text(task.span))
            return {};
        if (sections.back().tasks.size() >= kGanttMaxTasksPerSection) return {};
        sections.back().tasks.push_back(std::move(task));
    }

    if (!saw_header) return {};

    std::vector<std::string> out;
    auto emit = [&](std::string row) -> bool {
        if (out.size() >= kGanttMaxRows || ftxui::string_width(row) > kWidthLimit ||
            !work.spend(16 + row.size()))
            return false;
        out.push_back(std::move(row));
        return true;
    };

    if (!title.empty() && !emit(title)) return {};
    for (const auto& section : sections) {
        if (section.tasks.empty()) continue;
        if (!emit("")) return {};
        if (!section.name.empty() && !emit("Section: " + section.name)) return {};
        for (const auto& task : section.tasks) {
            std::vector<std::string> wrapped;
            if (!gantt_wrap(task.label, work, wrapped)) return {};
            for (std::size_t i = 0; i < wrapped.size(); ++i) {
                const std::string prefix = i == 0 ? "- " : "  ";
                if (!emit(prefix + wrapped[i])) return {};
            }
            if (!task.tags.empty()) {
                std::string row = "  tags:";
                for (const auto& tag : task.tags) {
                    row += ' ';
                    row += tag;
                }
                if (!emit(std::move(row))) return {};
            }
            if (!task.ids.empty()) {
                std::string row = "  id:";
                for (std::size_t i = 0; i < task.ids.size(); ++i) {
                    if (i) row += ',';
                    row += ' ';
                    row += task.ids[i];
                }
                if (!emit(std::move(row))) return {};
            }
            if (!task.start.empty() && !emit("  start: " + task.start)) return {};
            if (!task.span.empty() && !emit("  span: " + task.span)) return {};
        }
    }
    if (out.empty()) return {};
    // RFC 0040: the bounded disclosure footer is part of the returned art
    // (blank separator, then the header and one line per accepted leaf), with
    // the same cap, charge and height guard as the state/flow/git paths.
    if (!notices.empty()) {
        std::size_t bytes = 0u;
        for (const auto& line : notices) {
            if (line.size() > kJourneyFamNoticeLineLimit) return {};
            bytes += line.size();
        }
        if (out.size() + 1u + notices.size() > static_cast<std::size_t>(kHeightLimit) ||
            !work.spend((1u + notices.size()) * (4u * sizeof(std::string) + 256u) + bytes * 2u +
                        256u))
            return {};
        out.emplace_back();
        for (const auto& line : notices) out.push_back(line);
    }
    return out;
}

}  // namespace loom::ui::mermaid::detail
