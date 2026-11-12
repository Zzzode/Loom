// RFC 0031 venn-family renderer durable cohort gate. Public API only: enforces
// tools/mermaid/venn-cohort.json against the live renderer and the pinned
// manifest. The eleven clean venn originals render natively; the seven
// frontmatter-gated and three body-style-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/venn-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nVENN_SENTINEL";
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

TEST(MermaidVennCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_venn_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 11);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 11);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 7);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 3);
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

TEST(MermaidVennCohort, CleanVennsDrawOnWholeAndBodyTracks) {
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

TEST(MermaidVennCohort, GatedVennsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

// Key-based gate: prose mentioning "style"/"theme" is not a gate; a frontmatter
// key or a body style assignment closes the renderer.
TEST(MermaidVennCohort, VisualGateIsKeyBased) {
    const std::string plain = "venn-beta\n  set Style\n  union Style, Theme\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string body_style =
        "venn-beta\n  set A\n  set B\n  union A, B\n  style A fill:#ff0000\n";
    EXPECT_FALSE(draws(body_style, kWide));

    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "venn-beta\n  set A\n";
    EXPECT_FALSE(draws(with_look, kWide));
}

// All grammar shapes: bare and quoted set names, :size, ["label"], two/three
// name unions with and without sizes/labels, and nested text under a set or a
// union, all transcribed verbatim.
TEST(MermaidVennCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string sized =
        "venn-beta\n  set A:20\n  set B:15\n  set C:10\n"
        "  union A, B:5\n  union A, B, C:1\n";
    const CellCapture sized_cap = capture(sized, "mermaid", kWide);
    ASSERT_TRUE(draws(sized, kWide));
    EXPECT_NE(sized_cap.text.find("size 20"), std::string::npos);
    EXPECT_NE(sized_cap.text.find("size 5"), std::string::npos);

    const std::string labeled =
        "venn-beta\n  title Team Skills\n  set Frontend[\"Frontend\"]\n"
        "  set Backend[\"Backend\"]\n  union Frontend, Backend[\"Fullstack\"]\n";
    const CellCapture labeled_cap = capture(labeled, "mermaid", kWide);
    ASSERT_TRUE(draws(labeled, kWide));
    EXPECT_NE(labeled_cap.text.find("Fullstack"), std::string::npos);
    EXPECT_NE(labeled_cap.text.find("Frontend"), std::string::npos);
    EXPECT_NE(labeled_cap.text.find("Team Skills"), std::string::npos);

    const std::string quoted =
        "venn-beta\n  set \"JavaScript\"\n  set \"Python\"\n  union \"JavaScript\", \"Python\"\n";
    EXPECT_TRUE(draws(quoted, kWide));

    const std::string with_text =
        "venn-beta\n  set Fruits\n    text \"Apple\"\n    text \"Orange\"\n"
        "  set Veg\n    text \"Carrot\"\n  union Fruits, Veg\n    text \"Tomato\"\n";
    const CellCapture text_cap = capture(with_text, "mermaid", kWide);
    ASSERT_TRUE(draws(with_text, kWide));
    EXPECT_NE(text_cap.text.find("text Apple"), std::string::npos);
    EXPECT_NE(text_cap.text.find("text Tomato"), std::string::npos);
}

// Boundedness and fail-closed discipline: wrong header, unknown statement, a
// top-level text node, a union with fewer than two names, a non-numeric size,
// a structural tab, an unclosed label, and depth/cap exhaustion return source.
TEST(MermaidVennCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-venn\n  set A\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n  unknownStmt X\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n  text \"Orphan\"\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n  union A:5\n", kWide));          // one-name union
    EXPECT_FALSE(draws("venn-beta\n  set A:big\n", kWide));         // bad size
    EXPECT_FALSE(draws("venn-beta\n  set A\n\ttext \"X\"\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n  set A[\"unclosed\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n", kWide));                      // no statements
    // Role/level invariants: a set cannot nest under another set, and a text
    // node cannot sit deeper than one level.
    EXPECT_FALSE(draws("venn-beta\n  set A\n    set B\n", kWide));
    EXPECT_FALSE(draws("venn-beta\n  set A\n      text \"Deep\"\n", kWide));
}
