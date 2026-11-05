// RFC 0031 FS-3a: flowchart classDef/class/direct-style statements project to
// terminal style attributes in PRODUCTION (the style4 machinery graduated from
// its test-only preview gate). The terminal has no CSS: fill/stroke/color map
// to palette hue roles and stroke-width >= 4 projects to a bold border; every
// applied style statement is disclosed verbatim in a projection legend that
// also names the projected roles. Anything outside the closed property
// grammar (unsupported CSS keys, undefined class names, non-rectangle/group
// targets, linkStyle) keeps the whole source on ordinary fallback.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
constexpr int kWidth = 240;
constexpr int kHeight = 80;

std::string render_text(const std::string& body, const char* language) {
    const std::string fenced =
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS3_SENTINEL";
    auto element = loom::ui::render_markdown(fenced);
    if (!element) return {};
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(kWidth), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    return screen.ToString();
}

bool drawn_native(const std::string& body) {
    const auto native = render_text(body, "mermaid");
    const auto plain = render_text(body, "text");
    return !native.empty() && native != plain;
}
}  // namespace

TEST(MermaidFs3Style, ClassDefinitionAndAssignmentAreProjected) {
    const std::string source =
        "flowchart LR\n"
        "A[Lorem] --> B[Ipsum]\n"
        "B --> C[Dolor]\n"
        "classDef hot fill:#f9f,stroke:#333,stroke-width:4px\n"
        "class A,C hot\n";
    ASSERT_TRUE(drawn_native(source)) << source;
    const auto art = render_text(source, "mermaid");
    // The graph paints natively: node labels present, raw graph source dumped
    // nowhere (no header line, no edge syntax).
    EXPECT_NE(art.find("Lorem"), std::string::npos);
    EXPECT_NE(art.find("Dolor"), std::string::npos);
    EXPECT_EQ(art.find("flowchart"), std::string::npos);
    EXPECT_EQ(art.find("-->"), std::string::npos);
    // The applied statement is disclosed verbatim in the projection legend,
    // together with its terminal projection (bold border for width >= 4).
    EXPECT_NE(art.find("classDef hot fill:#f9f"), std::string::npos);
    EXPECT_NE(art.find("width:bold"), std::string::npos);
}

TEST(MermaidFs3Style, DirectStyleAndCascadeDrawWithLegend) {
    const std::string source =
        "flowchart TD\n"
        "S[Start] --> P[Process]\n"
        "P --> E[End]\n"
        "classDef hl fill:#ffff00\n"
        "class P hl\n"
        "style E fill:#00ff00,stroke:#000000\n";
    ASSERT_TRUE(drawn_native(source)) << source;
    const auto art = render_text(source, "mermaid");
    EXPECT_NE(art.find("Start"), std::string::npos);
    EXPECT_NE(art.find("Process"), std::string::npos);
    EXPECT_EQ(art.find("flowchart"), std::string::npos);
    EXPECT_NE(art.find("class P hl"), std::string::npos);
    EXPECT_NE(art.find("style E fill:#00ff00"), std::string::npos);
}

TEST(MermaidFs3Style, UnsupportedShapeAndGrammarKeepWholeSource) {
    // linkStyle is edge styling and stays outside FS-3a.
    EXPECT_FALSE(drawn_native(
        "flowchart LR\nA --> B\nlinkStyle 0 stroke:#ff0000\n"));
    // Undefined class name cannot resolve: whole source stays verbatim.
    EXPECT_FALSE(drawn_native(
        "flowchart LR\nA --> B\nclass A neverDefined\n"));
    // Unsupported CSS property fails closed.
    EXPECT_FALSE(drawn_native(
        "flowchart LR\nA --> B\nstyle A fill:#f9f,font-size:12px\n"));
    // Styles targeting nodes inside a group stay closed with the group.
    EXPECT_FALSE(drawn_native(
        "flowchart TD\nsubgraph G\nA\nend\nstyle A fill:#f9f\n"));
}
