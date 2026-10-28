// RFC 0030 B2 test-only worksheet. This is not a production renderer or an
// admission oracle. It keeps source scheduling and proposed production cost
// separate from the test's own bounded memory/CPU budget.
#pragma once
#include <ftxui/dom/node.hpp>

#include "mermaid_block58_source.hpp"

namespace loom::test::mermaid_block58::projection {
using Json = loom::utils::json::JsonVal;

struct Point {
    int x = 0, y = 0;
    bool operator==(const Point&) const = default;
};

struct Box {
    int x = 0, y = 0, w = 0, h = 0;

    bool contains(Point p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
};

inline constexpr std::array<Point, 4> kStep{{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

struct Placement {
    int child = 0, col = 0, row = 0, span = 1;
};

struct Entity {
    std::string id, shape;
    std::vector<std::string> rows;
    std::vector<Placement> children;
    std::vector<int> columns, row_heights;
    int parent = -1, span = 1, min_w = 0, min_h = 0;
    unsigned heads = 0;
    bool group = false, space = false;
    std::uint16_t fill = 1, stroke = 1;
    bool bold = false;
    Box band, body, caption, core;
    std::array<Point, 4> ports{};
};

struct Edge {
    int source = 0, target = 0;
    std::string label, start, end;
    bool bold = false, dotted = false;
};

// The proposed compact raster uses this exact ABI; it retains no owning string.
struct Cell {
    std::string_view glyph = " ";
    std::uint16_t style = 0;
    std::uint8_t owner = 0, flags = 0;
};

static_assert(sizeof(Cell) <= 24);

struct ParserSpan {
    std::string_view lexeme;
    std::size_t begin, end;
    std::uint16_t kind;
};

struct StylePair {
    std::string_view key, value;
    std::uint8_t fg, bg;
};

struct Gate {
    int group = 0;
    Point at;
};

struct Route {
    int source = 0, target = 0;
    std::vector<Point> points;
    std::vector<Gate> gates;
    Point start_marker, end_marker;
    int start_direction = -1, end_direction = -1;
    std::string start_kind, end_kind, label;
    Box label_box;
};

struct Metrics {
    std::size_t production_upper = 0, oracle_remaining = 0, area = 0, note_cells = 0;
    std::size_t contour_visits = 0, route_visits = 0, route_attempts = 0, runs = 0;
    std::size_t serialized_bytes_upper = 0, note_records = 0, note_rows = 0;
    int width = 0, grid_width = 0, grid_height = 0, height = 0;
};

struct Plan {
    std::vector<Entity> entities;
    std::vector<Edge> edges;
    std::vector<Cell> cells;
    std::vector<Route> routes;
    std::vector<std::string> notes;
    Metrics metrics;
    // A passing worksheet must never be confused with observed terminal art.
    static constexpr bool glyph_oracle_approved = false;
    static constexpr bool production_prepayment_verified = false;
    static constexpr int drawing_credit = 0;
};

inline void require(bool yes, std::string_view why) {
    if (!yes) throw std::runtime_error(std::string(why));
}

struct Ledger {
    std::size_t used = 0;

    void pay(std::size_t count, std::string_view stage) {
        if (count > 1'000'000 - used)
            throw std::runtime_error("planned-work/" + std::string(stage) + ": " +
                                     std::to_string(used) + " + " + std::to_string(count));
        used += count;
    }

    void product(std::size_t n, std::size_t unit, std::string_view stage) {
        require(unit == 0 || n <= std::numeric_limits<std::size_t>::max() / unit,
                "worksheet product overflow");
        pay(n * unit, stage);
    }
};

inline int number(Json j) {
    require(j.is_int() && j.as_int() >= -1 && j.as_int() <= 65536, "worksheet integer domain");
    return static_cast<int>(j.as_int());
}

inline std::string text(Json j) {
    require(j.is_str() && j.as_str().size() <= 65536, "worksheet text domain");
    return std::string(j.as_str());
}

inline int role(std::string_view name) {
    if (name == "Neutral") return 1;
    if (name == "Red") return 2;
    if (name == "Orange" || name == "Yellow") return 3;
    if (name == "Green") return 4;
    if (name == "Cyan" || name == "Blue") return 5;
    if (name == "Purple") return 6;
    throw std::runtime_error("worksheet unknown hue role");
}

inline int index(const Plan& p, std::string_view id) {
    for (std::size_t i = 0; i < p.entities.size(); ++i)
        if (p.entities[i].id == id) return static_cast<int>(i);
    throw std::runtime_error("worksheet unresolved identity: " + std::string(id));
}

inline std::vector<std::string> caption(Json j) {
    std::vector<std::string> out;
    auto rows = j.get("caption_rows");
    require(rows.is_arr() && rows.size() <= 256, "worksheet caption rows");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto row = text(rows.at(i));
        require(!row.empty() && row.size() <= 32 &&
                    std::ranges::all_of(row, [](unsigned char c) { return c >= 32 && c <= 126; }),
                "worksheet frozen caption ASCII domain");
        out.push_back(std::move(row));
    }
    return out;
}

inline void natural(Entity& e) {
    int w = 0;
    for (const auto& row : e.rows)
        w = std::max(w, static_cast<int>(row.size()));
    const int h = static_cast<int>(e.rows.size());
    if (e.space) {
        e.min_w = 1;
        e.min_h = 5;
        return;
    }
    e.min_w = std::max(9, w + 4);
    e.min_h = std::max(5, h + 2);
    if (e.shape == "circle" || e.shape == "diamond") {
        const int side = std::max(10, w + 2 * h + 6);
        e.min_w = e.min_h = side + (side % 2);
    } else if (e.shape == "hexagon" || e.shape == "lean_right" || e.shape == "trapezoid" ||
               e.shape == "rect_left_inv_arrow") {
        e.min_w = std::max(11, w + 8);
        e.min_h = std::max(7, h + 4);
    } else if (e.shape == "stadium") {
        e.min_h = std::max(7, h + 4);
        e.min_w = std::max(e.min_h + 4, w + 8);
    } else if (e.shape == "subroutine")
        e.min_w = std::max(9, w + 8);
    else if (e.shape == "block_arrow") {
        require(e.heads == 1 || e.heads == 2 || e.heads == 4 || e.heads == 8 || e.heads == 5 ||
                    e.heads == 10 || e.heads == 15,
                "worksheet unreviewed block-arrow direction set");
        const bool horizontal = (e.heads & 5) != 0, vertical = (e.heads & 10) != 0;
        const int l = std::max((e.heads & 4) ? 3 : 0, vertical ? 1 : 0),
                  r = std::max((e.heads & 1) ? 3 : 0, vertical ? 1 : 0);
        const int t = std::max((e.heads & 8) ? 3 : 0, horizontal ? 1 : 0),
                  b = std::max((e.heads & 2) ? 3 : 0, horizontal ? 1 : 0);
        e.min_w = std::max(7, w + 4) + l + r;
        e.min_h = std::max(5, h + 2) + t + b;
    } else
        require(e.shape == "rectangle" || e.shape == "rounded_rectangle",
                "worksheet unknown shape");
}

inline int measure(Plan& p, int id, Ledger& ledger, int depth = 0) {
    require(depth <= 16, "worksheet depth");
    auto& e = p.entities.at(static_cast<std::size_t>(id));
    if (!e.group) {
        natural(e);
        return e.min_w;
    }
    int k = 0, row_count = 0, unit = 1;
    for (const auto& c : e.children) {
        ledger.pay(32, "grid measure");
        measure(p, c.child, ledger, depth + 1);
        const auto& child = p.entities.at(static_cast<std::size_t>(c.child));
        k = std::max(k, c.col + c.span);
        row_count = std::max(row_count, c.row + 1);
        unit = std::max(unit, std::max(0, child.min_w - 3 * (c.span - 1) + c.span - 1) / c.span);
    }
    require(k > 0 && k <= 64 && row_count > 0 && row_count <= 64,
            "worksheet empty or excessive grid");
    ledger.product(static_cast<std::size_t>(k + row_count), 2 * sizeof(int), "grid arrays");
    e.columns.assign(static_cast<std::size_t>(k), unit);
    e.row_heights.assign(static_cast<std::size_t>(row_count), 0);
    for (const auto& c : e.children)
        e.row_heights[static_cast<std::size_t>(c.row)] =
            std::max(e.row_heights[static_cast<std::size_t>(c.row)],
                     p.entities[static_cast<std::size_t>(c.child)].min_h);
    const int pad = id == 0 ? 2 : 4;
    e.min_w = k * unit + 3 * (k - 1) + pad;
    e.min_h =
        std::accumulate(e.row_heights.begin(), e.row_heights.end(), 0) + 3 * (row_count - 1) + pad;
    require(e.min_w <= 240 && e.min_h <= 1024, "worksheet intrinsic canvas bound");
    return e.min_w;
}

inline void place(Plan& p, int id, Box band, Ledger& ledger) {
    auto& e = p.entities[static_cast<std::size_t>(id)];
    e.band = band;
    e.body = band;
    if (!e.group) {
        if (e.shape == "circle" || e.shape == "diamond") {
            e.body.x += (band.w - e.min_w) / 2;
            e.body.w = e.min_w;
        }
        e.core = e.body;
        if (e.shape == "block_arrow") {
            const bool hz = (e.heads & 5) != 0, vt = (e.heads & 10) != 0;
            const int l = std::max((e.heads & 4) ? 3 : 0, vt ? 1 : 0),
                      r = std::max((e.heads & 1) ? 3 : 0, vt ? 1 : 0);
            const int t = std::max((e.heads & 8) ? 3 : 0, hz ? 1 : 0),
                      b = std::max((e.heads & 2) ? 3 : 0, hz ? 1 : 0);
            e.core = {e.body.x + l, e.body.y + t, e.body.w - l - r, e.body.h - t - b};
        }
        int cw = 0;
        for (const auto& row : e.rows)
            cw = std::max(cw, static_cast<int>(row.size()));
        const int ch = static_cast<int>(e.rows.size());
        e.caption = {e.core.x + (e.core.w - cw) / 2, e.core.y + (e.core.h - ch) / 2, cw, ch};
        return;
    }
    const int pad = id == 0 ? 1 : 2, k = static_cast<int>(e.columns.size());
    const int available = band.w - 2 * pad - 3 * (k - 1);
    require(available >= k, "worksheet column grant");
    for (int x = 0; x < k; ++x) {
        ledger.pay(16, "column grant");
        e.columns[static_cast<std::size_t>(x)] = available / k + (x < available % k ? 1 : 0);
    }
    for (const auto& c : e.children) {
        ledger.pay(32, "grid placement");
        int x = band.x + pad, y = band.y + pad, w = 0;
        for (int col = 0; col < c.col; ++col) {
            ledger.pay(16, "column offset");
            x += e.columns[static_cast<std::size_t>(col)] + 3;
        }
        for (int row = 0; row < c.row; ++row) {
            ledger.pay(16, "row offset");
            y += e.row_heights[static_cast<std::size_t>(row)] + 3;
        }
        for (int col = c.col; col < c.col + c.span; ++col) {
            ledger.pay(16, "span width");
            w += e.columns[static_cast<std::size_t>(col)];
        }
        w += 3 * (c.span - 1);
        const auto& child = p.entities[static_cast<std::size_t>(c.child)];
        const int height = child.min_h;
        y += (e.row_heights[static_cast<std::size_t>(c.row)] - height) / 2;
        place(p, c.child, {x, y, w, height}, ledger);
    }
}

inline bool triangle(Point p, Point a, Point b, Point c) {
    const auto cross = [](Point u, Point v, Point q) -> std::int64_t {
        return static_cast<std::int64_t>(v.x - u.x) * (q.y - u.y) -
               static_cast<std::int64_t>(v.y - u.y) * (q.x - u.x);
    };
    const auto x = cross(a, b, p), y = cross(b, c, p), z = cross(c, a, p);
    return (x >= 0 && y >= 0 && z >= 0) || (x <= 0 && y <= 0 && z <= 0);
}

inline bool polygon(Point p, std::span<const Point> vertices) {
    bool in = false;
    for (std::size_t i = 0, j = vertices.size() - 1; i < vertices.size(); j = i++) {
        const auto a = vertices[j], b = vertices[i];
        const std::int64_t cross = static_cast<std::int64_t>(b.x - a.x) * (p.y - a.y) -
                                   static_cast<std::int64_t>(b.y - a.y) * (p.x - a.x);
        if (cross == 0 && p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
            p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y))
            return true;
        if ((a.y > p.y) != (b.y > p.y)) {
            // Sign-aware integer comparison avoids floating point edge ties.
            const auto lhs = static_cast<std::int64_t>(p.x - a.x) * (b.y - a.y),
                       rhs = static_cast<std::int64_t>(b.x - a.x) * (p.y - a.y);
            if ((b.y > a.y && lhs < rhs) || (b.y < a.y && lhs > rhs)) in = !in;
        }
    }
    return in;
}

inline bool mask(const Entity& e, Point world) {
    if (!e.body.contains(world)) return false;
    const int x = world.x - e.body.x, y = world.y - e.body.y, w = e.body.w, h = e.body.h;
    if (e.group || e.shape == "rectangle" || e.shape == "rounded_rectangle" ||
        e.shape == "subroutine")
        return true;
    if (e.shape == "circle") {
        const auto dx = 2 * x - (w - 1), dy = 2 * y - (h - 1);
        return dx * dx + dy * dy <= w * w;
    }
    if (e.shape == "diamond") return std::abs(2 * x - (w - 1)) + std::abs(2 * y - (h - 1)) <= w;
    if (e.shape == "stadium") {
        const int r = (h - 1) / 2;
        if (x >= r && x <= w - 1 - r) return true;
        const int cx = x < r ? r : w - 1 - r;
        return (x - cx) * (x - cx) + (y - r) * (y - r) <= r * r;
    }
    if (e.shape == "block_arrow") {
        const auto& c = e.core;
        const Point q{world.x, world.y};
        if (c.contains(q)) return true;
        const int l = c.x, r = c.x + c.w - 1, t = c.y, b = c.y + c.h - 1, mx = l + (c.w - 1) / 2,
                  my = t + (c.h - 1) / 2;
        return ((e.heads & 1) && triangle(q, {r - 1, t - 1}, {r + 3, my}, {r - 1, b + 1})) ||
               ((e.heads & 4) && triangle(q, {l + 1, t - 1}, {l - 3, my}, {l + 1, b + 1})) ||
               ((e.heads & 8) && triangle(q, {l - 1, t + 1}, {mx, t - 3}, {r + 1, t + 1})) ||
               ((e.heads & 2) && triangle(q, {l - 1, b - 1}, {mx, b + 3}, {r + 1, b - 1}));
    }
    if (e.shape == "hexagon") {
        const std::array<Point, 6> v{
            {{2, 0}, {w - 3, 0}, {w - 1, h / 2}, {w - 3, h - 1}, {2, h - 1}, {0, h / 2}}};
        return polygon({x, y}, v);
    }
    if (e.shape == "lean_right") {
        const std::array<Point, 4> v{{{2, 0}, {w - 1, 0}, {w - 3, h - 1}, {0, h - 1}}};
        return polygon({x, y}, v);
    }
    if (e.shape == "trapezoid") {
        const std::array<Point, 4> v{{{2, 0}, {w - 3, 0}, {w - 1, h - 1}, {0, h - 1}}};
        return polygon({x, y}, v);
    }
    if (e.shape == "rect_left_inv_arrow") {
        const std::array<Point, 5> v{{{0, 0}, {2, h / 2}, {0, h - 1}, {w - 1, h - 1}, {w - 1, 0}}};
        return polygon({x, y}, v);
    }
    return false;
}

inline std::size_t offset(const Plan& p, Point q) {
    require(q.x >= 0 && q.y >= 0 && q.x < p.metrics.grid_width && q.y < p.metrics.grid_height,
            "worksheet raster bounds");
    return static_cast<std::size_t>(q.y) * static_cast<std::size_t>(p.metrics.grid_width) +
           static_cast<std::size_t>(q.x);
}

inline bool inside(const Plan& p, Point q) {
    return q.x >= 0 && q.y >= 0 && q.x < p.metrics.grid_width && q.y < p.metrics.grid_height;
}

inline bool ancestor(const Plan& p, int group, int node) {
    for (int n = p.entities[static_cast<std::size_t>(node)].parent; n >= 0;
         n = p.entities[static_cast<std::size_t>(n)].parent)
        if (n == group) return true;
    return false;
}

inline void paint(Plan& p, Ledger& ledger) {
    for (std::size_t i = 1; i < p.entities.size(); ++i) {
        auto& e = p.entities[i];
        if (e.space) continue;
        ledger.product(static_cast<std::size_t>(e.body.w * e.body.h), 8, "mask visits");
        for (int y = e.body.y; y < e.body.y + e.body.h; ++y)
            for (int x = e.body.x; x < e.body.x + e.body.w; ++x) {
                ++p.metrics.contour_visits;
                if (!mask(e, {x, y})) continue;
                ledger.pay(8, "mask write");
                ++p.metrics.contour_visits;
                auto& cell = p.cells[offset(p, {x, y})];
                cell.owner = static_cast<std::uint8_t>(i);
                cell.style = static_cast<std::uint16_t>(1 | (e.fill << 5));
            }
        ledger.product(static_cast<std::size_t>(e.body.w * e.body.h), 8, "outline visits");
        for (int y = e.body.y; y < e.body.y + e.body.h; ++y)
            for (int x = e.body.x; x < e.body.x + e.body.w; ++x) {
                ++p.metrics.contour_visits;
                auto& cell = p.cells[offset(p, {x, y})];
                if (cell.owner != i) continue;
                bool border = false;
                for (const auto d : kStep) {
                    ledger.pay(8, "outline neighbour");
                    ++p.metrics.contour_visits;
                    Point q{x + d.x, y + d.y};
                    if (!inside(p, q) || p.cells[offset(p, q)].owner != i) border = true;
                }
                const bool rail = e.shape == "subroutine" &&
                                  (x == e.body.x + 2 || x == e.body.x + e.body.w - 3) &&
                                  y > e.body.y && y < e.body.y + e.body.h - 1;
                if (border || rail) {
                    ledger.pay(8, "outline write");
                    ++p.metrics.contour_visits;
                    cell.flags |= 1;
                    cell.glyph = "│";
                    // The worksheet uses a three-byte conservative contour glyph,
                    // not a claimed terminal contour/line-glyph rendering.
                    const auto bg =
                        rail ? e.fill
                             : (e.parent > 0 ? p.entities[static_cast<std::size_t>(e.parent)].fill
                                             : 0);
                    cell.style =
                        static_cast<std::uint16_t>(e.stroke | (bg << 5) | (e.bold ? 1024 : 0));
                }
            }
        if (e.group) continue;
        for (std::size_t row = 0; row < e.rows.size(); ++row)
            for (std::size_t col = 0; col < e.rows[row].size(); ++col) {
                const Point q{e.caption.x + static_cast<int>(col),
                              e.caption.y + static_cast<int>(row)};
                require(e.body.contains(q), "worksheet caption bounding containment");
                auto& c = p.cells[offset(p, q)];
                ledger.pay(8, "caption write");
                require(c.owner == i && (c.flags & 1) == 0,
                        "worksheet caption interior containment");
                c.flags |= 2;
                c.glyph = e.rows[row][col] == ' ' ? " " : "a";
            }
        const Point anchor{e.core.x + (e.core.w - 1) / 2, e.core.y + (e.core.h - 1) / 2};
        for (int d = 0; d < 4; ++d) {
            Point q = anchor;
            require(mask(e, q), "worksheet port anchor");
            while (true) {
                Point next{q.x + kStep[static_cast<std::size_t>(d)].x,
                           q.y + kStep[static_cast<std::size_t>(d)].y};
                ledger.pay(8, "port ray");
                ++p.metrics.contour_visits;
                if (!mask(e, next)) break;
                q = next;
            }
            e.ports[static_cast<std::size_t>(d)] = q;
        }
    }
    for (auto& e : p.entities)
        if (e.group) {
            const auto& b = e.body;
            e.ports = {Point{b.x + b.w - 1, b.y + b.h / 2}, Point{b.x + b.w / 2, b.y + b.h - 1},
                       Point{b.x, b.y + b.h / 2}, Point{b.x + b.w / 2, b.y}};
        }
}

inline Point outside(const Entity& e, int direction) {
    auto q = e.ports[static_cast<std::size_t>(direction)];
    const auto d = kStep[static_cast<std::size_t>(direction)];
    do {
        q.x += d.x;
        q.y += d.y;
    } while (e.band.contains(q));
    return q;
}

inline std::vector<Point> expand(std::span<const Point> bends) {
    std::vector<Point> points;
    for (std::size_t i = 1; i < bends.size(); ++i) {
        auto q = bends[i - 1], end = bends[i];
        require(q.x == end.x || q.y == end.y, "worksheet orthogonal route");
        const Point step{(end.x > q.x) - (end.x < q.x), (end.y > q.y) - (end.y < q.y)};
        if (points.empty()) points.push_back(q);
        while (q != end) {
            q.x += step.x;
            q.y += step.y;
            points.push_back(q);
        }
    }
    return points;
}

inline bool simple_polyline(std::span<const Point> bends) {
    // At most eight bends: pairwise segment tests need no second area buffer.
    for (std::size_t i = 2; i < bends.size(); ++i) {
        const Point a{bends[i - 1].x - bends[i - 2].x, bends[i - 1].y - bends[i - 2].y};
        const Point b{bends[i].x - bends[i - 1].x, bends[i].y - bends[i - 1].y};
        if (a.x * b.y == a.y * b.x && a.x * b.x + a.y * b.y < 0) return false;
    }
    for (std::size_t i = 1; i < bends.size(); ++i)
        for (std::size_t j = i + 2; j < bends.size(); ++j) {
            const auto a = bends[i - 1], b = bends[i], c = bends[j - 1], d = bends[j];
            const int lx = std::max(std::min(a.x, b.x), std::min(c.x, d.x)),
                      rx = std::min(std::max(a.x, b.x), std::max(c.x, d.x));
            const int ly = std::max(std::min(a.y, b.y), std::min(c.y, d.y)),
                      ry = std::min(std::max(a.y, b.y), std::max(c.y, d.y));
            if (lx <= rx && ly <= ry) return false;
        }
    return true;
}

inline bool on_stem(Point q, Point port, Point out) {
    return (port.x == out.x && q.x == port.x && q.y >= std::min(port.y, out.y) &&
            q.y <= std::max(port.y, out.y)) ||
           (port.y == out.y && q.y == port.y && q.x >= std::min(port.x, out.x) &&
            q.x <= std::max(port.x, out.x));
}

inline bool valid_route(Plan& p, const Edge& e, std::span<const Point> points, int sd, int td,
                        Route& out, Ledger& ledger) {
    const auto& source = p.entities[static_cast<std::size_t>(e.source)];
    const auto& target = p.entities[static_cast<std::size_t>(e.target)];
    const auto sp = source.ports[static_cast<std::size_t>(sd)],
               tp = target.ports[static_cast<std::size_t>(td)];
    const auto so = outside(source, sd), to = outside(target, td);
    std::vector<int> required;
    for (int g = source.parent; g > 0; g = p.entities[static_cast<std::size_t>(g)].parent)
        if (!ancestor(p, g, e.target)) required.push_back(g);
    std::vector<int> entering;
    for (int g = target.parent; g > 0; g = p.entities[static_cast<std::size_t>(g)].parent)
        if (!ancestor(p, g, e.source)) entering.push_back(g);
    std::ranges::reverse(entering);
    required.insert(required.end(), entering.begin(), entering.end());
    std::vector<Gate> gates;
    for (std::size_t at = 0; at < points.size(); ++at) {
        ledger.pay(8, "route cell");
        ++p.metrics.route_visits;
        const auto q = points[at];
        if (!inside(p, q)) return false;
        const auto& cell = p.cells[offset(p, q)];
        if ((cell.flags & 4) != 0) return false;
        for (std::size_t n = 1; n < p.entities.size(); ++n) {
            ledger.pay(8, "route allocation inspection");
            const auto& box = p.entities[n];
            if (box.space || !box.band.contains(q)) continue;
            const bool endpoint =
                static_cast<int>(n) == e.source || static_cast<int>(n) == e.target;
            if (endpoint) {
                if (!on_stem(q, sp, so) && !on_stem(q, tp, to)) return false;
                continue;
            }
            if (!box.group) return false;
            if (!ancestor(p, static_cast<int>(n), e.source) &&
                !ancestor(p, static_cast<int>(n), e.target))
                return false;
            const auto& b = box.body;
            const bool vertical = q.x == b.x || q.x == b.x + b.w - 1,
                       horizontal = q.y == b.y || q.y == b.y + b.h - 1;
            if (vertical || horizontal) {
                if (vertical && horizontal) return false;
                if (std::ranges::find(required, static_cast<int>(n)) == required.end() || at == 0 ||
                    at + 1 == points.size())
                    return false;
                const auto before = points[at - 1], after = points[at + 1];
                if ((vertical && (before.y != q.y || after.y != q.y)) ||
                    (horizontal && (before.x != q.x || after.x != q.x)))
                    return false;
                gates.push_back({static_cast<int>(n), q});
            }
        }
    }
    if (gates.size() != required.size()) return false;
    for (std::size_t i = 0; i < required.size(); ++i)
        if (gates[i].group != required[i]) return false;
    Box label{};
    if (!e.label.empty()) {
        bool found = false;
        for (std::size_t i = 1; i < points.size() && !found; ++i) {
            if (points[i].y != points[i - 1].y) continue;
            std::size_t last = i;
            while (last + 1 < points.size() && points[last + 1].y == points[i].y)
                ++last;
            const int lo = std::min(points[i - 1].x, points[last].x),
                      hi = std::max(points[i - 1].x, points[last].x);
            if (hi - lo >= static_cast<int>(e.label.size()) + 4)
                for (const int dy : {-1, 1}) {
                    Box candidate{lo + (hi - lo + 1 - static_cast<int>(e.label.size())) / 2,
                                  points[i].y + dy, static_cast<int>(e.label.size()), 1};
                    bool clear = true;
                    for (int x = candidate.x - 1; x <= candidate.x + candidate.w; ++x) {
                        ledger.pay(8, "route label");
                        Point q{x, candidate.y};
                        if (!inside(p, q) || (p.cells[offset(p, q)].flags & 7) != 0) {
                            clear = false;
                            break;
                        }
                        for (std::size_t n = 1; n < p.entities.size(); ++n)
                            if (!p.entities[n].group && !p.entities[n].space &&
                                p.entities[n].band.contains(q))
                                clear = false;
                    }
                    if (clear) {
                        label = candidate;
                        found = true;
                        break;
                    }
                }
            i = last;
        }
        if (!found) return false;
    }
    out.source = e.source;
    out.target = e.target;
    out.points.assign(points.begin(), points.end());
    out.gates = std::move(gates);
    out.label = e.label;
    out.label_box = label;
    out.start_kind = e.start;
    out.end_kind = e.end;
    out.start_marker = {sp.x + kStep[static_cast<std::size_t>(sd)].x,
                        sp.y + kStep[static_cast<std::size_t>(sd)].y};
    out.end_marker = {tp.x + kStep[static_cast<std::size_t>(td)].x,
                      tp.y + kStep[static_cast<std::size_t>(td)].y};
    out.start_direction = (sd + 2) % 4;
    out.end_direction = (td + 2) % 4;
    return true;
}

inline void route_all(Plan& p, Ledger& ledger) {
    std::vector<int> xs{0, p.metrics.grid_width - 1}, ys{0, p.metrics.grid_height - 1};
    for (const auto& n : p.entities) {
        if (n.group) {
            xs.push_back(n.body.x + 1);
            xs.push_back(n.body.x + n.body.w - 2);
            ys.push_back(n.body.y + 1);
            ys.push_back(n.body.y + n.body.h - 2);
        }
        if (n.space) {
            xs.push_back(n.band.x + n.band.w / 2);
            ys.push_back(n.band.y + n.band.h / 2);
        }
        for (const auto q : n.ports) {
            xs.push_back(q.x);
            ys.push_back(q.y);
        }
    }
    std::ranges::sort(xs);
    xs.erase(std::unique(xs.begin(), xs.end()), xs.end());
    std::ranges::sort(ys);
    ys.erase(std::unique(ys.begin(), ys.end()), ys.end());
    ledger.product(xs.capacity() + ys.capacity(), sizeof(int), "route lanes");
    for (const auto& edge : p.edges) {
        struct Pair {
            int sd, td, distance, order;
        };

        std::vector<Pair> pairs;
        const auto& s = p.entities[static_cast<std::size_t>(edge.source)];
        const auto& t = p.entities[static_cast<std::size_t>(edge.target)];
        ledger.pay(2 * 16 * sizeof(Pair) + 512, "port pair storage and ordering");
        constexpr std::array<int, 4> target_order{2, 3, 0, 1};
        for (int sd = 0; sd < 4; ++sd)
            for (int rank = 0; rank < 4; ++rank) {
                const int td = target_order[static_cast<std::size_t>(rank)];
                const auto a = s.ports[static_cast<std::size_t>(sd)],
                           b = t.ports[static_cast<std::size_t>(td)];
                pairs.push_back({sd, td, std::abs(a.x - b.x) + std::abs(a.y - b.y), sd * 4 + rank});
            }
        std::ranges::sort(pairs, [](const Pair& a, const Pair& b) {
            return std::tie(a.distance, a.order) < std::tie(b.distance, b.order);
        });
        bool done = false;
        Route selected;
        for (const auto pair : pairs) {
            ledger.pay(16, "port pair");
            if ((p.cells[offset(p, s.ports[static_cast<std::size_t>(pair.sd)])].flags & 4) != 0 ||
                (p.cells[offset(p, t.ports[static_cast<std::size_t>(pair.td)])].flags & 4) != 0)
                continue;
            const auto a = outside(s, pair.sd), b = outside(t, pair.td);
            const auto attempt = [&](std::vector<Point> mid) {
                if (done) return;
                ++p.metrics.route_attempts;
                std::vector<Point> bends{s.ports[static_cast<std::size_t>(pair.sd)]};
                bends.insert(bends.end(), mid.begin(), mid.end());
                bends.push_back(t.ports[static_cast<std::size_t>(pair.td)]);
                bends.erase(std::unique(bends.begin(), bends.end()), bends.end());
                ledger.pay(256 + 2 * 17 * (sizeof(int) + sizeof(Gate)), "candidate bends/gates");
                if (!simple_polyline(bends)) return;
                std::size_t length = 1;
                for (std::size_t i = 1; i < bends.size(); ++i)
                    length += static_cast<std::size_t>(std::abs(bends[i].x - bends[i - 1].x) +
                                                       std::abs(bends[i].y - bends[i - 1].y));
                ledger.product(length, 4 * sizeof(Point), "route candidate and retained storage");
                auto points = expand(bends);
                if (valid_route(p, edge, points, pair.sd, pair.td, selected, ledger)) done = true;
            };
            attempt({a, {b.x, a.y}, b});
            attempt({a, {a.x, b.y}, b});
            for (int x : xs)
                attempt({a, {x, a.y}, {x, b.y}, b});
            for (int y : ys)
                attempt({a, {a.x, y}, {b.x, y}, b});
            if (done) break;
        }
        require(done, "worksheet route unavailable: " + s.id + " -> " + t.id);
        for (const auto q : selected.points) {
            ledger.pay(8, "route raster write");
            auto& c = p.cells[offset(p, q)];
            c.flags |= 4;
            c.glyph = "│";
            c.style = static_cast<std::uint16_t>(1 | (c.style & 992) | (edge.bold ? 1024 : 0));
        }
        for (int x = selected.label_box.x; x < selected.label_box.x + selected.label_box.w; ++x) {
            ledger.pay(8, "route label write");
            auto& c = p.cells[offset(p, {x, selected.label_box.y})];
            c.flags |= 2;
            c.style = 1;
            c.glyph = "a";
        }
        p.routes.push_back(std::move(selected));
    }
}

inline Plan build(Json facts, std::size_t source_bytes, OracleWork& oracle,
                  Json source_model = Json{}) {
    require(facts.is_obj() && source_bytes <= 65536, "worksheet model/source bounds");
    Plan p;
    Ledger ledger;
    ledger.pay(source_bytes * 13 + 792 + 1024 * sizeof(ParserSpan) + 256 * sizeof(StylePair) +
                   64 * sizeof(Entity) + 64 * sizeof(Edge),
               "source and fixed scratch");
    const auto groups = facts.get("root_and_groups"), nodes = facts.get("nodes"),
               spaces = facts.get("spacers");
    require(groups.is_arr() && groups.size() > 0 && groups.size() <= 65 && nodes.is_arr() &&
                nodes.size() <= 64 && spaces.is_arr() && spaces.size() <= 64,
            "worksheet model population");
    oracle.spend(512 * 65536);
    p.entities.reserve(65);
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto g = groups.at(i);
        Entity n;
        n.id = text(g.get("handle"));
        n.shape = "group";
        n.group = true;
        n.fill = 6;
        n.rows = caption(g.get("caption"));
        require(n.rows.empty(), "worksheet closed source group caption profile");
        if (i) n.span = number(g.get("retained_span"));
        p.entities.push_back(std::move(n));
    }
    require(p.entities[0].id == "@root", "worksheet root identity");
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto j = nodes.at(i);
        Entity n;
        n.id = text(j.get("handle"));
        n.shape = text(j.get("resolved_display_shape"));
        n.rows = caption(j.get("caption"));
        n.span = number(j.get("retained_span"));
        n.parent = index(p, text(j.get("parent_scope")));
        const auto heads = j.get("resolved_direction_heads");
        for (std::size_t k = 0; k < heads.size(); ++k) {
            const auto d = text(heads.at(k));
            if (d == "right")
                n.heads |= 1;
            else if (d == "down")
                n.heads |= 2;
            else if (d == "left")
                n.heads |= 4;
            else if (d == "up")
                n.heads |= 8;
            else
                throw std::runtime_error("worksheet direction");
        }
        p.entities.push_back(std::move(n));
    }
    for (std::size_t i = 0; i < spaces.size(); ++i) {
        const auto j = spaces.at(i), slots = j.get("slots");
        for (std::size_t s = 0; s < slots.size(); ++s) {
            Entity n;
            n.id = text(slots.at(s));
            n.space = true;
            n.parent = index(p, text(j.get("parent")));
            p.entities.push_back(std::move(n));
        }
    }
    require(p.entities.size() <= 65, "worksheet entity cap");
    ledger.product(p.entities.capacity(), 2 * sizeof(Entity), "owned model");
    std::size_t captions_bytes = 0, id_bytes = 0, max_id = 0;
    for (const auto& n : p.entities) {
        id_bytes += n.id.size();
        max_id = std::max(max_id, n.id.size());
        for (const auto& row : n.rows)
            captions_bytes += row.size();
    }
    ledger.product(captions_bytes, 10, "caption copies/glyph scans");
    ledger.product(id_bytes, 2, "owned identity copies");
    if (source_model.valid()) {
        const auto declarations = source_model.get("declarations");
        require(declarations.is_arr() && declarations.size() <= 1024,
                "worksheet source declaration cap");
        ledger.product(declarations.size() * p.entities.size(), 1 + max_id,
                       "identity comparison upper");
    }
    for (std::size_t i = 1; i < groups.size(); ++i)
        p.entities[i].parent = index(p, text(groups.at(i).get("parent_scope")));
    std::array<int, 65> placed{};
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto g = groups.at(i);
        const auto grid = g.get("logical_source_grid"), placements = grid.get("placements");
        const int columns = number(grid.get("columns"));
        int position = 0;
        require((columns == -1 || (columns > 0 && columns <= 64)) && placements.is_arr() &&
                    placements.size() <= 64,
                "worksheet source columns/placement bounds");
        for (std::size_t k = 0; k < placements.size(); ++k) {
            const auto q = placements.at(k);
            const int span = number(q.get("retained_span")),
                      col = columns < 0 ? position : position % columns,
                      row = columns < 0 ? 0 : position / columns;
            require(span > 0 && span <= 64 && number(q.get("logical_column_index")) == col &&
                        number(q.get("logical_row_index")) == row,
                    "worksheet source placement arithmetic");
            const int child = index(p, text(q.get("handle")));
            require(p.entities[static_cast<std::size_t>(child)].parent == static_cast<int>(i),
                    "worksheet first parent");
            require(child > 0 && ++placed[static_cast<std::size_t>(child)] == 1 &&
                        p.entities[static_cast<std::size_t>(child)].span == span,
                    "worksheet unique first placement/span");
            p.entities[i].children.push_back({child, col, row, span});
            position += columns < 0 ? span : std::min(span, columns - position % columns);
        }
    }
    for (std::size_t i = 1; i < p.entities.size(); ++i)
        require(placed[i] == 1, "worksheet unplaced entity");
    const auto styles = facts.get("styles");
    for (std::size_t i = 0; i < styles.size(); ++i) {
        const auto st = styles.at(i);
        auto& n = p.entities[static_cast<std::size_t>(index(p, text(st.get("handle"))))];
        ledger.product(st.get("ordered_property_atoms").size() + 1, 16, "atomic style visits");
        const auto props = st.get("resolved_authored_properties");
        for (const auto key : {"fill", "stroke"})
            if (const auto v = props.get(key); v.valid()) {
                const auto value = v.get("value");
                const auto r = static_cast<std::uint16_t>(role(text(value.get("hue_role"))));
                if (std::string_view(key) == "fill")
                    n.fill = r;
                else
                    n.stroke = r;
            }
        if (const auto width = props.get("stroke-width"); width.valid())
            n.bold = width.get("value").get_string("border_emphasis") == "bold";
    }
    const auto edges = facts.get("edges");
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const auto e = edges.at(i);
        Edge edge;
        edge.source = index(p, text(e.get("source")));
        edge.target = index(p, text(e.get("target")));
        edge.start = text(e.get("resolved_marker_start"));
        edge.end = text(e.get("resolved_marker_end"));
        edge.bold = e.get_string("stroke_emphasis") == "bold";
        edge.dotted = e.get_string("pattern") == "dotted";
        const auto rows = caption(e.get("caption"));
        require(rows.size() <= 1, "worksheet edge label rows");
        if (!rows.empty()) edge.label = rows.front();
        p.edges.push_back(std::move(edge));
    }
    measure(p, 0, ledger);
    place(p, 0, {0, 0, p.entities[0].min_w, p.entities[0].min_h}, ledger);
    p.metrics.width = p.metrics.grid_width = p.entities[0].min_w;
    p.metrics.grid_height = p.entities[0].min_h;
    p.metrics.area = static_cast<std::size_t>(p.metrics.grid_width) *
                     static_cast<std::size_t>(p.metrics.grid_height);
    ledger.product(p.metrics.area, sizeof(Cell) + 2 * sizeof(std::uint16_t),
                   "raster and copied plane");
    ledger.pay(static_cast<std::size_t>(p.metrics.grid_height) * 4 * sizeof(std::string) + 1024,
               "grid rows");
    p.cells.resize(p.metrics.area);
    paint(p, ledger);
    route_all(p, ledger);
    std::size_t row_runs = 0;
    for (int y = 0; y < p.metrics.grid_height; ++y)
        for (int x = 0; x < p.metrics.grid_width; ++x) {
            if (x == 0 || p.cells[offset(p, {x, y})].style != p.cells[offset(p, {x - 1, y})].style)
                ++row_runs;
        }
    for (const auto key :
         {"required_metadata_notices", "required_slot_legend",
          "required_source_style_declaration_legend", "required_style_loss_notices"}) {
        const auto records = facts.get(key);
        require(records.is_arr() && records.size() <= 512, "worksheet note record bound");
        for (std::size_t i = 0; i < records.size(); ++i) {
            ++p.metrics.note_records;
            p.notes.emplace_back();
            const auto rows = records.at(i).get("rows");
            require(rows.is_arr() && rows.size() <= 256, "worksheet note rows bound");
            for (std::size_t r = 0; r < rows.size(); ++r) {
                auto line = text(rows.at(r));
                require(line.size() <= 32 && p.notes.size() < 1024, "worksheet note width/height");
                p.metrics.width = std::max(p.metrics.width, static_cast<int>(line.size()));
                p.metrics.note_cells += line.size();
                ++p.metrics.note_rows;
                p.notes.push_back(std::move(line));
            }
        }
    }
    p.metrics.height = p.metrics.grid_height + static_cast<int>(p.notes.size());
    require(p.metrics.height <= 1024, "worksheet total height including notes");
    ledger.product(p.notes.size(), 4 * sizeof(std::string), "note row storage");
    p.metrics.runs = row_runs + p.metrics.note_rows;
    p.metrics.serialized_bytes_upper = 3 * p.metrics.area + p.metrics.note_cells;
    ledger.product(p.metrics.note_cells, 2 * sizeof(std::uint16_t), "note copied plane");
    ledger.product(p.metrics.runs, 512 + 8 * sizeof(std::string), "fresh element runs");
    ledger.product(static_cast<std::size_t>(p.metrics.height), 256 + 4 * sizeof(ftxui::Element),
                   "fresh element rows");
    ledger.product(p.metrics.serialized_bytes_upper, 2, "serialization and copied glyph bytes");
    ledger.product(p.metrics.area, 2, "serialization and whole-glyph scans");
    p.metrics.production_upper = ledger.used;
    p.metrics.oracle_remaining = oracle.left;
    return p;
}

