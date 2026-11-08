// RFC 0031 FS-7a subgraph grammar: direction-less headers and forward
// group references. A `flowchart`/`graph` header without a direction
// defaults to TB and is byte-identical to an explicit TD header; a group
// id may appear on an edge before its `subgraph` declaration because the
// parser pre-registers every fixed-id subgraph as a deferred shell.
// Closed group-endpoint grammar (scalar, solid, one-way, unlabelled) and
// every other fail-closed guard are unchanged.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
bool drawn_native(const std::string& body) {
    auto n = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    auto t = loom::ui::render_markdown("```text\n" + body + "\n```\n");
    if (!n || !t) return false;
    auto a = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    auto b = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    ftxui::Render(a, n);
    ftxui::Render(b, t);
    return a.ToString() != b.ToString();
}

struct Grid {
    int width = 0, height = 0;
    std::vector<std::string> cells;
    std::string& at(int x, int y) { return cells[static_cast<std::size_t>(y) * width + x]; }
    const std::string& at(int x, int y) const {
        return cells[static_cast<std::size_t>(y) * width + x];
    }
};
Grid render_grid(const std::string& body, int width = 120, int height = 80) {
    auto element = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    Grid grid;
    grid.width = width;
    grid.height = height;
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    grid.cells.reserve(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) grid.cells.push_back(screen.PixelAt(x, y).character);
    return grid;
}
}  // namespace

TEST(MermaidFs7Subgraph, BareHeaderAdmitsSubgraphs) {
    EXPECT_TRUE(drawn_native("flowchart\n subgraph G\n a --> b\n end\n"));
    EXPECT_TRUE(drawn_native("graph\n subgraph G\n a\n end\n"));
    EXPECT_TRUE(drawn_native("flowchart\n\n subgraph G\n a\n end\n"));
}

TEST(MermaidFs7Subgraph, BareHeaderIsByteIdenticalToExplicitTD) {
    const std::string body =
        "subgraph Main\n subgraph Child1\n N1\n end\n subgraph Child2\n N2\n end\n end\n"
        " Main --> Out\n";
    const auto bare = render_grid("flowchart\n" + body);
    EXPECT_EQ(bare.cells, render_grid("flowchart TD\n" + body).cells);
    EXPECT_EQ(bare.cells, render_grid("graph TB\n" + body).cells);
}

TEST(MermaidFs7Subgraph, ForwardGroupReferencesDraw) {
    EXPECT_TRUE(drawn_native("flowchart TD\n P --> G\n subgraph G\n a\n end\n"));
    EXPECT_TRUE(drawn_native("flowchart TD\n G --> Q\n subgraph G\n a\n end\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n G1 --> G2\n subgraph G1\n a\n end\n subgraph G2\n b\n end\n"));
    // Dotted id and quoted-title forms from the pinned corpus.
    EXPECT_TRUE(drawn_native("flowchart TD\n P --> P1.5\n subgraph P1.5\n a\n end\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n start --> research\n subgraph research[\"Research\"]\n brief --> web\n"
        " end\n"));
    // Forward reference nested inside another group's body.
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n subgraph Outer\n x --> Inner\n subgraph Inner\n a\n end\n end\n"));
}

TEST(MermaidFs7Subgraph, ForwardEdgeArrowheadMeetsContainerBorder) {
    const auto grid =
        render_grid("flowchart TD\n P --> G\n subgraph G\n a\n end\n G --> Q\n");
    int head_x = -1, head_y = -1;
    for (int y = 0; y < grid.height && head_y < 0; ++y)
        for (int x = 0; x < grid.width; ++x)
            if (grid.at(x, y) == "▼") {
                head_x = x;
                head_y = y;
                break;
            }
    ASSERT_GE(head_x, 0);
    // The first inked cell below the arrowhead, in its exact column, must be
    // the top edge of the G container (a corner or horizontal rule) — the
    // forward-reference shell routes the edge to the group boundary, not to
    // an interior vertex or a blocked blank.
    std::string border;
    for (int y = head_y + 1; y < grid.height; ++y) {
        if (grid.at(head_x, y) != " ") {
            border = grid.at(head_x, y);
            break;
        }
    }
    EXPECT_TRUE(border == "┌" || border == "─" || border == "┐") << border;
    // The G title is painted inside the container.
    bool has_title = false;
    for (const auto& cell : grid.cells)
        if (cell == "G") has_title = true;
    EXPECT_TRUE(has_title);
}

