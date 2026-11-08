// RFC 0031 FS-sq: single-quoted labels in @{ ... } shape data.
// Mermaid delimits shape-data label values with either a single or double
// quote; the parser previously accepted only double quotes, so a large
// family of neo shape-theme fixtures fell back. Both quotes now decode
// through the same strict validator, while malformed quoting fails closed.
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

std::string surface_text(const std::string& body) {
    auto element = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    ftxui::Render(screen, element);
    std::string out;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 120; ++x) out += screen.PixelAt(x, y).character;
    return out;
}
}  // namespace

TEST(MermaidSingleQuoteLabel, BothQuoteStylesDraw) {
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ shape: rect, label: \"Box\" }\n"));
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ shape: rect, label: 'Box' }\n"));
    // No whitespace before the value.
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ shape: triangle, label:'Tri' }\n"));
    // Label-only metadata with a single quote.
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ label: 'Plain' }\n"));
    // Single-quoted label on an AWS icon node still projects the marker.
    const auto icon = surface_text(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\", label: 'Cloud' }\n");
    EXPECT_NE(icon.find("\xE2\x96\xA0 Cloud"), std::string::npos);
}

TEST(MermaidSingleQuoteLabel, LabelTextIsRetained) {
    auto grid = surface_text("flowchart TB\n N@{ shape: rect, label: 'My Node' }\n");
    EXPECT_NE(grid.find("My Node"), std::string::npos);
    // The raw shape-data syntax never reaches the canvas.
    EXPECT_EQ(grid.find("shape:"), std::string::npos);
    EXPECT_EQ(grid.find("'My Node'"), std::string::npos);
}

TEST(MermaidSingleQuoteLabel, MalformedQuotingFailsClosed) {
    // Unterminated single quote.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: 'Box }\n"));
    // Mismatched quote styles.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: 'Box\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: \"Box' }\n"));
    // Backslash escapes and embedded double quotes are not synthesised.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: 'a\\\\b' }\n"));
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: 'a\"b' }\n"));
    // Double-quoted forms keep their original strictness unchanged.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ shape: rect, label: \"Box }\n"));
}

TEST(MermaidSingleQuoteLabel, ExistingShapeDataFormsUnaffected) {
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ shape: cylinder }\n"));
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ label: \"Plain\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ animation: \"pulse\" }\n"));
}
