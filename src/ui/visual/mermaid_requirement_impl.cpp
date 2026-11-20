module;

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

// Requirement is intentionally a private typed language.  These records never
// enter Class, ER, or State parsing and only project to neutral boxes after
// every declaration and endpoint has been resolved.
struct PendingRequirementEdge {
    RequirementRelation kind;
    std::string source, target;
    std::size_t statement_order = 0;
};

struct RequirementDeclaration {
    RequirementNode node;
    bool inline_empty = false;
    // D204: class name attached through the `name:::class` declaration suffix.
    std::optional<std::string> inline_class;
};

char requirement_fold(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool requirement_equal(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t at = 0; at < left.size(); ++at)
        if (requirement_fold(left[at]) != requirement_fold(right[at])) return false;
    return true;
}

bool requirement_word_boundary(std::string_view value, std::size_t at) {
    return at >= value.size() || value[at] == ' ' || value[at] == '\t';
}

std::optional<std::string_view> requirement_keyword_tail(std::string_view line,
                                                         std::string_view keyword) {
    if (line.size() < keyword.size() ||
        !requirement_equal(line.substr(0, keyword.size()), keyword) ||
        !requirement_word_boundary(line, keyword.size()))
        return {};
    return trim_relation(line.substr(keyword.size()));
}

bool requirement_bare_header(std::string_view source) {
    const auto end = source.find('\n');
    return requirement_equal(trim_relation(source.substr(0, end)), "requirementDiagram");
}

bool requirement_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\f' || value == '\v';
}

std::optional<std::string_view> requirement_greedy_direction(std::string_view line, Work& work) {
    // In INITIAL, the locked lexer tries whole-physical-line rules in this
    // order: .*direction\s+TB[^\n]*, then BT, RL, LR.  Those rules run
    // before quote-state entry, so a matching quoted fragment is deliberately
    // not filtered here. This parser applies the same priority only to a
    // top-level physical statement before declaration/relationship handling.
    if (!work.spend(line.size() * 8u + 4u * sizeof(std::size_t) + 64u)) return {};
    constexpr std::array<std::string_view, 4> priority{{"TB", "BT", "RL", "LR"}};
    constexpr std::string_view token = "direction";
    for (const auto direction : priority) {
        for (std::size_t at = 0; at + token.size() <= line.size(); ++at) {
            if (!requirement_equal(line.substr(at, token.size()), token)) continue;
            std::size_t value = at + token.size();
            if (value == line.size() || !requirement_space(line[value])) continue;
            while (value < line.size() && requirement_space(line[value]))
                ++value;
            if (value + direction.size() <= line.size() &&
                requirement_equal(line.substr(value, direction.size()), direction))
                return direction;
        }
    }
    return {};
}

bool requirement_literal_prepay(std::string_view value, Work& work) {
    // relation_literal_label allocates its decoded string and copies each UTF-8
    // row before it can return a checked value.  Charge its bounded scans and
    // temporaries before invoking it; final projection strings retain their
    // own existing charge at the caller.
    const auto bytes = value.size(), rows = bytes + 1u;
    // Utf8ToGlyphs reserves one string slot per input byte.  Its decoded
    // string, row views, temporary row strings and glyph slots are all bounded
    // before the helper call, using the established relation-TU storage rate.
    constexpr std::size_t per_row = 16u + 4u * sizeof(std::string) + 4u * sizeof(std::string_view);
    constexpr std::size_t fixed = 4u * sizeof(std::string) + 128u;
    if (bytes > kLabelLimit || bytes > work.remaining / rows) return false;
    const auto scan = bytes * rows;
    const auto remaining = work.remaining - scan;
    if (rows > remaining / per_row) return false;
    const auto temporaries = rows * per_row;
    if (fixed > remaining - temporaries) return false;
    return work.spend(scan + temporaries + fixed);
}

std::optional<std::string> requirement_string(std::string_view value, Work& work) {
    value = trim_relation(value);
    if (value.empty() || value.size() > kLabelLimit ||
        value.find_first_of("\r\n{}") != std::string_view::npos)
        return {};
    if (value.front() == '"') {
        if (value.size() < 2u || value.back() != '"') return {};
        const auto inner = value.substr(1, value.size() - 2u);
        if (inner.find_first_of("\r\n\\\"") != std::string_view::npos ||
            !work.spend(inner.size() * 8u + sizeof(std::string) + 64u))
            return {};
        return std::string(inner);
    }
    if (value.find('"') != std::string_view::npos ||
        !work.spend(value.size() * 8u + sizeof(std::string) + 64u))
        return {};
    return std::string(value);
}

std::optional<std::string> requirement_scalar(std::string_view value, Work& work) {
    // A present empty scalar is only the explicit quoted spelling.  Omitted
    // fields remain nullopt, so projection never creates a blank metadata row.
    return requirement_string(value, work);
}

std::optional<RequirementKind> requirement_kind(std::string_view value) {
    if (requirement_equal(value, "requirement")) return RequirementKind::Requirement;
    if (requirement_equal(value, "functionalRequirement"))
        return RequirementKind::FunctionalRequirement;
    if (requirement_equal(value, "interfaceRequirement"))
        return RequirementKind::InterfaceRequirement;
    if (requirement_equal(value, "performanceRequirement"))
        return RequirementKind::PerformanceRequirement;
    if (requirement_equal(value, "physicalRequirement"))
        return RequirementKind::PhysicalRequirement;
    if (requirement_equal(value, "designConstraint")) return RequirementKind::DesignConstraint;
    return {};
}

std::optional<RequirementRelation> requirement_relation(std::string_view value) {
    if (requirement_equal(value, "contains")) return RequirementRelation::Contains;
    if (requirement_equal(value, "copies")) return RequirementRelation::Copies;
    if (requirement_equal(value, "derives")) return RequirementRelation::Derives;
    if (requirement_equal(value, "satisfies")) return RequirementRelation::Satisfies;
    if (requirement_equal(value, "verifies")) return RequirementRelation::Verifies;
    if (requirement_equal(value, "refines")) return RequirementRelation::Refines;
    if (requirement_equal(value, "traces")) return RequirementRelation::Traces;
    return {};
}

