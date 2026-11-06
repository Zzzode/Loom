// RFC 0031 FS-5 G1: the notched-rectangle card
// (@{ shape: card | notch-rect | notched-rectangle }) is a rectangle with
// its top-left corner cut by a one-cell diagonal; the folder/directory
// (@{ shape: folder | directory }) is a body rectangle with a top-left tab.
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
        "```" + std::string(language) + "\n" + body + "\n```\n\nG1_SENTINEL");
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
int first_row_with(const std::string& art, std::string_view glyph) {
    std::size_t pos = 0u;
    int row = 0;
    while (pos < art.size()) {
        const auto end = art.find('\n', pos);
        const auto line =
            art.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        if (line.find_first_not_of(' ') != std::string::npos &&
            line.find(glyph) != std::string::npos)
            return row;
        if (end == std::string::npos) break;
        pos = end + 1u;
        ++row;
    }
    return -1;
}
}  // namespace

TEST(MermaidFs5g1, CardAliasesDrawWithNotchedCornerAndLabels) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: card, label: \"card\" }\n"
        "n1@{ shape: notch-rect, label: \"notch-rect\" }\n"
        "n2@{ shape: notched-rectangle, label: \"notched-rectangle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    for (const char* label : {"card", "notch-rect", "notched-rectangle"})
        EXPECT_NE(art.find(label), std::string::npos) << label;
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    // The cut corner: a diagonal directly below the top edge at the left.
    EXPECT_NE(art.find("╱"), std::string::npos);
}

TEST(MermaidFs5g1, FolderAliasesDrawWithTabAndBodyEdge) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: folder, label: \"folder\" }\n"
        "n1@{ shape: directory, label: \"directory\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("folder"), std::string::npos);
    EXPECT_NE(art.find("directory"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    // The tab meets the body edge at a T junction; the body edge is one row
    // below the tab (a plain rectangle has no ┴).
    EXPECT_NE(art.find("┴"), std::string::npos);
    EXPECT_EQ(first_row_with(art, "┌"), 0);
    EXPECT_EQ(first_row_with(art, "┴"), 1);
}

TEST(MermaidFs5g1, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: bolt, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: cardd, label: \"x\" }\n"));
}

// A folder must accept edges on all four cardinal sides. The alias smoke
// fixture is edgeless, which would hide a dead port: an edge entering a
// folder from above (onto the body edge below the short tab) or sideways
// (onto the straight body walls) must still render the whole graph native.
TEST(MermaidFs5g1, FolderAcceptsTopAndSideEdges) {
    const std::string incoming_top =
        "flowchart TB\nA[alpha] --> B@{ shape: folder, label: \"q\" }\n";
    EXPECT_TRUE(drawn_native(incoming_top)) << incoming_top;
    const std::string side_edge =
        "flowchart LR\nA[alpha] --> B@{ shape: folder, label: \"reports\" }\n";
    EXPECT_TRUE(drawn_native(side_edge)) << side_edge;
}

TEST(MermaidFs5g1, CardAcceptsTopAndSideEdges) {
    const std::string body =
        "flowchart LR\nA[alpha] --> B@{ shape: card, label: \"card\" }\n";
    EXPECT_TRUE(drawn_native(body)) << body;
}
