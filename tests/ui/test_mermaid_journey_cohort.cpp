// RFC 0031 journey-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/journey-cohort.json against the live renderer and
// the pinned manifest. The ten clean (theme-free) journey originals render
// natively; the eight theme/look fixtures stay on whole-source fallback.
// Per-width states are measured, not assumed.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journey-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nJOURNEY_SENTINEL";
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
// Strip a leading YAML frontmatter block to obtain the diagram body.
std::string body_of(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidJourneyCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_journey_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 10);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 10);
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

TEST(MermaidJourneyCohort, CleanJourneysDrawOnWholeAndBodyTracks) {
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

        // Whole track: native wide frame, deterministic restored re-render,
        // per-width states matching the ledger.
        EXPECT_TRUE(whole_wide) << rel << " whole wide";
        const CellCapture wide_cap = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide_cap, capture(whole, "mermaid", kWide)))
            << rel << " restored determinism";
        ASSERT_FALSE(same_surface(wide_cap, capture(whole, "text", kWide)))
            << rel << " wide is a native frame";
        EXPECT_EQ(whole_wide, whole_states[0] == "drawn") << rel << " whole wide ledger";
        EXPECT_EQ(whole_narrow, whole_states[1] == "drawn") << rel << " whole narrow ledger";
        EXPECT_EQ(true, whole_states[2] == "drawn") << rel << " whole restored ledger";

        // Body track: the frontmatter-free diagram renders on its own and
        // matches the pinned body states at every width.
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

// Theme/look is a visual contract the terminal does not have; a themed
// journey must keep its whole source rather than silently drop the theme.
TEST(MermaidJourneyCohort, ThemedJourneysStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto excluded = ledger.root().get("selection").get("themed_excluded");
    excluded.iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
}

// The theme gate is key-based: the literal word "theme" in a task label or a
// non-theme init comment must not be mistaken for a theme configuration.
TEST(MermaidJourneyCohort, ThemeWordInLabelIsNotAVisualGate) {
    const std::string source =
        "journey\n"
        "  title Keyword test\n"
        "  section S\n"
        "    Choose a theme for the product: 4: Me\n";
    EXPECT_TRUE(draws(source, kWide));
    const std::string with_log_comment =
        "%%{init: { 'logLevel': 0 }}%%\n"
        "journey\n"
        "  section S\n"
        "    Discuss theming options: 3: Me\n";
    EXPECT_TRUE(draws(with_log_comment, kWide));
}

// An over-long score digit run must fail closed, never throw out_of_range.
TEST(MermaidJourneyCohort, OverflowedScoreFailsClosedWithoutThrowing) {
    const std::string overflow =
        "journey\n  section S\n    Big: 99999999999:\n";
    EXPECT_NO_THROW(EXPECT_FALSE(draws(overflow, kWide)));
    const std::string out_of_range =
        "journey\n  section S\n    Big: 2147483648:\n";
    EXPECT_NO_THROW(EXPECT_FALSE(draws(out_of_range, kWide)));
}

// A frontmatter look directive is a visual gate too and must stay on source.
TEST(MermaidJourneyCohort, FrontmatterLookIsAVisualGate) {
    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "journey\n  section S\n    Task: 3: Me\n";
    EXPECT_FALSE(draws(with_look, kWide));
}