std::optional<Risk> requirement_risk(std::string_view value) {
    if (requirement_equal(value, "low")) return Risk::Low;
    if (requirement_equal(value, "medium")) return Risk::Medium;
    if (requirement_equal(value, "high")) return Risk::High;
    return {};
}

std::optional<Verification> requirement_verification(std::string_view value) {
    if (requirement_equal(value, "analysis")) return Verification::Analysis;
    if (requirement_equal(value, "demonstration")) return Verification::Demonstration;
    if (requirement_equal(value, "inspection")) return Verification::Inspection;
    if (requirement_equal(value, "test")) return Verification::Test;
    return {};
}

std::string_view requirement_kind_text(RequirementKind value) {
    switch (value) {
    case RequirementKind::Requirement:
        return "Requirement";
    case RequirementKind::FunctionalRequirement:
        return "Functional Requirement";
    case RequirementKind::InterfaceRequirement:
        return "Interface Requirement";
    case RequirementKind::PerformanceRequirement:
        return "Performance Requirement";
    case RequirementKind::PhysicalRequirement:
        return "Physical Requirement";
    case RequirementKind::DesignConstraint:
        return "Design Constraint";
    }
    return {};
}

std::string_view requirement_risk_text(Risk value) {
    switch (value) {
    case Risk::Low:
        return "Low";
    case Risk::Medium:
        return "Medium";
    case Risk::High:
        return "High";
    }
    return {};
}

std::string_view requirement_verification_text(Verification value) {
    switch (value) {
    case Verification::Analysis:
        return "Analysis";
    case Verification::Demonstration:
        return "Demonstration";
    case Verification::Inspection:
        return "Inspection";
    case Verification::Test:
        return "Test";
    }
    return {};
}

std::string_view requirement_relation_text(RequirementRelation value) {
    switch (value) {
    case RequirementRelation::Contains:
        return "contains";
    case RequirementRelation::Copies:
        return "copies";
    case RequirementRelation::Derives:
        return "derives";
    case RequirementRelation::Satisfies:
        return "satisfies";
    case RequirementRelation::Verifies:
        return "verifies";
    case RequirementRelation::Refines:
        return "refines";
    case RequirementRelation::Traces:
        return "traces";
    }
    return {};
}

// D-066 F0: accepted non-terminal leaves are disclosed through the same
// bounded footer channel the state family uses; the line cap mirrors E2b.
constexpr std::size_t kRequirementNoticeLineLimit = 96u;

std::string requirement_inert_scalar(std::string_view raw, Work& work) {
    auto value = er_strip_comment(raw);
    const auto first = value.find_first_not_of(' ');
    const auto last = value.find_last_not_of(' ');
    value = first == std::string_view::npos ? std::string_view{}
                                            : value.substr(first, last - first + 1u);
    if (!work.spend(value.size() + 1u)) return {};
    if (value.size() >= 2u &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1u, value.size() - 2u);
    return std::string(value);
}

std::optional<RequirementEnvelope> requirement_envelope(std::string_view source, Work& work) {
    if (source.size() > kSourceLimit || source.find('\r') != std::string_view::npos ||
        !source.starts_with("---\n") || !work.spend(source.size() * 2u + 96u))
        return {};
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter || !envelope->description) return {};
    // D-066 F0: a description may coexist with a config subtree that carries
    // only non-terminal (inert) leaves. Any visual/semantic key keeps the
    // whole source on fallback; an empty config block stays rejected.
    bool config_seen = false;
    enum : unsigned { Log = 1u, Font = 2u, Html = 4u };
    unsigned fields = 0u;
    std::string log_value, html_value;
    for (const auto& event : envelope->events) {
        if (event.blank || event.comment) continue;
        if (event.top_level) {
            if (event.path == "description") continue;
            if (event.path == "config" && !config_seen) {
                config_seen = true;
                continue;
            }
            return {};
        }
        if (!config_seen) return {};
        auto text = requirement_inert_scalar(event.value, work);
        const auto mark = [&](unsigned bit) -> bool {
            if (fields & bit) return false;
            fields |= bit;
            return true;
        };
        if (event.namespace_entry) return {};
        if (event.path == "logLevel" && event.indent == 2) {
            if (!mark(Log) || (text != "0" && text != "1" && text != "2")) return {};
            log_value = std::move(text);
        } else if (event.path == "fontFamily" && event.indent == 2) {
            if (!mark(Font) || text != "courier") return {};
        } else if (event.path == "htmlLabels" && event.indent == 2) {
            if (!mark(Html) || (text != "false" && text != "true")) return {};
            html_value = std::move(text);
        } else {
            return {};
        }
    }
    if (config_seen && fields == 0u) return {};
    std::vector<std::string> notices;
    if (fields != 0u) {
        const auto push = [&](std::string line) -> bool {
            if (line.size() > kRequirementNoticeLineLimit ||
                !work.spend(line.size() * 4u + sizeof(std::string) + 32u))
                return false;
            notices.push_back(std::move(line));
            return true;
        };
        if (!push("Requirement native projection v1")) return {};
        if ((fields & Log) && !push("config.logLevel=" + log_value + " -> logger only; not applied"))
            return {};
        if ((fields & Font) &&
            !push("config.fontFamily=\"courier\" -> terminal font; not applied"))
            return {};
        if ((fields & Html) &&
            !push("config.htmlLabels=" + html_value + " -> native text labels; not applied"))
            return {};
    }
    const auto& description = *envelope->description;
    std::string raw(envelope->raw_frontmatter), decoded;
    if (description.form == EnvelopeScalarForm::Folded && description.header == ">-") {
        bool payload = false;
        std::size_t at = 0;
        while (at < description.text.size()) {
            const auto newline = description.text.find('\n', at);
            const auto line = description.text.substr(
                at, newline == std::string_view::npos ? std::string_view::npos : newline - at);
            at = newline == std::string_view::npos ? description.text.size() : newline + 1u;
            if (line.empty()) continue;
            if (line.size() < 3u || !line.starts_with("  ") || line.starts_with("   ") ||
                line.find('\t') != std::string_view::npos)
                return {};
            if (payload) decoded.push_back(' ');
            decoded.append(line.substr(2));
            payload = true;
        }
        if (!payload) return {};
    } else if (description.form == EnvelopeScalarForm::Plain && !description.header.empty()) {
        const auto scalar = description.header;
        if (scalar.empty() || scalar.front() == ' ' || scalar.front() == '\'' ||
            scalar.front() == '"' || scalar.front() == '|' || scalar.front() == '>' ||
            scalar.find('\t') != std::string_view::npos)
            return {};
        decoded = scalar;
    } else {
        return {};
    }
    if (!envelope->raw_body.starts_with("requirementDiagram")) return {};
    return RequirementEnvelope{envelope->raw_body, std::move(raw), std::move(decoded),
                               std::move(notices)};
}

