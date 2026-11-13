// RFC 0031 eventmodeling-family renderer durable cohort gate. Public API
// only: enforces tools/mermaid/eventmodeling-cohort.json against the live
// renderer and the pinned manifest. All six eventmodeling originals render
// natively (the family has no frontmatter-gated fixture).
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/eventmodeling-cohort.json";
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

TEST(MermaidEventModelingCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_eventmodeling_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 6);
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

TEST(MermaidEventModelingCohort, CleanFramesDrawOnWholeAndBodyTracks) {
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

TEST(MermaidEventModelingCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "eventmodeling\n"
        "tf 01 cmd AddItem { productId: 7 }\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\neventmodeling\ntf 01 ui CartScreen\n", kWide));
    EXPECT_FALSE(draws(
        "---\ntheme: dark\n---\neventmodeling\ntf 01 ui CartScreen\n", kWide));
    EXPECT_FALSE(draws(
        "eventmodeling\ntf 01 cmd AddItem :::critical\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\nclassDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\n%%{init: {}}%%\ntf 01 ui CartScreen\n",
                       kWide));
    EXPECT_FALSE(draws("eventmodeling\nstyle x fill:#f00\n", kWide));
}

// All real frame suffixes, qualified names and the data block are accepted
// and transcribed with their column layout.
TEST(MermaidEventModelingCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "eventmodeling\n"
        "tf 01 ui CartScreen\n"
        "tf 02 cmd Inventory.AddItem { productId: 7 }\n"
        "tf 03 evt ItemAdded [[ItemAddedData]]\n"
        "tf 04 rmo CartItems ->> 03\n"
        "tf 05 evt ItemChanged ->> 02 ->> 03\n"
        "\n"
        "data ItemAddedData\n"
        "{\n"
        "  productId: 7,\n"
        "  quantity: 1\n"
        "}\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Inventory.AddItem"), std::string::npos);
    EXPECT_NE(cap.text.find("->>"), std::string::npos);
    EXPECT_NE(cap.text.find("  quantity: 1"), std::string::npos);  // block indent kept

    // A header-only frame draws (markdown supplies the trailing newline).
    EXPECT_TRUE(draws("eventmodeling\n", kWide));
}

// Boundedness and fail-closed discipline across the frame/block shapes.
TEST(MermaidEventModelingCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-eventmodeling\ntf 01 ui CartScreen\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\n\ttf 01 ui CartScreen\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\n  tf 01 ui CartScreen\n", kWide));  // indented top-level

    // Frame index / kind.
    EXPECT_FALSE(draws("eventmodeling\ntf 1 ui CartScreen\n", kWide));    // one digit
    EXPECT_FALSE(draws("eventmodeling\ntf 001 ui CartScreen\n", kWide));  // three digits
    EXPECT_FALSE(draws("eventmodeling\ntf 01 widget CartScreen\n", kWide)); // unknown kind
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui\n", kWide));               // missing name
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui .AddItem\n", kWide));      // leading dot
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui Add..Item\n", kWide));     // double dot
    // M1 regression: every dotted-name segment must start with an ident char.
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui 1\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui 1A\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui A.1B\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui A [[B.2]]\n", kWide));

    // Suffix shapes.
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X [[Unclosed\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X [[ ]]\n", kWide));      // empty ref
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X ->> 9\n", kWide));     // bad index width
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X ->>\n", kWide));       // missing index
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X { unclosed\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X { }\n", kWide));       // empty inline data
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X bogus\n", kWide));     // unknown suffix
    // N1 regression: at most one inline block and one reference; relations may chain.
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X [[D]] [[E]]\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 evt X { a: 1 } { b: 2 }\n", kWide));

    // Data block balance and indentation.
    EXPECT_FALSE(draws(
        "eventmodeling\ndata ItemAddedData\n{\n  productId: 7\n", kWide));  // unclosed block
    EXPECT_FALSE(draws(
        "eventmodeling\n{\n  productId: 7\n}\n", kWide));                   // brace without data
    EXPECT_FALSE(draws(
        "eventmodeling\ndata D\n{\nproductId: 7\n}\n", kWide));             // content not indented
    EXPECT_FALSE(draws(
        "eventmodeling\ndata D\n{\n  no colon here\n}\n", kWide));         // missing colon
    EXPECT_FALSE(draws("eventmodeling\n}\n", kWide));                      // stray close brace
    // M2 regression: an empty data block is rejected.
    EXPECT_FALSE(draws("eventmodeling\ndata D\n{\n}\n", kWide));
    // N4 regression: only one optional trailing comma on a data entry.
    EXPECT_FALSE(draws("eventmodeling\ndata D\n{\n  k: v,,\n}\n", kWide));
    // N5 regression: a data header must be immediately followed by its block.
    EXPECT_FALSE(draws("eventmodeling\ndata A\ndata B\n{\n  k: v\n}\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ndata A\ntf 01 ui X\n", kWide));

    // Non-ASCII and a C0 control byte fail closed.
    EXPECT_FALSE(draws("eventmodeling\ntf 01 ui caf\xc3\xa9\n", kWide));
    EXPECT_FALSE(draws("eventmodeling\ntf 01 cmd X { a: \"\x00" "b\" }\n", kWide));
}

TEST(MermaidEventModelingCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "eventmodeling\r\n"
        "tf 01 ui CartScreen\r\n"
        "tf 02 cmd AddItem\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("CartScreen"), std::string::npos);
}
