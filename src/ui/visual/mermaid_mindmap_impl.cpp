module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;
import :detail;

namespace loom::ui::mermaid::detail {
namespace {

struct NodeSpan {
    std::string_view label;
    int indent = 0;
    int depth = 0;
    int parent = -1;
    bool square = false;
};
struct Node {
    std::string label;
};
struct CaptionRow {
    std::string_view text;
    int cells = 0;
};

// All size arithmetic crosses this checked boundary before a prepaid stage.
bool mindmap_add(std::size_t& total, std::size_t value) {
    if (value > std::numeric_limits<std::size_t>::max() - total) return false;
    total += value;
    return true;
}
bool mindmap_term(std::size_t& total, std::size_t count, std::size_t rate) {
    if (rate != 0 && count > std::numeric_limits<std::size_t>::max() / rate) return false;
    return mindmap_add(total, count * rate);
}

// Views only: CRLF is accepted, but the entry scan rejects every bare CR.
bool mindmap_line(std::string_view source, std::size_t& at, std::string_view& line) {
    if (at >= source.size()) return false;
    const auto end = source.find('\n', at);
    const auto stop = end == std::string_view::npos ? source.size() : end;
    line = source.substr(at, stop - at);
    at = end == std::string_view::npos ? source.size() : end + 1;
    if (line.ends_with('\r')) line.remove_suffix(1);
    return true;
}

// Validate the closed metadata grammar before asking the shared reader to
// allocate anything. Metadata never substitutes text into the unchanged body.
bool mindmap_description(std::string_view value) {
    if (value.empty() || value.find_first_not_of(' ') == std::string_view::npos ||
        !unicode_text(value)) return false;
    if (value.front() == '\'') {
        if (value.size() < 2 || value.back() != '\'') return false;
        for (std::size_t i = 1; i + 1 < value.size(); ++i) {
            if (value[i] != '\'') continue;
            if (i + 2 >= value.size() || value[i + 1] != '\'') return false;
            ++i;
        }
        return true;
    }
    if (std::string_view(" \"|>[]{}&*!%@`#").find(value.front()) != std::string_view::npos ||
        value.starts_with("- ") || value.starts_with("? ") || value.starts_with(": "))
        return false;
    return value.find(": ") == std::string_view::npos &&
           value.find(" #") == std::string_view::npos;
}

std::optional<std::string_view> mindmap_body(std::string_view source, Work& work) {
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(source, at, line)) return {};
    if (line != "---") return source;
    bool seen = false, folded = false, payload = false, closed = false;
    std::size_t block_indent = 0;
    while (mindmap_line(source, at, line)) {
        if (line == "---") { closed = true; break; }
        if (line.find_first_not_of(' ') == std::string_view::npos) continue;
        if (folded && line.front() == ' ') {
            const auto indent = line.find_first_not_of(' ');
            if (block_indent == 0) block_indent = indent;
            if (indent != block_indent || !unicode_text(line.substr(indent))) return {};
            payload = true;
            continue;
        }
        if (seen || !line.starts_with("description: ")) return {};
        seen = true;
        const auto value = line.substr(13);
        if (value == ">-") folded = true;
        else if (!mindmap_description(value)) return {};
    }
    if (!closed || !seen || (folded && !payload)) return {};
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter || !envelope->description ||
        envelope->title || !envelope->config.empty() || !envelope->init.empty()) return {};
    std::size_t fields = 0;
    for (const auto& event : envelope->events) {
        if (!work.spend()) return {};
        if (event.blank) continue;
        if (!event.top_level || event.comment || event.path != "description" || ++fields != 1)
            return {};
    }
    if (fields != 1) return {};
    return envelope->raw_body;
}

bool mindmap_id(std::string_view id) {
    const auto letter = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    };
    if (id.empty() || !letter(id.front())) return false;
    for (const char c : id)
        if (!letter(c) && !(c >= '0' && c <= '9') && c != '-') return false;
    return true;
}

bool mindmap_plain(std::string_view label) {
    // Shape, directive, Markdown, HTML and statement punctuation is syntax,
    // never a caption fallback. Ordinary in-word hyphens remain plain text.
    if (label.empty() || label.back() == ' ' ||
        label.find_first_of("[](){}<>`\"'\\|;:#%*_&$") != std::string_view::npos ||
        label.starts_with("- ") || label.starts_with("+ ") ||
        label.find("--") != std::string_view::npos) return false;
    return true;
}

