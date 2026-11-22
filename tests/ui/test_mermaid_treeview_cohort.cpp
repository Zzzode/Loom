// RFC 0031 tree-view family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/treeview-cohort.json against the live renderer and
// the pinned manifest. The eight clean tree-view originals render natively;
// the five frontmatter-gated and two body-style-gated fixtures stay source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treeview-cohort.json";
const fs::path kD433Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treeview-ns-d433-cohort.json";
const fs::path kD434Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treeview-icons-d434-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nTREEVIEW_SENTINEL";
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

TEST(MermaidTreeviewCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treeview_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 8);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 8);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 5);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 2);
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

TEST(MermaidTreeviewCohort, CleanTreeviewsDrawOnWholeAndBodyTracks) {
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

TEST(MermaidTreeviewCohort, GatedTreeviewsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    // D-433 admits two closed `config.treeView` rows and D-434 the closed
    // icon-map row; their positives live in the two namespace gates.
    // The partition is preserved: each admitted row must be a D-433 ledger
    // member, every other gated row stays whole source.
    std::set<std::string> namespace_rows;
    for (const auto& path : {kD433Ledger, kD434Ledger}) {
        auto ns_ledger = json::parse(read_file(path));
        ASSERT_TRUE(ns_ledger.has_value()) << "namespace cohort ledger must parse";
        ns_ledger->root().get("rows").iter(
            [&](const JsonVal row) { namespace_rows.insert(row.get_string("path")); });
    }
    int admitted = 0;
    selection.get("frontmatter_gated").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        if (namespace_rows.contains(std::string(rel))) {
            ++admitted;
            return;
        }
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
    EXPECT_EQ(admitted, 3);
    EXPECT_EQ(namespace_rows.size(), 3u);
    selection.get("body_style_gated").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
}

// Key-based gate: prose mentioning "config"/"theme" is not a gate; a
// frontmatter visual key or a body :::/classDef statement closes the renderer.
TEST(MermaidTreeviewCohort, VisualGateIsKeyBased) {
    const std::string plain = "treeView-beta\n  theme/\n      config.ts\n";
    EXPECT_TRUE(draws(plain, kWide));

    // D-433 admits the closed `config.treeView` namespace (showIcons,
    // defaultIconPack, rowIndent, lineThickness); an out-of-table leaf still
    // closes the renderer (the positive is owned by the D-433 gate).
    EXPECT_FALSE(draws(
        "---\nconfig:\n  treeView:\n    colour: red\n---\ntreeView-beta\n  \"root\"\n",
        kWide));
    EXPECT_FALSE(draws("---\ntheme: dark\n---\ntreeView-beta\n  \"root\"\n", kWide));
    // A trailing-whitespace closing fence still carries the frontmatter gate.
    EXPECT_FALSE(draws(
        "---\nconfig:\n  x: 1\n---      \ntreeView-beta\n  \"root\"\n", kWide));
    EXPECT_FALSE(draws("---\nlook: handDrawn\n---\ntreeView-beta\n  \"root\"\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::highlight\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  %%{init: {}}%%\n  a\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  style a fill:#f00\n", kWide));
    // MINOR 1: after a closing quoted label only suffix tokens may follow.
    EXPECT_FALSE(draws("treeView-beta\n  \"root\" stray text\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  \"root\" x\"y\"\n", kWide));
}

// All grammar shapes: quoted and bare labels, nested indentation, a trailing
// slash directory, icon(...) and ## description suffixes, emoji and unicode,
// consecutive spaces and multiple roots, each transcribed verbatim.
TEST(MermaidTreeviewCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "treeView-beta\n"
        "  src/\n"
        "      index.js icon(fa:bell) ## app entry\n"
        "      But  _  _ton💓.tsx\n"
        "  \"quoted root\"\n"
        "      \"nested file.ts\"\n"
        "  README.md\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("src/"), std::string::npos);
    EXPECT_NE(cap.text.find("index.js icon(fa:bell) ## app entry"), std::string::npos);
    EXPECT_NE(cap.text.find("But  _  _ton💓.tsx"), std::string::npos);
    EXPECT_NE(cap.text.find("\"quoted root\""), std::string::npos);
    EXPECT_NE(cap.text.find("\"nested file.ts\""), std::string::npos);

    // A header-only tree is a valid empty frame and still draws.
    const std::string empty = "treeView-beta\n";
    EXPECT_TRUE(draws(empty, kWide));
}

// Boundedness and fail-closed discipline: wrong header, a structural tab, a
// bare CR, an unclosed quote, an indentation off the 2+4k grid and an
// unknown icon/description bracket shape return source.
TEST(MermaidTreeviewCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-treeview\n  \"root\"\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n\t\"root\"\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n  \"unclosed\n", kWide));
    EXPECT_FALSE(draws("treeView-beta\n    root\n", kWide));      // indent 4, off grid
    EXPECT_FALSE(draws("treeView-beta\n root\n", kWide));        // indent 1
    EXPECT_FALSE(draws("treeView-beta\nroot\n", kWide));         // indent 0
    EXPECT_FALSE(draws("treeView-beta\n   root\n", kWide));      // indent 3
    EXPECT_FALSE(draws("treeView-beta\n     root\n", kWide));    // indent 5
    EXPECT_FALSE(draws("treeView-beta\n  a icon(bell\n", kWide));  // unclosed icon
    // MINOR 2: bare CR (not part of a CRLF pair) is structural.
    EXPECT_FALSE(draws("treeView-beta\n  a\rb\n", kWide));
    // Node suffix shape boundaries.
    EXPECT_FALSE(draws("treeView-beta\n  \"\"\n", kWide));                  // empty quoted label
    EXPECT_FALSE(draws("treeView-beta\n  a icon(x) icon(y)\n", kWide));     // two icons
    EXPECT_FALSE(draws("treeView-beta\n  a ## d icon(x)\n", kWide));        // icon after ##
    EXPECT_FALSE(draws("treeView-beta\n  a icon()\n", kWide));              // empty icon arg
    EXPECT_FALSE(draws("treeView-beta\n  a##b\n", kWide));                  // no-space ##
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidTreeviewCohort, CrlfLineEndingsDraw) {
    const std::string crlf = "treeView-beta\r\n  src/\r\n      index.js\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("src/"), std::string::npos);
    EXPECT_NE(cap.text.find("index.js"), std::string::npos);
}
