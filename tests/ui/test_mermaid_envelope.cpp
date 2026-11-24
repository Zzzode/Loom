// RFC 0031 E1 structural envelope tests. This translation unit reaches the
// closed parser exclusively through the loom.ui.visual.mermaid.testing seam
// facade (RFC 0031 E6a); it never declares the production module.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.serdes.json;
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

// RFC 0034 Phase 1: the active-ER supplemental layout Work meter. These are
// pure meter tests (no rendering) pinning the split-routing and fail-closed
// semantics that bound the bounded envelope allowance.
namespace {
using loom::ui::mermaid::testing::kErEnvelopeLayoutAllowance;
using loom::ui::mermaid::testing::kWorkLimit;
using loom::ui::mermaid::testing::Work;
}  // namespace

TEST(MermaidEnvelopeWork, DefaultOffKeepsLegacySingleBalanceClamp) {
    Work work;  // inactive supplemental
    EXPECT_FALSE(work.envelope_enabled);
    EXPECT_EQ(work.available(), kWorkLimit);
    EXPECT_TRUE(work.spend(kWorkLimit));
    EXPECT_EQ(work.remaining, 0u);
    EXPECT_EQ(work.available(), 0u);
    // Legacy clamp-to-zero on refusal, no supplemental draw.
    EXPECT_FALSE(work.spend(1));
    EXPECT_EQ(work.remaining, 0u);
    EXPECT_EQ(work.supplemental_remaining, 0u);
}

TEST(MermaidEnvelopeWork, SplitRoutingDrainsPrimaryThenSupplemental) {
    Work work;
    work.enable_envelope(kErEnvelopeLayoutAllowance);
    // Spend the primary down to a small residual.
    ASSERT_TRUE(work.spend(kWorkLimit - 10u));
    EXPECT_EQ(work.remaining, 10u);
    EXPECT_EQ(work.available(), 10u + kErEnvelopeLayoutAllowance);
    // A fee larger than the residual splits: primary -> 0, only the remainder
    // comes from the supplemental pool.
    EXPECT_TRUE(work.spend(25u));
    EXPECT_EQ(work.remaining, 0u);
    EXPECT_EQ(work.supplemental_remaining, kErEnvelopeLayoutAllowance - 15u);
}

TEST(MermaidEnvelopeWork, CombinedCeilingExactAndOneUnitShortFailClosed) {
    const std::size_t combined = kWorkLimit + kErEnvelopeLayoutAllowance;
    Work exact;
    exact.enable_envelope(kErEnvelopeLayoutAllowance);
    EXPECT_TRUE(exact.spend(combined));  // exact combined ceiling
    EXPECT_EQ(exact.available(), 0u);
    EXPECT_FALSE(exact.spend(1));       // one more fails closed
    EXPECT_EQ(exact.remaining, 0u);
    EXPECT_EQ(exact.supplemental_remaining, 0u);

    Work short_meter;
    short_meter.enable_envelope(kErEnvelopeLayoutAllowance);
    // Full primary + full supplemental: requesting one beyond the combined
    // ceiling must refuse and drain both pools fail-closed.
    EXPECT_FALSE(short_meter.spend(combined + 1u));
    EXPECT_EQ(short_meter.available(), 0u);
}

TEST(MermaidEnvelopeWork, BoundaryPositionIndependentOfFeeDecomposition) {
    const std::size_t combined = kWorkLimit + kErEnvelopeLayoutAllowance;
    // Decomposition A: one large fee that crosses the primary boundary.
    Work a;
    a.enable_envelope(kErEnvelopeLayoutAllowance);
    ASSERT_TRUE(a.spend(kWorkLimit + 50u));
    EXPECT_EQ(a.remaining, 0u);
    EXPECT_EQ(a.supplemental_remaining, kErEnvelopeLayoutAllowance - 50u);
    EXPECT_TRUE(a.spend(kErEnvelopeLayoutAllowance - 50u));
    EXPECT_EQ(a.available(), 0u);

    // Decomposition B: the same total as many unit fees, each tiny.
    Work b;
    b.enable_envelope(kErEnvelopeLayoutAllowance);
    for (std::size_t i = 0; i < combined; ++i)
        ASSERT_TRUE(b.spend(1)) << "at unit " << i;
    EXPECT_EQ(b.available(), 0u);
    EXPECT_FALSE(b.spend(1));
}

