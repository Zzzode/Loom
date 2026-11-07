// RFC 0031 FS-5 G4: the stacked rectangle (multi-process) and tagged
// rectangle (tagged process) shapes - offset cards that extend one column
// beyond their front rectangle.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG4_SENTINEL");
    if (!element) return {};
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(80));
    ftxui::Render(screen, element);
    return screen.ToString();
}
bool drawn_native(const std::string& body) {
    const auto native = capture(body, "mermaid");
    const auto plain = capture(body, "text");
    return !native.empty() && native != plain;
}
}  // namespace

TEST(MermaidFs5g4, StackedRectangleAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: procs, label: \"procs\" }\n"
        "n1@{ shape: processes, label: \"processes\" }\n"
        "n2@{ shape: st-rect, label: \"st-rect\" }\n"
        "n3@{ shape: stacked-rectangle, label: \"stacked-rectangle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("st-rect"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g4, TaggedRectangleAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: tag-rect, label: \"tag-rect\" }\n"
        "n1@{ shape: tag-proc, label: \"tag-proc\" }\n"
        "n2@{ shape: tagged-rectangle, label: \"tagged-rectangle\" }\n"
        "n3@{ shape: tagged-process, label: \"tagged-process\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("tag-proc"), std::string::npos);
    // The folded corner projects as a diagonal at the bottom-right.
    EXPECT_NE(art.find("╱"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

// Edge attachment on both orientations (G1 lesson); both shapes extend the
// box by a column, so this pins the custom port/corridor handling.
TEST(MermaidFs5g4, ShapesAcceptEdges) {
    EXPECT_TRUE(drawn_native(
        "flowchart TB\nA[a] --> B@{ shape: st-rect, label: \"st\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: st-rect, label: \"st\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\nA[a] --> B@{ shape: tag-rect, label: \"t\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: tag-rect, label: \"t\" }\n"));
    // Right-side ports/corridors: the edge leaves to the right (LR) or enters
    // from the right (RL places the source to the right of the target). The
    // tagged right port sits on the front wall while its outside peek cell
    // lies inside the bbox and must be unblocked by classic_port_corridors.
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA@{ shape: st-rect, label: \"st\" } --> B[b]\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart RL\nA[a] --> B@{ shape: st-rect, label: \"st\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA@{ shape: tag-rect, label: \"t\" } --> B[b]\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart RL\nA[a] --> B@{ shape: tag-rect, label: \"t\" }\n"));
}

TEST(MermaidFs5g4, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: paper-tape, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: st-rect-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: tagged-rects, label: \"x\" }\n"));
}