// Rows are views into the stable validated label, not copied word strings.
// FTXUI's second wide-cell slot stays attached even if it holds a combining
// character. The final partial long-word chunk remains open for the next word.
bool mindmap_wrap(const std::string& label, const std::vector<std::string>& glyphs,
                  std::vector<CaptionRow>& rows, Work& work) {
    rows.reserve(glyphs.size());
    std::size_t row_begin = 0, row_end = 0, byte = 0, i = 0;
    int row_cells = 0;
    const auto flush = [&]() {
        if (row_cells == 0 || rows.size() >= 256 || !work.spend(4)) return false;
        rows.push_back({std::string_view(label).substr(row_begin, row_end - row_begin), row_cells});
        row_cells = 0;
        return true;
    };
    while (i < glyphs.size()) {
        if (!work.spend()) return false;
        // Only an actual standalone ASCII space is a word boundary; a space
        // carrying combining ink stays with its indivisible FTXUI glyph.
        if (glyphs[i] == " ") { ++byte; ++i; continue; }
        const auto word_begin = byte;
        const auto first = i;
        while (i < glyphs.size() && glyphs[i] != " ") {
            if (!work.spend()) return false;
            byte += glyphs[i].size();
            ++i;
        }
        const auto word_cells = i - first;
        if (word_cells <= 32) {
            const int gap = row_cells == 0 ? 0 : 1;
            if (row_cells + gap + static_cast<int>(word_cells) > 32 && !flush()) return false;
            if (row_cells == 0) row_begin = word_begin;
            else ++row_cells;
            row_end = byte;
            row_cells += static_cast<int>(word_cells);
            continue;
        }
        if (row_cells != 0 && !flush()) return false;
        std::size_t chunk_byte = word_begin;
        for (auto g = first; g < i;) {
            if (!work.spend(glyphs[g].size() + 2)) return false;
            const auto width = ftxui::string_width(glyphs[g]);
            const std::size_t slots = width == 2 ? 2u : 1u;
            if (width < 1 || width > 2 || g + slots > i) return false;
            if (row_cells + static_cast<int>(slots) > 32 && !flush()) return false;
            if (row_cells == 0) row_begin = chunk_byte;
            for (std::size_t n = 0; n < slots; ++n) chunk_byte += glyphs[g + n].size();
            row_end = chunk_byte;
            row_cells += static_cast<int>(slots);
            g += slots;
        }
    }
    return row_cells != 0 && flush();
}

} // namespace

