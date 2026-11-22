// RFC 0031 D204 requirement-family style cohort gate. Public API only:
// enforces tools/mermaid/requirement-cohort.json against the live renderer
// and the pinned manifest. The four description-only-frontmatter requirement
// originals whose bodies use classDef/class/style/::: statements render
// natively through the shared ClassDirectStyles seam. The D-204 counts are a
// frozen snapshot: D-066 F0 (D-395) additionally admits eight inert-config
// rows governed by requirement-f0-cohort.json, so the D-204+ F0 partition is
// 172 total / 24 native / 148 gated. RFC 0037 then admits the config.look
// table (P1), so a `look` row now draws; every unadmitted key (theme before
// P2, title before P3, namespaced subtrees) still keeps its whole-source
// fallback; RFC 0044 admits the `config.layout: elk` rows and the live counts
// are asserted by test_mermaid_requirement_0037_cohort.cpp and the official
// matrix.
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
// D-066 F0: per-line trailing-space trim plus trailing blank-row strip, so a
// diagram frame can be compared with its config-free twin's frame.
std::vector<std::string> trimmed_lines(const CellCapture& capture, int width) {
    if (capture.chars.size() !=
        static_cast<std::size_t>(width) * static_cast<std::size_t>(kHeight))
        return {};
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(kHeight));
    for (int y = 0; y < kHeight; ++y) {
        std::string line;
        line.reserve(static_cast<std::size_t>(width));
        for (int x = 0; x < width; ++x)
            line += capture.chars[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)];
        while (!line.empty() && line.back() == ' ') line.pop_back();
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
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
// indented YAML fence keep the whole-source fallback disposition. RFC 0037 P3
// promotes the titled row (title + look + theme are all admitted now), so it
// is asserted separately: drawn wide, still source-fallback narrow because the
// disclosure footer exceeds the narrow budget.
TEST(MermaidRequirementCohort, FrontmatterAndThemeGatesStayClosed) {
    // RFC 0044 admitted the ELK row through the requirement envelope, so it is
    // no longer part of this frozen negative set.
    const std::array<std::string, 2> gated{{
        "requirement/visual-check/with-a-theme-default.mmd",
        "requirement/visual-check/with-a-title-default.mmd",
    }};
    for (const auto& rel : gated) {
        const auto source = read_file(kCases / rel);
        EXPECT_FALSE(draws(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel;
    }
    const auto titled = read_file(
        kCases / "requirement/nightly-check/a-diagram-with-a-title-neo-look-redux-dark-theme.mmd");
    EXPECT_TRUE(draws(titled, kWide));
    EXPECT_FALSE(draws(titled, kNarrow));
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

// D-066 F0 inert-config cohort. Each row carries description +
// config{htmlLabels:false} over a body whose config-free -default twin already
// draws; the projected frame must equal the twin's frame plus the bounded
// disclosure footer. This is the inertness proof: the accepted key changes
// nothing but disclosure.
TEST(MermaidRequirementCohort, F0InertConfigCohortMatchesTwins) {
    const fs::path ledger_path =
        fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/requirement-f0-cohort.json";
    const auto ledger = parse_doc(ledger_path);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_requirement_f0_cohort/v1");
    EXPECT_EQ(root.get("selection").get_int("row_count"), 8);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    int checked = 0;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto twin_rel = row.get_string("twin");
        const auto source = read_file(kCases / rel);
        const auto twin = read_file(kCases / twin_rel);
        ASSERT_TRUE(draws(twin, kWide)) << twin_rel << " (config-free twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        auto got = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        auto expected = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        // The fenced-markdown wrapper appends the sentinel line; the
        // requirement footer is projected inside the diagram, before it.
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "EM_SENTINEL") << rel;
        expected.pop_back();
        const bool tail_blank = !expected.empty() && expected.back().empty();
        if (tail_blank) expected.pop_back();
        expected.emplace_back();
        expected.emplace_back("Requirement native projection v1");
        expected.emplace_back("config.htmlLabels=false -> native text labels; not applied");
        if (tail_blank) expected.emplace_back();
        expected.emplace_back("EM_SENTINEL");
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        const auto states = load_states(row.get("expected_states"));
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide";
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored";
        ++checked;
    });
    EXPECT_EQ(checked, 8);
}

