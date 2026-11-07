// RFC 0031 FS-5 G3: the polygon-family flowchart shapes:
//  - notched pentagon (notch-pent/loop-limit/notched-pentagon)
//  - hourglass (collate/hourglass) and bow tie (bow-rect/stored-data/
//    bow-tie-rectangle): centre-waisted contours, top/bottom-only ports
//  - lightning bolt (bolt/com-link/lightning-bolt)
//  - comment curly brace (comment/brace-l)
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG3_SENTINEL");
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

TEST(MermaidFs5g3, NotchedPentagonAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: notch-pent, label: \"notch-pent\" }\n"
        "n1@{ shape: loop-limit, label: \"loop-limit\" }\n"
        "n2@{ shape: notched-pentagon, label: \"notched-pentagon\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("loop-limit"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    // The shoulder diagonal and inset top edge.
    EXPECT_NE(art.find("╱"), std::string::npos);
    EXPECT_NE(art.find("╲"), std::string::npos);
}

TEST(MermaidFs5g3, HourglassAliasesDrawWithWaist) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: collate, label: \"collate\" }\n"
        "n1@{ shape: hourglass, label: \"hourglass\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("hourglass"), std::string::npos);
    EXPECT_NE(art.find("╲"), std::string::npos);
    EXPECT_NE(art.find("╱"), std::string::npos);
}

TEST(MermaidFs5g3, BowTieAliasesDrawWithWaist) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: bow-rect, label: \"bow-rect\" }\n"
        "n1@{ shape: stored-data, label: \"stored-data\" }\n"
        "n2@{ shape: bow-tie-rectangle, label: \"bow-tie-rectangle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("stored-data"), std::string::npos);
    EXPECT_NE(art.find("╲"), std::string::npos);
    EXPECT_NE(art.find("╱"), std::string::npos);
}

TEST(MermaidFs5g3, BoltAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: bolt, label: \"bolt\" }\n"
        "n1@{ shape: com-link, label: \"com-link\" }\n"
        "n2@{ shape: lightning-bolt, label: \"lightning-bolt\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("bolt"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g3, CommentBraceAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: comment, label: \"comment\" }\n"
        "n1@{ shape: brace-l, label: \"brace-l\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("comment"), std::string::npos);
    // The brace tip opens right and the interior rule meets the right wall
    // at a T junction (the wall loop must not overwrite ┤ with │).
    EXPECT_NE(art.find("◁"), std::string::npos);
    EXPECT_NE(art.find("┤"), std::string::npos);
}

TEST(MermaidFs5g3, BoltJogEdgeIsFullWidth) {
    // The waist jog is a near-full-width edge even when the label is short,
    // so it meets the left └ without an outline gap (regression guard).
    const std::string body = "flowchart \nA@{ shape: bolt, label: \"b\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("└"), std::string::npos);
    EXPECT_NE(art.find("┐"), std::string::npos);
    // The jog row carries the left corner, a rule cell next to it, and the
    // right corner on the same render.
    std::size_t pos = 0u;
    bool found_jog = false;
    while (pos < art.size()) {
        const auto end = art.find('\n', pos);
        const auto line =
            art.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (line.find("└") != std::string::npos && line.find("─") != std::string::npos &&
            line.find("┐") != std::string::npos)
            found_jog = true;
        if (end == std::string::npos) break;
        pos = end + 1u;
    }
    EXPECT_TRUE(found_jog);
}

// Edge attachment for every new contour (G1 lesson): straight-sided shapes
// accept LR edges; the centre-waisted hourglass/bow-tie accept a TB edge,
// and a sideways edge detours onto a top/bottom edge without whole-source
// fallback.
TEST(MermaidFs5g3, ShapesAcceptEdges) {
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: notched-pentagon, label: \"p\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: bolt, label: \"b\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: comment, label: \"c\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\nA[a] --> B@{ shape: hourglass, label: \"h\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\nA[a] --> B@{ shape: bow-tie-rectangle, label: \"b\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: hourglass, label: \"h\" }\n"));
}

TEST(MermaidFs5g3, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: papers, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: hourglass-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: bolts, label: \"x\" }\n"));
}