TEST(MermaidFs7Subgraph, ForwardGroupEdgesKeepClosedGrammar) {
    // A vertex id that later declares a group cannot be claimed as a node.
    EXPECT_FALSE(drawn_native("flowchart TD\n G\n subgraph G\n a\n end\n"));
    // Group endpoints stay scalar: no compound fan-out.
    EXPECT_FALSE(drawn_native("flowchart TD\n G & A --> Q\n subgraph G\n a\n end\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n P --> G & A\n subgraph G\n a\n end\n"));
    // Unlabelled, solid, one-way, length-one only.
    EXPECT_FALSE(drawn_native("flowchart TD\n P -->|label| G\n subgraph G\n a\n end\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n P -.-> G\n subgraph G\n a\n end\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n P <--> G\n subgraph G\n a\n end\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n P --x G\n subgraph G\n a\n end\n"));
}

TEST(MermaidFs7Subgraph, ExistingGroupFormsAreUnaffected) {
    // Declared-first edges still draw.
    EXPECT_TRUE(drawn_native("flowchart TD\n subgraph G\n a\n end\n P --> G\n"));
    // Reopened groups and title-only (generated id) groups keep working.
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n subgraph G\n a\n end\n subgraph G\n b\n end\n"));
    EXPECT_TRUE(drawn_native("flowchart TD\n subgraph \"Just A Title\"\n a\n end\n"));
    // Self loops at depth under a direction-less header (self-loops corpus).
    EXPECT_TRUE(drawn_native(
        "flowchart\n A --> A\n subgraph B\n B1 --> B1\n end\n"));
}

TEST(MermaidFs7Subgraph, StatementSemicolonBoundariesMatchMainParser) {
    // The pre-scan must split physical lines on the same top-level ';'
    // boundary as the main parser, so trailing-semicolon group headers stay
    // admitted (regression: the whole physical line once produced a
    // mismatched deferred shell or a hard parse failure).
    EXPECT_TRUE(drawn_native("flowchart TD\n subgraph G;\n a\n end\n"));
    EXPECT_TRUE(drawn_native("flowchart TD\n subgraph G [T];\n a\n end\n"));
    EXPECT_TRUE(drawn_native("flowchart TD\n subgraph \"T\";\n a\n end\n"));
    // A forward reference resolved against a semicolon-terminated declaration.
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n P --> G\n subgraph G;\n a\n end\n"));
    // Two statements joined on one line; the group header is the first.
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n subgraph G; a --> b\n end\n"));
    // A semicolon inside a quoted title is content, not a statement boundary.
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n subgraph G[\"a;b\"]\n a\n end\n P --> G\n"));
    // A genuinely unterminated quote still fails closed as before.
    EXPECT_FALSE(drawn_native("flowchart TD\n subgraph G[\"unterminated\n a\n end\n"));
}

TEST(MermaidFs7Subgraph, ForwardReferencedGroupReopenStaysFailClosed) {
    // The forward-reference + reopen identity-merge composition has no
    // reviewed positive Model yet, so it stays on ordinary fallback. A
    // forward-referenced group with a single declaration still draws.
    EXPECT_FALSE(drawn_native(
        "flowchart TD\n G --> A\n subgraph G\n B\n end\n subgraph G\n C\n end\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n G --> A\n subgraph G\n B\n end\n"));
    // Reopen without any forward reference is unaffected (existing grammar).
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n subgraph G\n A\n end\n subgraph G\n B\n end\n"));
}
