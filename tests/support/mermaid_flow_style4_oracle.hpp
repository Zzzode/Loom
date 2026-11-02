// Restricted source-owned three-rectangle Flow oracle. No producer geometry.
#pragma once
#include "mermaid_class_style13_oracle.hpp"
#include "mermaid_flow_style4_authority.hpp"

namespace loom::test::mermaid_flow::style4 {
using Attributes = loom::test::mermaid_class_style13::Attributes;

struct Screen {
    Grid glyphs;
    std::vector<Attributes> attributes;
};
enum class Outcome { Match, Mismatch, Inconclusive };
enum class Reason { Complete, Domain, Geometry, Legend, Attributes, Footer, Resource, Unexpected };

struct Result {
    Outcome outcome = Outcome::Inconclusive;
    Reason reason = Reason::Unexpected;
    bool resource = false;
    std::array<ShapeFrame, 3> boxes{};
    int legend_row = -1;
};

inline constexpr std::string_view kProjectionKey =
    "key: hue roles use palette; width>=4=bold border; color on fill not projected; assignment "
    "order; direct last; normalized consecutive repeats keep first spelling; source TAB=\\t";

inline bool resource_message(std::string_view why) {
    return why.find("work exhausted") != std::string_view::npos ||
           why.find("bounds exceeded") != std::string_view::npos ||
           why.find("Too many typed") != std::string_view::npos ||
           why.find("matching exhausted") != std::string_view::npos;
}

inline std::vector<std::string> legend(const Expected& expected, Work& work) {
    require(expected.selected < 4u, "FlowStyle4 legend domain");
    const bool small = expected.selected == 2u;
    require(expected.raw_records.size() == (small ? 3u : 2u),
            "FlowStyle4 complete source record count");
    work.spend(4u * 4u * sizeof(std::string) + 32768u);
    std::vector<std::string> rows;
    rows.reserve(4u);
    for (const auto& raw : expected.raw_records) {
        require(raw.size() <= 1024u, "FlowStyle4 raw record byte bound");
        work.spend(raw.size() * 16u + 1024u);
        std::string escaped;
        escaped.reserve(raw.size() * 2u + 256u);
        for (const auto c : raw) {
            if (c == '\t')
                escaped += "\\t";
            else
                escaped += c;
        }
        rows.push_back(std::move(escaped));
    }
    if (small) {
        rows[0] += " -> definition:highlight (applied)";
        rows[1] += " -> class:highlight fill:Orange(shared warning) stroke:Neutral width:normal";
        rows[2] += " -> fill:Green stroke:Neutral width:normal";
    } else {
        rows[0] += " -> definition:dark (applied)";
        rows[1] += " -> class:dark fill:Neutral stroke:Neutral width:bold fg:Neutral(fill targets "
                   "use palette.text)";
    }
    rows.emplace_back(kProjectionKey);
    return rows;
}

inline ftxui::Color fill_color(Fill fill, const loom::ui::design::tokens::Palette& palette) {
    switch (fill) {
    case Fill::Neutral:
        return palette.surface_tint_2;
    case Fill::Orange:
        return palette.warning_tint_bg;
    case Fill::Green:
        return palette.success_tint_bg;
    default:
        return ftxui::Color::Default;
    }
}

inline Result check(const Screen& screen, const Expected& expected,
                    const loom::ui::design::tokens::Palette& palette, Work& work,
                    std::string_view footer) {
    try {
        work.spend(1024u);
        const auto& grid = screen.glyphs;
        if (grid.width <= 0 || grid.width > 240 || grid.height <= 0 || grid.height > 1040 ||
            grid.cells.size() != static_cast<std::size_t>(grid.width * grid.height) ||
            screen.attributes.size() != grid.cells.size())
            return {Outcome::Inconclusive, Reason::Domain, true};
        if (expected.selected >= 4u ||
            (expected.domain != Domain::Original && expected.domain != Domain::BodyDiagnostic) ||
            expected.graph.nodes.size() != 3u || expected.graph.edges.size() != 2u ||
            !expected.graph.groups.empty())
            return {Outcome::Inconclusive, Reason::Domain, false};
        const bool small = expected.selected == 2u;
        const auto& model = expected.graph;
        if (model.direction != (small ? "TD" : "LR"))
            return {Outcome::Inconclusive, Reason::Domain, false};
        const std::array<std::string_view, 3> ids =
            small ? std::array<std::string_view, 3>{{"A", "B", "C"}}
                  : std::array<std::string_view, 3>{{"Lorem", "Ipsum", "Dolor"}};
        const std::array<std::string_view, 3> labels =
            small ? std::array<std::string_view, 3>{{"Start", "Process", "End"}} : ids;
        for (std::size_t n = 0; n < 3u; ++n) {
            const auto& node = model.nodes[n];
            if (node.id != ids[n] || node.label != labels[n] || node.shape != Shape::Rectangle)
                return {Outcome::Inconclusive, Reason::Domain, false};
            const NodeStyle wanted =
                small ? (n == 0u ? NodeStyle{}
                                 : NodeStyle{n == 1u ? Fill::Orange : Fill::Green, true, false})
                      : (n == 1u ? NodeStyle{} : NodeStyle{Fill::Neutral, true, true});
            if (expected.styles[n].fill != wanted.fill ||
                expected.styles[n].stroke != wanted.stroke ||
                expected.styles[n].bold != wanted.bold)
                return {Outcome::Inconclusive, Reason::Domain, false};
        }
        for (std::size_t n = 0; n < 2u; ++n) {
            const auto& edge = model.edges[n];
            if (edge.from != static_cast<int>(n) || edge.to != static_cast<int>(n + 1u) ||
                !edge.label.empty() || edge.style != "solid" || edge.start || !edge.end)
                return {Outcome::Inconclusive, Reason::Domain, false};
        }
        if (footer.empty() || footer.size() > 240u)
            return {Outcome::Inconclusive, Reason::Domain, false};
        for (const auto c : footer)
            if (static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) > 0x7eu)
                return {Outcome::Inconclusive, Reason::Domain, false};
        work.product(grid.cells.size(), 16u);
        for (const auto& cell : grid.cells) {
            if (cell.size() > 64u) return {Outcome::Inconclusive, Reason::Domain, true};
            work.spend(cell.size() * 16u + 8u);
        }
        const auto frames = closed_frames(grid, work);
        if (frames.size() != 3u) return {Outcome::Mismatch, Reason::Geometry, false};
        Result result;
        result.reason = Reason::Geometry;
        result.outcome = Outcome::Mismatch;
        std::array<bool, 3> used{};
        int bottom = -1, left = grid.width;
        for (std::size_t n = 0; n < 3u; ++n) {
            auto candidates =
                frame_candidates(grid, frames, model.nodes[n].label, Shape::Rectangle, work);
            std::erase_if(candidates, [&](int at) {
                return !complete_node_content(grid, frames[static_cast<std::size_t>(at)],
                                              model.nodes[n].label, work);
            });
            if (candidates.size() != 1u) return result;
            const auto selected = static_cast<std::size_t>(candidates.front());
            if (selected >= 3u || used[selected]) return result;
            used[selected] = true;
            result.boxes[n] = frames[selected];
            bottom = std::max(bottom, frames[selected].bottom);
            left = std::min(left, frames[selected].left);
            if (!exact_box(grid, frames[selected], work)) return result;
        }
        const auto wanted_legend = legend(expected, work);
        if (bottom + static_cast<int>(wanted_legend.size()) >= grid.height || left < 0)
            return {Outcome::Inconclusive, Reason::Domain, true};
        result.legend_row = bottom + 1;
        for (std::size_t n = 0; n < wanted_legend.size(); ++n) {
            const auto& line = wanted_legend[n];
            if (line.size() > 240u || line.size() > static_cast<std::size_t>(grid.width - left))
                return {Outcome::Inconclusive, Reason::Domain, true};
            for (int x = 0; x < grid.width; ++x) {
                work.spend(4u);
                const auto at = x - left;
                const auto wanted = at >= 0 && static_cast<std::size_t>(at) < line.size()
                                        ? std::string_view(line.data() + at, 1u)
                                        : std::string_view(" ");
                if (grid.at({x, bottom + 1 + static_cast<int>(n)}) != wanted) {
                    result.reason = Reason::Legend;
                    return result;
                }
            }
        }
        std::optional<int> following;
        for (int y = 0; y < grid.height; ++y) {
            work.spend(static_cast<std::size_t>(grid.width) * 4u + footer.size() + 64u);
            const auto row = grid.row(y);
            if (row.find(footer) != std::string::npos) {
                if (following || trim_record(row) != footer) {
                    result.reason = Reason::Footer;
                    return result;
                }
                following = y;
            }
        }
        if (!following || *following <= bottom + static_cast<int>(wanted_legend.size())) {
            result.reason = Reason::Footer;
            return result;
        }
        for (int y = *following + 1; y < grid.height; ++y)
            for (int x = 0; x < grid.width; ++x) {
                work.spend();
                if (grid.at({x, y}) != " " && !grid.at({x, y}).empty()) {
                    result.reason = Reason::Footer;
                    return result;
                }
            }
        work.product(grid.cells.size(), 3u * sizeof(std::string) + 16u);
        for (const auto& cell : grid.cells)
            work.spend(cell.size() * 4u);
        auto geometry = grid;
        for (std::size_t n = 0; n < wanted_legend.size(); ++n)
            for (int x = 0; x < grid.width; ++x) {
                work.spend(4u);
                geometry.put({x, bottom + 1 + static_cast<int>(n)}, " ");
            }
        const int footer_row = *following;
        const auto errors = validate_impl(geometry, model, work, nullptr, &footer_row);
        if (!errors.empty()) {
            const bool resource = std::ranges::any_of(
                errors, [](const auto& error) { return resource_message(error); });
            result.outcome = resource ? Outcome::Inconclusive : Outcome::Mismatch;
            result.reason = resource ? Reason::Resource : Reason::Geometry;
            result.resource = resource;
            return result;
        }
        for (int y = 0; y < grid.height; ++y)
            for (int x = 0; x < grid.width; ++x) {
                work.spend(32u);
                Attributes wanted;
                for (std::size_t n = 0; n < 3u; ++n) {
                    const auto& box = result.boxes[n];
                    if (x < box.left || x > box.right || y < box.top || y > box.bottom) continue;
                    const auto& style = expected.styles[n];
                    const bool border =
                        x == box.left || x == box.right || y == box.top || y == box.bottom;
                    if (style.fill == Fill::None) continue;
                    wanted.foreground = palette.text;
                    wanted.background =
                        border ? ftxui::Color::Default : fill_color(style.fill, palette);
                    wanted.flags = border && style.bold ? 1u : 0u;
                }
                if (screen.attributes[static_cast<std::size_t>(grid.index({x, y}))] != wanted) {
                    result.reason = Reason::Attributes;
                    return result;
                }
            }
        result.outcome = Outcome::Match;
        result.reason = Reason::Complete;
        return result;
    } catch (const std::bad_alloc&) {
        return {Outcome::Inconclusive, Reason::Resource, true};
    } catch (const std::exception& error) {
        const bool resource = work.remaining == 0u || resource_message(error.what());
        return {Outcome::Inconclusive, resource ? Reason::Resource : Reason::Unexpected, resource};
    } catch (...) {
        return {Outcome::Inconclusive, Reason::Unexpected, false};
    }
}

