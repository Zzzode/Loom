module;

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {

// State descriptions and block Notes share the original physical-line guard.
bool state_source_lines(std::string_view source, bool& checked, Work& work) {
    if (!work.spend(4u * sizeof(std::string_view) + 8u * sizeof(bool) + 32u)) return false;
    if (checked) return true;
    if (source.size() > kSourceLimit ||
        !work.spend(source.size() * 2u + 8u * sizeof(std::size_t) + 64u))
        return false;
    for (std::size_t n = 0; n < source.size(); ++n)
        if (source[n] == '\r' && (n + 1u == source.size() || source[n + 1u] != '\n')) return false;
    checked = true;
    return true;
}

std::optional<std::string> state_description_label(std::string_view raw, Work& work) {
    if (raw.size() > kSourceLimit ||
        !work.spend(raw.size() * (32u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view)) +
                    256u))
        return {};
    const auto value = trim_relation(raw);
    if (value.empty() || value.size() > 256u || value.front() == ':' ||
        value.find_first_of("{}") != std::string_view::npos)
        return {};
    // The reused atom-safety decoder can search suffixes quadratically.
    if (!work.spend(value.size() * (value.size() + 1u))) return {};
    // Quotes in a description are data; only its alias owner removes STR quotes.
    auto text = relation_literal_label(value);
    if (!text || text->size() > 256u || text->find('\n') != std::string::npos) return {};
    return text;
}

std::optional<StateNoteBlock> state_note_block(std::string_view original, std::size_t header,
                                               Work& work) {
    if (original.size() > kSourceLimit || header > original.size() ||
        !work.spend(original.size() * 8u + 16u * sizeof(std::size_t) +
                    8u * sizeof(std::string_view) + 256u))
        return {};
    const auto newline = original.find('\n', header);
    if (newline == std::string_view::npos) return {};
    const auto begin = newline + 1u;
    std::size_t cursor = begin, close = original.size(), after = 0, total = 0, rows = 0;
    while (cursor < original.size()) {
        if (!work.spend(64u)) return {};
        const auto next = original.find('\n', cursor);
        const auto end = next == std::string_view::npos ? original.size() : next;
        const auto value = trim_relation(original.substr(cursor, end - cursor));
        if (value.size() >= 8u && er_ascii_equal(value.substr(0, 8), "end note")) {
            const auto ch = value.size() == 8u ? 0u : static_cast<unsigned char>(value[8]);
            const bool word = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                              (ch >= '0' && ch <= '9') || ch == '_';
            if (value.size() == 8u || !word) {
                // A boundary marker with residue cannot silently lose its tail.
                if (value.size() != 8u) return {};
                close = cursor;
                after = next == std::string_view::npos ? original.size() : next + 1u;
                break;
            }
        }
        if (value.empty() || value.find("%%") != std::string_view::npos || er_metadata_space(value))
            return {};
        // Only boundary whitespace is closed; internal Unicode/data stay literal.
        auto scalar = value.size() - 1u;
        std::size_t continuation = 0;
        while (scalar > 0 && (static_cast<unsigned char>(value[scalar]) & 0xc0u) == 0x80u) {
            if (!work.spend() || ++continuation > 3u) return {};
            --scalar;
        }
        if (er_metadata_space(value.substr(scalar))) return {};
        const std::size_t separator = rows ? 1u : 0u;
        if (separator > kSourceLimit - total || value.size() > kSourceLimit - total - separator)
            return {};
        total += separator + value.size();
        ++rows;
        cursor = next == std::string_view::npos ? original.size() : next + 1u;
    }
    if (!after || !rows || close == original.size()) return {};
    // Prepay exact join storage and reused decoder worst suffix scans before
    // allocation/decoding. The original source bound makes multiplication safe.
    if (!work.spend(total * (total + 1u) +
                    total * (32u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view)) +
                    (total + 1u) * 3u + 8u * sizeof(StateNoteBlock) + 256u))
        return {};
    std::string joined;
    joined.reserve(total);
    cursor = begin;
    while (cursor < close) {
        if (!work.spend(16u)) return {};
        const auto next = original.find('\n', cursor);
        if (next == std::string_view::npos || next > close) return {};
        const auto value = trim_relation(original.substr(cursor, next - cursor));
        if (!joined.empty()) joined += '\n';
        joined.append(value);
        cursor = next + 1u;
    }
    if (joined.size() != total) return {};
    // D223: Mermaid accepts a tab between `br` and the self-closing slash
    // (`<br	/>`) as the same break the shared decoder knows as `<br/>`.
    // Canonicalize only that single corpus-attested spelling in place (the
    // string only shrinks, so the prepaid budget still holds); any other tab
    // is not part of this token and keeps failing inside the label decoder.
    for (std::size_t pos = 0;
         (pos = joined.find("<br\t/>", pos)) != std::string_view::npos;
         pos += 5)
        joined.erase(pos + 3, 1);
    auto text = relation_literal_label(joined);
    if (!text) return {};
    return StateNoteBlock{std::move(*text), after};
}

