// RFC 0031 timeline-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/timeline-cohort.json against the live renderer and
// the pinned manifest. The seven clean (theme-free) timeline originals render
// natively; RFC 0040 promotes the sixteen frontmatter-only look/theme rows
// (their per-row gate is test_mermaid_journeyfam_0040_cohort), and the seven
// rows whose theme arrives through an in-body %%{init}%% directive are
// disclosed: RFC 0047 admits the flat logLevel/theme payloads and D-454 the
// closed nested `timeline`/`themeVariables` objects, so no timeline row is
// whole-source any more.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

import std;
import loom.ui.visual.markdown;
import loom.serdes.json;
import loom.crypto.crypto;

namespace {
namespace fs = std::filesystem;
namespace json = loom::utils::json;
using json::JsonDoc;
using json::JsonVal;

const fs::path kCases = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases";
const fs::path kManifest = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/timeline-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 800;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};
CellCapture capture(const std::string& source, const char* language, int width) {
    const std::string fenced =
        "```" + std::string(language) + "\n" + source + "\n```\n\nTIMELINE_SENTINEL";
    auto element = loom::ui::render_markdown(fenced);
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.chars.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x) {
            out.chars.push_back(screen.PixelAt(x, y).character);
            out.text += screen.PixelAt(x, y).character;
        }
    return out;
}
bool same_surface(const CellCapture& a, const CellCapture& b) { return a.chars == b.chars; }
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
std::array<std::string, 3> load_states(const JsonVal& states) {
    return {states.get_string("wide_240"), states.get_string("narrow_40"),
            states.get_string("restored_240")};
}
std::string body_of(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidTimelineCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_timeline_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 7);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 7);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
    });
}

TEST(MermaidTimelineCohort, CleanTimelinesDrawOnWholeAndBodyTracks) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        const auto body = body_of(whole);
        const auto whole_states = load_states(row.get("expected_states"));
        const auto body_states = load_states(row.get("expected_body_states"));

        const bool whole_wide = draws(whole, kWide);
        const bool whole_narrow = draws(whole, kNarrow);
        RecordProperty(rel + " narrow", whole_narrow);

        EXPECT_TRUE(whole_wide) << rel << " whole wide";
        const CellCapture whole_wide_cap = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(whole_wide_cap, capture(whole, "mermaid", kWide)))
            << rel << " whole restored determinism";
        ASSERT_FALSE(same_surface(whole_wide_cap, capture(whole, "text", kWide)))
            << rel << " whole wide is a native frame";
        EXPECT_EQ(whole_wide, whole_states[0] == "drawn") << rel << " whole wide ledger";
        EXPECT_EQ(whole_narrow, whole_states[1] == "drawn") << rel << " whole narrow ledger";
        EXPECT_EQ(true, whole_states[2] == "drawn") << rel << " whole restored ledger";

        const bool body_wide = draws(body, kWide);
        const bool body_narrow = draws(body, kNarrow);
        EXPECT_TRUE(body_wide) << rel << " body wide";
        const CellCapture body_wide_cap = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(body_wide_cap, capture(body, "mermaid", kWide)))
            << rel << " body restored determinism";
        EXPECT_EQ(body_wide, body_states[0] == "drawn") << rel << " body wide ledger";
        EXPECT_EQ(body_narrow, body_states[1] == "drawn") << rel << " body narrow ledger";
        EXPECT_EQ(true, body_states[2] == "drawn") << rel << " body restored ledger";
    });
}

TEST(MermaidTimelineCohort, ThemedTimelinesStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto excluded = ledger.root().get("selection").get("themed_excluded");
    excluded.iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
    // RFC 0047: the inert leading logLevel/theme directives are admitted and
    // disclosed. D-454 admits the closed nested `timeline`/`themeVariables`
    // payloads; every other content key stays whole-source.
    const std::string directive =
        "%%{init: { 'logLevel': 'debug', 'theme': 'forest' } }%%\n"
        "timeline\n  title T\n  2024 : Event one\n";
    ASSERT_TRUE(draws(directive, kWide));
    const auto frame = capture(directive, "mermaid", kWide).text;
    EXPECT_NE(frame.find("Timeline native projection v1"), std::string::npos);
    EXPECT_NE(frame.find("config.logLevel=debug -> logger only; not applied"),
              std::string::npos);
    EXPECT_NE(frame.find("config.theme=forest -> colour palette; not applied"),
              std::string::npos);
    // A content key and an out-of-table theme still gate.
    EXPECT_FALSE(draws("%%{init: { 'theme': 'base', 'flowchart': {'htmlLabels': true}}}%%\n"
                       "timeline\n  title T\n  2024 : Event one\n",
                       kWide));
    EXPECT_FALSE(draws("%%{init: { 'theme': 'neo' }}%%\ntimeline\n  title T\n  2024 : Event one\n",
                       kWide));
    // D-454 direct positives: the two formerly-themed corpus shapes draw and
    // disclose (their per-row frame oracle is the RFC 0047 cohort gate).
    const std::string nested_boolean =
        "%%{init: { 'theme': 'base', 'timeline': {'disableMulticolor': true}}}%%\n"
        "timeline\n  title T\n  2024 : Event one\n";
    ASSERT_TRUE(draws(nested_boolean, kWide));
    EXPECT_NE(capture(nested_boolean, "mermaid", kWide).text.find(
                  "config.timeline.disableMulticolor=true -> colour palette; not applied"),
              std::string::npos);
    const std::string nested_colours =
        " %%{init: { 'theme': 'default' , 'themeVariables': {\n"
        "              'cScale0': '#ff0000',\n"
        "              'cScale1': '#00ff00'\n"
        "       } } }%%\n"
        "timeline\n  title T\n  2024 : Event one\n";
    ASSERT_TRUE(draws(nested_colours, kWide));
    const auto colours_frame = capture(nested_colours, "mermaid", kWide).text;
    EXPECT_NE(colours_frame.find("config.themeVariables.cScale0=#ff0000 -> colour palette; not applied"),
              std::string::npos);
    EXPECT_NE(colours_frame.find("config.themeVariables.cScale1=#00ff00 -> colour palette; not applied"),
              std::string::npos);
}

