module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;

namespace loom::ui::mermaid {
namespace {

constexpr std::size_t kSourceLimit = 65536;
constexpr int kWidthLimit = 240;
constexpr int kHeightLimit = 1024;
constexpr int kLabelLimit = 256;
constexpr std::size_t kWorkLimit = 1000000;

struct Work {
    std::size_t remaining = kWorkLimit;
    bool spend(std::size_t count = 1) {
        if (count > remaining) { remaining = 0; return false; }
        remaining -= count;
        return true;
    }
};

std::string_view trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

bool id_start(char value) {
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || value == '_';
}

bool id_char(char value) { return id_start(value) || (value >= '0' && value <= '9'); }

// Reject malformed UTF-8 and terminal/bidi controls before the FTXUI width
// helpers can silently discard them. Labels are data, never terminal commands.
bool plain_unicode(std::string_view text) {
    for (std::size_t at = 0; at < text.size();) {
        const auto lead = static_cast<unsigned char>(text[at++]);
        std::uint32_t scalar = lead;
        int trailing = 0;
        if (lead >= 0xc2 && lead <= 0xdf) { scalar = lead & 0x1f; trailing = 1; }
        else if (lead >= 0xe0 && lead <= 0xef) { scalar = lead & 0x0f; trailing = 2; }
        else if (lead >= 0xf0 && lead <= 0xf4) { scalar = lead & 0x07; trailing = 3; }
        else if (lead >= 0x80) return false;
        if (at + static_cast<std::size_t>(trailing) > text.size()) return false;
        for (int n = 0; n < trailing; ++n) {
            const auto next = static_cast<unsigned char>(text[at++]);
            if ((next & 0xc0) != 0x80) return false;
            scalar = (scalar << 6) | (next & 0x3f);
        }
        if ((trailing == 1 && scalar < 0x80) || (trailing == 2 && scalar < 0x800) ||
            (trailing == 3 && scalar < 0x10000) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff)) return false;
        if (scalar < 0x20 || (scalar >= 0x7f && scalar <= 0x9f) ||
            scalar == 0x2028 || scalar == 0x2029 || (scalar >= 0x202a && scalar <= 0x202e) ||
            (scalar >= 0x2066 && scalar <= 0x2069)) return false;
        // Directional marks can change displayed label order despite occupying
        // an ordinary FTXUI cell. Keep ZWJ and other unrelated format characters.
        if (scalar == 0x061c || scalar == 0x200e || scalar == 0x200f) return false;
        // Mermaid also recognizes #name; and #digits; entities. Reject the
        // introducer conservatively, including fragments before a sequence
        // statement separator, rather than drawing an entity as literal text.
        if (scalar == '<' || scalar == '>' || scalar == '&' || scalar == '#' || scalar == '`') return false;
    }
    return true;
}

std::optional<std::string> validated_label(std::string_view text) {
    if (trim(text).empty() || !plain_unicode(text)) return {};
    const auto result = std::string(text);
    const auto cells = ftxui::Utf8ToGlyphs(result);
    if (cells.empty() || cells.size() > kLabelLimit || ftxui::string_width(result) > kLabelLimit) return {};
    // A leading combining mark has no display cell and would lose information.
    std::string round_trip;
    for (const auto& glyph : cells) round_trip += glyph;
    if (round_trip != result) return {};
    return result;
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
    return validated_label(input);
}

std::optional<std::string> sequence_label(std::string_view input) {
    input = trim(input);
    // Sequence TXT has its own quoting and wrapping semantics. This subset
    // admits plain text only, without borrowing flowchart string decoding.
    if (input.find_first_of("\"\\") != std::string_view::npos ||
        input.starts_with("wrap:") || input.starts_with("nowrap:") ||
        input.starts_with(":wrap:") || input.starts_with(":nowrap:")) return {};
    return validated_label(input);
}

bool sequence_id(std::string_view id) {
    // sequenceDiagram.jison's lexer is case-insensitive. Conservatively keep
    // its declaration/control/metadata words out of both explicit and implicit
    // participant IDs; label text is checked separately and remains unaffected.
    // https://github.com/mermaid-js/mermaid/blob/develop/packages/mermaid/src/diagrams/sequence/parser/sequenceDiagram.jison
    constexpr std::string_view reserved[]{"sequencediagram", "participant", "actor", "as",
        "create", "destroy", "box", "loop", "rect", "opt", "alt", "else", "par",
        "par_over", "and", "critical", "option", "break", "end", "over", "note",
        "activate", "deactivate", "title", "acctitle", "accdescr", "autonumber",
        "off", "links", "link", "properties", "details"};
    std::string folded;
    folded.reserve(id.size());
    for (const auto ch : id) folded += ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch + ('a' - 'A')) : ch;
    return std::ranges::find(reserved, folded) == std::end(reserved);
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
        if (rest.empty() || !id_start(rest.front())) return {};
        std::size_t length = 1;
        while (length < rest.size() && id_char(rest[length])) ++length;
        auto result = std::string(rest.substr(0, length));
        rest.remove_prefix(length);
        return result;
    }
    std::optional<std::string> enclosed(char close) {
        bool quoted = false;
        bool escaped = false;
        for (std::size_t at = 0; at < rest.size(); ++at) {
            const auto ch = rest[at];
            if (escaped) { escaped = false; continue; }
            if (quoted && ch == '\\') { escaped = true; continue; }
            if (ch == '"') { quoted = !quoted; continue; }
            if (!quoted && ch == close) {
                auto result = label(rest.substr(0, at), true);
                rest.remove_prefix(at + 1);
                return result;
            }
        }
        return {};
    }
};

