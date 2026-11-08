// RFC 0031 E1 structural envelope tests. This translation unit reaches the
// closed parser exclusively through the loom.ui.visual.mermaid.testing seam
// facade (RFC 0031 E6a); it never declares the production module.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;

namespace {

using loom::ui::mermaid::testing::Envelope;
using loom::ui::mermaid::testing::EnvelopeScalarForm;
using loom::ui::mermaid::testing::parse_envelope;
using loom::ui::mermaid::testing::Work;

std::optional<Envelope> parse(std::string_view source, Work& work) {
    return parse_envelope(source, work);
}

bool has_config(const Envelope& envelope, std::string_view path, std::string_view value) {
    return std::ranges::any_of(envelope.config, [&](const auto& entry) {
        return entry.path == path && entry.value == value;
    });
}

TEST(MermaidEnvelope, NoFrontmatterKeepsWholeSource) {
    std::string source = "flowchart TD\nA-->B\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_FALSE(envelope->has_frontmatter);
    EXPECT_EQ(envelope->raw_body, source);
    EXPECT_EQ(envelope->effective_body, source);
    EXPECT_TRUE(envelope->config.empty());
    EXPECT_TRUE(envelope->init.empty());
}

TEST(MermaidEnvelope, DescriptionAndNestedConfig) {
    std::string source = "---\n"
                         "description: hello\n"
                         "config:\n"
                         "  layout: dagre\n"
                         "  flowchart:\n"
                         "    htmlLabels: true\n"
                         "---\n"
                         "flowchart TD\n"
                         "A-->B\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_TRUE(envelope->has_frontmatter);
    ASSERT_TRUE(envelope->description);
    EXPECT_EQ(envelope->description->text, "hello");
    EXPECT_EQ(envelope->description->form, EnvelopeScalarForm::Plain);
    EXPECT_TRUE(has_config(*envelope, "layout", "dagre"));
    EXPECT_TRUE(has_config(*envelope, "flowchart.htmlLabels", "true"));
    EXPECT_EQ(envelope->raw_body, "flowchart TD\nA-->B\n");
    EXPECT_EQ(envelope->events.front().path, "description");
    EXPECT_TRUE(envelope->events.front().top_level);
}

TEST(MermaidEnvelope, FoldedDescriptionKeepsRawLines) {
    std::string source = "---\n"
                         "description: >-\n"
                         "  line one\n"
                         "  line two\n"
                         "\n"
                         "  line three\n"
                         "---\n"
                         "flowchart TD\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_TRUE(envelope->description);
    EXPECT_EQ(envelope->description->form, EnvelopeScalarForm::Folded);
    EXPECT_NE(envelope->description->text.find("line one"), std::string::npos);
    EXPECT_NE(envelope->description->text.find("line three"), std::string::npos);
    EXPECT_EQ(envelope->raw_body, "flowchart TD\n");
}

TEST(MermaidEnvelope, LiteralConfigBlock) {
    std::string source = "---\n"
                         "config:\n"
                         "  themeCSS: |\n"
                         "    .a { color: red; }\n"
                         "    .b { color: blue; }\n"
                         "---\n"
                         "erDiagram\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_EQ(envelope->config.size(), 1u);
    EXPECT_EQ(envelope->config.front().path, "themeCSS");
    EXPECT_EQ(envelope->config.front().form, EnvelopeScalarForm::Literal);
    EXPECT_NE(envelope->config.front().value.find(".a"), std::string::npos);
    EXPECT_NE(envelope->config.front().value.find(".b"), std::string::npos);
}

TEST(MermaidEnvelope, QuotedTitle) {
    std::string source = "---\n"
                         "title: \"Hello: world\"\n"
                         "---\n"
                         "classDiagram\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_TRUE(envelope->title);
    EXPECT_EQ(envelope->title->form, EnvelopeScalarForm::Quoted);
    EXPECT_EQ(envelope->title->text, "\"Hello: world\"");
}

TEST(MermaidEnvelope, DuplicateTopLevelRejected) {
    std::string source = "---\n"
                         "title: A\n"
                         "title: B\n"
                         "---\n"
                         "classDiagram\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, DuplicateConfigPathRejected) {
    std::string source = "---\n"
                         "config:\n"
                         "  layout: dagre\n"
                         "  layout: elk\n"
                         "---\n"
                         "flowchart TD\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, UnknownTopLevelIsAnEvent) {
    std::string source = "---\n"
                         "description: x\n"
                         "coveredShapes: 3\n"
                         "---\n"
                         "flowchart TD\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    const bool found = std::ranges::any_of(envelope->events, [](const auto& event) {
        return event.top_level && event.path == "coveredShapes" && event.value == "3";
    });
    EXPECT_TRUE(found);
}

