// RFC 0031 treemap-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/treemap-cohort.json against the live renderer and the
// pinned manifest. The ten plain (no classDef/config) treemap originals render
// natively; the ten classDef/::: and six frontmatter-config fixtures stay on
// whole-source fallback.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treemap-cohort.json";
const fs::path kD427Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treemap-fmt-d427-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nTREEMAP_SENTINEL";
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

TEST(MermaidTreemapCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treemap_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 10);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 10);
    EXPECT_EQ(root.get("selection").get_int("styled_excluded_count"), 10);
    EXPECT_EQ(root.get("selection").get_int("config_excluded_count"), 6);
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

TEST(MermaidTreemapCohort, PlainTreemapsDrawOnWholeAndBodyTracks) {
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

TEST(MermaidTreemapCohort, StyledAndConfigTreemapsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    selection.get("styled_excluded").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
    // D-427 admits every `config_excluded` row through the closed
    // `config.treemap.valueFormat` grammar (the spec is honoured in the
    // labels). The partition is preserved: each excluded row must be a D-427
    // ledger member, and that gate owns the positive assertions.
    std::set<std::string> value_format_rows;
    auto format_ledger = json::parse(read_file(kD427Ledger));
    ASSERT_TRUE(format_ledger.has_value()) << "D-427 cohort ledger must parse";
    format_ledger->root().get("rows").iter(
        [&](const JsonVal row) { value_format_rows.insert(row.get_string("path")); });
    int config_rows = 0;
    selection.get("config_excluded").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_TRUE(value_format_rows.contains(std::string(rel))) << rel;
        ++config_rows;
    });
    EXPECT_EQ(config_rows, 6);
    EXPECT_EQ(value_format_rows.size(), 6u);
}

// Key-based gate: the word "theme" inside a description or node name is not a
// gate; a frontmatter theme/look/config leaf or a classDef/::: style closes
// the renderer.
TEST(MermaidTreemapCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "treemap-beta\n  \"Theme discussion\"\n    \"Pick a theme\": 10\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "treemap-beta\n  \"Root\"\n    \"Leaf\": 1\n";
    EXPECT_FALSE(draws(with_look, kWide));

    // D-427 admits the closed valueFormat shape; its positive lives in
    // test_mermaid_treemap_fmt_d427_cohort. Every other config key still gates.
    const std::string with_config =
        "---\ndescription:\n  x\nconfig:\n  treemap:\n    color: red\n---\n"
        "treemap\n  \"Root\"\n    \"Leaf\": 1\n";
    EXPECT_FALSE(draws(with_config, kWide));

    const std::string with_classdef =
        "treemap-beta\n  \"Root\"\n    \"A\": 1:::redClass\n  classDef redClass fill:#ff0000;\n";
    EXPECT_FALSE(draws(with_classdef, kWide));
}

// Both header spellings draw; hierarchy and source values are transcribed
// verbatim into the indented outline.
TEST(MermaidTreemapCohort, BothHeadersAndHierarchyTranscribe) {
    const std::string beta =
        "treemap-beta\n  \"L1\"\n    \"L2A\"\n      \"Leaf\": 10\n    \"L2B\"\n      \"Other\": 0.5\n";
    const CellCapture beta_cap = capture(beta, "mermaid", kWide);
    ASSERT_TRUE(draws(beta, kWide));
    EXPECT_NE(beta_cap.text.find("L1"), std::string::npos);
    EXPECT_NE(beta_cap.text.find("Leaf: 10"), std::string::npos);
    EXPECT_NE(beta_cap.text.find("Other: 0.5"), std::string::npos);

    const std::string bare =
        "treemap\n  \"Root\"\n    \"North\": 1234567\n";
    EXPECT_TRUE(draws(bare, kWide));
}

// Boundedness and fail-closed discipline: wrong header, a skipped indentation
// level, an unquoted name, a bad leaf value, a structural tab, and cap
// exhaustion all return to whole-source rather than partial output.
TEST(MermaidTreemapCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-a-treemap\n  \"Root\"\n", kWide));
    EXPECT_FALSE(draws("treemap-beta\n  \"Root\"\n      \"Leaf\": 1\n", kWide));  // skipped level
    EXPECT_FALSE(draws("treemap-beta\n  Root\n    Leaf: 1\n", kWide));            // unquoted
    EXPECT_FALSE(draws("treemap-beta\n  \"Root\"\n    \"Leaf\": abc\n", kWide));  // bad value
    EXPECT_FALSE(draws("treemap-beta\n  \"Root\"\n    \"Leaf\": 1.2.3\n", kWide));
    const std::string tabbed = "treemap-beta\n  \"Root\"\n\t\"Leaf\": 1\n";
    EXPECT_FALSE(draws(tabbed, kWide));
    EXPECT_FALSE(draws("treemap-beta\n", kWide));  // no nodes
}

// Review M1: a value node is a leaf and cannot have descendants; a sibling at
// the same level after a leaf stays valid.
TEST(MermaidTreemapCohort, ValueLeafCannotHaveChildren) {
    const std::string leaf_then_child =
        "treemap-beta\n  \"Root\"\n    \"Leaf\": 1\n      \"Illegal\": 2\n";
    EXPECT_FALSE(draws(leaf_then_child, kWide));

    const std::string leaf_then_sibling =
        "treemap-beta\n  \"Root\"\n    \"Leaf A\": 1\n    \"Leaf B\": 2\n";
    EXPECT_TRUE(draws(leaf_then_sibling, kWide));

    const std::string leaf_then_uncle =
        "treemap-beta\n  \"Root\"\n    \"Branch\"\n      \"Leaf\": 1\n    \"Other\": 2\n";
    EXPECT_TRUE(draws(leaf_then_uncle, kWide));
}

// Review M2: depth and node caps fail closed instead of growing without bound.
TEST(MermaidTreemapCohort, DepthAndNodeCapsFailClosed) {
    // 33 nested containers exceeds the depth-32 cap (base indent 2 then 33
    // further 2-space steps -> deepest depth 33).
    std::string too_deep = "treemap-beta\n";
    for (int i = 0; i < 34; ++i)
        too_deep += std::string(static_cast<std::size_t>((i + 1) * 2), ' ') +
                    "\"N" + std::to_string(i) + "\"\n";
    EXPECT_FALSE(draws(too_deep, kWide));

    // 513 flat leaves exceed the 512-node cap.
    std::string too_many = "treemap-beta\n  \"Root\"\n";
    for (int i = 0; i < 513; ++i)
        too_many += "    \"L" + std::to_string(i) + "\": 1\n";
    EXPECT_FALSE(draws(too_many, kWide));
}

