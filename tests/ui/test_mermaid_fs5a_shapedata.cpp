// RFC 0031 FS-5a: the label-only shape-data node form `id@{ label: "text" }`
// (mermaid's newer per-node metadata syntax, with no `shape:` key) is a
// rectangular labeled node. The parser currently accepts only
// `@{shape: x, label: ...}`; the label-first/label-only form fails. Unknown
// metadata keys (icon, animation, a non-rectangle shape) keep whole-source
// fallback.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS5A_SENTINEL");
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

TEST(MermaidFs5aShapeData, LabelOnlyNodeRendersAsRectangle) {
    const std::string chain =
        "flowchart TB\n"
        "n2[\"label for n2\"] & n4@{ label: \"label for n4\"} & "
        "n5@{label : \"label for n5\"}\n";
    ASSERT_TRUE(drawn_native(chain)) << chain;
    const auto art = capture(chain, "mermaid");
    EXPECT_NE(art.find("label for n4"), std::string::npos);
    EXPECT_NE(art.find("label for n5"), std::string::npos);
    EXPECT_EQ(art.find("@{"), std::string::npos);

    // A standalone label-only node and an edge into one.
    const std::string standalone =
        "flowchart TD\n"
        "A[\"A\"] --> B[\"for B\"] & C@{ label: \"for c\"}\n"
        "D@{label: \"for D\"}\n";
    EXPECT_TRUE(drawn_native(standalone)) << standalone;
}

TEST(MermaidFs5aShapeData, UnknownShapeDataKeysKeepWholeSource) {
    // An icon binding or an explicit non-rectangle shape is not a plain
    // label node and must keep whole-source fallback.
    EXPECT_FALSE(drawn_native("flowchart TD\nB@{ icon: \"fa:bell\", label: \"x\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\nB@{ shape: bolt, label: \"x\" }\n"));
    // A dangling or empty metadata block is not a label node.
    EXPECT_FALSE(drawn_native("flowchart TD\nB@{ label: }\n"));
    // Same-prefix keys are not the label key.
    EXPECT_FALSE(drawn_native("flowchart TD\nB@{ labelled: \"x\" }\n"));
}

TEST(MermaidFs5aShapeData, LabelOnlyMetadataDoesNotFlattenBracketShape) {
    // A label-only block following a shaped bracket declaration must retain
    // the bracket shape rather than force a rectangle; the diagram still
    // draws and the metadata label reconciles with the bracket text.
    const std::string round =
        "flowchart LR\nA(Round)@{ label: \"Round\" } --> B[Next]\n";
    ASSERT_TRUE(drawn_native(round)) << round;
    // An explicit shape in the metadata is still honored (non-rectangle
    // shapes outside the renderer fail closed as before).
    EXPECT_FALSE(drawn_native(
        "flowchart LR\nA(Round)@{ shape: bolt, label: \"Round\" } --> B\n"));
}
