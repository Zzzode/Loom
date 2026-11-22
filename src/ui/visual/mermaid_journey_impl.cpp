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


// RFC 0040: the shared "no color contract" gate becomes a validating one. A
// `theme`/`look` key at indent 2 inside the single `config:` block is validated
// against this family's closed corpus table and disclosed through a footer
// below the art; a visual key anywhere else (top level, inside a
// `description:` payload, or at indent 4 or deeper) keeps the whole source, as
// does an in-body %%{...theme...}%% initializer. Every other frontmatter line
// is ignored (the three renderers' long-standing pass-through contract).
constexpr std::size_t kJourneyFamNoticeLineLimit = 96u;
constexpr std::string_view kFamilyNoticeHeader = "Journey native projection v1";
constexpr std::array<std::string_view, 0> kFamilyLookTable{};
constexpr std::array<std::string_view, 4> kFamilyThemeTable{"redux", "redux-dark", "redux-color", "redux-dark-color"};
constexpr bool kFamilyInitDirectivesEnabled = true;
constexpr std::array<std::string_view, 4> kFamilyInitThemeTable{"dark", "default", "forest", "neutral"};

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

// RFC 0047: the leading body-directive block is a closed grammar. Only the
// bare `init` no-op, an empty `{}`, and a flat object over
// `logLevel`/`theme` are admitted. The split helpers honour single/double
// quotes; a control byte, an unterminated quote or a backslash inside a
// single-quoted string keeps the whole source (the same structural rule as
// normalize_directive_quotes).
constexpr std::array<std::string_view, 12> kJourneyFamLogLevels{
    "0", "1", "2", "3", "4", "5", "trace", "debug", "info", "warn", "error", "fatal"};

std::string_view journey_fam_rtrim(std::string_view value) {
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

std::string_view journey_fam_ltrim(std::string_view value) {
    while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    return value;
}

bool journey_fam_directive_quotes(std::string_view text) {
    for (std::size_t at = 0; at < text.size();) {
        const char quote = text[at];
        if (quote != '\'' && quote != '"') {
            if (static_cast<unsigned char>(quote) < 0x20) return false;
            ++at;
            continue;
        }
        ++at;
        bool escape = false;
        for (; at < text.size(); ++at) {
            const char c = text[at];
            if (quote == '\'' && c == '\\') return false;
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == quote) break;
            if (static_cast<unsigned char>(c) < 0x20) return false;
        }
        if (at == text.size()) return false;
        ++at;
    }
    return true;
}

std::vector<std::string_view> journey_fam_split_commas(std::string_view text) {
    std::vector<std::string_view> parts;
    char quote = 0;
    std::size_t start = 0;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char c = text[at];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == ',') {
            parts.push_back(text.substr(start, at - start));
            start = at + 1;
        }
    }
    parts.push_back(text.substr(start));
    return parts;
}

std::size_t journey_fam_colon(std::string_view text) {
    char quote = 0;
    bool escape = false;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const char c = text[at];
        if (quote) {
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == ':') return at;
    }
    return std::string_view::npos;
}

// True when `line` is an admitted leading directive; the no-op forms fill
// nothing. `log_level`/`theme` are shared across the whole block, so a key
// repeated in a second directive is a duplicate and gates.
bool journey_fam_init_directive(std::string_view line,
                                std::span<const std::string_view> themes,
                                std::optional<std::string>& log_level,
                                std::optional<std::string>& theme, Work& work) {
    line = journey_fam_rtrim(line);
    if (!line.starts_with("%%{") || !line.ends_with("}%%") || line.size() < 7u) return false;
    const std::string_view inner = line.substr(3u, line.size() - 6u);
    if (!work.spend(inner.size() * 4u + 128u)) return false;
    const std::string_view directive = journey_fam_rtrim(journey_fam_ltrim(inner));
    if (directive == "init") return true;
    if (!directive.starts_with("init:")) return false;
    const std::string_view payload =
        journey_fam_rtrim(journey_fam_ltrim(directive.substr(5u)));
    if (payload.size() < 2u || payload.front() != '{' || payload.back() != '}') return false;
    const std::string_view interior = payload.substr(1u, payload.size() - 2u);
    if (interior.find_first_of("{}[]") != std::string_view::npos) return false;
    if (!journey_fam_directive_quotes(interior)) return false;
    if (journey_fam_rtrim(journey_fam_ltrim(interior)).empty()) return true;
    for (const std::string_view entry : journey_fam_split_commas(interior)) {
        const auto colon = journey_fam_colon(entry);
        if (colon == std::string_view::npos) return false;
        const auto key = journey_fam_scalar(entry.substr(0u, colon), work);
        const auto value = journey_fam_scalar(entry.substr(colon + 1u), work);
        if (!key || !value) return false;
        if (*key == "logLevel") {
            if (log_level ||
                std::ranges::find(kJourneyFamLogLevels, *value) == kJourneyFamLogLevels.end())
                return false;
            log_level = *value;
        } else if (*key == "theme") {
            if (theme || std::ranges::find(themes, *value) == themes.end()) return false;
            theme = *value;
        } else {
            return false;
        }
    }
    return true;
}

