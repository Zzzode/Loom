// RFC 0031 ishikawa-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/ishikawa-cohort.json against the live renderer and
// the pinned manifest. Thirteen originals: seven clean rows render natively,
// two RFC 0043 theme rows and three RFC 0046 `config.ishikawa.*` tuning rows
// are admitted with a footer, and one (`look: handDrawn`) stays on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/ishikawa-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nISHIKAWA_SENTINEL";
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

TEST(MermaidIshikawaCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_ishikawa_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 7);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 7);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 0);
    EXPECT_EQ(root.get("selection").get_int("d443_admitted_count"), 1);
    EXPECT_EQ(root.get("selection").get_int("admitted_count"), 2);
    EXPECT_EQ(root.get("selection").get_int("layout_admitted_count"), 3);
    EXPECT_EQ(root.get("selection").get_int("family_total"), 13);
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

TEST(MermaidIshikawaCohort, CleanIshikawasDrawOnWholeAndBodyTracks) {
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

TEST(MermaidIshikawaCohort, GatedIshikawasStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    // RFC 0043: the partition sizes are asserted explicitly so a shrinking
    // gate list cannot turn this loop vacuous.
    int frontmatter = 0;
    selection.get("frontmatter_gated").iter([&](JsonVal value) {
        ++frontmatter;
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
    // D-443 admitted the one remaining frontmatter row (its own d443 bucket).
    EXPECT_EQ(frontmatter, 0);
    int body_style = 0;
    selection.get("body_style_gated").iter([&](JsonVal value) {
        ++body_style;
        EXPECT_FALSE(draws(read_file(kCases / value.as_str()), kWide)) << value.as_str();
    });
    EXPECT_EQ(body_style, 0);
    // The two admitted theme rows draw and disclose exactly one footer.
    int admitted = 0;
    selection.get("admitted").iter([&](JsonVal value) {
        ++admitted;
        const auto rel = value.as_str();
        const auto source = read_file(kCases / rel);
        EXPECT_TRUE(draws(source, kWide)) << rel;
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "Ishikawa native projection v1"),
                  std::string::npos)
            << rel;
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("config.theme="),
                  std::string::npos)
            << rel;
    });
    EXPECT_EQ(admitted, 2);
    // D-443: the look row draws and discloses under the family footer; the
    // per-row frame oracle is test_mermaid_d443_inert_leaves_cohort.
    int d443 = 0;
    selection.get("d443_admitted").iter([&](JsonVal value) {
        ++d443;
        const auto rel = value.as_str();
        const auto source = read_file(kCases / rel);
        EXPECT_TRUE(draws(source, kWide)) << rel;
        const auto text = capture(source, "mermaid", kWide).text;
        EXPECT_NE(text.find("Ishikawa native projection v1"), std::string::npos) << rel;
        EXPECT_NE(text.find("config.look=handDrawn"), std::string::npos) << rel;
    });
    EXPECT_EQ(d443, 1);
    // RFC 0046: the three tuning rows draw, disclose the family header and
    // their `config.ishikawa.` line, and stay outside the theme cohort.
    int layout = 0;
    selection.get("layout_admitted").iter([&](JsonVal value) {
        ++layout;
        const auto rel = value.as_str();
        const auto source = read_file(kCases / rel);
        EXPECT_TRUE(draws(source, kWide)) << rel;
        const auto text = capture(source, "mermaid", kWide).text;
        EXPECT_NE(text.find("Ishikawa native projection v1"), std::string::npos) << rel;
        EXPECT_NE(text.find("config.ishikawa."), std::string::npos) << rel;
        EXPECT_EQ(text.find("config.theme="), std::string::npos) << rel;
    });
    EXPECT_EQ(layout, 3);
}

// Key-based gate: prose mentioning "theme"/"config" is not a gate; a
// frontmatter visual key or a body style statement closes the renderer.
TEST(MermaidIshikawaCohort, VisualGateIsKeyBased) {
    const std::string plain = "ishikawa-beta\n  theme issue\n      config drift\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig:\n  ishikawa:\n    x: 1\n---\nishikawa-beta\n  Problem\n",
        kWide));
    EXPECT_FALSE(draws("---\ntheme: dark\n---\nishikawa-beta\n  Problem\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook: handDrawn\n---      \nishikawa-beta\n  Problem\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  Problem :::critical\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  %%{init: {}}%%\n  Problem\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  style Problem fill:#f00\n", kWide));
}

// The grammar is a relative-indentation tree of bare ASCII labels; mixed
// indents define parent/child depth and are transcribed verbatim, including
// digits, hyphens and the root-only case. Non-ASCII/emoji text fails closed
// because the bare-label character set is ASCII-only.
TEST(MermaidIshikawaCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "ishikawa-beta\n"
        "  Server Outage\n"
        "  Hardware\n"
        "      Disk\n"
        "          Bad sectors\n"
        "  3-1\n"
        "    3-1-1\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Server Outage"), std::string::npos);
    EXPECT_NE(cap.text.find("Hardware"), std::string::npos);
    EXPECT_NE(cap.text.find("Disk"), std::string::npos);
    EXPECT_NE(cap.text.find("Bad sectors"), std::string::npos);
    EXPECT_NE(cap.text.find("3-1-1"), std::string::npos);
    // The relative indentation is preserved on the emitted node rows.
    EXPECT_NE(cap.text.find("      Disk"), std::string::npos);

    // A root-only diagram is a valid empty-cause frame and still draws, with
    // or without a trailing newline.
    const std::string root_only = "ishikawa-beta\n  Problem\n";
    EXPECT_TRUE(draws(root_only, kWide));
    EXPECT_TRUE(draws("ishikawa-beta\n", kWide));
}

// Boundedness and fail-closed discipline: wrong header, a structural tab, a
// bare CR, a node with no indentation, an empty label and an unknown quoted
// or suffixed shape return source.
TEST(MermaidIshikawaCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-ishikawa\n  Problem\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n\tProblem\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  a\rb\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\nProblem\n", kWide));      // no indentation
    EXPECT_FALSE(draws("ishikawa-beta\n Problem\n", kWide));     // indent 1, below the >=2 bound
    EXPECT_FALSE(draws("ishikawa-beta\n  \"Quoted\"\n", kWide));  // quotes unsupported
    EXPECT_FALSE(draws("ishikawa-beta\n  Problem\n  Cause[x]\n", kWide));  // suffix unsupported
    // MINOR 1: non-ASCII and emoji labels fail closed (ASCII-only label set).
    EXPECT_FALSE(draws("ishikawa-beta\n  café\n", kWide));
    EXPECT_FALSE(draws("ishikawa-beta\n  \xf0\x9f\x9a\x80 launch\n", kWide));
    // A blank line is whitespace and is skipped, not a node; the rest draws.
    EXPECT_TRUE(draws("ishikawa-beta\n\n  Problem\n", kWide));
    // A whitespace-only indented line is likewise skipped.
    EXPECT_TRUE(draws("ishikawa-beta\n  Problem\n     \n      Cause\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidIshikawaCohort, CrlfLineEndingsDraw) {
    const std::string crlf = "ishikawa-beta\r\n  Problem\r\n      Cause\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("Problem"), std::string::npos);
    EXPECT_NE(cap.text.find("Cause"), std::string::npos);
}
