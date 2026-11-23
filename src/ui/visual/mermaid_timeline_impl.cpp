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


// RFC 0040: the shared "no color contract" gate becomes a validating one. A
// `theme`/`look` key at indent 2 inside the single `config:` block is validated
// against this family's closed corpus table and disclosed through a footer
// below the art; a visual key anywhere else (top level, inside a
// `description:` payload, or at indent 4 or deeper) keeps the whole source, as
// does an in-body %%{...theme...}%% initializer. Every other frontmatter line
// is ignored (the three renderers' long-standing pass-through contract).
constexpr std::size_t kJourneyFamNoticeLineLimit = 96u;
// D-454: upper bound for a leading directive spanning several body lines.
constexpr std::size_t kJourneyFamDirectiveBufferLimit = 2048u;
constexpr std::string_view kFamilyNoticeHeader = "Timeline native projection v1";
constexpr std::array<std::string_view, 2> kFamilyLookTable{"classic", "neo"};
constexpr std::array<std::string_view, 10> kFamilyThemeTable{"dark", "default", "forest", "neo", "neo-dark", "neutral", "redux", "redux-dark", "redux-color", "redux-dark-color"};
constexpr bool kFamilyInitDirectivesEnabled = true;
constexpr std::array<std::string_view, 5> kFamilyInitThemeTable{"base", "dark", "default", "forest", "neutral"};

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

// D-454: a top-level-only split. A comma inside a nested `{...}` object or a
// quoted scalar does not split; unbalanced braces or an unterminated quote
// reject the whole directive.
std::optional<std::vector<std::string_view>> journey_fam_split_entries(std::string_view text) {
    std::vector<std::string_view> parts;
    char quote = 0;
    int depth = 0;
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
        if (c == '{') {
            ++depth;
        } else if (c == '}') {
            if (--depth < 0) return std::nullopt;
        } else if (c == ',' && depth == 0) {
            parts.push_back(text.substr(start, at - start));
            start = at + 1;
        }
    }
    if (quote != 0 || depth != 0) return std::nullopt;
    parts.push_back(text.substr(start));
    return parts;
}

// D-454: the closed nested directive objects. The timeline projection is a
// text table and never reads a colour, so a boolean/colour payload inside the
// leading `%%{init}%%` block is inert and disclosed. Only the timeline family
// enables the table; journey passes an empty span and keeps RFC 0047's flat
// grammar byte-identical.
enum class JourneyFamNestedKind { Boolean, Colour };

struct JourneyFamNestedLeaf {
    std::string_view outer;  // `timeline`
    std::string_view inner;  // `disableMulticolor`; empty = `cScale<index>`
    JourneyFamNestedKind kind;
};

constexpr std::array<JourneyFamNestedLeaf, 2> kTimelineNestedLeaves{{
    {"timeline", "disableMulticolor", JourneyFamNestedKind::Boolean},
    {"themeVariables", {}, JourneyFamNestedKind::Colour},
}};

bool journey_fam_boolean(std::string_view value) {
    return value == "true" || value == "false";
}

bool journey_fam_colour(std::string_view value) {
    if (value.size() != 4u && value.size() != 7u) return false;
    if (value.front() != '#') return false;
    for (const char c : value.substr(1u))
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    return true;
}

// True for `cScale<index>` with a canonical index in 0..11 (no leading zero).
bool journey_fam_cscale(std::string_view key) {
    constexpr std::string_view kPrefix = "cScale";
    if (!key.starts_with(kPrefix)) return false;
    const std::string_view digits = key.substr(kPrefix.size());
    if (digits.empty() || digits.size() > 2u) return false;
    if (digits.size() == 2u && digits.front() == '0') return false;
    int index = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') return false;
        index = index * 10 + (c - '0');
    }
    return index <= 11;
}

// One admitted nested object: `{ 'key': value, ... }` with balanced braces,
// no deeper nesting, and every (outer, inner) pair in the family's closed
// table. `lines` receives the disclosure line per admitted leaf in source
// order; `seen` rejects a repeated leaf across directives.
bool journey_fam_nested_object(std::string_view outer, std::string_view object,
                               std::span<const JourneyFamNestedLeaf> nested,
                               std::vector<std::string>& lines,
                               std::vector<std::string>& seen, Work& work) {
    if (object.size() < 2u || object.front() != '{' || object.back() != '}')
        return false;
    const std::string_view interior = object.substr(1u, object.size() - 2u);
    if (interior.find_first_of("{}[]") != std::string_view::npos) return false;
    if (!journey_fam_directive_quotes(interior)) return false;
    const auto entries = journey_fam_split_entries(interior);
    if (!entries || entries->empty()) return false;
    for (const std::string_view entry : *entries) {
        const auto colon = journey_fam_colon(entry);
        if (colon == std::string_view::npos) return false;
        const auto key = journey_fam_scalar(entry.substr(0u, colon), work);
        const auto value = journey_fam_scalar(entry.substr(colon + 1u), work);
        if (!key || !value) return false;
        std::string id = std::string{outer} + "." + *key;
        const JourneyFamNestedLeaf* leaf = nullptr;
        for (const auto& candidate : nested) {
            if (candidate.outer != outer) continue;
            if (candidate.inner == *key ||
                (candidate.inner.empty() && journey_fam_cscale(*key)))
                leaf = &candidate;
        }
        if (leaf == nullptr) return false;
        if (leaf->kind == JourneyFamNestedKind::Boolean ? !journey_fam_boolean(*value)
                                                        : !journey_fam_colour(*value))
            return false;
        if (!work.spend(id.size() * 4u + 2u * value->size() + 64u)) return false;
        if (std::ranges::find(seen, id) != seen.end()) return false;
        seen.push_back(id);
        lines.push_back("config." + id + "=" + *value + " -> colour palette; not applied");
    }
    return true;
}

