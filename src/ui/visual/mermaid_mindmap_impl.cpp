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

// D-465: the shaped canvas charge is `2*sizeof(Cell)+16` per cell; one further
// unit per emitted cell covers the output copy. A canvas-derived supplemental
// pool of exactly that plus a fixed margin funds a wide/deep shaped tree
// without changing any charge a tree that fits the primary would make.
constexpr std::size_t mindmap_envelope_layout_allowance(std::size_t width, std::size_t height) {
    const std::size_t cells = width * height;
    return cells * (2u * sizeof(Cell) + 17u) + 262'144u;
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
        !unicode_text(value))
        return false;
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
    return value.find(": ") == std::string_view::npos && value.find(" #") == std::string_view::npos;
}

// D-439: a mindmap envelope may also carry the closed `config.layout`
// namespace with exactly `layout: tidy-tree`. The tidy-tree engine is an
// external layout engine the terminal projection does not run, so the leaf is
// inert and disclosed (`Mindmap native projection v1`); every other config
// key, init directive, title or extra field keeps the whole source on
// fallback.
struct MindmapEnvelope {
    std::string_view body;
    bool tidy_tree = false;
};

std::optional<MindmapEnvelope> mindmap_body(std::string_view source, Work& work) {
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(source, at, line)) return {};
    if (line != "---") return MindmapEnvelope{source, false};
    bool seen = false, folded = false, payload = false, closed = false, config_seen = false,
         layout_seen = false;
    std::size_t block_indent = 0;
    while (mindmap_line(source, at, line)) {
        if (line == "---") {
            closed = true;
            break;
        }
        if (line.find_first_not_of(' ') == std::string_view::npos) continue;
        if (folded && !config_seen && line.front() == ' ') {
            const auto indent = line.find_first_not_of(' ');
            if (block_indent == 0) block_indent = indent;
            if (indent != block_indent || !unicode_text(line.substr(indent))) return {};
            payload = true;
            continue;
        }
        if (config_seen) {
            if (line != "  layout: tidy-tree" || layout_seen) return {};
            layout_seen = true;
            continue;
        }
        if (line == "config:") {
            if (!seen) return {};
            config_seen = true;
            continue;
        }
        if (seen || !line.starts_with("description: ")) return {};
        seen = true;
        const auto value = line.substr(13);
        if (value == ">-")
            folded = true;
        else if (!mindmap_description(value))
            return {};
    }
    if (!closed || !seen || (folded && !payload) || (config_seen && !layout_seen)) return {};
    const auto envelope = detail::parse_envelope(source, work);
    if (!envelope || !envelope->has_frontmatter || !envelope->description || envelope->title ||
        !envelope->init.empty())
        return {};
    if (config_seen) {
        if (envelope->config.size() != 1u) return {};
        const auto& event = envelope->config.front();
        if (event.path != "layout" || event.indent != 2 || event.namespace_entry ||
            event.comment || event.blank || event.form != EnvelopeScalarForm::Plain ||
            event.value != "tidy-tree")
            return {};
    } else if (!envelope->config.empty()) {
        return {};
    }
    std::size_t fields = 0;
    for (const auto& event : envelope->events) {
        if (!work.spend()) return {};
        if (event.blank) continue;
        if (event.comment) return {};
        if (event.top_level) {
            if (event.path == "description") {
                if (++fields != 1) return {};
                continue;
            }
            if (config_seen && event.namespace_entry && event.path == "config") continue;
            return {};
        }
        if (config_seen && event.path == "layout") continue;
        return {};
    }
    if (fields != 1) return {};
    return MindmapEnvelope{envelope->raw_body, config_seen};
}

// D-439/D-464/D-465: canonical disclosures for admitted-but-inert constructs.
// A source that carries none keeps its frame byte-identical (no footer). Order
// is fixed: layout, shapes (bang, cloud, hex), icons, markdown emphasis.
struct MindmapNotices {
    bool tidy_tree = false;
    bool bang = false;
    bool cloud = false;
    bool hex = false;
    bool icons = false;
    bool markdown = false;
};