bool requirement_same_role_name(const RequirementNode& node, RequirementRole role,
                                std::string_view name) {
    if (role == RequirementRole::Requirement) {
        const auto* value = std::get_if<RequirementFields>(&node);
        return value && value->logical_name == name;
    }
    const auto* value = std::get_if<ElementFields>(&node);
    return value && value->logical_name == name;
}

bool requirement_append_node(RequirementDiagram& diagram, RequirementNode&& node, Work& work) {
    const auto role = std::holds_alternative<RequirementFields>(node) ? RequirementRole::Requirement
                                                                      : RequirementRole::Element;
    const auto& name = role == RequirementRole::Requirement
                           ? std::get<RequirementFields>(node).logical_name
                           : std::get<ElementFields>(node).logical_name;
    if (name.size() > kLabelLimit || !work.spend(name.size() * 8u + sizeof(RequirementNode) + 64u))
        return false;
    for (const auto& old : diagram.nodes) {
        if (!work.spend(name.size() + sizeof(RequirementNode) + 8u)) return false;
        if (requirement_same_role_name(old, role, name))
            return true;  // first same-kind definition wins
    }
    if (diagram.nodes.size() >= 64u) return false;
    if (diagram.nodes.size() == diagram.nodes.capacity()) {
        const auto capacity =
            std::min(std::size_t{64u}, std::max(std::size_t{1u}, diagram.nodes.size() * 2u));
        if (!work.spend((capacity + diagram.nodes.size() + 1u) * sizeof(RequirementNode) + 128u))
            return false;
        diagram.nodes.reserve(capacity);
    }
    diagram.nodes.push_back(std::move(node));
    return true;
}

bool requirement_append_pending(std::vector<PendingRequirementEdge>& edges,
                                PendingRequirementEdge&& edge, Work& work) {
    if (edges.size() >= 128u || edge.source.size() > kLabelLimit ||
        edge.target.size() > kLabelLimit ||
        !work.spend((edge.source.size() + edge.target.size()) * 8u +
                    sizeof(PendingRequirementEdge) + 64u))
        return false;
    if (edges.size() == edges.capacity()) {
        const auto capacity =
            std::min(std::size_t{128u}, std::max(std::size_t{1u}, edges.size() * 2u));
        if (!work.spend((capacity + edges.size() + 1u) * sizeof(PendingRequirementEdge) + 128u))
            return false;
        edges.reserve(capacity);
    }
    edges.push_back(std::move(edge));
    return true;
}

bool requirement_append_edge(RequirementDiagram& diagram, RequirementEdge edge, Work& work) {
    if (diagram.edges.size() >= 128u || !work.spend(sizeof(RequirementEdge) + 64u)) return false;
    if (diagram.edges.size() == diagram.edges.capacity()) {
        const auto capacity =
            std::min(std::size_t{128u}, std::max(std::size_t{1u}, diagram.edges.size() * 2u));
        if (!work.spend((capacity + diagram.edges.size() + 1u) * sizeof(RequirementEdge) + 128u))
            return false;
        diagram.edges.reserve(capacity);
    }
    diagram.edges.push_back(edge);
    return true;
}

std::optional<NodeRef> requirement_resolve(const RequirementDiagram& diagram, std::string_view name,
                                           Work& work) {
    // The source DB checks Requirement before Element when names collide.
    for (const auto desired : {RequirementRole::Requirement, RequirementRole::Element}) {
        for (std::size_t at = 0; at < diagram.nodes.size(); ++at) {
            if (!work.spend(name.size() + sizeof(RequirementNode) + 8u)) return {};
            if (requirement_same_role_name(diagram.nodes[at], desired, name))
                return NodeRef{desired, at};
        }
    }
    return {};
}

bool requirement_field(RequirementNode& node, std::string_view line, Work& work) {
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) return false;
    const auto key = trim_relation(line.substr(0, colon));
    const auto value = requirement_scalar(line.substr(colon + 1), work);
    if (!value || !work.spend(key.size() + value->size() * 6u + 64u)) return false;
    if (auto* fields = std::get_if<RequirementFields>(&node)) {
        if (requirement_equal(key, "id")) {
            if (!fields->id) fields->id = std::move(*value);
            return true;
        }
        if (requirement_equal(key, "text")) {
            if (!fields->text) fields->text = std::move(*value);
            return true;
        }
        if (requirement_equal(key, "risk")) {
            const auto parsed = requirement_risk(*value);
            if (!parsed) return false;
            if (!fields->risk) fields->risk = *parsed;
            return true;
        }
        if (requirement_equal(key, "verifymethod")) {
            const auto parsed = requirement_verification(*value);
            if (!parsed) return false;
            if (!fields->verification) fields->verification = *parsed;
            return true;
        }
        return false;
    }
    auto& fields = std::get<ElementFields>(node);
    if (requirement_equal(key, "type")) {
        if (!fields.type) fields.type = std::move(*value);
        return true;
    }
    if (requirement_equal(key, "docRef")) {
        if (!fields.doc_ref) fields.doc_ref = std::move(*value);
        return true;
    }
    return false;
}