// True when `line` is an admitted leading directive; the no-op forms fill
// nothing. `log_level`/`theme` are shared across the whole block, so a key
// repeated in a second directive is a duplicate and gates. D-454: a value may
// be a closed nested object from `nested`; its disclosure lines land in
// `nested_lines` in source order and `seen_nested` rejects a repeated leaf.
bool journey_fam_init_directive(std::string_view line,
                                std::span<const std::string_view> themes,
                                std::span<const JourneyFamNestedLeaf> nested,
                                std::optional<std::string>& log_level,
                                std::optional<std::string>& theme,
                                std::vector<std::string>& nested_lines,
                                std::vector<std::string>& seen_nested, Work& work) {
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
    if (interior.find_first_of("[]") != std::string_view::npos) return false;
    if (!journey_fam_directive_quotes(interior)) return false;
    if (journey_fam_rtrim(journey_fam_ltrim(interior)).empty()) return true;
    const auto entries = journey_fam_split_entries(interior);
    if (!entries) return false;
    for (const std::string_view entry : *entries) {
        const auto colon = journey_fam_colon(entry);
        if (colon == std::string_view::npos) return false;
        const auto key = journey_fam_scalar(entry.substr(0u, colon), work);
        if (!key) return false;
        const std::string_view raw_value =
            journey_fam_rtrim(journey_fam_ltrim(entry.substr(colon + 1u)));
        if (!raw_value.empty() && raw_value.front() == '{') {
            if (!journey_fam_nested_object(*key, raw_value, nested, nested_lines, seen_nested,
                                           work))
                return false;
            continue;
        }
        const auto value = journey_fam_scalar(raw_value, work);
        if (!value) return false;
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

struct JourneyFamVisual {
    std::vector<std::string> notices;
    // Offset into the body where the diagram starts (after the admitted
    // leading directive block; 0 when there is none).
    std::size_t body_tail_start = 0u;
};

// nullopt: the whole source stays whole-source. Otherwise the bounded
// disclosure footer (possibly empty), emitted below the art by the caller,
// plus the body offset the diagram parse must start from (D-454: a multi-line
// directive's continuation lines are not `%%`-prefixed and must not be parsed
// as diagram statements).
std::optional<JourneyFamVisual> journey_fam_visual_config(
    std::string_view source, std::string_view header,
    std::span<const std::string_view> looks, std::span<const std::string_view> themes,
    bool init_enabled, std::span<const std::string_view> init_themes,
    std::span<const JourneyFamNestedLeaf> nested, Work& work) {
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
    std::vector<std::string> nested_lines, seen_nested;
    std::size_t tail_start = 0u;
    if (init_enabled) {
        if (!work.spend(body.size() * 2u + 256u)) return std::nullopt;
        std::size_t pos = 0u;
        bool saw_directive = false;
        while (pos <= body.size()) {
            const auto eol = body.find('\n', pos);
            std::string_view line = body.substr(
                pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos);
            std::size_t next = eol == std::string_view::npos ? body.size() + 1u : eol + 1u;
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
            // D-454: a directive may span lines until its closing `}%%`; the
            // joined text is the same closed grammar with whitespace between
            // entries, bounded so a stray opener cannot swallow the body.
            std::string buffer{trimmed};
            while (!journey_fam_rtrim(buffer).ends_with("}%%")) {
                if (next > body.size()) return std::nullopt;
                const auto part_eol = body.find('\n', next);
                std::string_view part = body.substr(
                    next, part_eol == std::string_view::npos ? std::string_view::npos
                                                             : part_eol - next);
                if (!part.empty() && part.back() == '\r') part.remove_suffix(1u);
                const std::string_view piece = journey_fam_ltrim(part);
                if (buffer.size() + piece.size() + 1u > kJourneyFamDirectiveBufferLimit ||
                    !work.spend(piece.size() * 2u + 16u))
                    return std::nullopt;
                buffer.push_back(' ');
                buffer += piece;
                next = part_eol == std::string_view::npos ? body.size() + 1u : part_eol + 1u;
            }
            if (!journey_fam_init_directive(buffer, init_themes, nested, init_log_level,
                                            init_theme, nested_lines, seen_nested, work))
                return std::nullopt;
            saw_directive = true;
            pos = next;
            if (next > body.size()) {
                tail_start = body.size();
                break;
            }
        }
        // A directives-only body has no header to start the tail scan; the
        // admitted block must not be re-read by the legacy rule.
        if (saw_directive && tail_start == 0u) tail_start = body.size();
        if (fields != 0u && (init_log_level || init_theme || !nested_lines.empty()))
            return std::nullopt;
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
    if (fields != 0u || init_log_level || init_theme || !nested_lines.empty()) {
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
        for (const auto& line : nested_lines)
            if (!journey_fam_notice_line(notices, line, work)) return std::nullopt;
    }
    return JourneyFamVisual{std::move(notices), tail_start};
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

    const auto visual = journey_fam_visual_config(
        source, kFamilyNoticeHeader, kFamilyLookTable, kFamilyThemeTable, kFamilyInitDirectivesEnabled,
        kFamilyInitThemeTable, kTimelineNestedLeaves, work);
    if (!visual) return {};
    const std::vector<std::string>& notices = visual->notices;
    const std::string_view body =
        timeline_body(source).substr(visual->body_tail_start);

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
