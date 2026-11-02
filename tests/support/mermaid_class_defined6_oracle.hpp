// Restricted test-private screen oracle for the source-reviewed ClassDefined6 cohort.
// Expected graph/style comes from immutable source authority, never captured art.
#pragma once
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

#include "mermaid_class_defined6_authority.hpp"
import loom.ui.foundation.design_tokens;

namespace loom::test::mermaid_class_defined6 {
struct Attributes {
    ftxui::Color foreground = ftxui::Color::Default, background = ftxui::Color::Default;
    unsigned flags = 0u, hyperlink = 0u;
    bool operator==(const Attributes&) const = default;
};

inline Attributes attributes(const ftxui::Pixel& pixel) {
    return {pixel.foreground_color, pixel.background_color,
            static_cast<unsigned>(pixel.bold) | (static_cast<unsigned>(pixel.dim) << 1u) |
                (static_cast<unsigned>(pixel.underlined) << 2u) |
                (static_cast<unsigned>(pixel.underlined_double) << 3u) |
                (static_cast<unsigned>(pixel.blink) << 4u) |
                (static_cast<unsigned>(pixel.inverted) << 5u) |
                (static_cast<unsigned>(pixel.strikethrough) << 6u) |
                (static_cast<unsigned>(pixel.automerge) << 7u),
            pixel.hyperlink};
}

struct Screen {
    relations::Grid glyphs;
    std::vector<Attributes> attributes;
};
enum class Outcome { Match, Mismatch, Inconclusive };
enum class Reason { Complete, Domain, Geometry, Legend, Attributes, Footer, Resource, Unexpected };

struct Result {
    Outcome outcome = Outcome::Inconclusive;
    Reason reason = Reason::Unexpected;
    bool resource = false;
    std::optional<relations::Rect> box;
};

inline std::string escaped_declaration(std::string_view raw, Work& work) {
    require(raw.size() <= 1024u, "ClassDefined6 raw declaration resource bound");
    work.spend(raw.size() * 8u + 256u);
    std::string result;
    result.reserve(raw.size() * 2u);
    for (const auto c : raw) {
        if (c == '\t')
            result += "\\t";
        else
            result += c;
    }
    return result;
}

inline std::array<std::string, 5> legend(const Expected& expected, Work& work) {
    std::array<std::string, 5> result;
    for (std::size_t n = 0; n < 4u; ++n)
        result[n] = escaped_declaration(expected.raw_records[n], work);
    work.spend(16384u);
    result[0] += " -> class:pink fill:Purple(surface tint)";
    result[1] += " -> class:bold stroke:Neutral width:bold fg:Neutral(not projected on fill)";
    result[2] += " -> definition:pink (applied)";
    result[3] += " -> definition:bold (applied)";
    result[4] = "key: hue roles use palette; width>=4=bold border; color on fill not projected; "
                "monochrome/ANSI16 may share shades; source TAB=\\t";
    return result;
}

inline bool resource_message(std::string_view message) {
    return message.find("work exhausted") != std::string_view::npos ||
           message.find("bounds exceeded") != std::string_view::npos ||
           message.find("outline limit exceeded") != std::string_view::npos ||
           message.find("matching exhausted") != std::string_view::npos;
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
            return {Outcome::Inconclusive, Reason::Domain, true, {}};
        if (expected.index >= kPins.size() || expected.graph.family != "class" ||
            expected.graph.entities.size() != 1u || !expected.graph.relations.empty() ||
            !expected.graph.groups.empty() || !expected.graph.notes.empty())
            return {Outcome::Inconclusive, Reason::Domain, false, {}};
        const auto& entity = expected.graph.entities.front();
        if (expected.graph.direction != "TB" || entity.id != "Class10" ||
            entity.label != "Class10" || entity.shape != "box" || !entity.owner.empty() ||
            !entity.rows.empty() || entity.typed_er || entity.headers.size() != 1u ||
            entity.headers.front().size() != 1u || entity.headers.front().front() != "Class10")
            return {Outcome::Inconclusive, Reason::Domain, false, {}};
        constexpr std::array<std::string_view, 4> logical{
            {"class Class10:::pink", "cssClass \"Class10\" bold", "classDef pink fill:#f9f",
             "classDef bold stroke:#333,stroke-width:6px,color:#fff"}};
        for (std::size_t n = 0; n < logical.size(); ++n) {
            if (expected.raw_records[n].size() > 1024u)
                return {Outcome::Inconclusive, Reason::Domain, true, {}};
            work.spend(expected.raw_records[n].size() * 4u + 1024u);
            if (trim(expected.raw_records[n]) != logical[n])
                return {Outcome::Inconclusive, Reason::Domain, false, {}};
        }
        if (footer.empty() || footer.size() > 240u)
            return {Outcome::Inconclusive, Reason::Domain, false, {}};
        for (const auto c : footer)
            if (static_cast<unsigned char>(c) < 0x20u || static_cast<unsigned char>(c) > 0x7eu)
                return {Outcome::Inconclusive, Reason::Domain, false, {}};
        // Bound and fund bytes before outline discovery, concatenated footer
        // rows, substring scans or grid copies can touch adversarial strings.
        work.spend(grid.cells.size() * 8u + 256u);
        for (std::size_t n = 0; n < grid.cells.size(); ++n) {
            const auto& text = grid.cells[n];
            if (text.size() > 64u) return {Outcome::Inconclusive, Reason::Domain, true, {}};
            work.spend(text.size() * 16u + 8u);
        }

        const auto frames = relations::outlines(grid, work);
        if (frames.size() != 1u || frames.front().shape != "box")
            return {Outcome::Mismatch, Reason::Geometry, false, {}};
        const auto box = frames.front();
        for (int y = box.t; y <= box.b; ++y)
            for (int x = box.l; x <= box.r; ++x) {
                work.spend(8u);
                if (x != box.l && x != box.r && y != box.t && y != box.b) continue;
                const auto wanted = y == box.t   ? (x == box.l   ? "┌"
                                                    : x == box.r ? "┐"
                                                                 : "─")
                                    : y == box.b ? (x == box.l   ? "└"
                                                    : x == box.r ? "┘"
                                                                 : "─")
                                                 : "│";
                if (grid.at({x, y}) != wanted)
                    return {Outcome::Mismatch, Reason::Geometry, false, box};
            }
        const auto expected_legend = legend(expected, work);
        if (box.b + 5 >= grid.height || box.l < 0)
            return {Outcome::Inconclusive, Reason::Domain, true, {}};
        for (const auto& line : expected_legend)
            if (line.size() > 240u || line.size() > static_cast<std::size_t>(grid.width - box.l))
                return {Outcome::Inconclusive, Reason::Domain, true, box};
        const auto exact_row = [&](int y, std::string_view text) {
            if (box.l < 0 || text.size() > static_cast<std::size_t>(grid.width - box.l))
                return false;
            for (int x = 0; x < grid.width; ++x) {
                work.spend(4u);
                const auto at = x - box.l;
                const auto character = at >= 0 && static_cast<std::size_t>(at) < text.size()
                                           ? std::string_view(text.data() + at, 1u)
                                           : std::string_view(" ");
                if (grid.at({x, y}) != character) return false;
            }
            return true;
        };
        for (std::size_t n = 0; n < expected_legend.size(); ++n)
            if (!exact_row(box.b + 1 + static_cast<int>(n), expected_legend[n]))
                return {Outcome::Mismatch, Reason::Legend, false, box};
        std::optional<int> following;
        for (int y = 0; y < grid.height; ++y) {
            work.spend(static_cast<std::size_t>(grid.width) * 4u + footer.size() + 64u);
            const auto row = grid.row(y);
            if (row.find(footer) != std::string::npos) {
                if (following || trim(row) != footer)
                    return {Outcome::Mismatch, Reason::Footer, false, box};
                following = y;
            }
        }
        if (!following || *following <= box.b + 5)
            return {Outcome::Mismatch, Reason::Footer, false, box};
        for (int y = *following + 1; y < grid.height; ++y)
            for (int x = 0; x < grid.width; ++x) {
                work.spend();
                if (grid.at({x, y}) != " " && !grid.at({x, y}).empty())
                    return {Outcome::Mismatch, Reason::Footer, false, box};
            }
        work.spend(grid.cells.size() * (3u * sizeof(std::string) + 8u) + 256u);
        for (const auto& cell : grid.cells)
            work.spend(cell.size() * 4u);
        auto geometry = grid;
        for (int y = box.b + 1; y <= box.b + 5; ++y)
            for (int x = 0; x < grid.width; ++x) {
                work.spend(4u);
                geometry.put({x, y}, " ");
            }
        // No explicit hide-empty setting exists in these6 sources. Preserve
        // the already reviewed default Class terminal geometry; implicit browser
        // compartment defaults remain foreign debt, not a new style projection.
        const auto errors = relations::validate_impl(geometry, expected.graph, work, false, nullptr,
                                                     false, nullptr, footer);
        if (!errors.empty()) {
            const bool resource = std::ranges::any_of(
                errors, [](const auto& error) { return resource_message(error); });
            return {resource ? Outcome::Inconclusive : Outcome::Mismatch,
                    resource ? Reason::Resource : Reason::Geometry, resource, box};
        }
        for (int y = 0; y < grid.height; ++y)
            for (int x = 0; x < grid.width; ++x) {
                work.spend(32u);
                Attributes wanted;
                if (x >= box.l && x <= box.r && y >= box.t && y <= box.b) {
                    const bool border = x == box.l || x == box.r || y == box.t || y == box.b;
                    wanted.foreground = palette.text;
                    wanted.background = border ? ftxui::Color::Default : palette.surface_tint_4;
                    wanted.flags = border ? 1u : 0u;
                }
                if (screen.attributes[static_cast<std::size_t>(grid.index({x, y}))] != wanted)
                    return {Outcome::Mismatch, Reason::Attributes, false, box};
            }
        return {Outcome::Match, Reason::Complete, false, box};
    } catch (const std::bad_alloc&) {
        return {Outcome::Inconclusive, Reason::Resource, true, {}};
    } catch (const std::exception& error) {
        const bool resource = work.left == 0u || resource_message(error.what());
        return {
            Outcome::Inconclusive, resource ? Reason::Resource : Reason::Unexpected, resource, {}};
    }
}

