// RFC 0031 FS-5 G5: the four wave-edged document shapes - document,
// stacked document, lined document, and tagged document. Their common
// signature is a bottom edge that dips one cell into a centred trough.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nG5_SENTINEL");
    if (!element) return {};
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(120));
    ftxui::Render(screen, element);
    return screen.ToString();
}
bool drawn_native(const std::string& body) {
    const auto native = capture(body, "mermaid");
    const auto plain = capture(body, "text");
    return !native.empty() && native != plain;
}
}  // namespace

TEST(MermaidFs5g5, DocumentAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: doc, label: \"doc\" }\n"
        "n1@{ shape: document, label: \"document\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("doc"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

TEST(MermaidFs5g5, StackedDocumentAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: docs, label: \"docs\" }\n"
        "n1@{ shape: documents, label: \"documents\" }\n"
        "n2@{ shape: st-doc, label: \"st-doc\" }\n"
        "n3@{ shape: stacked-document, label: \"stacked-document\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    EXPECT_NE(capture(body, "mermaid").find("st-doc"), std::string::npos);
}

TEST(MermaidFs5g5, LinedDocumentAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: lin-doc, label: \"lin-doc\" }\n"
        "n1@{ shape: lined-document, label: \"lined-document\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    EXPECT_NE(capture(body, "mermaid").find("lin-doc"), std::string::npos);
}

TEST(MermaidFs5g5, TaggedDocumentAliasesDraw) {
    const std::string body =
        "flowchart \n"
        "n0@{ shape: tag-doc, label: \"tag-doc\" }\n"
        "n1@{ shape: tagged-document, label: \"tagged-document\" }\n";
    ASSERT_TRUE(drawn_native(body)) << body;
    const auto art = capture(body, "mermaid");
    EXPECT_NE(art.find("tag-doc"), std::string::npos);
    // The side tag projects as a right-pointing tip.
    EXPECT_NE(art.find("▷"), std::string::npos);
    EXPECT_EQ(art.find("shape:"), std::string::npos);
}

// Every wave shape keeps edge connectivity from all four directions - the
// trough moves the bottom port one row below the chamber and the stacked/
// tagged variants carry custom side corridors.
TEST(MermaidFs5g5, ShapesAcceptEdges) {
    static const char* kShapes[] = {"doc", "docs", "lin-doc", "tag-doc"};
    for (const char* shape : kShapes) {
        const std::string quoted = std::string("\"") + shape + "\"";
        EXPECT_TRUE(drawn_native(
            "flowchart TB\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n"))
            << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart LR\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n"))
            << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart LR\nA@{ shape: " + std::string(shape) +
            ", label: \"x\" } --> B[b]\n"))
            << shape;
        EXPECT_TRUE(drawn_native(
            "flowchart RL\nA[a] --> B@{ shape: " + std::string(shape) +
            ", label: \"x\" }\n"))
            << shape;
    }
}

TEST(MermaidFs5g5, WaveTroughSignature) {
    // The dip is a one-cell trough centred in the box: descent and ascent on
    // the baseline row with a gap between them, and a horizontal floor one
    // row below at the centre column.
    const auto art = capture(
        "flowchart \nA@{ shape: doc, label: \"doc\" }\n", "mermaid");
    // Split a rendered row into per-column glyphs (UTF-8 code points).
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
    int baseline = -1;
    int notch_col = -1;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
        for (int x = 0; x + 2 < static_cast<int>(rows[y].size()); ++x) {
            if (rows[y][x] == "╲" && rows[y][x + 1] == " " && rows[y][x + 2] == "╱") {
                baseline = y;
                notch_col = x;
                break;
            }
        }
        if (baseline >= 0) break;
    }
    ASSERT_GE(baseline, 0);
    ASSERT_LT(baseline + 1, static_cast<int>(rows.size()));
    // The floor sits one row below the gap cell, i.e. one column right of the
    // descent glyph.
    EXPECT_EQ(rows[baseline + 1][notch_col + 1], "─");
}

TEST(MermaidFs5g5, BottomPortAlignsWithTroughOnEvenWidth) {
    // Regression: an even-width box (label of six chars gives width ten) puts
    // the trough centre at l+(w-1)/2 while width/2 is one column to the
    // right. A bottom-entering edge (BT source sits below) must arrive on the
    // trough floor column, not a blocked blank cell.
    const std::string body =
        "flowchart BT\nA[a] --> B@{ shape: doc, label: \"aaaaaa\" }\n";
    ASSERT_TRUE(drawn_native(body));
    const auto art = capture(body, "mermaid");
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
    int baseline = -1;
    int descent_col = -1;
    for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
        for (int x = 0; x + 2 < static_cast<int>(rows[y].size()); ++x) {
            if (rows[y][x] == "╲" && rows[y][x + 1] == " " && rows[y][x + 2] == "╱") {
                baseline = y;
                descent_col = x;
                break;
            }
        }
        if (baseline >= 0) break;
    }
    ASSERT_GE(baseline, 0);
    const int floor_col = descent_col + 1;
    EXPECT_EQ(rows[baseline + 1][floor_col], "─");
    // The up-pointing arrowhead sits one row below the floor on the same
    // column (the edge source is below the document in BT orientation).
    ASSERT_LT(baseline + 2, static_cast<int>(rows.size()));
    EXPECT_EQ(rows[baseline + 2][floor_col], "▲");
}

TEST(MermaidFs5g5, UnknownShapeAliasesKeepWholeSource) {
    // paper-tape is the far-roadmap wave-rectangle alias (shape 35).
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: paper-tape, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: doc-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: st-doc-x, label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nA@{ shape: tagged-documents, label: \"x\" }\n"));
}
