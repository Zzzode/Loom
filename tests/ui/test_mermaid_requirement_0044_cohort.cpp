// RFC 0044 requirement `layout: elk` admission cohort gate.
//
// Public API only, mirroring tools/mermaid/requirement-0044-cohort.json against
// the live renderer. Every row's frame must equal its drawn twin's frame with
// the twin's own footer block removed and the source footer block appended —
// the twin is footer-free (no config block) for 20 rows and footer-prefixed
// (`htmlLabels: false`) for 3. The table and grammar batteries are an
// independent oracle: expected verdicts and footer strings are hardcoded
// literals, never derived from the ledger or the renderer's own tables. The
// RFC 0037 gate remains the 120-row drawn-frame regression oracle.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/requirement-0044-cohort.json";
const fs::path kLedger0037 =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/requirement-0037-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "Requirement native projection v1";
constexpr std::string_view kBody =
    "requirementDiagram\n"
    "  requirement test_req {\n"
    "    id: 1\n"
    "    text: The test text.\n"
    "    risk: high\n"
    "    verifymethod: test\n"
    "  }\n";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nRQ44_SENTINEL";
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
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "layout:" || line == "  layout:") return false;
    }
    return got.back() == "RQ44_SENTINEL";
}
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
std::string layout_line(std::string_view value) {
    return "config.layout=" + std::string(value) + " -> external layout engine; not applied";
}
std::string html_line(std::string_view value) {
    return "config.htmlLabels=" + std::string(value) + " -> native text labels; not applied";
}
std::string look_line(std::string_view value) {
    return "config.look=" + std::string(value) + " -> source node style; not applied";
}
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  requirement layout boundary probe\n" +
           std::string(config) + "---\n" + std::string(kBody);
}
std::string config_probe(std::string_view config) {
    return probe(std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
// The footer block sits immediately above the fenced block's trailing blank row
// as one blank separator plus the ledger lines.
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    std::vector<std::string> expected{"", std::string(kHeader)};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "RQ44_SENTINEL") << label;
    ASSERT_TRUE(lines[lines.size() - 2u].empty()) << label << " (trailing blank row)";
    const auto begin = lines.size() - 2u - expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(lines[begin + i], expected[i]) << label << " footer line " << i;
}
std::vector<std::string> string_list(const JsonVal& value) {
    std::vector<std::string> out;
    value.iter([&](JsonVal item) { out.emplace_back(item.as_str()); });
    return out;
}
}  // namespace

TEST(MermaidRequirement0044Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_requirement_0044_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 44);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("originals"), 172);
    EXPECT_EQ(counts.get_int("drawn"), 144);
    EXPECT_EQ(counts.get_int("admitted"), 23);
    EXPECT_EQ(counts.get_int("remaining"), 5);
    const auto review = root.get("review");
    EXPECT_EQ(review.get_string("verdict"), "approved");
    EXPECT_FALSE(review.get_string("reviewer_agent").empty());

    // The admission tables are the literals the battery below hardcodes.
    const auto tables = root.get("admission").get("theme_tables");
    EXPECT_EQ(string_list(tables.get("layout")), (std::vector<std::string>{"elk"}));
    EXPECT_EQ(string_list(tables.get("htmlLabels")),
              (std::vector<std::string>{"false", "true"}));
    EXPECT_EQ(string_list(tables.get("look")),
              (std::vector<std::string>{"classic", "handDrawn", "neo"}));

    const auto selection = root.get("selection");
    std::set<std::string> pinned;
    selection.get("path_pin").iter(
        [&](JsonVal value) { pinned.insert(std::string(value.as_str())); });
    EXPECT_EQ(pinned.size(), 23u);
    EXPECT_EQ(selection.get("twin_kinds").get_int("footer-free"), 20);
    EXPECT_EQ(selection.get("twin_kinds").get_int("footer-prefix"), 3);

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });

    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> admitted;
    std::map<std::string, int> kinds;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        admitted.insert(rel);
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        auto footer = string_list(row.get("expected_footer_lines"));
        ASSERT_TRUE(footer.size() == 2u || footer.size() == 3u) << rel;
        EXPECT_EQ(footer.front(), kHeader) << rel;
        EXPECT_EQ(footer.back(), layout_line("elk")) << rel;
        if (footer.size() == 3u) EXPECT_EQ(footer[1], html_line("false")) << rel;
        const auto twin = row.get("twin");
        const auto kind = twin.get_string("kind");
        EXPECT_TRUE(kind == "footer-free" || kind == "footer-prefix") << rel;
        ++kinds[kind];
        const std::string twin_rel = twin.get_string("path");
        ASSERT_NE(twin_rel, rel) << "self twin";
        ASSERT_TRUE(fs::exists(kCases / twin_rel)) << twin_rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / twin_rel)),
                  twin.get_string("sha256"))
            << rel << " twin pin";
        EXPECT_EQ(frontmatter_stripped(read_file(kCases / rel)),
                  frontmatter_stripped(read_file(kCases / twin_rel)))
            << rel << " body equivalence";
        // The twin must be drawn: "both fall back equal" is not inertness.
        EXPECT_TRUE(draws(read_file(kCases / twin_rel), kWide)) << twin_rel;
        auto twin_footer = string_list(twin.get("footer_lines"));
        if (kind == "footer-free") {
            EXPECT_TRUE(twin_footer.empty()) << rel;
        } else {
            ASSERT_EQ(twin_footer.size() + 1u, footer.size()) << rel;
            for (std::size_t i = 0; i < twin_footer.size(); ++i)
                EXPECT_EQ(twin_footer[i], footer[i]) << rel << " footer prefix " << i;
            EXPECT_EQ(twin_footer.back(), html_line("false")) << rel;
            EXPECT_EQ(footer.back(), layout_line("elk")) << rel;
        }
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(admitted, pinned);
    EXPECT_EQ(kinds["footer-free"], 20);
    EXPECT_EQ(kinds["footer-prefix"], 3);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));

    // Sibling <-> RFC 0037 ledger: every admitted row is a `layout: elk`
    // exclusion there, the sets are disjoint, and the exclusion count holds.
    const auto ledger0037 = parse_doc(kLedger0037);
    std::set<std::string> rows0037, excluded_elk;
    int elk_excluded = 0;
    ledger0037.root().get("rows").iter(
        [&](JsonVal row) { rows0037.insert(row.get_string("path")); });
    ledger0037.root().get("excluded").iter([&](JsonVal entry) {
        if (entry.get_string("class") != "layout: elk") return;
        ++elk_excluded;
        excluded_elk.insert(entry.get_string("path"));
    });
    EXPECT_EQ(elk_excluded, 24);
    for (const auto& rel : admitted) {
        EXPECT_TRUE(excluded_elk.contains(rel)) << rel;
        EXPECT_FALSE(rows0037.contains(rel)) << rel;
    }
}

