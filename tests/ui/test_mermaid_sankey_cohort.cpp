// RFC 0031 D-211 sankey-family cohort gate. Public API only: enforces
// tools/mermaid/sankey-cohort.json against the live renderer and the pinned
// manifest. The one description-only sankey-beta original renders natively;
// the 17 config-gated originals keep their whole-source fallback.
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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/sankey-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nEM_SENTINEL";
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

TEST(MermaidSankeyCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_sankey_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 1);
    EXPECT_EQ(root.get("selection").get_int("family_total"), 18);
    EXPECT_EQ(root.get("selection").get_int("native_draw_count"), 1);
    EXPECT_EQ(root.get("selection").get_int("gated_count"), 17);
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

TEST(MermaidSankeyCohort, NativeFixtureDrawsOnWholeAndBodyTracks) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        const auto body = body_of(whole);
        const auto whole_states = load_states(row.get("expected_states"));
        const auto body_states = load_states(row.get("expected_body_states"));

        const bool whole_wide = draws(whole, kWide);
        const bool whole_narrow = draws(whole, kNarrow);
        EXPECT_TRUE(whole_wide) << rel << " whole wide";
        const CellCapture cap = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(cap, capture(whole, "mermaid", kWide)))
            << rel << " restored determinism";
        ASSERT_FALSE(same_surface(cap, capture(whole, "text", kWide)))
            << rel << " whole wide is a native frame";
        EXPECT_EQ(whole_wide, whole_states[0] == "drawn");
        EXPECT_EQ(whole_narrow, whole_states[1] == "drawn");

        const bool body_wide = draws(body, kWide);
        const bool body_narrow = draws(body, kNarrow);
        EXPECT_EQ(body_wide, body_states[0] == "drawn");
        EXPECT_EQ(body_narrow, body_states[1] == "drawn");
        EXPECT_EQ(body_wide, body_states[2] == "drawn");
    });
}

// The validated CSV link is transcribed verbatim with its source, target and
// value, rather than drawn as an approximate graph.
TEST(MermaidSankeyCohort, GrammarIsTranscribedFaithfully) {
    const std::string source =
        "sankey-beta\n"
        "  sourceNode,targetNode,10\n"
        "  targetNode,Net Profit,250\n";
    ASSERT_TRUE(draws(source, kWide));
    const auto cap = capture(source, "mermaid", kWide);
    EXPECT_NE(cap.text.find("sankey-beta"), std::string::npos);
    EXPECT_NE(cap.text.find("sourceNode -> targetNode: 10"), std::string::npos);
    EXPECT_NE(cap.text.find("targetNode -> Net Profit: 250"), std::string::npos);
}

// Strict fail-closed discipline for the three-field CSV grammar.
TEST(MermaidSankeyCohort, MalformedInputFailsClosed) {
    // Wrong header dialect and missing header.
    EXPECT_FALSE(draws("sankey\n  a,b,10\n", kWide));
    EXPECT_FALSE(draws("  a,b,10\n", kWide));
    // Field-count violations (two and four fields, embedded comma).
    EXPECT_FALSE(draws("sankey-beta\n  a,b\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b,c,10\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,\"b,c\",10\n", kWide));
    // Non-numeric / zero / oversized / signed / fractional value.
    EXPECT_FALSE(draws("sankey-beta\n  a,b,ten\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b,0\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b,-5\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b,1.5\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b,1234567890\n", kWide));
    // Empty labels and control byte fail closed.
    EXPECT_FALSE(draws("sankey-beta\n  ,b,10\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n  a,b\x01,10\n", kWide));
    // Odd indentation, tab and CR reject.
    EXPECT_FALSE(draws("sankey-beta\n a,b,10\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\n\ta,b,10\n", kWide));
    EXPECT_FALSE(draws("sankey-beta\r\n  a,b,10\n", kWide));
    // Header with no link row.
    EXPECT_FALSE(draws("sankey-beta\n", kWide));
    // An engine init directive is an unsupported visual contract, not a comment.
    EXPECT_FALSE(draws(
        "sankey-beta\n  %%{init: {'theme':'dark'}}%%\n  a,b,10\n", kWide));
    EXPECT_FALSE(draws(
        "sankey-beta\n%%{init: {'sankey': {'nodeWidth': 20}}}%%\n  a,b,10\n", kWide));
    // A plain comment still draws; an indented closing fence is malformed.
    EXPECT_TRUE(draws("sankey-beta\n  %% a plain comment\n  a,b,10\n", kWide));
    EXPECT_FALSE(draws(
        "---\ndescription: x\n  ---\nsankey-beta\n  a,b,10\n", kWide));
}

// The frozen frontmatter gate is untouched: config/theme keys keep fallback.
TEST(MermaidSankeyCohort, FrontmatterGatesStayClosed) {
    const std::array<std::string, 3> gated{{
        "sankey/nightly-check/custom-node-width.mmd",
        "sankey/nightly-check/fallback-colors-for-unspecified-nodes.mmd",
        "sankey/nightly-check/custom-node-colors.mmd",
    }};
    for (const auto& rel : gated) {
        const auto source = read_file(kCases / rel);
        EXPECT_FALSE(draws(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel;
    }
    // A synthetic config gate fails closed even with a valid CSV body.
    EXPECT_FALSE(draws(
        "---\ndescription: x\nconfig:\n  sankey:\n    nodeWidth: 20\n---\n"
        "sankey-beta\n  a,b,10\n",
        kWide));
}
