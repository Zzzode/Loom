// RFC 0031 FS-5 G7: the shaded/lined process, curved trapezoid (display),
// and paper tape (flag) flowchart shapes.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG7_SENTINEL");
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

TEST(MermaidFs5g7, ShadedProcessAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: lin-rect, label: \"lin-rect\" }\n"
        "n1@{ shape: lined-rectangle, label: \"lined-rectangle\" }\n"
        "n2@{ shape: lin-proc, label: \"lin-proc\" }\n"
        "n3@{ shape: lined-process, label: \"lined-process\" }\n"
        "n4@{ shape: shaded-process, label: \"shaded-process\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("lin-rect"), std::string::npos);
    // The left frame compartment junction.
    EXPECT_NE(art.find("┬"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g7, CurvedTrapezoidAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: curv-trap, label: \"curv-trap\" }\n"
        "n1@{ shape: display, label: \"display\" }\n"
        "n2@{ shape: curved-trapezoid, label: \"curved-trapezoid\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("curv-trap"), std::string::npos);
    // Left chevron point and the rounded right cap.
    EXPECT_NE(art.find("◁"), std::string::npos);
    EXPECT_NE(art.find(")"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g7, PaperTapeAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: flag, label: \"flag\" }\n"
        "n1@{ shape: paper-tape, label: \"paper-tape\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("flag"), std::string::npos);
    // Wavy top (crest) and wavy bottom (trough).
    EXPECT_NE(art.find("╲ ╱"), std::string::npos);
    EXPECT_NE(art.find("╱ ╲"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g7, ShapesAcceptEdges) {
    static const char* kShapes[] = {"lin-rect", "curv-trap", "flag"};
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
        // BT places the source below the target, so the edge enters through
        // the bottom port (shaded ┴ / curved lower cap / paper trough floor).
        EXPECT_TRUE(drawn_native(
            "flowchart BT\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n")) << shape;
    }
}

TEST(MermaidFs5g7, SingleNodeContourRows) {
    // Row-level signature checks, stronger than whole-surface substrings.
    auto lines = [](const std::string& body) {
        const auto art = capture(body, "mermaid");
        std::vector<std::string> out;
        for (std::size_t start = 0; start < art.size();) {
            const auto end = art.find('\n', start);
            out.push_back(art.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return out;
    };
    const auto shaded =
        lines("flowchart \nA@{ shape: lin-rect, label: \"x\" }\n");
    bool top_junction = false, bottom_junction = false;
    for (const auto& row : shaded) {
        if (row.rfind("┌┬", 0) == 0) top_junction = true;
        if (row.rfind("└┴", 0) == 0) bottom_junction = true;
    }
    EXPECT_TRUE(top_junction);
    EXPECT_TRUE(bottom_junction);

    const auto curved =
        lines("flowchart \nA@{ shape: curv-trap, label: \"x\" }\n");
    EXPECT_TRUE(std::any_of(curved.begin(), curved.end(),
                            [](const std::string& r) { return r.find("◁") != std::string::npos; }));
}

TEST(MermaidFs5g7, PaperTapeCrestAlignsWithBaselineOnEvenWidth) {
    // A six-char label forces an even (ten-column) box. The crest cap on the
    // row above must sit in the same column as the gap between the baseline
    // ascent/descent (the wave axis l+(w-1)/2, not width/2).
    const auto art = capture(
        "flowchart \nA@{ shape: flag, label: \"abcdef\" }\n", "mermaid");
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
    int baseline = -1, gap_col = -1;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
        for (int x = 0; x + 2 < static_cast<int>(rows[y].size()); ++x)
            if (rows[y][x] == "╱" && rows[y][x + 1] == " " && rows[y][x + 2] == "╲") {
                baseline = y;
                gap_col = x + 1;
            }
    ASSERT_GE(baseline, 1);
    ASSERT_GE(gap_col, 0);
    EXPECT_EQ(rows[baseline - 1][gap_col], "─");  // crest cap aligned
}

TEST(MermaidFs5g7, UnknownShapeAliasesKeepWholeSource) {
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: lin-rect-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: shaded-processes, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: curv-traps, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: flags, label: \"x\" }\n"));
}
