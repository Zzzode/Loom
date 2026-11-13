// RFC 0031 packet-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/packet-cohort.json against the live renderer and
// the pinned manifest. The four clean packet originals render natively; the
// two frontmatter config-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/packet-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nPACKET_SENTINEL";
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

TEST(MermaidPacketCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_packet_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 4);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 4);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 2);
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

TEST(MermaidPacketCohort, CleanPacketsDrawOnWholeAndBodyTracks) {
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

TEST(MermaidPacketCohort, GatedPacketsStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

TEST(MermaidPacketCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "packet\n"
        "  title config drift and theme fallout\n"
        "  0: \"a config themed style label\"\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\npacket\n  0: \"x\"\n", kWide));
    EXPECT_FALSE(draws(
        "---\ntheme: dark\n---\npacket\n  0: \"x\"\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook: handDrawn\n---      \npacket\n  0: \"x\"\n", kWide));
    EXPECT_FALSE(draws("packet\n  0: \"x\" :::critical\n", kWide));
    EXPECT_FALSE(draws("packet\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("packet\n  %%{init: {}}%%\n  0: \"x\"\n", kWide));
    EXPECT_FALSE(draws("packet\n  style x fill:#f00\n", kWide));
}

// Both headers and the two field shapes are accepted and transcribed.
TEST(MermaidPacketCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string ranged =
        "packet\n"
        "  title My header\n"
        "  0-15: \"Source Port\"\n"
        "  16: \"URG\"\n";
    const CellCapture cap = capture(ranged, "mermaid", kWide);
    ASSERT_TRUE(draws(ranged, kWide));
    EXPECT_NE(cap.text.find("Source Port"), std::string::npos);
    EXPECT_NE(cap.text.find("  0-15:"), std::string::npos);

    // The packet-beta alias is an interchangeable header.
    EXPECT_TRUE(draws("packet-beta\n  title Hello world\n  0-10: \"hello\"\n", kWide));
    // A header-only frame draws (markdown supplies the trailing newline).
    EXPECT_TRUE(draws("packet\n", kWide));
    // Blank lines are skipped.
    EXPECT_TRUE(draws("packet\n\n  0: \"h\"\n     \n", kWide));
}

// Boundedness and fail-closed discipline across the packet statement shapes.
TEST(MermaidPacketCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-packet\n  0: \"x\"\n", kWide));
    EXPECT_FALSE(draws("packet\n\t0: \"x\"\n", kWide));
    EXPECT_FALSE(draws("packet\n  a\rb\n", kWide));
    EXPECT_FALSE(draws("packet\n0: \"x\"\n", kWide));      // indent 0 statement
    EXPECT_FALSE(draws("packet\n 0: \"x\"\n", kWide));     // indent 1
    EXPECT_FALSE(draws("packet\n    0: \"x\"\n", kWide));  // indent 4, off grid

    // Field shapes.
    EXPECT_FALSE(draws("packet\n  0 \"x\"\n", kWide));     // missing colon
    EXPECT_FALSE(draws("packet\n  a: \"x\"\n", kWide));    // non-numeric position
    EXPECT_FALSE(draws("packet\n  0-: \"x\"\n", kWide));   // empty range side
    EXPECT_FALSE(draws("packet\n  0-1-2: \"x\"\n", kWide)); // too many range parts
    EXPECT_FALSE(draws("packet\n  -1: \"x\"\n", kWide));   // signed position
    EXPECT_FALSE(draws("packet\n  0: x\n", kWide));        // unquoted label
    EXPECT_FALSE(draws("packet\n  0: \"x\" extra\n", kWide)); // trailing token
    EXPECT_FALSE(draws("packet\n  0: \"a\"b\"\"\n", kWide));  // embedded quote
    EXPECT_FALSE(draws("packet\n  0: \"\"\n", kWide));     // empty label

    // Unknown statements and a bad title.
    EXPECT_FALSE(draws("packet\n  showBits false\n", kWide));
    EXPECT_FALSE(draws("packet\n  title\n", kWide));

    // Non-ASCII and a C0 control byte fail closed.
    EXPECT_FALSE(draws("packet\n  title caf\xc3\xa9\n", kWide));
    EXPECT_FALSE(draws("packet\n  0: \"a\x00" "b\"\n", kWide));
}

TEST(MermaidPacketCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "packet-beta\r\n"
        "  0-10: \"hello\"\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("hello"), std::string::npos);
}
