// RFC 0031 D204 requirement-family style cohort gate. Public API only:
// enforces tools/mermaid/requirement-cohort.json against the live renderer
// and the pinned manifest. The four description-only-frontmatter requirement
// originals whose bodies use classDef/class/style/::: statements render
// natively through the shared ClassDirectStyles seam; every config/theme
// gated requirement original keeps its whole-source fallback.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/requirement-cohort.json";
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
// M1 regression gate: count cells carrying the terminal bold attribute, so a
// font-weight that only appears in the legend but never reaches the pixels
// cannot pass.
std::size_t bold_cell_count(const std::string& source, const char* language, int width) {
    const std::string fenced =
        "```" + std::string(language) + "\n" + source + "\n```\n\nEM_SENTINEL";
    auto element = loom::ui::render_markdown(fenced);
    if (!element) return 0u;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    std::size_t bold = 0u;
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x)
            if (screen.PixelAt(x, y).bold) ++bold;
    return bold;
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

TEST(MermaidRequirementCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_requirement_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 4);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 4);
    // Full-family partition measured by the renderer-free generator.
    EXPECT_EQ(root.get("selection").get_int("family_total"), 172);
    EXPECT_EQ(root.get("selection").get_int("native_draw_count"), 16);
    EXPECT_EQ(root.get("selection").get_int("gated_count"), 156);
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

