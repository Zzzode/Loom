// RFC 0031 FS-init: whitelist the inert `logLevel` key inside flowchart
// body `%%{init ...}%%` directives. Mermaid's legacy grammar spells the
// JSON5-ish payload with single quotes, which the strict JSON parser
// rejects; the directive previously accepted only the double-quoted
// {"flowchart":{"htmlLabels":bool}} shape, so the four legacy-graph
// fixtures fell back on the directive alone. logLevel only selects logger
// verbosity and has zero rendering semantics, so both quote styles now
// decode through one strict whitelist with a disclosure footer; every
// other key or malformed spelling keeps failing closed.
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

constexpr std::string_view kGraph = "flowchart TD\n A --> B\n";
}  // namespace

TEST(MermaidInitDirective, LogLevelDrawsBothQuoteStyles) {
    // The pinned legacy spelling: single quotes with interior whitespace.
    EXPECT_TRUE(drawn_native("%%{init: { 'logLevel': 0} }%%\n" + std::string(kGraph)));
    // Compact single-quoted form.
    EXPECT_TRUE(drawn_native("%%{init: {'logLevel':0}}%%\n" + std::string(kGraph)));
    // Strict double-quoted JSON keeps working.
    EXPECT_TRUE(drawn_native("%%{init: {\"logLevel\": 0} }%%\n" + std::string(kGraph)));
}

TEST(MermaidInitDirective, LogLevelValueGrammar) {
    for (int level = 0; level <= 5; ++level)
        EXPECT_TRUE(drawn_native("%%{init: { 'logLevel': " + std::to_string(level) +
                                 "} }%%\n" + std::string(kGraph)))
            << level;
    for (const char* name : {"trace", "debug", "info", "warn", "error", "fatal"})
        EXPECT_TRUE(drawn_native(std::string("%%{init: { 'logLevel': '") + name +
                                 "'} }%%\n" + std::string(kGraph)))
            << name;
    // Out-of-range numbers, fractional values and unknown names fail closed.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 6} }%%\n" + std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': -1} }%%\n" + std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 1.5} }%%\n" + std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 'verbose'} }%%\n" +
                              std::string(kGraph)));
    // Names are exact lowercase matches: gluing and case changes fail closed.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 'tracex'} }%%\n" +
                              std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 'INFO'} }%%\n" +
                              std::string(kGraph)));
    // Structured values are not scalars.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': [0]} }%%\n" + std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': {'x':0}} }%%\n" +
                              std::string(kGraph)));
}

TEST(MermaidInitDirective, DisclosureNoticeRendered) {
    const auto grid = surface_text("%%{init: { 'logLevel': 0} }%%\n" + std::string(kGraph));
    EXPECT_NE(grid.find("config.logLevel=0 -> logger only; not applied"),
              std::string::npos);
    const auto named =
        surface_text("%%{init: { 'logLevel': 'info'} }%%\n" + std::string(kGraph));
    EXPECT_NE(named.find("config.logLevel=info -> logger only; not applied"),
              std::string::npos);
    // The raw directive never reaches the canvas.
    EXPECT_EQ(grid.find("%%{"), std::string::npos);
}

TEST(MermaidInitDirective, HtmlLabelsWhitelistUnaffected) {
    // The pre-existing double-quoted htmlLabels shape stays accepted.
    EXPECT_TRUE(drawn_native("%%{init: {\"flowchart\": {\"htmlLabels\": false}} }%%\n" +
                             std::string(kGraph)));
    // Quote normalization admits the same whitelisted shape in single quotes.
    EXPECT_TRUE(drawn_native("%%{init: { 'flowchart': { 'htmlLabels': false }} }%%\n" +
                             std::string(kGraph)));
    const auto grid =
        surface_text("%%{init: {\"flowchart\": {\"htmlLabels\": false}} }%%\n" +
                     std::string(kGraph));
    EXPECT_NE(grid.find("config.flowchart.htmlLabels=false -> native text labels; "
                        "not applied"),
              std::string::npos);
}