struct MindmapNoticeRule {
    bool MindmapNotices::*flag;
    std::string_view text;
};

constexpr std::array<MindmapNoticeRule, 6> kMindmapNoticeRules{{
    {&MindmapNotices::tidy_tree,
     "config.layout=tidy-tree -> external layout engine; not applied"},
    {&MindmapNotices::bang, "shape bang -> square frame; not applied"},
    {&MindmapNotices::cloud, "shape cloud -> rounded frame; not applied"},
    {&MindmapNotices::hex, "shape hex -> square frame; not applied"},
    {&MindmapNotices::icons, "::icon annotations -> icon glyph; not applied"},
    {&MindmapNotices::markdown, "markdown emphasis -> plain text; not applied"},
}};

bool mindmap_append_notices(std::vector<std::string>& rows, const MindmapNotices& flags,
                            Work& work) {
    std::array<std::string_view, kMindmapNoticeRules.size()> notices{};
    std::size_t size = 0;
    std::size_t extra = 256;
    for (const auto& rule : kMindmapNoticeRules) {
        if (!(flags.*rule.flag)) continue;
        notices[size++] = rule.text;
        extra += rule.text.size();
    }
    if (size == 0) return true;
    // Preserve the pre-D-464 row guard exactly (a 510/511-row frame that drew
    // before still draws); the notice rows are covered by the spend below.
    if (rows.size() >= 512u || !work.spend(extra)) return false;
    rows.push_back(std::string{});
    rows.push_back("Mindmap native projection v1");
    for (std::size_t i = 0; i < size; ++i) rows.push_back(std::string{notices[i]});
    return true;
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
        label.find("--") != std::string_view::npos)
        return false;
    return true;
}

// Rows are views into the stable validated label, not copied word strings.
// FTXUI's second wide-cell slot stays attached even if it holds a combining
// character. The final partial long-word chunk remains open for the next word.
bool mindmap_wrap(std::string_view label, const std::vector<std::string>& glyphs,
                  std::vector<CaptionRow>& rows, Work& work) {
    rows.reserve(glyphs.size());
    std::size_t row_begin = 0, row_end = 0, byte = 0, i = 0;
    int row_cells = 0;
    const auto flush = [&]() {
        if (row_cells == 0 || rows.size() >= 256 || !work.spend(4)) return false;
        rows.push_back({label.substr(row_begin, row_end - row_begin), row_cells});
        row_cells = 0;
        return true;
    };
    while (i < glyphs.size()) {
        if (!work.spend()) return false;
        // Only an actual standalone ASCII space is a word boundary; a space
        // carrying combining ink stays with its indivisible FTXUI glyph.
        if (glyphs[i] == " ") {
            ++byte;
            ++i;
            continue;
        }
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
            if (row_cells == 0)
                row_begin = word_begin;
            else
                ++row_cells;
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
            for (std::size_t n = 0; n < slots; ++n)
                chunk_byte += glyphs[g + n].size();
            row_end = chunk_byte;
            row_cells += static_cast<int>(slots);
            g += slots;
        }
    }
    return row_cells != 0 && flush();
}

// D-465: a markdown-string label keeps its text and drops its emphasis. Only
// `**bold**` and `*italic*` spans are admitted: each must be non-empty, hold
// no nested marker and have no edge space; every other marker form rejects.
bool mindmap_markdown_plain(std::string_view value, std::string& out, Work& work) {
    out.clear();
    out.reserve(value.size());
    const auto find_single = [&](std::size_t from) {
        while (from < value.size()) {
            const auto pos = value.find('*', from);
            if (pos == std::string_view::npos) return pos;
            if (pos + 1 < value.size() && value[pos + 1] == '*') {
                from = pos + 2;
                continue;
            }
            return pos;
        }
        return std::string_view::npos;
    };
    for (std::size_t i = 0; i < value.size();) {
        if (!work.spend()) return false;
        if (value[i] != '*') {
            out.push_back(value[i++]);
            continue;
        }
        const bool strong = i + 1 < value.size() && value[i + 1] == '*';
        const std::size_t marker = strong ? 2u : 1u;
        const auto close = strong ? value.find("**", i + marker) : find_single(i + marker);
        if (close == std::string_view::npos || close == i + marker) return false;
        const auto span = value.substr(i + marker, close - i - marker);
        if (span.front() == ' ' || span.back() == ' ' ||
            span.find('*') != std::string_view::npos || !work.spend(span.size()))
            return false;
        out.append(span);
        i = close + marker;
    }
    return !out.empty();
}

}  // namespace

