module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;
import loom.ui.foundation.theme_provider;

namespace loom::ui::mermaid {
namespace {

using namespace detail;

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

bool id_start(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool id_char(char value) { return id_start(value) || (value >= '0' && value <= '9'); }

// This is the bounded ASCII projection needed by the pinned Flow lexer.  Keep
// complete-link precedence in flow_statement: hyphen never absorbs --> or
// -.->, while dotted and hyphenated source IDs remain whole identifiers.
bool flow_id_start(char value) { return id_char(value); }
bool flow_id_continue(std::string_view value, std::size_t at) {
    const auto ch = value[at];
    if (flow_id_start(ch) || ch == '.') return true;
    return ch == '-' && at + 1u < value.size() && value[at + 1u] != '>' &&
           value[at + 1u] != '-' && value[at + 1u] != '.';
}

// Syntax admission remains local to the flow/sequence grammar. After UTF-8
// validation, ASCII marker bytes cannot occur inside a multibyte scalar.
bool plain_unicode(std::string_view text) {
    return detail::unicode_text(text) && text.find_first_of("<>&#`") == std::string_view::npos;
}

std::optional<std::string> label(std::string_view input, bool delimited = false) {
    input = trim(input);
    if (input.empty()) return {};
    if (input.front() == '"') {
        if (input.size() < 2 || input.back() != '"') return {};
        input = input.substr(1, input.size() - 2);
        // Flowchart quoted strings do not use C-style backslash decoding.
        // Preserve source for escapes instead of inventing a different label.
        if (input.find_first_of("\"\\") != std::string_view::npos) return {};
    } else {
        if (input.find('"') != std::string_view::npos) return {};
        if (delimited && input.find_first_of("[]{}()|") != std::string_view::npos) return {};
    }
    // The pinned Mermaid createText renderer expands Font Awesome markers,
    // even inside otherwise plain quoted labels. Drawing them literally would
    // lose an icon while claiming the whole diagram was understood.
    for (std::size_t at = 0; (at = input.find("fa", at)) != std::string_view::npos; ++at) {
        auto marker = input.substr(at + 2);
        if (!marker.empty() && std::string_view{"bklrs"}.find(marker.front()) != std::string_view::npos)
            marker.remove_prefix(1);
        if (!marker.starts_with(":fa-")) continue;
        marker.remove_prefix(4);
        if (!marker.empty() && (marker.front() == '-' || id_char(marker.front()))) return {};
    }
    // Flowchart line breaks are presentation syntax, not arbitrary HTML. Decode
    // only the explicit break forms, then validate every retained plain row.
    std::string decoded;
    std::size_t glyphs = 0, cells = 0;
    while (!input.empty()) {
        if (input.starts_with("<br>") || input.starts_with("<br/>") || input.starts_with("<br />")) {
            const auto end = input.find('>');
            decoded += '\n'; input.remove_prefix(end + 1); ++glyphs;
        } else {
            const auto end = input.find('<');
            const auto row = input.substr(0, end);
            if (!plain_unicode(row)) return {};
            const auto parts = ftxui::Utf8ToGlyphs(std::string(row));
            std::string round_trip;
            for (const auto& part : parts) round_trip += part;
            if (round_trip != row) return {};
            glyphs += parts.size(); cells += static_cast<std::size_t>(ftxui::string_width(std::string(row)));
            decoded += row;
            input.remove_prefix(row.size());
            if (!input.empty() && !input.starts_with("<br>") && !input.starts_with("<br/>") && !input.starts_with("<br />")) return {};
        }
        if (glyphs > kLabelLimit || cells > kLabelLimit) return {};
    }
    if (trim(decoded).empty()) return {};
    return decoded;
}


struct Scanner {
    std::string_view rest;
    void space() { rest = trim_front(rest); }
    static std::string_view trim_front(std::string_view value) {
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        return value;
    }
    bool take(std::string_view text) {
        if (!rest.starts_with(text)) return false;
        rest.remove_prefix(text.size());
        return true;
    }
    std::optional<std::string> id() {
        space();
        if (rest.empty() || !flow_id_start(rest.front())) return {};
        std::size_t length = 1;
        while (length < rest.size() && flow_id_continue(rest, length)) ++length;
        auto result = std::string(rest.substr(0, length));
        rest.remove_prefix(length);
        return result;
    }
    std::optional<std::string_view> enclosed_view(char close) {
        bool quoted = false;
        bool escaped = false;
        for (std::size_t at = 0; at < rest.size(); ++at) {
            const auto ch = rest[at];
            if (escaped) { escaped = false; continue; }
            if (quoted && ch == '\\') { escaped = true; continue; }
            if (ch == '"') { quoted = !quoted; continue; }
            if (!quoted && ch == close) {
                const auto result = rest.substr(0, at);
                rest.remove_prefix(at + 1);
                return result;
            }
        }
        return {};
    }
    std::optional<std::string> enclosed(char close) {
        const auto raw = enclosed_view(close);
        return raw ? label(*raw, true) : std::nullopt;
    }
};

enum class Direction { Down, Up, Right, Left };
enum class Shape {
    Rectangle, Rounded, Decision,
    Stadium, Circle, Subroutine, DoubleCircle, Hexagon, Cylinder, Asymmetric,
    Trapezoid, TrapezoidAlt, Parallelogram, ParallelogramAlt, Triangle, Delay, SmallCircle, ForkJoin,
};
enum class Stroke { Solid, Dotted, Thick };

bool classic_shape(Shape shape) {
    switch (shape) {
    case Shape::Stadium: case Shape::Circle: case Shape::Subroutine: case Shape::DoubleCircle:
    case Shape::Hexagon: case Shape::Cylinder: case Shape::Asymmetric: case Shape::Trapezoid:
    case Shape::TrapezoidAlt: case Shape::Parallelogram: case Shape::ParallelogramAlt:
    case Shape::Triangle: case Shape::Delay: case Shape::SmallCircle: case Shape::ForkJoin:
        return true;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}
int classic_height(Shape shape) {
    if (shape == Shape::Circle || shape == Shape::SmallCircle) return 9;
    if (shape == Shape::DoubleCircle) return 13;
    if (shape == Shape::Cylinder) return 7;
    if (shape == Shape::ForkJoin) return 5;
    return 5;
}
int classic_min_width(Shape shape) {
    if (shape == Shape::DoubleCircle) return 13;
    if (shape == Shape::Cylinder || shape == Shape::Parallelogram || shape == Shape::ParallelogramAlt) return 11;
    if (shape == Shape::ForkJoin) return 17;
    return 9;
}
bool classic_requires_odd_width(Shape shape) {
    switch (shape) {
    case Shape::Circle: case Shape::SmallCircle: case Shape::DoubleCircle: case Shape::Hexagon: case Shape::Cylinder:
    case Shape::Trapezoid: case Shape::TrapezoidAlt: case Shape::Parallelogram: case Shape::ParallelogramAlt:
    case Shape::Triangle:
        return true;
    case Shape::Stadium: case Shape::Subroutine: case Shape::Asymmetric: case Shape::Delay: case Shape::ForkJoin:
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}
struct Vertex {
    std::string id;
    std::string text;
    Shape shape = Shape::Rectangle;
    bool explicit_label = false;
    int group = -1;
    std::size_t order = 0;
};
enum class EndpointKind : unsigned char { Vertex, Group };

struct Endpoint {
    EndpointKind kind = EndpointKind::Vertex;
    std::size_t index = 0u;
    auto operator<=>(const Endpoint&) const = default;
};

struct Edge {
    Endpoint from{};
    Endpoint to{};
    std::optional<std::string> text;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    bool arrow_end = true;
    int minimum_length = 1;
};
enum class LinkScan { NoMatch, Complete, Unsupported };

struct LinkLexeme {
    LinkScan state = LinkScan::NoMatch;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    bool arrow_end = true;
    int minimum_length = 1;
    std::size_t consumed = 0;
};

bool link_space(char value) { return value == ' ' || value == '\t'; }

std::size_t link_space_after(std::string_view value, std::size_t at) {
    while (at < value.size() && link_space(value[at])) ++at;
    return at;
}

// This finite projection retains the primary complete-LINK-before-START_LINK
// priority. It recognizes every normal, dotted, and thick spelling needed by
// the closed LinkProfile3 sources, then refuses markers outside point/none.
LinkLexeme complete_link(std::string_view value) {
    LinkLexeme result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') { result.arrow_start = true; ++at; }
    else if (value[at] == 'x' || value[at] == 'o') { invalid_start = true; ++at; }
    if (at >= value.size()) return {};

    const auto finish = [&](Stroke stroke, bool arrow_end, int minimum_length, std::size_t end,
                            bool invalid_end) {
        LinkLexeme link;
        link.stroke = stroke;
        link.arrow_start = result.arrow_start;
        link.arrow_end = arrow_end;
        link.state = (invalid_start || invalid_end || (link.arrow_start && !arrow_end))
            ? LinkScan::Unsupported : LinkScan::Complete;
        link.minimum_length = minimum_length;
        link.consumed = link_space_after(value, end);
        return link;
    };

    if (value[at] == '-') {
        std::size_t end = at;
        while (end < value.size() && value[end] == '-') ++end;
        const auto count = end - at;
        if (count < 2) return {};
        if (end < value.size() && (value[end] == '>' || value[end] == 'x' || value[end] == 'o'))
            return finish(Stroke::Solid, value[end] == '>', static_cast<int>(count - 1u), end + 1u,
                          value[end] != '>');
        if (count >= 3u)
            return finish(Stroke::Solid, false, static_cast<int>(count - 2u), end, false);
        return {};
    }
    if (value[at] == '=') {
        std::size_t end = at;
        while (end < value.size() && value[end] == '=') ++end;
        const auto count = end - at;
        if (count < 2) return {};
        if (end < value.size() && (value[end] == '>' || value[end] == 'x' || value[end] == 'o'))
            return finish(Stroke::Thick, value[end] == '>', static_cast<int>(count - 1u), end + 1u,
                          value[end] != '>');
        if (count >= 3u)
            return finish(Stroke::Thick, false, static_cast<int>(count - 2u), end, false);
        return {};
    }
    if (value[at] == '-' || value[at] == '.') return {};
    return {};
}

LinkLexeme complete_dotted_link(std::string_view value) {
    LinkLexeme result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') { result.arrow_start = true; ++at; }
    else if (value[at] == 'x' || value[at] == 'o') { invalid_start = true; ++at; }
    if (at < value.size() && value[at] == '-') ++at;
    const auto dots = at;
    while (at < value.size() && value[at] == '.') ++at;
    if (at == dots || at >= value.size() || value[at] != '-') return {};
    ++at;
    bool arrow_end = false, invalid_end = false;
    if (at < value.size() && (value[at] == '>' || value[at] == 'x' || value[at] == 'o')) {
        arrow_end = value[at] == '>';
        invalid_end = !arrow_end;
        ++at;
    }
    result.state = (invalid_start || invalid_end || (result.arrow_start && !arrow_end))
        ? LinkScan::Unsupported : LinkScan::Complete;
    result.stroke = Stroke::Dotted;
    result.arrow_end = arrow_end;
    // The primary FlowDB measures dotted rank length by dots, not hyphens.
    result.minimum_length = static_cast<int>(std::ranges::count(value.substr(dots, at - dots), '.'));
    result.consumed = link_space_after(value, at);
    return result;
}

LinkLexeme complete_flow_link(std::string_view value) {
    const auto dotted = complete_dotted_link(value);
    if (dotted.state != LinkScan::NoMatch) return dotted;
    return complete_link(value);
}

struct StartLink {
    LinkScan state = LinkScan::NoMatch;
    Stroke stroke = Stroke::Solid;
    bool arrow_start = false;
    std::size_t consumed = 0;
};

StartLink start_link(std::string_view value) {
    StartLink result;
    if (value.empty()) return result;
    std::size_t at = 0;
    bool invalid_start = false;
    if (value[at] == '<') { result.arrow_start = true; ++at; }
    else if (value[at] == 'x' || value[at] == 'o') { invalid_start = true; ++at; }
    if (at + 1u >= value.size()) return {};
    if (value.substr(at, 2u) == "--") result.stroke = Stroke::Solid;
    else if (value.substr(at, 2u) == "==") result.stroke = Stroke::Thick;
    else if (value.substr(at, 2u) == "-.") result.stroke = Stroke::Dotted;
    else return {};
    result.state = invalid_start ? LinkScan::Unsupported : LinkScan::Complete;
    result.consumed = link_space_after(value, at + 2u);
    return result;
}

std::optional<std::pair<std::string_view, LinkLexeme>> edge_text_until_link(
    std::string_view value, const StartLink& start) {
    bool quoted = false;
    bool escaped = false;
    for (std::size_t at = 0; at < value.size(); ++at) {
        const auto ch = value[at];
        if (escaped) { escaped = false; continue; }
        if (quoted && ch == '\\') { escaped = true; continue; }
        if (ch == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        const auto link = complete_flow_link(value.substr(at));
        if (link.state == LinkScan::Unsupported) return {};
        if (link.state == LinkScan::Complete) {
            if (link.stroke != start.stroke || link.arrow_start) return {};
            return std::pair{value.substr(0, at), link};
        }
        // Jison edgeText may contain a single '-' or '=', but a possible
        // complete-link prefix cannot be swallowed as prose after it fails.
        const auto tail = value.substr(at);
        if (tail.starts_with("--") || tail.starts_with("==") || tail.starts_with("-.") ||
            ((ch == '<' || ch == 'x' || ch == 'o') &&
             (tail.substr(1).starts_with("--") || tail.substr(1).starts_with("==") ||
              tail.substr(1).starts_with("-.")))) return {};
        // In dottedEdgeText, a dot run that did not produce LINK has no
        // internal dotted-LINK start: the primary rule needs its terminal '-'.
        // Skip it once so an unclosed run cannot trigger suffix rescans.
        if (ch == '.') {
            std::size_t end = at;
            while (end < value.size() && value[end] == '.') ++end;
            at = end - 1u;
        }
    }
    return {};
}

struct Group {
    std::string id, text;
    int parent = -1;
    std::optional<Direction> direction;
    std::size_t order = 0;
    bool implicit_id = false;
};
struct Diagram {
    Direction direction = Direction::Down;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Group> groups;
    std::vector<std::size_t> group_stack;
    std::size_t next_order = 0;
    std::size_t group_counter = 0;
    bool has_group_endpoint = false;
    std::optional<std::size_t> vertex(std::string id, Work& work, bool& cross_owner_membership_request) {
        (void)work; // RFC 0031 E5: per-vertex bookkeeping folded into the
                   // whole-body flow_parse_prepay (both profiles).
        if (std::ranges::any_of(groups, [&](const Group& group) { return group.id == id; })) return {};
        for (std::size_t n = 0; n < vertices.size(); ++n)
            if (vertices[n].id == id) {
                if (!group_stack.empty() && vertices[n].group >= 0 &&
                    vertices[n].group != static_cast<int>(group_stack.back())) cross_owner_membership_request = true;
                if (!group_stack.empty() && vertices[n].group < 0)
                    vertices[n].group = static_cast<int>(group_stack.back());
                return n;
            }
        if (vertices.size() >= 64u) return {};
        vertices.push_back({id, id, Shape::Rectangle, false,
            group_stack.empty() ? -1 : static_cast<int>(group_stack.back()), next_order++});
        return vertices.size() - 1;
    }
};

bool flow_envelope_parse_prepay(std::string_view body, Work& work);
bool flow_parse_prepay(std::string_view body, Work& work, bool full_profile,
                       bool upgrade_to_full = false);

struct GroupHeader {
    std::optional<std::string> id;
    std::string title;
};

std::optional<GroupHeader> flow_group_header(Scanner& group) {
    group.space();
    const auto source = group.rest;
    if (source.empty()) return {};
    std::optional<std::string> id;
    std::optional<std::string> title;
    if (source.front() != '"') {
        id = group.id();
        if (id) {
            group.space();
            if (group.take("[")) {
                title = group.enclosed(']');
                group.space();
                if (!title || !group.rest.empty()) return {};
            }
        }
    }
    if (!title) {
        // The primary Flow DB receives the same id/title object for a plain
        // whitespace title, then assigns a subGraphN identity at completion.
        title = label(source);
        if (!title) return {};
        group.rest = {};
        if (title->find_first_of(" \t") != std::string::npos) id.reset();
        else id = *title;
    }
    return GroupHeader{std::move(id), std::move(*title)};
}

struct FlowShapeData {
    Shape shape = Shape::Rectangle;
    std::optional<std::string> label;
};

std::optional<FlowShapeData> flow_shape_data(Scanner& scanner, Work& work) {
    // RFC 0031 E5: this closed fixed-token production is covered by the
    // whole-body flow_parse_prepay fee.
    (void)work;
    if (!scanner.take("@{")) return {};
    scanner.space();
    if (!scanner.take("shape")) return {};
    scanner.space();
    if (!scanner.take(":")) return {};
    scanner.space();
    const auto token = scanner.id();
    if (!token) return {};
    scanner.space();
    Shape shape;
    if (*token == "rect" || *token == "process" || *token == "proc" ||
        *token == "rectangle") shape = Shape::Rectangle;
    else if (*token == "rounded" || *token == "event") shape = Shape::Rounded;
    else if (*token == "sm-circ" || *token == "small-circle" || *token == "start") shape = Shape::SmallCircle;
    else if (*token == "diam" || *token == "decision" || *token == "diamond") shape = Shape::Decision;
    else if (*token == "stadium" || *token == "pill" || *token == "terminal") shape = Shape::Stadium;
    else if (*token == "db" || *token == "database" || *token == "cylinder" || *token == "cyl") shape = Shape::Cylinder;
    else if (*token == "fr-rect" || *token == "subproc" || *token == "subprocess" ||
             *token == "framed-rectangle" || *token == "subroutine") shape = Shape::Subroutine;
    else if (*token == "hex" || *token == "hexagon" || *token == "prepare") shape = Shape::Hexagon;
    else if (*token == "lean-l" || *token == "lean-left" || *token == "out-in") shape = Shape::Parallelogram;
    else if (*token == "lean-r" || *token == "lean-right" || *token == "in-out") shape = Shape::ParallelogramAlt;
    else if (*token == "trap-b" || *token == "trapezoid-bottom" || *token == "priority") shape = Shape::Trapezoid;
    else if (*token == "trap-t" || *token == "trapezoid-top" || *token == "manual") shape = Shape::TrapezoidAlt;
    else if (*token == "dbl-circ" || *token == "double-circle") shape = Shape::DoubleCircle;
    else if (*token == "extract" || *token == "tri" || *token == "triangle") shape = Shape::Triangle;
    else if (*token == "delay" || *token == "half-rounded-rectangle") shape = Shape::Delay;
    else if (*token == "fork" || *token == "join") shape = Shape::ForkJoin;
    else return {};
    std::optional<std::string> shape_label;
    if (scanner.take(",")) {
        scanner.space();
        if (!scanner.take("label")) return {};
        scanner.space();
        if (!scanner.take(":")) return {};
        scanner.space();
        if (scanner.rest.empty() || scanner.rest.front() != '"') return {};
        const auto end = scanner.rest.find('"', 1u);
        if (end == std::string_view::npos) return {};
        shape_label = label(scanner.rest.substr(0u, end + 1u));
        if (!shape_label) return {};
        scanner.rest.remove_prefix(end + 1u);
        scanner.space();
    }
    if (!scanner.take("}")) return {};
    return FlowShapeData{shape, std::move(shape_label)};
}

bool classic_opener(std::string_view text) {
    return text.starts_with("((") || text.starts_with("([") || text.starts_with("[[") ||
        text.starts_with("[(") || text.starts_with("{{") || text.starts_with(">") ||
        text.starts_with("[/") || text.starts_with("[\\");
}

struct ClassicDeclaration { Shape shape; std::string text; };
std::optional<ClassicDeclaration> classic_declaration(Scanner& scanner, Work& work) {
    // RFC 0031 E5: the eleven bracket forms are a closed Flow production; the
    // allocation-free close-delimiter scan is input-bounded and folded into
    // the whole-body flow_parse_prepay fee.
    (void)work;
    struct Form { std::string_view open, close; Shape shape; };
    // Prefix forms are ordered longest-first. The bracket/slash alternatives
    // stay explicit: this is not an alias table or a generic metadata parser.
    constexpr std::array<Form, 11> forms{{
        {"(((", ")))", Shape::DoubleCircle}, {"((", "))", Shape::Circle},
        {"([", "])", Shape::Stadium}, {"[(", ")]", Shape::Cylinder},
        {"[[", "]]", Shape::Subroutine}, {"{{", "}}", Shape::Hexagon},
        {">", "]", Shape::Asymmetric}, {"[/", "\\]", Shape::Trapezoid},
        {"[\\", "/]", Shape::TrapezoidAlt}, {"[/", "/]", Shape::Parallelogram},
        {"[\\", "\\]", Shape::ParallelogramAlt},
    }};
    const auto selected = std::ranges::find_if(forms, [&](const Form& form) {
        return scanner.rest.starts_with(form.open);
    });
    if (selected == forms.end()) return {};
    const auto body = scanner.rest.substr(selected->open.size());
    bool quoted = false, escaped = false;
    for (std::size_t at = 0; at < body.size(); ++at) {
        const auto ch = body[at];
        if (escaped) { escaped = false; continue; }
        if (quoted && ch == '\\') { escaped = true; continue; }
        if (ch == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        // Shared openers own the first complete unquoted close, regardless of
        // which alternative appears first in the grammar table. A later node's
        // close cannot steal this declaration's earlier valid boundary.
        for (const auto& form : forms) {
            if (form.open != selected->open || !body.substr(at).starts_with(form.close)) continue;
            auto text = label(body.substr(0, at), true);
            if (!text) return {};
            scanner.rest = body.substr(at + form.close.size());
            return ClassicDeclaration{form.shape, std::move(*text)};
        }
    }
    return {};
}

std::optional<std::size_t> flow_node_from_id(std::string id, Scanner& scanner, Diagram& graph, Work& work,
                                             bool& cross_owner_membership_request) {
    // Unsupported structural keywords must not turn into ordinary nodes.
    constexpr std::string_view reserved[]{"end", "subgraph", "direction", "style",
        "linkStyle", "classDef", "class", "click"};
    if (std::ranges::find(reserved, id) != std::end(reserved)) return {};
    auto index = graph.vertex(std::move(id), work, cross_owner_membership_request);
    if (!index) return {};
    scanner.space();
    if (scanner.rest.empty()) return index;
    if (scanner.rest.starts_with("@{")) {
        const auto metadata = flow_shape_data(scanner, work);
        if (!metadata) return {};
        auto& node = graph.vertices[*index];
        node.shape = metadata->shape; // Pinned FlowDB shape metadata is last-wins.
        if (metadata->label) {
            if (node.explicit_label && node.text != *metadata->label) return {};
            node.text = std::move(*metadata->label);
            node.explicit_label = true;
        }
        return index;
    }
    if (classic_opener(scanner.rest)) {
        const auto declaration = classic_declaration(scanner, work);
        if (!declaration) return {};
        auto& node = graph.vertices[*index];
        if (node.explicit_label && (node.text != declaration->text || node.shape != declaration->shape)) return {};
        node.text = declaration->text;
        node.shape = declaration->shape;
        node.explicit_label = true;
        scanner.space();
        // Shape metadata has its independently closed three-token grammar. A
        // classic bracket declaration never flattens to its old vocabulary.
        if (scanner.rest.starts_with("@{")) return {};
        return index;
    }
    const auto open = scanner.rest.front();
    if (open != '[' && open != '(' && open != '{') return index;
    scanner.rest.remove_prefix(1);
    // These are distinct official shape openers, not punctuation inside the
    // supported rectangular/rounded labels. Quoted literal labels still work.
    if (!scanner.rest.empty() &&
        ((open == '[' && (scanner.rest.front() == '/' || scanner.rest.front() == '\\')) ||
         (open == '(' && scanner.rest.front() == '-'))) return {};
    const auto shape = open == '[' ? Shape::Rectangle : open == '(' ? Shape::Rounded : Shape::Decision;
    auto text = scanner.enclosed(open == '[' ? ']' : open == '(' ? ')' : '}');
    if (!text) return {};
    auto& node = graph.vertices[*index];
    if (node.explicit_label && (node.text != *text || node.shape != shape)) return {};
    node.text = std::move(*text);
    node.shape = shape;
    node.explicit_label = true;
    scanner.space();
    if (scanner.rest.starts_with("@{")) {
        const auto metadata = flow_shape_data(scanner, work);
        if (!metadata || (metadata->label && *metadata->label != node.text)) return {};
        node.shape = metadata->shape; // Metadata follows this vertex declaration and wins.
    }
    return index;
}

std::optional<Endpoint> flow_endpoint(Scanner& scanner, Diagram& graph, Work& work,
                                      bool& cross_owner_membership_request) {
    auto id = scanner.id();
    if (!id) return {};
    // Only an exact, already-declared Group.id has group endpoint meaning.
    // The comparison charge precedes each scan; titles and implicit ids never
    // participate because the parser keeps no title lookup table.
    for (std::size_t index = 0u; index < graph.groups.size(); ++index) {
        const auto& group = graph.groups[index];
        if (group.id != *id) continue;
        scanner.space();
        // A group name is an endpoint only. It cannot acquire a vertex shape,
        // metadata, or compound endpoint role; a later link may make it a
        // terminal endpoint, while flow_statement rejects a bare group line.
        if (scanner.rest.starts_with("&") || scanner.rest.starts_with("@{") ||
            classic_opener(scanner.rest) || scanner.rest.starts_with("[") ||
            scanner.rest.starts_with("(") || scanner.rest.starts_with("{")) return {};
        return Endpoint{EndpointKind::Group, index};
    }
    const auto vertex = flow_node_from_id(std::move(*id), scanner, graph, work,
                                          cross_owner_membership_request);
    return vertex ? std::optional<Endpoint>{Endpoint{EndpointKind::Vertex, *vertex}} : std::nullopt;
}

bool flow_statement(std::string_view text, Diagram& graph, Work& work, std::string_view original,
                    bool already_prepaid, bool& saw_reopen, bool& cross_owner_membership_request) {
    // RFC 0031 E5: the former bounded statement-local fee is covered by the
    // whole-body flow_parse_prepay (bare or configured profile) charged at
    // parse() entry; only the reopen profile activation below still charges.
    if (text == "end") {
        if (graph.group_stack.empty()) return false;
        const auto completed_index = graph.group_stack.back();
        auto& completed = graph.groups[completed_index];
        if (completed.implicit_id) {
            auto generated = "subGraph" + std::to_string(graph.group_counter);
            if (std::ranges::any_of(graph.vertices, [&](const Vertex& vertex) { return vertex.id == generated; }) ||
                std::ranges::any_of(graph.groups, [&](const Group& candidate) {
                    return &candidate != &completed && candidate.id == generated;
                })) return false;
            completed.id = std::move(generated);
        }
        ++graph.group_counter;
        graph.group_stack.pop_back(); return true;
    }
    if (text.starts_with("subgraph ") || text.starts_with("subgraph\t")) {
        Scanner group{text}; group.take("subgraph");
        if (graph.group_stack.size() >= kGroupDepthLimit) return false;
        auto header = flow_group_header(group);
        if (!header || (header->id && std::ranges::any_of(graph.vertices,
            [&](const Vertex& v) { return v.id == *header->id; }))) return false;
        std::optional<std::size_t> existing;
        if (header->id) for (std::size_t n = 0; n < graph.groups.size(); ++n) {
            if (graph.groups[n].id == *header->id) { existing = n; break; }
        }
        if (existing) {
            const int parent = graph.group_stack.empty() ? -1 : static_cast<int>(graph.group_stack.back());
            if (graph.groups[*existing].implicit_id || graph.groups[*existing].parent != parent ||
                std::ranges::find(graph.group_stack, *existing) != graph.group_stack.end()) return false;
            // Only a newly activated bare repeat pays the delta to the
            // full-source profile, instead of the whole full bound on top of
            // the already-paid bare profile. Earlier live elements are
            // included in its future relocation bound.
            if (!saw_reopen && !already_prepaid) {
                if (!flow_parse_prepay(original, work, true, true)) return false;
            }
        } else if (graph.groups.size() >= kGroupLimit) return false;
        if (existing) {
            if (graph.groups[*existing].text != header->title) return false;
            saw_reopen = true;
            graph.group_stack.push_back(*existing); return true;
        }
        const bool implicit_id = !header->id;
        graph.groups.push_back({header->id ? std::move(*header->id) : std::string{}, std::move(header->title),
            graph.group_stack.empty() ? -1 : static_cast<int>(graph.group_stack.back()), {}, graph.next_order++, implicit_id});
        graph.group_stack.push_back(graph.groups.size() - 1); return true;
    }
    if (text.starts_with("direction ") || text.starts_with("direction\t")) {
        if (graph.group_stack.empty()) return false;
        Scanner direction{text}; direction.take("direction"); direction.space();
        auto& value = graph.groups[graph.group_stack.back()].direction;
        if (direction.rest == "LR") value = Direction::Right;
        else if (direction.rest == "RL") value = Direction::Left;
        else if (direction.rest == "TD" || direction.rest == "TB") value = Direction::Down;
        else if (direction.rest == "BT") value = Direction::Up;
        else return false;
        return true;
    }
    Scanner scanner{text};
    bool uses_group_endpoint = false;
    auto nodes = [&]() -> std::optional<std::vector<Endpoint>> {
        constexpr std::size_t kEndpointSlots = 64u;
        std::vector<Endpoint> result;
        while (true) {
            const auto endpoint = flow_endpoint(scanner, graph, work, cross_owner_membership_request);
            if (!endpoint || result.size() >= kEndpointSlots) return {};
            // A group id has a closed scalar grammar. Reject both `G & A`
            // and `A & G` before reserving or storing the mixed compound.
            if (endpoint->kind == EndpointKind::Group && !result.empty()) return {};
            if (result.size() == result.capacity()) {
                const auto capacity = result.capacity();
                const auto next_capacity = capacity == 0u ? 1u : std::min(kEndpointSlots, capacity * 2u);
                if (next_capacity <= capacity) return {};
                result.reserve(next_capacity);
            }
            uses_group_endpoint = uses_group_endpoint || endpoint->kind == EndpointKind::Group;
            result.push_back(*endpoint);
            scanner.space();
            if (!scanner.take("&")) return result;
            if (endpoint->kind == EndpointKind::Group) return {};
        }
    };
    auto from = nodes();
    if (!from) return false;
    bool emitted_link = false;
    scanner.space();
    // RFC 0031 E5: the former bounded per-statement LINK/EDGE_TEXT scan and
    // label copies are covered by the whole-body flow_parse_prepay fee.
    while (!scanner.rest.empty()) {
        LinkLexeme link = complete_flow_link(scanner.rest);
        std::optional<std::string> edge_label;
        if (link.state == LinkScan::Unsupported) return false;
        if (link.state == LinkScan::Complete) {
            scanner.rest.remove_prefix(link.consumed);
            // Preserve pipe labels for every fully recognized stroke form.
            if (scanner.take("|")) {
                const auto raw = scanner.enclosed_view('|');
                if (!raw) return false;
                edge_label = label(*raw, true);
                if (!edge_label) return false;
            }
        } else {
            const auto start = start_link(scanner.rest);
            if (start.state != LinkScan::Complete) return false;
            scanner.rest.remove_prefix(start.consumed);
            const auto text_and_link = edge_text_until_link(scanner.rest, start);
            if (!text_and_link) return false;
            const auto raw = trim(text_and_link->first);
            if (raw.empty()) return false;
            edge_label = label(raw, true);
            if (!edge_label) return false;
            link = text_and_link->second;
            link.arrow_start = start.arrow_start;
            if (link.arrow_start && !link.arrow_end) return false;
            scanner.rest.remove_prefix(text_and_link->first.size() + link.consumed);
        }
        auto to = nodes();
        if (!to || from->size() > (128u - graph.edges.size()) / to->size()) return false;
        const auto carries_group = std::ranges::any_of(*from, [](const Endpoint& endpoint) {
            return endpoint.kind == EndpointKind::Group;
        }) || std::ranges::any_of(*to, [](const Endpoint& endpoint) {
            return endpoint.kind == EndpointKind::Group;
        });
        // GroupEndpoint15 is a closed scalar spelling: its existing source
        // links are unlabelled, solid, one-way arrows with rank length one.
        // Other group-adjacent forms remain whole-source unsupported instead
        // of borrowing broader vertex link syntax.
        if (carries_group && (from->size() != 1u || to->size() != 1u ||
                              edge_label || link.stroke != Stroke::Solid || link.arrow_start ||
                              !link.arrow_end || link.minimum_length != 1)) return false;
        graph.has_group_endpoint = graph.has_group_endpoint || carries_group;
        for (const auto a : *from) for (const auto b : *to)
            graph.edges.push_back({a, b, edge_label, link.stroke, link.arrow_start, link.arrow_end, link.minimum_length});
        emitted_link = true;
        from = std::move(to);
        scanner.space();
    }
    return !uses_group_endpoint || emitted_link;
}

std::optional<Diagram> parse(std::string_view source, Work& work, bool already_prepaid = false) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    // RFC 0031 E5: the bare whole-body profile is charged as soon as a valid
    // graph/flowchart header is recognized (the configured/envelope path has
    // already paid the full profile). Charging follows the allocation-free
    // header peek rather than function entry, so non-Flow sources that share
    // this first-attempt parser pay nothing before classification, exactly as
    // the former fixed entry charge behaved.
    bool prepaid = already_prepaid;
    const auto original = source;
    bool saw_reopen = false, cross_owner_membership_request = false;
    for (const auto byte : source) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x20 && value != '\n' && value != '\r' && value != '\t') return {};
    }
    Diagram diagram;
    bool have_header = false, default_direction_header = false;
    bool have_statement = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        line = trim(line);
        if (line.starts_with("%%{")) return {};
        if (line.empty() || line.starts_with("%%")) continue;
        bool quoted = false;
        bool escaped = false;
        char close = 0;
        std::size_t start = 0;
        for (std::size_t at = 0; at <= line.size(); ++at) {
            const auto ch = at == line.size() ? '\0' : line[at];
            if (at == line.size() && (quoted || close || escaped)) return {};
            if (escaped) { escaped = false; continue; }
            if (quoted && ch == '\\') { escaped = true; continue; }
            if (ch == '"') { quoted = !quoted; continue; }
            if (!quoted && have_header) {
                if (close && ch == close) close = 0;
                else if (!close && (ch == '[' || ch == '(' || ch == '{' || ch == '|' ||
                                     (ch == '>' && (at == 0u || (line[at - 1u] != '-' && line[at - 1u] != '=')))))
                    close = ch == '[' || ch == '>' ? ']' : ch == '(' ? ')' : ch == '{' ? '}' : '|';
            }
            if (ch != '\0' && (ch != ';' || close || quoted)) continue;
            if (quoted || close || escaped) return {};
            const auto statement = trim(line.substr(start, at - start));
            start = at + 1;
            if (statement.empty()) continue;
            if (!have_header) {
                Scanner header{statement};
                auto kind = header.id();
                if (!kind || (*kind != "graph" && *kind != "flowchart")) return {};
                if (header.rest.empty()) {
                    diagram.direction = Direction::Down;
                    default_direction_header = true;
                }
                else {
                    if (header.rest.front() != ' ' && header.rest.front() != '\t') return {};
                    header.space();
                    if (header.rest == "TD" || header.rest == "TB") diagram.direction = Direction::Down;
                    else if (header.rest == "BT") diagram.direction = Direction::Up;
                    else if (header.rest == "LR") diagram.direction = Direction::Right;
                    else if (header.rest == "RL") diagram.direction = Direction::Left;
                    else return {};
                }
                have_header = true;
                if (!prepaid) {
                    if (!flow_parse_prepay(original, work, false)) return {};
                    prepaid = true;
                }
            } else {
                if (!flow_statement(statement, diagram, work, original, already_prepaid, saw_reopen,
                                    cross_owner_membership_request)) return {};
                have_statement = true;
            }
        }
    }
    // Mermaid defaults an omitted direction to TB. Keep that newly admitted
    // header profile closed over group-free diagrams until the group layout
    // can guarantee whole-scene output for its nested-container cases.
    if (!have_header || !have_statement || !diagram.group_stack.empty() ||
        (default_direction_header && !diagram.groups.empty()) ||
        (diagram.vertices.empty() && diagram.groups.empty())) return {};
    if (saw_reopen) {
        // RFC 0031 E5: the bounded reopen validation walks are covered by the
        // full profile (activated on first reopen) / bare linear term.
        if (cross_owner_membership_request) return {};
        for (const auto& group : diagram.groups) {
            if (group.direction.has_value()) return {};
        }
    }
    return diagram;
}


// Returns the total coarse parse bound for the selected profile.
std::size_t flow_parse_prepay_total(std::string_view body, bool full_profile) {
    if (body.empty() || body.size() > kSourceLimit) return std::numeric_limits<std::size_t>::max();
    std::size_t legacy_identifiers = 0, numeric_identifiers = 0, group_words = 0;
    for (std::size_t at = 0; at < body.size();) {
        if (!id_start(body[at])) { ++at; continue; }
        const auto first = at++;
        while (at < body.size() && id_char(body[at])) ++at;
        ++legacy_identifiers;
        if (body.substr(first, at - first) == "subgraph") ++group_words;
    }
    for (std::size_t at = 0; at < body.size();) {
        if (!flow_id_start(body[at])) { ++at; continue; }
        const bool digit_start = !id_start(body[at]);
        ++at;
        while (at < body.size() && flow_id_continue(body, at)) ++at;
        if (digit_start) ++numeric_identifiers;
    }
    const auto identifiers = legacy_identifiers + numeric_identifiers;
    const auto add = [](std::size_t total, std::size_t count, std::size_t unit) {
        if (!unit || count > (std::numeric_limits<std::size_t>::max() - total) / unit)
            return std::numeric_limits<std::size_t>::max();
        return total + count * unit;
    };
    std::size_t total = 16u * (sizeof(Diagram) + sizeof(std::optional<Diagram>)) +
                        16u * sizeof(std::size_t) + 256u;
    total = add(total, body.size(), 4u);
    if (!full_profile) {
        constexpr std::size_t kIdentifierUnit =
            8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
            (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u);
        constexpr std::size_t kBarePerByte = 60u;
        constexpr std::size_t kBareLinearUnit = 34u;
        constexpr std::size_t kBareLinearVertices = 64u;
        constexpr std::size_t kBareLinearEdges = 128u;
        constexpr std::size_t kBareFixed = 4096u;
        total = add(total, body.size(), kBarePerByte);
        total = add(total, identifiers + 1u, kIdentifierUnit);
        total = add(total, kBareLinearVertices * kBareLinearEdges, kBareLinearUnit);
        if (total > std::numeric_limits<std::size_t>::max() - kBareFixed)
            return std::numeric_limits<std::size_t>::max();
        return total + kBareFixed;
    }
    const auto vertices = std::min<std::size_t>(64u, identifiers);
    const auto groups = std::min(kGroupLimit, group_words);
    const auto edges = identifiers >= 12u ? 128u : std::min<std::size_t>(128u, identifiers * identifiers);
    const auto depth = std::min(kGroupDepthLimit, groups);
    const auto bytes = body.size() + 1u;
    total = add(total, bytes, 4u * sizeof(std::string_view) + 4u * sizeof(std::size_t) + 32u);
    total = add(total, bytes, 32u * sizeof(std::string) + 512u);
    total = add(total, vertices, 8u * sizeof(Vertex));
    total = add(total, groups, 8u * sizeof(Group));
    total = add(total, edges, 8u * sizeof(Edge));
    total = add(total, depth, 8u * sizeof(std::size_t));
    total = add(total, identifiers + 1u, 8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
           (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u));
    total = add(total, edges, 8u * (body.size() + 1u));
    return total;
}

// RFC 0031 E5: whole-body coarse parse prepayment with two deliberately
// distinct profiles. upgrade_to_full is set only when a bare parse activates
// the full profile on its first group reopen: it charges the delta so the
// source pays once at the full-profile total, never both profiles.
bool flow_parse_prepay(std::string_view body, Work& work, bool full_profile,
                       bool upgrade_to_full) {
    if (body.empty() || body.size() > kSourceLimit) return false;
    if (upgrade_to_full) {
        const auto bare = flow_parse_prepay_total(body, false);
        const auto full = flow_parse_prepay_total(body, true);
        if (full < bare) {
            // The full profile's analytic total is below what the bare profile
            // already charged; the bare fee dominates.
            return work.spend(0);
        }
        const auto delta = full - bare;
        if (delta > work.remaining) {
            static_cast<void>(work.spend(std::numeric_limits<std::size_t>::max()));
            return false;
        }
        return work.spend(delta);
    }
    const auto charge = [&](std::size_t count, std::size_t unit) {
        if (unit && count > work.remaining / unit) {
            static_cast<void>(work.spend(std::numeric_limits<std::size_t>::max()));
            return false;
        }
        return work.spend(count * unit);
    };
    if (!work.spend(16u * (sizeof(Diagram) + sizeof(std::optional<Diagram>)) +
                    16u * sizeof(std::size_t) + 256u) || !charge(body.size(), 4u)) return false;
    // Overcounts words in quotes/labels deliberately: these are bounds on
    // existing calls/allocations, not a second grammar or an admission decision.
    // Preserve the legacy word split exactly: it is the prior conservative
    // prepayment bound, rather than a grammar decision. The second pass only
    // adds digit-start Flow IDs, so dots and hyphens never lower that bound.
    std::size_t legacy_identifiers = 0, numeric_identifiers = 0, group_words = 0;
    for (std::size_t at = 0; at < body.size();) {
        if (!id_start(body[at])) { ++at; continue; }
        const auto first = at++;
        while (at < body.size() && id_char(body[at])) ++at;
        ++legacy_identifiers;
        if (body.substr(first, at - first) == "subgraph") ++group_words;
    }
    for (std::size_t at = 0; at < body.size();) {
        if (!flow_id_start(body[at])) { ++at; continue; }
        const bool digit_start = !id_start(body[at]);
        ++at;
        while (at < body.size() && flow_id_continue(body, at)) ++at;
        if (digit_start) ++numeric_identifiers;
    }
    const auto identifiers = legacy_identifiers + numeric_identifiers;
    if (!full_profile) {
        // Bare profile: 60/byte dominates the deleted statement fees
        // (4/byte plus label scans at <=56/byte); the per-identifier unit is
        // the same vertex/index-table bound as the full profile below;
        // 64*128*34 dominates the linear vertex/group/edge scans (endpoint
        // group comparisons, reopen validation); a fixed term covers the
        // per-statement constants under the declaration caps.
        constexpr std::size_t kBarePerByte = 60u;
        constexpr std::size_t kBareLinearUnit = 34u;
        constexpr std::size_t kBareLinearVertices = 64u;
        constexpr std::size_t kBareLinearEdges = 128u;
        constexpr std::size_t kBareFixed = 4096u;
        constexpr std::size_t kIdentifierUnit =
            8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
            (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u);
        if (body.size() > work.remaining / kBarePerByte) return false;
        if (!work.spend(kBarePerByte * body.size()) ||
            !charge(identifiers + 1u, kIdentifierUnit) ||
            !charge(kBareLinearVertices * kBareLinearEdges, kBareLinearUnit) ||
            !work.spend(kBareFixed)) return false;
        return true;
    }
    const auto vertices = std::min<std::size_t>(64u, identifiers);
    const auto groups = std::min(kGroupLimit, group_words);
    const auto edges = identifiers >= 12u ? 128u : std::min<std::size_t>(128u, identifiers * identifiers);
    const auto depth = std::min(kGroupDepthLimit, groups);
    const auto bytes = body.size() + 1u;
    // New Flow lexer/group-header views and generated-ID materialization are
    // paid before configured/reopened parsing can allocate or scan them.
    if (!charge(bytes, 4u * sizeof(std::string_view) + 4u * sizeof(std::size_t) + 32u)) return false;
    // Text input scans, Unicode/glyph tables, temporary strings, glyph vector
    // storage/copies, round-trip and decoded growth. Fan-out payloads follow.
    if (!charge(bytes, 32u * sizeof(std::string) + 512u) ||
        !charge(vertices, 8u * sizeof(Vertex)) ||
        !charge(groups, 8u * sizeof(Group)) ||
        !charge(edges, 8u * sizeof(Edge)) ||
        !charge(depth, 8u * sizeof(std::size_t)) ||
        !charge(identifiers + 1u, 8u * sizeof(std::vector<std::size_t>) + 8u * sizeof(std::size_t) + 64u +
               (64u + kGroupLimit + 8u) * (2u * sizeof(std::size_t) + 8u)) ||
        !charge(edges, 8u * (body.size() + 1u))) return false;
    return true;
}

bool flow_envelope_parse_prepay(std::string_view body, Work& work) {
    return flow_parse_prepay(body, work, true);
}

struct FlowEnvelope {
    std::string_view body;
    std::optional<std::string> title;
};

std::optional<FlowEnvelope> flow_envelope_body(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    // The whole-envelope profile accepts CRLF, never a hidden bare CR.
    for (std::size_t n = 0; n < source.size(); ++n)
        if (source[n] == '\r' && (n + 1u == source.size() || source[n + 1u] != '\n')) return {};
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter) return {};
    if (envelope->raw_frontmatter.find('\t') != std::string_view::npos) return {};
    enum ConfigBit : unsigned { Layout = 1u, Font = 2u, RootHtml = 4u, Flowchart = 8u,
                                NestedHtml = 16u, Security = 32u, Log = 64u, MaxWidth = 128u,
                                TitleTopMargin = 256u, DiagramPadding = 512u };
    bool description = false, description_text = false, config = false, title = false;
    bool root_html_value = false, nested_html_value = false, log_one = false;
    bool title_top_margin_zero = false, diagram_padding_zero = false;
    std::optional<bool> use_max_width;
    unsigned fields = 0;
    const auto mark = [&](unsigned bit) { if (fields & bit) return false; fields |= bit; return true; };
    const auto boolean = [&](std::string_view scalar, unsigned bit, bool& value) {
        if (!mark(bit)) return false;
        if (scalar == "true") { value = true; return true; }
        if (scalar == "false") { value = false; return true; }
        return false;
    };
    const auto description_block = [&](const detail::EnvelopeScalar& scalar) -> bool {
        if (!((scalar.form == detail::EnvelopeScalarForm::Plain && scalar.header.empty()) ||
              (scalar.form == detail::EnvelopeScalarForm::Folded && scalar.header == ">-"))) return false;
        std::size_t at = 0;
        while (at < scalar.text.size()) {
            const auto newline = scalar.text.find('\n', at);
            const auto line = scalar.text.substr(at, newline == std::string_view::npos
                ? std::string_view::npos : newline - at);
            at = newline == std::string_view::npos ? scalar.text.size() : newline + 1u;
            if (trim(line).empty()) continue;
            if (!line.starts_with("  ") || line.size() == 2u || line[2] == ' ') return false;
            const auto payload = line.substr(2);
            if (!unicode_text(payload)) return false;
            if (scalar.form == detail::EnvelopeScalarForm::Plain && !description_text) {
                const auto first = static_cast<unsigned char>(payload.front());
                constexpr std::array<unsigned char, 16> forbidden{{44,91,93,123,125,35,38,42,33,124,62,39,34,37,64,96}};
                if (std::ranges::find(forbidden, first) != forbidden.end()) return false;
                if ((first == '?' || first == '-') && (payload.size() == 1u || payload[1] == ' ')) return false;
            }
            if (scalar.form == detail::EnvelopeScalarForm::Plain) {
                for (std::size_t pos = 0; pos < payload.size(); ++pos) {
                    if (payload[pos] == ':' && (pos + 1u == payload.size() || payload[pos + 1u] == ' ')) return false;
                    if (payload[pos] == '#' && (pos == 0u || payload[pos - 1u] == ' ')) return false;
                }
            }
            description_text = true;
        }
        return true;
    };
    for (const auto& event : envelope->events) {
        if (event.blank || event.comment) continue;
        if (event.top_level) {
            if (event.path == "description") {
                if (description || (config && fields != Layout)) return {};
                if (!envelope->description || !description_block(*envelope->description)) return {};
                description = true;
                continue;
            }
            if (event.path == "title") {
                if (title || !envelope->title ||
                    envelope->title->form != detail::EnvelopeScalarForm::Plain ||
                    envelope->title->text.empty() || envelope->title->text.size() > 256u ||
                    envelope->title->text.find_first_of("\r\n") != std::string::npos ||
                    !unicode_text(envelope->title->text)) return {};
                title = true;
                continue;
            }
            if (event.path == "config") {
                if (config || (description && !description_text)) return {};
                config = true;
                continue;
            }
            return {};
        }
        if (event.namespace_entry) {
            if (event.path != "flowchart" || event.indent != 2) return {};
            if (!mark(Flowchart)) return {};
            continue;
        }
        if (!config) return {};
        if (event.indent == 2) {
            if (event.path.find('.') != std::string::npos) return {};
            if (event.path == "layout") {
                const auto scalar = trim(event.value);
                if (!mark(Layout) || (scalar != "dagre" && scalar != "\"dagre\"")) return {};
            } else if (event.path == "fontFamily") {
                const auto scalar = trim(event.value);
                if (!mark(Font) || (scalar != "courier" && scalar != "\"courier\"")) return {};
            } else if (event.path == "securityLevel") {
                const auto scalar = trim(event.value);
                if (!mark(Security) || (scalar != "loose" && scalar != "\"loose\"")) return {};
            } else if (event.path == "logLevel") {
                const auto scalar = trim(event.value);
                if (!mark(Log) || scalar != "1") return {};
                log_one = true;
            } else if (event.path == "htmlLabels") {
                if (!boolean(trim(event.value), RootHtml, root_html_value)) return {};
            } else {
                return {};
            }
            continue;
        }
        if (event.indent == 4) {
            if (!(fields & Flowchart)) return {};
            if (event.path == "flowchart.htmlLabels") {
                if (!boolean(trim(event.value), NestedHtml, nested_html_value)) return {};
            } else if (event.path == "flowchart.useMaxWidth") {
                // This option controls responsive SVG sizing. Terminal rows
                // use the same intrinsic cell geometry at either value.
                if (!mark(MaxWidth)) return {};
                const auto value = trim(event.value);
                if (value == "true") use_max_width = true;
                else if (value == "false") use_max_width = false;
                else return {};
            } else if (event.path == "flowchart.titleTopMargin") {
                // A zero margin has no terminal effect when there is no
                // diagram title, but it must not block the rest of the graph.
                // Non-zero values remain unsupported because they affect SVG
                // title placement and have no terminal equivalent.
                if (!mark(TitleTopMargin) || trim(event.value) != "0") return {};
                title_top_margin_zero = true;
            } else if (event.path == "flowchart.diagramPadding") {
                // SVG viewport padding has no terminal equivalent. Zero is
                // the exact inert profile and matches the terminal canvas.
                if (!mark(DiagramPadding) || trim(event.value) != "0") return {};
                diagram_padding_zero = true;
            } else {
                return {};
            }
            continue;
        }
        return {};
    }
    if ((!description && !config) || (description && !description_text)) return {};
    if (!config) {
        if (title) return {};
        return FlowEnvelope{envelope->raw_body, {}};
    }
    const bool legacy_dagre = fields == Layout && !title;
    const bool dagre_diagram_title = fields == Layout && title;
    const bool terminal_max_width = fields == (Flowchart | MaxWidth) && use_max_width.has_value();
    const bool terminal_zero_diagram_padding =
        (fields == (Flowchart | DiagramPadding) ||
         fields == (Layout | Flowchart | DiagramPadding)) && diagram_padding_zero;
    const bool title_top_margin_zero_without_layout =
        fields == (Flowchart | TitleTopMargin) && title_top_margin_zero && !title;
    const bool dagre_title_top_margin_zero =
        fields == (Layout | Flowchart | TitleTopMargin) && title_top_margin_zero && !title;
    const bool courier_only = fields == Font;
    const bool courier_nested_false = fields == (Font | Flowchart | NestedHtml) && !nested_html_value;
    const bool logging_false = fields == (Log | RootHtml) && log_one && !root_html_value;
    const bool both_false = fields == (RootHtml | Flowchart | NestedHtml) &&
        !root_html_value && !nested_html_value;
    const unsigned strict_bits = RootHtml | Flowchart | NestedHtml | Security;
    const bool strict = (fields == strict_bits || fields == (strict_bits | Layout)) &&
        root_html_value && nested_html_value;
    if (legacy_dagre || dagre_diagram_title || terminal_max_width || terminal_zero_diagram_padding ||
        title_top_margin_zero_without_layout || dagre_title_top_margin_zero ||
        (description && (courier_only || courier_nested_false || logging_false || both_false || strict)))
    {
        FlowEnvelope result{envelope->raw_body, {}};
        if (title) {
            const auto& value = envelope->title->text;
            if (!work.spend(value.size() * 8u + sizeof(std::string) + 128u)) return {};
            result.title.emplace(value);
        }
        return result;
    }
    return {};
}

bool prepend_flow_title(std::vector<std::string>& lines, std::string_view title, Work& work) {
    if (title.empty() || title.size() > 256u || title.find_first_of("\r\n") != std::string_view::npos ||
        !unicode_text(title) || lines.empty()) return false;
    const int title_width = ftxui::string_width(std::string(title));
    if (title_width <= 0 || title_width > kWidthLimit) return false;
    int graph_width = 0;
    std::size_t bytes = title.size();
    for (const auto& line : lines) {
        const int width = ftxui::string_width(line);
        if (width < 0 || width > kWidthLimit) return false;
        graph_width = std::max(graph_width, width);
        if (line.size() > std::numeric_limits<std::size_t>::max() - bytes) return false;
        bytes += line.size();
    }
    const int width = std::max(graph_width, title_width);
    if (width > kWidthLimit || lines.size() > static_cast<std::size_t>(kHeightLimit - 2)) return false;
    if (bytes > (std::numeric_limits<std::size_t>::max() - 4096u) / 8u ||
        !work.spend(bytes * 8u + (lines.size() + 2u) * (sizeof(std::string) + 64u) + 4096u)) return false;
    std::vector<std::string> titled;
    titled.reserve(lines.size() + 2u);
    const int title_left = (width - title_width) / 2;
    titled.emplace_back(static_cast<std::size_t>(title_left), ' ');
    titled.back().append(title);
    titled.back().append(static_cast<std::size_t>(width - title_left - title_width), ' ');
    titled.emplace_back(static_cast<std::size_t>(width), ' ');
    for (auto& line : lines) {
        const int line_width = ftxui::string_width(line);
        const int left = (width - line_width) / 2;
        std::string centered(static_cast<std::size_t>(left), ' ');
        centered.append(line);
        centered.append(static_cast<std::size_t>(width - left - line_width), ' ');
        titled.push_back(std::move(centered));
    }
    lines = std::move(titled);
    return true;
}

struct Box { int x; int y; int width; int height = 5; };
std::optional<Box> node_size(const Vertex& node,Work& work) {
    // RFC 0031 E5: the classic/decision label-row measurement is linear in
    // validated <=kLabelLimit labels; its bookkeeping is folded into the parse
    // prepay (bare profile) and the collector literal_bytes term (full
    // profile). The parameter is retained for the shared call chain.
    (void)work;
    const int rows=static_cast<int>(label_rows(node.text).size());
    if (classic_shape(node.shape)) {
        if (rows != 1) return {};
        if (node.shape == Shape::ForkJoin) {
            const int width = std::max(classic_min_width(node.shape), label_width(node.text) + 8);
            if (width > kWidthLimit) return {};
            return Box{0, 0, width, classic_height(node.shape)};
        }
        if (node.shape == Shape::SmallCircle) {
            int diameter = std::max(classic_min_width(node.shape), label_width(node.text) + 4);
            if ((diameter % 2) == 0) ++diameter;
            if (diameter > kWidthLimit || diameter > kHeightLimit) return {};
            return Box{0, 0, diameter, diameter};
        }
        if (node.shape == Shape::Triangle) {
            int height = std::max(5, label_width(node.text) + 2);
            if ((height % 2) == 0) ++height;
            const int width = 2 * height - 1;
            if (width > kWidthLimit || height > kHeightLimit) return {};
            return Box{0, 0, width, height};
        }
        const int padding = node.shape == Shape::Subroutine || node.shape == Shape::DoubleCircle ? 8 : 4;
        int width = std::max(classic_min_width(node.shape), label_width(node.text) + padding);
        if (classic_requires_odd_width(node.shape) && (width % 2) == 0) ++width;
        const int height = classic_height(node.shape);
        if (width > kWidthLimit || height > kHeightLimit) return {};
        return Box{0, 0, width, height};
    }
    int height=std::max(5,rows+2);
    if(node.shape!=Shape::Decision)return Box{0,0,std::max(9,label_width(node.text)+4),height};
    if(height%2==0)++height;
    const int half=height/2;
    const int width=std::max(2*half+7,label_width(node.text)+2*half);
    if(width>kWidthLimit||height>kHeightLimit)return {};
    return Box{0,0,width,height};
}

bool classic_ring(Canvas& canvas, int left, int top, int right, int bottom) {
    if (right - left + 1 < 9 || bottom - top + 1 < 9 || ((right - left + 1) % 2) == 0 ||
        ((bottom - top + 1) % 2) == 0) return false;
    const auto put = [&](int x, int y, std::string_view glyph) { return canvas.put({x, y}, glyph); };
    if (!put(left + 2, top, "╭") || !put(right - 2, top, "╮") ||
        !put(left + 1, top + 1, "╭") || !put(right - 1, top + 1, "╮") ||
        !put(left + 1, bottom - 1, "╰") || !put(right - 1, bottom - 1, "╯") ||
        !put(left + 2, bottom, "╰") || !put(right - 2, bottom, "╯")) return false;
    for (int x = left + 3; x < right - 2; ++x)
        if (!put(x, top, "─") || !put(x, bottom, "─")) return false;
    for (int x = left + 2; x < right - 1; ++x)
        if (!put(x, top + 1, "─") || !put(x, bottom - 1, "─")) return false;
    for (int y = top + 2; y <= bottom - 2; ++y)
        if (!put(left, y, "│") || !put(right, y, "│")) return false;
    return true;
}

bool classic_box(Canvas& canvas, const Box& box, Shape shape) {
    const int l = box.x, r = box.x + box.width - 1, t = box.y, b = box.y + box.height - 1;
    const auto put = [&](int x, int y, std::string_view glyph) { return canvas.put({x, y}, glyph); };
    const auto sharp = [&]() {
        for (int x = l + 1; x < r; ++x) if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y) if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return put(l, t, "┌") && put(r, t, "┐") && put(l, b, "└") && put(r, b, "┘");
    };
    switch (shape) {
    case Shape::ForkJoin:
        for (int y=t;y<=b;++y) for (int x=l;x<=r;++x)
            if (!put(x,y,"█")) return false;
        return put(l,t,"▛") && put(r,t,"▜") && put(l,b,"▙") && put(r,b,"▟");
    case Shape::Triangle: {
        if (box.height < 5 || (box.height % 2) == 0 || box.width != 2 * box.height - 1) return false;
        const int center = box.x + box.width / 2;
        if (!put(center, t, "△")) return false;
        for (int y = 1; y < box.height - 1; ++y) {
            const int inset = box.height - 1 - y;
            if (!put(l + inset, t + y, "╱") || !put(r - inset, t + y, "╲")) return false;
        }
        if (!put(l, b, "╱") || !put(r, b, "╲")) return false;
        for (int x = l + 1; x < r; ++x) if (!put(x, b, "─")) return false;
        return true;
    }
    case Shape::Delay:
        if (!put(l, t, "┌") || !put(r - 2, t, "─") ||
            !put(l, t + 1, "│") || !put(r - 1, t + 1, "╮") ||
            !put(l, t + 2, "│") || !put(r, t + 2, ")") ||
            !put(l, t + 3, "│") || !put(r - 1, t + 3, "╯") ||
            !put(l, b, "└") || !put(r - 2, b, "─")) return false;
        for (int x = l + 1; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    case Shape::Stadium:
        if (!put(l + 1, t, "╭") || !put(r - 1, t, "╮") || !put(l + 1, b, "╰") || !put(r - 1, b, "╯")) return false;
        for (int x = l + 2; x < r - 1; ++x) if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y) if (!put(l, y, "(") || !put(r, y, ")")) return false;
        return true;
    case Shape::Circle: case Shape::SmallCircle: return classic_ring(canvas, l, t, r, b);
    case Shape::DoubleCircle:
        return classic_ring(canvas, l, t, r, b) && classic_ring(canvas, l + 2, t + 2, r - 2, b - 2);
    case Shape::Subroutine:
        if (!sharp()) return false;
        for (int y = t + 1; y < b; ++y) if (!put(l + 2, y, "║") || !put(r - 2, y, "║")) return false;
        return true;
    case Shape::Hexagon:
        if (!put(l + 2, t, "╱") || !put(r - 2, t, "╲") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╲") || !put(l, t + 2, "│") || !put(r, t + 2, "│") ||
            !put(l + 1, b - 1, "╲") || !put(r - 1, b - 1, "╱") ||
            !put(l + 2, b, "╲") || !put(r - 2, b, "╱")) return false;
        for (int x = l + 3; x < r - 2; ++x) if (!put(x, t, "─") || !put(x, b, "─")) return false;
        return true;
    case Shape::Cylinder:
        if (!put(l + 2, t, "╭") || !put(r - 2, t, "╮") || !put(l + 1, t + 1, "╭") ||
            !put(r - 1, t + 1, "╮") || !put(l + 1, b - 1, "╰") || !put(r - 1, b - 1, "╯") ||
            !put(l + 2, b, "╰") || !put(r - 2, b, "╯")) return false;
        for (int x = l + 3; x < r - 2; ++x)
            if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int x = l + 2; x < r - 1; ++x)
            if (!put(x, t + 1, "─") || !put(x, b - 1, "─")) return false;
        for (int y = t + 2; y <= b - 2; ++y) if (!put(l, y, "│") || !put(r, y, "│")) return false;
        return true;
    case Shape::Asymmetric:
        if (!put(l + 2, t, "╱") || !put(r, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(l, t + 2, "◁") || !put(l + 1, b - 1, "╲") || !put(l + 2, b, "╲") ||
            !put(r, b, "┘")) return false;
        for (int x = l + 3; x < r; ++x) if (!put(x, t, "─") || !put(x, b, "─")) return false;
        for (int y = t + 1; y < b; ++y) if (!put(r, y, "│")) return false;
        return true;
    case Shape::Trapezoid:
        if (!put(l + 2, t, "┌") || !put(r - 2, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╲") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l, b - 1, "│") || !put(r, b - 1, "│") || !put(l, b, "└") || !put(r, b, "┘")) return false;
        for (int x = l + 3; x < r - 2; ++x) if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r; ++x) if (!put(x, b, "─")) return false;
        return true;
    case Shape::TrapezoidAlt:
        if (!put(l, t, "┌") || !put(r, t, "┐") || !put(l, t + 1, "│") || !put(r, t + 1, "│") ||
            !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") || !put(l + 1, b - 1, "│") ||
            !put(r - 1, b - 1, "│") || !put(l + 2, b, "└") || !put(r - 2, b, "┘")) return false;
        for (int x = l + 1; x < r; ++x) if (!put(x, t, "─")) return false;
        for (int x = l + 3; x < r - 2; ++x) if (!put(x, b, "─")) return false;
        return true;
    case Shape::Parallelogram:
        if (!put(l + 2, t, "┌") || !put(r, t, "┐") || !put(l + 1, t + 1, "╱") ||
            !put(r - 1, t + 1, "╱") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l, b - 1, "│") || !put(r - 2, b - 1, "│") || !put(l, b, "└") || !put(r - 2, b, "┘")) return false;
        for (int x = l + 3; x < r; ++x) if (!put(x, t, "─")) return false;
        for (int x = l + 1; x < r - 2; ++x) if (!put(x, b, "─")) return false;
        return true;
    case Shape::ParallelogramAlt:
        if (!put(l, t, "┌") || !put(r - 2, t, "┐") || !put(l, t + 1, "╲") ||
            !put(r - 2, t + 1, "╲") || !put(l + 1, t + 2, "│") || !put(r - 1, t + 2, "│") ||
            !put(l + 1, b - 1, "│") || !put(r - 1, b - 1, "│") || !put(l + 2, b, "└") || !put(r, b, "┘")) return false;
        for (int x = l + 1; x < r - 2; ++x) if (!put(x, t, "─")) return false;
        for (int x = l + 3; x < r; ++x) if (!put(x, b, "─")) return false;
        return true;
    case Shape::Rectangle: case Shape::Rounded: case Shape::Decision: return false;
    }
    return false;
}

bool classic_port_corridors(Canvas& canvas, const Box& box, Shape shape) {
    const bool shifted_side = shape == Shape::Trapezoid || shape == Shape::TrapezoidAlt ||
        shape == Shape::Parallelogram || shape == Shape::ParallelogramAlt;
    if (!shifted_side) return true;
    const int middle_y = box.y + box.height / 2;
    const Point left{box.x, middle_y}, right{box.x + box.width - 1, middle_y};
    // These are the two blank envelope cells immediately outside the actual
    // l+1/r-1 side terminals. Keep the contour and interior blocked.
    if (canvas.at(left).glyph != " " || canvas.at(right).glyph != " ") return false;
    return canvas.put(left, " ", false) && canvas.put(right, " ", false);
}

// The explicit ASCII Flow profile needs the actual native canvas rows before
// FTXUI adds Screen padding.  It records only literal text cells; geometry is
// projected after the same router has completed the ordinary canvas.
struct FlowTextCollector {
    explicit FlowTextCollector(Work& work) : work_(work) {}

