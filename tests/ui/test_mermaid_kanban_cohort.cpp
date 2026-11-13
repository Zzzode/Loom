// RFC 0031 kanban-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/kanban-cohort.json against the live renderer and the
// pinned manifest. All eleven clean kanban originals render natively.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/kanban-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nKANBAN_SENTINEL";
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

TEST(MermaidKanbanCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_kanban_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 11);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 11);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 0);
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

TEST(MermaidKanbanCohort, CleanKanbansDrawOnWholeAndBodyTracks) {
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

// Key-based gate: prose mentioning "config"/"theme" is not a gate; a
// frontmatter visual key or a body style statement closes the renderer.
TEST(MermaidKanbanCohort, VisualGateIsKeyBased) {
    const std::string plain = "kanban\n  theme[Config Board]\n  cfg[Theme work]\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string with_config =
        "---\ndescription:\n  x\nconfig:\n  kanban:\n    foo: 1\n---\n"
        "kanban\n  id1[Todo]\n";
    EXPECT_FALSE(draws(with_config, kWide));

    // MINOR 1: every frontmatter visual key closes the renderer.
    const std::string with_theme = "---\ntheme: dark\n---\nkanban\n  id1[Todo]\n";
    EXPECT_FALSE(draws(with_theme, kWide));
    const std::string with_look = "---\nlook: handDrawn\n---\nkanban\n  id1[Todo]\n";
    EXPECT_FALSE(draws(with_look, kWide));

    // MINOR 1: every in-body visual branch closes the renderer.
    EXPECT_FALSE(draws("kanban\n  id1[Todo]\n%%{init: {}}%%\n", kWide));
    const std::string body_style =
        "kanban\n  id1[Todo]\n  style id1 fill:#ff0000\n";
    EXPECT_FALSE(draws(body_style, kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]\n  classDef c color:#ff0000\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]:::critical\n", kWide));
}

// All grammar shapes: a bare id, an id[label], a single- and multi-key
// annotation with quoted and bare values, including spaces, apostrophes and
// ticket ids, each transcribed verbatim.
TEST(MermaidKanbanCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "kanban\n"
        "  id1[Todo]\n"
        "  docs[Create Documentation]\n"
        "  id2\n"
        "  id8[Design grammar]@{ assigned: 'knsv' }\n"
        "  id5[define getData]@{ ticket: MC-2037, assigned: knsv, priority: 'High' }\n"
        "  id12[Can't reproduce]\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("id1[Todo]"), std::string::npos);
    EXPECT_NE(cap.text.find("docs[Create Documentation]"), std::string::npos);
    EXPECT_NE(cap.text.find("id2"), std::string::npos);
    EXPECT_NE(cap.text.find("id8[Design grammar]@{ assigned: 'knsv' }"), std::string::npos);
    EXPECT_NE(cap.text.find("id5[define getData]@{ ticket: MC-2037, assigned: knsv, "
                            "priority: 'High' }"),
              std::string::npos);
    EXPECT_NE(cap.text.find("id12[Can't reproduce]"), std::string::npos);
}

// Boundedness and fail-closed discipline: wrong header, an unknown statement,
// an unclosed label or annotation, a structural tab and a stray tail return
// source.
TEST(MermaidKanbanCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-kanban\n  id1[Todo]\n", kWide));
    EXPECT_FALSE(draws("kanban\n  unknown stmt x\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]@{ assigned: 'knsv'\n", kWide));
    EXPECT_FALSE(draws("kanban\n\tid1[Todo]\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo] trailing\n", kWide));
    EXPECT_FALSE(draws("kanban\n  [Todo]\n", kWide));
    // NIT 5: empty label, empty annotation and nested braces fail closed.
    EXPECT_FALSE(draws("kanban\n  id1[]\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]@{}\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]@{ }\n", kWide));
    EXPECT_FALSE(draws("kanban\n  id1[Todo]@{ a: { b: 1 } }\n", kWide));
}

// MINOR 2: a header-only board is a valid empty frame and still draws; a bare
// CR (not part of a CRLF pair) is structural and fails closed.
TEST(MermaidKanbanCohort, EmptyFrameDrawsAndBareCrFailsClosed) {
    const std::string empty = "kanban\n";
    const CellCapture cap = capture(empty, "mermaid", kWide);
    ASSERT_TRUE(draws(empty, kWide));
    EXPECT_NE(cap.text.find("kanban"), std::string::npos);

    EXPECT_FALSE(draws("kanban\n  id1[Todo]\rid2[Done]\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidKanbanCohort, CrlfLineEndingsDraw) {
    const std::string crlf = "kanban\r\n  id1[Todo]\r\n  id2[In progress]\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("id1[Todo]"), std::string::npos);
    EXPECT_NE(cap.text.find("id2[In progress]"), std::string::npos);
}