// Key-based gate: the word "theme" in a period/event must not be a false gate;
// a frontmatter look directive still closes the renderer.
TEST(MermaidTimelineCohort, ThemeWordInEventIsNotAVisualGate) {
    const std::string source =
        "timeline\n  title T\n  section S\n  2020 : Pick a theme for the product\n";
    EXPECT_TRUE(draws(source, kWide));
    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "timeline\n  section S\n  2020 : Event\n";
    EXPECT_FALSE(draws(with_look, kWide));
}

// A malformed period (no event after the colon) fails closed rather than
// emitting a partial frame.
TEST(MermaidTimelineCohort, EmptyEventFailsClosed) {
    const std::string malformed = "timeline\n  section S\n  2020 :\n";
    EXPECT_FALSE(draws(malformed, kWide));
    // An all-whitespace event is equivalent to an empty one.
    const std::string whitespace = "timeline\n  section S\n  2020 :    \n";
    EXPECT_FALSE(draws(whitespace, kWide));
}

// A colon continuation before any period exists has nowhere to append and must
// fail closed.
TEST(MermaidTimelineCohort, ContinuationBeforePeriodFailsClosed) {
    const std::string orphan = "timeline\n  : orphan event\n";
    EXPECT_FALSE(draws(orphan, kWide));
}

// Unknown statements (no colon, not title/section/header) and wrong headers
// are rejected instead of rendered as a partial frame.
TEST(MermaidTimelineCohort, UnknownStatementAndHeaderRejected) {
    EXPECT_FALSE(draws("not-a-timeline\n  2020 : Event\n", kWide));
    EXPECT_FALSE(draws("timeline\n  weirdstatement-without-colon\n", kWide));
    EXPECT_FALSE(draws("timeline\n", kWide));
}

// Every bounded cap fails closed: 17 events on one period, 257 periods, 33
// sections, and a single word wider than the 64-cell wrap limit.
TEST(MermaidTimelineCohort, CapExhaustionAndHardWordFailClosed) {
    std::string too_many_events = "timeline\n  P :";
    for (int i = 0; i < 17; ++i) too_many_events += " e" + std::to_string(i) + " :";
    too_many_events += " last\n";
    EXPECT_FALSE(draws(too_many_events, kWide));

    std::string too_many_periods = "timeline\n";
    for (int i = 0; i < 257; ++i)
        too_many_periods += "  P" + std::to_string(i) + " : Event\n";
    EXPECT_FALSE(draws(too_many_periods, kWide));

    std::string too_many_sections = "timeline\n";
    for (int i = 0; i < 33; ++i)
        too_many_sections += "  section S" + std::to_string(i) + "\n";
    EXPECT_FALSE(draws(too_many_sections, kWide));

    // kTimelineWrapWidth is 64 cells in the renderer; a 65-cell unbreakable
    // word exceeds it and must fail closed.
    const std::string hard_word = "timeline\n  P : " + std::string(65, 'a') + "\n";
    EXPECT_FALSE(draws(hard_word, kWide));
}

// M1: a colon continuation splits on ':' exactly like a period line, so all
// continuation events land in the most recent period's native frame.
TEST(MermaidTimelineCohort, ContinuationSplitsEventsLikePeriodLine) {
    const std::string source =
        "timeline\n  P : firstalpha\n    : secondbeta : thirdgamma\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("firstalpha"), std::string::npos);
    EXPECT_NE(cap.text.find("secondbeta"), std::string::npos);
    EXPECT_NE(cap.text.find("thirdgamma"), std::string::npos);
}

// N1: <br> markers are case-insensitive like upstream HTML tag matching; the
// markup never leaks into the native frame in any spelling.
TEST(MermaidTimelineCohort, BreakMarkersAreCaseInsensitive) {
    const std::string upper = "timeline\n  P : Event one<BR/>two\n";
    const CellCapture upper_cap = capture(upper, "mermaid", kWide);
    ASSERT_TRUE(draws(upper, kWide));
    EXPECT_EQ(upper_cap.text.find("<BR"), std::string::npos);
    EXPECT_NE(upper_cap.text.find("one two"), std::string::npos);

    const std::string mixed = "timeline\n  P : Event one<Br>two\n";
    const CellCapture mixed_cap = capture(mixed, "mermaid", kWide);
    ASSERT_TRUE(draws(mixed, kWide));
    EXPECT_EQ(mixed_cap.text.find("<Br"), std::string::npos);
    EXPECT_NE(mixed_cap.text.find("one two"), std::string::npos);
}