    bool prepare(int width, int height, std::size_t literal_cell_bound, std::size_t literal_bytes) {
        if (prepared_ || width <= 0 || height <= 0) return false;
        const auto area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        if (area / static_cast<std::size_t>(width) != static_cast<std::size_t>(height)) return false;
        // The collector is the only new owner of this index and retained literal
        // copies. Row output is charged later from the real cropped geometry and
        // literal byte totals, before either output vector is allocated.
        // RFC 0031 E5: the classic/decision node_size label measurement
        // (former 352/literal-byte fee) is folded into this term.
        constexpr std::size_t kClassicMeasurePerByte =
            8u * sizeof(std::string_view) + 4u * sizeof(std::string) + 128u;
        constexpr std::size_t kClassicMeasureFixed = 64u * 512u;
        if (!charge({area * sizeof(int),
                     literal_cell_bound * sizeof(std::string),
                     literal_bytes * (1u + kClassicMeasurePerByte) + kClassicMeasureFixed,
                     2u * area + 4096u})) return false;
        owners_.assign(area, 0);
        glyphs_.reserve(literal_cell_bound);
        width_ = width;
        height_ = height;
        prepared_ = true;
        return true;
    }

    // Re-arm after a discarded strict routing pass so the single bounded
    // relaxed retry repaints onto a fresh canvas. Same dimensions and bounds
    // as prepare(), charged anew; the strict pass's partial owner/glyph
    // records are discarded and never reach finish().
    bool restart(int width, int height, std::size_t literal_cell_bound, std::size_t literal_bytes) {
        prepared_ = false;
        owners_.clear();
        glyphs_.clear();
        ascii_rows_.reset();
        return prepare(width, height, literal_cell_bound, literal_bytes);
    }