// D-461: the native state projection draws a second bounded envelope derived
// from the actual post-prepare canvas. These pin the formula's two measured
// points and the split/fail-closed semantics; the corpus rows that actually
// need it are pinned by the state cohort gate.
TEST(MermaidEnvelopeWork, StateEnvelopeIsCanvasDerivedAndFailsClosed) {
    using loom::ui::mermaid::testing::state_envelope_layout_allowance;
    EXPECT_EQ(state_envelope_layout_allowance(64u, 134u), 1'650'688u);
    EXPECT_EQ(state_envelope_layout_allowance(99u, 267u), 5'079'232u);
    const auto allowance = state_envelope_layout_allowance(64u, 134u);
    Work work;
    EXPECT_FALSE(work.envelope_enabled);
    work.enable_envelope(allowance);
    EXPECT_TRUE(work.envelope_enabled);
    EXPECT_EQ(work.available(), kWorkLimit + allowance);
    // Primary-first split routing.
    ASSERT_TRUE(work.spend(kWorkLimit - 10u));
    EXPECT_EQ(work.remaining, 10u);
    EXPECT_EQ(work.available(), 10u + allowance);
    EXPECT_TRUE(work.spend(25u));
    EXPECT_EQ(work.remaining, 0u);
    EXPECT_EQ(work.supplemental_remaining, allowance - 15u);
    // Exact combined ceiling, then fail closed with both pools drained.
    EXPECT_TRUE(work.spend(allowance - 15u));
    EXPECT_EQ(work.available(), 0u);
    EXPECT_FALSE(work.spend(1u));
    EXPECT_EQ(work.remaining, 0u);
    EXPECT_EQ(work.supplemental_remaining, 0u);
}

// D-461: non-state families keep the historical single-balance clamp: the
// envelope must stay inactive for a class source even though the shared
// relations layout runs.
TEST(MermaidEnvelopeWork, NonStateFamiliesNeverEnableTheEnvelope) {
    namespace mtest = loom::ui::mermaid::testing;
    Work work;
    EXPECT_TRUE(mtest::layout_succeeds("classDiagram\nA --> B\n", false, work));
    EXPECT_FALSE(work.envelope_enabled);
    EXPECT_EQ(work.supplemental_remaining, 0u);
}