TEST(MermaidInitDirective, UnknownKeysAndMalformedFailClosed) {
    // A theme key is not part of this whitelist.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 0, 'theme': 'dark'} }%%\n" +
                              std::string(kGraph)));
    EXPECT_FALSE(drawn_native("%%{init: { 'theme': 'dark'} }%%\n" + std::string(kGraph)));
    // Two top-level keys never parse, even when both are whitelisted names.
    EXPECT_FALSE(drawn_native(
        "%%{init: { 'logLevel': 0, 'flowchart': {'htmlLabels': false}} }%%\n" +
        std::string(kGraph)));
    // Unterminated single quote.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 0 }%%\n" + std::string(kGraph)));
    // Backslash escapes do not exist inside single-quoted directive strings.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 'inf\\o'} }%%\n" +
                              std::string(kGraph)));
    // Trailing junk after the closing wrapper.
    EXPECT_FALSE(drawn_native("%%{init: { 'logLevel': 0} }%% junk\n" +
                              std::string(kGraph)));
    // A different directive wrapper stays rejected.
    EXPECT_FALSE(drawn_native("%%{notinit: { 'logLevel': 0} }%%\n" +
                              std::string(kGraph)));
    // Bare non-object payload.
    EXPECT_FALSE(drawn_native("%%{init: 0}%%\n" + std::string(kGraph)));
}

// RFC 0031 FS-init: accTitle/accDescr accessibility statements. Upstream
// exposes these only to screen readers; the terminal consumes them as
// validated non-visual no-ops so they no longer block the body parse.
TEST(MermaidAccessibilityStatements, ScalarStatementsDrawAndDoNotPaint) {
    EXPECT_TRUE(drawn_native("flowchart TD\n accTitle: Title text\n A --> B\n"));
    EXPECT_TRUE(drawn_native("flowchart TD\n accDescr: Description text\n A --> B\n"));
    // Whitespace around the colon is tolerated.
    EXPECT_TRUE(drawn_native("flowchart TD\n accTitle : Spaced title\n A --> B\n"));
    const auto grid = surface_text(
        "flowchart TD\n accTitle: Hidden title\n accDescr: Hidden description\n"
        " A --> B\n");
    // The accessibility values never reach the canvas.
    EXPECT_EQ(grid.find("Hidden title"), std::string::npos);
    EXPECT_EQ(grid.find("Hidden description"), std::string::npos);
    EXPECT_NE(grid.find("A"), std::string::npos);
    // Combined with the logLevel directive, as the pinned fixtures spell it.
    EXPECT_TRUE(drawn_native("%%{init: { 'logLevel': 0} }%%\n"
                             "graph TD\n accTitle: T\n accDescr: D\n A --> B\n"));
}

TEST(MermaidAccessibilityStatements, MalformedAndDuplicateFailClosed) {
    // Empty value.
    EXPECT_FALSE(drawn_native("flowchart TD\n accTitle:\n A --> B\n"));
    // Markup/backtick characters are outside the scalar contract.
    EXPECT_FALSE(drawn_native("flowchart TD\n accTitle: A <b>bold</b>\n A --> B\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n accDescr: use `code` here\n A --> B\n"));
    // The multi-line block form is not whitelisted for flowchart.
    EXPECT_FALSE(drawn_native("flowchart TD\n accDescr {\n line one\n }\n A --> B\n"));
    // Duplicate statements of the same kind reject the whole source.
    EXPECT_FALSE(drawn_native("flowchart TD\n accTitle: One\n accTitle: Two\n A --> B\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n accDescr: One\n accDescr: Two\n A --> B\n"));
    // A keyword prefix glued to another identifier is not the statement.
    EXPECT_FALSE(drawn_native("flowchart TD\n accTitleX: Nope\n A --> B\n"));
    // The 256-byte value is the inclusive limit; 257 fails closed.
    const std::string at_limit(256, 'x');
    const std::string over_limit(257, 'x');
    EXPECT_TRUE(drawn_native("flowchart TD\n accTitle: " + at_limit + "\n A --> B\n"));
    EXPECT_FALSE(drawn_native("flowchart TD\n accTitle: " + over_limit + "\n A --> B\n"));
    // Diagram-level statements inside a subgraph are outside the whitelist.
    EXPECT_FALSE(drawn_native("flowchart TD\n subgraph G\n accTitle: Nested\n end\n"
                              " A --> B\n"));
    // Accessibility metadata alone never makes a graph: the empty parse
    // stays verbatim source, so the raw statement remains on the canvas
    // (a native flow render would have consumed and hidden it).
    const auto empty = surface_text("flowchart TD\n accTitle: Only this\n");
    EXPECT_NE(empty.find("accTitle: Only this"), std::string::npos);
}