TEST(MermaidEnvelope, MissingClosingFenceRejected) {
    std::string source = "---\ndescription: x\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, CrLfBodyOffset) {
    std::string source = "---\r\n"
                         "description: x\r\n"
                         "---\r\n"
                         "flowchart TD\r\n"
                         "A-->B\r\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_TRUE(envelope->raw_body.starts_with("flowchart TD"));
    ASSERT_TRUE(envelope->description);
    EXPECT_EQ(envelope->description->text, "x");
}

TEST(MermaidEnvelope, LeadingInitIsExposedAndSkipped) {
    std::string source = "---\n"
                         "description: x\n"
                         "---\n"
                         "%%{init: {\"theme\":\"dark\"}}%%\n"
                         "flowchart TD\n"
                         "A-->B\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_EQ(envelope->init.size(), 1u);
    EXPECT_NE(envelope->init.front().raw.find("%%{init:"), std::string::npos);
    EXPECT_NE(envelope->init.front().json.find("\"theme\""), std::string::npos);
    EXPECT_TRUE(envelope->effective_body.starts_with("flowchart TD"));
    EXPECT_TRUE(envelope->raw_body.starts_with("%%{init:"));
}

TEST(MermaidEnvelope, ConfigDepthLimit) {
    std::string source = "---\n"
                         "config:\n"
                         "  a:\n"
                         "    b:\n"
                         "      c:\n"
                         "        d:\n"
                         "          e: 1\n"
                         "---\n"
                         "flowchart TD\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, DirectiveWithTrailingTextRejected) {
    std::string source = "%%{init: {\"theme\": \"dark\"}}%% trailing\n"
                         "flowchart TD\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, SecondDirectiveIsCaptured) {
    std::string source = "%%{init: {\"theme\": \"dark\"}}%%\n"
                         "%%{init: {\"theme\": \"forest\"}}%%\n"
                         "flowchart TD\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_EQ(envelope->init.size(), 2u);
    EXPECT_TRUE(envelope->effective_body.starts_with("flowchart TD"));
}

TEST(MermaidEnvelope, WorkBudgetExhaustionRejected) {
    std::string source = "---\n"
                         "description: x\n"
                         "---\n"
                         "flowchart TD\n";
    Work work;
    work.remaining = 8u;
    EXPECT_FALSE(parse(source, work));
}

// RFC 0031 E5: the envelope charges in two phases. A balance that clears the
// entry fee but not the frontmatter fee can only parse a no-frontmatter
// envelope; a frontmatter source rejects after its closing fence is found.
TEST(MermaidEnvelope, WorkBudgetSplitPhase) {
    const std::string source = "---\n"
                               "description: x\n"
                               "---\n"
                               "flowchart TD\n";
    // Balance above the entry fee but below the frontmatter fee: the
    // frontmatter source rejects (fee fires only on the has_frontmatter path).
    {
        Work work;
        work.remaining = 3072u + 3u * source.size() + 1u;
        EXPECT_FALSE(parse(source, work));
    }
    // The same balance parses a no-frontmatter envelope, proving the
    // frontmatter term is never charged without a closing fence.
    {
        const std::string plain = "pie title T\n\"a\" : 1\n";
        Work work;
        work.remaining = 3072u + 3u * source.size() + 1u;
        const auto envelope = parse(plain, work);
        ASSERT_TRUE(envelope);
        EXPECT_FALSE(envelope->has_frontmatter);
    }
    // A fresh full budget parses the frontmatter source through both phases.
    {
        Work work;
        EXPECT_TRUE(parse(source, work));
    }
}