// RFC 0034 Phase 1 (design §3.2): sweep every ER fixture whose frontmatter
// reaches an active projection (the 97 look-envelope originals) with a fresh
// DEFAULT production Work (primary kWorkLimit; the supplemental stays inactive
// until relations_layout auto-enables it), and prove ordinal 198 is the UNIQUE
// row whose cost exceeds the bare 1M primary. We do not suppress the
// supplemental pool to re-probe the old single-pool threshold; we observe the
// two-pool accounting directly. The supplemental is debited only inside the
// spend() branch where a fee cannot fit the primary balance, so a non-full
// supplemental after a successful draw is an exact witness that the primary
// reached zero (bare minimum > kWorkLimit), while a byte-identical full
// allowance proves the 1M pool alone covered the row — runtime conservation
// evidence requiring no test-only production switch.
TEST(MermaidEnvelopeWork, SupplementalAllowanceIsDrawnByOnlyOrdinal198) {
    namespace mtest = loom::ui::mermaid::testing;
    const auto root = std::filesystem::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
    std::ifstream manifest_in(root / "manifest.json", std::ios::binary);
    ASSERT_TRUE(manifest_in);
    const std::string manifest_bytes((std::istreambuf_iterator<char>(manifest_in)),
                                     std::istreambuf_iterator<char>());
    auto manifest_doc = loom::utils::json::parse(manifest_bytes);
    ASSERT_TRUE(manifest_doc.has_value());
    const auto fixtures = manifest_doc->root().get("fixtures");

    const std::string sole_crosser_path =
        "er-diagram/visual-check/"
        "nested-er-subgraphs-with-direction-override-handdrawn-look.mmd";

    std::size_t scanned = 0, drawable = 0, construct_rejected = 0, over_cap = 0;
    std::size_t crosser_allowance_used = 0;
    std::string sole_crosser;
    for (std::size_t i = 0; i < fixtures.size(); ++i) {
        const auto record = fixtures.at(i);
        const auto rel = record.get_string("path");
        if (!rel.starts_with("er-diagram/") ||
            (rel.find("handdrawn-look") == std::string_view::npos &&
             rel.find("-neo") == std::string_view::npos))
            continue;
        ++scanned;
        std::ifstream in(root / "cases" / rel, std::ios::binary);
        ASSERT_TRUE(in) << rel;
        const std::string source((std::istreambuf_iterator<char>(in)),
                                 std::istreambuf_iterator<char>());

        Work work;  // default production meter; supplemental activates on layout
        const auto rows = mtest::class_relations_rows(source, work, false);
        if (!rows.has_value()) {
            // A construct/authority gate (classDef/style/unicode presentation
            // contracts) refuses independent of budget; the allowance cannot
            // change this outcome, so it is not counted as a budget result.
            ++construct_rejected;
            continue;
        }
        ++drawable;

        if (work.envelope_enabled &&
            work.supplemental_remaining < kErEnvelopeLayoutAllowance) {
            // The primary 1M was exhausted and the allowance funded the overshoot.
            ++over_cap;
            sole_crosser = rel;
            crosser_allowance_used =
                kErEnvelopeLayoutAllowance - work.supplemental_remaining;
            EXPECT_EQ(work.remaining, 0u) << rel;
        } else {
            // Isolation: every other admitted row leaves the supplemental pool
            // byte-identical to its post-enable value (the full allowance when
            // enabled, else the inactive zero), proving the 1M primary alone
            // paid its whole cost.
            EXPECT_EQ(work.supplemental_remaining,
                      work.envelope_enabled ? kErEnvelopeLayoutAllowance : 0u)
                << rel;
        }
    }

    // 97 look-envelope ER fixtures = 92 drawable (91 within the bare 1M cap plus
    // the now-admitted ordinal 198) + 5 construct-gated fallbacks. Before the
    // allowance this split was 86/11; exactly one budget-gated row moved. D-460
    // then admitted five `entities-with-styles-applied-*-handdrawn-look` rows
    // through the closed ER style surface, moving them off the construct gate:
    // 87/10 -> 92/5 (the unicode-script handdrawn-look rows stay gated).
    EXPECT_EQ(scanned, 97u);
    EXPECT_EQ(construct_rejected, 5u);
    EXPECT_EQ(drawable, 92u);
    EXPECT_EQ(over_cap, 1u)
        << "exactly one drawable ER look-envelope fixture exceeds the bare 1M primary";
    EXPECT_EQ(sole_crosser, sole_crosser_path);
    // The single overshoot fits well inside the bounded allowance: the fail-closed
    // ceiling is kWorkLimit + kErEnvelopeLayoutAllowance, never unbounded. The
    // exact draw is pinned as a deterministic budget lock: ordinal 198 exhausts
    // the bare 1M primary and draws 289,582 from the supplemental (a total layout
    // cost of 1,289,582), under 1% of the 29,986,816 allowance.
    EXPECT_GT(crosser_allowance_used, 0u);
    EXPECT_LT(crosser_allowance_used, kErEnvelopeLayoutAllowance);
    EXPECT_EQ(crosser_allowance_used, 289'582u);
}

namespace {
// Load the ordinal-198 handDrawn whole source: the unique row whose bare layout
// cost (1,289,582) crosses the 1M production primary.
std::string rfc0034_handdrawn_source() {
    const auto path =
        std::filesystem::path(LOOM_TESTS_DIR) /
        "fixtures/mermaid_official/cases/er-diagram/visual-check/"
        "nested-er-subgraphs-with-direction-override-handdrawn-look.mmd";
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Finish an Element into a fixed terminal grid for byte-for-byte comparison.
std::string rfc0034_grid(ftxui::Element element) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(240),
                                        ftxui::Dimension::Fixed(80));
    ftxui::Render(screen, std::move(element));
    return screen.ToString();
}
}  // namespace

