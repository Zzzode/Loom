// NON-AUTHORITATIVE REGRESSION SMOKE ONLY — RFC 0033 §1.4 / §7.1 NIT-4.
//
// This is the shelved D295 char-difference cohort gate, relabeled and retained
// purely as a renderer regression smoke. It compares a char-only CellCapture
// surface against a text-control fence at the same width and pins the seven-row
// D295 selection (six draw candidates plus the sha-bound handDrawn Work-ceiling
// row). It is explicitly NOT a matrix authority:
//   * it is never referenced by run_official_er206_case or any supplemental
//     proof path, and
//   * the sole matrix authority for these six paths is the independently
//     reviewed owned-proof cohort in mermaid_er_groupedge_official.hpp plus
//     tools/mermaid/verify_er_groupedge_admission.py.
// NO-GO §3 already classifies this char gate as unfit for drawn authority. Do
// not wire it into admission; a renderer change that moves a char cell is a
// smoke signal, never proof that unreviewed render output earns a draw.
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/er-group-edge-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1600;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\nER_GROUP_SENTINEL";
}

struct CellCapture {
    std::vector<std::string> chars;
};

CellCapture capture(const std::string& source, const char* language, int width) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.chars.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x)
            out.chars.push_back(screen.PixelAt(x, y).character);
    return out;
}

bool draws(const std::string& source, int width) {
    const auto native = capture(source, "mermaid", width);
    const auto control = capture(source, "text", width);
    return !native.chars.empty() && native.chars != control.chars;
}

JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}

std::string body_of(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}

std::array<std::string, 3> states_of(const JsonVal& states) {
    return {states.get_string("wide_240"), states.get_string("narrow_40"),
            states.get_string("restored_240")};
}
}  // namespace

TEST(MermaidErGroupEdgeCohort, LedgerMatchesSchemaAndReviewed) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_er_group_edge_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 7);
    EXPECT_EQ(counts.get_int("predicted_draw"), 6);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 1);
    // The D295 char cohort never earned drawn authority (the D298 NO-GO): its
    // review verdict is intentionally "pending". This smoke only pins the
    // selection/count facts and the blocker vocabulary; matrix authority lives
    // in the separate RFC 0033 owned-proof cohort, never in this ledger.
    EXPECT_EQ(root.get("review").get_string("verdict"), "pending");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 1u);
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-blocked")
            EXPECT_TRUE(vocabulary.contains(row.get("blocker").get_string("kind")));
        else
            EXPECT_TRUE(row.get("blocker").is_null());
    });
}

TEST(MermaidErGroupEdgeCohort, RowsPinFixtureBytesAndPriorStatus) {
    auto manifest = json::parse(read_file(kManifest));
    ASSERT_TRUE(manifest.has_value());
    std::map<std::string, JsonVal> manifest_rows;
    manifest->root().get("fixtures").iter([&](const JsonVal fixture) {
        manifest_rows.emplace(fixture.get_string("path"), fixture);
    });
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto path = row.get_string("path");
        const auto found = manifest_rows.find(std::string(path));
        ASSERT_NE(found, manifest_rows.end()) << path;
        const auto bytes = read_file(kCases / std::string(path));
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), row.get_string("sha256")) << path;
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), found->second.get_string("sha256"))
            << path;
        EXPECT_EQ(found->second.get("raw").get_string("expected"), "source") << path;
        EXPECT_EQ(found->second.get("raw").get_string("reason"), "frontmatter") << path;
    });
}

TEST(MermaidErGroupEdgeCohort, WholeAndBodyTracksMatchLedgerAtEveryWidth) {
    const auto ledger = parse_doc(kLedger);
    int drawn = 0, blocked = 0;
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / std::string(rel));
        const auto body = body_of(whole);
        const bool is_blocked = row.get_string("prediction") == "predicted-blocked";

        const std::array<bool, 3> whole_live{
            draws(whole, kWide), draws(whole, kNarrow), draws(whole, kWide)};
        const std::array<bool, 3> body_live{
            draws(body, kWide), draws(body, kNarrow), draws(body, kWide)};

        const auto whole_expected = states_of(row.get("expected_states"));
        const auto body_expected = states_of(row.get("expected_body_states"));
        for (std::size_t i = 0; i < 3; ++i) {
            EXPECT_EQ(whole_live[i], whole_expected[i] == "drawn")
                << rel << " whole track state " << i;
            EXPECT_EQ(body_live[i], body_expected[i] == "drawn")
                << rel << " body track state " << i;
        }

        if (is_blocked) {
            ++blocked;
            EXPECT_FALSE(whole_live[0]) << "blocked whole original rendered at 240: " << rel;
            EXPECT_TRUE(body_live[0]) << "blocked row body did not draw at 240: " << rel;
        } else {
            ++drawn;
            EXPECT_TRUE(whole_live[0]) << "registered draw kept whole source at 240: " << rel;
        }
    });
    EXPECT_EQ(drawn, 6);
    EXPECT_EQ(blocked, 1);
}

// Fail-closed guards: admitting valid group endpoints and in-group direction
// overrides must not relax the parser for malformed grammar. These drive the
// public render seam (not source-text matching) and expect ordinary fallback.
TEST(MermaidErGroupEdgeCohort, MalformedGroupGrammarKeepsWholeSource) {
    // An in-group direction must still be one of the four closed values; an
    // unknown value rejects the whole source rather than being swallowed.
    const std::string bad_direction =
        "---\n"
        "description: >-\n"
        "  Invalid in-group direction value.\n"
        "---\n"
        "erDiagram\n"
        "  subgraph domain\n"
        "  direction SIDEWAYS\n"
        "  CUSTOMER\n"
        "  ORDER\n"
        "  end\n"
        "  CUSTOMER ||--o{ ORDER : places\n";
    EXPECT_FALSE(draws(bad_direction, kWide));

    // A subgraph header with no id is not a valid group and stays fallback.
    const std::string bad_group =
        "erDiagram\n"
        "  subgraph\n"
        "  CUSTOMER\n"
        "  end\n";
    EXPECT_FALSE(draws(bad_group, kWide));
}

// Sibling ER subgraph originals that do NOT address a group as an edge
// endpoint are outside the selection and must keep their prior surface; the
// admission must not over-claim unrelated subgraph rows.
TEST(MermaidErGroupEdgeCohort, NonGroupEdgeSubgraphsKeepPriorSurface) {
    const auto ledger = parse_doc(kLedger);
    std::set<std::string> cohort;
    ledger.root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });

    const std::array<std::string, 4> siblings{{
        "er-diagram/nightly-check/should-render-a-simple-subgraph.mmd",
        "er-diagram/nightly-check/should-render-empty-subgraphs.mmd",
        "er-diagram/nightly-check/should-render-relationships-across-subgraphs.mmd",
        "er-diagram/pr-check/should-render-nested-subgraphs.mmd",
    }};
    for (const auto& rel : siblings) {
        EXPECT_FALSE(cohort.contains(rel)) << "sibling wrongly admitted to cohort: " << rel;
        const auto whole = read_file(kCases / rel);
        // These already drew before the admission; assert they still draw (no
        // regression).
        EXPECT_TRUE(draws(whole, kWide)) << "previously drawing sibling regressed: " << rel;
    }
}
