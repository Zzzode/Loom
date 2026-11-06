// RFC 0031 FS-6: FontAwesome icon markers in flowchart node and edge labels
// project to a single fixed terminal marker cell (the terminal has no icon
// font); the human label text is retained. Only a closed icon vocabulary is
// admitted; an unknown icon name or a malformed marker keeps the whole source
// on ordinary fallback.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS6_SENTINEL");
    if (!element) return {};
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(60));
    ftxui::Render(screen, element);
    return screen.ToString();
}
bool drawn_native(const std::string& body) {
    const auto native = capture(body, "mermaid");
    const auto plain = capture(body, "text");
    return !native.empty() && native != plain;
}
}  // namespace

TEST(MermaidFs6Icon, KnownIconsProjectToMarkerAndTextSurvives) {
    // Node label icon.
    const std::string node = "flowchart TD\nF[fa:fa-car Car] --> G[Garage]\n";
    ASSERT_TRUE(drawn_native(node)) << node;
    auto art = capture(node, "mermaid");
    EXPECT_NE(art.find("Car"), std::string::npos);
    EXPECT_NE(art.find("Garage"), std::string::npos);
    // The marker syntax itself never leaks.
    EXPECT_EQ(art.find("fa:fa-"), std::string::npos);

    // Icon in an edge label and in a quoted diamond label.
    const std::string edge =
        "flowchart TD\nC -->|fa:fa-car Car| F[fa:fa-car Car]\n"
        "D --> I{\"fa:fa-code Text\"}\n";
    ASSERT_TRUE(drawn_native(edge)) << edge;
    art = capture(edge, "mermaid");
    EXPECT_EQ(art.find("fa:fa-"), std::string::npos);
    EXPECT_NE(art.find("Text"), std::string::npos);
}

TEST(MermaidFs6Icon, UnknownIconsKeepWholeSource) {
    // An icon marker with a name outside the closed vocabulary fails closed
    // even when the rest of the graph is legal.
    EXPECT_FALSE(drawn_native("flowchart TD\nF[fa:fa-rocket Ship] --> G\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nF[fa:fa-cogs Engine] --> G\n"));
}