// D-066 F0 boundary: only the documented inert leaves are admitted; every
// visual/semantic key, every invalid value and an empty config block keep the
// whole source on fallback.
TEST(MermaidRequirementCohort, F0InertBoundaryKeepsVisualKeysClosed) {
    const std::string body =
        "requirementDiagram\n"
        "requirement test_req {\n"
        "id: 1\ntext: the test text.\nrisk: high\nverifymethod: test\n}\n"
        "element test_entity {\ntype: simulation\n}\n"
        "test_entity - satisfies -> test_req\n";
    const auto with = [&](std::string_view config) {
        return "---\ndescription: inert boundary probe\nconfig:\n" + std::string(config) +
               "---\n" + body;
    };
    EXPECT_TRUE(draws(with("  htmlLabels: false\n"), kWide));
    EXPECT_TRUE(draws(with("  htmlLabels: true\n"), kWide));
    EXPECT_TRUE(draws(with("  logLevel: 2\n"), kWide));
    EXPECT_TRUE(draws(with("  fontFamily: courier\n"), kWide));
    // A namespaced flowchart.* key is outside this family whitelist.
    EXPECT_FALSE(draws(with("  flowchart:\n    htmlLabels: false\n"), kWide));
    // Visual/semantic keys and unsupported values stay whole-source. RFC 0037
    // P1 admits the closed look table, so `look: handDrawn` now draws with its
    // disclosure footer; `theme: dark` is outside the closed palette table and
    // stays rejected in every phase.
    EXPECT_FALSE(draws(with("  theme: dark\n"), kWide));
    EXPECT_TRUE(draws(with("  look: handDrawn\n"), kWide));
    // RFC 0044 admits exactly `layout: elk`; any other engine stays fallback.
    EXPECT_TRUE(draws(with("  layout: elk\n"), kWide));
    EXPECT_FALSE(draws(with("  layout: dagre\n"), kWide));
    EXPECT_FALSE(draws(with("  securityLevel: strict\n"), kWide));
    EXPECT_FALSE(draws(with("  flowchart:\n    curve: linear\n"), kWide));
    EXPECT_FALSE(draws(with("  logLevel: 7\n"), kWide));
    EXPECT_FALSE(draws(with("  logLevel: info\n"), kWide));
    EXPECT_FALSE(draws(with("  fontFamily: arial\n"), kWide));
    EXPECT_FALSE(draws(with("  htmlLabels: maybe\n"), kWide));
    EXPECT_FALSE(draws(with("  theme: dark\n  htmlLabels: false\n"), kWide));
    EXPECT_FALSE(draws("---\ndescription: empty config probe\nconfig:\n---\n" + body, kWide));
    // Duplicates, deeper nesting, extra indentation, tabs, CRLF and a second
    // config block all stay whole-source.
    EXPECT_FALSE(draws(with("  htmlLabels: false\n  htmlLabels: true\n"), kWide));
    EXPECT_FALSE(draws(with("    htmlLabels: false\n"), kWide));
    EXPECT_FALSE(draws(with("  a:\n    b:\n      htmlLabels: false\n"), kWide));
    EXPECT_FALSE(draws(with("  flowchart:\n    htmlLabels: false\n  logLevel: 2\n"), kWide));
    EXPECT_FALSE(draws(with("  htmlLabels: false\nconfig:\n  logLevel: 2\n"), kWide));
    EXPECT_FALSE(draws(
        "---\ndescription: tab probe\nconfig:\n\thtmlLabels: false\n---\n" + body, kWide));
    EXPECT_FALSE(draws("---\r\ndescription: crlf probe\r\nconfig:\r\n  htmlLabels: false\r\n"
                       "---\r\n" + body,
                       kWide));
    // A trailing YAML comment on an inert leaf is accepted like State E2b.
    EXPECT_TRUE(draws(with("  htmlLabels: false # note\n"), kWide));
    // The deferred corpus row stays whole-source (no config-free twin).
    EXPECT_FALSE(draws(read_file(kCases /
                                 "requirement/nightly-check/"
                                 "edge-labels-correctly-when-flowchart-htmllabels-is-false-default.mmd"),
                       kWide));
    // The footer text is part of the published contract, not just presence.
    const auto footer = capture(with("  logLevel: 2\n  fontFamily: courier\n"), "mermaid", kWide);
    EXPECT_NE(footer.text.find("Requirement native projection v1"), std::string::npos);
    EXPECT_NE(footer.text.find("config.logLevel=2 -> logger only; not applied"), std::string::npos);
    EXPECT_NE(footer.text.find("config.fontFamily=\"courier\" -> terminal font; not applied"),
              std::string::npos);
}