// RFC 0031 E5: one cumulative Work covers parse and layout; it is never reset
// between phases. Probe the smallest balance that lets a tiny bare flow run
// parse and layout back to back, and pin the boundary.
TEST(MermaidEnvelope, WorkBudgetSharedAcrossPhases) {
    const std::string source = "flowchart LR\nA[A] -->|go| B[B]\n";
    std::size_t threshold = 0;
    for (std::size_t remaining = 1u; remaining < 5000000u; ++remaining) {
        Work work;
        work.remaining = remaining;
        if (loom::ui::mermaid::testing::flow_ascii_rows(source, work)) {
            threshold = remaining;
            break;
        }
    }
    ASSERT_GT(threshold, 0u);
    {
        Work work;
        work.remaining = threshold - 1u;
        EXPECT_FALSE(loom::ui::mermaid::testing::flow_ascii_rows(source, work));
    }
    {
        Work work;
        work.remaining = threshold;
        EXPECT_TRUE(loom::ui::mermaid::testing::flow_ascii_rows(source, work));
    }
    // A phase reset would let parse-only balances lay out; assert the
    // threshold is well above the parse-only share by confirming a balance a
    // little above the parse entry fee still fails.
    EXPECT_GT(threshold, 4096u);
}

// The layout seam consumes its caller's Work and does not include dispatcher
// overhead; the source-bound public geometry reports test the full path.
TEST(MermaidEnvelope, ClassRelationLayoutsRespectOneWorkBudget) {
    const std::array<std::string_view, 2> sources{{
        R"CLASS26(classDiagram
  Class01 "1" <|--|> "*" AveryLongClass : Cool
  <<interface>> Class01
  Class03 "1" *-- "*" Class04
  Class05 "1" o-- "many" Class06
  Class07 "1" .. "*" Class08
  Class09 "1" --> "*" C2 : Where am i?
  Class09 "*" --* "*" C3
  Class09 "1" --|> "1" Class07
  Class07  : equals()
  Class07  : Object[] elementData
  Class01  : size()
  Class01  : int chimp
  Class01  : int gorilla
  Class08 "1" <--> "*" C2: Cool label
  class Class10 {
  <<service>>
  int id
  test()
  }
)CLASS26",
        R"CLASS26(classDiagram
  note "I love this diagram!\nDo you love it?"
  Class01 <|-- AveryLongClass : Cool
  <<interface>> Class01
  Class03 "1" *-- "*" Class04
  Class05 "1" o-- "many" Class06
  Class07 "1" .. "*" Class08
  Class09 "1" --> "*" C2 : Where am i?
  Class09 "*" --* "*" C3
  Class09 "1" --|> "1" Class07
  Class12 <|.. Class08
  Class11 ..>Class12
  Class07 : equals()
  Class07 : Object[] elementData
  Class01 : size()
  Class01 : int chimp
  Class01 : int gorilla
  Class01 : -int privateChimp
  Class01 : +int publicGorilla
  Class01 : #int protectedMarmoset
  Class08 <--> C2: Cool label
  class Class10 {
    <<service>>
    int id
    test()
  }
  note for Class10 "Cool class\nI said it's very cool class!"
)CLASS26"}};
    for (const auto source : sources) {
        const auto rows_at = [&](std::size_t budget) {
            Work work;
            work.remaining = budget;
            auto rows = loom::ui::mermaid::testing::class_relations_rows(source, work, false);
            return std::pair{std::move(rows), work.remaining};
        };
        const auto full = rows_at(loom::ui::mermaid::testing::kWorkLimit);
        ASSERT_TRUE(full.first.has_value()) << "source=" << source << " remaining=" << full.second;
        ASSERT_FALSE(full.first->empty());
        EXPECT_FALSE(rows_at(0).first.has_value());
        std::size_t low = 0, high = loom::ui::mermaid::testing::kWorkLimit;
        while (low + 1 < high) {
            const auto middle = low + (high - low) / 2;
            if (rows_at(middle).first)
                high = middle;
            else
                low = middle;
        }
        ASSERT_GT(high, 0u);
        EXPECT_FALSE(rows_at(high - 1).first.has_value());
        const auto at = rows_at(high);
        ASSERT_TRUE(at.first.has_value());
        EXPECT_EQ(*at.first, *full.first);
        EXPECT_EQ(rows_at(high).first, at.first);
        EXPECT_LT(at.second, high);
        Work shared;
        shared.remaining = high;
        ASSERT_TRUE(loom::ui::mermaid::testing::class_relations_rows(source, shared, false));
        EXPECT_LT(shared.remaining, high);
        EXPECT_FALSE(loom::ui::mermaid::testing::class_relations_rows(source, shared, false));
    }
}

