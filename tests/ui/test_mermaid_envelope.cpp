// RFC 0031 E1 structural envelope tests. This translation unit is a module
// implementation unit so it can exercise the private detail parser directly.
module;

#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

module loom.ui.visual.mermaid;

import std;
import loom.ui.visual.markdown;

namespace {

using loom::ui::mermaid::detail::Envelope;
using loom::ui::mermaid::detail::EnvelopeScalarForm;
using loom::ui::mermaid::detail::Work;
using loom::ui::mermaid::detail::parse_envelope;

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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
        "title: A\n"
        "title: B\n"
        "---\n"
        "classDiagram\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, DuplicateConfigPathRejected) {
    std::string source =
        "---\n"
        "config:\n"
        "  layout: dagre\n"
        "  layout: elk\n"
        "---\n"
        "flowchart TD\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, UnknownTopLevelIsAnEvent) {
    std::string source =
        "---\n"
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
    std::string source =
        "---\r\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "%%{init: {\"theme\": \"dark\"}}%% trailing\n"
        "flowchart TD\n";
    Work work;
    EXPECT_FALSE(parse(source, work));
}

TEST(MermaidEnvelope, SecondDirectiveIsCaptured) {
    std::string source =
        "%%{init: {\"theme\": \"dark\"}}%%\n"
        "%%{init: {\"theme\": \"forest\"}}%%\n"
        "flowchart TD\n";
    Work work;
    const auto envelope = parse(source, work);
    ASSERT_TRUE(envelope);
    EXPECT_EQ(envelope->init.size(), 2u);
    EXPECT_TRUE(envelope->effective_body.starts_with("flowchart TD"));
}

TEST(MermaidEnvelope, WorkBudgetExhaustionRejected) {
    std::string source =
        "---\n"
        "description: x\n"
        "---\n"
        "flowchart TD\n";
    Work work;
    work.remaining = 8u;
    EXPECT_FALSE(parse(source, work));
}

} // namespace

TEST(MermaidEnvelope, EmptyDescriptionHeaderCollectsBlock) {
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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

TEST(MermaidEnvelope, ErCommentFrontmatterRendersDirectly) {
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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
    std::string source =
        "---\n"
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