    std::optional<int> row_count(std::string_view value) {
        if (!prepared_ || !charge({value.size() * 2u + 32u})) return {};
        int count = 1;
        for (const char value_byte : value) if (value_byte == '\n') ++count;
        return count;
    }

    bool multiline(Canvas& canvas, Point point, std::string_view value, int width) {
        if (!prepared_ || canvas.width != width_ || canvas.height != height_) return false;
        // Do not call label_rows here. Its vector allocation would otherwise
        // precede the collector payment. This single pass has the same LF row
        // boundaries and keeps the existing null-collector path untouched.
        for (std::size_t begin = 0u;;) {
            const auto newline = value.find('\n', begin);
            const auto end = newline == std::string_view::npos ? value.size() : newline;
            const auto row = value.substr(begin, end - begin);
            // FTXUI's Utf8ToGlyphs first copies `row` into a std::string, then
            // reserves one string slot per input byte. Its temporary `append`
            // strings and output payloads together cannot exceed two row-byte
            // copies. The retained glyph copy was reserved in prepare().
            if (!charge({sizeof(std::string) + row.size(), sizeof(std::vector<std::string>),
                         row.size() * sizeof(std::string), row.size() * 2u,
                         row.size() * 4u + 32u})) return false;
            const std::string input(row);
            const auto cells = ftxui::Utf8ToGlyphs(input);
            // Utf8ToGlyphs includes exactly one empty cell for a full-width
            // glyph, so its cell count is the actual centered terminal width.
            Point cell{point.x + (width - static_cast<int>(cells.size())) / 2, point.y};
            for (const auto& glyph : cells) {
                if (!canvas.inside(cell)) return false;
                const auto index = static_cast<std::size_t>(canvas.index(cell));
                if (owners_[index] != 0 || !canvas.put(cell, glyph)) return false;
                glyphs_.emplace_back(glyph);
                owners_[index] = static_cast<int>(glyphs_.size());
                ++cell.x;
            }
            ++point.y;
            if (newline == std::string_view::npos) return true;
            begin = newline + 1u;
        }
    }