std::optional<std::vector<std::string>> mindmap_original_layout(std::string_view source,
                                                                Work& work) {
    if (source.empty() || source.size() > kSourceLimit) return {};
    std::size_t fee = 256;
    if (!mindmap_term(fee, source.size(), 8) || !work.spend(fee)) return {};
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    }
    const auto envelope = mindmap_body(source, work);
    if (!envelope) return {};
    const auto body = envelope->body;
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(body, at, line) || line != "mindmap") return {};

    fee = 1024;
    if (!mindmap_term(fee, 64, 2 * sizeof(NodeSpan) + 2 * sizeof(Node) + 32) ||
        !mindmap_term(fee, 16, 4 * sizeof(int)) || !work.spend(fee))
        return {};
    std::array<NodeSpan, 64> spans{};
    std::array<int, 16> indents{}, ancestors{};
    std::size_t count = 0;
    int levels = 0;
    while (mindmap_line(body, at, line)) {
        if (line.empty()) continue;
        const auto leading = line.find_first_not_of(' ');
        if (leading == std::string_view::npos) continue;
        if (count == spans.size()) return {};
        const int indent = static_cast<int>(leading);  // source is bounded to 64 KiB
        auto label = line.substr(leading);
        bool square = false;
        const auto bracket = label.find('[');
        if (bracket != std::string_view::npos) {
            if (count != 0 || !label.ends_with(']') || !mindmap_id(label.substr(0, bracket)))
                return {};
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
                    indent != indents[static_cast<std::size_t>(levels - 1)] || !work.spend(16))
                    return {};
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
            !mindmap_add(label_bytes, spans[i].label.size()))
            return {};
    }
    fee = 0;
    if (label_bytes > source.size() ||
        !mindmap_term(fee, label_bytes, 8 * sizeof(std::string) + 64) ||
        !mindmap_term(fee, count, 512) || !work.spend(fee))
        return {};
    std::array<Node, 64> nodes{};
    std::vector<CaptionRow> rows;
    int width = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (!work.spend(spans[i].label.size() + 16)) return {};
        // Token-set and repeated-space scans belong to the fully prepaid L
        // stage, not the cheaper raw lexeme-span collection pass.
        if (!mindmap_plain(spans[i].label) ||
            (spans[i].square && (spans[i].label.front() == ' ' ||
                                 spans[i].label.find("  ") != std::string_view::npos)))
            return {};
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
        } else
            width = std::max(width, 4 * spans[i].depth + 2 + cells);
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
        !work.spend(fee))
        return {};
    Canvas canvas(width, height, work);
    if (square) {
        if (!canvas.segment({0, 0}, {width - 1, 0}) ||
            !canvas.segment({width - 1, 0}, {width - 1, height - 1}) ||
            !canvas.segment({width - 1, height - 1}, {0, height - 1}) ||
            !canvas.segment({0, height - 1}, {0, 0}))
            return {};
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (!work.spend(rows[i].text.size()) ||
                !canvas.text({2, static_cast<int>(i) + 1}, rows[i].text))
                return {};
    } else {
        for (std::size_t i = 1; i < count; ++i) {
            const auto parent = static_cast<std::size_t>(spans[i].parent);
            const Point start{4 * spans[parent].depth, 2 * static_cast<int>(parent) + 1};
            const Point bend{start.x, 2 * static_cast<int>(i)};
            const Point end{4 * spans[i].depth - 1, bend.y};
            if (!canvas.segment(start, bend) || !canvas.segment(bend, end) || !work.spend(2))
                return {};
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
                !canvas.text({marker.x + 2, marker.y}, nodes[i].label))
                return {};
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
    if (!mindmap_append_notices(result, MindmapNotices{.tidy_tree = envelope->tidy_tree},
                                work))
        return {};
    return result;
}