inline Screen capture(ftxui::Element element, int width, int height, Work& work) {
    require(width > 0 && width <= 240 && height > 0 && height <= 1040,
            "ClassDefined6 capture bounds");
    const auto cells = static_cast<std::size_t>(width * height);
    work.spend(cells * (4u * sizeof(ftxui::Pixel) + 4u * sizeof(Attributes) +
                        4u * sizeof(std::string) + 64u) +
               8192u);
    auto actual =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(actual, std::move(element));
    Screen result;
    result.glyphs.width = width;
    result.glyphs.height = height;
    result.glyphs.cells.reserve(cells);
    result.attributes.reserve(cells);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& pixel = actual.PixelAt(x, y);
            work.spend(pixel.character.size() * 4u + 32u);
            result.glyphs.cells.push_back(pixel.character);
            result.attributes.push_back(attributes(pixel));
        }
    return result;
}
}  // namespace loom::test::mermaid_class_defined6

namespace loom::test::mermaid_class_defined6 {
inline Screen copy_screen(const Screen& screen, Work& work) {
    work.spend(screen.glyphs.cells.size() *
                   (3u * sizeof(Attributes) + 3u * sizeof(std::string) + 16u) +
               1024u);
    for (const auto& text : screen.glyphs.cells)
        work.spend(text.size() * 4u);
    return screen;
}

inline Screen hand_paper(const Expected& expected, const loom::ui::design::tokens::Palette& palette,
                         Work& work, std::string_view footer) {
    constexpr int width = 240, height = 24;
    work.spend(static_cast<std::size_t>(width * height) *
                   (4u * sizeof(Attributes) + 4u * sizeof(std::string) + 32u) +
               8192u);
    Screen result;
    result.glyphs = {width, height, std::vector<std::string>(width * height, " ")};
    result.attributes.resize(width * height);
    const auto write = [&](int x, int y, std::string_view text) {
        require(text.size() <= 1024u, "ClassDefined6 paper text bound");
        work.spend(text.size() * (4u * sizeof(std::string) + 32u) + 256u);
        for (const auto& value : ftxui::Utf8ToGlyphs(std::string(text))) {
            work.spend(value.size() * 4u + 16u);
            result.glyphs.put({x++, y}, value);
        }
    };
    // Independent one-Class10 paper: three fixed rows, two blank interior
    // margins and no producer canvas, graph parser, model or layout calls.
    write(2, 2, "┌─────────┐");
    write(2, 3, "│ Class10 │");
    write(2, 4, "└─────────┘");
    for (int y = 2; y <= 4; ++y)
        for (int x = 2; x <= 12; ++x) {
            work.spend(32u);
            const bool border = y == 2 || y == 4 || x == 2 || x == 12;
            result.attributes[static_cast<std::size_t>(result.glyphs.index({x, y}))] = {
                palette.text, border ? ftxui::Color::Default : palette.surface_tint_4,
                border ? 1u : 0u, 0u};
        }
    const auto lines = legend(expected, work);
    for (std::size_t n = 0; n < lines.size(); ++n)
        write(2, 5 + static_cast<int>(n), lines[n]);
    write(0, 12, footer);
    return result;
}

inline constexpr std::size_t kPhysicalNegatives = 8u;

inline void mutate(Screen& screen, const relations::Rect& box, std::size_t kind, Work& work) {
    work.spend(8192u);
    const auto at = [&](int x, int y) -> Attributes& {
        return screen.attributes.at(static_cast<std::size_t>(screen.glyphs.index({x, y})));
    };
    switch (kind) {
    case 0u: {
        bool changed = false;
        for (int y = box.t + 1; y < box.b && !changed; ++y)
            for (int x = box.l + 1; x < box.r; ++x) {
                work.spend();
                if (screen.glyphs.at({x, y}) == "C") {
                    screen.glyphs.put({x, y}, "X");
                    changed = true;
                    break;
                }
            }
        require(changed, "ClassDefined6 physical label target");
        break;
    }
    case 1u:
        at(box.l + 1, box.t + 1).background = ftxui::Color::Default;
        break;
    case 2u:
        at(box.l, box.t).flags ^= 1u;
        break;
    case 3u:
        at(box.l, box.t).foreground = ftxui::Color::Default;
        break;
    case 4u:
        screen.glyphs.put({box.l, box.b + 1}, "!");
        break;
    case 5u:
        at(screen.glyphs.width - 1, box.b + 2).flags = 1u;
        break;
    case 6u:
        screen.glyphs.put({screen.glyphs.width - 1, screen.glyphs.height - 1}, "!");
        break;
    case 7u:
        screen.glyphs.put({box.l + 1, box.t}, " ");
        break;
    default:
        throw std::runtime_error("ClassDefined6 private mutation index");
    }
}
}  // namespace loom::test::mermaid_class_defined6

namespace loom::test::mermaid_class_defined6 {
inline Screen crop_width(const Screen& input, int width, Work& work) {
    require(width > 0 && width <= input.glyphs.width, "ClassDefined6 crop control width");
    const auto count = static_cast<std::size_t>(width * input.glyphs.height);
    work.spend(count * (3u * sizeof(std::string) + 3u * sizeof(Attributes) + 32u) + 8192u);
    Screen result;
    result.glyphs.width = width;
    result.glyphs.height = input.glyphs.height;
    result.glyphs.cells.reserve(count);
    result.attributes.reserve(count);
    for (int y = 0; y < input.glyphs.height; ++y)
        for (int x = 0; x < width; ++x) {
            const auto index = static_cast<std::size_t>(input.glyphs.index({x, y}));
            work.spend(input.glyphs.cells[index].size() * 4u + 32u);
            result.glyphs.cells.push_back(input.glyphs.cells[index]);
            result.attributes.push_back(input.attributes[index]);
        }
    return result;
}
}  // namespace loom::test::mermaid_class_defined6