    std::optional<std::vector<std::string>> finish(const Canvas& canvas) {
        if (!prepared_ || canvas.width != width_ || canvas.height != height_) return {};
        const auto area = static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
        // Pay the full native-canvas scan and its per-row literal accounting
        // before allocating the accounting vector or inspecting a canvas cell.
        if (!charge({area * 4u, static_cast<std::size_t>(height_) * sizeof(std::size_t) + 256u})) return {};
        std::vector<std::size_t> literal_bytes(static_cast<std::size_t>(height_), 0u);
        int x_min = width_, y_min = height_, x_max = -1, y_max = -1;
        for (int y = 0; y < height_; ++y) for (int x = 0; x < width_; ++x) {
            const auto index = static_cast<std::size_t>(canvas.index({x, y}));
            const auto& glyph = canvas.at({x, y}).glyph;
            const auto owner = owners_[index];
            if (owner != 0) {
                // A later route or frame write must not be hidden by restoring
                // a stale literal glyph into the ASCII projection.
                if (owner < 1) return {};
                const auto offset = static_cast<std::size_t>(owner - 1);
                if (offset >= glyphs_.size() || glyph != glyphs_[offset]) return {};
                if (glyph.size() > std::numeric_limits<std::size_t>::max() - literal_bytes[static_cast<std::size_t>(y)]) return {};
                literal_bytes[static_cast<std::size_t>(y)] += glyph.size();
            }
            if (glyph != " ") {
                x_min = std::min(x_min, x); x_max = std::max(x_max, x);
                y_min = std::min(y_min, y); y_max = std::max(y_max, y);
            }
        }
        if (x_max < x_min || y_max < y_min) return {};
        const auto rows = static_cast<std::size_t>(y_max - y_min + 1);
        const auto columns = static_cast<std::size_t>(x_max - x_min + 1);
        constexpr std::size_t kNativeGeometryBytes = 3u;
        std::size_t output_capacity = 0u;
        for (int y = y_min; y <= y_max; ++y) {
            const auto literal = literal_bytes[static_cast<std::size_t>(y)];
            if (columns > (std::numeric_limits<std::size_t>::max() - literal) / (kNativeGeometryBytes + 1u)) return {};
            const auto row_capacity = literal * 2u + columns * (kNativeGeometryBytes + 1u);
            if (row_capacity > std::numeric_limits<std::size_t>::max() - output_capacity) return {};
            output_capacity += row_capacity;
        }
        // Reserve both row vectors and every per-row native/ASCII capacity from
        // the measured geometry span plus the actual literal UTF-8 bytes.
        if (!charge({rows * 2u * sizeof(std::string), output_capacity, rows * columns * 2u + 256u})) return {};
        std::vector<std::string> native, ascii;
        native.reserve(rows); ascii.reserve(rows);
        for (int y = y_min; y <= y_max; ++y) {
            const auto literal = literal_bytes[static_cast<std::size_t>(y)];
            std::string native_row, ascii_row;
            native_row.reserve(columns * kNativeGeometryBytes + literal);
            ascii_row.reserve(columns + literal);
            for (int x = x_min; x <= x_max; ++x) {
                const auto index = static_cast<std::size_t>(canvas.index({x, y}));
                const auto& glyph = canvas.at({x, y}).glyph;
                native_row += glyph;
                if (owners_[index] != 0) {
                    ascii_row += glyphs_[static_cast<std::size_t>(owners_[index] - 1)];
                    continue;
                }
                const auto projected = geometry_glyph(glyph);
                if (!projected) return {};
                ascii_row += *projected;
            }
            native.emplace_back(std::move(native_row));
            ascii.emplace_back(std::move(ascii_row));
        }
        ascii_rows_ = std::move(ascii);
        return native;
    }