TEST(MermaidEnvelope, ClassNoteCandidateOrderPreservesStableInputIndices) {
    const std::vector<std::vector<int>> inputs{
        {}, {3}, {7, 7, 7}, {4, 1, 4, 0, 1, 4, 0}, {4064, 0, 4064, 0}};
    for (const auto& scores : inputs) {
        std::vector<std::size_t> expected(scores.size());
        std::iota(expected.begin(), expected.end(), 0u);
        std::stable_sort(expected.begin(), expected.end(),
                         [&](auto a, auto b) { return scores[a] < scores[b]; });
        Work work;
        const auto actual = loom::ui::mermaid::testing::class_note_candidate_order(scores, work);
        ASSERT_TRUE(actual.has_value());
        EXPECT_EQ(*actual, expected);
        Work zero;
        zero.remaining = 0;
        EXPECT_FALSE(loom::ui::mermaid::testing::class_note_candidate_order(scores, zero));
    }
    for (const auto count : {65535u, 65536u}) {
        std::vector<int> scores(count, 0);
        Work work;
        EXPECT_FALSE(loom::ui::mermaid::testing::class_note_candidate_order(scores, work));
    }
    for (const auto score : {-1, 4065}) {
        Work work;
        EXPECT_FALSE(
            loom::ui::mermaid::testing::class_note_candidate_order(std::array{score}, work));
    }
}

}  // namespace

TEST(MermaidEnvelope, EmptyDescriptionHeaderCollectsBlock) {
    std::string source = "---\n"
                         "description:\n"
                         "  Flow.\n"
                         "---\n"
                         "flowchart LR\n"
                         "A --> B\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_TRUE(envelope->description);
    EXPECT_EQ(envelope->description->header, "");
    EXPECT_EQ(envelope->description->form, EnvelopeScalarForm::Plain);
    EXPECT_EQ(envelope->description->text, "  Flow.\n");
    EXPECT_EQ(envelope->raw_body, "flowchart LR\nA --> B\n");
}

TEST(MermaidEnvelope, FlowEnvelopeRendersDirectly) {
    std::string source = "---\n"
                         "description:\n"
                         "  Flow.\n"
                         "---\n"
                         "flowchart LR\n"
                         "A --> B\n";
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fallback"));
    ASSERT_TRUE(element);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, *element);
    const auto text = screen.ToString();
    EXPECT_EQ(text.find("flowchart LR"), std::string::npos) << text;
}

TEST(MermaidEnvelope, FlowEnvelopeThroughMarkdown) {
    std::string source = "---\n"
                         "description:\n"
                         "  Flow.\n"
                         "---\n"
                         "flowchart LR\n"
                         "A --> B\n";
    const auto markdown = "```mermaid\n" + source + "\n```\n\nAFTER_FLOW";
    auto element = loom::ui::render_markdown(markdown);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, element);
    const auto text = screen.ToString();
    EXPECT_EQ(text.find("flowchart LR"), std::string::npos) << text;
}

TEST(MermaidEnvelope, DagreBareRenders) {
    std::string source = "---\n"
                         "description:\n"
                         "  Flow.\n"
                         "config:\n"
                         "  layout: dagre\n"
                         "---\n"
                         "flowchart LR\n"
                         "A --> B\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_TRUE(has_config(*envelope, "layout", "dagre"));
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fallback"));
    ASSERT_TRUE(element);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, *element);
    const auto text = screen.ToString();
    EXPECT_EQ(text.find("flowchart LR"), std::string::npos) << text;
}

// RFC 0031 FS-spacing: nodeSpacing/rankSpacing only position dagre ranks;
// the terminal uses its own fixed layout, so they are non-terminal envelope
// leaves and must not hold the whole source on ordinary fallback.
namespace {
bool renders_natively(const std::string& source) {
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("FALLBACK_MARKER"));
    if (!element) return false;
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240),
                                        ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, *element);
    return screen.ToString().find("FALLBACK_MARKER") == std::string::npos;
}
}  // namespace

