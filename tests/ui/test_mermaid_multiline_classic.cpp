// RFC 0031 multiline classic labels: explicit <br/> line breaks in stadium
// (and other height-scaling box) classic glyphs. Long single-line labels
// already auto-wrap; this covers authored multi-line text.
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
    auto a = ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(80));
    auto b = ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(80));
    ftxui::Render(a, n);
    ftxui::Render(b, t);
    return a.ToString() != b.ToString();
}
std::string capture(const std::string& body) {
    auto e = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    auto s = ftxui::Screen::Create(ftxui::Dimension::Fixed(40), ftxui::Dimension::Fixed(20));
    ftxui::Render(s, e);
    return s.ToString();
}
}  // namespace

TEST(MermaidMultilineClassic, StadiumExplicitBreakDrawsBothLines) {
    const std::string body = "flowchart TD\n C([One<br/>Two<br/>Three])\n";
    ASSERT_TRUE(drawn_native(body));
    const auto art = capture(body);
    EXPECT_NE(art.find("One"), std::string::npos);
    EXPECT_NE(art.find("Two"), std::string::npos);
    EXPECT_NE(art.find("Three"), std::string::npos);
    // The stadium rounded caps survive the taller box.
    EXPECT_NE(art.find("("), std::string::npos);
    EXPECT_NE(art.find(")"), std::string::npos);
}

TEST(MermaidMultilineClassic, AllThreeBreakSpellingsEquivalent) {
    for (const char* br : {"<br>", "<br/>", "<br />"}) {
        EXPECT_TRUE(drawn_native(std::string("flowchart TD\n A([x") + br + "y])\n")) << br;
    }
}

TEST(MermaidMultilineClassic, MultilineStadiumAcceptsEdges) {
    EXPECT_TRUE(drawn_native(
        "flowchart TD\n A[a] --> B([One<br/>Two])\n B --> C[c]\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart LR\n A([L1<br/>L2]) --> B[b]\n"));
}

TEST(MermaidMultilineClassic, NonScalingClassicShapesStillFailClosed) {
    // Circle ring geometry is a fixed odd-diameter silhouette; a multiline
    // label must not silently produce a broken contour, so the source stays
    // on fallback until that shape gets its own multiline adaptation.
    EXPECT_FALSE(drawn_native("flowchart TD\n A((Multi<br/>Line))\n"));
}
