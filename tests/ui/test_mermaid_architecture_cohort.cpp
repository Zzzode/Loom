// RFC 0031 architecture-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/architecture-cohort.json against the live renderer
// and the pinned manifest. The sixteen clean architecture originals render
// natively; the four frontmatter-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/architecture-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nARCHITECTURE_SENTINEL";
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

TEST(MermaidArchitectureCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_architecture_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 16);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 16);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 4);
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

TEST(MermaidArchitectureCohort, CleanArchitecturesDrawOnWholeAndBodyTracks) {
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

TEST(MermaidArchitectureCohort, GatedArchitecturesStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

// Key-based gate: prose mentioning "config"/"theme" is not a gate; a
// frontmatter visual key or a body style statement closes the renderer.
TEST(MermaidArchitectureCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "architecture-beta\n  service Theme(config)[Node]\n  group cfg\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string with_config =
        "---\ndescription:\n  x\nconfig:\n  architecture:\n    randomize: false\n---\n"
        "architecture-beta\n  service a(server)[A]\n";
    EXPECT_FALSE(draws(with_config, kWide));

    const std::string with_theme =
        "---\ntheme: dark\n---\narchitecture-beta\n  service a(server)[A]\n";
    EXPECT_FALSE(draws(with_theme, kWide));

    const std::string body_style =
        "architecture-beta\n  service a(server)[A]\n  classDef c color:#ff0000\n";
    EXPECT_FALSE(draws(body_style, kWide));
}

// All grammar shapes: groups/services/junctions with optional icon/label and
// `in` containment (including a digit-leading id and a label-less service),
// the three edge forms, group-boundary {group} ports, and align directives,
// each transcribed verbatim.
TEST(MermaidArchitectureCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "architecture-beta\n"
        "  title Nested Cloud\n"
        "  group sub1(cloud)[Subscription A]\n"
        "  group vnet1(cloud)[VNet A] in sub1\n"
        "  service vm1(server)[VM] in vnet1\n"
        "  junction mid in vnet1\n"
        "  service cell[Table Cell]\n"
        "  vm1:R --> L:cell\n"
        "  vm1:B -- T:mid\n"
        "  vm1:L -[Label]- R:cell\n"
        "  align row vm1 cell\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Nested Cloud"), std::string::npos);
    EXPECT_NE(cap.text.find("group sub1(cloud)[Subscription A]"), std::string::npos);
    EXPECT_NE(cap.text.find("group vnet1(cloud)[VNet A] in sub1"), std::string::npos);
    EXPECT_NE(cap.text.find("service vm1(server)[VM] in vnet1"), std::string::npos);
    EXPECT_NE(cap.text.find("junction mid in vnet1"), std::string::npos);
    EXPECT_NE(cap.text.find("service cell[Table Cell]"), std::string::npos);
    EXPECT_NE(cap.text.find("vm1:R --> L:cell"), std::string::npos);
    EXPECT_NE(cap.text.find("vm1:B -- T:mid"), std::string::npos);
    EXPECT_NE(cap.text.find("vm1:L -[Label]- R:cell"), std::string::npos);
    EXPECT_NE(cap.text.find("align row vm1 cell"), std::string::npos);

    // Group-boundary edges preserve the {group} marker and the compass ports.
    const std::string group_edge =
        "architecture-beta\n  group left\n  group right\n"
        "  service d(disk)[D] in left\n  service c(disk)[C] in right\n"
        "  d{group}:R --> L:c{group}\n";
    const CellCapture ge = capture(group_edge, "mermaid", kWide);
    ASSERT_TRUE(draws(group_edge, kWide));
    EXPECT_NE(ge.text.find("d{group}:R --> L:c{group}"), std::string::npos);

    // A digit-leading identifier is a valid architecture-beta id.
    const std::string digit_id =
        "architecture-beta\n  junction 1Leftofmid in on_prem\n";
    EXPECT_TRUE(draws(digit_id, kWide));

    // MINOR 2: an icon-only declaration (no [label]) is its own branch.
    const std::string icon_only = "architecture-beta\n  service cache(redis)[Cache]\n";
    EXPECT_TRUE(draws(icon_only, kWide));
    const std::string icon_no_label = "architecture-beta\n  service q(queue)\n";
    const CellCapture inl = capture(icon_no_label, "mermaid", kWide);
    ASSERT_TRUE(draws(icon_no_label, kWide));
    EXPECT_NE(inl.text.find("service q(queue)"), std::string::npos);

    // NIT 6: a label may itself contain an arrow token; the labeled-edge
    // connector still wins and the line is transcribed verbatim.
    const std::string arrow_in_label =
        "architecture-beta\n  service a(server)[A]\n  service b(server)[B]\n"
        "  a:R -[flow --> b]- L:b\n";
    const CellCapture ail = capture(arrow_in_label, "mermaid", kWide);
    ASSERT_TRUE(draws(arrow_in_label, kWide));
    EXPECT_NE(ail.text.find("a:R -[flow --> b]- L:b"), std::string::npos);
}

// Boundedness and fail-closed discipline: wrong header, an unknown statement,
// a bad compass port, a malformed endpoint, an unclosed label, a structural
// tab, a duplicate title and an unknown align direction return source.
TEST(MermaidArchitectureCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-architecture\n  service a(server)[A]\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  unknownStmt x\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  a:X --> L:b\n", kWide));      // bad port
    EXPECT_FALSE(draws("architecture-beta\n  a:R --> X:b\n", kWide));      // bad remote port
    EXPECT_FALSE(draws("architecture-beta\n  a:R --- L:b\n", kWide));      // triple dash
    EXPECT_FALSE(draws("architecture-beta\n  service a(server)[A\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n\tservice a(server)[A]\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  title A\n  title B\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  align diagonal a b\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  service a(server)[A] in \n", kWide));
    // MINOR 1: the two endpoints have opposite field order. Reversing either
    // side, dropping the target port, or using an empty edge label fails.
    EXPECT_FALSE(draws("architecture-beta\n  R:a --> L:b\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  a:R --> b\n", kWide));
    EXPECT_FALSE(draws("architecture-beta\n  a:R -[]- L:b\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidArchitectureCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "architecture-beta\r\n  service a(server)[A]\r\n  a:R --> L:b\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("service a(server)[A]"), std::string::npos);
    EXPECT_NE(cap.text.find("a:R --> L:b"), std::string::npos);
}
