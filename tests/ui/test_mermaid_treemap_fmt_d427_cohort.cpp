// D-427 treemap `config.treemap.valueFormat` gate.
//
// Selection rationale (TL directive): the body-only census leaves treemap as
// the largest single-family clean seam (6 of 16), and all six share one closed
// config shape — a folded description plus `config:` > `treemap:` >
// `valueFormat:` with a d3-format subset. Expected drop: 6 rows (treemap
// 16 -> 10, global 553 -> 547, formal 1406 -> 1412).
//
// Criterion (semantic, D-412): the leaf is *honoured*, not disclosed — the
// terminal draws numbers, so the source's requested format must appear in the
// labels. The formatter is exact decimal string arithmetic (leaf values are
// non-negative plain decimals; no floating point), mirrored renderer-free by
// tools/mermaid/generate_treemap_fmt_d427_cohort.py, which pins every expected
// label in the ledger. The gate asserts each expected label appears verbatim
// and that the raw unformatted token does not.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treemap-fmt-d427-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP427_SENTINEL";
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
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
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
        if (line == "description:" || line == "config:" || line == "  treemap:") return false;
        if (line == "    valueFormat:" || line == "---") return false;
        if (line == "treemap" || line == "treemap-beta") return false;
    }
    return got.back() == "P427_SENTINEL";
}
std::vector<std::string> string_list(const JsonVal& value) {
    std::vector<std::string> out;
    value.iter([&](JsonVal item) { out.emplace_back(item.as_str()); });
    return out;
}
std::string probe(std::string_view frontmatter_tail, std::string_view body) {
    return "---\ndescription: >-\n  treemap valueFormat probe\n" +
           std::string(frontmatter_tail) + "---\n" + std::string(body);
}
constexpr std::string_view kTree = "treemap-beta\n  \"Root\"\n    \"Leaf\": 1234567\n";
constexpr std::string_view kSmallTree = "treemap-beta\n  \"Root\"\n    \"Leaf\": 0.35\n";
}  // namespace

TEST(MermaidTreemapFormatD427Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treemap_fmt_d427_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-427");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    const auto formats = root.get("counts").get("formats");
    for (const auto& [spec, count] :
         {std::pair{"'$0,0'", 2}, std::pair{"'.1%'", 1}, std::pair{"'.2f'", 1},
          std::pair{"'$.2f'", 1}, std::pair{"'$,.2f'", 1}})
        EXPECT_EQ(formats.get_int(spec), count) << spec;
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "0b00a3fa1d3d112f066ce7674d5e0c1fbb5a2c474a22855b6f853046466aafaa");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));

    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        EXPECT_FALSE(row.get_string("value_format").empty()) << rel;
        EXPECT_GE(string_list(row.get("expected_labels")).size(), 4u) << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 6u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidTreemapFormatD427Cohort, RowsHonourTheFormatter) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto states = load_states(row.get("expected_states"));
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        const auto text = capture(source, "mermaid", kWide).text;
        for (const auto& label : string_list(row.get("expected_labels")))
            EXPECT_NE(text.find(label), std::string::npos) << rel << " missing label " << label;
        // A formatted label means the raw token is gone wherever the two differ.
        if (rel.find("8-should-handle-dollar-value-formatting") != std::string::npos) {
            EXPECT_EQ(text.find("700000"), std::string::npos) << rel << " raw token still drawn";
            EXPECT_NE(text.find("$700,000"), std::string::npos) << rel;
        }
        ++checked;
    });
    EXPECT_EQ(checked, 6);
}

TEST(MermaidTreemapFormatD427Cohort, ClosedGrammarAndGateBattery) {
    // Positives: the five corpus specs, each with its exact formatted label.
    const auto expect_label = [](std::string_view tail, std::string_view body,
                                 std::string_view expected) {
        const auto source = probe(tail, body);
        ASSERT_TRUE(draws(source, kWide)) << tail;
        ASSERT_TRUE(native_frame(source, kWide)) << tail;
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(expected), std::string::npos)
            << tail << " expected " << expected;
    };
    expect_label("config:\n  treemap:\n    valueFormat: '$0,0'\n", kTree,
                 "Leaf: $1,234,567");
    expect_label("config:\n  treemap:\n    valueFormat: '.1%'\n", kSmallTree,
                 "Leaf: 35.0%");
    expect_label("config:\n  treemap:\n    valueFormat: '.2f'\n", kTree,
                 "Leaf: 1234567.00");
    expect_label("config:\n  treemap:\n    valueFormat: '$.2f'\n", kTree,
                 "Leaf: $1234567.00");
    expect_label("config:\n  treemap:\n    valueFormat: '$,.2f'\n", kTree,
                 "Leaf: $1,234,567.00");
    // Percentage formatting multiplies by 100 with the requested precision.
    {
        const auto source = probe("config:\n  treemap:\n    valueFormat: '.1%'\n", kSmallTree);
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_TRUE(has_text(source, "Leaf: 35.0%"));
    }
    // Exact decimal rounding carries into the integer part.
    {
        const auto source =
            probe("config:\n  treemap:\n    valueFormat: '.2f'\n",
                  "treemap-beta\n  \"Root\"\n    \"Leaf\": 9.999\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_TRUE(has_text(source, "Leaf: 10.00"));
    }
    // Negatives: every other frontmatter shape and out-of-grammar spec stays
    // whole-source, and the style gate is unchanged.
    for (const auto* tail : {
             "config:\n  theme: dark\n",
             "config:\n  look: neo\n",
             "config:\n  treemap:\n    color: red\n",
             "config:\n  treemap:\n    valueFormat: '2d'\n",
             "config:\n  treemap:\n    valueFormat: '$00,0'\n",
             "config:\n  treemap:\n    valueFormat: '.7f'\n",
             "config:\n  treemap:\n    valueFormat: value\n",
             "config:\n  treemap:\n    valueFormat: '$0,0'\n    valueFormat: '$0,0'\n",
             "config:\n    treemap:\n      valueFormat: '$0,0'\n",
             "treemap:\n  valueFormat: '$0,0'\n",
         }) {
        const auto source = probe(tail, kTree);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    for (const auto* body : {"treemap-beta\n  \"Root\"\n    \"Leaf\": 1:::red\n",
                             "treemap-beta\n  \"Root\"\n    \"Leaf\": 1\n  classDef red "
                             "fill:#ff0000;\n"}) {
        EXPECT_FALSE(draws(std::string(body), kWide)) << body;
        EXPECT_FALSE(draws(probe("config:\n  treemap:\n    valueFormat: '$0,0'\n", body),
                           kWide))
            << body << " (format does not bypass the style gate)";
    }
}
