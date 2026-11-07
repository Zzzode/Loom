// RFC 0031 FS-5d: the sloped-rectangle manual-input shape
// (@{ shape: sl-rect | manual-input | sloped-rectangle }) is a rectangle
// whose top edge slopes up to the right: the right wall extends two rows
// above the left wall (upstream shapes/slopedRect.ts polygon).
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>
#include <string_view>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS5D_SENTINEL");
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
// First rendered row (0-based) that contains glyph, within the diagram's
// non-blank vertical band. Single-node diagrams paint exactly one node.
int first_row_with(const std::string& art, std::string_view glyph) {
    std::size_t pos = 0u;
    int row = 0;
    while (pos < art.size()) {
        const auto end = art.find('\n', pos);
        const auto line = art.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (line.find_first_not_of(' ') != std::string::npos && line.find(glyph) != std::string::npos)
            return row;
        if (end == std::string::npos) break;
        pos = end + 1u;
        ++row;
    }
    return -1;
}
}  // namespace

TEST(MermaidFs5dSlopedRectangle, AliasTokensDrawWithRaisedRightWallAndLabels) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: sl-rect, label: \"sl-rect\" }\n"
        "n1@{ shape: manual-input, label: \"manual-input\" }\n"
        "n2@{ shape: sloped-rectangle, label: \"sloped-rectangle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"sl-rect", "manual-input", "sloped-rectangle"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    // The shape-data syntax is consumed and the sloping top is painted.
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    EXPECT_NE(art.find("╱"), std::string::npos);
    EXPECT_NE(art.find("┐"), std::string::npos);
}

TEST(MermaidFs5dSlopedRectangle, RaisedRightWallSignatureDistinguishesManualToken) {
    // Identity guard against the string-coupled wrong-answer hazard:
    // `manual` is the already-admitted TrapezoidAlt (top corners ┌ and ┐ on
    // the SAME row), while `manual-input` is the sloped rectangle (the raised
    // ┐ sits two rows above the body's ┌). A scanner/parser regression that
    // collapses manual-input onto manual would still draw but move both
    // corners onto one row, which this test catches.
    const std::string trapezoid =
        "flowchart \nA@{ shape: manual, label: \"manual\" }\n";
    ASSERT_TRUE(drawn_native(trapezoid));
    const auto trapezoid_art = capture(trapezoid, "mermaid");
    const int trap_top = first_row_with(trapezoid_art, "┐");
    EXPECT_GE(trap_top, 0);
    EXPECT_EQ(first_row_with(trapezoid_art, "┌"), trap_top);

    const std::string sloped =
        "flowchart \nA@{ shape: manual-input, label: \"manual-input\" }\n";
    ASSERT_TRUE(drawn_native(sloped));
    const auto sloped_art = capture(sloped, "mermaid");
    const int raised_top = first_row_with(sloped_art, "┐");
    EXPECT_GE(raised_top, 0);
    EXPECT_EQ(first_row_with(sloped_art, "┌"), raised_top + 2);
}

TEST(MermaidFs5dSlopedRectangle, UnknownShapeAliasKeepsWholeSource) {
    // An exotic alias without a terminal contour, and a near-miss of the
    // mapped token, still force ordinary source fallback. The negative
    // example must stay an alias no batch admits yet; win-pane graduated in
    // FS-5e, so use hourglass (exotic contour still unsupported) here.
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: paper-tape, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: sl-rectangle, label: \"x\" }\n"));
}