// nullopt: the whole source stays whole-source. Otherwise the bounded
// disclosure footer (possibly empty), emitted below the art by the caller.
std::optional<std::vector<std::string>> journey_fam_visual_config(
    std::string_view source, std::string_view header,
    std::span<const std::string_view> looks, std::span<const std::string_view> themes,
    bool init_enabled, std::span<const std::string_view> init_themes, Work& work) {
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
    // RFC 0047: the leading directive block is admitted for the enabled
    // families; a frontmatter visual leaf and a keyed directive are mutually
    // exclusive. The legacy "any body %%{ line that mentions theme keeps the
    // whole source" rule still covers everything from the diagram header on.
    std::optional<std::string> init_log_level, init_theme;
    std::size_t tail_start = 0u;
    if (init_enabled) {
        if (!work.spend(body.size() * 2u + 256u)) return std::nullopt;
        std::size_t pos = 0u;
        bool saw_directive = false;
        while (pos <= body.size()) {
            const auto eol = body.find('\n', pos);
            std::string_view line = body.substr(
                pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
            const std::size_t next =
                eol == std::string_view::npos ? body.size() + 1u : eol + 1u;
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1u);
            const std::string_view trimmed = journey_fam_ltrim(line);
            if (trimmed.empty() || (trimmed.starts_with("%%") && !trimmed.starts_with("%%{"))) {
                pos = next;
                continue;
            }
            if (!trimmed.starts_with("%%{")) {
                tail_start = pos;
                break;
            }
            if (!journey_fam_init_directive(trimmed, init_themes, init_log_level, init_theme,
                                            work))
                return std::nullopt;
            saw_directive = true;
            pos = next;
            if (eol == std::string_view::npos) {
                tail_start = body.size();
                break;
            }
        }
        // A directives-only body has no header to start the tail scan; the
        // admitted block must not be re-read by the legacy rule.
        if (saw_directive && tail_start == 0u) tail_start = body.size();
        if (fields != 0u && (init_log_level || init_theme)) return std::nullopt;
    }
    const std::string_view tail = body.substr(tail_start);
    for (std::size_t pos = 0;;) {
        const auto eol = tail.find('\n', pos);
        const std::string_view line = tail.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
        pos = eol == std::string_view::npos ? tail.size() + 1u : eol + 1u;
        const auto trimmed = line.find_first_not_of(" \t");
        if (trimmed != std::string_view::npos && line.substr(trimmed).starts_with("%%{") &&
            line.find("theme") != std::string_view::npos)
            return std::nullopt;
        if (eol == std::string_view::npos) break;
    }
    std::vector<std::string> notices;
    if (fields != 0u || init_log_level || init_theme) {
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
        if (init_log_level &&
            !journey_fam_notice_line(
                notices, "config.logLevel=" + *init_log_level +
                             " -> logger only; not applied",
                work))
            return std::nullopt;
        if (init_theme &&
            !journey_fam_notice_line(notices,
                                     "config.theme=" + *init_theme +
                                         " -> colour palette; not applied",
                                     work))
            return std::nullopt;
    }
    return notices;
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
    const auto visual = journey_fam_visual_config(
        source, kFamilyNoticeHeader, kFamilyLookTable, kFamilyThemeTable, kFamilyInitDirectivesEnabled,
        kFamilyInitThemeTable, work);
    if (!visual) return {};
    const std::vector<std::string>& notices = *visual;
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
