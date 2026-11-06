// RFC 0031 FS-5b: the divided-rectangle process shape
// (@{ shape: div-rect | div-proc | divided-rectangle | divided-process }) is
// a rectangle with a title-band divider; the single label is centred in the
// body band below the divider.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS5B_SENTINEL");
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

TEST(MermaidFs5bDividedRectangle, AliasTokensDrawWithDividerAndBodyLabel) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: div-rect, label: \"div-rect\" }\n"
        "n1@{ shape: div-proc, label: \"div-proc\" }\n"
        "n2@{ shape: divided-rectangle, label: \"divided-rectangle\" }\n"
        "n3@{ shape: divided-process, label: \"divided-process\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"div-rect", "div-proc", "divided-rectangle",
                              "divided-process"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    // The shape-data syntax is consumed and the divider junction is painted.
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    EXPECT_NE(art.find("├"), std::string::npos);
    EXPECT_NE(art.find("┤"), std::string::npos);
}

TEST(MermaidFs5bDividedRectangle, UnknownProcessAliasKeepsWholeSource) {
    // An exotic process alias that has no terminal contour still forces
    // ordinary source fallback (the diagram must not silently degrade).
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: bolt, label: \"x\" }\n"));
}