TEST(MermaidEnvelope, NodeAndRankSpacingScalarsRenderNatively) {
    // Bare non-negative integer.
    EXPECT_TRUE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: 100\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Quoted integer (the pinned rankSpacing spelling).
    EXPECT_TRUE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    rankSpacing: '100'\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Both keys together, including the small value 1 and 0.
    EXPECT_TRUE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: 1\n    rankSpacing: 1\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    EXPECT_TRUE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: 0\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Combined with the already-whitelisted courier font leaf.
    EXPECT_TRUE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  fontFamily: courier\n  flowchart:\n    nodeSpacing: 50\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Combined with a diagram title.
    EXPECT_TRUE(renders_natively(
        "---\n"
        "title: Spacing example\n"
        "config:\n  flowchart:\n    nodeSpacing: 250\n    rankSpacing: 250\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
}

TEST(MermaidEnvelope, InvalidSpacingValuesKeepWholeSource) {
    // Negative spacing is not a valid pixel value.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: -5\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Fractional pixel spacing.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    rankSpacing: 1.5\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Non-numeric string.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: wide\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Structured value.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: [10, 20]\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Exponential notation is not an integer pixel value.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: 1e3\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Mismatched and single-sided quotes leave a quote byte in the scalar.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: '100\"\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: \"100\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // Empty quoted value.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: ''\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // A spacing key at indent 2 is an unknown top-level leaf.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  nodeSpacing: 100\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // A handDrawn look is an independent hard gate even with spacing.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  look: handDrawn\n  flowchart:\n    nodeSpacing: 100\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
    // An unknown sibling indent-4 key keeps the whole source.
    EXPECT_FALSE(renders_natively(
        "---\n"
        "description:\n  Flow.\n"
        "config:\n  flowchart:\n    nodeSpacing: 100\n    minNodeWidth: 0\n"
        "---\n"
        "flowchart TD\nA --> B\n"));
}

TEST(MermaidEnvelope, ErCommentFrontmatterRendersDirectly) {
    std::string source = "---\n"
                         "# root comment\n"
                         "config: # config comment\n"
                         "  # leaf comment\n"
                         "  logLevel: 1.0 # debug\n"
                         "---\n"
                         "erDiagram\n"
                         "A ||--o{ B : linked\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_FALSE(envelope->title);
    // The shared reader keeps the raw scalar; each family strips comments.
    EXPECT_TRUE(has_config(*envelope, "logLevel", "1.0 # debug"));
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fallback"));
    ASSERT_TRUE(element);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, *element);
    const auto text = screen.ToString();
    EXPECT_EQ(text.find("erDiagram"), std::string::npos) << text;
}

TEST(MermaidEnvelope, ErTitleRendersDirectly) {
    std::string source = "---\n"
                         "title: Visible authored title\n"
                         "config:\n"
                         "  logLevel: 1\n"
                         "---\n"
                         "erDiagram\n"
                         "A ||--o{ B : linked\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_TRUE(envelope->title);
    EXPECT_EQ(envelope->title->text, "Visible authored title");
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fallback"));
    ASSERT_TRUE(element);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(64));
    ftxui::Render(screen, *element);
    const auto text = screen.ToString();
    EXPECT_NE(text.find("Visible authored title"), std::string::npos) << text;
}

TEST(MermaidEnvelope, ErThemeCssBlockRendersDirectly) {
    std::string source = "---\n"
                         "config:\n"
                         "  themeCSS: |\n"
                         "    rect { fill: red; }\n"
                         "---\n"
                         "erDiagram\n"
                         "  CUSTOMER ||--o{ ORDER : places\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    ASSERT_EQ(envelope->config.size(), 1u);
    EXPECT_EQ(envelope->config.front().value, "rect { fill: red; }\n");
    auto element = loom::ui::mermaid::render_mermaid(source, ftxui::text("fallback"));
    ASSERT_TRUE(element);
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, *element);
    const auto text = screen.ToString();
    EXPECT_EQ(text.find("erDiagram"), std::string::npos) << text;
}