    std::optional<std::vector<std::string>> take_ascii_rows() {
        return std::exchange(ascii_rows_, std::nullopt);
    }

private:
    bool charge(std::initializer_list<std::size_t> costs) {
        std::size_t total = 0u;
        for (const auto cost : costs) {
            if (cost > std::numeric_limits<std::size_t>::max() - total) return false;
            total += cost;
        }
        return work_.spend(total);
    }

    static std::optional<std::string_view> geometry_glyph(std::string_view glyph) {
        if (glyph == " ") return " ";
        if (glyph == "─") return "-";
        if (glyph == "│") return "|";
        if (glyph == "╱") return "/";
        if (glyph == "╲") return "\\";
        if (glyph == "△") return "^";
        if (glyph == "┌" || glyph == "┐" || glyph == "└" || glyph == "┘" || glyph == "┬" ||
            glyph == "┤" || glyph == "┴" || glyph == "├" || glyph == "┼") return "+";
        if (glyph == "▶") return ">";
        if (glyph == "◀") return "<";
        if (glyph == "▼") return "v";
        if (glyph == "▲") return "^";
        if (glyph == "█" || glyph == "▛" || glyph == "▜" || glyph == "▙" || glyph == "▟") return "#";
        return {};
    }

    Work& work_;
    int width_ = 0, height_ = 0;
    bool prepared_ = false;
    std::vector<int> owners_;
    std::vector<std::string> glyphs_;
    std::optional<std::vector<std::string>> ascii_rows_;
};

bool paint_multiline(Canvas& canvas, FlowTextCollector* collector, Point point,
                     std::string_view value, int width) {
    return collector ? collector->multiline(canvas, point, value, width) :
        canvas.multiline(point, value, width);
}

bool node_box(Canvas& canvas, const Box& box, const Vertex& node, FlowTextCollector* collector = nullptr) {
    for (int y = 0; y < box.height; ++y) for (int x = 0; x < box.width; ++x)
        if (!canvas.put({box.x + x, box.y + y}, " ")) return false;
    if (node.shape == Shape::Decision) {
        if(!canvas.work.spend(8u*sizeof(int)+32u)||box.height<5||box.height%2==0)return false;
        const int half=box.height/2;
        if(box.width<2*half+3)return false;
        for(int y=0;y<box.height;++y) {
            const int inset=std::abs(half-y);
            const auto left=y<half?"╱":y>half?"╲":"◇";
            const auto right=y<half?"╲":y>half?"╱":"◇";
            if(!canvas.put({box.x+inset,box.y+y},left)||!canvas.put({box.x+box.width-inset-1,box.y+y},right))return false;
        }
        for(int x=half+1;x<box.width-half-1;++x)
            if(!canvas.put({box.x+x,box.y},"─")||!canvas.put({box.x+x,box.y+box.height-1},"─"))return false;
    } else if (classic_shape(node.shape)) {
        if (!classic_box(canvas, box, node.shape) || !classic_port_corridors(canvas, box, node.shape)) return false;
    } else {
        const auto top_left = node.shape == Shape::Rounded ? "╭" : "┌";
        const auto top_right = node.shape == Shape::Rounded ? "╮" : "┐";
        const auto bottom_left = node.shape == Shape::Rounded ? "╰" : "└";
        const auto bottom_right = node.shape == Shape::Rounded ? "╯" : "┘";
        for (int x = 1; x < box.width - 1; ++x)
            if (!canvas.put({box.x + x, box.y}, "─") || !canvas.put({box.x + x, box.y + box.height - 1}, "─")) return false;
        for (int y = 1; y < box.height - 1; ++y)
            if (!canvas.put({box.x, box.y + y}, "│") || !canvas.put({box.x + box.width - 1, box.y + y}, "│")) return false;
        if (!canvas.put({box.x, box.y}, top_left) || !canvas.put({box.x + box.width - 1, box.y}, top_right) ||
            !canvas.put({box.x, box.y + box.height - 1}, bottom_left) || !canvas.put({box.x + box.width - 1, box.y + box.height - 1}, bottom_right)) return false;
    }
    const auto rows = collector ? collector->row_count(node.text) :
        std::optional<int>{static_cast<int>(label_rows(node.text).size())};
    if (!rows) return false;
    return paint_multiline(canvas, collector, {box.x, box.y + (box.height - *rows) / 2}, node.text, box.width);
}

// Ranking ignores DFS back edges only in its temporary DAG. All original
// edges, including back edges and self loops, still go through the router.
struct RankedEdge { std::size_t target; int minimum_length; };

std::optional<std::vector<int>> ranks(const Diagram& graph, Work& work) {
    const auto count = graph.vertices.size();
    if (count == 0) return std::vector<int>{};
    // Pay every rank-owned adjacency, stack, and layer predecessor before any
    // new vector allocation. The fixed graph limits bound this calculation.
    const auto rank_charge = count * (2u * sizeof(std::vector<RankedEdge>) + sizeof(std::vector<std::size_t>) +
                                      3u * sizeof(std::vector<int>) + 3u * sizeof(std::size_t) + 48u) +
        graph.edges.size() * (8u * sizeof(RankedEdge) + 4u * sizeof(std::size_t) + 32u);
    if (!work.spend(rank_charge)) return {};
    std::vector<std::vector<RankedEdge>> children(count), dag(count);
    // ranks() accepts only the local vertex-only quotient built by
    // place_region(). Authored group endpoints must never become accidental
    // vector indexes here.
    const auto local_vertex = [&](const Endpoint& endpoint) -> std::optional<std::size_t> {
        if (endpoint.kind != EndpointKind::Vertex || endpoint.index >= count) return {};
        return endpoint.index;
    };
    for (const auto& edge : graph.edges) {
        const auto from = local_vertex(edge.from), to = local_vertex(edge.to);
        if (!from || !to || edge.minimum_length < 1 || edge.minimum_length > kHeightLimit) return {};
        if (*from != *to) children[*from].push_back({*to, edge.minimum_length});
    }
    // Prefer graph entries, then the first edge's source in cyclic components.
    // Standalone declaration order must not arbitrarily turn the main chain
    // into a long back edge which encloses the component's remaining ports.
    std::vector<std::size_t> roots;
    std::vector<int> incoming(count);
    for (const auto& edge : graph.edges) if (edge.from != edge.to) ++incoming[edge.to.index];
    for (std::size_t n = 0; n < count; ++n) if (!incoming[n]) roots.push_back(n);
    for (const auto& edge : graph.edges) roots.push_back(edge.from.index);
    for (std::size_t n = 0; n < count; ++n) roots.push_back(n);
    std::vector<int> color(count), order;
    for (const auto root : roots) {
        if (color[root]) continue;
        std::vector<std::pair<std::size_t, std::size_t>> stack{{root, 0}};
        color[root] = 1;
        while (!stack.empty()) {
            auto& [current, next] = stack.back();
            if (next == children[current].size()) {
                color[current] = 2;
                order.push_back(static_cast<int>(current));
                stack.pop_back();
                continue;
            }
            const auto edge = children[current][next++];
            // Preserve the existing DFS back-edge suppression: only retained
            // temporary-DAG edges may advance the weighted rank recurrence.
            if (color[edge.target] == 1) continue;
            dag[current].push_back(edge);
            if (color[edge.target] == 0) { color[edge.target] = 1; stack.emplace_back(edge.target, 0); }
        }
    }
    std::vector<int> result(count);
    for (auto at = order.rbegin(); at != order.rend(); ++at)
        for (const auto& edge : dag[static_cast<std::size_t>(*at)]) {
            const auto source = result[static_cast<std::size_t>(*at)];
            if (source > kHeightLimit - edge.minimum_length) return {};
            result[edge.target] = std::max(result[edge.target], source + edge.minimum_length);
        }
    return result;
}

std::optional<std::vector<Box>> place_sized_nodes(const Diagram& graph, std::vector<Box> boxes,
                                                   bool compact_major_gap, Work& work) {
    if (boxes.empty()) return boxes;
    const auto rank = ranks(graph, work);
    if (!rank || rank->empty()) return {};
    const auto last = *std::max_element(rank->begin(), rank->end());
    // The rank may contain intentional empty bands. Reject the maximum before
    // allocating layers, preserving the 240 x 1024 canvas cap.
    if (last < 0 || last >= kHeightLimit ||
        !work.spend((static_cast<std::size_t>(last) + 1u) *
                    (sizeof(std::vector<std::size_t>) + 4u * sizeof(std::size_t) + 32u))) return {};
    std::vector<std::vector<std::size_t>> layers(static_cast<std::size_t>(last + 1));
    for (std::size_t n = 0; n < rank->size(); ++n) layers[static_cast<std::size_t>((*rank)[n])].push_back(n);
    const bool horizontal = graph.direction == Direction::Right || graph.direction == Direction::Left;
    int cross_extent = 0;
    for (const auto& layer : layers) {
        int extent = -6;
        for (const auto n : layer) {
            extent += (horizontal ? boxes[n].height : boxes[n].width) + 6;
        }
        cross_extent = std::max(cross_extent, extent);
    }
    // The caller establishes this while it already measures each direct node.
    // The closed Classic11 LR chain needs a compact major-axis gap inside the
    // existing 240-cell canvas; old-shape placement retains its eight-cell gap.
    const int major_gap = compact_major_gap ? 2 : 8;
    int major = 4;
    for (const auto& layer : layers) {
        int extent = -6;
        int major_size = 0;
        for (const auto n : layer) {
            extent += (horizontal ? boxes[n].height : boxes[n].width) + 6;
            major_size = std::max(major_size, horizontal ? boxes[n].width : boxes[n].height);
        }
        int cross = 4 + (cross_extent - extent) / 2;
        for (const auto n : layer) {
            boxes[n].x = horizontal ? major : cross;
            boxes[n].y = horizontal ? cross : major;
            cross += (horizontal ? boxes[n].height : boxes[n].width) + 6;
        }
        major += major_size + major_gap;
    }
    int width = 0, height = 0;
    for (const auto& box : boxes) { width = std::max(width, box.x + box.width); height = std::max(height, box.y + box.height); }
    if (width + 4 > kWidthLimit || height + 4 > kHeightLimit) return {};
    for (auto& box : boxes) {
        if (graph.direction == Direction::Left) box.x = width + 4 - box.x - box.width;
        if (graph.direction == Direction::Up) box.y = height + 4 - box.y - box.height;
    }
    return boxes;
}

std::optional<std::vector<Box>> place_nodes(const Diagram& graph,Work& work) {
    std::vector<Box> boxes;
    bool compact_major_gap = false;
    for (const auto& node : graph.vertices) {
        const auto size=node_size(node,work);if(!size)return {};
        compact_major_gap = compact_major_gap || classic_shape(node.shape);
        boxes.push_back(*size);
    }
    return place_sized_nodes(graph, std::move(boxes), compact_major_gap, work);
}

struct Region {
    int width = 0, height = 0;
    std::vector<std::pair<std::size_t, Box>> nodes, groups;
};

// Compound regions are placed bottom-up. A parent ranks complete child frames,
// never individual descendants which could interleave with a sibling region.
std::optional<Region> place_region(const Diagram& graph, int owner, Direction inherited,Work& work) {
    const auto direction = owner < 0 ? inherited : graph.groups[static_cast<std::size_t>(owner)].direction.value_or(inherited);
    struct Item {
        std::size_t index, order;
        bool group;
        Box size;
        std::optional<Region> child;
    };
    std::size_t local_item_count = 0u;
    for (const auto& vertex : graph.vertices) if (vertex.group == owner) ++local_item_count;
    for (const auto& group : graph.groups) if (group.parent == owner) ++local_item_count;
    // The region owns its temporary item vector.  Prepay before reserve and
    // keep each child frame as one item rather than substituting its members.
    if (!work.spend(local_item_count * (sizeof(Item) + sizeof(Box) + 3u * sizeof(std::size_t) + 32u))) return {};
    std::vector<Item> items;
    items.reserve(local_item_count);
    bool compact_major_gap = false;
    for (std::size_t n = 0; n < graph.vertices.size(); ++n) {
        const auto& node = graph.vertices[n];
        if (node.group != owner) continue;
        const auto size=node_size(node,work);if(!size)return {};
        compact_major_gap = compact_major_gap || classic_shape(node.shape);
        items.push_back({n,node.order,false,*size,{}});
    }
    for (std::size_t g = 0; g < graph.groups.size(); ++g) {
        if (graph.groups[g].parent != owner) continue;
        auto child = place_region(graph, static_cast<int>(g), direction,work);
        if (!child) return {};
        const Box size{0, 0, child->width, child->height};
        items.push_back({g, graph.groups[g].order, true, size, std::move(child)});
    }
    std::ranges::sort(items, {}, &Item::order);

    // Project only this region's real direct vertices and child group frames
    // into the temporary ranking graph.  Authored endpoints remain tagged in
    // graph.edges; owner-frontier projection is only a layout implementation
    // detail, so it cannot invent a child vertex for a group endpoint.
    const auto projection_charge =
        (graph.vertices.size() + graph.groups.size()) * (sizeof(int) + 2u * sizeof(std::size_t) + 16u) +
        items.size() * (sizeof(Vertex) + sizeof(Box) + 16u) +
        graph.edges.size() * (sizeof(Endpoint) * 4u + sizeof(Edge) * 2u + 32u);
    if (!work.spend(projection_charge)) return {};
    std::vector<int> direct_vertex(graph.vertices.size(), -1);
    std::vector<int> direct_group(graph.groups.size(), -1);
    for (std::size_t n = 0; n < items.size(); ++n) {
        if (items[n].group) direct_group[items[n].index] = static_cast<int>(n);
        else direct_vertex[items[n].index] = static_cast<int>(n);
    }
    const auto frontier_group = [&](int group) -> std::optional<std::size_t> {
        std::size_t hops = 0u;
        while (group >= 0) {
            const auto index = static_cast<std::size_t>(group);
            if (index >= graph.groups.size()) return {};
            if (graph.groups[index].parent == owner) {
                const auto item = direct_group[index];
                return item >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(item)} : std::nullopt;
            }
            group = graph.groups[index].parent;
            if (++hops > graph.groups.size()) return {};
        }
        return {};
    };
    const auto item_for = [&](const Endpoint& endpoint) -> std::optional<std::size_t> {
        if (endpoint.kind == EndpointKind::Vertex) {
            if (endpoint.index >= graph.vertices.size()) return {};
            const auto group = graph.vertices[endpoint.index].group;
            if (group == owner) {
                const auto item = direct_vertex[endpoint.index];
                return item >= 0 ? std::optional<std::size_t>{static_cast<std::size_t>(item)} : std::nullopt;
            }
            return frontier_group(group);
        }
        if (endpoint.index >= graph.groups.size()) return {};
        const auto group = static_cast<int>(endpoint.index);
        if (group == owner) return {};
        return frontier_group(group);
    };

    Diagram quotient;
    quotient.direction = direction;
    quotient.vertices.resize(items.size());
    quotient.edges.reserve(graph.edges.size());
    for (const auto& edge : graph.edges) {
        const auto from = item_for(edge.from), to = item_for(edge.to);
        if (from && to && from != to) {
            // Preserve the legacy per-projected-edge copy tariff before the
            // quotient insertion. projection_charge separately prepays the
            // new typed projection tables and reserved quotient capacity.
            if (!work.spend(4u * sizeof(Edge) + 32u)) return {};
            quotient.edges.push_back({{EndpointKind::Vertex, *from}, {EndpointKind::Vertex, *to}, {},
                                      edge.stroke, edge.arrow_start, edge.arrow_end, edge.minimum_length});
        }
    }
    std::vector<Box> sizes;
    sizes.reserve(items.size());
    for (const auto& item : items) sizes.push_back(item.size);
    auto placed = place_sized_nodes(quotient, std::move(sizes), compact_major_gap, work);
    if (!placed) return {};
    Region result;
    for (const auto& box : *placed) {
        result.width = std::max(result.width, box.x + box.width + 4);
        result.height = std::max(result.height, box.y + box.height + 4);
    }
    int shift_x = 0, shift_y = 0;
    if (owner >= 0) {
        const auto& title = graph.groups[static_cast<std::size_t>(owner)].text;
        const auto width = std::max(result.width, label_width(title) + 4);
        shift_x = (width - result.width) / 2;
        shift_y = static_cast<int>(label_rows(title).size()) + 2;
        result.width = width;
        result.height = std::max(2, result.height) + shift_y;
        result.groups.push_back({static_cast<std::size_t>(owner), {0, 0, result.width, result.height}});
    }
    if (result.width <= 0 || result.height <= 0 || result.width > kWidthLimit || result.height > kHeightLimit) return {};
    for (std::size_t n = 0; n < items.size(); ++n) {
        auto box = (*placed)[n]; box.x += shift_x; box.y += shift_y;
        if (!items[n].group) result.nodes.push_back({items[n].index, box});
        else {
            auto append = [&](const auto& source, auto& target) {
                for (const auto& [index, relative] : source) {
                    auto absolute = relative; absolute.x += box.x; absolute.y += box.y;
                    target.push_back({index, absolute});
                }
            };
            append(items[n].child->nodes, result.nodes);
            append(items[n].child->groups, result.groups);
        }
    }
    return result;
}