// Defined with the D204 requirement style grammar below; forward-declared so
// declaration-header `:::` parsing can share the identifier profile.
bool requirement_style_identifier(std::string_view value);

std::optional<RequirementDeclaration> requirement_declaration(std::string_view line, Work& work) {
    const auto open = line.rfind('{');
    if (open == std::string_view::npos) return {};
    const auto after = trim_relation(line.substr(open + 1));
    const bool inline_empty = after == "}";
    if (!after.empty() && !inline_empty) return {};
    const auto before = trim_relation(line.substr(0, open));
    for (const auto keyword :
         {"functionalRequirement", "interfaceRequirement", "performanceRequirement",
          "physicalRequirement", "designConstraint", "requirement", "element"}) {
        const auto tail = requirement_keyword_tail(before, keyword);
        if (!tail) continue;
        // D204: a declaration may carry one inline `name:::class` shorthand.
        // The shorthand is restricted to bare identifiers, matching Mermaid's
        // node-id grammar; quoted names cannot carry a class suffix.
        auto name_text = *tail;
        std::optional<std::string> inline_class;
        if (const auto suffix = name_text.find(":::"); suffix != std::string_view::npos) {
            if (name_text.find(":::", suffix + 3u) != std::string_view::npos) return {};
            const auto class_name = trim_relation(name_text.substr(suffix + 3u));
            name_text = trim_relation(name_text.substr(0, suffix));
            if (!requirement_style_identifier(name_text) ||
                !requirement_style_identifier(class_name))
                return {};
            inline_class = std::string(class_name);
        }
        const auto name = requirement_string(name_text, work);
        if (!name || !work.spend(before.size() + name->size() * 6u + 64u)) return {};
        if (requirement_equal(keyword, "element"))
            return RequirementDeclaration{
                RequirementNode{ElementFields{std::move(*name), {}, {}}}, inline_empty,
                inline_class};
        const auto kind = requirement_kind(keyword);
        if (!kind) return {};
        return RequirementDeclaration{
            RequirementNode{RequirementFields{*kind, std::move(*name), {}, {}, {}, {}}},
            inline_empty, inline_class};
    }
    return {};
}

std::optional<std::size_t> requirement_arrow(std::string_view line, std::string_view token) {
    bool quote = false;
    for (std::size_t at = 0; at + token.size() <= line.size(); ++at) {
        if (line[at] == '"') quote = !quote;
        if (!quote && line.substr(at, token.size()) == token) return at;
    }
    return {};
}

std::optional<std::size_t> requirement_separator(std::string_view line, bool last) {
    bool quote = false;
    std::optional<std::size_t> result;
    for (std::size_t at = 0; at < line.size(); ++at) {
        if (line[at] == '"') quote = !quote;
        if (!quote && line[at] == '-') {
            result = at;
            if (!last) return result;
        }
    }
    return result;
}

std::optional<PendingRequirementEdge> requirement_relationship(std::string_view line,
                                                               std::size_t order, Work& work) {
    const auto forward = requirement_arrow(line, "->"), reverse = requirement_arrow(line, "<-");
    if (forward && reverse) return {};
    const bool reversed = reverse.has_value();
    const auto arrow = reversed ? reverse : forward;
    if (!arrow) return {};
    const auto left = trim_relation(line.substr(0, *arrow));
    const auto right = trim_relation(line.substr(*arrow + 2u));
    const auto split =
        reversed ? requirement_separator(right, false) : requirement_separator(left, true);
    if (!split) return {};
    const auto source_text =
        reversed ? trim_relation(right.substr(*split + 1u)) : trim_relation(left.substr(0, *split));
    const auto relation_text =
        reversed ? trim_relation(right.substr(0, *split)) : trim_relation(left.substr(*split + 1u));
    const auto target_text = reversed ? left : right;
    const auto source = requirement_string(source_text, work),
               target = requirement_string(target_text, work);
    const auto kind = requirement_relation(relation_text);
    if (!source || !target || !kind ||
        !work.spend(line.size() * 6u + source->size() + target->size() + 128u))
        return {};
    return PendingRequirementEdge{*kind, std::move(*source), std::move(*target), order};
}

// ---- D204: bounded requirement style grammar -----------------------------
//
// Requirement sources carry the common Mermaid style vocabulary: classDef
// profiles, `class` assignments, direct `style` statements and the `:::`
// shorthand. The text surface cannot paint source RGB, so every statement is
// validated strictly and projected onto the shared ClassDirectStyles seam
// (palette hue roles plus a disclosure legend). Unknown properties, colors,
// classes or targets reject the whole source: nothing is silently ignored.

// Per-vector statement caps. The shared ClassDirectStyles legend is the
// tighter effective bound: resolve_class_direct_styles fails closed once
// seven distinct legend rows would be emitted, so sources needing more than
// seven distinct painted declarations fall back even below these caps.
constexpr std::size_t kReqStyleLimit = 64u;
constexpr std::size_t kReqClassPerAssignment = 8u;

bool requirement_style_identifier(std::string_view value) {
    if (value.empty() || value.size() > 64u) return false;
    for (std::size_t n = 0; n < value.size(); ++n) {
        const char c = value[n];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
              (n > 0u && c >= '0' && c <= '9')))
            return false;
    }
    return true;
}

// Corpus-bounded CSS named-color profile. Only named colors exercised by the
// pinned official requirement corpus are admitted; each canonical CSS Color 3
// triple is classified through the same hue boundaries as hex inputs
// (source_style_role), keeping the palette-role contract single-sourced.
// Unknown names fail closed, matching the finite-profile convention used by
// the other mermaid renderers. Extend only against new corpus evidence.
struct RequirementNamedColor {
    std::string_view name;
    int red, green, blue;
};

constexpr std::array<RequirementNamedColor, 5> kRequirementNamedColors{{
    {"blue", 0x00, 0x00, 0xFF},
    {"lightblue", 0xAD, 0xD8, 0xE6},
    {"pink", 0xFF, 0xC0, 0xCB},
    {"green", 0x00, 0x80, 0x00},
    {"grey", 0x80, 0x80, 0x80},
}};

