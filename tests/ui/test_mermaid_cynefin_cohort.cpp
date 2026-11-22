// RFC 0031 cynefin-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/cynefin-cohort.json against the live renderer and
// the pinned manifest. The seven clean cynefin originals render natively;
// the five frontmatter-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/cynefin-cohort.json";
const fs::path kD431Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/cynefin-ns-d431-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nCYNEFIN_SENTINEL";
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

TEST(MermaidCynefinCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_cynefin_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 7);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 7);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 5);
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

TEST(MermaidCynefinCohort, CleanCynefinsDrawOnWholeAndBodyTracks) {
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

TEST(MermaidCynefinCohort, GatedCynefinsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    // D-431 admits every `frontmatter_gated` row through the closed tables;
    // the positives live in test_mermaid_cynefin_ns_d431_cohort. The partition
    // is preserved: each excluded row must be a D-431 ledger member.
    std::set<std::string> namespace_rows;
    auto ns_ledger = json::parse(read_file(kD431Ledger));
    ASSERT_TRUE(ns_ledger.has_value()) << "D-431 cohort ledger must parse";
    ns_ledger->root().get("rows").iter(
        [&](const JsonVal row) { namespace_rows.insert(row.get_string("path")); });
    int gated = 0;
    selection.get("frontmatter_gated").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_TRUE(namespace_rows.contains(std::string(rel))) << rel;
        ++gated;
    });
    EXPECT_EQ(gated, 5);
    EXPECT_EQ(namespace_rows.size(), 5u);
    selection.get("body_style_gated").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
}

// Key-based gate: prose mentioning "theme"/"config" inside quoted items is
// not a gate; a frontmatter visual key or a body style statement closes the
// renderer.
TEST(MermaidCynefinCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "cynefin-beta\n"
        "  complex\n"
        "  \"theme drift and config fallout\"\n";
    EXPECT_TRUE(draws(plain, kWide));

    // D-431 admits the closed `config.cynefin` leaves (seed included); an
    // out-of-table leaf still closes the renderer.
    EXPECT_FALSE(draws(
        "---\nconfig:\n  cynefin:\n    colour: red\n---\ncynefin-beta\n  complex\n  \"Probe\"\n",
        kWide));
    // MAJOR-1 regression: YAML allows whitespace before the mapping colon, so
    // the gate key must be trimmed; "config :"/"theme :"/"look  :" are the
    // same visual contract as "config:" and must still close the renderer.
    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\ncynefin-beta\n  complex\n", kWide));
    EXPECT_FALSE(draws(
        "---\ntheme : dark\n---\ncynefin-beta\n  complex\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook  : handDrawn\n---\ncynefin-beta\n  complex\n", kWide));
    EXPECT_FALSE(draws("---\ntheme: dark\n---\ncynefin-beta\n  complex\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook: handDrawn\n---      \ncynefin-beta\n  complex\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  complex\n  \"Item\" :::critical\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  %%{init: {}}%%\n  complex\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  style complex fill:#f00\n", kWide));
}

// The grammar is a strict two-space frame: the five domain keywords, quoted
// items, title/acc directives and labeled transitions (self-loops included)
// are transcribed verbatim with their indentation preserved.
TEST(MermaidCynefinCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "cynefin-beta\n"
        "  title Strategy Categorization\n"
        "  accTitle: Cynefin framework\n"
        "  accDescr: Five domains of practice\n"
        "\n"
        "  complex\n"
        "  \"Market research\"\n"
        "  confusion\n"
        "  \"Unknown failure mode\"\n"
        "\n"
        "  complex --> complicated : \"Pattern identified\"\n"
        "  complex --> complex : \"Self-loop (dropped)\"\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Strategy Categorization"), std::string::npos);
    EXPECT_NE(cap.text.find("Market research"), std::string::npos);
    EXPECT_NE(cap.text.find("Unknown failure mode"), std::string::npos);
    EXPECT_NE(cap.text.find("Pattern identified"), std::string::npos);
    EXPECT_NE(cap.text.find("Self-loop (dropped)"), std::string::npos);
    // The two-space indentation is preserved on the emitted statement rows.
    EXPECT_NE(cap.text.find("  complex"), std::string::npos);

    // Domains without items is a valid empty frame and still draws.
    const std::string empty_frame =
        "cynefin-beta\n"
        "  complex\n"
        "  complicated\n"
        "  clear\n"
        "  chaotic\n";
    EXPECT_TRUE(draws(empty_frame, kWide));
    // A header-only diagram is likewise a valid frame (the markdown path
    // always supplies a trailing newline).
    EXPECT_TRUE(draws("cynefin-beta\n", kWide));
    // A blank or whitespace-only line is skipped, not a statement.
    EXPECT_TRUE(draws("cynefin-beta\n\n  complex\n     \n  \"Item A\"\n", kWide));
}

// Boundedness and fail-closed discipline: a wrong header, a structural tab, a
// bare CR, a missing/wrong indent, an unquoted or empty item, an unknown
// domain or transition endpoint, a missing transition label, an embedded
// quote, non-ASCII text and an unknown statement all return source.
TEST(MermaidCynefinCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-cynefin\n  complex\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n\tcomplex\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  a\rb\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\ncomplex\n", kWide));       // indent 0 statement
    EXPECT_FALSE(draws("cynefin-beta\n complex\n", kWide));      // indent 1, below the grid
    EXPECT_FALSE(draws("cynefin-beta\n    complex\n", kWide));   // indent 4, grid violation
    EXPECT_FALSE(draws("cynefin-beta\n  Unquoted item\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  complex\n  \"\"\n", kWide));  // empty item
    EXPECT_FALSE(draws("cynefin-beta\n  complex\n  \"a\"b\"c\"\n", kWide));  // embedded quote
    EXPECT_FALSE(draws("cynefin-beta\n  unknown_domain\n", kWide));
    EXPECT_FALSE(draws(
        "cynefin-beta\n  complex --> unknown : \"Label\"\n", kWide));  // bad endpoint
    EXPECT_FALSE(draws(
        "cynefin-beta\n  complex --> complicated\n", kWide));  // missing labeled shape
    EXPECT_FALSE(draws(
        "cynefin-beta\n  complex --> complicated : \"\"\n", kWide));  // empty label
    EXPECT_FALSE(draws("cynefin-beta\n  complex\n  \"caf\xc3\xa9\"\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  complex\n  \"\xf0\x9f\x9a\x80\"\n", kWide));
    EXPECT_FALSE(draws("cynefin-beta\n  note over complex: x\n", kWide));  // unknown statement
    // A non-empty title rest is mandatory.
    EXPECT_FALSE(draws("cynefin-beta\n  title \n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidCynefinCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "cynefin-beta\r\n"
        "  complex\r\n"
        "  \"Market research\"\r\n"
        "  complex --> complicated : \"Pattern identified\"\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("Market research"), std::string::npos);
    EXPECT_NE(cap.text.find("Pattern identified"), std::string::npos);
}
