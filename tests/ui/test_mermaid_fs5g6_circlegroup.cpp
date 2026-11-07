// RFC 0031 FS-5 G6: the circle-family flowchart shapes - filled circle
// (junction), framed/stop circle (UML final state), and crossed circle
// (summary). All reuse the circular ring silhouette.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG6_SENTINEL");
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

TEST(MermaidFs5g6, FilledCircleAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: f-circ, label: \"f-circ\" }\n"
        "n1@{ shape: junction, label: \"junction\" }\n"
        "n2@{ shape: filled-circle, label: \"filled-circle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    // Upstream filledCircle is a solid disc with the label cleared.
    EXPECT_NE(art.find("█"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
    EXPECT_EQ(art.find("filled-circle"), std::string::npos);
}

TEST(MermaidFs5g6, FramedCircleAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: fr-circ, label: \"fr-circ\" }\n"
        "n1@{ shape: framed-circle, label: \"framed-circle\" }\n"
        "n2@{ shape: stop, label: \"stop\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    // Outer ring plus a filled inner core (the UML final-state dot).
    EXPECT_NE(art.find("█"), std::string::npos);
    EXPECT_NE(art.find("╭"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g6, CrossedCircleAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: cross-circ, label: \"cross-circ\" }\n"
        "n1@{ shape: summary, label: \"summary\" }\n"
        "n2@{ shape: crossed-circle, label: \"crossed-circle\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    // Upstream crossedCircle is a fixed-size label-less glyph (label is
    // cleared), so the crossing centre stays visible and no label survives.
    EXPECT_NE(art.find("╳"), std::string::npos);
    EXPECT_EQ(art.find("crossed-circle"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g6, ShapesAcceptEdges) {
    static const char* kShapes[] = {"f-circ", "fr-circ", "cross-circ"};
    for (const char* shape : kShapes) {
        EXPECT_TRUE(drawn_native(
            "flowchart TB\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n")) << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart LR\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n")) << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart LR\nA@{ shape: " + std::string(shape) +
            ", label: \"x\" } --> B[b]\n")) << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart RL\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n")) << shape;
    }
}

TEST(MermaidFs5g6, CrossedXIsTopBottomSymmetric) {
    // The two strokes must mirror about the centre row for the produced
    // (fixed) diameter: a descent glyph at column c above the ╳ must pair
    // with an ascent glyph at the mirrored column on the mirrored row.
    const auto art = capture(
        "flowchart \nA@{ shape: cross-circ, label: \"x\" }\n", "mermaid");
    auto columns = [](const std::string& row) {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < row.size();) {
            int len = 1;
            const unsigned char lead = static_cast<unsigned char>(row[i]);
            if (lead >= 0xF0) len = 4;
            else if (lead >= 0xE0) len = 3;
            else if (lead >= 0xC0) len = 2;
            out.push_back(row.substr(i, len));
            i += static_cast<std::size_t>(len);
        }
        return out;
    };
    std::vector<std::vector<std::string>> rows;
    for (std::size_t start = 0; start < art.size();) {
        const auto end = art.find('\n', start);
        rows.push_back(columns(art.substr(start, end == std::string::npos ? end : end - start)));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    int cy = -1, cx = -1;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x)
            if (rows[y][x] == "╳") { cy = y; cx = x; }
    ASSERT_GE(cy, 0);
    // Nearest row above the centre that carries an X stroke; the mirrored
    // row below must contain the swapped strokes at mirrored columns.
    int top = cy - 1;
    while (top >= 0) {
        const auto& r = rows[top];
        if (std::find(r.begin(), r.end(), std::string("╲")) != r.end() ||
            std::find(r.begin(), r.end(), std::string("╱")) != r.end())
            break;
        --top;
    }
    ASSERT_GE(top, 0);
    const int bottom = 2 * cy - top;
    ASSERT_LT(bottom, static_cast<int>(rows.size()));
    // A stroke mirrors to the same glyph on the opposite column: the
    // descending ╲ at top column x reaches the descending ╲ at bottom column
    // 2*cx-x (and conversely for the ascending ╱); it is a position mirror,
    // not a glyph swap.
    for (int x = 0; x <= cx; ++x) {
        const int mx = 2 * cx - x;
        ASSERT_LT(mx, static_cast<int>(rows[bottom].size()));
        EXPECT_EQ(rows[bottom][mx], rows[top][x]) << "x=" << x;
    }
}

TEST(MermaidFs5g6, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: cloud, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: f-circ-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: filled-circles, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: cross-circ-x, label: \"x\" }\n"));
}
