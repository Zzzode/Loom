// RFC 0031 FS-aws: closed-vocabulary AWS icon shape data
// (`@{ icon: "aws:<name>" [, label: "..."] }`). The terminal has no icon
// font, so an admitted icon projects to one fixed marker cell (U+25A0)
// in front of the human label or node id inside a rectangle; unknown
// icons, other namespaces, and extra keys keep whole-source fallback.
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
    auto a = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    auto b = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    ftxui::Render(a, n);
    ftxui::Render(b, t);
    return a.ToString() != b.ToString();
}

std::string surface_text(const std::string& body) {
    auto element = loom::ui::render_markdown("```mermaid\n" + body + "\n```\n");
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(80));
    ftxui::Render(screen, element);
    std::string out;
    for (int y = 0; y < 80; ++y)
        for (int x = 0; x < 120; ++x) out += screen.PixelAt(x, y).character;
    return out;
}
}  // namespace

TEST(MermaidAwsIcon, ClosedVocabularyDraws) {
    EXPECT_TRUE(drawn_native(
        "flowchart TB\n Cloudwatch@{ icon: \"aws:arch-amazon-cloudwatch\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\n EKS@{ icon: \"aws:arch-amazon-eks-cloud\", label: \"EKS Cloud\" }\n"));
    EXPECT_TRUE(drawn_native(
        "flowchart TB\n R@{ icon: \"aws:arch-amazon-route-53\" }\n"));
}

TEST(MermaidAwsIcon, MarkerCellAndHumanText) {
    // Icon-only node: marker plus the node id.
    auto solo = surface_text("flowchart TB\n Solo@{ icon: \"aws:arch-amazon-route-53\" }\n");
    EXPECT_NE(solo.find("\xE2\x96\xA0 Solo"), std::string::npos);
    // Labelled node: marker plus the explicit label, not the id.
    auto labelled = surface_text(
        "flowchart TB\n CW@{ icon: \"aws:arch-amazon-cloudwatch\", label: \"CloudWatch\" }\n");
    EXPECT_NE(labelled.find("\xE2\x96\xA0 CloudWatch"), std::string::npos);
    EXPECT_EQ(labelled.find("CW"), std::string::npos);
}

TEST(MermaidAwsIcon, ChainedIconNodesRouteEdges) {
    const auto body =
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\" }\n"
        " B@{ icon: \"aws:arch-amazon-route-53\" }\n A --> B\n";
    EXPECT_TRUE(drawn_native(body));
    const auto grid = surface_text(body);
    EXPECT_NE(grid.find("▼"), std::string::npos);
}

TEST(MermaidAwsIcon, UnknownIconsAndForeignNamespacesFailClosed) {
    // Unknown aws icon name.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ icon: \"aws:not-a-real-icon\" }\n"));
    // A non-aws namespace is not part of the closed vocabulary.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ icon: \"fa:car\" }\n"));
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ icon: \"gcp:pubsub\" }\n"));
    // Malformed / unquoted values.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ icon: aws:arch-amazon-cloudwatch }\n"));
    // Extra keys are rejected rather than partially admitted.
    EXPECT_FALSE(drawn_native(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\", shape: \"rect\" }\n"));
    EXPECT_FALSE(drawn_native(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\", animation: \"pulse\" }\n"));
}

TEST(MermaidAwsIcon, RepeatedIconStatementsAreIdempotent) {
    // Repeated icon-only declarations must not stack a second marker.
    const auto twice = surface_text(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\" }\n"
        " A@{ icon: \"aws:arch-amazon-cloudwatch\" }\n");
    EXPECT_NE(twice.find("\xE2\x96\xA0 A"), std::string::npos);
    EXPECT_EQ(twice.find("\xE2\x96\xA0 \xE2\x96\xA0"), std::string::npos);
    // Icon plus an explicit label repeated stays a single marker with the
    // label intact (logical-label comparison ignores the prepended marker).
    const auto labelled_twice = surface_text(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\", label: \"X\" }\n"
        " A@{ icon: \"aws:arch-amazon-cloudwatch\", label: \"X\" }\n");
    EXPECT_NE(labelled_twice.find("\xE2\x96\xA0 X"), std::string::npos);
    EXPECT_EQ(labelled_twice.find("\xE2\x96\xA0 \xE2\x96\xA0"), std::string::npos);
    // A conflicting repeated label still fails closed.
    EXPECT_FALSE(drawn_native(
        "flowchart TB\n A@{ icon: \"aws:arch-amazon-cloudwatch\", label: \"X\" }\n"
        " A@{ icon: \"aws:arch-amazon-cloudwatch\", label: \"Y\" }\n"));
}

TEST(MermaidAwsIcon, BracketNodeWithIconFailsClosed) {
    // Icon metadata is only projected on the bare-id path; a bracket node
    // must not silently swallow the icon and render marker-less.
    EXPECT_FALSE(drawn_native(
        "flowchart TB\n A[Label]@{ icon: \"aws:arch-amazon-cloudwatch\" }\n"));
    // Label-only metadata on a bracket node is still admitted.
    EXPECT_TRUE(drawn_native("flowchart TB\n A[Label]@{ label: \"Label\" }\n"));
}

TEST(MermaidAwsIcon, ExistingShapeDataFormsUnaffected) {
    // Label-only metadata still draws as a rectangle (FS-5a).
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ label: \"Plain\" }\n"));
    // Explicit shape metadata is unchanged (unquoted token grammar).
    EXPECT_TRUE(drawn_native("flowchart TB\n A@{ shape: cylinder }\n"));
    // FontAwesome inline markers remain their own FS-6 path.
    EXPECT_TRUE(drawn_native("flowchart TB\n F[fa:fa-car Car] --> G[Garage]\n"));
    // A bare unknown metadata key still fails.
    EXPECT_FALSE(drawn_native("flowchart TB\n A@{ animation: \"pulse\" }\n"));
}