enum class Direction { Down, Up, Right, Left };
enum class Shape { Rectangle, Rounded, Decision };
struct Vertex {
    std::string id;
    std::string text;
    Shape shape = Shape::Rectangle;
    bool explicit_label = false;
};
struct Edge { std::size_t from; std::size_t to; std::optional<std::string> text; };
struct Message { std::size_t from; std::size_t to; std::string text; bool dashed = false; };
struct Diagram {
    bool sequence = false;
    Direction direction = Direction::Down;
    std::vector<Vertex> vertices;
    std::vector<Edge> edges;
    std::vector<Message> messages;
    std::optional<std::size_t> vertex(std::string id) {
        if (sequence && !sequence_id(id)) return {};
        for (std::size_t n = 0; n < vertices.size(); ++n)
            if (vertices[n].id == id) return n;
        if (vertices.size() >= (sequence ? 32u : 64u)) return {};
        vertices.push_back({id, id, Shape::Rectangle, false});
        return vertices.size() - 1;
    }
};

std::optional<std::size_t> flow_node(Scanner& scanner, Diagram& graph) {
    auto id = scanner.id();
    if (!id) return {};
    // Unsupported structural keywords must not turn into ordinary nodes.
    constexpr std::string_view reserved[]{"end", "subgraph", "direction", "style",
        "linkStyle", "classDef", "class", "click"};
    if (std::ranges::find(reserved, *id) != std::end(reserved)) return {};
    auto index = graph.vertex(std::move(*id));
    if (!index) return {};
    scanner.space();
    if (scanner.rest.empty()) return index;
    const auto open = scanner.rest.front();
    if (open != '[' && open != '(' && open != '{') return index;
    scanner.rest.remove_prefix(1);
    const auto shape = open == '[' ? Shape::Rectangle : open == '(' ? Shape::Rounded : Shape::Decision;
    auto text = scanner.enclosed(open == '[' ? ']' : open == '(' ? ')' : '}');
    if (!text) return {};
    auto& node = graph.vertices[*index];
    if (node.explicit_label && (node.text != *text || node.shape != shape)) return {};
    node.text = std::move(*text);
    node.shape = shape;
    node.explicit_label = true;
    return index;
}

bool flow_statement(std::string_view text, Diagram& graph) {
    Scanner scanner{text};
    auto from = flow_node(scanner, graph);
    if (!from) return false;
    scanner.space();
    while (!scanner.rest.empty()) {
        if (!scanner.take("-->")) return false;
        std::optional<std::string> edge_label;
        if (scanner.take("|")) {
            edge_label = scanner.enclosed('|');
            if (!edge_label) return false;
        }
        auto to = flow_node(scanner, graph);
        if (!to || graph.edges.size() >= 128) return false;
        graph.edges.push_back({*from, *to, std::move(edge_label)});
        from = to;
        scanner.space();
    }
    return true;
}

