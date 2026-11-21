// RFC 0031 gantt-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/gantt-cohort.json against the live renderer and the
// pinned manifest. The thirty-nine theme-free, valid-date gantt originals
// render natively; RFC 0040 promotes the eleven frontmatter-only theme rows
// (their per-row gate is test_mermaid_journeyfam_0040_cohort) and the two
// malformed-date fixtures fail closed.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/gantt-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1600;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nGANTT_SENTINEL";
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

TEST(MermaidGanttCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_gantt_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 39);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 39);
    // RFC 0040 promotes every themed row through the closed table; the only
    // whole-source remainder is the two malformed-date fixtures.
    EXPECT_EQ(root.get("selection").get_int("themed_excluded_count"), 0);
    EXPECT_EQ(root.get("selection").get_int("admitted_count"), 11);
    EXPECT_EQ(root.get("selection").get_int("fail_closed_count"), 2);
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

TEST(MermaidGanttCohort, CleanGanttsDrawOnWholeAndBodyTracks) {
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

// RFC 0040: gantt's themed-only list is empty, so this family gate asserts the
// boundary values the closed table must keep on whole-source fallback, plus a
// control that an in-table palette still draws.
TEST(MermaidGanttCohort, OutOfTableVisualValuesStayWholeSource) {
    const auto probe = [](std::string_view config) {
        return "---\ndescription: >-\n  gantt boundary probe\nconfig:\n" + std::string(config) +
               "---\ngantt\n  dateFormat YYYY-MM-DD\n  section S\n  Task one :a1, 2024-01-01, 2d\n";
    };
    for (const auto* config : {"  theme: neo\n", "  theme: neo-dark\n", "  theme: base\n",
                               "  look: classic\n", "  look: neo\n", "  look: handDrawn\n"}) {
        EXPECT_FALSE(draws(probe(config), kWide)) << config;
    }
    EXPECT_TRUE(draws(probe("  theme: dark\n"), kWide));
}

TEST(MermaidGanttCohort, MalformedDateFixturesFailClosed) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("selection").get("fail_closed").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
}

// Key-based gate: the word "theme" in a task/section label is not a gate; a
// frontmatter look directive still closes the renderer.
TEST(MermaidGanttCohort, ThemeWordInLabelIsNotAVisualGate) {
    const std::string source =
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n  Pick a theme :a, 2024-01-01, 5d\n";
    EXPECT_TRUE(draws(source, kWide));
    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n  Task : 2024-01-01, 5d\n";
    EXPECT_FALSE(draws(with_look, kWide));
}

// Strict date shapes: a 3-digit year or 1-digit day must fail closed; valid
// zero-padded dates draw.
TEST(MermaidGanttCohort, StrictDateShapes) {
    EXPECT_FALSE(draws("gantt\n  dateFormat YYYY-MM-DD\n  section S\n  T : 202-12-01, 7d\n",
                       kWide));
    EXPECT_FALSE(draws("gantt\n  dateFormat YYYY-MM-DD\n  section S\n  T : 2019-09-1, 20d\n",
                       kWide));
    EXPECT_TRUE(draws("gantt\n  dateFormat YYYY-MM-DD\n  section S\n  T : 2024-01-01, 5d\n",
                      kWide));
}

// The measured task field shapes: tags, ids, after deps, two-date end,
// duration-only chained tasks, epoch points and milestones all draw.
TEST(MermaidGanttCohort, AcceptedTaskShapesDraw) {
    const std::string base = "gantt\n  dateFormat YYYY-MM-DD\n  section S\n";
    EXPECT_TRUE(draws(base + "A :crit, done, a1, 2024-01-01, 3d\n", kWide));
    EXPECT_TRUE(draws(base + "B :b1, 2024-01-01, 2024-01-08\n", kWide));
    EXPECT_TRUE(draws(base + "C :active, c1, after a1, 2d\n", kWide));
    EXPECT_TRUE(draws(base + "D : 4d\n", kWide));
    const std::string epoch = "gantt\n  dateFormat x\n  section S\n  E :e1, 0, 30ms\n";
    EXPECT_TRUE(draws(epoch, kWide));
    const std::string milestone =
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n  M :milestone, crit, m1, 2024-02-01, 0\n";
    const CellCapture milestone_cap = capture(milestone, "mermaid", kWide);
    ASSERT_TRUE(draws(milestone, kWide));
    // Review M1: a zero-span milestone transcribes as the milestone marker,
    // never as an explicit "end 0" date.
    EXPECT_NE(milestone_cap.text.find("span: milestone"), std::string::npos);
    EXPECT_EQ(milestone_cap.text.find("end 0"), std::string::npos);

    // Review N2: after/until keywords are matched case-insensitively.
    const std::string upper_dep =
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n"
        "a :a1, 2024-01-01, 1d\n  b : AFTER a1, 2d\n";
    EXPECT_TRUE(draws(upper_dep, kWide));
}

// after/until dependencies and multi-id after lists are transcribed, not
// resolved, and still draw.
TEST(MermaidGanttCohort, DependencyFormsDraw) {
    const std::string source =
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n"
        "onboarding :onb, 2024-01-02, 1w\n"
        "review :lit, 2024-01-02, 10d\n"
        "project A :pa, after onb lit, until m1\n"
        "milestone one :milestone, m1, 2024-02-01, 0\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("after onb lit"), std::string::npos);
    EXPECT_NE(cap.text.find("until m1"), std::string::npos);
}

// Bounded caps and unknown statements fail closed rather than emitting a
// partial frame.
TEST(MermaidGanttCohort, UnknownStatementAndCapsFailClosed) {
    EXPECT_FALSE(draws("not-a-gantt\n  section S\n", kWide));
    EXPECT_FALSE(draws("gantt\n  weirdstatement-without-colon\n", kWide));
    EXPECT_FALSE(draws("gantt\n  dateFormat YYYY-MM-DD\n  section S\n  T : 2024-01-01, 5q\n",
                       kWide));  // bad duration unit
    std::string too_many_sections = "gantt\n  dateFormat YYYY-MM-DD\n";
    for (int i = 0; i < 33; ++i)
        too_many_sections += "  section S" + std::to_string(i) + "\n";
    EXPECT_FALSE(draws(too_many_sections, kWide));
}

// Break markers sanitize case-insensitively, including the corpus tab form
// <br\t/>, and never leak markup; an unrelated structural tab still rejects.
TEST(MermaidGanttCohort, BreakMarkersSanitizeIncludingTabForm) {
    const std::string upper =
        "gantt\n  dateFormat YYYY-MM-DD\n  section A<BR/>multiline\n  T : 2024-01-01, 1d\n";
    const CellCapture upper_cap = capture(upper, "mermaid", kWide);
    ASSERT_TRUE(draws(upper, kWide));
    EXPECT_EQ(upper_cap.text.find("<BR"), std::string::npos);

    const std::string tab_form =
        "gantt\n  dateFormat YYYY-MM-DD\n  section A<br\t/>multiline\n  T : 2024-01-01, 1d\n";
    const CellCapture tab_cap = capture(tab_form, "mermaid", kWide);
    ASSERT_TRUE(draws(tab_form, kWide));
    EXPECT_EQ(tab_cap.text.find("<br"), std::string::npos);

    const std::string structural_tab =
        "gantt\n  dateFormat YYYY-MM-DD\n  section S\n  T\t: 2024-01-01, 1d\n";
    EXPECT_FALSE(draws(structural_tab, kWide));
}