std::optional<std::vector<std::string>> mindmap_original_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!mindmap_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' || (source[i] == '\r' &&
            (i + 1 == source.size() || source[i + 1] != '\n'))) return {};
    }
    const auto body = mindmap_body(source, work);
    if (!body) return {};
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(*body, at, line) || line != "mindmap") return {};

    fee = 1024;
    if (!mindmap_term(fee, 64, 2 * sizeof(NodeSpan) + 2 * sizeof(Node) + 32) ||
        !mindmap_term(fee, 16, 4 * sizeof(int)) || !work.spend(fee)) return {};
    std::array<NodeSpan, 64> spans{};
    std::array<int, 16> indents{}, ancestors{};
    std::size_t count = 0;
    int levels = 0;
    while (mindmap_line(*body, at, line)) {
        if (line.empty()) continue;
        const auto leading = line.find_first_not_of(' ');
        if (leading == std::string_view::npos) continue;
        if (count == spans.size()) return {};
        const int indent = static_cast<int>(leading); // source is bounded to 64 KiB
        auto label = line.substr(leading);
        bool square = false;
        const auto bracket = label.find('[');
        if (bracket != std::string_view::npos) {
            if (count != 0 || !label.ends_with(']') || !mindmap_id(label.substr(0, bracket))) return {};
            square = true;
            label = label.substr(bracket + 1, label.size() - bracket - 2);
        }
        if ((count != 0 && spans[0].square) || label.empty() || label.size() > 1024) return {};
        int parent = -1;
        if (count == 0) {
            if (!work.spend(16)) return {};
            levels = 1;
            indents[0] = indent;
        } else {
            if (!work.spend(16)) return {};
            if (indent > indents[static_cast<std::size_t>(levels - 1)]) {
                if (levels == 16 || !work.spend(16)) return {};
                parent = ancestors[static_cast<std::size_t>(levels - 1)];
                indents[static_cast<std::size_t>(levels++)] = indent;
            } else {
                while (levels > 0) {
                    if (!work.spend(16)) return {};
                    if (indent >= indents[static_cast<std::size_t>(levels - 1)]) break;
                    --levels;
                }
                if (levels <= 1 || !work.spend(16) ||
                    indent != indents[static_cast<std::size_t>(levels - 1)] ||
                    !work.spend(16)) return {};
                parent = ancestors[static_cast<std::size_t>(levels - 2)];
            }
        }
        spans[count] = {label, indent, levels - 1, parent, square};
        ancestors[static_cast<std::size_t>(levels - 1)] = static_cast<int>(count++);
    }
    if (count == 0) return {};

    // No label string or glyph allocation precedes this complete byte sum.
    std::size_t label_bytes = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!work.spend() || spans[i].label.size() > 1024 ||
            !mindmap_add(label_bytes, spans[i].label.size())) return {};
    }
    fee = 0;
    if (label_bytes > source.size() ||
        !mindmap_term(fee, label_bytes, 8 * sizeof(std::string) + 64) ||
        !mindmap_term(fee, count, 512) || !work.spend(fee)) return {};
    std::array<Node, 64> nodes{};
    std::vector<CaptionRow> rows;
    int width = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!work.spend(spans[i].label.size() + 16)) return {};
        // Token-set and repeated-space scans belong to the fully prepaid L
        // stage, not the cheaper raw lexeme-span collection pass.
        if (!mindmap_plain(spans[i].label) ||
            (spans[i].square && (spans[i].label.front() == ' ' ||
             spans[i].label.find("  ") != std::string_view::npos))) return {};
        auto label = validated_plain_label(spans[i].label);
        if (!label) return {};
        nodes[i].label = std::move(*label);
        const auto glyphs = ftxui::Utf8ToGlyphs(nodes[i].label);
        const auto cells = ftxui::string_width(nodes[i].label);
        if (cells < 1 || cells > 256 || glyphs.size() != static_cast<std::size_t>(cells)) return {};
        if (spans[i].square) {
            if (!mindmap_wrap(nodes[i].label, glyphs, rows, work)) return {};
            for (const auto& row : rows) {
                if (!work.spend()) return {};
                width = std::max(width, row.cells + 4);
            }
        } else width = std::max(width, 4 * spans[i].depth + 2 + cells);
    }
    const bool square = spans[0].square;
    const int height = square ? static_cast<int>(rows.size()) + 2 : 2 * static_cast<int>(count) - 1;
    if (width < 1 || width > kWidthLimit || height < 1 || height > kHeightLimit) return {};

    std::size_t visits = 0;
    for (std::size_t i = 1; i < count; ++i) {
        if (!work.spend(16)) return {};
        const auto parent = static_cast<std::size_t>(spans[i].parent);
        if (parent >= i || !mindmap_add(visits, 2 * (i - parent) + 4)) return {};
    }
    if (square) {
        // Four inclusive outline segments, with shared corners visited twice.
        if (!mindmap_term(visits, static_cast<std::size_t>(width + height), 2)) return {};
    }
    fee = 0;
    if (!mindmap_term(fee, visits, 2 * sizeof(Point) + 16) || !work.spend(fee)) return {};
    fee = 1024;
    std::size_t area = 0;
    if (!mindmap_term(area, static_cast<std::size_t>(width), static_cast<std::size_t>(height)) ||
        !mindmap_term(fee, area, 2 * sizeof(Cell) + 16) ||
        !mindmap_term(fee, static_cast<std::size_t>(height), 4 * sizeof(std::string) + 32) ||
        !work.spend(fee)) return {};
    Canvas canvas(width, height, work);
    if (square) {
        if (!canvas.segment({0, 0}, {width - 1, 0}) ||
            !canvas.segment({width - 1, 0}, {width - 1, height - 1}) ||
            !canvas.segment({width - 1, height - 1}, {0, height - 1}) ||
            !canvas.segment({0, height - 1}, {0, 0})) return {};
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (!work.spend(rows[i].text.size()) ||
                !canvas.text({2, static_cast<int>(i) + 1}, rows[i].text)) return {};
    } else {
        for (std::size_t i = 1; i < count; ++i) {
            const auto parent = static_cast<std::size_t>(spans[i].parent);
            const Point start{4 * spans[parent].depth, 2 * static_cast<int>(parent) + 1};
            const Point bend{start.x, 2 * static_cast<int>(i)};
            const Point end{4 * spans[i].depth - 1, bend.y};
            if (!canvas.segment(start, bend) || !canvas.segment(bend, end) || !work.spend(2)) return {};
            auto& above = canvas.at(start);
            above.bits |= kUp;
            above.glyph = line_glyph(above.bits);
            auto& beside = canvas.at(end);
            beside.bits |= kRight;
            beside.glyph = line_glyph(beside.bits);
        }
        for (std::size_t i = 0; i < count; ++i) {
            const Point marker{4 * spans[i].depth, 2 * static_cast<int>(i)};
            if (!canvas.put(marker, "●") || !work.spend(nodes[i].label.size()) ||
                !canvas.text({marker.x + 2, marker.y}, nodes[i].label)) return {};
        }
    }
    // Canvas::lines retains its normal per-cell fee. Prepay the actual UTF-8
    // byte copies as well, including arbitrarily long combined glyph strings.
    std::size_t output_bytes = 0;
    for (const auto& cell : canvas.cells)
        if (!work.spend() || !mindmap_add(output_bytes, cell.glyph.size())) return {};
    if (!work.spend(output_bytes)) return {};
    auto result = canvas.lines();
    if (result.empty()) return {};
    return result;
}