std::optional<SourceStyleColor> requirement_style_color(std::string_view raw, Work& work) {
    if (!work.spend(raw.size() * 8u + 128u)) return {};
    if (requirement_equal(raw, "transparent")) {
        SourceStyleColor transparent;
        transparent.transparent = true;
        return transparent;
    }
    if (raw.starts_with('#')) return source_style_color(raw);
    for (const auto& entry : kRequirementNamedColors) {
        if (!requirement_equal(raw, entry.name)) continue;
        SourceStyleColor color;
        color.rgb = {static_cast<unsigned char>(entry.red),
                     static_cast<unsigned char>(entry.green),
                     static_cast<unsigned char>(entry.blue)};
        color.role = source_style_role(entry.red, entry.green, entry.blue);
        return color;
    }
    return {};
}

bool requirement_style_properties(std::string_view rest, ClassStyleProperties& properties,
                                  Work& work) {
    if (!work.spend(rest.size() * 8u + 256u)) return false;
    unsigned seen = 0u;
    while (!rest.empty()) {
        const auto separator = rest.find_first_of(",;");
        const auto component = trim_relation(rest.substr(0, separator));
        if (component.empty()) return false;
        const auto colon = component.find(':');
        if (colon == std::string_view::npos) return false;
        const auto key = trim_relation(component.substr(0, colon));
        const auto value = trim_relation(component.substr(colon + 1u));
        if (value.empty()) return false;
        const unsigned bit = requirement_equal(key, "fill")           ? 1u
                             : requirement_equal(key, "stroke")       ? 2u
                             : requirement_equal(key, "color")        ? 4u
                             : requirement_equal(key, "stroke-width") ? 8u
                             : requirement_equal(key, "font-weight")  ? 16u
                                                                      : 0u;
        if (!bit || (seen & bit)) return false;
        seen |= bit;
        if (bit == 8u) {
            auto number = std::string_view(value);
            // Case-insensitive unit, matching the Class style grammar's
            // ascii-lower normalization.
            if (number.size() >= 2u && requirement_equal(number.substr(number.size() - 2u), "px"))
                number.remove_suffix(2u);
            double width = 0.0;
            const auto parsed =
                std::from_chars(number.data(), number.data() + number.size(), width);
            if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
                !std::isfinite(width) || width < 0.0 || width > 64.0)
                return false;
            properties.width = width;
        } else if (bit == 16u) {
            if (requirement_equal(value, "bold"))
                properties.bold = true;
            else if (requirement_equal(value, "normal"))
                properties.bold = false;
            else
                return false;
        } else {
            const auto color = requirement_style_color(value, work);
            if (!color || (bit != 1u && color->transparent)) return false;
            if (bit == 1u)
                properties.fill = *color;
            else if (bit == 2u)
                properties.stroke = *color;
            else
                properties.color = *color;
        }
        if (separator == std::string_view::npos) break;
        rest = trim_relation(rest.substr(separator + 1u));
    }
    return seen != 0u;
}

bool requirement_keyword_boundary(std::string_view line, std::size_t prefix) {
    return line.size() > prefix && (line[prefix] == ' ' || line[prefix] == '\t');
}

std::optional<RequirementStyleDefinition> requirement_classdef(std::string_view line,
                                                               Work& work) {
    auto rest = trim_relation(line.substr(8u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos || split == 0u) return {};
    const auto name = rest.substr(0, split);
    if (!requirement_style_identifier(name)) return {};
    RequirementStyleDefinition definition;
    definition.name = std::string(name);
    definition.verbatim = std::string(line);
    if (!requirement_style_properties(trim_relation(rest.substr(split)), definition.properties,
                                      work) ||
        !work.spend(line.size() * 6u + sizeof(RequirementStyleDefinition) + 128u))
        return {};
    return definition;
}

std::optional<RequirementStyleAssignment> requirement_class_statement(std::string_view line,
                                                                      Work& work) {
    auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos || split == 0u) return {};
    const auto target = rest.substr(0, split);
    if (!requirement_style_identifier(target)) return {};
    RequirementStyleAssignment assignment;
    assignment.target = std::string(target);
    auto classes = rest.substr(split);
    while (!classes.empty()) {
        const auto comma = classes.find(',');
        const auto name = trim_relation(classes.substr(0, comma));
        if (!requirement_style_identifier(name) || assignment.classes.size() >= kReqClassPerAssignment)
            return {};
        for (const auto& previous : assignment.classes)
            if (previous == name) return {};
        assignment.classes.emplace_back(name);
        if (comma == std::string_view::npos) break;
        classes = trim_relation(classes.substr(comma + 1u));
        if (classes.empty()) return {};  // reject a trailing/empty comma segment
    }
    if (assignment.classes.empty() ||
        !work.spend(line.size() * 6u + sizeof(RequirementStyleAssignment) + 128u))
        return {};
    return assignment;
}

std::optional<RequirementStyleDirect> requirement_style_statement(std::string_view line,
                                                                   Work& work) {
    auto rest = trim_relation(line.substr(5u));
    const auto split = rest.find_first_of(" \t");
    if (split == std::string_view::npos || split == 0u) return {};
    RequirementStyleDirect direct;
    direct.verbatim = std::string(line);
    auto targets = rest.substr(0, split);
    while (!targets.empty()) {
        const auto comma = targets.find(',');
        const auto name = trim_relation(targets.substr(0, comma));
        if (!requirement_style_identifier(name) || direct.targets.size() >= kReqStyleLimit)
            return {};
        for (const auto& previous : direct.targets)
            if (previous == name) return {};
        direct.targets.emplace_back(name);
        if (comma == std::string_view::npos) break;
        targets = targets.substr(comma + 1u);
        if (trim_relation(targets).empty()) return {};  // trailing/empty segment
    }
    if (direct.targets.empty() ||
        !requirement_style_properties(trim_relation(rest.substr(split)), direct.properties, work) ||
        !work.spend(line.size() * 6u + sizeof(RequirementStyleDirect) + 128u))
        return {};
    return direct;
}

