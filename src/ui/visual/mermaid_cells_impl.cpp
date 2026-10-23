module;

#include <ftxui/screen/string.hpp>

module loom.ui.visual.mermaid;

import std;

namespace loom::ui::mermaid::detail {

// Text safety is independent of each Mermaid family's source grammar.
bool unicode_text(std::string_view text) {
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

    }
    return true;
}

std::optional<std::string> validated_plain_label(std::string_view text) {
    if (text.find_first_not_of(" \t") == std::string_view::npos || !unicode_text(text)) return {};
    const auto result = std::string(text);
    const auto cells = ftxui::Utf8ToGlyphs(result);
    if (cells.empty() || cells.size() > kLabelLimit || ftxui::string_width(result) > kLabelLimit) return {};
    // A leading combining mark has no display cell and would lose information.
    std::string round_trip;
    for (const auto& glyph : cells) round_trip += glyph;
    if (round_trip != result) return {};
    return result;
}

bool Work::spend(std::size_t count) {
    if (count > remaining) { remaining = 0; return false; }
    remaining -= count;
    return true;
}

std::vector<std::string_view> label_rows(std::string_view text) {
    std::vector<std::string_view> rows;
    while (true) {
        const auto end = text.find('\n');
        rows.push_back(text.substr(0, end));
        if (end == std::string_view::npos) return rows;
        text.remove_prefix(end + 1);
    }
}

int label_width(std::string_view text) {
    int width = 0;
    for (const auto row : label_rows(text)) width = std::max(width, ftxui::string_width(std::string(row)));
    return width;
}

Point operator+(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point operator-(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
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

Canvas::Canvas(int w, int h, Work& budget) : width(w), height(h), work(budget), cells(static_cast<std::size_t>(w * h)) {}
bool Canvas::inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < width && p.y < height; }
int Canvas::index(Point p) const { return p.y * width + p.x; }
Cell& Canvas::at(Point p) { return cells[static_cast<std::size_t>(index(p))]; }
const Cell& Canvas::at(Point p) const { return cells[static_cast<std::size_t>(index(p))]; }
bool Canvas::free(Point p) const { return work.spend() && inside(p) && !at(p).blocked; }
bool Canvas::put(Point p, std::string_view glyph, bool block) {
    if (!inside(p) || !work.spend()) return false;
    at(p).glyph = glyph;
    at(p).blocked = block;
    return true;
}
bool Canvas::text(Point p, std::string_view value) {
    for (const auto& glyph : ftxui::Utf8ToGlyphs(std::string(value))) {
        if (!put(p, glyph)) return false;
        ++p.x;
    }
    return true;
}
bool Canvas::multiline(Point p, std::string_view value, int width) {
    for (const auto row : label_rows(value)) {
        if (!text({p.x + (width - ftxui::string_width(std::string(row))) / 2, p.y}, row)) return false;
        ++p.y;
    }
    return true;
}
bool Canvas::segment(Point from, Point to, bool dashed) {
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
std::vector<std::string> Canvas::lines() {
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

} // namespace loom::ui::mermaid::detail