inline Screen capture(ftxui::Element element, int width, int height, Work& work) {
    require(width > 0 && width <= 240 && height > 0 && height <= 1040,
            "FlowStyle4 capture grid bounds");
    const auto count = static_cast<std::size_t>(width * height);
    work.product(count, 4u * sizeof(ftxui::Pixel) + 4u * sizeof(Attributes) +
                            4u * sizeof(std::string) + 64u);
    work.spend(8192u);
    auto actual =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(actual, std::move(element));
    Screen result;
    result.glyphs = {width, height, {}};
    result.glyphs.cells.reserve(count);
    result.attributes.reserve(count);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& pixel = actual.PixelAt(x, y);
            require(pixel.character.size() <= 64u, "FlowStyle4 captured cell bounds");
            work.spend(pixel.character.size() * 4u + 32u);
            result.glyphs.cells.push_back(pixel.character);
            result.attributes.push_back(loom::test::mermaid_class_style13::attributes(pixel));
        }
    return result;
}

inline Screen copy_screen(const Screen& screen, Work& work) {
    work.product(screen.glyphs.cells.size(),
                 3u * sizeof(Attributes) + 3u * sizeof(std::string) + 32u);
    for (const auto& cell : screen.glyphs.cells) {
        require(cell.size() <= 64u, "FlowStyle4 copied cell bound");
        work.spend(cell.size() * 4u);
    }
    return screen;
}