inline std::uint16_t route_background(const Plan& p, Point q) {
    // Reconstruct compositing from source geometry, not the mutable cell's owner
    // or role. Groups are source preorder; leaves follow all groups.
    std::uint16_t bg = 0;
    for (std::size_t i = 1; i < p.entities.size(); ++i) {
        const auto& n = p.entities[i];
        if (n.space) continue;
        if (n.group && n.body.contains(q)) {
            const auto& b = n.body;
            const bool border =
                q.x == b.x || q.x == b.x + b.w - 1 || q.y == b.y || q.y == b.y + b.h - 1;
            bg = border ? (n.parent > 0 ? p.entities[static_cast<std::size_t>(n.parent)].fill : 0)
                        : n.fill;
        } else if (!n.group && mask(n, q))
            bg = n.parent > 0 ? p.entities[static_cast<std::size_t>(n.parent)].fill : 0;
    }
    return bg;
}

inline std::vector<Gate> required_crossings(const Plan& p, const Route& r) {
    std::vector<int> required, entering;
    for (int g = p.entities[static_cast<std::size_t>(r.source)].parent; g > 0;
         g = p.entities[static_cast<std::size_t>(g)].parent)
        if (!ancestor(p, g, r.target)) required.push_back(g);
    for (int g = p.entities[static_cast<std::size_t>(r.target)].parent; g > 0;
         g = p.entities[static_cast<std::size_t>(g)].parent)
        if (!ancestor(p, g, r.source)) entering.push_back(g);
    std::ranges::reverse(entering);
    required.insert(required.end(), entering.begin(), entering.end());
    std::vector<Gate> observed;
    for (std::size_t at = 0; at < r.points.size(); ++at)
        for (std::size_t i = 1; i < p.entities.size(); ++i) {
            const auto& n = p.entities[i];
            if (!n.group || static_cast<int>(i) == r.source || static_cast<int>(i) == r.target)
                continue;
            const auto q = r.points[at];
            const auto& b = n.body;
            if (!b.contains(q)) continue;
            const bool vertical = q.x == b.x || q.x == b.x + b.w - 1,
                       horizontal = q.y == b.y || q.y == b.y + b.h - 1;
            if (!vertical && !horizontal) continue;
            require(!(vertical && horizontal) && at > 0 && at + 1 < r.points.size(),
                    "worksheet invalid container crossing");
            const auto before = r.points[at - 1], after = r.points[at + 1];
            require((vertical && before.y == q.y && after.y == q.y && before.x != after.x) ||
                        (horizontal && before.x == q.x && after.x == q.x && before.y != after.y),
                    "worksheet nonreciprocal container crossing");
            observed.push_back({static_cast<int>(i), q});
        }
    require(observed.size() == required.size(), "worksheet required ancestor gate count");
    for (std::size_t i = 0; i < required.size(); ++i)
        require(observed[i].group == required[i], "worksheet ancestor gate order");
    return observed;
}

