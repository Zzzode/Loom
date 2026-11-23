// D-452 treemap classDef/::: admission gate.
//
// Selection rationale: the ten treemap fallbacks whose only blocker is the
// in-body `classDef` declaration plus `:::class` references. The native
// treemap projection is an indented text outline that draws no colour or
// rectangle geometry, so every declaration is inert and disclosed once in the
// footer; the `:::` references render no suffix.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treemap-classdef-d452-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP452_SENTINEL";
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
        if (line == "description:" || line == "config:" || line == "---") return false;
        // A source echo is a classDef node line that is not a disclosure.
        if (line.starts_with("classDef") && line.find("not applied") == std::string::npos)
            return false;
        if (line.find(":::") != std::string::npos) return false;
    }
    return got.back() == "P452_SENTINEL";
}
// The same fixture with the classDef lines removed and every :::class suffix
// stripped from node lines.
std::string class_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string head = source.substr(0, close + 5);
    std::string out;
    const std::string body = source.substr(close + 5);
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        const auto end = eol == std::string::npos ? body.size() : eol;
        std::string line = body.substr(pos, end - pos);
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) first = 0;
        bool drop = line.substr(first).starts_with("classDef");
        if (!drop) {
            const auto marker = line.rfind(":::");
            if (marker != std::string::npos && line.find('"') != std::string::npos)
                line.erase(marker);
        }
        if (!drop) {
            out += line;
            if (eol != std::string::npos) out += '\n';
        }
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return head + out;
}
constexpr std::string_view kBody =
    "treemap-beta\n"
    "  \"Root\"\n"
    "    \"Item A\": 30\n"
    "    \"Item B\": 20\n";
std::string probe(std::string_view extra) {
    return "---\ndescription: >-\n  treemap class probe\n---\n" + std::string(kBody) +
           std::string(extra);
}
}  // namespace

TEST(MermaidTreemapClassdefD452Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treemap_classdef_d452_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-452");
    EXPECT_EQ(root.get("counts").get_int("total"), 10);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("treemap"), 10);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "951d076dbd90cf9108795a9b714ee754ae317bc3fb3cfc8abb985d3005bb7c38");
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
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 10u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidTreemapClassdefD452Cohort, ClassesAreInertAgainstClassFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = class_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        // Rule 34: the 40-column state is measured live, never inherited.
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        std::vector<std::string> art(lines.begin(),
                                     lines.begin() + static_cast<std::ptrdiff_t>(begin));
        while (!art.empty() && art.back().empty()) art.pop_back();
        auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        while (!twin_lines.empty() &&
               (twin_lines.back().empty() || twin_lines.back() == "P452_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the classes changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 10);
}

TEST(MermaidTreemapClassdefD452Cohort, ClosedClassNegatives) {
    // Admitted positives: a defined class applied to a leaf and to a section,
    // plus the classDef-only declaration.
    {
        const auto source = probe("  \"Item A\": 30:::red\n"
                                  "  \"Root\":::red\n"
                                  "  classDef red fill:#ff0000;\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("Treemap native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("classDef red [fill] -> SVG colour; not applied"),
                  std::string::npos);
    }
    {
        const auto source = probe("  classDef red fill:red,stroke:#333,"
                                  "stroke-dasharray:5 5;\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "classDef red [fill,stroke,stroke-dasharray] -> SVG colour; not applied"),
                  std::string::npos);
    }
    for (const auto* extra : {
             // The closed property set, duplicates and value shape.
             "  classDef red fill:red,unknown:1;\n",
             "  classDef red fill:red,fill:blue;\n",
             "  classDef red fill:\"red\";\n",
             "  classDef red fill:red(x);\n",
             "  classDef red;\n",
             "  classDef red fill:red;\n  classDef red fill:blue;\n",
             // Reference shape: undefined, malformed and duplicated markers.
             "  \"Item A\": 30:::missing\n",
             "  \"Item A\": 30:::1bad\n",
             "  \"Item A\": 30:::\n",
             "  \"Item A\": 30:::a:::b\n",
             "  classDef red fill:red;\n  \"Item A\": 30:::red:::red\n",
             // Body directives stay a whole-source gate.
             "  %%{init: {\"theme\": \"dark\"}}%%\n",
         }) {
        const auto source = probe(extra);
        EXPECT_FALSE(draws(source, kWide)) << extra;
        EXPECT_FALSE(native_frame(source, kWide)) << extra;
    }
    // A class-free source keeps no disclosure footer.
    {
        const auto frame = capture(probe(""), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Treemap native projection v1"), std::string::npos);
    }
}