bool sequence_statement(std::string_view text, Diagram& graph) {
    Scanner scanner{text};
    if (scanner.rest.starts_with("participant ") || scanner.rest.starts_with("participant\t")) {
        scanner.take("participant");
        auto id = scanner.id();
        if (!id) return false;
        auto index = graph.vertex(std::move(*id));
        if (!index) return false;
        scanner.space();
        if (scanner.rest.empty()) return true;
        if (!scanner.take("as ") && !scanner.take("as\t")) return false;
        auto alias = sequence_label(scanner.rest);
        if (!alias) return false;
        auto& participant = graph.vertices[*index];
        if (participant.explicit_label && participant.text != *alias) return false;
        participant.text = std::move(*alias);
        participant.explicit_label = true;
        return true;
    }
    auto from_id = scanner.id();
    if (!from_id) return false;
    scanner.space();
    const bool dashed = scanner.take("-->>");
    if (!dashed && !scanner.take("->>")) return false;
    auto to_id = scanner.id();
    scanner.space();
    if (!to_id || !scanner.take(":")) return false;
    auto message = sequence_label(scanner.rest);
    if (!message || graph.messages.size() >= 128) return false;
    const auto from = graph.vertex(std::move(*from_id));
    const auto to = graph.vertex(std::move(*to_id));
    if (!from || !to) return false;
    graph.messages.push_back({*from, *to, std::move(*message), dashed});
    return true;
}

std::optional<Diagram> parse(std::string_view source) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    for (const auto byte : source) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < 0x20 && value != '\n' && value != '\r' && value != '\t') return {};
    }
    Diagram diagram;
    bool have_header = false;
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
            if (!quoted && !diagram.sequence && have_header) {
                if (close && ch == close) close = 0;
                else if (!close && (ch == '[' || ch == '(' || ch == '{' || ch == '|'))
                    close = ch == '[' ? ']' : ch == '(' ? ')' : ch == '{' ? '}' : '|';
            }
            if (ch != '\0' && (ch != ';' || close || quoted)) continue;
            if (quoted || close || escaped) return {};
            const auto statement = trim(line.substr(start, at - start));
            start = at + 1;
            if (statement.empty()) continue;
            if (!have_header) {
                if (statement == "sequenceDiagram") diagram.sequence = true;
                else {
                    Scanner header{statement};
                    auto kind = header.id();
                    if (!kind || (*kind != "graph" && *kind != "flowchart")) return {};
                    if (header.rest.empty() || (header.rest.front() != ' ' && header.rest.front() != '\t')) return {};
                    header.space();
                    if (header.rest == "TD" || header.rest == "TB") diagram.direction = Direction::Down;
                    else if (header.rest == "BT") diagram.direction = Direction::Up;
                    else if (header.rest == "LR") diagram.direction = Direction::Right;
                    else if (header.rest == "RL") diagram.direction = Direction::Left;
                    else return {};
                }
                have_header = true;
            } else {
                if (!(diagram.sequence ? sequence_statement(statement, diagram) : flow_statement(statement, diagram))) return {};
                have_statement = true;
            }
        }
    }
    if (!have_header || !have_statement || diagram.vertices.empty()) return {};
    return diagram;
}