TEST(MermaidRequirement0044Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = row.get("twin");
        const auto twin_source = read_file(kCases / twin.get_string("path"));
        const auto states = load_states(row.get("expected_states"));

        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // Compose: twin frame, minus the twin's own footer block, plus the
        // source footer block. The footer block is a blank separator, the
        // header and the leaf lines.
        auto expected = trimmed_lines(capture(twin_source, "mermaid", kWide), kWide);
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "RQ44_SENTINEL") << rel;
        expected.pop_back();
        std::size_t tail_blanks = 0;
        while (!expected.empty() && expected.back().empty()) {
            expected.pop_back();
            ++tail_blanks;
        }
        // `footer_lines` is the twin's full notice block (header + leaves).
        const auto twin_footer = string_list(twin.get("footer_lines"));
        if (!twin_footer.empty()) {
            std::vector<std::string> twin_block{""};
            twin_block.insert(twin_block.end(), twin_footer.begin(), twin_footer.end());
            ASSERT_GE(expected.size(), twin_block.size()) << rel;
            const auto begin = expected.size() - twin_block.size();
            for (std::size_t i = 0; i < twin_block.size(); ++i)
                ASSERT_EQ(expected[begin + i], twin_block[i]) << rel << " twin footer " << i;
            expected.resize(begin);
        }
        expected.emplace_back();
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        for (std::size_t i = 0; i < tail_blanks; ++i) expected.emplace_back();
        expected.emplace_back("RQ44_SENTINEL");
        const auto got = trimmed_lines(first, kWide);
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        ++checked;
    });
    EXPECT_EQ(checked, 23);
}

