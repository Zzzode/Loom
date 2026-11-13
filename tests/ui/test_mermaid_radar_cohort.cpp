// RFC 0031 radar-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/radar-cohort.json against the live renderer and the
// pinned manifest. The three clean radar originals render natively; the
// three frontmatter config/theme-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/radar-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nRADAR_SENTINEL";
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
    return close == std::string_view::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidRadarCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_radar_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 3);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 3);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 0);
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

TEST(MermaidRadarCohort, CleanRadarsDrawOnWholeAndBodyTracks) {
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
        const CellCapture body_wide_cap = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(body_wide_cap, capture(body, "mermaid", kWide)))
            << rel << " body restored determinism";
        EXPECT_EQ(body_wide, body_states[0] == "drawn") << rel << " body wide ledger";
        EXPECT_EQ(body_narrow, body_states[1] == "drawn") << rel << " body narrow ledger";
        EXPECT_EQ(body_wide, body_states[2] == "drawn") << rel << " body restored ledger";
    });
}

TEST(MermaidRadarCohort, GatedRadarsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

TEST(MermaidRadarCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "radar-beta\n"
        "  title config drift and theme fallout\n"
        "  axis A,B,C\n"
        "  curve c1{1, 2, 3}\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\nradar-beta\n  axis A,B,C\n  curve c1{1}\n", kWide));
    EXPECT_FALSE(draws(
        "---\ntheme: dark\n---\nradar-beta\n  axis A,B,C\n  curve c1{1}\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook: handDrawn\n---      \nradar-beta\n  axis A,B,C\n  curve c1{1}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  axis A,B,C\n  curve c1{1} :::critical\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  %%{init: {}}%%\n  axis A,B,C\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  style c1 fill:#f00\n", kWide));
}

// All real statement shapes (axes with aliases, the three curve styles and
// the scalar directives) are accepted and transcribed with indentation.
TEST(MermaidRadarCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "radar-beta\n"
        "  title My favorite ninjas\n"
        "  axis Agility, Speed, Strength\n"
        "  axis Stam[\"Stamina\"] , Intel[\"Intelligence\"]\n"
        "  curve Ninja1[\"Naruto Uzumaki\"]{\n"
        "      Agility 2, Speed 2,\n"
        "      Intel 0\n"
        "  }\n"
        "  curve Ninja2[\"Sasuke\"]{2, 3, 4}\n"
        "  curve Ninja3 {3, 2, 1}\n"
        "  showLegend true\n"
        "  ticks 3\n"
        "  max 8\n"
        "  min 0\n"
        "  graticule polygon\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Naruto Uzumaki"), std::string::npos);
    EXPECT_NE(cap.text.find("Agility 2"), std::string::npos);
    EXPECT_NE(cap.text.find("graticule polygon"), std::string::npos);
    // Block continuation and the two-space statement indent are preserved.
    EXPECT_NE(cap.text.find("      Intel 0"), std::string::npos);
    EXPECT_NE(cap.text.find("  curve Ninja2"), std::string::npos);

    // A header-only frame draws (markdown supplies the trailing newline).
    EXPECT_TRUE(draws("radar-beta\n", kWide));
    // Blank lines are skipped.
    EXPECT_TRUE(draws("radar-beta\n\n  axis A,B,C\n     \n  curve c1{1}\n", kWide));
}

// Boundedness and fail-closed discipline across the radar statement shapes.
TEST(MermaidRadarCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-radar\n  axis A,B,C\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n\taxis A,B,C\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  a\rb\n", kWide));
    EXPECT_FALSE(draws("radar-beta\naxis A,B,C\n", kWide));     // indent 0 statement
    EXPECT_FALSE(draws("radar-beta\n axis A,B,C\n", kWide));   // indent 1
    EXPECT_FALSE(draws("radar-beta\n    axis A,B,C\n", kWide)); // indent 4, off grid

    // Axis shapes.
    EXPECT_FALSE(draws("radar-beta\n  axis \n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  axis A,,B\n", kWide));     // empty spec
    EXPECT_FALSE(draws("radar-beta\n  axis 1A\n", kWide));       // bad identifier
    EXPECT_FALSE(draws("radar-beta\n  axis A[\n", kWide));       // broken alias
    EXPECT_FALSE(draws("radar-beta\n  axis A[\"x\"]extra\n", kWide));  // trailing token

    // Curve shapes.
    EXPECT_FALSE(draws("radar-beta\n  curve {1}\n", kWide));     // missing name
    EXPECT_FALSE(draws("radar-beta\n  curve c1\n", kWide));      // missing brace
    EXPECT_FALSE(draws("radar-beta\n  curve c1{\n", kWide));     // unclosed block at EOF
    EXPECT_FALSE(draws("radar-beta\n  curve c1{a, b}\n", kWide));  // non-numeric value
    EXPECT_FALSE(draws("radar-beta\n  curve c1{1 2}\n", kWide));   // space instead of comma
    // A trailing comma is the same optional-comma rule the real multi-line
    // block uses, so an inline list ending in one comma is accepted; a list
    // with no element at all (or only commas) still fails.
    EXPECT_TRUE(draws("radar-beta\n  curve c1{1,}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{,}\n", kWide));
    // MAJOR-1 regression: an empty slot is legal only as the final piece, so
    // leading, internal and repeated commas fail closed.
    EXPECT_FALSE(draws("radar-beta\n  curve c1{,1}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{1,,2}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{1,,,2}\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{1,,}\n", kWide));
    // The same rule holds inside a multi-line block.
    EXPECT_FALSE(draws(
        "radar-beta\n  curve c1{\n      1,,2\n  }\n", kWide));
    EXPECT_FALSE(draws(
        "radar-beta\n  curve c1{\n      ,1\n  }\n", kWide));
    // MINOR-1 regression: an empty multi-line block is rejected like {}.
    EXPECT_FALSE(draws("radar-beta\n  curve c1{\n  }\n", kWide));
    // A block continuation must be deeper indented; a column-2 line inside an
    // open block that is not the closing brace fails closed.
    EXPECT_FALSE(draws(
        "radar-beta\n  curve c1{\n  Agility 2\n  }\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{\n      Agility x\n  }\n", kWide));

    // Scalar directives.
    EXPECT_FALSE(draws("radar-beta\n  ticks 3.5\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  showLegend yes\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  graticule circle\n", kWide));  // not in corpus grammar
    EXPECT_FALSE(draws("radar-beta\n  unknownDirective 5\n", kWide));

    // Non-ASCII and a C0 control byte fail closed.
    EXPECT_FALSE(draws("radar-beta\n  title caf\xc3\xa9\n", kWide));
    EXPECT_FALSE(draws("radar-beta\n  curve c1{\"a\x00" "b\"}\n", kWide));
}

TEST(MermaidRadarCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "radar-beta\r\n"
        "  axis A, B, C\r\n"
        "  curve c1{1, 2, 3}\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("curve c1"), std::string::npos);
}