struct Point {
    int x = 0;
    int y = 0;
    bool operator==(const Point&) const = default;
};
Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
constexpr Point kSteps[]{{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
constexpr unsigned kRight = 1, kDown = 2, kLeft = 4, kUp = 8;
unsigned direction_bits(Point delta) {
    if (delta.x > 0) return kRight;
    if (delta.x < 0) return kLeft;
    return delta.y > 0 ? kDown : kUp;
}
std::string_view line_glyph(unsigned bits) {
    switch (bits) {
        case kRight: case kLeft: case kRight | kLeft: return "─";
        case kDown: case kUp: case kDown | kUp: return "│";
        case kRight | kDown: return "┌";
        case kDown | kLeft: return "┐";
        case kLeft | kUp: return "┘";
        case kUp | kRight: return "└";
        case kRight | kDown | kLeft: return "┬";
        case kDown | kLeft | kUp: return "┤";
        case kLeft | kUp | kRight: return "┴";
        case kUp | kRight | kDown: return "├";
        default: return "┼";
    }
}
std::string_view arrow(Point delta) {
    return delta.x > 0 ? "▶" : delta.x < 0 ? "◀" : delta.y > 0 ? "▼" : "▲";
}

struct Cell {
    std::string glyph = " ";
    unsigned bits = 0;
    bool blocked = false;
};
struct Canvas {
    int width;
    int height;
    Work& work;
    std::vector<Cell> cells;
    Canvas(int w, int h, Work& budget) : width(w), height(h), work(budget), cells(static_cast<std::size_t>(w * h)) {}
    bool inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; }
    int index(Point p) const { return p.y * width + p.x; }
    Cell& at(Point p) { return cells[static_cast<std::size_t>(index(p))]; }
    const Cell& at(Point p) const { return cells[static_cast<std::size_t>(index(p))]; }
    bool free(Point p) const { return work.spend() && inside(p) && !at(p).blocked; }
    bool put(Point p, std::string_view glyph, bool block = true) {
        if (!inside(p) || !work.spend()) return false;
        at(p).glyph = glyph;
        at(p).blocked = block;
        return true;
    }
    bool text(Point p, std::string_view value) {
        for (const auto& glyph : ftxui::Utf8ToGlyphs(std::string(value))) {
            if (!put(p, glyph)) return false;
            ++p.x;
        }
        return true;
    }
    bool segment(Point from, Point to, bool dashed = false) {
        if (from.x != to.x && from.y != to.y) return false;
        const Point step{(to.x > from.x) - (to.x < from.x), (to.y > from.y) - (to.y < from.y)};
        for (Point p = from;; p = p + step) {
            if (!inside(p) || !work.spend()) return false;
            auto& cell = at(p);
            cell.blocked = true;
            if (p != from) cell.bits |= direction_bits(Point{} - step);
            if (p != to) cell.bits |= direction_bits(step);
            if (dashed && (cell.bits == (kLeft | kRight) || cell.bits == kLeft || cell.bits == kRight)) cell.glyph = "┄";
            else if (dashed && (cell.bits == (kUp | kDown) || cell.bits == kUp || cell.bits == kDown)) cell.glyph = "┆";
            else cell.glyph = line_glyph(cell.bits);
            if (p == to) break;
        }
        return true;
    }
    std::vector<std::string> lines() {
        int x_min = width, y_min = height, x_max = -1, y_max = -1;
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            if (!work.spend()) return {};
            if (at({x, y}).glyph != " ") {
                x_min = std::min(x_min, x); x_max = std::max(x_max, x);
                y_min = std::min(y_min, y); y_max = std::max(y_max, y);
            }
        }
        if (x_max < x_min || y_max < y_min) return {};
        std::vector<std::string> result;
        for (int y = y_min; y <= y_max; ++y) {
            std::string line;
            for (int x = x_min; x <= x_max; ++x) {
                if (!work.spend()) return {};
                line += at({x, y}).glyph;
            }
            result.push_back(std::move(line));
        }
        return result;
    }
};

struct Box { int x; int y; int width; int height = 5; };
bool node_box(Canvas& canvas, const Box& box, const Vertex& node) {
    for (int y = 0; y < box.height; ++y) for (int x = 0; x < box.width; ++x)
        if (!canvas.put({box.x + x, box.y + y}, " ")) return false;
    const auto top_left = node.shape == Shape::Rounded ? "╭" : "┌";
    const auto top_right = node.shape == Shape::Rounded ? "╮" : "┐";
    const auto bottom_left = node.shape == Shape::Rounded ? "╰" : "└";
    const auto bottom_right = node.shape == Shape::Rounded ? "╯" : "┘";
    if (node.shape == Shape::Decision) {
        if (!canvas.put({box.x + 2, box.y}, "╱") || !canvas.put({box.x + box.width - 3, box.y}, "╲") ||
            !canvas.put({box.x + 2, box.y + 4}, "╲") || !canvas.put({box.x + box.width - 3, box.y + 4}, "╱")) return false;
        for (int x = 3; x < box.width - 3; ++x)
            if (!canvas.put({box.x + x, box.y}, "─") || !canvas.put({box.x + x, box.y + 4}, "─")) return false;
        if (!canvas.put({box.x + 1, box.y + 1}, "╱") || !canvas.put({box.x + box.width - 2, box.y + 1}, "╲") ||
            !canvas.put({box.x + 1, box.y + 3}, "╲") || !canvas.put({box.x + box.width - 2, box.y + 3}, "╱") ||
            !canvas.put({box.x, box.y + 2}, "◇") || !canvas.put({box.x + box.width - 1, box.y + 2}, "◇")) return false;
    } else {
        for (int x = 1; x < box.width - 1; ++x)
            if (!canvas.put({box.x + x, box.y}, "─") || !canvas.put({box.x + x, box.y + box.height - 1}, "─")) return false;
        for (int y = 1; y < box.height - 1; ++y)
            if (!canvas.put({box.x, box.y + y}, "│") || !canvas.put({box.x + box.width - 1, box.y + y}, "│")) return false;
        if (!canvas.put({box.x, box.y}, top_left) || !canvas.put({box.x + box.width - 1, box.y}, top_right) ||
            !canvas.put({box.x, box.y + box.height - 1}, bottom_left) ||
            !canvas.put({box.x + box.width - 1, box.y + box.height - 1}, bottom_right)) return false;
    }
    return canvas.text({box.x + (box.width - ftxui::string_width(node.text)) / 2, box.y + box.height / 2}, node.text);
}