bool group_frame(Canvas& canvas, const Box& box, std::string_view title) {
    auto put = [&](Point point, unsigned bits, unsigned allowed, bool corner) {
        if (!canvas.put(point, line_glyph(bits), corner)) return false;
        canvas.at(point).bits = bits;
        canvas.at(point).allowed = allowed;
        return true;
    };
    for (int x = 1; x + 1 < box.width; ++x)
        if (!put({box.x + x, box.y}, kLeft | kRight, kUp | kDown, false) ||
            !put({box.x + x, box.y + box.height - 1}, kLeft | kRight, kUp | kDown, false)) return false;
    for (int y = 1; y + 1 < box.height; ++y)
        if (!put({box.x, box.y + y}, kUp | kDown, kLeft | kRight, false) ||
            !put({box.x + box.width - 1, box.y + y}, kUp | kDown, kLeft | kRight, false)) return false;
    if (!put({box.x, box.y}, kRight | kDown, 0, true) ||
        !put({box.x + box.width - 1, box.y}, kLeft | kDown, 0, true) ||
        !put({box.x, box.y + box.height - 1}, kRight | kUp, 0, true) ||
        !put({box.x + box.width - 1, box.y + box.height - 1}, kLeft | kUp, 0, true)) return false;
    return canvas.multiline({box.x, box.y + 1}, title, box.width);
}