std::optional<RequirementStyleAssignment> requirement_shorthand(std::string_view line,
                                                                 Work& work) {
    const auto pos = line.find(":::");
    if (pos == std::string_view::npos || pos == 0u ||
        line.find(":::", pos + 3u) != std::string_view::npos)
        return {};
    const auto target = trim_relation(line.substr(0, pos));
    const auto class_name = trim_relation(line.substr(pos + 3u));
    if (!requirement_style_identifier(target) || !requirement_style_identifier(class_name) ||
        !work.spend(line.size() * 6u + sizeof(RequirementStyleAssignment) + 128u))
        return {};
    return RequirementStyleAssignment{std::string(target), {std::string(class_name)}};
}

bool requirement_node_named(const RequirementDiagram& diagram, std::string_view name, Work& work) {
    // Mirror requirement_resolve precedence: Requirement before Element.
    for (const auto desired : {RequirementRole::Requirement, RequirementRole::Element}) {
        for (const auto& node : diagram.nodes) {
            if (!work.spend(name.size() + 32u)) return false;
            if (requirement_same_role_name(node, desired, name)) return true;
        }
    }
    return false;
}

bool requirement_validate_styles(const RequirementDiagram& diagram, Work& work) {
    if (diagram.style_definitions.size() > kReqStyleLimit ||
        diagram.style_assignments.size() > kReqStyleLimit ||
        diagram.style_directs.size() > kReqStyleLimit)
        return false;
    for (const auto& definition : diagram.style_definitions) {
        if (!work.spend(definition.name.size() + 32u)) return false;
        for (const auto& other : diagram.style_definitions)
            if (&other != &definition && other.name == definition.name) return false;
    }
    const auto definition_known = [&](std::string_view name) {
        for (const auto& definition : diagram.style_definitions)
            if (definition.name == name) return true;
        return false;
    };
    for (const auto& assignment : diagram.style_assignments) {
        if (!work.spend(assignment.target.size() + 64u) ||
            !requirement_node_named(diagram, assignment.target, work))
            return false;
        for (const auto& class_name : assignment.classes)
            if (!definition_known(class_name)) return false;
    }
    for (const auto& direct : diagram.style_directs)
        for (const auto& target : direct.targets)
            if (!work.spend(target.size() + 32u) ||
                !requirement_node_named(diagram, target, work))
                return false;
    return true;
}
}  // namespace

std::optional<RequirementDiagram> parse_requirement(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit || !work.spend(source.size())) return {};
    std::optional<RequirementEnvelope> envelope;
    if (source.starts_with("---")) {
        // A prefixed source is admitted only after the strict complete-prefix
        // reader closes it; a convenient body suffix can never rescue it.
        envelope = requirement_envelope(source, work);
        if (!envelope) return {};
    } else {
        if (!requirement_bare_header(source) || !work.spend(sizeof(RequirementEnvelope) + 64u))
            return {};
        envelope = RequirementEnvelope{source, {}, {}, {}};
    }
    if (!work.spend(2u * sizeof(std::vector<RequirementNode>) +
                    2u * sizeof(std::vector<RequirementEdge>) + sizeof(RequirementDiagram) + 128u))
        return {};
    RequirementDiagram diagram{std::move(*envelope), "TB", {}, {}, {}, {}, {}};
    std::vector<PendingRequirementEdge> pending;
    if (!work.spend(sizeof(std::vector<PendingRequirementEdge>) + 128u)) return {};
    std::size_t at = 0, statement_order = 0;
    bool header = false;
    const auto next_line = [&](std::string_view& line) -> bool {
        if (at > diagram.envelope.body.size()) return false;
        const auto end = diagram.envelope.body.find('\n', at);
        if (end == std::string_view::npos) {
            if (at == diagram.envelope.body.size()) return false;
            line = trim_relation(diagram.envelope.body.substr(at));
            at = diagram.envelope.body.size();
            return work.spend(line.size() + 1u);
        }
        line = trim_relation(diagram.envelope.body.substr(at, end - at));
        at = end + 1u;
        return work.spend(line.size() + 1u);
    };
    for (;;) {
        std::string_view line;
        if (!next_line(line)) break;
        if (line.empty() || line.starts_with("%%")) continue;
        if (!header) {
            if (!requirement_equal(line, "requirementDiagram")) return {};
            header = true;
            continue;
        }
        if (const auto direction = requirement_greedy_direction(line, work)) {
            if (!work.spend(direction->size() * 4u + sizeof(std::string) + 16u)) return {};
            diagram.direction = std::string(*direction);
            continue;  // greedy: last accepted line wins
        }
        if (auto declaration = requirement_declaration(line, work)) {
            // Copy the logical name before the node variant is moved, so an
            // inline class assignment never references a moved-from string.
            const std::string declared_name =
                std::holds_alternative<RequirementFields>(declaration->node)
                    ? std::get<RequirementFields>(declaration->node).logical_name
                    : std::get<ElementFields>(declaration->node).logical_name;
            auto node = std::move(declaration->node);
            bool closed = declaration->inline_empty;
            while (!closed) {
                std::string_view field;
                if (!next_line(field)) return {};
                if (field.empty() || field.starts_with("%%")) continue;
                if (field == "}") {
                    closed = true;
                    break;
                }
                if (!requirement_field(node, field, work)) return {};
            }
            if (!requirement_append_node(diagram, std::move(node), work)) return {};
            if (declaration->inline_class) {
                if (diagram.style_assignments.size() >= kReqStyleLimit ||
                    !work.spend(declared_name.size() + sizeof(RequirementStyleAssignment) + 128u))
                    return {};
                diagram.style_assignments.push_back(
                    {declared_name, {std::move(*declaration->inline_class)}});
            }
            continue;
        }
        if (line.size() > 8u && line.starts_with("classDef") &&
            requirement_keyword_boundary(line, 8u)) {
            auto definition = requirement_classdef(line, work);
            if (!definition || diagram.style_definitions.size() >= kReqStyleLimit) return {};
            diagram.style_definitions.push_back(std::move(*definition));
            continue;
        }
        if (line.size() > 5u && line.starts_with("class") &&
            requirement_keyword_boundary(line, 5u)) {
            auto assignment = requirement_class_statement(line, work);
            if (!assignment || diagram.style_assignments.size() >= kReqStyleLimit) return {};
            diagram.style_assignments.push_back(std::move(*assignment));
            continue;
        }
        if (line.size() > 5u && line.starts_with("style") &&
            requirement_keyword_boundary(line, 5u)) {
            auto direct = requirement_style_statement(line, work);
            if (!direct || diagram.style_directs.size() >= kReqStyleLimit) return {};
            diagram.style_directs.push_back(std::move(*direct));
            continue;
        }
        if (line.find(":::") != std::string_view::npos) {
            auto assignment = requirement_shorthand(line, work);
            if (!assignment || diagram.style_assignments.size() >= kReqStyleLimit) return {};
            diagram.style_assignments.push_back(std::move(*assignment));
            continue;
        }
        auto edge = requirement_relationship(line, statement_order, work);
        if (!edge) return {};
        if (!requirement_append_pending(pending, std::move(*edge), work)) return {};
        ++statement_order;
    }
    if (!header || diagram.nodes.empty()) return {};
    if (!requirement_validate_styles(diagram, work)) return {};
    for (const auto& edge : pending) {
        const auto source = requirement_resolve(diagram, edge.source, work);
        const auto target = requirement_resolve(diagram, edge.target, work);
        if (!source || !target) return {};  // unresolved endpoint rejects the whole source
        if (!requirement_append_edge(diagram, {edge.kind, *source, *target, edge.statement_order},
                                     work))
            return {};
    }
    return diagram;
}