namespace {
enum class MindmapShape { Default, Square, Rounded, Circle };
struct ShapedSpan {
    std::string_view raw, caption;
    MindmapShape shape = MindmapShape::Default;
    int depth = 0, parent = -1;
};
struct ShapedNode {
    std::string caption;
    std::vector<CaptionRow> rows;
    int width = 0, height = 0, left = 0, top = 0;
};

// This grammar adds complete shaped trees, never a partial interpretation of
// icons, configuration, Markdown or a mismatched/unfinished shape declaration.
std::optional<std::vector<std::string>> mindmap_shapes_layout(std::string_view source, Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!mindmap_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i)
        if (source[i] == '\t' || (source[i] == '\r' &&
            (i + 1 == source.size() || source[i + 1] != '\n'))) return {};
    const auto body = mindmap_body(source, work);
    if (!body) return {};
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(*body, at, line) || line != "mindmap") return {};
    fee = 1024;
    if (!mindmap_term(fee, 64, 2 * sizeof(ShapedSpan) + 2 * sizeof(ShapedNode) + 32) ||
        !mindmap_term(fee, 16, 4 * sizeof(int)) || !work.spend(fee)) return {};
    std::array<ShapedSpan, 64> spans{};
    std::array<int, 16> indents{}, ancestors{};
    std::size_t count = 0, label_bytes = 0;
    int levels = 0;
    bool shaped = false;
    while (mindmap_line(*body, at, line)) {
        const auto leading = line.find_first_not_of(' ');
        if (leading == std::string_view::npos) continue;
        if (count == spans.size() || !work.spend(32)) return {};
        const int indent = static_cast<int>(leading);
        const auto declaration = line.substr(leading);
        auto raw = declaration, caption = declaration;
        auto shape = MindmapShape::Default;
        const auto opening = declaration.find_first_of("[(");
        if (opening != std::string_view::npos) {
            if (!mindmap_id(declaration.substr(0, opening))) return {};
            auto value = declaration.substr(opening);
            std::string_view begin, end;
            if (value.starts_with("((")) { begin = "(("; end = "))"; shape = MindmapShape::Circle; }
            else if (value.starts_with('(')) { begin = "("; end = ")"; shape = MindmapShape::Rounded; }
            else { begin = "["; end = "]"; shape = MindmapShape::Square; }
            value.remove_prefix(begin.size());
            if (value.empty()) {
                // Keep the entire authored description, including CRLF/indent,
                // separate from its one nonempty terminal caption line.
                const auto* raw_start = value.data();
                std::string_view payload, closing;
                if (!mindmap_line(*body, at, payload) || !mindmap_line(*body, at, closing)) return {};
                const auto text_start = payload.find_first_not_of(' ');
                const auto close_start = closing.find_first_not_of(' ');
                if (text_start == std::string_view::npos || close_start == std::string_view::npos ||
                    closing.substr(close_start) != end) return {};
                const auto* raw_end = closing.data() + close_start;
                raw = std::string_view(raw_start, static_cast<std::size_t>(raw_end - raw_start));
                caption = payload.substr(text_start);
            } else {
                if (!value.ends_with(end) || value.size() <= end.size()) return {};
                raw = value.substr(0, value.size() - end.size());
                caption = raw;
            }
            shaped = true;
        }
        if (raw.empty() || raw.size() > 1024 || caption.empty() ||
            !mindmap_add(label_bytes, raw.size())) return {};
        int parent = -1;
        if (count == 0) { levels = 1; indents[0] = indent; }
        else if (indent > indents[static_cast<std::size_t>(levels - 1)]) {
            if (levels == 16) return {};
            parent = ancestors[static_cast<std::size_t>(levels - 1)];
            indents[static_cast<std::size_t>(levels++)] = indent;
        } else {
            while (levels > 0 && indent < indents[static_cast<std::size_t>(levels - 1)]) {
                if (!work.spend(16)) return {};
                --levels;
            }
            if (levels <= 1 || indent != indents[static_cast<std::size_t>(levels - 1)]) return {};
            parent = ancestors[static_cast<std::size_t>(levels - 2)];
        }
        spans[count] = {raw, caption, shape, levels - 1, parent};
        ancestors[static_cast<std::size_t>(levels - 1)] = static_cast<int>(count++);
    }
    if (!shaped || count == 0 || label_bytes > source.size()) return {};
    fee = 0;
    if (!mindmap_term(fee, label_bytes, 8 * sizeof(std::string) + 64) ||
        !mindmap_term(fee, count, 512) || !work.spend(fee)) return {};
    std::array<ShapedNode, 64> nodes{};
    int largest_width = 0, height = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto caption = spans[i].caption;
        if (!work.spend(caption.size() + 32) || !mindmap_plain(caption) ||
            caption.front() == ' ' || caption.find("  ") != std::string_view::npos) return {};
        auto label = validated_plain_label(caption);
        if (!label) return {};
        auto& node = nodes[i]; node.caption = std::move(*label);
        const auto glyphs = ftxui::Utf8ToGlyphs(node.caption);
        const int cells = ftxui::string_width(node.caption);
        if (cells < 1 || cells > 256 || glyphs.size() != static_cast<std::size_t>(cells)) return {};
        if (spans[i].shape == MindmapShape::Default) { node.width = cells + 2; node.height = 1; }
        else {
            if (!mindmap_wrap(node.caption, glyphs, node.rows, work)) return {};
            int widest = 0;
            for (const auto& row : node.rows) { if (!work.spend()) return {}; widest = std::max(widest, row.cells); }
            node.width = widest + 4; node.height = static_cast<int>(node.rows.size()) + 2;
            if (spans[i].shape == MindmapShape::Circle) {
                node.width = std::max(9, widest + 6); node.width |= 1;
                node.height = std::max(9, static_cast<int>(node.rows.size()) + 4); node.height |= 1;
            }
        }
        if (node.width > kWidthLimit || node.height > kHeightLimit ||
            height > kHeightLimit - node.height) return {};
        node.top = height;
        height += node.height;
        if (i + 1 < count) {
            if (height > kHeightLimit - 2) return {};
            height += 2;
        }
        largest_width = std::max(largest_width, node.width);
    }
    int width = 0;
    const int stride = largest_width + 4;
    std::size_t visits = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!work.spend(32)) return {};
        auto& node = nodes[i];
        // Width <= 240, stride <= 244 and depth < 16 bound this product.
        node.left = spans[i].depth * stride;
        if (node.left > kWidthLimit - node.width) return {};
        width = std::max(width, node.left + node.width);
        if (!mindmap_term(visits, static_cast<std::size_t>(node.width + node.height), 4) ||
            !mindmap_add(visits, 8)) return {};
        if (i) {
            const auto parent = static_cast<std::size_t>(spans[i].parent);
            if (parent >= i) return {};
            const auto& p = nodes[parent];
            const int px = p.left + (spans[parent].shape == MindmapShape::Default ? 0 : p.width / 2);
            const int cy = node.top + node.height / 2;
            if (cy <= p.top + p.height - 1 || node.left <= px ||
                !mindmap_add(visits, static_cast<std::size_t>(cy - p.top - p.height + 1 + node.left - px))) return {};
        }
    }
    fee = 0;
    if (!mindmap_term(fee, visits, 2 * sizeof(Point) + 16) || !work.spend(fee)) return {};
    fee = 1024;
    std::size_t area = 0;
    if (!mindmap_term(area, static_cast<std::size_t>(width), static_cast<std::size_t>(height)) ||
        !mindmap_term(fee, area, 2 * sizeof(Cell) + 16) ||
        !mindmap_term(fee, static_cast<std::size_t>(height), 4 * sizeof(std::string) + 32) || !work.spend(fee)) return {};
    Canvas canvas(width, height, work);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& n = nodes[i]; const auto shape = spans[i].shape;
        const int l = n.left, t = n.top, r = l + n.width - 1, b = t + n.height - 1;
        const auto put = [&](int x, int y, std::string_view glyph) { return canvas.put({x, y}, glyph); };
        if (shape == MindmapShape::Default) {
            if (!put(l, t, "●") || !work.spend(n.caption.size()) || !canvas.text({l + 2, t}, n.caption)) return {};
            continue;
        }
        if (shape == MindmapShape::Circle) {
            if (!put(l+2,t,"╭") || !put(r-2,t,"╮") || !put(l+1,t+1,"╭") || !put(r-1,t+1,"╮") ||
                !put(l+1,b-1,"╰") || !put(r-1,b-1,"╯") || !put(l+2,b,"╰") || !put(r-2,b,"╯")) return {};
            for (int x=l+3; x<r-2; ++x) if (!put(x,t,"─") || !put(x,b,"─")) return {};
            for (int x=l+2; x<r-1; ++x) if (!put(x,t+1,"─") || !put(x,b-1,"─")) return {};
            for (int y=t+2; y<=b-2; ++y) if (!put(l,y,"│") || !put(r,y,"│")) return {};
        } else {
            const bool rounded = shape == MindmapShape::Rounded;
            if (!put(l,t,rounded?"╭":"┌") || !put(r,t,rounded?"╮":"┐") ||
                !put(l,b,rounded?"╰":"└") || !put(r,b,rounded?"╯":"┘")) return {};
            for (int x=l+1; x<r; ++x) if (!put(x,t,"─") || !put(x,b,"─")) return {};
            for (int y=t+1; y<b; ++y) if (!put(l,y,"│") || !put(r,y,"│")) return {};
        }
        const int inset = shape == MindmapShape::Circle ? 3 : 2;
        const int first = shape == MindmapShape::Circle ? 2 : 1;
        for (std::size_t j=0; j<n.rows.size(); ++j)
            if (!work.spend(n.rows[j].text.size()) ||
                !canvas.text({l+inset,t+first+static_cast<int>(j)},n.rows[j].text)) return {};
    }
    for (std::size_t i = 1; i < count; ++i) {
        const auto parent = static_cast<std::size_t>(spans[i].parent);
        const auto& p = nodes[parent]; const auto& n = nodes[i];
        const Point port{p.left + (spans[parent].shape == MindmapShape::Default ? 0 : p.width/2),p.top+p.height-1};
        const Point entry{n.left,n.top+n.height/2};
        const Point start{port.x,port.y+1}, bend{port.x,entry.y}, end{entry.x-1,entry.y};
        if (!canvas.segment(start,bend) || !canvas.segment(bend,end) || !work.spend(4)) return {};
        auto& above = canvas.at(start); above.bits |= kUp; above.glyph = line_glyph(above.bits);
        auto& beside = canvas.at(end); beside.bits |= kRight; beside.glyph = line_glyph(beside.bits);
        if (spans[parent].shape != MindmapShape::Default && !canvas.put(port,"┬")) return {};
        if (spans[i].shape != MindmapShape::Default && !canvas.put(entry,"┤")) return {};
    }
    std::size_t output_bytes = 0;
    for (const auto& cell : canvas.cells)
        if (!work.spend() || !mindmap_add(output_bytes,cell.glyph.size())) return {};
    if (!work.spend(output_bytes)) return {};
    auto result = canvas.lines();
    if (result.empty()) return {};
    return result;
}
} // namespace

std::optional<std::vector<std::string>> mindmap_layout(std::string_view source, Work& work) {
    // Keep every successful Original10 path and its exact fees/frame unchanged.
    // The shaped profile pays for its own pass from the same remaining budget.
    if (auto original = mindmap_original_layout(source, work)) return original;
    return mindmap_shapes_layout(source, work);
}

} // namespace loom::ui::mermaid::detail
