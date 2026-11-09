// RFC 0031 FS-7b collapsed-subgraph directive parser behavior. Public API
// only: locks the accept/reject matrix for `<id>@{ view: collapsed }` so a
// future loosening (single quotes, a second key, an unknown value) cannot
// silently turn a whole-source fallback into a native frame. Acceptance is
// asserted with a CellCapture char-vector comparison, never Screen::ToString.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

import std;
import loom.ui.visual.markdown;

namespace {
constexpr int kWidth = 120;
constexpr int kHeight = 80;

// True only when the mermaid fence paints a frame different from the
// verbatim text fence (a real native render, not color-escape noise).
bool draws_native(const std::string& body) {
    auto native = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    auto ordinary = loom::ui::render_markdown("```text\n" + body + "\n```\n");
    if (!native || !ordinary) return false;
    auto a = ftxui::Screen::Create(ftxui::Dimension::Fixed(kWidth),
                                   ftxui::Dimension::Fixed(kHeight));
    auto b = ftxui::Screen::Create(ftxui::Dimension::Fixed(kWidth),
                                   ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(a, native);
    ftxui::Render(b, ordinary);
    std::vector<std::string> native_cells, text_cells;
    native_cells.reserve(static_cast<std::size_t>(kWidth) * kHeight);
    text_cells.reserve(static_cast<std::size_t>(kWidth) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWidth; ++x) {
            native_cells.push_back(a.PixelAt(x, y).character);
            text_cells.push_back(b.PixelAt(x, y).character);
        }
    return native_cells != text_cells;
}

// A collapsed group whose accepted directive makes the body draw natively.
constexpr const char* kAcceptedBody =
    "flowchart TD\n"
    "  Start --> one\n"
    "  subgraph one [My Group]\n"
    "    A --> B\n"
    "  end\n"
    "  one --> End\n";
}  // namespace

TEST(MermaidFs7bDirective, AcceptedSpellingsDrawNatively) {
    // Bare token.
    EXPECT_TRUE(draws_native(std::string(kAcceptedBody) + "  one@{ view: collapsed }\n"));
    // Double-quoted token.
    EXPECT_TRUE(draws_native(std::string(kAcceptedBody) +
                             "  one@{ view: \"collapsed\" }\n"));
    // Idempotent repeat of the same value stays accepted.
    EXPECT_TRUE(draws_native(std::string(kAcceptedBody) +
                             "  one@{ view: collapsed }\n"
                             "  one@{ view: collapsed }\n"));
}

TEST(MermaidFs7bDirective, MalformedDirectivesKeepWholeSource) {
    // Unknown view value.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: expanded }\n"));
    // Single quotes are not accepted (body statements bypass the init
    // directive quote normalizer).
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: 'collapsed' }\n"));
    // A second key makes the block non-singular.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: collapsed, label: \"x\" }\n"));
    // Trailing tokens after the closing brace.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: collapsed } junk\n"));
    // Key-prefix adhesion must not be treated as `view`.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ viewX: collapsed }\n"));
    // Value-prefix adhesion must not be treated as `collapsed`.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: collapsedx }\n"));
    // Unknown group id.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  nope@{ view: collapsed }\n"));
    // A conflicting repeat value rejects the whole source.
    EXPECT_FALSE(draws_native(std::string(kAcceptedBody) +
                              "  one@{ view: collapsed }\n"
                              "  one@{ view: expanded }\n"));
}

TEST(MermaidFs7bDirective, OpenStackAndDeferredShellAndVertexReject) {
    // Directive while the group is still open on the group stack.
    EXPECT_FALSE(draws_native(
        "flowchart TD\n"
        "  subgraph one [My Group]\n"
        "    one@{ view: collapsed }\n"
        "    A --> B\n"
        "  end\n"));
    // A directive naming a group before its declaration targets the
    // pre-registered deferred shell and must reject.
    EXPECT_FALSE(draws_native(
        "flowchart TD\n"
        "  gx@{ view: collapsed }\n"
        "  subgraph gx\n"
        "    B\n"
        "  end\n"));
    // A vertex id (not a group) carrying the directive falls through to the
    // shape-data parser, which has no `view` key, and rejects.
    EXPECT_FALSE(draws_native(
        "flowchart TD\n"
        "  A --> B\n"
        "  A@{ view: collapsed }\n"));
}

TEST(MermaidFs7bDirective, CrossCollapsedGroupEdgeFailsClosed) {
    // An edge joining vertices hidden inside TWO DIFFERENT collapsed groups
    // cannot be drawn until Slice C remaps it onto both collapsed nodes; Slice
    // B must keep the whole source rather than silently drop the edge. The
    // same-group internal edge, by contrast, is correctly hidden (native).
    EXPECT_FALSE(draws_native(
        "flowchart TD\n"
        "  subgraph g1\n"
        "    A\n"
        "  end\n"
        "  subgraph g2\n"
        "    B\n"
        "  end\n"
        "  A --> B\n"
        "  g1@{ view: collapsed }\n"
        "  g2@{ view: collapsed }\n"));
    // One collapsed group with a purely internal edge still draws natively.
    EXPECT_TRUE(draws_native(
        "flowchart TD\n"
        "  Start --> g1\n"
        "  subgraph g1\n"
        "    A --> B\n"
        "  end\n"
        "  g1@{ view: collapsed }\n"));
}