TEST(MermaidRequirement0044Cohort, ClosedTableAndGrammarBattery) {
    const auto elk = config_probe("  layout: elk\n");
    ASSERT_TRUE(draws(elk, kWide));
    ASSERT_TRUE(native_frame(elk, kWide));
    EXPECT_TRUE(has_text(elk, std::string(kHeader)));
    EXPECT_TRUE(has_text(elk, layout_line("elk")));
    // Scalar normalization: a trailing comment and a quoted value still read
    // as `elk` through requirement_inert_scalar.
    for (const auto* form : {"  layout: elk # note\n", "  layout: \"elk\"\n"}) {
        const auto source = config_probe(form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        EXPECT_TRUE(has_text(source, layout_line("elk"))) << form;
    }
    // Composition order: existing leaves first, layout last.
    const auto html_elk = config_probe("  htmlLabels: false\n  layout: elk\n");
    ASSERT_TRUE(draws(html_elk, kWide));
    {
        const auto text = capture(html_elk, "mermaid", kWide).text;
        const auto html_at = text.find(html_line("false"));
        const auto layout_at = text.find(layout_line("elk"));
        ASSERT_NE(html_at, std::string::npos);
        ASSERT_NE(layout_at, std::string::npos);
        EXPECT_LT(html_at, layout_at);
    }
    const auto look_elk = config_probe("  look: neo\n  layout: elk\n");
    ASSERT_TRUE(draws(look_elk, kWide));
    {
        const auto text = capture(look_elk, "mermaid", kWide).text;
        ASSERT_NE(text.find(look_line("neo")), std::string::npos);
        EXPECT_LT(text.find(look_line("neo")), text.find(layout_line("elk")));
    }
    // Closed table and shape: every other value or position rejects.
    for (const auto* form : {"  layout: dagre\n", "  layout: elk-layout\n",
                             "  layout: ELK\n", "  layout:\n", "  layout: elk extra\n",
                             "    layout: elk\n", "   layout: elk\n",
                             "  layout: elk\n  layout: elk\n", "  layout\n",
                             "  securityLevel: strict\n",
                             "  flowchart:\n    htmlLabels: false\n"}) {
        EXPECT_FALSE(draws(config_probe(form), kWide)) << form;
    }
    // The envelope parser trims a key's surrounding spaces, so `layout : elk`
    // at indent 2 is the same leaf as `layout: elk` (pre-existing behavior for
    // every admitted leaf).
    {
        const auto spaced = config_probe("  layout : elk\n");
        ASSERT_TRUE(draws(spaced, kWide));
        EXPECT_TRUE(has_text(spaced, layout_line("elk")));
    }
    EXPECT_FALSE(draws(probe("layout: elk\n"), kWide)) << "column-0 layout";
    EXPECT_FALSE(draws(probe("config:\n"), kWide)) << "empty config";
    EXPECT_FALSE(draws(probe("config:\n  layout: elk\nconfig:\n  layout: elk\n"), kWide))
        << "second config";
    // The nested config.flowchart map stays outside the whitelist.
    EXPECT_FALSE(draws(probe("config:\n  layout: elk\n  flowchart:\n    htmlLabels: false\n"),
                       kWide));
}

TEST(MermaidRequirement0044Cohort, ExistingLeafBatteriesUnchanged) {
    // The five pre-existing leaves keep their verdicts and line order.
    struct Case {
        const char* config;
        std::vector<std::string> footer;
    };
    const std::vector<Case> cases = {
        {"  logLevel: 2\n", {"config.logLevel=2 -> logger only; not applied"}},
        {"  fontFamily: courier\n",
         {"config.fontFamily=\"courier\" -> terminal font; not applied"}},
        {"  htmlLabels: false\n", {html_line("false")}},
        {"  look: handDrawn\n", {look_line("handDrawn")}},
        {"  theme: redux-dark-color\n",
         {"config.theme=redux-dark-color -> colour palette; not applied"}},
        {"  logLevel: 1\n  htmlLabels: true\n  theme: neo\n",
         {"config.logLevel=1 -> logger only; not applied", html_line("true"),
          "config.theme=neo -> colour palette; not applied"}},
    };
    for (const auto& item : cases) {
        const auto source = config_probe(item.config);
        ASSERT_TRUE(draws(source, kWide)) << item.config;
        expect_footer(source, item.footer, item.config);
    }
    for (const auto* form : {"  logLevel: 7\n", "  logLevel: info\n",
                             "  fontFamily: arial\n", "  htmlLabels: maybe\n",
                             "  look: fancy\n", "  theme: jazz\n"}) {
        EXPECT_FALSE(draws(config_probe(form), kWide)) << form;
    }
    // A description-only envelope still draws without a footer.
    const auto plain = probe("");
    ASSERT_TRUE(draws(plain, kWide));
    EXPECT_FALSE(has_text(plain, std::string(kHeader)));
}

TEST(MermaidRequirement0044Cohort, LiteralPartition) {
    // Literal arithmetic against the filesystem, not a recomputation from the
    // ledger's own lists.
    int files = 0;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "requirement"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++files;
    EXPECT_EQ(files, 172);

    const auto ledger = parse_doc(kLedger);
    const auto counts = ledger.root().get("counts");
    EXPECT_EQ(counts.get_int("drawn") + counts.get_int("admitted") +
                  counts.get_int("remaining"),
              172);
    const auto census = ledger.root().get("selection").get("census");
    EXPECT_EQ(census.get_int("rfc0037"), 120);
    EXPECT_EQ(census.get_int("f0"), 8);
    EXPECT_EQ(census.get_int("base"), 4);

    std::set<std::string> admitted;
    ledger.root().get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    const auto ledger0037 = parse_doc(kLedger0037);
    const auto ledger_f0 = parse_doc(
        fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/requirement-f0-cohort.json");
    std::set<std::string> drawn;
    ledger0037.root().get("rows").iter(
        [&](JsonVal row) { drawn.insert(row.get_string("path")); });
    EXPECT_EQ(drawn.size(), 120u);
    std::set<std::string> f0_rows;
    ledger_f0.root().get("rows").iter(
        [&](JsonVal row) { f0_rows.insert(row.get_string("path")); });
    EXPECT_EQ(f0_rows.size(), 8u);
    for (const auto& rel : f0_rows) {
        EXPECT_TRUE(drawn.insert(rel).second) << "duplicate " << rel;
        EXPECT_FALSE(admitted.contains(rel)) << rel;
    }
    for (const auto& rel : admitted) EXPECT_FALSE(drawn.contains(rel)) << rel;
    // The four style rows and the twelve pre-existing clean rows are the
    // remaining drawn rows the ledgers do not enumerate.
    EXPECT_EQ(static_cast<int>(drawn.size()), 128);
}