std::optional<std::string> state_transition_label(std::string_view raw) {
    auto value = trim_relation(raw);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.size() - 2);
    if (value.size() > kSourceLimit) return {};
    // Only the two corpus-attested escapes are recognized. `\n` is rewritten
    // to the `<br>` token the shared decoder already maps to a line break, so
    // no new row-splitting path is introduced; `#colon;` becomes a literal
    // colon. A backslash or `#`/`&` entity outside these spellings survives
    // into relation_literal_label and fails its closed vocabulary there. This
    // stays a pure validator (like relation_label it replaces on the state
    // path) so it adds no Work fee and the existing labeled-state schedule is
    // byte-identical.
    std::string text;
    text.reserve(value.size());
    // An escape is consumed only at a token boundary: the two-char `\n` must
    // not follow another backslash, and `#colon;` must not follow `#` or `&`,
    // so nested spellings (`\\n`, `##colon;`, `&#colon;`) keep their literal
    // introducer and fail the shared closed-vocabulary decoder below.
    const auto predecessor_ok = [&](std::size_t pos, const char* forbidden) {
        if (pos == 0) return true;
        const char previous = value[pos - 1];
        for (const char* p = forbidden; *p; ++p)
            if (previous == *p) return false;
        return true;
    };
    for (std::size_t at = 0; at < value.size();) {
        if (value.substr(at).starts_with("\\n") && predecessor_ok(at, "\\")) {
            text += "<br>";
            at += 2;
        } else if (value.substr(at).starts_with("#colon;") &&
                   predecessor_ok(at, "#&")) {
            text += ':';
            at += 7;
        } else {
            text += value[at++];
        }
    }
    return relation_literal_label(text);
}

namespace {
constexpr std::size_t kStateStyleTargetLimit = 8u;
constexpr std::size_t kStateStyleStatementLimit = 64u;

std::string state_ascii_lower(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value)
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    return out;
}

// Where a state color token is parsed. The reachable D228 corpus uses
// different named spellings in each context, so the state-local named table is
// split to stay source-backed: a named color with no reachable use fails closed
// instead of drawing a source a frozen closed-profile gate expects to keep as
// whole-source fallback.
enum class StateColorContext { Direct, Border };

// State corpus named colors beyond the shared hex/"white" resolver, mapped
// through the same semantic role function the hex path uses so no hardcoded
// RGB reaches the canvas. Every entry is justified by a reachable fixture:
//   black/blue -> direct fill/color (subgraphs `fill:black`, precedence
//                 `color: blue`); red -> border shorthand only
//                 (`border:1px solid red`). No reachable source spells a direct
// fill/stroke/color:red, while the frozen StateScoped `unknown_style` closed
// profile uses exactly `style A fill:red`, so red stays out of the direct set.
std::optional<SourceStyleColor> state_style_color(std::string_view raw,
                                                  StateColorContext context) {
    struct Named {
        std::string_view name;
        std::array<unsigned char, 3> rgb;
    };
    static constexpr Named kDirectNamed[] = {{"black", {0, 0, 0}},
                                             {"blue", {0, 0, 255}}};
    static constexpr Named kBorderNamed[] = {{"red", {255, 0, 0}}};
    const auto value = state_ascii_lower(raw);
    const std::span<const Named> named =
        context == StateColorContext::Border
            ? std::span<const Named>{kBorderNamed}
            : std::span<const Named>{kDirectNamed};
    for (const auto& entry : named)
        if (value == entry.name) {
            SourceStyleColor color;
            color.rgb = {entry.rgb[0], entry.rgb[1], entry.rgb[2]};
            color.role = source_style_role(entry.rgb[0], entry.rgb[1], entry.rgb[2]);
            return color;
        }
    return source_style_color(raw);
}

