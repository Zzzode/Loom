// RFC 0031 FS-5e: the window-pane internal-storage shape
// (@{ shape: win-pane | internal-storage | window-pane }) is a rectangle
// split into four panes by a horizontal and a vertical centre divider
// (upstream shapes/windowPane.ts path).
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS5E_SENTINEL");
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

TEST(MermaidFs5eWindowPane, AliasTokensDrawWithCrossDividersAndLabels) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: win-pane, label: \"win-pane\" }\n"
        "n1@{ shape: internal-storage, label: \"internal-storage\" }\n"
        "n2@{ shape: window-pane, label: \"window-pane\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"win-pane", "internal-storage", "window-pane"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    // The shape-data syntax is consumed and the pane cross is painted. The
    // four wall junctions are the signature; the centre cell is overwritten
    // by the label (as the upstream label floats over the cross).
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    for (const char* glyph : {"┬", "┴", "├", "┤"})
        EXPECT_NE(art.find(glyph), std::string::npos) << glyph;
}

TEST(MermaidFs5eWindowPane, UnknownShapeAliasKeepsWholeSource) {
    // Exotic aliases without a terminal contour, and a near-miss of the
    // mapped token, still force ordinary source fallback.
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: bolt, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: win-pan, label: \"x\" }\n"));
}
