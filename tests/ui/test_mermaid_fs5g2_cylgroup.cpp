// RFC 0031 FS-5 G2: the cylinder-family storage shapes.
//  - horizontal cylinder (h-cyl/das/horizontal-cylinder): a cylinder on its
//    side (straight top/bottom, shallow middle-row end caps, centre line)
//  - lined cylinder (lin-cyl/disk/lined-cylinder): database cylinder with a
//    stacked-disk line on the first body row
//  - datastore (datastore/data-store): an open rectangle of just two rules
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG2_SENTINEL");
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

TEST(MermaidFs5g2, HorizontalCylinderAliasesDrawSideways) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: h-cyl, label: \"h-cyl\" }\n"
        "n1@{ shape: das, label: \"das\" }\n"
        "n2@{ shape: horizontal-cylinder, label: \"horizontal-cylinder\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"h-cyl", "das", "horizontal-cylinder"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    // Sideways caps and the centre far-rim junction.
    EXPECT_NE(art.find("("), std::string::npos);
    EXPECT_NE(art.find(")"), std::string::npos);
    EXPECT_NE(art.find("┬"), std::string::npos);
}

TEST(MermaidFs5g2, LinedCylinderAliasesReuseCylinderWithDiskLine) {
    const std::string good =
        "flowchart \n"
        "n0@{ shape: lin-cyl, label: \"lin-cyl\" }\n"
        "n1@{ shape: disk, label: \"disk\" }\n"
        "n2@{ shape: lined-cylinder, label: \"lined-cylinder\" }\n";
    ASSERT_TRUE(drawn_native(good)) << good;
    const auto art = capture(good, "mermaid");
    // The stacked-disk line meets the cylinder walls at T junctions.
    EXPECT_NE(art.find("├"), std::string::npos);
    EXPECT_NE(art.find("┤"), std::string::npos);
    EXPECT_NE(art.find("disk"), std::string::npos);
}

TEST(MermaidFs5g2, DatastoreAliasesDrawTwoRules) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: datastore, label: \"datastore\" }\n"
        "n1@{ shape: data-store, label: \"data-store\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("datastore"), std::string::npos);
    EXPECT_NE(art.find("data-store"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    // Two horizontal rules and deliberately no side walls/corners.
    EXPECT_NE(art.find("─"), std::string::npos);
    EXPECT_EQ(art.find("┌"), std::string::npos);
    EXPECT_EQ(art.find("└"), std::string::npos);
}

// Edge attachment (the edgeless smoke fixtures cannot catch dead ports):
// the horizontal/lined cylinders accept sideways edges, the datastore
// accepts a vertical edge onto its top rule.
TEST(MermaidFs5g2, ShapesAcceptEdges) {
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: h-cyl, label: \"hcyl\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: lin-cyl, label: \"disk\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\nA[a] --> B@{ shape: datastore, label: \"store\" }\n"));
    // The datastore exposes only top/bottom ports (no side walls); a sideways
    // edge detours onto the top/bottom rule and must still render native.
    EXPECT_TRUE(drawn_native(
        "flowchart LR\nA[a] --> B@{ shape: datastore, label: \"store\" }\n"));
}

TEST(MermaidFs5g2, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: paper-tape, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: h-cyl-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: datastores, label: \"x\" }\n"));
}