std::optional<std::vector<std::string>> sequence_layout(const Diagram& diagram, Work& work) {
    std::vector<Box> boxes;
    std::vector<int> centers;
    int longest = 0;
    bool last_self = false;
    int height = 5;
    for (const auto& message : diagram.messages) {
        longest = std::max(longest, ftxui::string_width(message.text));
        height += message.from == message.to ? 6 : 4;
        last_self |= message.from == message.to && message.from + 1 == diagram.vertices.size();
    }
    const auto gap = std::max(7, longest + 4);
    int width = 0;
    for (const auto& participant : diagram.vertices) {
        const auto node_width = std::max(7, ftxui::string_width(participant.text) + 4);
        boxes.push_back({width, 0, node_width, 3});
        centers.push_back(width + node_width / 2);
        width += node_width + gap;
    }
    width -= gap;
    if (last_self) width = std::max(width, centers.back() + longest + 6);
    if (width <= 0 || width > kWidthLimit || height > kHeightLimit ||
        !work.spend(static_cast<std::size_t>(width * height))) return {};
    Canvas canvas(width, height, work);
    for (std::size_t n = 0; n < boxes.size(); ++n) {
        if (!node_box(canvas, boxes[n], diagram.vertices[n]) ||
            !canvas.segment({centers[n], 3}, {centers[n], height - 1})) return {};
    }
    int y = 5;
    for (const auto& message : diagram.messages) {
        const auto from = centers[message.from], to = centers[message.to];
        const auto label_x = std::min(from, to) + 2;
        if (!canvas.text({label_x, y}, message.text)) return {};
        if (from == to) {
            const auto turn = from + std::max(4, ftxui::string_width(message.text) + 3);
            if (turn >= width || !canvas.segment({from, y + 1}, {turn, y + 1}, message.dashed) ||
                !canvas.segment({turn, y + 1}, {turn, y + 3}, message.dashed) ||
                !canvas.segment({turn, y + 3}, {from, y + 3}, message.dashed) ||
                !canvas.put({from, y + 3}, "◀")) return {};
            y += 6;
        } else {
            if (!canvas.segment({from, y + 1}, {to, y + 1}, message.dashed) ||
                !canvas.put({to, y + 1}, arrow({to - from, 0}))) return {};
            y += 4;
        }
    }
    auto lines = canvas.lines();
    if (lines.empty()) return {};
    return lines;
}

// Ranking ignores DFS back edges only in its temporary DAG. All original
// edges, including back edges and self loops, still go through the router.
std::vector<int> ranks(const Diagram& graph) {
    const auto count = graph.vertices.size();
    std::vector<std::vector<std::size_t>> children(count), dag(count);
    for (const auto& edge : graph.edges) if (edge.from != edge.to) children[edge.from].push_back(edge.to);
    std::vector<int> color(count), order;
    for (std::size_t root = 0; root < count; ++root) {
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
            const auto target = children[current][next++];
            if (color[target] == 1) continue;
            dag[current].push_back(target);
            if (color[target] == 0) { color[target] = 1; stack.emplace_back(target, 0); }
        }
    }
    std::vector<int> result(count);
    for (auto at = order.rbegin(); at != order.rend(); ++at)
        for (const auto target : dag[static_cast<std::size_t>(*at)])
            result[target] = std::max(result[target], result[static_cast<std::size_t>(*at)] + 1);
    return result;
}

