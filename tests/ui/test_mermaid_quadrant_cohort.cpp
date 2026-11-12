// RFC 0031 quadrant-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/quadrant-cohort.json against the live renderer and
// the pinned manifest. The seven clean quadrant originals render natively;
// the five %%{init}%%-gated and two body-style-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/quadrant-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nQUADRANT_SENTINEL";
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

TEST(MermaidQuadrantCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_quadrant_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 7);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 7);
    EXPECT_EQ(root.get("selection").get_int("init_gated_count"), 5);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 2);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 0);
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

TEST(MermaidQuadrantCohort, CleanQuadrantsDrawOnWholeAndBodyTracks) {
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
        // The header-only fixture's one-line body is surface-identical to plain
        // text (ledger body track "source"); every other body draws.
        EXPECT_EQ(body_wide, body_states[0] == "drawn") << rel << " body wide ledger";
        EXPECT_EQ(body_narrow, body_states[1] == "drawn") << rel << " body narrow ledger";
        EXPECT_EQ(body_wide, body_states[2] == "drawn") << rel << " body restored ledger";
    });
}

TEST(MermaidQuadrantCohort, GatedQuadrantsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key :
         {"init_gated", "body_style_gated", "frontmatter_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

// Key/shape-based gate: prose mentioning "color"/"radius" is not a gate; an
// init directive, a classDef/::: statement, a frontmatter visual key or an
// inline post-bracket point style closes the renderer.
TEST(MermaidQuadrantCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "quadrantChart\n  title Color radius prose\n  quadrant-1 Theme words\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string init =
        "quadrantChart\n%%{init: {\"quadrantChart\": {}}}%%\n  quadrant-1 A\n";
    EXPECT_FALSE(draws(init, kWide));

    const std::string point_style =
        "quadrantChart\n  Campaign A: [0.3, 0.6] radius: 20\n";
    EXPECT_FALSE(draws(point_style, kWide));

    const std::string classdef =
        "quadrantChart\n  quadrant-1 A\n  classDef class1 color: #908342\n";
    EXPECT_FALSE(draws(classdef, kWide));

    const std::string triple_colon =
        "quadrantChart\n  Campaign A:::class1: [0.3, 0.6]\n";
    EXPECT_FALSE(draws(triple_colon, kWide));

    const std::string with_config =
        "---\ndescription:\n  x\nconfig:\n  theme: dark\n---\n"
        "quadrantChart\n  quadrant-1 A\n";
    EXPECT_FALSE(draws(with_config, kWide));
}

// All grammar shapes: the bare header frame, the three axis-label forms, all
// four quadrant labels and named [x, y] points, each transcribed verbatim.
TEST(MermaidQuadrantCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string bare = "quadrantChart\n";
    const CellCapture bare_cap = capture(bare, "mermaid", kWide);
    ASSERT_TRUE(draws(bare, kWide));
    EXPECT_NE(bare_cap.text.find("quadrantChart"), std::string::npos);

    const std::string complete =
        "quadrantChart\n"
        "  title Reach and engagement of campaigns\n"
        "  x-axis Low Reach --> High Reach\n"
        "  y-axis Low Engagement\n"
        "  quadrant-1 We should expand\n"
        "  quadrant-2 Need to promote\n"
        "  quadrant-3 Re-evaluate\n"
        "  quadrant-4 May be improved\n"
        "  Campaign A: [0.3, 0.6]\n"
        "  Campaign B: [0.45, 0.23]\n";
    const CellCapture cap = capture(complete, "mermaid", kWide);
    ASSERT_TRUE(draws(complete, kWide));
    EXPECT_NE(cap.text.find("Reach and engagement of campaigns"), std::string::npos);
    EXPECT_NE(cap.text.find("x-axis: Low Reach --> High Reach"), std::string::npos);
    EXPECT_NE(cap.text.find("y-axis: Low Engagement"), std::string::npos);
    EXPECT_NE(cap.text.find("Q1: We should expand"), std::string::npos);
    EXPECT_NE(cap.text.find("Q4: May be improved"), std::string::npos);
    EXPECT_NE(cap.text.find("Campaign A (0.3, 0.6)"), std::string::npos);
    EXPECT_NE(cap.text.find("Campaign B (0.45, 0.23)"), std::string::npos);

    // Single label with a trailing arrow and a bare single label both draw.
    const std::string single_arrow =
        "quadrantChart\n  x-axis Reach -->\n  y-axis Engagement -->\n";
    EXPECT_TRUE(draws(single_arrow, kWide));
}

// Boundedness and fail-closed discipline: wrong header, unknown statement,
// malformed coordinates, an unclosed coordinate bracket, a structural tab, a
// duplicate title and an out-of-range quadrant number return source.
TEST(MermaidQuadrantCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-quadrant\n  quadrant-1 A\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  unknownStmt x\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [0.3, 0.x]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [.3, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [1.2.3, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [0.3, 0.6\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n\tP: [0.3, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  title A\n  title B\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  quadrant-9 Label\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  quadrant-1\n", kWide));
    // Coordinate token shapes outside the non-negative decimal grammar.
    EXPECT_FALSE(draws("quadrantChart\n  P: [-0.3, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [1e-3, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [+1, 0.6]\n", kWide));
    EXPECT_FALSE(draws("quadrantChart\n  P: [1., 0.6]\n", kWide));
    // A trailing token after the coordinate bracket is not a point.
    EXPECT_FALSE(draws("quadrantChart\n  P: [0.3, 0.6] extra\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidQuadrantCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "quadrantChart\r\n  quadrant-1 We should expand\r\n  P: [0.3, 0.6]\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("Q1: We should expand"), std::string::npos);
    EXPECT_NE(cap.text.find("P (0.3, 0.6)"), std::string::npos);
}