inline constexpr std::size_t kPhysicalNegatives = 8u;

inline void mutate(Screen& screen, const Result& good, std::size_t kind, const Expected& expected,
                   Work& work) {
    work.spend(8192u);
    const auto styled = expected.selected == 2u ? 1u : 0u,
               plain = expected.selected == 2u ? 0u : 1u;
    const auto& box = good.boxes[styled];
    const auto attr = [&](int x, int y) -> Attributes& {
        return screen.attributes.at(static_cast<std::size_t>(screen.glyphs.index({x, y})));
    };
    if (kind == 0u) {
        const auto& first = good.boxes[0];
        bool changed = false;
        for (int y = first.top + 1; y < first.bottom && !changed; ++y)
            for (int x = first.left + 1; x < first.right; ++x) {
                work.spend();
                const auto cell = screen.glyphs.at({x, y});
                if (cell != " " && !cell.empty()) {
                    screen.glyphs.put({x, y}, "X");
                    changed = true;
                    break;
                }
            }
        require(changed, "FlowStyle4 decisive label target");
    } else if (kind == 1u) {
        bool changed = false;
        for (int y = 0; y < good.legend_row && !changed; ++y)
            for (int x = 0; x < screen.glyphs.width; ++x) {
                work.spend();
                const auto head = glyph(screen.glyphs.at({x, y})).arrow;
                if (head >= 0) {
                    screen.glyphs.put({x, y}, head == 0   ? "▼"
                                              : head == 1 ? "◀"
                                              : head == 2 ? "▲"
                                                          : "▶");
                    changed = true;
                    break;
                }
            }
        require(changed, "FlowStyle4 decisive arrow target");
    } else if (kind == 2u) {
        bool changed = false;
        for (int y = 0; y < good.legend_row && !changed; ++y)
            for (int x = 0; x < screen.glyphs.width; ++x) {
                work.spend();
                const Point p{x, y};
                if (std::ranges::any_of(good.boxes, [&](const auto& frame) {
                        return frame.interior(p) || frame.boundary(p);
                    }))
                    continue;
                const auto cell = screen.glyphs.at(p);
                if (cell == "─" || cell == "│") {
                    screen.glyphs.put(p, " ");
                    changed = true;
                    break;
                }
            }
        require(changed, "FlowStyle4 decisive route target");
    } else if (kind == 3u)
        attr(box.left + 1, box.top + 1).background = ftxui::Color::Default;
    else if (kind == 4u)
        attr(box.left, box.top).flags ^= 1u;
    else if (kind == 5u) {
        const auto& other = good.boxes[plain];
        attr(other.left + 1, other.top + 1).flags ^= 2u;
    } else if (kind == 6u)
        screen.glyphs.put({0, good.legend_row}, "!");
    else if (kind == 7u)
        screen.glyphs.put({screen.glyphs.width - 1, screen.glyphs.height - 1}, "!");
    else
        throw std::runtime_error("FlowStyle4 unknown physical adversary");
}
}  // namespace loom::test::mermaid_flow::style4