inline void validate(const Plan& p) {
    require(p.metrics.grid_width > 2 && p.metrics.grid_width <= 240 && p.metrics.grid_height > 2 &&
                p.metrics.height <= 1024,
            "worksheet plan dimensions");
    require(p.cells.size() ==
                static_cast<std::size_t>(p.metrics.grid_width * p.metrics.grid_height),
            "worksheet plan raster size");
    for (const auto& c : p.cells)
        require((c.style & 0xf800) == 0 && (c.style & 31) <= 6 && ((c.style >> 5) & 31) <= 6,
                "worksheet unknown packed role/bits");
    for (std::size_t i = 1; i < p.entities.size(); ++i) {
        const auto& n = p.entities[i];
        require(n.parent >= 0 && static_cast<std::size_t>(n.parent) < p.entities.size(),
                "worksheet plan parent");
        const auto& parent = p.entities[static_cast<std::size_t>(n.parent)];
        const int inset = n.parent == 0 ? 1 : 2;
        require(n.band.x >= parent.band.x + inset && n.band.y >= parent.band.y + inset &&
                    n.band.x + n.band.w <= parent.band.x + parent.band.w - inset &&
                    n.band.y + n.band.h <= parent.band.y + parent.band.h - inset,
                "worksheet plan containment");
        if (n.space) continue;
        for (int y = n.body.y; y < n.body.y + n.body.h; ++y)
            for (int x = n.body.x; x < n.body.x + n.body.w; ++x) {
                const Point q{x, y};
                const auto& c = p.cells[offset(p, q)];
                if (!n.group && mask(n, q))
                    require(c.owner == i, "worksheet plan missing mask cell");
                if (n.group && (x == n.body.x || x == n.body.x + n.body.w - 1 || y == n.body.y ||
                                y == n.body.y + n.body.h - 1))
                    require((c.flags & 1) != 0, "worksheet plan open container");
                if (c.owner == i && (c.flags & 4) == 0) {
                    const bool internal_rail =
                        n.shape == "subroutine" &&
                        (x == n.body.x + 2 || x == n.body.x + n.body.w - 3) && y > n.body.y &&
                        y < n.body.y + n.body.h - 1;
                    const auto parent_bg =
                        internal_rail
                            ? n.fill
                            : (n.parent > 0 ? p.entities[static_cast<std::size_t>(n.parent)].fill
                                            : 0);
                    const auto expected = static_cast<std::uint16_t>(
                        (c.flags & 1) ? (n.stroke | (parent_bg << 5) | (n.bold ? 1024 : 0))
                                      : (1 | (n.fill << 5)));
                    require(c.style == expected, "worksheet plan style/blank fill mismatch");
                }
            }
        for (std::size_t r = 0; r < n.rows.size(); ++r)
            for (std::size_t c = 0; c < n.rows[r].size(); ++c)
                require((p.cells[offset(p, {n.caption.x + static_cast<int>(c),
                                            n.caption.y + static_cast<int>(r)})]
                             .flags &
                         2) != 0,
                        "worksheet plan missing caption cell");
    }
    require(p.routes.size() == p.edges.size(), "worksheet plan missing edge");
    for (std::size_t number = 0; number < p.routes.size(); ++number) {
        const auto& r = p.routes[number];
        const auto& e = p.edges[number];
        require(r.source == e.source && r.target == e.target && r.start_kind == e.start &&
                    r.end_kind == e.end && r.label == e.label,
                "worksheet plan edge identity/markers");
        require(r.points.size() > 1, "worksheet plan empty route");
        require(std::ranges::find(p.entities[static_cast<std::size_t>(r.source)].ports,
                                  r.points.front()) !=
                        p.entities[static_cast<std::size_t>(r.source)].ports.end() &&
                    std::ranges::find(p.entities[static_cast<std::size_t>(r.target)].ports,
                                      r.points.back()) !=
                        p.entities[static_cast<std::size_t>(r.target)].ports.end(),
                "worksheet plan endpoint retarget");
        for (std::size_t i = 0; i < r.points.size(); ++i) {
            const auto q = r.points[i];
            require((p.cells[offset(p, q)].flags & 4) != 0, "worksheet plan route hole");
            const auto expected_style =
                static_cast<std::uint16_t>(1 | (route_background(p, q) << 5) | (e.bold ? 1024 : 0));
            require(p.cells[offset(p, q)].style == expected_style,
                    "worksheet route foreground/background/bold mismatch");
            if (i)
                require(std::abs(q.x - r.points[i - 1].x) + std::abs(q.y - r.points[i - 1].y) == 1,
                        "worksheet plan disconnected route");
        }
        if (r.start_kind == "point")
            require(r.start_direction >= 0 && r.start_direction < 4 &&
                        r.points.front() ==
                            Point{r.start_marker.x +
                                      kStep[static_cast<std::size_t>(r.start_direction)].x,
                                  r.start_marker.y +
                                      kStep[static_cast<std::size_t>(r.start_direction)].y},
                    "worksheet start marker faces endpoint");
        if (r.end_kind == "point")
            require(
                r.end_direction >= 0 && r.end_direction < 4 &&
                    r.points.back() ==
                        Point{r.end_marker.x + kStep[static_cast<std::size_t>(r.end_direction)].x,
                              r.end_marker.y + kStep[static_cast<std::size_t>(r.end_direction)].y},
                "worksheet end marker faces endpoint");
        const auto crossings = required_crossings(p, r);
        require(crossings.size() == r.gates.size(), "worksheet missing gate evidence");
        for (std::size_t i = 0; i < crossings.size(); ++i) {
            const auto g = r.gates[i];
            require(g.group == crossings[i].group && g.at == crossings[i].at,
                    "worksheet gate evidence location/order");
            require((p.cells[offset(p, g.at)].flags & 5) == 5, "worksheet reciprocal gate missing");
        }
    }
}
}  // namespace loom::test::mermaid_block58::projection
