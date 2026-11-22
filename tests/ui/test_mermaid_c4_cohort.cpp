// RFC 0031 c4-family renderer durable cohort gate. Public API only: enforces
// tools/mermaid/c4-cohort.json against the live renderer and the pinned
// manifest. The twenty-three structural c4 originals render natively; the nine
// style/layout-gated fixtures (eight Update*Style plus one frontmatter config)
// stay on whole-source fallback.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/c4-cohort.json";
const fs::path kD432Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/c4-wrap-d432-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nC4_SENTINEL";
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

TEST(MermaidC4Cohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_c4_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 23);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 23);
    EXPECT_EQ(root.get("selection").get_int("styled_excluded_count"), 9);
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

TEST(MermaidC4Cohort, StructuralC4sDrawOnWholeAndBodyTracks) {
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

TEST(MermaidC4Cohort, StyledC4sStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    // D-432 admits the one closed `config.wrap` row through the frontmatter
    // state machine; its positive is owned by
    // test_mermaid_c4_wrap_d432_cohort. The partition is preserved: that row
    // must be a D-432 ledger member, every other styled row stays whole source.
    std::set<std::string> wrap_rows;
    auto wrap_ledger = json::parse(read_file(kD432Ledger));
    ASSERT_TRUE(wrap_ledger.has_value()) << "D-432 cohort ledger must parse";
    wrap_ledger->root().get("rows").iter(
        [&](const JsonVal row) { wrap_rows.insert(row.get_string("path")); });
    int admitted = 0;
    ledger.root().get("selection").get("styled_excluded").iter([&](JsonVal value) {
        const auto rel = value.as_str();
        if (wrap_rows.contains(std::string(rel))) {
            ++admitted;
            return;
        }
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    });
    EXPECT_EQ(admitted, 1);
    EXPECT_EQ(wrap_rows.size(), 1u);
}

// Key-based gate: prose mentioning "style" is not a gate; a frontmatter
// theme/look/config key or an Update*Style statement closes the renderer.
TEST(MermaidC4Cohort, VisualGateIsKeyBased) {
    const std::string plain =
        "C4Context\n  title Style discussion\n  System(s, \"A style guide\")\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string update_style =
        "C4Context\n  System(a, \"A\")\n  UpdateElementStyle(a, $shape=\"folder\")\n";
    EXPECT_FALSE(draws(update_style, kWide));

    const std::string with_look =
        "---\ndescription:\n  x\nconfig:\n  look: handDrawn\n---\n"
        "C4Context\n  System(s, \"A\")\n";
    EXPECT_FALSE(draws(with_look, kWide));
}

// All five headers, representative elements across the vocabulary, all six
// relation directions, BiRel, technology/description args and a nested block
// draw, and the model is transcribed verbatim.
TEST(MermaidC4Cohort, AcceptedVocabularyAndNestingDraw) {
    static constexpr std::array<const char*, 5> kHeaders = {
        "C4Context", "C4Container", "C4Component", "C4Deployment", "C4Dynamic"};
    for (const auto* header : kHeaders)
        EXPECT_TRUE(draws(std::string(header) + "\n  System(s, \"A\")\n", kWide)) << header;

    const std::string elements =
        "C4Context\n"
        "  Person(p, \"Person\")\n  Person_Ext(pe, \"External\")\n"
        "  SystemDb(sdb, \"DB\")\n  SystemQueue_Ext(sqe, \"Q\")\n"
        "  ContainerDb(cdb, \"CDB\", \"Postgres\")\n"
        "  ComponentQueue_Ext(cqe, \"CQ\")\n";
    EXPECT_TRUE(draws(elements, kWide));

    const std::string rels =
        "C4Context\n  System(a, \"A\")\n  System(b, \"B\")\n"
        "  Rel(a, b, \"Uses\")\n  Rel_U(a, b, \"Up\")\n  Rel_D(a, b, \"Down\")\n"
        "  Rel_L(a, b, \"Left\")\n  Rel_R(b, a, \"Right\")\n  Rel_Back(a, b, \"Back\")\n"
        "  BiRel(a, b, \"Bi\")\n";
    const CellCapture rels_cap = capture(rels, "mermaid", kWide);
    ASSERT_TRUE(draws(rels, kWide));
    EXPECT_NE(rels_cap.text.find("a -> b: Uses"), std::string::npos);

    const std::string nested =
        "C4Deployment\n  Deployment_Node(dc, \"DC\", \"us-east-1\") {\n"
        "    Container(api, \"API\", \"Java\")\n  }\n  Rel(api, x, \"r\")\n";
    const CellCapture nested_cap = capture(nested, "mermaid", kWide);
    ASSERT_TRUE(draws(nested, kWide));
    EXPECT_NE(nested_cap.text.find("Deployment_Node dc - DC"), std::string::npos);
    EXPECT_NE(nested_cap.text.find("  - Container api - API"), std::string::npos);

    // Non-rendered $ attributes are omitted, not rejected.
    const std::string with_attr =
        "C4Context\n  Person(p, \"P\", \"desc\", $link=\"https://x\", $tags=\"v1\")\n";
    const CellCapture attr_cap = capture(with_attr, "mermaid", kWide);
    ASSERT_TRUE(draws(with_attr, kWide));
    EXPECT_EQ(attr_cap.text.find("$link"), std::string::npos);
}

// Boundedness and fail-closed discipline: wrong header, unknown call target,
// bad alias, an unterminated string, a relation opening a block, an unmatched
// close brace, an unclosed block, duplicate title, %% comment handling and cap
// exhaustion all return to source rather than partial output.
TEST(MermaidC4Cohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-c4\n  System(s, \"A\")\n", kWide));
    EXPECT_FALSE(draws("C4Context\n  NotAType(x, \"A\")\n", kWide));
    EXPECT_FALSE(draws("C4Context\n  System(1bad, \"A\")\n", kWide));
    EXPECT_FALSE(draws("C4Context\n  System(s, \"A)\n", kWide));          // unterminated
    EXPECT_FALSE(draws("C4Context\n  Rel(a, b, \"r\") {\n", kWide));      // rel cannot block
    EXPECT_FALSE(draws("C4Context\n  System(s, \"A\")\n}\n", kWide));    // unmatched close
    EXPECT_FALSE(draws("C4Context\n  Boundary(b, \"B\") {\n", kWide));   // unclosed block
    EXPECT_FALSE(draws("C4Context\n  plain statement without call\n", kWide));
    // Review MINOR-1: a duplicate title is rejected and a %% comment is skipped.
    EXPECT_FALSE(draws("C4Context\n  title One\n  title Two\n  System(s, \"A\")\n", kWide));
    const std::string with_comment =
        "C4Context\n  %% a comment\n  System(s, \"A\")\n";
    EXPECT_TRUE(draws(with_comment, kWide));
    // A bare (unquoted) extra relation argument is an upstream syntax error.
    EXPECT_FALSE(draws("C4Context\n  System(a,\"A\")\n  System(b,\"B\")\n  Rel(a, b, \"r\", bare)\n",
                       kWide));
}

// Review NIT-2: depth and statement caps fail closed instead of growing
// without bound.
TEST(MermaidC4Cohort, DepthAndStatementCapsFailClosed) {
    // 33 nested boundaries exceed the depth-32 cap.
    std::string too_deep = "C4Context\n";
    for (int i = 0; i < 33; ++i)
        too_deep += std::string(static_cast<std::size_t>(i * 2), ' ') +
                    "Boundary(b" + std::to_string(i) + ", \"B" + std::to_string(i) + "\") {\n";
    for (int i = 0; i < 33; ++i) too_deep += "}\n";
    EXPECT_FALSE(draws(too_deep, kWide));

    // 513 systems exceed the statement cap.
    std::string too_many = "C4Context\n";
    for (int i = 0; i < 513; ++i)
        too_many += "  System(s" + std::to_string(i) + ", \"S" + std::to_string(i) + "\")\n";
    EXPECT_FALSE(draws(too_many, kWide));
}