namespace loom::test::mermaid_flow::style4 {
inline Screen hand_paper(const Expected& expected, const loom::ui::design::tokens::Palette& palette,
                         Work& work, std::string_view footer) {
    const bool small = expected.selected == 2u;
    constexpr int width = 240, height = 40;
    work.product(width * height, 4u * sizeof(std::string) + 4u * sizeof(Attributes) + 64u);
    work.spend(32768u);
    Screen result;
    result.glyphs = {width, height, std::vector<std::string>(width * height, " ")};
    result.attributes.resize(width * height);
    const auto write = [&](int x, int y, std::string_view value) {
        require(value.size() <= 1024u, "FlowStyle4 paper text bound");
        work.spend(value.size() * (4u * sizeof(std::string) + 32u) + 256u);
        for (const auto& cell : ftxui::Utf8ToGlyphs(std::string(value))) {
            work.spend(cell.size() * 4u + 16u);
            result.glyphs.put({x++, y}, cell);
        }
    };
    std::array<Rect, 3> boxes{};
    for (std::size_t n = 0; n < 3u; ++n) {
        const int x = small ? 2 : 2 + static_cast<int>(n) * 14,
                  y = small ? 2 + static_cast<int>(n) * 6 : 2;
        const int right = x + (small ? 10 : 8), bottom = y + 2;
        boxes[n] = {x, y, right, bottom};
        write(x, y, small ? "┌─────────┐" : "┌───────┐");
        write(x, bottom, small ? "└─────────┘" : "└───────┘");
        if (small)
            write(x, y + 1, n == 0u ? "│  Start  │" : n == 1u ? "│ Process │" : "│   End   │");
        else
            write(x, y + 1, n == 0u ? "│ Lorem │" : n == 1u ? "│ Ipsum │" : "│ Dolor │");
        const auto style = expected.styles[n];
        if (style.fill != Fill::None)
            for (int row = y; row <= bottom; ++row)
                for (int col = x; col <= right; ++col) {
                    work.spend(32u);
                    const bool border = col == x || col == right || row == y || row == bottom;
                    result.attributes[static_cast<std::size_t>(result.glyphs.index({col, row}))] = {
                        palette.text,
                        border ? ftxui::Color::Default : fill_color(style.fill, palette),
                        border && style.bold ? 1u : 0u, 0u};
                }
    }
    for (std::size_t n = 0; n < 2u; ++n) {
        if (small) {
            write(7, boxes[n].bottom, "┬");
            const int start = boxes[n].bottom + 1;
            write(7, start, "│");
            write(7, start + 1, "│");
            write(7, start + 2, "▼");
        } else {
            write(boxes[n].right, 3, "├");
            const int x = boxes[n].right + 1;
            write(x, 3, "────▶");
        }
    }
    const int last = small ? 16 : 4;
    const auto rows = legend(expected, work);
    for (std::size_t n = 0; n < rows.size(); ++n)
        write(2, last + 1 + static_cast<int>(n), rows[n]);
    write(0, last + static_cast<int>(rows.size()) + 4, footer);
    return result;
}

inline Screen crop_width(const Screen& source, int width, Work& work) {
    require(width > 0 && width <= source.glyphs.width, "FlowStyle4 crop domain");
    const auto count = static_cast<std::size_t>(width * source.glyphs.height);
    work.product(count, 4u * sizeof(std::string) + 4u * sizeof(Attributes) + 32u);
    work.spend(8192u);
    Screen result;
    result.glyphs = {width, source.glyphs.height, {}};
    result.glyphs.cells.reserve(count);
    result.attributes.reserve(count);
    for (int y = 0; y < source.glyphs.height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto at = static_cast<std::size_t>(source.glyphs.index({x, y}));
            require(source.glyphs.cells[at].size() <= 64u, "FlowStyle4 crop cell byte bound");
            work.spend(source.glyphs.cells[at].size() * 4u + 32u);
            result.glyphs.cells.push_back(source.glyphs.cells[at]);
            result.attributes.push_back(source.attributes[at]);
        }
    return result;
}
}  // namespace loom::test::mermaid_flow::style4