// RFC 0034 Phase 1 (design §2.3, finding N-1): the supplemental pool is pure
// budget, never a routing input. The admitted row is rendered with the same
// total ceiling but the primary/supplemental exhaustion boundary placed at
// different fee positions; the emitted wide grid must be byte-identical whether
// the primary alone pays or the allowance funds a large overshoot.
TEST(MermaidEnvelopeWork, GeometryInvariantAcrossPrimaryExhaustionBoundary) {
    namespace mtest = loom::ui::mermaid::testing;
    const std::string source = rfc0034_handdrawn_source();
    ASSERT_FALSE(source.empty());

    const auto layout = [&](std::size_t primary) {
        Work work;
        work.remaining = primary;  // supplemental auto-enabled at layout entry
        auto rows = mtest::class_relations_rows(source, work, false);
        return std::pair{std::move(rows), std::move(work)};
    };

    // Production split: the 1M primary exhausts and the allowance funds 289,582.
    auto production = layout(kWorkLimit);
    ASSERT_TRUE(production.first.has_value());
    ASSERT_FALSE(production.first->empty());
    EXPECT_EQ(production.second.remaining, 0u);
    EXPECT_LT(production.second.supplemental_remaining, kErEnvelopeLayoutAllowance);

    // Boundary moved earlier: a smaller primary still clears the pre-enable
    // parse costs but forces a strictly larger supplemental draw, crossing the
    // pool boundary at different fee positions. Geometry must not move.
    for (const std::size_t primary : {700'000u, 900'000u}) {
        auto moved = layout(primary);
        ASSERT_TRUE(moved.first.has_value()) << "primary=" << primary;
        EXPECT_EQ(moved.second.remaining, 0u) << "primary=" << primary;
        EXPECT_LT(moved.second.supplemental_remaining, kErEnvelopeLayoutAllowance)
            << "primary=" << primary;
        EXPECT_EQ(*moved.first, *production.first) << "primary=" << primary;
    }

    // Boundary never crossed: a primary that alone covers the 1,289,582 total
    // leaves the supplemental byte-identical to its full post-enable value, yet
    // emits the same grid — proof the meter only gates, never routes.
    for (const std::size_t primary : {1'300'000u, 2'000'000u}) {
        auto solo = layout(primary);
        ASSERT_TRUE(solo.first.has_value()) << "primary=" << primary;
        EXPECT_EQ(solo.second.supplemental_remaining, kErEnvelopeLayoutAllowance)
            << "primary=" << primary;
        EXPECT_EQ(*solo.first, *production.first) << "primary=" << primary;
    }
}

// RFC 0034 Phase 1 (design §3.2.1, finding N-4): the production LayoutRowsCache
// reproduces the admitted row identically on a miss and a hit. On the miss the
// two-pool meter lays out: the primary saturates at 1,000,000 and the 289,582
// overshoot is supplemental, invisible to Work.remaining (so measured layout
// work means "primary Work consumed", not total). On the hit the cache
// re-debits that cached primary-only amount against a fresh flag-off meter and
// finishes the retained rows; the rendered terminal grid is byte-identical.
TEST(MermaidEnvelopeWork, LayoutCacheMissHitParityForAdmittedRow) {
    namespace mtest = loom::ui::mermaid::testing;
    const std::string source = rfc0034_handdrawn_source();
    ASSERT_FALSE(source.empty());
    auto fallback = ftxui::text("RFC0034_FALLBACK");

    loom::ui::mermaid::LayoutRowsCache cache(8, 4 * 1024 * 1024);

    mtest::Work miss_work;
    mtest::FinalEmissionTrace miss_trace;
    auto miss_element =
        mtest::CacheBudgetAccess::render(cache, source, false, fallback, miss_work, &miss_trace);
    ASSERT_TRUE(miss_element.has_value());
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 0u);
    // Primary-only observation: the primary 1M is fully consumed even though the
    // supplemental separately funded the overshoot; remaining cannot see it.
    EXPECT_EQ(miss_work.remaining, 0u);

    mtest::Work hit_work;
    mtest::FinalEmissionTrace hit_trace;
    auto hit_element =
        mtest::CacheBudgetAccess::render(cache, source, false, fallback, hit_work, &hit_trace);
    ASSERT_TRUE(hit_element.has_value());
    EXPECT_EQ(cache.misses(), 1u);
    EXPECT_EQ(cache.hits(), 1u);
    // The fresh flag-off hit meter re-debits exactly the cached primary-only
    // measured layout work and likewise lands at a zero primary balance.
    EXPECT_EQ(hit_work.remaining, 0u);

    // Full-finish rendered grid is byte-identical across the cache boundary.
    EXPECT_EQ(rfc0034_grid(*hit_element), rfc0034_grid(*miss_element));

    // The retained layout rows also match an independent, uncached layout.
    auto cached_rows = cache.get_or_layout(source, false);
    ASSERT_TRUE(cached_rows.has_value());
    mtest::Work direct_work;
    auto direct_rows = mtest::class_relations_rows(source, direct_work, false);
    ASSERT_TRUE(direct_rows.has_value());
    EXPECT_EQ(*cached_rows, *direct_rows);
}