TEST(MermaidRequirementCohort, StyledFixturesDrawOnWholeAndBodyTracks) {
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

// Every style statement shape is parsed, applied through palette roles and
// disclosed in the legend: nothing is silently dropped.
TEST(MermaidRequirementCohort, StyleStatementsPaintAndDisclose) {
    const std::string source =
        "---\n"
        "description: style disclosure\n"
        "---\n"
        "requirementDiagram\n"
        "requirement test_req:::blue {\n"
        "id: 1\n"
        "text: the test text.\n"
        "risk: high\n"
        "verifymethod: test\n"
        "}\n"
        "element test_entity {\n"
        "type: simulation\n"
        "}\n"
        "test_entity - satisfies -> test_req\n"
        "classDef bold font-weight: bold\n"
        "classDef blue stroke:lightblue, color: #0000FF\n"
        "test_entity:::bold\n";
    ASSERT_TRUE(draws(source, kWide));
    const auto cap = capture(source, "mermaid", kWide);
    // Verbatim classDef profile statements survive in the legend with their
    // resolved palette roles; the painted boxes carry the per-node application.
    EXPECT_NE(cap.text.find("classDef bold font-weight: bold"), std::string::npos);
    EXPECT_NE(cap.text.find("classDef blue stroke:lightblue, color: #0000FF"),
              std::string::npos);
    EXPECT_NE(cap.text.find("font:bold"), std::string::npos);
    EXPECT_NE(cap.text.find("stroke:Cyan(shared info)"), std::string::npos);
    EXPECT_NE(cap.text.find("fg:Blue(shared info)"), std::string::npos);
}

// The `default` classDef cascades to every node; direct styles apply last and
// keep non-overridden base properties (fixture 4 precedence contract).
TEST(MermaidRequirementCohort, DefaultClassAndDirectStylePrecedence) {
    const std::string source =
        "requirementDiagram\n"
        "requirement test_req:::blue {\n"
        "id: 1\n"
        "text: the test text.\n"
        "risk: high\n"
        "verifymethod: test\n"
        "}\n"
        "element test_entity {\n"
        "type: simulation\n"
        "}\n"
        "test_entity - satisfies -> test_req\n"
        "classDef blue stroke:lightblue, color:blue\n"
        "classDef default fill:pink\n"
        "style test_entity color:green\n";
    ASSERT_TRUE(draws(source, kWide));
    const auto cap = capture(source, "mermaid", kWide);
    EXPECT_NE(cap.text.find("classDef default fill:pink"), std::string::npos);
    EXPECT_NE(cap.text.find("style test_entity color:green"), std::string::npos);
    EXPECT_NE(cap.text.find("fill:Red"), std::string::npos);
    EXPECT_NE(cap.text.find("fg:Green"), std::string::npos);
}

// Strict grammar: any property, color, class, target or shape the bounded
// profile cannot model rejects the whole source instead of drawing a
// half-styled diagram.
TEST(MermaidRequirementCohort, MalformedStylesFailClosed) {
    const std::string base =
        "requirementDiagram\n"
        "requirement test_req {\n"
        "id: 1\n"
        "text: the test text.\n"
        "risk: high\n"
        "verifymethod: test\n"
        "}\n"
        "element test_entity {\n"
        "type: simulation\n"
        "}\n"
        "test_entity - satisfies -> test_req\n";
    // Unknown style property and malformed values.
    EXPECT_FALSE(draws(base + "classDef c bogus: 1\n", kWide));
    EXPECT_FALSE(draws(base + "classDef c fill: #nothex\n", kWide));
    EXPECT_FALSE(draws(base + "classDef c fill: rebeccapurple\n", kWide));  // outside profile
    EXPECT_FALSE(draws(base + "classDef c font-weight: 900\n", kWide));
    EXPECT_FALSE(draws(base + "classDef c fill:#f00, fill:#0f0\n", kWide));  // duplicate
    EXPECT_FALSE(draws(base + "classDef c stroke:transparent\n", kWide));
    // Duplicate classDef names and malformed class statements.
    EXPECT_FALSE(draws(base + "classDef c fill:#f00\nclassDef c fill:#0f0\n", kWide));
    EXPECT_FALSE(draws(base + "class test_req blue, blue\n", kWide));
    EXPECT_FALSE(draws(base + "class ghost blue\n", kWide));       // unknown target
    EXPECT_FALSE(draws(base + "class test_req ghost\n", kWide));   // unknown class
    EXPECT_FALSE(draws(base + "class test_req blue,\n", kWide));   // trailing comma
    EXPECT_FALSE(draws(base + "class test_req ,blue\n", kWide));   // empty first segment
    // Direct styles: bad target list, unknown property.
    EXPECT_FALSE(draws(base + "style ghost fill:#f00\n", kWide));
    EXPECT_FALSE(draws(base + "style test_req,test_entity fill:#f00, bogus:1\n", kWide));
    EXPECT_FALSE(draws(base + "style test_req, fill:#f00\n", kWide));  // trailing comma
    EXPECT_FALSE(draws(base + "style ,test_entity fill:#f00\n", kWide));
    // Unit suffix is case-insensitive; a malformed numeric still rejects.
    EXPECT_TRUE(draws(base + "style test_req stroke-width:2PX\n", kWide));
    EXPECT_FALSE(draws(base + "style test_req stroke-width:xx\n", kWide));
    // Shorthand shapes.
    EXPECT_FALSE(draws(base + "test_entity:::ghost\n", kWide));
    EXPECT_FALSE(draws(base + "test_entity:::blue:::bold\n", kWide));
    EXPECT_FALSE(draws(base + ":::blue\n", kWide));
    EXPECT_FALSE(draws(base + "test_entity:::\n", kWide));
    // Inline declaration suffix must be a bare identifier pair.
    EXPECT_FALSE(draws(
        "requirementDiagram\nrequirement \"spaced name\":::blue {\ntext: x\n}\n"
        "element test_entity {}\ntest_entity - satisfies -> spaced name\n",
        kWide));
}

// M1 regression: font-weight must reach actual terminal bold cells, not only
// the disclosure legend. A bold-only classDef still paints, and an explicit
// normal overrides an earlier bold.
TEST(MermaidRequirementCohort, FontWeightReachesBoldPixels) {
    const std::string base =
        "requirementDiagram\n"
        "requirement test_req {\n"
        "id: 1\ntext: the test text.\nrisk: high\nverifymethod: test\n}\n"
        "element test_entity {\ntype: simulation\n}\n"
        "test_entity - satisfies -> test_req\n";
    const std::string bold_source = base + "classDef bold font-weight: bold\n"
                                            "class test_entity bold\n";
    ASSERT_TRUE(draws(bold_source, kWide));
    EXPECT_GT(bold_cell_count(bold_source, "mermaid", kWide), 0u);
    EXPECT_EQ(bold_cell_count(bold_source, "text", kWide), 0u);

    // A bold-only declaration applied through the shorthand paints as well.
    const std::string shorthand = base + "classDef loud font-weight: bold\n"
                                          "test_req:::loud\n";
    ASSERT_TRUE(draws(shorthand, kWide));
    EXPECT_GT(bold_cell_count(shorthand, "mermaid", kWide), 0u);

    // Without any font-weight no cell is bold.
    const std::string plain = base + "classDef c fill:#f9f\nclass test_entity c\n";
    ASSERT_TRUE(draws(plain, kWide));
    EXPECT_EQ(bold_cell_count(plain, "mermaid", kWide), 0u);

    // font-weight: normal on a later direct statement clears the bold.
    const std::string overridden = base + "classDef loud font-weight: bold\n"
                                           "test_req:::loud\n"
                                           "style test_req font-weight: normal\n";
    ASSERT_TRUE(draws(overridden, kWide));
    EXPECT_EQ(bold_cell_count(overridden, "mermaid", kWide), 0u);
}

// The frozen frontmatter gates are untouched: engine directives and the
// indented YAML fence keep the whole-source fallback disposition.
TEST(MermaidRequirementCohort, FrontmatterAndThemeGatesStayClosed) {
    const std::array<std::string, 4> gated{{
        "requirement/nightly-check/a-not-so-simple-requirement-diagram-elk-layout.mmd",
        "requirement/nightly-check/a-diagram-with-a-title-neo-look-redux-dark-theme.mmd",
        "requirement/visual-check/with-a-theme-default.mmd",
        "requirement/visual-check/with-a-title-default.mmd",
    }};
    for (const auto& rel : gated) {
        const auto source = read_file(kCases / rel);
        EXPECT_FALSE(draws(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel;
    }
}

// The pre-D204 clean requirement sources keep drawing: the style extension is
// additive and never disturbs the existing twelve native fixtures.
TEST(MermaidRequirementCohort, ExistingCleanFixturesStillDraw) {
    const std::array<std::string, 3> sample{{
        "requirement/nightly-check/a-simple-requirement-diagram-default.mmd",
        "requirement/nightly-check/a-requirement-diagram-with-bt-direction-default.mmd",
        "requirement/pr-check/sample.mmd",
    }};
    for (const auto& rel : sample) {
        const auto source = read_file(kCases / rel);
        EXPECT_TRUE(draws(source, kWide)) << rel;
        EXPECT_TRUE(draws(source, kNarrow)) << rel;
    }
}
