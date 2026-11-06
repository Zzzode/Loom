// RFC 0031 FS-4h: a closed subset of inline HTML in quoted flowchart labels
// projects to terminal text: <br> (already admitted), <strong>/<b> tags are
// stripped while retaining their inner text, and void <img ...> tags are
// consumed. Anything outside this tag set keeps the whole source on ordinary
// fallback so arbitrary HTML is never silently mangled.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>

import std;
import loom.ui.visual.markdown;

namespace {
std::string capture(const std::string& body, const char* language) {
    auto element = loom::ui::render_markdown(
        "```" + std::string(language) + "\n" + body + "\n```\n\nFS4H_SENTINEL");
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

TEST(MermaidFs4HtmlLabel, StrongTextSurvivesAndTagMarkupIsConsumed) {
    ASSERT_TRUE(drawn_native("graph TD\n  a[\"<strong>Haiya</strong>\"]-->b\n"));
    const auto art = capture("graph TD\n  a[\"<strong>Haiya</strong>\"]-->b\n", "mermaid");
    EXPECT_NE(art.find("Haiya"), std::string::npos);
    EXPECT_EQ(art.find("<strong"), std::string::npos);
    EXPECT_EQ(art.find("</strong>"), std::string::npos);

    // Void image tags are consumed, surrounding text retained.
    const std::string images =
        "flowchart TD\n"
        "B-->C[\"<img src='https://example.test/a.svg'> more text\"]\n";
    ASSERT_TRUE(drawn_native(images)) << images;
    const auto art2 = capture(images, "mermaid");
    EXPECT_NE(art2.find("more text"), std::string::npos);
    EXPECT_EQ(art2.find("<img"), std::string::npos);
    EXPECT_EQ(art2.find("example.test"), std::string::npos);
}

TEST(MermaidFs4HtmlLabel, ImageFormsTheNodeScannerCannotDelimitStaySource) {
    // An image-only label leaves the node without text, and an unquoted
    // bracket label cannot carry the attribute's embedded quotes; these
    // independent scanner limits keep the whole source rather than draw a
    // different graph.
    EXPECT_FALSE(drawn_native("flowchart TD\nB[<img>]\n"));
    EXPECT_FALSE(drawn_native(
        "flowchart TD\nB-->C[<img src=\"https://example.test/a.svg\"> more text]\n"));
}

TEST(MermaidFs4HtmlLabel, UnknownTagsKeepWholeSource) {
    // Tags outside the {br, strong, b, img} set must fail closed.
    EXPECT_FALSE(drawn_native("flowchart TD\n  a[\"<em>Haiya</em>\"]-->b\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n  a[\"<div>Haiya</div>\"]-->b\n"));
    // A tag whose name merely starts with img is not an image element.
    EXPECT_FALSE(drawn_native("flowchart TD\n  a[\"<imgrand>Haiya</imgrand>\"]-->b\n"));
    // A '>' inside a quoted attribute does not truncate the tag: the whole
    // image element is consumed and the trailing text is retained without
    // leaking the attribute fragment.
    const std::string quoted_gt =
        "flowchart TD\n  a[\"<img src='a>b'> tail\"]-->b\n";
    ASSERT_TRUE(drawn_native(quoted_gt)) << quoted_gt;
    const auto quoted_art = capture(quoted_gt, "mermaid");
    EXPECT_NE(quoted_art.find("tail"), std::string::npos);
    EXPECT_EQ(quoted_art.find("src="), std::string::npos);
    EXPECT_EQ(quoted_art.find("a>b"), std::string::npos);
}