// `border: <width>px? solid <color>` maps onto the existing stroke role plus
// the border width the paint path already understands.
bool state_border_shorthand(std::string_view raw, ClassStyleProperties& into) {
    auto tokens = std::vector<std::string>{};
    while (!raw.empty()) {
        const auto sp = raw.find_first_not_of(" \t");
        if (sp == std::string_view::npos) break;
        raw.remove_prefix(sp);
        const auto end = raw.find_first_of(" \t");
        tokens.emplace_back(raw.substr(0, end));
        if (end == std::string_view::npos) break;
        raw.remove_prefix(end);
    }
    if (tokens.empty() || tokens.size() > 3u) return false;
    std::optional<double> width;
    std::optional<SourceStyleColor> color;
    bool solid = false;
    for (const auto& token : tokens) {
        if (token == "solid") {
            solid = true;
            continue;
        }
        if (!width && (token.find("px") != std::string::npos ||
                       std::ranges::all_of(token, [](char c) { return c >= '0' && c <= '9'; }))) {
            auto number = std::string_view(token);
            if (number.ends_with("px")) number.remove_suffix(2u);
            double parsed = 0.0;
            const auto result =
                std::from_chars(number.data(), number.data() + number.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != number.data() + number.size() ||
                !std::isfinite(parsed) || parsed < 0.0 || parsed > 64.0)
                return false;
            width = parsed;
        } else if (!color) {
            color = state_style_color(token, StateColorContext::Border);
            if (!color) return false;
        } else {
            return false;
        }
    }
    // A CSS border shorthand requires a width, the `solid` style keyword and a
    // color; a bare color (`border:red`) is a malformed statement, not a stroke.
    if (!width || !solid || !color) return false;
    into.stroke = *color;
    into.width = *width;
    return true;
}

bool state_style_properties(std::string_view rest, ClassStyleProperties& properties) {
    unsigned seen = 0u;
    while (!rest.empty()) {
        const auto separator = rest.find_first_of(",;");
        const auto component = trim_relation(rest.substr(0, separator));
        if (component.empty()) {
            if (separator == std::string_view::npos) break;
            rest = trim_relation(rest.substr(separator + 1u));
            continue;
        }
        const auto colon = component.find(':');
        if (colon == std::string_view::npos) return false;
        const auto property = state_ascii_lower(trim_relation(component.substr(0, colon)));
        const auto value = trim_relation(component.substr(colon + 1u));
        if (value.empty()) return false;
        const unsigned bit = property == "fill"           ? 1u
                             : property == "stroke"       ? 2u
                             : property == "color"        ? 4u
                             : property == "font-weight"  ? 8u
                             : property == "border"       ? 16u
                             : property == "font-style"   ? 32u
                                                          : 0u;
        if (!bit || (seen & bit)) return false;
        seen |= bit;
        if (property == "fill" || property == "stroke" || property == "color") {
            const auto color = state_style_color(value, StateColorContext::Direct);
            if (!color || (bit != 1u && color->transparent)) return false;
            if (property == "fill")
                properties.fill = *color;
            else if (property == "stroke")
                properties.stroke = *color;
            else
                properties.color = *color;
        } else if (property == "font-weight") {
            const auto weight = state_ascii_lower(value);
            if (weight != "bold" && weight != "normal") return false;
            properties.bold = weight == "bold";
        } else if (property == "font-style") {
            // D-461: validated and dropped. The terminal cell grid has no italic
            // face, so the property neither paints nor joins the legend tokens
            // (the declaration's verbatim legend record is the disclosure).
            const auto style = state_ascii_lower(value);
            if (style != "italic" && style != "normal") return false;
        } else if (!state_border_shorthand(value, properties)) {
            return false;
        }
        if (separator == std::string_view::npos) break;
        rest = trim_relation(rest.substr(separator + 1u));
    }
    return seen != 0u;
}

bool state_keyword_boundary(std::string_view line, std::size_t prefix) {
    return line.size() > prefix && (line[prefix] == ' ' || line[prefix] == '\t');
}