namespace {
std::optional<std::string> requirement_identity(RequirementRole role, std::string_view name,
                                                Work& work) {
    if (name.size() > kLabelLimit || !work.spend(name.size() * 8u + 96u)) return {};
    std::string result = role == RequirementRole::Requirement ? "requirement:" : "element:";
    result.reserve(result.size() + name.size() * 2u);
    constexpr char hex[] = "0123456789abcdef";
    for (const unsigned char byte : name) {
        result.push_back(hex[byte >> 4u]);
        result.push_back(hex[byte & 15u]);
    }
    return result;
}

std::optional<std::string> requirement_box_label(std::string_view stereotype, std::string_view name,
                                                 Work& work) {
    // Stereotype delimiters are fixed Requirement presentation, never parsed
    // source markup.  The source name remains independently plain-text
    // validated before joining the two visible rows.
    if (!requirement_literal_prepay(name, work)) return {};
    const auto checked = relation_literal_label(name);
    if (!checked || *checked != name ||
        !work.spend((stereotype.size() + name.size()) * 8u + sizeof(std::string) + 64u))
        return {};
    std::string label = "<<";
    label.append(stereotype);
    label.append(">>\n");
    label.append(name);
    if (label_width(label) > kLabelLimit) return {};
    return label;
}

std::optional<std::string> requirement_member_text(std::string_view key, std::string_view value,
                                                   Work& work) {
    if (!requirement_literal_prepay(value, work)) return {};
    const auto checked = relation_literal_label(value);
    if (!checked || *checked != value ||
        !work.spend((key.size() + value.size()) * 8u + sizeof(std::string) + 64u))
        return {};
    return std::string(key) + ": " + std::string(value);
}

std::string_view requirement_node_name(const RequirementNode& node) {
    if (const auto* fields = std::get_if<RequirementFields>(&node)) return fields->logical_name;
    return std::get<ElementFields>(node).logical_name;
}

// Project the validated requirement style ledger onto the shared
// ClassDirectStyles seam. Cascade order mirrors Mermaid's stylesheet: the
// implicit `default` classDef first on every node, then class assignments in
// source order, then direct `style` statements last; the shared resolver does
// the per-property merge, painting and legend disclosure.
bool requirement_project_styles(const RequirementDiagram& parsed, RelationDiagram& result,
                                const std::vector<int>& projected, Work& work) {
    if (parsed.style_definitions.empty() && parsed.style_assignments.empty() &&
        parsed.style_directs.empty())
        return true;
    const auto resolve_node = [&](std::string_view name) -> std::optional<std::size_t> {
        for (const auto desired :
             {RequirementRole::Requirement, RequirementRole::Element}) {
            for (std::size_t at = 0; at < parsed.nodes.size(); ++at)
                if (requirement_same_role_name(parsed.nodes[at], desired, name)) return at;
        }
        return std::nullopt;
    };
    const auto find_definition = [&](std::string_view name)
        -> const RequirementStyleDefinition* {
        for (const auto& definition : parsed.style_definitions)
            if (definition.name == name) return &definition;
        return nullptr;
    };
    if (!work.spend(64u * sizeof(ClassDirectStyle) + 4u * sizeof(ClassDirectStyles) + 256u))
        return false;
    auto styles = std::make_unique<ClassDirectStyles>();
    styles->declarations.reserve(kReqStyleLimit);
    const auto emit = [&](std::string_view node_name, std::string class_name,
                          const std::string& verbatim,
                          const ClassStyleProperties& properties) -> bool {
        const auto node = resolve_node(node_name);
        if (!node || *node >= projected.size()) return false;
        const auto entity = static_cast<std::size_t>(projected[*node]);
        if (entity >= result.entities.size()) return false;
        if (styles->declarations.size() >= 128u ||
            !work.spend(verbatim.size() * 8u + sizeof(ClassDirectStyle) + 64u))
            return false;
        styles->declarations.push_back(
            ClassDirectStyle{result.entities[entity].id, verbatim, std::move(class_name), 0u,
                             properties, -1});
        return true;
    };
    if (const auto* default_definition = find_definition("default"))
        for (const auto& node : parsed.nodes)
            if (!emit(requirement_node_name(node), "default", default_definition->verbatim,
                      default_definition->properties))
                return false;
    for (const auto& assignment : parsed.style_assignments) {
        for (const auto& class_name : assignment.classes) {
            const auto* definition = find_definition(class_name);
            // The legend discloses the classDef profile statement plus its
            // resolved palette roles; declaration.target carries the node for
            // painting and class_name is printed in the legend's class field.
            if (!definition ||
                !emit(assignment.target, class_name, definition->verbatim, definition->properties))
                return false;
        }
    }
    for (const auto& direct : parsed.style_directs)
        for (const auto& target : direct.targets)
            if (!emit(target, "", direct.verbatim, direct.properties)) return false;
    result.direct_styles = std::move(styles);
    return resolve_class_direct_styles(result, work, std::span<const std::string_view>{},
                                       parsed.envelope.body);
}
}  // namespace