std::optional<std::vector<Box>> place_nodes(const Diagram& graph) {
    const auto rank = ranks(graph);
    const auto last = *std::max_element(rank.begin(), rank.end());
    std::vector<std::vector<std::size_t>> layers(static_cast<std::size_t>(last + 1));
    for (std::size_t n = 0; n < rank.size(); ++n) layers[static_cast<std::size_t>(rank[n])].push_back(n);
    std::vector<Box> boxes(graph.vertices.size(), {0, 0, 0, 5});
    const bool horizontal = graph.direction == Direction::Right || graph.direction == Direction::Left;
    int cross_extent = 0;
    for (const auto& layer : layers) {
        int extent = -6;
        for (const auto n : layer) {
            boxes[n].width = std::max(graph.vertices[n].shape == Shape::Decision ? 11 : 9,
                ftxui::string_width(graph.vertices[n].text) + 4);
            extent += (horizontal ? boxes[n].height : boxes[n].width) + 6;
        }
        cross_extent = std::max(cross_extent, extent);
    }
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
        major += major_size + 8;
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

struct Port { Point border; Point outside; Point normal; };
int distance(Point a, Point b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
std::vector<Port> ports(const Box& box, Shape shape, Point target) {
    std::vector<Port> result;
    const int inset = shape == Shape::Decision ? 3 : 1;
    for (int x = inset; x < box.width - inset; ++x) {
        result.push_back({{box.x + x, box.y}, {box.x + x, box.y - 1}, {0, -1}});
        result.push_back({{box.x + x, box.y + box.height - 1}, {box.x + x, box.y + box.height}, {0, 1}});
    }
    for (int y = 1; y < box.height - 1; ++y) {
        if (shape == Shape::Decision && y != 2) continue;
        result.push_back({{box.x, box.y + y}, {box.x - 1, box.y + y}, {-1, 0}});
        result.push_back({{box.x + box.width - 1, box.y + y}, {box.x + box.width, box.y + y}, {1, 0}});
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

class Router {
    Canvas& canvas_;
    std::vector<int> distance_, parent_;
    std::vector<unsigned> seen_;
    unsigned search_ = 0;
public:
    explicit Router(Canvas& canvas) : canvas_(canvas), distance_(canvas.cells.size()), parent_(canvas.cells.size()), seen_(canvas.cells.size()) {}
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
                if (!canvas_.free(next)) continue;
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

bool draw_path(Canvas& canvas, const std::vector<Point>& path, Point before, Point after, bool endpoint_arrow) {
    if (path.empty()) return false;
    for (std::size_t n = 0; n < path.size(); ++n) {
        const auto prior = n ? path[n - 1] : before;
        const auto next = n + 1 < path.size() ? path[n + 1] : after;
        const auto bits = direction_bits(prior - path[n]) | direction_bits(next - path[n]);
        if (!canvas.put(path[n], endpoint_arrow && n + 1 == path.size() ? arrow(after - path[n]) : line_glyph(bits))) return false;
    }
    return true;
}

bool source_port(Canvas& canvas, const Port& port) {
    auto bits = port.normal.x ? kUp | kDown : kLeft | kRight;
    bits |= direction_bits(port.normal);
    if (canvas.at(port.border).glyph == "◇") return true;
    return canvas.put(port.border, line_glyph(bits));
}

bool connect(Canvas& canvas, Router& router, const Port& from, const Port& to,
             const std::optional<std::string>& label_text) {
    const auto goal = to.outside + to.normal;
    if (from.border == to.border || !canvas.free(from.outside) || !canvas.free(to.outside) || !canvas.free(goal)) return false;
    canvas.at(to.outside).blocked = true;
    if (!label_text) {
        auto path = router.path(from.outside, goal);
        canvas.at(to.outside).blocked = false;
        if (!path) return false;
        path->push_back(to.outside);
        return draw_path(canvas, *path, from.border, to.border, true) && source_port(canvas, from);
    }
    const auto length = ftxui::string_width(*label_text);
    const Point ideal{(from.outside.x + goal.x - length) / 2, (from.outside.y + goal.y) / 2};
    // Try nearby label positions without an unbounded search over the plane.
    for (int radius = 0; radius < 32 && canvas.work.remaining; ++radius) {
        const Point offsets[]{{0, radius}, {0, -radius}, {radius, 0}, {-radius, 0}};
        for (const auto offset : offsets) {
            const auto position = ideal + offset;
            const Point left{position.x - 1, position.y}, right{position.x + length, position.y};
            bool free = canvas.free(left) && canvas.free(right) && left != from.outside && right != from.outside &&
                left != goal && right != goal;
            for (int x = 0; free && x < length; ++x) {
                if (!canvas.work.spend()) { free = false; break; }
                const Point cell{position.x + x, position.y};
                free = canvas.free(cell) && cell != from.outside && cell != goal;
            }
            if (!free) continue;
            if (!canvas.work.spend(static_cast<std::size_t>(length) * 2)) break;
            for (int x = 0; x < length; ++x) canvas.at({position.x + x, position.y}).blocked = true;
            auto first = router.path(from.outside, left);
            if (first && !canvas.work.spend(first->size() * 2)) {
                for (int x = 0; x < length; ++x) canvas.at({position.x + x, position.y}).blocked = false;
                canvas.at(to.outside).blocked = false;
                return false;
            }
            if (first) for (const auto cell : *first) canvas.at(cell).blocked = true;
            auto second = first ? router.path(right, goal) : std::nullopt;
            if (first) for (const auto cell : *first) canvas.at(cell).blocked = false;
            for (int x = 0; x < length; ++x) canvas.at({position.x + x, position.y}).blocked = false;
            if (!first || !second) continue;
            canvas.at(to.outside).blocked = false;
            second->push_back(to.outside);
            return draw_path(canvas, *first, from.border, position, false) &&
                draw_path(canvas, *second, {right.x - 1, right.y}, to.border, true) &&
                canvas.text(position, *label_text) && source_port(canvas, from);
        }
    }
    canvas.at(to.outside).blocked = false;
    return false;
}

std::optional<std::vector<std::string>> flow_layout(const Diagram& graph, Work& work) {
    auto placed = place_nodes(graph);
    if (!placed) return {};
    int width = 0, height = 0, longest = 0;
    for (const auto& box : *placed) { width = std::max(width, box.x + box.width); height = std::max(height, box.y + box.height); }
    for (const auto& edge : graph.edges) if (edge.text) longest = std::max(longest, ftxui::string_width(*edge.text));
    if (longest + 4 > kWidthLimit) return {};
    width = std::min(kWidthLimit, width + 12 + 2 * longest + static_cast<int>(graph.edges.size()) * 2);
    height = std::min(kHeightLimit, height + 12 + static_cast<int>(graph.edges.size()) * 3);
    if (!work.spend(static_cast<std::size_t>(width * height) * 4)) return {};
    Canvas canvas(width, height, work);
    for (std::size_t n = 0; n < placed->size(); ++n)
        if (!node_box(canvas, (*placed)[n], graph.vertices[n])) return {};
    Router router(canvas);
    std::unordered_set<int> used_ports;
    for (const auto& edge : graph.edges) {
        const auto& from = (*placed)[edge.from];
        const auto& to = (*placed)[edge.to];
        const auto sources = ports(from, graph.vertices[edge.from].shape, {to.x + to.width / 2, to.y + to.height / 2});
        const auto targets = ports(to, graph.vertices[edge.to].shape, {from.x + from.width / 2, from.y + from.height / 2});
        bool connected = false;
        for (const auto& source : sources) {
            if (used_ports.contains(canvas.index(source.border))) continue;
            for (const auto& target : targets) {
                if (!work.remaining) return {};
                if (used_ports.contains(canvas.index(target.border))) continue;
                if (!connect(canvas, router, source, target, edge.text)) continue;
                used_ports.insert(canvas.index(source.border));
                used_ports.insert(canvas.index(target.border));
                connected = true;
                break;
            }
            if (connected) break;
        }
        if (!connected) return {};
    }
    auto lines = canvas.lines();
    if (lines.empty()) return {};
    return lines;
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

} // namespace

std::optional<ftxui::Element> render_mermaid(std::string_view source, ftxui::Element fallback) {
    if (!fallback) return {};
    auto diagram = parse(source);
    if (!diagram) return {};
    Work work;
    auto lines = diagram->sequence ? sequence_layout(*diagram, work) : flow_layout(*diagram, work);
    if (!lines || lines->empty()) return {};
    int width = 0;
    ftxui::Elements rows;
    for (auto& line : *lines) {
        width = std::max(width, ftxui::string_width(line));
        rows.push_back(ftxui::text(std::move(line)));
    }
    if (width > kWidthLimit || rows.size() > kHeightLimit) return {};
    return std::make_shared<MermaidNode>(ftxui::vbox(std::move(rows)), std::move(fallback), width);
}

} // namespace loom::ui::mermaid