struct Port { Point border; Point outside; Point normal; };
int distance(Point a, Point b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
std::vector<Port> ports(const Box& box, Shape shape, Point target) {
    std::vector<Port> result;
    const auto add = [&](Point border, Point normal) {
        result.push_back({border, border + normal, normal});
    };
    if (classic_shape(shape)) {
        const int middle_x = box.x + box.width / 2;
        const int middle_y = box.y + box.height / 2;
        if (shape == Shape::Triangle) {
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
        } else if (shape == Shape::Asymmetric) {
            add({box.x, middle_y}, {-1, 0});
            add({box.x + box.width - 1, middle_y}, {1, 0});
        } else {
            add({middle_x, box.y}, {0, -1});
            add({middle_x, box.y + box.height - 1}, {0, 1});
            const bool shifted_side = shape == Shape::Trapezoid || shape == Shape::TrapezoidAlt ||
                shape == Shape::Parallelogram || shape == Shape::ParallelogramAlt;
            const int left = box.x + (shifted_side ? 1 : 0);
            const int right = box.x + box.width - 1 - (shifted_side ? 1 : 0);
            add({left, middle_y}, {-1, 0});
            add({right, middle_y}, {1, 0});
        }
    } else {
        const int inset = shape == Shape::Decision ? box.height/2+1 : 1;
        for (int x = inset; x < box.width - inset; ++x) {
            add({box.x + x, box.y}, {0, -1});
            add({box.x + x, box.y + box.height - 1}, {0, 1});
        }
        for (int y = 1; y < box.height - 1; ++y) {
            if (shape == Shape::Decision && y != box.height/2) continue;
            add({box.x, box.y + y}, {-1, 0});
            add({box.x + box.width - 1, box.y + y}, {1, 0});
        }
    }
    const Point center{box.x + box.width / 2, box.y + box.height / 2};
    std::stable_sort(result.begin(), result.end(), [&](const Port& a, const Port& b) {
        const auto score = [&](const Port& port) {
            const auto middle = port.normal.x ? std::abs(port.border.y - center.y) : std::abs(port.border.x - center.x);
            return distance(port.outside, target) * 4 + middle;
        };
        return score(a) < score(b);
    });
    return result;
}

struct Connection {
    std::vector<Point> path;
    std::size_t source, target;
};

class Router {
    Canvas& canvas_;
    std::vector<int> distance_, parent_;
    std::vector<unsigned> seen_;
    unsigned search_ = 0;
    // Strict-first pass 2 only: cells painted by earlier connector interiors.
    // When non-null the A* expansion may enter such a blocked cell (route
    // crossing); node/label cells, frame corners, port heads and temporary
    // reservations are never marked, so terminals and painted art stay sealed.
    // Nullptr selects the exact strict search pass 1 always used.
    const std::vector<unsigned char>* crossable_ = nullptr;
    // Relaxed mode shares one terminal/origin scratch across all edges, paid
    // once per pass instead of per connection() call (the strict pass keeps
    // its per-call allocation and fee untouched).
    std::vector<int> terminal_relaxed_, origin_relaxed_;
    bool relaxed_scratch_paid_ = false;
    // Mirrors Canvas::free (one Work unit per probe, inside bounds) but treats
    // marked earlier-connector cells as enterable in the relaxed pass.
    bool enterable(Point point) const {
        if (!canvas_.work.spend() || !canvas_.inside(point)) return false;
        if (!canvas_.at(point).blocked) return true;
        return crossable_ != nullptr &&
               (*crossable_)[static_cast<std::size_t>(canvas_.index(point))] != 0u;
    }
public:
    explicit Router(Canvas& canvas, const std::vector<unsigned char>* crossable = nullptr)
        : canvas_(canvas), distance_(canvas.cells.size()), parent_(canvas.cells.size()),
          seen_(canvas.cells.size()), crossable_(crossable) {
        if (crossable != nullptr) {
            terminal_relaxed_.assign(canvas.cells.size(), -1);
            origin_relaxed_.assign(canvas.cells.size(), -1);
        }
    }
    // Landing cells are terminals, entered only toward their node border.
    // A multi-source search chooses ports using actual route length, avoiding
    // long detours around the source which can enclose its remaining ports.
    std::optional<Connection> connection(const std::vector<Port>& sources, const std::vector<Port>& targets,
                                         const std::unordered_set<int>& used) {
        const bool relaxed = crossable_ != nullptr;
        if (relaxed) {
            // One workspace fee for the whole relaxed pass, charged on the
            // first multi-source search.
            if (!relaxed_scratch_paid_) {
                if (!canvas_.work.spend(canvas_.cells.size() * 2)) return {};
                relaxed_scratch_paid_ = true;
            }
            std::fill(terminal_relaxed_.begin(), terminal_relaxed_.end(), -1);
            std::fill(origin_relaxed_.begin(), origin_relaxed_.end(), -1);
        } else if (!canvas_.work.spend(canvas_.cells.size() * 2)) return {};
        std::vector<int> terminal_strict, origin_strict;
        std::vector<int>* terminal = &terminal_relaxed_;
        std::vector<int>* origin = &origin_relaxed_;
        if (!relaxed) {
            terminal_strict.assign(canvas_.cells.size(), -1);
            origin_strict.assign(canvas_.cells.size(), -1);
            terminal = &terminal_strict;
            origin = &origin_strict;
        }
        Point low{canvas_.width, canvas_.height}, high{-1, -1};
        for (std::size_t n = 0; n < targets.size(); ++n) {
            if (!canvas_.work.spend()) return {};
            const auto& port = targets[n];
            const auto before = port.outside + port.normal;
            if (used.contains(canvas_.index(port.border)) || !canvas_.free(port.outside) || !canvas_.free(before) ||
                !(canvas_.at(port.outside).allowed & direction_bits(port.normal)) ||
                !(canvas_.at(before).allowed & direction_bits(port.normal))) continue;
            (*terminal)[static_cast<std::size_t>(canvas_.index(port.outside))] = static_cast<int>(n);
            low.x = std::min(low.x, port.outside.x); low.y = std::min(low.y, port.outside.y);
            high.x = std::max(high.x, port.outside.x); high.y = std::max(high.y, port.outside.y);
        }
        if (high.x < 0) return {};
        const auto heuristic = [&](Point point) {
            return std::max({low.x - point.x, point.x - high.x, 0}) +
                   std::max({low.y - point.y, point.y - high.y, 0});
        };
        ++search_;
        using Entry = std::tuple<int, std::size_t, int>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
        std::size_t serial = 0;
        for (std::size_t n = 0; n < sources.size(); ++n) {
            if (!canvas_.work.spend()) return {};
            const auto& port = sources[n];
            if (used.contains(canvas_.index(port.border)) || !canvas_.free(port.outside) ||
                !(canvas_.at(port.outside).allowed & direction_bits(port.normal))) continue;
            const auto index = static_cast<std::size_t>(canvas_.index(port.outside));
            if (seen_[index] == search_ || (*terminal)[index] >= 0) continue;
            seen_[index] = search_; distance_[index] = 0; parent_[index] = -1;
            (*origin)[index] = static_cast<int>(n);
            queue.emplace(heuristic(port.outside), serial++, static_cast<int>(index));
        }
        while (!queue.empty()) {
            if (!canvas_.work.spend()) return {};
            const auto [priority, order, current_index] = queue.top();
            queue.pop();
            (void)order;
            const auto index = static_cast<std::size_t>(current_index);
            const Point current{current_index % canvas_.width, current_index / canvas_.width};
            if (priority != distance_[index] + heuristic(current)) continue;
            if ((*terminal)[index] >= 0) {
                Connection result{{}, static_cast<std::size_t>((*origin)[index]),
                                  static_cast<std::size_t>((*terminal)[index])};
                for (int at = current_index; at >= 0; at = parent_[static_cast<std::size_t>(at)]) {
                    if (!canvas_.work.spend()) return {};
                    result.path.push_back({at % canvas_.width, at / canvas_.width});
                }
                std::reverse(result.path.begin(), result.path.end());
                return result;
            }
            for (const auto step : kSteps) {
                if (!canvas_.work.spend()) return {};
                const auto next = current + step;
                if (!enterable(next) || !(canvas_.at(current).allowed & direction_bits(step)) ||
                    !(canvas_.at(next).allowed & direction_bits(Point{} - step))) continue;
                const auto next_index = static_cast<std::size_t>(canvas_.index(next));
                const auto target = (*terminal)[next_index];
                if (target >= 0 && step != Point{} - targets[static_cast<std::size_t>(target)].normal) continue;
                const auto cost = distance_[index] + 1;
                if (seen_[next_index] == search_ && cost >= distance_[next_index]) continue;
                seen_[next_index] = search_; distance_[next_index] = cost;
                parent_[next_index] = current_index; (*origin)[next_index] = (*origin)[index];
                queue.emplace(cost + heuristic(next), serial++, static_cast<int>(next_index));
            }
        }
        return {};
    }
    std::optional<std::vector<Point>> path(Point start, Point goal) {
        if (!canvas_.free(start) || !canvas_.free(goal)) return {};
        ++search_;
        using QueueItem = std::pair<int, int>;
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> queue;
        const auto first = canvas_.index(start), last = canvas_.index(goal);
        seen_[static_cast<std::size_t>(first)] = search_;
        distance_[static_cast<std::size_t>(first)] = 0;
        parent_[static_cast<std::size_t>(first)] = -1;
        queue.emplace(distance(start, goal), first);
        while (!queue.empty()) {
            if (!canvas_.work.spend()) return {};
            const auto [priority, index] = queue.top();
            queue.pop();
            const Point current{index % canvas_.width, index / canvas_.width};
            if (priority != distance_[static_cast<std::size_t>(index)] + distance(current, goal)) continue;
            if (index == last) {
                std::vector<Point> result;
                for (int at = last; at >= 0; at = parent_[static_cast<std::size_t>(at)]) {
                    if (!canvas_.work.spend()) return {};
                    result.push_back({at % canvas_.width, at / canvas_.width});
                }
                std::reverse(result.begin(), result.end());
                return result;
            }
            for (const auto step : kSteps) {
                if (!canvas_.work.spend()) return {};
                const auto next = current + step;
                if (!enterable(next)) continue;
                if (!(canvas_.at(current).allowed & direction_bits(step)) ||
                    !(canvas_.at(next).allowed & direction_bits(Point{} - step))) continue;
                const auto next_index = static_cast<std::size_t>(canvas_.index(next));
                const auto cost = distance_[static_cast<std::size_t>(index)] + 1;
                if (seen_[next_index] == search_ && cost >= distance_[next_index]) continue;
                seen_[next_index] = search_;
                distance_[next_index] = cost;
                parent_[next_index] = index;
                queue.emplace(cost + distance(next, goal), static_cast<int>(next_index));
            }
        }
        return {};
    }
};

bool draw_path(Canvas& canvas, const std::vector<Point>& path, Point before, Point after,
               bool arrow_start, bool arrow_end, Stroke stroke = Stroke::Solid,
               std::optional<Point> prefixed_head = {}, std::optional<Point> suffixed_head = {},
               std::vector<unsigned char>* routed = nullptr) {
    if (path.empty() || (arrow_start && arrow_end && path.size() + prefixed_head.has_value() + suffixed_head.has_value() == 1u))
        return false;
    const auto prefix = prefixed_head ? 1u : 0u;
    const auto total = path.size() + prefix + (suffixed_head ? 1u : 0u);
    const auto point_at = [&](std::size_t index) {
        if (prefixed_head && index == 0) return *prefixed_head;
        if (suffixed_head && index + 1u == total) return *suffixed_head;
        return path[index - prefix];
    };
    for (std::size_t n = 0; n < total; ++n) {
        const auto current = point_at(n);
        const auto prior = n ? point_at(n - 1u) : before;
        const auto next = n + 1u < total ? point_at(n + 1u) : after;
        const auto bits = direction_bits(prior - current) | direction_bits(next - current) | canvas.at(current).bits;
        auto glyph = line_glyph(bits);
        if (bits == (kLeft | kRight) && stroke != Stroke::Solid) glyph = stroke == Stroke::Dotted ? "┄" : "━";
        if (bits == (kUp | kDown) && stroke != Stroke::Solid) glyph = stroke == Stroke::Dotted ? "┆" : "┃";
        const bool head = (arrow_start && n == 0) || (arrow_end && n + 1u == total);
        const auto painted = arrow_start && n == 0 ? arrow(before - current) :
                             arrow_end && n + 1u == total ? arrow(after - current) : glyph;
        if (!canvas.put(current, painted)) return false;
        // Relaxed pass bookkeeping: arrow heads stay sealed (never marked),
        // line interiors become crossable and their union bits persist so a
        // later connector crossing here renders the correct junction glyph.
        if (routed != nullptr && !head) {
            (*routed)[static_cast<std::size_t>(canvas.index(current))] = 1u;
            canvas.at(current).bits = bits;
        }
    }
    return true;
}

bool source_port(Canvas& canvas, const Port& port) {
    auto bits = port.normal.x ? kUp | kDown : kLeft | kRight;
    bits |= direction_bits(port.normal);
    if (canvas.at(port.border).glyph == "◇") return true;
    return canvas.put(port.border, line_glyph(bits));
}

struct ScopedBlockedCell {
    Canvas& canvas;
    Point point;
    bool active = false;
    ScopedBlockedCell(Canvas& canvas_, Point point_, bool reserve) : canvas(canvas_), point(point_), active(reserve) {
        if (active) canvas.at(point).blocked = true;
    }
    ScopedBlockedCell(const ScopedBlockedCell&) = delete;
    ScopedBlockedCell& operator=(const ScopedBlockedCell&) = delete;
    ~ScopedBlockedCell() { release(); }
    void release() {
        if (!active) return;
        canvas.at(point).blocked = false;
        active = false;
    }
};

bool connect(Canvas& canvas, Router& router, const Port& from, const Port& to,
             const std::optional<std::string>& label_text, bool arrow_start, bool arrow_end,
             Stroke stroke = Stroke::Solid, FlowTextCollector* collector = nullptr,
             std::vector<unsigned char>* routed = nullptr) {
    const auto start = arrow_start ? from.outside + from.normal : from.outside;
    const auto goal = to.outside + to.normal;
    if (from.border == to.border || !canvas.free(from.outside) || !canvas.free(start) ||
        !canvas.free(to.outside) || !canvas.free(goal)) return false;
    if (!(canvas.at(from.outside).allowed & direction_bits(from.normal)) ||
        !(canvas.at(to.outside).allowed & direction_bits(to.normal)) ||
        !(canvas.at(goal).allowed & direction_bits(to.normal))) return false;
    // A point source must leave its frame by the straight outward shaft. Keep
    // its arrow cell unavailable to the search, release it before paint, and
    // logically prepend it to the first painted path without a new vector.
    ScopedBlockedCell target_hold{canvas, to.outside, true};
    ScopedBlockedCell source_head{canvas, from.outside, arrow_start};
    const auto prefix = arrow_start ? std::optional<Point>{from.outside} : std::nullopt;
    const auto suffix = std::optional<Point>{to.outside};
    if (!label_text) {
        auto path = router.path(start, goal);
        target_hold.release();
        source_head.release();
        if (!path) return false;
        if (!draw_path(canvas, *path, from.border, to.border, arrow_start, arrow_end, stroke, prefix, suffix, routed)) return false;
        if (!arrow_start && !source_port(canvas, from)) return false;
        return arrow_end || source_port(canvas, to);
    }
    const auto length = label_width(*label_text);
    const auto row_count = collector ? collector->row_count(*label_text) :
        std::optional<int>{static_cast<int>(label_rows(*label_text).size())};
    if (!row_count) return false;
    const int rows = *row_count;
    const Point ideal{(start.x + goal.x - length) / 2, (start.y + goal.y - rows + 1) / 2};
    // Try nearby label positions without an unbounded search over the plane.
    for (int radius = 0; radius < 32 && canvas.work.remaining; ++radius) {
        const Point offsets[]{{0, radius}, {0, -radius}, {radius, 0}, {-radius, 0}};
        for (const auto offset : offsets) {
            const auto position = ideal + offset;
            const Point left{position.x - 1, position.y + rows / 2}, right{position.x + length, position.y + rows / 2};
            bool free = canvas.free(left) && canvas.free(right) && left != start && right != start &&
                left != goal && right != goal;
            for (int y = 0; free && y < rows; ++y) for (int x = 0; free && x < length; ++x) {
                if (!canvas.work.spend()) { free = false; break; }
                const Point cell{position.x + x, position.y + y};
                free = canvas.free(cell) && canvas.at(cell).bits == 0 && cell != start && cell != goal;
            }
            if (!free) continue;
            if (!canvas.work.spend(static_cast<std::size_t>(length * rows) * 2u)) break;
            auto reserve = [&](bool blocked) {
                for (int y = 0; y < rows; ++y) for (int x = 0; x < length; ++x)
                    canvas.at({position.x + x, position.y + y}).blocked = blocked;
            };
            reserve(true);
            auto first = router.path(start, left);
            if (first && !canvas.work.spend(first->size() * 2u)) {
                reserve(false);
                return false;
            }
            if (first) for (const auto cell : *first) canvas.at(cell).blocked = true;
            auto second = first ? router.path(right, goal) : std::nullopt;
            if (first) for (const auto cell : *first) canvas.at(cell).blocked = false;
            reserve(false);
            if (!first || !second) continue;
            target_hold.release();
            source_head.release();
            if (!draw_path(canvas, *first, from.border, {left.x + 1, left.y}, arrow_start, false, stroke, prefix, {}, routed) ||
                !draw_path(canvas, *second, {right.x - 1, right.y}, to.border, false, arrow_end, stroke, {}, suffix, routed)) return false;
            reserve(true);
            if (!paint_multiline(canvas, collector, position, *label_text, length)) {
                // The ordinary branch returns as before. The collector may fail
                // after recording a private cell, so release its temporary text
                // reservation before the discarded ASCII layout falls back.
                if (collector) reserve(false);
                return false;
            }
            if (!arrow_start && !source_port(canvas, from)) return false;
            return arrow_end || source_port(canvas, to);
        }
    }
    return false;
}

std::optional<std::vector<std::string>> flow_layout(const Diagram& graph, Work& work,
                                                    FlowTextCollector* collector = nullptr) {
    std::optional<Region> region;
    std::optional<std::vector<Box>> placed;
    if (graph.groups.empty()) placed = place_nodes(graph,work);
    else {
        region = place_region(graph, -1, graph.direction,work);
        if (!region) return {};
        if (!work.spend(graph.vertices.size() * (sizeof(Box) + 16u))) return {};
        placed.emplace(graph.vertices.size(), Box{0, 0, 0, 0});
        for (const auto& [index, box] : region->nodes) {
            if (index >= placed->size() || box.width <= 0 || box.height <= 0) return {};
            (*placed)[index] = box;
        }
    }
    if (!placed) return {};

    const bool has_group_endpoint = graph.has_group_endpoint;
    struct EndpointBox { Box box; Shape shape = Shape::Rectangle; };
    struct RoutedEdge {
        std::size_t edge = 0u;
        EndpointBox from, to;
        int distance = 0;
    };
    std::vector<RoutedEdge> tagged_routing_order;
    if (has_group_endpoint) {
        // Route from the real box belonging to each authored endpoint. A group
        // is its painted frame, never a representative child vertex. These
        // tables and the stable routing order are prepaid before allocation.
        const auto route_table_charge =
            graph.groups.size() * (sizeof(std::optional<Box>) + sizeof(Box) + 16u) +
            graph.edges.size() * (2u * sizeof(RoutedEdge) + sizeof(std::size_t) + 48u);
        if (!work.spend(route_table_charge)) return {};
        std::vector<std::optional<Box>> group_boxes(graph.groups.size());
        if (region) for (const auto& [index, box] : region->groups) {
            if (index >= group_boxes.size() || group_boxes[index] || box.width <= 0 || box.height <= 0) return {};
            group_boxes[index] = box;
        }
        for (const auto& group : group_boxes) if (!group) return {};
        const auto box_for = [&](const Endpoint& endpoint) -> std::optional<EndpointBox> {
            if (endpoint.kind == EndpointKind::Vertex) {
                if (endpoint.index >= placed->size()) return {};
                const auto box = (*placed)[endpoint.index];
                if (box.width <= 0 || box.height <= 0) return {};
                return EndpointBox{box, graph.vertices[endpoint.index].shape};
            }
            if (endpoint.index >= group_boxes.size() || !group_boxes[endpoint.index]) return {};
            return EndpointBox{*group_boxes[endpoint.index], Shape::Rectangle};
        };
        tagged_routing_order.reserve(graph.edges.size());
        for (std::size_t index = 0; index < graph.edges.size(); ++index) {
            const auto from = box_for(graph.edges[index].from);
            const auto to = box_for(graph.edges[index].to);
            if (!from || !to) return {};
            tagged_routing_order.push_back({index, *from, *to,
                distance({from->box.x + from->box.width / 2, from->box.y + from->box.height / 2},
                         {to->box.x + to->box.width / 2, to->box.y + to->box.height / 2})});
        }
    }

    int width = 0, height = 0, longest = 0;
    for (const auto& box : *placed) {
        if (box.width <= 0 || box.height <= 0) return {};
        width = std::max(width, box.x + box.width);
        height = std::max(height, box.y + box.height);
    }
    if (region) { width = std::max(width, region->width); height = std::max(height, region->height); }
    for (const auto& edge : graph.edges) if (edge.text) longest = std::max(longest, label_width(*edge.text));
    if (longest + 4 > kWidthLimit) return {};
    width = std::min(kWidthLimit, width + 12 + 2 * longest + static_cast<int>(graph.edges.size()) * 2);
    height = std::min(kHeightLimit, height + 12 + static_cast<int>(graph.edges.size()) * 3);
    if (!work.spend(static_cast<std::size_t>(width * height) * 4)) return {};
    std::size_t literal_bytes = 0u;
    if (collector) {
        for (const auto& vertex : graph.vertices) literal_bytes += vertex.text.size();
        for (const auto& edge : graph.edges) if (edge.text) literal_bytes += edge.text->size();
        if (!collector->prepare(width, height,
            (graph.vertices.size() + graph.edges.size()) * static_cast<std::size_t>(kLabelLimit), literal_bytes)) return {};
    }
    const auto canvas_area = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    // One whole-plan paint-and-route attempt over a fresh canvas. Pass 1
    // (strict) reproduces the historical routing byte-for-byte: connector
    // cells are sealed once laid, so edges never cross. Pass 2 (relaxed) runs
    // only after a full strict failure; it may cross earlier connector
    // interiors while node/label cells, frame corners and port heads stay
    // sealed. Both passes are deterministic and metered by the same Work.
    const auto attempt = [&](bool strict) -> std::optional<std::vector<std::string>> {
    std::vector<unsigned char> routed;
    FlowTextCollector* pass_collector = collector;
    if (!strict) {
        // The discarded strict pass may have recorded label owners. Re-arm the
        // collector with its identical bounds before repainting.
        if (collector &&
            !collector->restart(width, height,
                (graph.vertices.size() + graph.edges.size()) *
                    static_cast<std::size_t>(kLabelLimit),
                literal_bytes)) {
            return {};
        }
        // Rebuild fee for the second workspace: the crossing byte grid plus a
        // fixed slack. The fresh canvas and the Router's BFS scratch are
        // allocated once per pass; the multi-source search charges its own
        // shared workspace fee on the first edge (instead of per edge as in
        // the strict pass). A fresh Work per layout means a failed strict pass
        // already spent its budget; an unaffordable retry is the same
        // whole-source result as today.
        if (canvas_area > std::numeric_limits<std::size_t>::max() - 4096u) return {};
        if (!work.spend(canvas_area + 4096u)) return {};
        routed.assign(canvas_area, 0u);
    }
    Canvas canvas(width, height, work);
    if (region) for (const auto& [index, box] : region->groups)
        if (index >= graph.groups.size() || !group_frame(canvas, box, graph.groups[index].text)) return {};
    for (std::size_t n = 0; n < placed->size(); ++n)
        if (!node_box(canvas, (*placed)[n], graph.vertices[n], pass_collector)) return {};

    Router router(canvas, strict ? nullptr : &routed);
    std::unordered_set<int> used_ports;
    const auto route_one = [&](const Edge& edge, const EndpointBox& source_box, const EndpointBox& target_box) {
        const auto& from = source_box.box;
        const auto& to = target_box.box;
        auto sources = ports(from, source_box.shape, {to.x + to.width / 2, to.y + to.height / 2});
        auto targets = ports(to, target_box.shape, {from.x + from.width / 2, from.y + from.height / 2});
        bool group_source_targets_descendant = false;
        if (edge.from.kind == EndpointKind::Group && edge.to.kind == EndpointKind::Vertex) {
            if (edge.from.index >= graph.groups.size() || edge.to.index >= graph.vertices.size()) return false;
            int owner = graph.vertices[edge.to.index].group;
            for (std::size_t depth = 0; owner >= 0 && depth < kGroupDepthLimit; ++depth) {
                if (!work.spend() || static_cast<std::size_t>(owner) >= graph.groups.size()) return false;
                if (static_cast<std::size_t>(owner) == edge.from.index) {
                    group_source_targets_descendant = true;
                    break;
                }
                owner = graph.groups[static_cast<std::size_t>(owner)].parent;
            }
            if (owner < -1 || (owner >= 0 && !group_source_targets_descendant)) return false;
        }
        const bool horizontal = graph.direction == Direction::Right || graph.direction == Direction::Left;
        const bool reverse = graph.direction == Direction::Left || graph.direction == Direction::Up;
        const auto advance = horizontal ? to.x - from.x : to.y - from.y;
        if ((reverse ? -advance : advance) <= 0) {
            // Prefer outside-facing ports for back edges and self loops, reducing
            // paths that enclose their own node or intervening nodes along the spine.
            const auto outside_first = [&](const Port& a, const Port& b) {
                return (horizontal ? a.normal.y : a.normal.x) > (horizontal ? b.normal.y : b.normal.x);
            };
            std::stable_sort(sources.begin(), sources.end(), outside_first);
            std::stable_sort(targets.begin(), targets.end(), outside_first);
        }
        if (group_source_targets_descendant) for (auto& source : sources) {
            source.normal = {-source.normal.x, -source.normal.y};
            source.outside = source.border + source.normal;
        }
        if (!edge.text && edge.from != edge.to && !edge.arrow_start && edge.arrow_end) {
            auto connection = router.connection(sources, targets, used_ports);
            if (!connection) return false;
            const auto& source = sources[connection->source];
            const auto& target = targets[connection->target];
            if (!draw_path(canvas, connection->path, source.border, target.border, false, true,
                           edge.stroke, {}, {}, strict ? nullptr : &routed) ||
                !source_port(canvas, source)) return false;
            used_ports.insert(canvas.index(source.border)); used_ports.insert(canvas.index(target.border));
            return true;
        }
        for (const auto& source : sources) {
            if (used_ports.contains(canvas.index(source.border))) continue;
            for (const auto& target : targets) {
                if (!work.remaining) return false;
                if (used_ports.contains(canvas.index(target.border))) continue;
                if (!connect(canvas, router, source, target, edge.text, edge.arrow_start, edge.arrow_end,
                             edge.stroke, pass_collector, strict ? nullptr : &routed)) continue;
                used_ports.insert(canvas.index(source.border));
                used_ports.insert(canvas.index(target.border));
                return true;
            }
        }
        return false;
    };
    if (has_group_endpoint) {
        // The source order is retained through `edge`; only route order is
        // stably distance-sorted, exactly as for the legacy branch below.
        std::stable_sort(tagged_routing_order.begin(), tagged_routing_order.end(), [](const RoutedEdge& a, const RoutedEdge& b) {
            return a.distance < b.distance;
        });
        for (const auto& route : tagged_routing_order)
            if (!route_one(graph.edges[route.edge], route.from, route.to)) {
                return std::optional<std::vector<std::string>>{};
            }
    } else {
        // The private parser only creates vertex endpoints on this branch, but
        // validate that invariant before the unchanged index-based route sort.
        if (!work.spend(graph.edges.size() * (2u * sizeof(Endpoint) + 16u))) return {};
        for (const auto& edge : graph.edges)
            if (edge.from.kind != EndpointKind::Vertex || edge.to.kind != EndpointKind::Vertex ||
                edge.from.index >= placed->size() || edge.to.index >= placed->size()) return {};
        std::vector<std::size_t> routing_order(graph.edges.size());
        std::iota(routing_order.begin(), routing_order.end(), 0);
        const auto edge_distance = [&](std::size_t index) {
            const auto& edge = graph.edges[index];
            const auto& a = (*placed)[edge.from.index];
            const auto& b = (*placed)[edge.to.index];
            return distance({a.x + a.width / 2, a.y + a.height / 2}, {b.x + b.width / 2, b.y + b.height / 2});
        };
        std::stable_sort(routing_order.begin(), routing_order.end(), [&](std::size_t a, std::size_t b) {
            return edge_distance(a) < edge_distance(b);
        });
        for (const auto index : routing_order) {
            const auto& edge = graph.edges[index];
            if (!route_one(edge, {(*placed)[edge.from.index], graph.vertices[edge.from.index].shape},
                            {(*placed)[edge.to.index], graph.vertices[edge.to.index].shape})) {
                return std::optional<std::vector<std::string>>{};
            }
        }
    }

    auto lines = pass_collector
        ? pass_collector->finish(canvas)
        : std::optional<std::vector<std::string>>{canvas.lines()};
    if (!lines || lines->empty()) return {};
    return lines;
    };

    // Pass 1: the exact strict plan. Every input that routes today keeps this
    // output; the relaxed retry exists only for full-plan failure, and its
    // result is consumed only when pass 1 failed.
    if (auto lines = attempt(true)) return lines;
    // Pass 2: one deterministic bounded whole-plan retry permitting crossings.
    return attempt(false);
}

bool ascii_flow_header(std::string_view source) {
    const auto end = source.find('\n');
    const auto header = trim(source.substr(0, end));
    const auto valid = [](std::string_view line, std::string_view name) {
        if (!line.starts_with(name) || line.size() == name.size()) return false;
        const auto separator = line[name.size()];
        if (separator != ' ' && separator != '\t') return false;
        line.remove_prefix(name.size());
        line = trim(line);
        return line == "LR" || line == "TD" || line == "BT";
    };
    return valid(header, "graph") || valid(header, "flowchart");
}

bool ascii_flow_profile(const Diagram& graph, Work& work) {
    if (!graph.groups.empty()) return false;
    // A complete three-node DAG is a chain plus one skip edge. Inspect its
    // topology independently of vertex/edge declaration order; projection uses
    // the same already-bounded native layout and literal ownership as LR.
    if ((graph.direction == Direction::Down || graph.direction == Direction::Up) &&
        graph.vertices.size() == 3u && graph.edges.size() == 3u) {
        // Prepay the fixed three-vertex/three-edge topology check. The existing
        // LR profile keeps its original budget accounting exactly.
        if (!work.spend(512u)) return false;
        std::array<bool, 9> seen{};
        std::array<int, 3> incoming{};
        for (const auto& vertex : graph.vertices)
            if (vertex.shape != Shape::Rectangle || vertex.text.empty() ||
                vertex.text.find('\n') != std::string::npos) return false;
        for (const auto& edge : graph.edges) {
            if (edge.from.kind != EndpointKind::Vertex || edge.to.kind != EndpointKind::Vertex ||
                edge.from.index >= 3u || edge.to.index >= 3u || edge.from == edge.to ||
                edge.text.has_value() || edge.stroke != Stroke::Solid ||
                edge.arrow_start || !edge.arrow_end || edge.minimum_length != 1) return false;
            const auto pair = edge.from.index * 3u + edge.to.index;
            // Reject either orientation of a repeated unordered pair.
            if (seen[pair] || seen[edge.to.index * 3u + edge.from.index]) return false;
            seen[pair] = true;
            ++incoming[edge.to.index];
        }
        std::ranges::sort(incoming);
        return incoming == std::array<int, 3>{{0, 1, 2}};
    }
    if (graph.direction != Direction::Right || graph.vertices.size() != 2u ||
        graph.edges.size() != 1u) return false;
    for (const auto& vertex : graph.vertices)
        if (vertex.shape != Shape::Rectangle || !vertex.explicit_label || vertex.text.empty()) return false;
    const auto& edge = graph.edges.front();
    return edge.from.kind == EndpointKind::Vertex && edge.from.index == 0u &&
        edge.to.kind == EndpointKind::Vertex && edge.to.index == 1u && edge.text.has_value() && !edge.text->empty() &&
        edge.stroke == Stroke::Solid && !edge.arrow_start && edge.arrow_end && edge.minimum_length == 1;
}

std::optional<detail::FlowAsciiRows> ascii_flow_rows(std::string_view source, Work& work) {
    if (!ascii_flow_header(source)) return {};
    const auto graph = parse(source, work);
    if (!graph || !ascii_flow_profile(*graph, work)) return {};
    FlowTextCollector collector{work};
    auto native = flow_layout(*graph, work, &collector);
    auto ascii = collector.take_ascii_rows();
    if (!native || !ascii || native->empty() || ascii->empty() || native->size() != ascii->size()) return {};
    return detail::FlowAsciiRows{std::move(*native), std::move(*ascii)};
}

// A routing-only probe. The complete untouched source still goes to its owner.
// It never grants config syntax to another family or parses config values.
enum class PrefixOwner : unsigned char { Other,Sequence,Class };
bool class_prefix_header(std::string_view line) { return line=="classDiagram"||line=="classDiagram-v2"; }
bool requirement_prefix_header(std::string_view line) {
    // The Requirement lexer is case-insensitive; compare two folded spellings,
    // not a folded input against the mixed-case source keyword.
    constexpr std::string_view header="requirementdiagram";
    if(line.size()!=header.size())return false;
    for(std::size_t at=0;at<header.size();++at) {
        const auto byte=line[at];
        const auto folded=byte>='A'&&byte<='Z'?static_cast<char>(byte-'A'+'a'):byte;
        if(folded!=header[at])return false;
    }
    return true;
}
std::optional<PrefixOwner> sequence_prefix_route(std::string_view source, Work& work) {
    // RFC 0031 E5: this linear source walk is dominated by the single
    // classify_mermaid probe fee.
    (void)work;
    bool frontmatter = false, seen_frontmatter = false, directive = false;
    while (!source.empty()) {
        const auto end = source.find('\n');
        auto line = source.substr(0, end);
        source = end == std::string_view::npos ? std::string_view{} : source.substr(end + 1);
        if (line.ends_with('\r')) line.remove_suffix(1);
        line = trim(line);
        if (directive) {
            // A close inside quoted data with a nonblank tail is not a prefix
            // boundary. Complete string/object validation belongs to the owner.
            if (line.ends_with("}%%")) directive = false;
            continue;
        }
        if (frontmatter) { if (line == "---") frontmatter = false; continue; }
        if (line.empty()) continue;
        if (line == "---" && !seen_frontmatter) { frontmatter = true; seen_frontmatter = true; continue; }
        if (line.starts_with("%%{")) { directive = !line.ends_with("}%%"); continue; }
        if (line.starts_with("%%")) continue;
        while (line.starts_with(';')) line = trim(line.substr(1));
        if(line.starts_with("sequenceDiagram"))return PrefixOwner::Sequence;
        if(line.starts_with("classDiagram")) {
            return class_prefix_header(line)?PrefixOwner::Class:PrefixOwner::Other;
        }
        return PrefixOwner::Other;
    }
    return PrefixOwner::Other;
}

class MermaidNode final : public ftxui::Node {
    int diagram_width_;
    bool diagram_ = true;
    bool changed_ = false;
public:
    MermaidNode(ftxui::Element art, ftxui::Element fallback, int width)
        : Node({std::move(art), std::move(fallback)}), diagram_width_(width) {}
    void ComputeRequirement() override {
        for (const auto& child : children_) child->ComputeRequirement();
        requirement_ = children_[diagram_ ? 0 : 1]->requirement();
        requirement_.flex_shrink_x = 1;
    }
    void SetBox(ftxui::Box box) override {
        box_ = box;
        const auto desired = box.x_max - box.x_min + 1 >= diagram_width_;
        if (desired != diagram_) { diagram_ = desired; changed_ = true; }
        children_[diagram_ ? 0 : 1]->SetBox(box);
    }
    void Check(Status* status) override {
        children_[diagram_ ? 0 : 1]->Check(status);
        status->need_iteration |= changed_ || status->iteration == 0;
        changed_ = false;
    }
    void Render(ftxui::Screen& screen) override { children_[diagram_ ? 0 : 1]->Render(screen); }
};

std::optional<ftxui::Element> finish_legacy_rows(std::vector<std::string> lines, ftxui::Element fallback) {
    if (lines.empty()) return {};
    int width = 0;
    ftxui::Elements rows;
    for (auto& line : lines) {
        width = std::max(width, ftxui::string_width(line));
        rows.push_back(ftxui::text(std::move(line)));
    }
    if (width > kWidthLimit || rows.size() > kHeightLimit) return {};
    return std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback), width);
}

} // namespace