std::optional<RelationDiagram> project_requirement(const RequirementDiagram& parsed, Work& work) {
    if (parsed.nodes.empty() || parsed.nodes.size() > 64u || parsed.edges.size() > 128u ||
        !work.spend(parsed.nodes.size() * (sizeof(RelationEntity) + 4u * sizeof(Member) + 128u) +
                    parsed.edges.size() * (sizeof(Relationship) + 128u) + 256u))
        return {};
    if (!work.spend(parsed.direction.size() * 4u + sizeof(std::string) + 32u)) return {};
    if (!work.spend(8u * sizeof(std::optional<std::string>) + 128u)) return {};
    RelationDiagram result;
    result.family = RelationFamily::Requirement;
    result.direction = parsed.direction;
    result.entities.reserve(parsed.nodes.size());
    result.edges.reserve(parsed.edges.size());
    if (!work.spend(parsed.nodes.size() * sizeof(int) + 128u)) return {};
    std::vector<int> projected;
    projected.reserve(parsed.nodes.size());
    for (std::size_t at = 0; at < parsed.nodes.size(); ++at) {
        const auto role = std::holds_alternative<RequirementFields>(parsed.nodes[at])
                              ? RequirementRole::Requirement
                              : RequirementRole::Element;
        const auto name =
            role == RequirementRole::Requirement
                ? std::string_view(std::get<RequirementFields>(parsed.nodes[at]).logical_name)
                : std::string_view(std::get<ElementFields>(parsed.nodes[at]).logical_name);
        const auto id = requirement_identity(role, name, work);
        if (!id) return {};
        std::string label;
        std::vector<Member> rows;
        if (role == RequirementRole::Requirement) {
            const auto& fields = std::get<RequirementFields>(parsed.nodes[at]);
            const auto kind = requirement_kind_text(fields.kind);
            if (!work.spend(4u * sizeof(Member) + sizeof(std::vector<Member>) + 128u)) return {};
            const auto title = requirement_box_label(kind, name, work);
            if (!title) return {};
            label = std::move(*title);
            rows.reserve(4u);
            const auto row = [&](std::string_view key, std::string_view value) -> bool {
                const auto text = requirement_member_text(key, value, work);
                if (!text || !work.spend(sizeof(Member) + 32u)) return false;
                rows.push_back({std::move(*text), false});
                return true;
            };
            if (fields.id && !row("ID", *fields.id)) return {};
            if (fields.text && !row("Text", *fields.text)) return {};
            if (fields.risk && !row("Risk", requirement_risk_text(*fields.risk))) return {};
            if (fields.verification &&
                !row("Verification", requirement_verification_text(*fields.verification)))
                return {};
        } else {
            const auto& fields = std::get<ElementFields>(parsed.nodes[at]);
            if (!work.spend(2u * sizeof(Member) + sizeof(std::vector<Member>) + 128u)) return {};
            const auto title = requirement_box_label("Element", name, work);
            if (!title) return {};
            label = std::move(*title);
            rows.reserve(2u);
            const auto row = [&](std::string_view key, std::string_view value) -> bool {
                const auto text = requirement_member_text(key, value, work);
                if (!text || !work.spend(sizeof(Member) + 32u)) return false;
                rows.push_back({std::move(*text), false});
                return true;
            };
            if (fields.type && !row("Type", *fields.type)) return {};
            if (fields.doc_ref && !row("Doc Ref", *fields.doc_ref)) return {};
        }
        RelationEntity entity{
            std::move(*id), std::move(label), {}, EntityShape::Box, -1, std::move(rows), true};
        result.entities.push_back(std::move(entity));
        projected.push_back(static_cast<int>(at));
    }
    for (const auto& edge : parsed.edges) {
        if (edge.source.index >= projected.size() || edge.target.index >= projected.size())
            return {};
        Relationship projected_edge;
        projected_edge.from = projected[edge.source.index];
        projected_edge.to = projected[edge.target.index];
        const auto relation_name = requirement_relation_text(edge.kind);
        if (!work.spend(relation_name.size() * 8u + sizeof(std::string) + 64u)) return {};
        projected_edge.label = "<<" + std::string(relation_name) + ">>";
        if (edge.kind == RequirementRelation::Contains) {
            projected_edge.start = Marker::RequirementCirclePlus;
            projected_edge.end = Marker::None;
            projected_edge.dotted = false;
        } else {
            projected_edge.start = Marker::None;
            projected_edge.end = Marker::RequirementOpenArrow;
            projected_edge.dotted = true;
        }
        if (!append_relationship(result.edges, std::move(projected_edge), work)) return {};
    }
    if (!requirement_project_styles(parsed, result, projected, work)) return {};
    if (!parsed.envelope.config_notices.empty()) {
        std::size_t notice_bytes = 0u;
        for (const auto& line : parsed.envelope.config_notices) notice_bytes += line.size();
        if (!work.spend(parsed.envelope.config_notices.size() * (sizeof(std::string) + 32u) +
                        notice_bytes + 64u))
            return {};
        result.requirement_config_notices = parsed.envelope.config_notices;
    }
    return result;
}
}  // namespace loom::ui::mermaid::detail
