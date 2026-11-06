// RFC 0031 FS-5c: the flipped-triangle manual-file shape
// (@{ shape: flip-tri | manual-file | flipped-triangle }) is the vertical
// mirror of the extract triangle: a flat edge on top and the apex at the
// bottom centre (upstream shapes/flippedTriangle.ts polygon).
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS5C_SENTINEL");
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

TEST(MermaidFs5cFlippedTriangle, AliasTokensDrawWithDownApexAndLabels) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: flip-tri, label: \"flip-tri\" }\n"
        "n1@{ shape: manual-file, label: \"manual-file\" }\n"
        "n2@{ shape: flipped-triangle, label: \"flipped-triangle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"flip-tri", "manual-file", "flipped-triangle"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    // The shape-data syntax is consumed; the apex points down.
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    EXPECT_NE(art.find("▽"), std::string::npos);
    EXPECT_EQ(art.find("△"), std::string::npos);
}

TEST(MermaidFs5cFlippedTriangle, UpwardTriangleTokenKeepsUpApex) {
    // The extract/triangle alias must keep its apex on top: the two triangle
    // contours are distinct, not aliased onto each other.
    const std::string body =
        "flowchart \n"
        "n0@{ shape: tri, label: \"tri\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("△"), std::string::npos);
    EXPECT_EQ(art.find("▽"), std::string::npos);
}

TEST(MermaidFs5cFlippedTriangle, UnknownShapeAliasKeepsWholeSource) {
    // An exotic alias that has no terminal contour still forces ordinary
    // source fallback (the diagram must not silently degrade).
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: doc, label: \"x\" }\n"));
}