std::optional<detail::FlowAsciiRows> detail::flow_ascii_rows(std::string_view source, detail::Work& work) {
    return ascii_flow_rows(source, work);
}

std::optional<detail::MermaidProbeDecision> detail::classify_mermaid(std::string_view source, Work& work) {
    using detail::MermaidProbeDecision;
    using detail::MermaidProbeFamily;

    // RFC 0031 E5: the probe-table boundary fee replaces the former flag
    // bookkeeping. The fixed term equals the historic fixed flag fees. A
    // frontmatter peek adds 1/peek-byte (the legacy prefix_work accumulation)
    // plus a small constant dominating the deleted body-line charges.
    constexpr std::size_t kProbePerPeekByte = 1u;
    constexpr std::size_t kProbePeekFixed = 600u;
    constexpr std::size_t kProbeFixed = 192u;
    std::size_t peek_bytes = 0;
    bool has_frontmatter_peek = false;

    // ---- textual scan (byte-identical to the legacy header/peek loops) ----
    auto remaining = source;
    std::string_view header;
    bool multiline_directive = false;
    bool class_envelope_seen = false, class_peek = false;
    bool flow = false, requirement = false;
    while (!remaining.empty()) {
        const auto end = remaining.find('\n');
        header = remaining.substr(0, end);
        if (header.ends_with('\r')) header.remove_suffix(1);
        header = trim(header);
        while (header.starts_with(';')) header = trim(header.substr(1));
        if (header.starts_with("%%{") && !header.ends_with("}%%")) multiline_directive = true;
        if (header.starts_with("%%{") && header.ends_with("}%%")) class_envelope_seen = true;
        if (!header.empty() && !header.starts_with("%%")) break;
        if (end == std::string_view::npos) break;
        remaining.remove_prefix(end + 1);
    }
    bool sequence = false, er = false, gitgraph = false, state = false;
    bool mindmap = header == "mindmap";
    bool block_input = header == "block" || header == "block-beta";
    if (header == "---") {
        has_frontmatter_peek = true;
        auto peek = remaining;
        bool opening = true;
        bool closed = false;
        while (!peek.empty()) {
            const auto end = peek.find('\n');
            auto line = peek.substr(0, end);
            peek_bytes += line.size() + 1;
            peek = end == std::string_view::npos ? std::string_view{} : peek.substr(end + 1);
            if (line.ends_with('\r')) line.remove_suffix(1);
            if (opening) { opening = false; continue; }
            if (!closed) {
                if (line == "---") closed = true;
                continue;
            }
            line = trim(line);
            while (line.starts_with(';')) line = trim(line.substr(1));
            if (line.starts_with("%%{") && !line.ends_with("}%%")) multiline_directive = true;
            if (line.empty() || line.starts_with("%%")) continue;
            flow = (line.starts_with("flowchart") &&
                    (line.size() == 9u || line[9] == ' ' || line[9] == '\t')) ||
                   (line.starts_with("graph") &&
                    (line.size() == 5u || line[5] == ' ' || line[5] == '\t'));
            sequence = line.starts_with("sequenceDiagram");
            er = line.starts_with("erDiagram") &&
                 (line.size() == 9 || line[9] == ' ' || line[9] == '\t');
            requirement = requirement_prefix_header(line);
            gitgraph = line.starts_with("gitGraph") &&
                       (line.size() == 8u || line[8] == ' ' || line[8] == '\t' || line[8] == ':');
            state = line == "stateDiagram-v2" || line == "stateDiagram";
            mindmap = line == "mindmap";
            block_input = line == "block" || line == "block-beta";
            if (line.starts_with("classDiagram")) {
                class_peek = class_prefix_header(line);
            }
            break;
        }
    }
    // Legacy semantics: when the multiline-directive route runs it OVERWRITES
    // (never ORs) the peeked class/sequence flags.
    bool class_flag = class_peek;
    if (multiline_directive && !sequence && !header.starts_with("sequenceDiagram")) {
        // A directive header without a frontmatter fence still walks the full
        // source once; count that span in the bounded probe term.
        if (!has_frontmatter_peek) peek_bytes = std::max(peek_bytes, source.size());
        const auto probe = sequence_prefix_route(source, work);
        if (!probe) return std::nullopt; // hard reject
        sequence = *probe == PrefixOwner::Sequence;
        class_flag = *probe == PrefixOwner::Class;
    }

    // Charge after the peek span is known; the multiline route above walks the
    // source one extra time and is bounded by the same term.
    const std::size_t probe_total = kProbeFixed +
        (has_frontmatter_peek || multiline_directive ?
         kProbePeekFixed + kProbePerPeekByte * peek_bytes : 0u);
    if (probe_total > work.remaining) return std::nullopt;
    if (!work.spend(probe_total)) return std::nullopt;

    // ---- ordered probes, first match wins; Charts is the terminal fallback ----
    // RFC 0031 E5: the old per-family prepay accumulator is dominated by the
    // probe fee above; decision.prepay stays zero for the switch.
    MermaidProbeDecision decision;

    if (flow) {
        decision.family = MermaidProbeFamily::Flow;
        return decision;
    }
    if (header.starts_with("sequenceDiagram") || sequence) {
        decision.family = MermaidProbeFamily::Sequence;
        return decision;
    }
    if (header.starts_with("gitGraph") || gitgraph) {
        decision.family = MermaidProbeFamily::GitGraph;
        return decision;
    }

    // Relations family: the execution switch calls one relations_layout for
    // all four; the subfamily label documents the probe order only.
    const bool header_requirement = requirement_prefix_header(header);
    const bool relations_owner =
        header.starts_with("classDiagram") || header.starts_with("erDiagram") ||
        header.starts_with("stateDiagram") || header_requirement ||
        er || class_flag || requirement || state;
    if (!relations_owner) {
        decision.family = block_input ? MermaidProbeFamily::Block : mindmap ? MermaidProbeFamily::Mindmap : MermaidProbeFamily::Charts;
        return decision;
    }
    if (header_requirement || requirement)
        decision.family = MermaidProbeFamily::Requirement;
    else if (class_flag || header.starts_with("classDiagram"))
        decision.family = MermaidProbeFamily::Class;
    else if (state || header.starts_with("stateDiagram"))
        decision.family = MermaidProbeFamily::State;
    else
        decision.family = MermaidProbeFamily::Er;
    decision.class_envelope =
        class_flag || (class_envelope_seen && class_prefix_header(header));
    decision.requirement_input = header_requirement || requirement;
    return decision;
}

// RFC 0031 E6b: layout-only seam. This is the exact pre-E6b dispatch body
// of the two-argument render_mermaid, plus the explicit-ASCII routing of
// the three-argument entry point; it owns a fresh Work budget per call and
// returns immutable rows/optional roles (nullopt = whole-source rejection).
namespace {
std::optional<detail::LayoutPayload> layout_payload_unchecked(
    std::string_view source, bool use_ascii, detail::Work& work) {
    using namespace detail;
    if (source.empty() || source.size() > kSourceLimit) return {};
    if (use_ascii) {
        // An explicit bare Flow header is owned by the closed ASCII Flow
        // profile. Rejected Flow never falls through to GitGraph as a
        // label-like foreign input.
        if (ascii_flow_header(source)) {
            auto rows = detail::flow_ascii_rows(source, work);
            return rows ? std::optional<LayoutPayload>(LayoutPayload{std::move(rows->ascii), {}})
                        : std::nullopt;
        }
        auto rows = detail::gitgraph_layout(source, work, true);
        if (!rows) return {};
        return LayoutPayload{std::move(*rows), {}};
    }
    std::optional<std::vector<std::string>> lines;
    if (const auto diagram = parse(source, work)) {
        lines = flow_layout(*diagram, work);
    } else {
        // RFC 0031 E4: ordered family probes replace the mutable flag cluster.
        // Classification is one textual scan; execution spends exactly the
        // charges the legacy flags did. nullopt means the legacy route bailed
        // on Work exhaustion before selecting an owner (whole-source result).
        const auto decision = detail::classify_mermaid(source, work);
        if (!decision) return {};
        switch (decision->family) {
        case detail::MermaidProbeFamily::Flow: {
            const auto envelope = flow_envelope_body(source, work);
            if (!envelope || !flow_envelope_parse_prepay(envelope->body, work)) return {};
            const auto diagram = parse(envelope->body, work, true);
            if (!diagram) return {};
            lines = flow_layout(*diagram, work);
            if (lines && envelope->title && !prepend_flow_title(*lines, *envelope->title, work)) return {};
            break;
        }
        case detail::MermaidProbeFamily::Sequence:
            lines = detail::sequence_layout(source, work);
            break;
        case detail::MermaidProbeFamily::GitGraph:
            lines = detail::gitgraph_layout(source, work);
            break;
        case detail::MermaidProbeFamily::Class:
        case detail::MermaidProbeFamily::Er:
        case detail::MermaidProbeFamily::State:
        case detail::MermaidProbeFamily::Requirement:
            return detail::relations_layout(source, work,
                decision->class_envelope, decision->requirement_input);
        case detail::MermaidProbeFamily::Block:
            return detail::block_layout(source, work);
        case detail::MermaidProbeFamily::Mindmap:
            lines = detail::mindmap_layout(source, work);
            break;
        case detail::MermaidProbeFamily::Charts:
            lines = detail::charts_layout(source, work);
            break;
        }
    }
    if (!lines) return {};
    return LayoutPayload{std::move(*lines), {}};
}

} // namespace

std::optional<detail::LayoutPayload> detail::layout_mermaid_payload(
    std::string_view source, bool use_ascii) {
    Work work;
    return layout_mermaid_payload(source,use_ascii,work);
}

std::optional<detail::LayoutPayload> detail::layout_mermaid_payload(
    std::string_view source, bool use_ascii, Work& work) {
    auto payload = layout_payload_unchecked(source,use_ascii,work);
    if (payload && !valid_layout_payload(*payload)) return {};
    return payload;
}

bool detail::valid_layout_payload(const LayoutPayload& payload) {
    if (payload.styles.empty()) return true;
    if (payload.rows.empty() || payload.rows.size() > kHeightLimit) return false;
    std::size_t slot = 0, bytes = 0;
    for (const auto& line : payload.rows) {
        if (line.size() > kSourceLimit || !unicode_text(line)) return false;
        bytes += line.size();
        const int width = ftxui::string_width(line);
        if (width < 0 || width > kWidthLimit) return false;
        // Validate without allocating a byte-capacity glyph vector. FTXUI's
        // scalar width is the same predicate its Utf8ToGlyphs uses; zero-width
        // scalars extend the previous complete glyph, including a wide tail.
        // unicode_text above has already checked every scalar's encoding.
        bool base = false;
        for (std::size_t byte = 0; byte < line.size();) {
            const auto lead = static_cast<unsigned char>(line[byte]);
            const std::size_t length = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            const int cells = ftxui::string_width(line.substr(byte, length));
            byte += length;
            if (cells == 0) { if (!base) return false; continue; }
            if ((cells != 1 && cells != 2) || slot > payload.styles.size() ||
                static_cast<std::size_t>(cells) > payload.styles.size() - slot) return false;
            const auto style = payload.styles[slot];
            if ((style & 0xf800u) != 0 || (style & 31u) > 6 || ((style >> 5) & 31u) > 6 ||
                (cells == 2 && payload.styles[slot+1] != style)) return false;
            slot += static_cast<std::size_t>(cells);
            base = true;
        }
    }
    return slot == payload.styles.size() && slot <= bytes;
}

std::optional<ftxui::Element> detail::finish_mermaid_payload(
    std::optional<LayoutPayload> payload, ftxui::Element fallback) {
    if (!fallback || !payload || payload->rows.empty()) return {};
    if (payload->styles.empty())
        return finish_legacy_rows(std::move(payload->rows), std::move(fallback));
    // This private finish only receives admitted payloads from the single
    // layout kernel or immutable exact-key cache. Do not revalidate/relayout.
    // Resolve semantic roles exactly once per fresh Element; immutable cache
    // entries contain neither this palette nor an Element/view into its data.
    const auto active = loom::ui::design::theme::current_theme();
    const auto& p = *active.palette;
    // Role 0 inherits below; slot 0 is an unused palette-only placeholder.
    const std::array<ftxui::Color,7> fg{p.text,p.text,p.danger,p.warning,p.success,p.info,p.accent_purple};
    const std::array<ftxui::Color,7> bg{p.background,p.surface_tint_2,p.danger_tint_bg,p.warning_tint_bg,p.success_tint_bg,p.info_tint_bg,p.surface_tint_4};
    int width = 0;
    std::size_t slot = 0;
    ftxui::Elements rows;
    rows.reserve(payload->rows.size());
    for (const auto& line : payload->rows) {
        width = std::max(width, ftxui::string_width(line));
        ftxui::Elements runs;
        std::string run;
        std::uint16_t code = 0;
        const auto flush = [&] {
            if (run.empty()) return;
            auto node = ftxui::text(std::move(run));
            if (const auto role = code & 31u; role != 0) node = node | ftxui::color(fg[role]);
            if (const auto role = (code >> 5) & 31u; role != 0) node = node | ftxui::bgcolor(bg[role]);
            if (code & 1024u) node = node | ftxui::bold;
            runs.push_back(std::move(node));
            run.clear();
        };
        std::size_t cells = 0;
        for (std::size_t byte = 0; byte < line.size();) {
            const auto begin = byte;
            const auto scalar_length = [&](std::size_t at) -> std::size_t {
                const auto lead = static_cast<unsigned char>(line[at]);
                return lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            };
            const auto length = scalar_length(byte);
            const int occupied = ftxui::string_width(line.substr(byte,length));
            byte += length;
            // Group every zero-width continuation with its base. This retains
            // wide+combining tails as exact original bytes, with no temporary
            // byte-capacity Utf8ToGlyphs vector during fresh-node assembly.
            while (byte < line.size()) {
                const auto tail = scalar_length(byte);
                if (ftxui::string_width(line.substr(byte,tail)) != 0) break;
                byte += tail;
            }
            const auto next = payload->styles[slot+cells];
            if (!run.empty() && code != next) flush();
            code = next;
            run.append(line,begin,byte-begin);
            cells += static_cast<std::size_t>(occupied);
        }
        flush();
        slot += cells;
        // An empty annotation separator still owns one physical row.
        rows.push_back(runs.empty() ? ftxui::text("") : ftxui::hbox(std::move(runs)));
    }
    return std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback), width);
}

std::optional<ftxui::Element> render_mermaid(std::string_view source, ftxui::Element fallback) {
    if (!fallback) return {};
    // RFC 0031 E6b: layout seam + fresh-node finish.
    return detail::finish_mermaid_payload(detail::layout_mermaid_payload(source, false), std::move(fallback));
}

// The existing two-argument entrypoint owns the default dispatch. Explicit
// ASCII currently admits the source-faithful GitGraph profile only.
std::optional<ftxui::Element> render_mermaid(std::string_view source, ftxui::Element fallback, bool use_ascii) {
    if (!fallback) return {};
    if (!use_ascii) return render_mermaid(source, std::move(fallback));
    return detail::finish_mermaid_payload(detail::layout_mermaid_payload(source, true), std::move(fallback));
}

} // namespace loom::ui::mermaid