namespace {
enum class MindmapShape { Default, Square, Rounded, Circle, Bang, Cloud, Hex };

// The shape delimiters this grammar admits. Bang and cloud open with a closing
// bracket and close with an opening one, exactly as upstream spells them.
struct ShapeToken {
    std::string_view open;
    std::string_view close;
    MindmapShape shape;
};

// Longest opener first: `((`/`))`/`{{` before their one-character prefixes.
std::optional<ShapeToken> mindmap_shape_token(std::string_view value) {
    if (value.starts_with("((")) return ShapeToken{"((", "))", MindmapShape::Circle};
    if (value.starts_with("{{")) return ShapeToken{"{{", "}}", MindmapShape::Hex};
    if (value.starts_with("))")) return ShapeToken{"))", "((", MindmapShape::Bang};
    if (value.starts_with('(')) return ShapeToken{"(", ")", MindmapShape::Rounded};
    if (value.starts_with('[')) return ShapeToken{"[", "]", MindmapShape::Square};
    if (value.starts_with(')')) return ShapeToken{")", "(", MindmapShape::Cloud};
    return std::nullopt;
}

struct ShapedSpan {
    std::string_view raw, caption;
    MindmapShape shape = MindmapShape::Default;
    bool markdown = false;
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
        if (source[i] == '\t' ||
            (source[i] == '\r' && (i + 1 == source.size() || source[i + 1] != '\n')))
            return {};
    const auto envelope = mindmap_body(source, work);
    if (!envelope) return {};
    const auto body = envelope->body;
    std::size_t at = 0;
    std::string_view line;
    if (!mindmap_line(body, at, line) || line != "mindmap") return {};
    fee = 1024;
    if (!mindmap_term(fee, 64, 2 * sizeof(ShapedSpan) + 2 * sizeof(ShapedNode) + 32) ||
        !mindmap_term(fee, 16, 4 * sizeof(int)) || !work.spend(fee))
        return {};
    std::array<ShapedSpan, 64> spans{};
    std::array<int, 16> indents{}, ancestors{};
    std::size_t count = 0, label_bytes = 0;
    int levels = 0;
    bool shaped = false, saw_icon = false, saw_bang = false, saw_cloud = false, saw_hex = false,
         saw_emphasis = false;
    while (mindmap_line(body, at, line)) {
        const auto leading = line.find_first_not_of(' ');
        if (leading == std::string_view::npos) continue;
        if (count == spans.size() || !work.spend(32)) return {};
        const int indent = static_cast<int>(leading);
        const auto declaration = line.substr(leading);
        // D-464: an `::icon(...)` annotation attaches to the preceding node.
        // The projection never draws an icon, so the directive is validated,
        // dropped and disclosed; it creates no node and does not touch the
        // indentation stack.
        if (declaration.starts_with("::icon(")) {
            const auto body_start = std::string_view{"::icon("}.size();
            if (count == 0 || !declaration.ends_with(')') ||
                declaration.size() <= body_start + 1u || !work.spend(declaration.size() + 16u))
                return {};
            const auto payload =
                declaration.substr(body_start, declaration.size() - body_start - 1u);
            if (payload.size() > 64u || payload.front() == ' ' || payload.back() == ' ' ||
                payload.find_first_of("()") != std::string_view::npos ||
                !unicode_text(payload))
                return {};
            saw_icon = true;
            continue;
        }
        auto raw = declaration, caption = declaration;
        auto shape = MindmapShape::Default;
        bool markdown = false;
        const auto opening = declaration.find_first_of("[({)");
        if (opening != std::string_view::npos) {
            const auto token = mindmap_shape_token(declaration.substr(opening));
            if (!token) return {};
            const auto id = declaration.substr(0, opening);
            // An id may be omitted only on the anonymous root shorthand.
            if (id.empty() ? count != 0 : !mindmap_id(id)) return {};
            shape = token->shape;
            auto value = declaration.substr(opening);
            value.remove_prefix(token->open.size());
            // D-465: a backtick markdown-string payload may span physical
            // lines; the closing sequence is ` + the shape's close token.
            const auto markdown_ends = [&](std::string_view text) {
                return text.size() > token->close.size() && text.ends_with(token->close) &&
                       text[text.size() - token->close.size() - 1] == '`';
            };
            if (value.starts_with('`')) {
                // The payload is contiguous in the source, so the raw span runs
                // from the opening backtick through the closing one even when
                // the label spans lines.
                const auto* raw_start = value.data() + 1;
                while (!markdown_ends(value)) {
                    std::string_view next;
                    if (!mindmap_line(body, at, next)) return {};
                    const auto start = next.find_first_not_of(' ');
                    if (start == std::string_view::npos || !work.spend(next.size() + 16u))
                        return {};
                    value = next.substr(start);
                }
                const auto* raw_end = value.data() + value.size() - token->close.size() - 1;
                if (raw_end < raw_start) return {};
                raw = std::string_view(raw_start, static_cast<std::size_t>(raw_end - raw_start));
                caption = raw;
                markdown = true;
                saw_emphasis = saw_emphasis || raw.find('*') != std::string_view::npos;
            } else if (value.empty()) {
                // Keep the entire authored description, including CRLF/indent,
                // separate from its one nonempty terminal caption line.
                const auto* raw_start = value.data();
                std::string_view payload, closing;
                if (!mindmap_line(body, at, payload) || !mindmap_line(body, at, closing))
                    return {};
                const auto text_start = payload.find_first_not_of(' ');
                const auto close_start = closing.find_first_not_of(' ');
                if (text_start == std::string_view::npos || close_start == std::string_view::npos ||
                    closing.substr(close_start) != token->close)
                    return {};
                const auto* raw_end = closing.data() + close_start;
                raw = std::string_view(raw_start, static_cast<std::size_t>(raw_end - raw_start));
                caption = payload.substr(text_start);
            } else {
                if (!value.ends_with(token->close) || value.size() <= token->close.size())
                    return {};
                raw = value.substr(0, value.size() - token->close.size());
                caption = raw;
            }
            shaped = true;
            saw_bang = saw_bang || shape == MindmapShape::Bang;
            saw_cloud = saw_cloud || shape == MindmapShape::Cloud;
            saw_hex = saw_hex || shape == MindmapShape::Hex;
        }
        if (raw.empty() || raw.size() > 1024 || caption.empty() ||
            !mindmap_add(label_bytes, raw.size()))
            return {};
        int parent = -1;
        if (count == 0) {
            levels = 1;
            indents[0] = indent;
        } else if (indent > indents[static_cast<std::size_t>(levels - 1)]) {
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
        spans[count] = {raw, caption, shape, markdown, levels - 1, parent};
        ancestors[static_cast<std::size_t>(levels - 1)] = static_cast<int>(count++);
    }
    if (!shaped || count == 0 || label_bytes > source.size()) return {};
    fee = 0;
    if (!mindmap_term(fee, label_bytes, 8 * sizeof(std::string) + 64) ||
        !mindmap_term(fee, count, 512) || !work.spend(fee))
        return {};
    std::array<ShapedNode, 64> nodes{};
    int largest_width = 0, height = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto caption = spans[i].caption;
        if (!work.spend(caption.size() + 32)) return {};
        auto& node = nodes[i];
        const bool entity_break =
            !spans[i].markdown && caption.find("&lt;br/>") != std::string_view::npos;
        if (!spans[i].markdown && !entity_break) {
            // The single-line path is unchanged: every pre-D-465 row keeps its
            // exact validation, width and height.
            if (!mindmap_plain(caption) || caption.front() == ' ' ||
                caption.find("  ") != std::string_view::npos)
                return {};
            auto label = validated_plain_label(caption);
            if (!label) return {};
            node.caption = std::move(*label);
            const auto glyphs = ftxui::Utf8ToGlyphs(node.caption);
            const int cells = ftxui::string_width(node.caption);
            if (cells < 1 || cells > 256 || glyphs.size() != static_cast<std::size_t>(cells))
                return {};
            if (spans[i].shape == MindmapShape::Default) {
                node.width = cells + 2;
                node.height = 1;
            } else {
                if (!mindmap_wrap(node.caption, glyphs, node.rows, work)) return {};
                int widest = 0;
                for (const auto& row : node.rows) {
                    if (!work.spend()) return {};
                    widest = std::max(widest, row.cells);
                }
                node.width = widest + 4;
                node.height = static_cast<int>(node.rows.size()) + 2;
                if (spans[i].shape == MindmapShape::Circle) {
                    node.width = std::max(9, widest + 6);
                    node.width |= 1;
                    node.height = std::max(9, static_cast<int>(node.rows.size()) + 4);
                    node.height |= 1;
                }
            }
        } else {
            // D-465: a markdown-string label splits on its physical lines
            // (emphasis stripped); any other caption splits on the escaped
            // break `&lt;br/>`. Every resulting line is validated on its own.
            std::string text;
            std::size_t at = 0;
            for (;;) {
                std::string_view line;
                std::size_t next_at = 0;
                if (!spans[i].markdown && (caption.starts_with("&lt;br/>") ||
                                           caption.ends_with("&lt;br/>")))
                    return {};
                if (spans[i].markdown) {
                    const auto nl = caption.find('\n', at);
                    line = caption.substr(at, nl == std::string_view::npos ? caption.size() - at
                                                                           : nl - at);
                    next_at = nl == std::string_view::npos ? caption.size() : nl + 1;
                } else {
                    const auto br = caption.find("&lt;br/>", at);
                    line = caption.substr(at, br == std::string_view::npos ? caption.size() - at
                                                                           : br - at);
                    next_at = br == std::string_view::npos ? caption.size() : br + 8;
                }
                std::string stripped;
                if (spans[i].markdown) {
                    // The raw span is contiguous, so a CRLF source leaves a
                    // CR on every non-final line; the tree view never does.
                    if (line.ends_with('\r')) line.remove_suffix(1);
                    // Continuation-line indentation is significant to the
                    // terminal tree, never to the label text.
                    const auto lead = line.find_first_not_of(' ');
                    if (lead == std::string_view::npos) return {};
                    line = line.substr(lead);
                    if (line.empty() || line.front() == ' ' || line.back() == ' ' ||
                        line.find("  ") != std::string_view::npos ||
                        !mindmap_markdown_plain(line, stripped, work))
                        return {};
                    line = stripped;
                }
                if (line.empty() || !mindmap_plain(line) || line.front() == ' ' ||
                    line.find("  ") != std::string_view::npos)
                    return {};
                if (!text.empty()) text.push_back('\n');
                text.append(line);
                if (text.size() > 1024) return {};
                if (next_at >= caption.size()) break;
                at = next_at;
            }
            node.caption = std::move(text);
            std::size_t line_at = 0;
            int widest = 0;
            for (;;) {
                const auto nl = node.caption.find('\n', line_at);
                const auto line = std::string_view{node.caption}.substr(
                    line_at, nl == std::string::npos ? node.caption.size() - line_at : nl - line_at);
                const auto glyphs = ftxui::Utf8ToGlyphs(std::string{line});
                const int cells = ftxui::string_width(std::string{line});
                if (cells < 1 || cells > 256 ||
                    glyphs.size() != static_cast<std::size_t>(cells))
                    return {};
                if (spans[i].shape == MindmapShape::Default) {
                    if (node.rows.size() >= 256u) return {};
                    widest = std::max(widest, cells);
                    node.rows.push_back({line, cells});
                } else {
                    if (!mindmap_wrap(line, glyphs, node.rows, work)) return {};
                }
                if (nl == std::string::npos) break;
                line_at = nl + 1;
            }
            if (spans[i].shape == MindmapShape::Default) {
                node.width = widest + 2;
                node.height = static_cast<int>(node.rows.size());
            } else {
                for (const auto& row : node.rows) {
                    if (!work.spend()) return {};
                    widest = std::max(widest, row.cells);
                }
                node.width = widest + 4;
                node.height = static_cast<int>(node.rows.size()) + 2;
                if (spans[i].shape == MindmapShape::Circle) {
                    node.width = std::max(9, widest + 6);
                    node.width |= 1;
                    node.height = std::max(9, static_cast<int>(node.rows.size()) + 4);
                    node.height |= 1;
                }
            }
        }
        if (node.width > kWidthLimit || node.height > kHeightLimit ||
            height > kHeightLimit - node.height)
            return {};
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
            !mindmap_add(visits, 8))
            return {};
        if (i) {
            const auto parent = static_cast<std::size_t>(spans[i].parent);
            if (parent >= i) return {};
            const auto& p = nodes[parent];
            const int px =
                p.left + (spans[parent].shape == MindmapShape::Default ? 0 : p.width / 2);
            const int cy = node.top + node.height / 2;
            if (cy <= p.top + p.height - 1 || node.left <= px ||
                !mindmap_add(visits,
                             static_cast<std::size_t>(cy - p.top - p.height + 1 + node.left - px)))
                return {};
        }
    }
    // D-465: the shaped canvas charge is area-proportional, so a wide/deep
    // tree can exhaust the fixed 1M primary before its own drawing fees. Fund
    // exactly this canvas from a bounded supplemental pool; enabling the pool
    // allocates nothing and charges nothing, so a tree that fits the primary
    // never reaches the clamp and its charge sequence is unchanged. Nodes
    // (<= 64), per-node rows (<= 256) and both dimensions (kWidthLimit/
    // kHeightLimit) are bounded independently, so the dimension caps — not the
    // Work primary — are the ceiling for an area-dominated tree.
    work.enable_envelope(mindmap_envelope_layout_allowance(
        static_cast<std::size_t>(width), static_cast<std::size_t>(height)));
    fee = 0;
    if (!mindmap_term(fee, visits, 2 * sizeof(Point) + 16) || !work.spend(fee)) return {};
    fee = 1024;
    std::size_t area = 0;
    if (!mindmap_term(area, static_cast<std::size_t>(width), static_cast<std::size_t>(height)) ||
        !mindmap_term(fee, area, 2 * sizeof(Cell) + 16) ||
        !mindmap_term(fee, static_cast<std::size_t>(height), 4 * sizeof(std::string) + 32) ||
        !work.spend(fee))
        return {};
    Canvas canvas(width, height, work);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& n = nodes[i];
        const auto shape = spans[i].shape;
        const int l = n.left, t = n.top, r = l + n.width - 1, b = t + n.height - 1;
        const auto put = [&](int x, int y, std::string_view glyph) {
            return canvas.put({x, y}, glyph);
        };
        if (shape == MindmapShape::Default) {
            if (!put(l, t, "●") || !work.spend(n.caption.size())) return {};
            // A D-465 multi-line default node draws one row per caption line
            // under the single marker; a single-line node keeps its exact draw.
            if (n.rows.empty()) {
                if (!canvas.text({l + 2, t}, n.caption)) return {};
            } else {
                for (std::size_t j = 0; j < n.rows.size(); ++j)
                    if (!canvas.text({l + 2, t + static_cast<int>(j)}, n.rows[j].text))
                        return {};
            }
            continue;
        }
        if (shape == MindmapShape::Circle) {
            if (!put(l + 2, t, "╭") || !put(r - 2, t, "╮") || !put(l + 1, t + 1, "╭") ||
                !put(r - 1, t + 1, "╮") || !put(l + 1, b - 1, "╰") || !put(r - 1, b - 1, "╯") ||
                !put(l + 2, b, "╰") || !put(r - 2, b, "╯"))
                return {};
            for (int x = l + 3; x < r - 2; ++x)
                if (!put(x, t, "─") || !put(x, b, "─")) return {};
            for (int x = l + 2; x < r - 1; ++x)
                if (!put(x, t + 1, "─") || !put(x, b - 1, "─")) return {};
            for (int y = t + 2; y <= b - 2; ++y)
                if (!put(l, y, "│") || !put(r, y, "│")) return {};
        } else {
            // D-464: bang and hex have no dedicated cell frame; the closest
            // admitted frame is drawn and the substitution is disclosed.
            const bool rounded =
                shape == MindmapShape::Rounded || shape == MindmapShape::Cloud;
            if (!put(l, t, rounded ? "╭" : "┌") || !put(r, t, rounded ? "╮" : "┐") ||
                !put(l, b, rounded ? "╰" : "└") || !put(r, b, rounded ? "╯" : "┘"))
                return {};
            for (int x = l + 1; x < r; ++x)
                if (!put(x, t, "─") || !put(x, b, "─")) return {};
            for (int y = t + 1; y < b; ++y)
                if (!put(l, y, "│") || !put(r, y, "│")) return {};
        }
        const int inset = shape == MindmapShape::Circle ? 3 : 2;
        const int first = shape == MindmapShape::Circle ? 2 : 1;
        for (std::size_t j = 0; j < n.rows.size(); ++j)
            if (!work.spend(n.rows[j].text.size()) ||
                !canvas.text({l + inset, t + first + static_cast<int>(j)}, n.rows[j].text))
                return {};
    }
    for (std::size_t i = 1; i < count; ++i) {
        const auto parent = static_cast<std::size_t>(spans[i].parent);
        const auto& p = nodes[parent];
        const auto& n = nodes[i];
        const Point port{p.left + (spans[parent].shape == MindmapShape::Default ? 0 : p.width / 2),
                         p.top + p.height - 1};
        const Point entry{n.left, n.top + n.height / 2};
        const Point start{port.x, port.y + 1}, bend{port.x, entry.y}, end{entry.x - 1, entry.y};
        if (!canvas.segment(start, bend) || !canvas.segment(bend, end) || !work.spend(4)) return {};
        auto& above = canvas.at(start);
        above.bits |= kUp;
        above.glyph = line_glyph(above.bits);
        auto& beside = canvas.at(end);
        beside.bits |= kRight;
        beside.glyph = line_glyph(beside.bits);
        if (spans[parent].shape != MindmapShape::Default && !canvas.put(port, "┬")) return {};
        if (spans[i].shape != MindmapShape::Default && !canvas.put(entry, "┤")) return {};
    }
    std::size_t output_bytes = 0;
    for (const auto& cell : canvas.cells)
        if (!work.spend() || !mindmap_add(output_bytes, cell.glyph.size())) return {};
    if (!work.spend(output_bytes)) return {};
    auto result = canvas.lines();
    if (result.empty()) return {};
    if (!mindmap_append_notices(
            result,
            MindmapNotices{.tidy_tree = envelope->tidy_tree,
                           .bang = saw_bang,
                           .cloud = saw_cloud,
                           .hex = saw_hex,
                           .icons = saw_icon,
                           .markdown = saw_emphasis},
            work))
        return {};
    return result;
}
}  // namespace

std::optional<std::vector<std::string>> mindmap_layout(std::string_view source, Work& work) {
    // Keep every successful Original10 path and its exact fees/frame unchanged.
    // The shaped profile pays for its own pass from the same remaining budget.
    if (auto original = mindmap_original_layout(source, work)) return original;
    return mindmap_shapes_layout(source, work);
}

}  // namespace loom::ui::mermaid::detail