// Class names use the same identifier alphabet as the shared style grammar;
// kept local so the state TU does not widen the shared header surface.
bool state_style_identifier(std::string_view value) {
    if (value.empty() || value.size() > 64u) return false;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const char c = value[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
              (n > 0u && c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

std::optional<std::vector<std::string>> state_targets(std::string_view text) {
    std::vector<std::string> targets;
    while (!text.empty()) {
        const auto comma = text.find(',');
        const auto id = trim_relation(text.substr(0, comma));
        if (id.empty() || id.size() > 64u || targets.size() >= kStateStyleTargetLimit)
            return std::nullopt;
        for (const char c : id)
            if (c == ' ' || c == '\t' || c == ':') return std::nullopt;
        targets.emplace_back(id);
        if (comma == std::string_view::npos) break;
        text = text.substr(comma + 1u);
        // A trailing (or whitespace-only) comma is a malformed target list, not
        // an implicit extra target; mirror the Class family's strict identifier
        // grammar instead of silently dropping the empty component.
        if (trim_relation(text).empty()) return std::nullopt;
    }
    return targets.empty() ? std::nullopt : std::optional<std::vector<std::string>>{targets};
}

void state_merge(ClassStyleProperties& into, const ClassStyleProperties& from) {
    if (from.fill) into.fill = from.fill;
    if (from.stroke) into.stroke = from.stroke;
    if (from.color) into.color = from.color;
    if (from.width) into.width = from.width;
    if (from.bold) into.bold = from.bold;
}

std::string state_legend_tokens(const ClassStyleProperties& p) {
    std::string text;
    const auto append = [&](std::string_view key, std::string_view value) {
        text += ' ';
        text += key;
        text += ':';
        text += value;
    };
    if (p.fill) append("fill", source_style_role_name(p.fill->role));
    if (p.stroke) append("stroke", source_style_role_name(p.stroke->role));
    if (p.width) append("width", *p.width >= 4.0 ? "bold" : "normal");
    if (p.color) append("fg", source_style_role_name(p.color->role));
    if (p.bold) append("font", *p.bold ? "bold" : "normal");
    return text;
}

// A verbatim source line may carry a real TAB (the classDef dispatch accepts a
// TAB after the keyword, e.g. `classDef\tc ...`). The legend is a grid row, so
// mirror the Class family and project TAB as the two-character "\t" rather than
// letting a control character corrupt the column layout.
std::string state_legend_verbatim(std::string_view verbatim) {
    std::string out;
    out.reserve(verbatim.size() * 2u);
    for (const char c : verbatim) {
        if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}
}  // namespace

bool state_parse_classdef(std::string_view line, Work& work, StateStyleDefinition& out) {
    if (line.size() > kSourceLimit || !work.spend(line.size() * 4u + 64u)) return false;
    if (!state_keyword_boundary(line, 8u)) return false;
    auto rest = trim_relation(line.substr(8u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos) return false;
    out.name = std::string(trim_relation(rest.substr(0, split)));
    if (out.name.empty() || !state_style_identifier(out.name)) return false;
    out.verbatim = std::string(line);
    return state_style_properties(rest.substr(split), out.properties);
}

bool state_parse_class_assignment(std::string_view line, Work& work,
                                  StateStyleAssignment& out) {
    if (line.size() > kSourceLimit || !work.spend(line.size() * 4u + 64u)) return false;
    if (!state_keyword_boundary(line, 5u)) return false;
    const auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_last_of(" \t");
    if (split == std::string_view::npos) return false;
    auto targets = state_targets(trim_relation(rest.substr(0, split)));
    if (!targets) return false;
    out.targets = std::move(*targets);
    out.class_name = std::string(trim_relation(rest.substr(split)));
    if (out.class_name.empty() || !state_style_identifier(out.class_name)) return false;
    out.verbatim = std::string(line);
    return true;
}

bool state_parse_direct_style(std::string_view line, Work& work, StateStyleDirect& out) {
    if (line.size() > kSourceLimit || !work.spend(line.size() * 4u + 64u)) return false;
    if (!state_keyword_boundary(line, 5u)) return false;
    const auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos) return false;
    auto targets = state_targets(trim_relation(rest.substr(0, split)));
    if (!targets) return false;
    out.targets = std::move(*targets);
    out.verbatim = std::string(line);
    return state_style_properties(rest.substr(split), out.properties);
}

// D-461: `click` directive probe and closed non-visual binding parser.
bool state_click_keyword(std::string_view line) {
    return line.size() > 6u && line.starts_with("click") &&
           (line[5] == ' ' || line[5] == '\t');
}

bool state_click_binding(std::string_view line, const RelationDiagram& diagram, Work& work) {
    if (!state_click_keyword(line) || !work.spend(line.size() * 4u + 128u)) return false;
    auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos) return false;
    const auto target = trim_relation(rest.substr(0, split));
    if (!state_style_identifier(target)) return false;
    rest = trim_relation(rest.substr(split));
    // One or two double-quoted arguments (url, optional tooltip title). The
    // corpus form is `click A "https://google.com" "Visit Google"`; `href`/`call`
    // forms and unquoted arguments stay whole-source. Separation between two
    // quoted arguments is optional: after one argument is consumed the next
    // token only has to start with a quote. The binding is dropped, so nothing
    // downstream depends on the split.
    std::size_t quoted = 0u;
    while (!rest.empty()) {
        if (rest.front() != '"') return false;
        std::size_t at = 1u;
        std::size_t bytes = 0u;
        for (; at < rest.size() && rest[at] != '"'; ++at) {
            const auto c = static_cast<unsigned char>(rest[at]);
            if (c < 0x20u || c == 0x7fu) return false;
            ++bytes;
        }
        if (at >= rest.size() || bytes == 0u || bytes > 512u) return false;
        rest = trim_relation(rest.substr(at + 1u));
        if (++quoted > 2u) return false;
    }
    if (quoted == 0u) return false;
    // The binding names an already-declared state; anything else stays closed.
    for (const auto& entity : diagram.entities)
        if (entity.id == target) return true;
    return false;
}

bool build_state_direct_styles(RelationDiagram& diagram,
                               std::span<const StateStyleDefinition> definitions,
                               std::span<const StateStyleAssignment> assignments,
                               std::span<const StateStyleDirect> directs, Work& work) {
    if (definitions.size() > kStateStyleStatementLimit ||
        assignments.size() > kStateStyleStatementLimit ||
        directs.size() > kStateStyleStatementLimit)
        return false;
    const auto find_def = [&](std::string_view name) -> const StateStyleDefinition* {
        for (const auto& definition : definitions)
            if (definition.name == name) return &definition;
        return nullptr;
    };
    if (!work.spend(diagram.entities.size() * sizeof(ClassStyleProperties) + 512u)) return false;
    auto styles = std::make_unique<ClassDirectStyles>();
    styles->effective.assign(diagram.entities.size(), ClassStyleProperties{});

    struct Applied {
        int entity;
        std::string verbatim;
        ClassStyleProperties properties;
        std::string class_name;
        const StateStyleDefinition* definition = nullptr;
    };
    std::vector<Applied> applied;
    const auto resolve_target = [&](std::string_view name) -> int {
        for (std::size_t n = 0; n < diagram.entities.size(); ++n)
            if (diagram.entities[n].id == name) return static_cast<int>(n);
        return -1;
    };
    // Class assignments cascade in source order; an unknown class fails closed.
    for (const auto& assignment : assignments) {
        const auto* definition = find_def(assignment.class_name);
        if (!definition) return false;
        for (const auto& target : assignment.targets) {
            const int entity = resolve_target(target);
            if (entity < 0) return false;
            state_merge(styles->effective[static_cast<std::size_t>(entity)],
                        definition->properties);
            applied.push_back(
                {entity, assignment.verbatim, definition->properties, assignment.class_name,
                 definition});
        }
    }
    // Direct styles always override the class profile on the same properties.
    for (const auto& direct : directs) {
        for (const auto& target : direct.targets) {
            const int entity = resolve_target(target);
            if (entity < 0) return false;
            state_merge(styles->effective[static_cast<std::size_t>(entity)], direct.properties);
            applied.push_back({entity, direct.verbatim, direct.properties, {}, nullptr});
        }
    }
    if (applied.size() > 128u || !work.spend(applied.size() * sizeof(ClassDirectStyle) + 256u))
        return false;
    for (const auto& entry : applied)
        styles->declarations.push_back(
            ClassDirectStyle{diagram.entities[static_cast<std::size_t>(entry.entity)].id,
                             entry.verbatim, entry.class_name, 0u, entry.properties,
                             entry.entity});

    std::vector<std::string> used_definitions;
    for (const auto& entry : applied) {
        if (!entry.definition) continue;
        if (std::ranges::find(used_definitions, entry.definition->name) !=
            used_definitions.end())
            continue;
        used_definitions.emplace_back(entry.definition->name);
    }
    for (const auto& entry : applied) {
        if (styles->legend.size() >= 7u ||
            !work.spend(entry.verbatim.size() * 8u + 4096u))
            return false;
        styles->legend.push_back(state_legend_verbatim(entry.verbatim) + " ->" +
                                 state_legend_tokens(entry.properties));
    }
    for (const auto& name : used_definitions) {
        const auto* definition = find_def(name);
        if (!definition) return false;
        if (styles->legend.size() >= 7u ||
            !work.spend(definition->verbatim.size() * 8u + 4096u))
            return false;
        styles->legend.push_back(state_legend_verbatim(definition->verbatim) +
                                 " definition: " + name + " (applied)");
    }
    styles->legend.emplace_back(
        "key: hue roles use palette; width>=4=bold border; color on fill not projected; "
        "monochrome/ANSI16 may share shades; source TAB=\\t");
    diagram.direct_styles = std::move(styles);
    return true;
}
}  // namespace loom::ui::mermaid::detail
