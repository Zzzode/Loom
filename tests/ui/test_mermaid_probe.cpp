// RFC 0031 E4: ordered family probe-table invariants. This unit drives the
// classify probe through the loom.ui.visual.mermaid.testing seam facade
// (RFC 0031 E6a); it never declares the production module.
#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

import std;
import loom.ui.visual.mermaid.testing;

namespace {
using loom::ui::mermaid::testing::MermaidProbeFamily;
using loom::ui::mermaid::testing::Work;

std::optional<loom::ui::mermaid::testing::MermaidProbeDecision>
classify(const std::string& source, std::size_t remaining = 1'000'000) {
    Work work;
    work.remaining = remaining;
    return loom::ui::mermaid::testing::classify_mermaid(source, work);
}

} // namespace

TEST(MermaidProbe, BareAndConfiguredFlowHeadersResolveToFlow) {
    // A bare parseable flow is owned by the pre-table parse ahead of the
    // probe table; classify runs only on its fallback, so assert configured.
    auto configured = classify(
        "---\ndescription: x\n---\nflowchart LR\nA-->B\n");
    ASSERT_TRUE(configured.has_value());
    EXPECT_EQ(configured->family, MermaidProbeFamily::Flow);
    // RFC 0031 E5: the prefix walk is folded into the single classify probe
    // fee; decision.prepay stays zero for every family.
    EXPECT_EQ(configured->prepay, 0u);
}

TEST(MermaidProbe, SequenceRoutesBareTitledAndMultilineDirective) {
    auto bare = classify("sequenceDiagram\nA->>B: x\n");
    ASSERT_TRUE(bare.has_value());
    EXPECT_EQ(bare->family, MermaidProbeFamily::Sequence);
    EXPECT_EQ(bare->prepay, 0u);

    auto titled = classify("---\ntitle: T\n---\nsequenceDiagram\nA->>B: x\n");
    ASSERT_TRUE(titled.has_value());
    EXPECT_EQ(titled->family, MermaidProbeFamily::Sequence);
    // RFC 0031 E5: folded prefix walk — prepay stays zero.
    EXPECT_EQ(titled->prepay, 0u);

    auto multiline = classify(
        "%%{init: {\n  \"theme\":\"base\"\n}%%\nsequenceDiagram\nA->>B: x\n");
    ASSERT_TRUE(multiline.has_value());
    EXPECT_EQ(multiline->family, MermaidProbeFamily::Sequence);
}

TEST(MermaidProbe, MultilineDirectiveOwnsClassOnlyForExactHeader) {
    auto exact = classify(
        "%%{init: {\n  \"x\":1\n}%%\nclassDiagram\nclass A\n");
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(exact->family, MermaidProbeFamily::Class);
    EXPECT_TRUE(exact->class_envelope);

    auto typo = classify(
        "%%{init: {\n  \"x\":1\n}%%\nclassDiagramFoo\n");
    ASSERT_TRUE(typo.has_value());
    EXPECT_EQ(typo->family, MermaidProbeFamily::Charts);
}

TEST(MermaidProbe, LeadingInitDirectiveMarksClassEnvelope) {
    auto init = classify("%%{init: {}}%%\nclassDiagram\nclass A\n");
    ASSERT_TRUE(init.has_value());
    EXPECT_EQ(init->family, MermaidProbeFamily::Class);
    EXPECT_TRUE(init->class_envelope);
    EXPECT_EQ(init->prepay, 0u); // no frontmatter -> no prefix prepay

    auto er = classify("%%{init: {}}%%\nerDiagram\nA ||--o{ B\n");
    ASSERT_TRUE(er.has_value());
    EXPECT_EQ(er->family, MermaidProbeFamily::Er);
    EXPECT_FALSE(er->class_envelope);
}

TEST(MermaidProbe, RelationsPrefixesResolveExactFamilies) {
    EXPECT_EQ(classify("erDiagram\nA ||--o{ B\n")->family, MermaidProbeFamily::Er);
    EXPECT_EQ(classify("stateDiagram\n[*] --> A\n")->family, MermaidProbeFamily::State);
    EXPECT_EQ(classify("stateDiagram-v2\n[*] --> A\n")->family, MermaidProbeFamily::State);
    EXPECT_EQ(classify("classDiagram\nclass A\n")->family, MermaidProbeFamily::Class);
    EXPECT_EQ(classify("classDiagram-v2\nclass A\n")->family, MermaidProbeFamily::Class);
    // starts_with probe shape stays: erDiagramX selects Er, owner rejects it.
    auto typo = classify("erDiagramX\n");
    ASSERT_TRUE(typo.has_value());
    EXPECT_EQ(typo->family, MermaidProbeFamily::Er);
}

TEST(MermaidProbe, RequirementResolvesFoldedAndExact) {
    auto exact = classify(
        "---\ndescription: >-\n  x\n---\nrequirementDiagram\ntest satisfies t\n");
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(exact->family, MermaidProbeFamily::Requirement);
    EXPECT_TRUE(exact->requirement_input);
}

TEST(MermaidProbe, GitGraphResolvesBeforeRelationsAndCharts) {
    EXPECT_EQ(classify("gitGraph\n  commit id:\"1\"\n")->family, MermaidProbeFamily::GitGraph);
    EXPECT_EQ(classify("gitGraph TB:\n  commit\n")->family, MermaidProbeFamily::GitGraph);
    auto cohort = classify(
        "---\ndescription: >-\n  d\n---\ngitGraph\n  commit id:\"1\"\n  commit id:\"2\"\n  commit id:\"3\"\n");
    ASSERT_TRUE(cohort.has_value());
    EXPECT_EQ(cohort->family, MermaidProbeFamily::GitGraph);
}

TEST(MermaidProbe, StateFrontmatterResolvesRelationsState) {
    auto state = classify(
        "---\ndescription: >-\n  d\n---\nstateDiagram-v2\n  [*] --> State1\n");
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->family, MermaidProbeFamily::State);
}

TEST(MermaidProbe, ChartsIsTheFinalFallback) {
    EXPECT_EQ(classify("pie\n\"a\" : 1\n")->family, MermaidProbeFamily::Charts);
    EXPECT_EQ(classify("xychart-beta\nbar [1]\n")->family, MermaidProbeFamily::Charts);
    EXPECT_EQ(classify("mindmap\n  a\n")->family, MermaidProbeFamily::Charts);
    EXPECT_EQ(classify("random garbage input\n")->family, MermaidProbeFamily::Charts);
}

TEST(MermaidProbe, MalformedEnvelopeStillClassifiesTextually) {
    // Unterminated frontmatter: textual scan tolerates it; charts fallback.
    auto unterminated = classify("---\nx: 1\npie\n\"a\" : 1\n");
    ASSERT_TRUE(unterminated.has_value());
    EXPECT_EQ(unterminated->family, MermaidProbeFamily::Charts);
    // A directive whose quoted data contains a fake close still routes
    // sequence textually (parse_envelope may reject; classification must not).
    auto quoted = classify(
        "%%{init: {\"x\":\"}%%\"} }%%\nsequenceDiagram\nA->>B: x\n");
    ASSERT_TRUE(quoted.has_value());
    EXPECT_EQ(quoted->family, MermaidProbeFamily::Sequence);
}

TEST(MermaidProbe, RouteBudgetFailureReturnsNoDecision) {
    auto exhausted = classify("---\ndescription: x\n---\nsequenceDiagram\nA->>B\n",
                              0);
    EXPECT_FALSE(exhausted.has_value());
}
