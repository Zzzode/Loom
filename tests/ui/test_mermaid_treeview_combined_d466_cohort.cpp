// D-466 tree-view combined-annotation cohort gate.
//
// One tree-view original places a `:::class` annotation between the label and
// the existing `icon(...)`/`## description` suffixes on the same node. The
// terminal draws no highlight colour, so the class token is validated,
// dropped and disclosed; the icon and description suffixes stay verbatim.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treeview-combined-d466-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 400;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nS466_SENTINEL";
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
}  // namespace

TEST(MermaidTreeviewCombinedD466Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treeview_combined_d466_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-466");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("tree-view"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto manifest = parse_doc(kManifest);
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  row.get_string("sha256"))
            << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 1u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidTreeviewCombinedD466Cohort, AdmittedRowDrawsAndPinsItsStates) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto states = row.get("expected_states");
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto wide = capture(source, "mermaid", kWide);
        // Real re-entry: the narrow width renders whole source in between.
        const auto narrow = capture(source, "mermaid", kNarrow);
        EXPECT_TRUE(same_surface(narrow, capture(source, "text", kNarrow)))
            << rel << " narrow render is not the whole source";
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        EXPECT_NE(wide.text.find("TreeView native projection v1"), std::string::npos) << rel;
        EXPECT_NE(wide.text.find("body class annotations -> highlight colour; not applied"),
                  std::string::npos)
            << rel << " class disclosure";
        row.get("expected_excerpts").iter([&](JsonVal excerpt) {
            const auto text = excerpt.as_str();
            EXPECT_NE(wide.text.find(text), std::string::npos) << rel << text;
        });
        // The dropped class token never reaches the art.
        EXPECT_EQ(wide.text.find(":::highlight"), std::string::npos) << rel;
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidTreeviewCombinedD466Cohort, CombinedAnnotationGrammarStaysClosed) {
    // Paired positives: the admitted placements draw.
    EXPECT_TRUE(draws("treeView-beta\n  App.tsx :::highlight icon(fa:bell) ## main\n", kWide))
        << "class before icon and description";
    EXPECT_TRUE(draws("treeView-beta\n  App.tsx :::highlight\n", kWide)) << "trailing class";
    {
        // The D-457 trailing form keeps its exact transcription: the label is
        // untouched and only the class token is dropped.
        const auto text = capture("treeView-beta\n  App.tsx :::highlight\n", "mermaid", kWide).text;
        EXPECT_NE(text.find("App.tsx"), std::string::npos);
        EXPECT_EQ(text.find(":::highlight"), std::string::npos);
        EXPECT_NE(text.find("body class annotations -> highlight colour; not applied"),
                  std::string::npos);
    }
    EXPECT_TRUE(draws("treeView-beta\n  App.tsx :::highlight ## main\n", kWide))
        << "class before a description";
    // Paired negatives: malformed placements stay whole source.
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::1red icon(fa:bell)\n", kWide))
        << "non-identifier class before an icon";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::a :::b icon(fa:bell)\n", kWide))
        << "two class tokens";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx:::highlight icon(fa:bell)\n", kWide))
        << "class without the separating space";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::highlight stray\n", kWide))
        << "class followed by stray text";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx icon(fa:bell) :::highlight\n", kWide))
        << "class after an icon";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx ## main :::highlight\n", kWide))
        << "class after a description";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::highlight ##\n", kWide))
        << "empty description after a class";
    EXPECT_FALSE(draws("treeView-beta\n  App.tsx :::highlight icon(\n", kWide))
        << "unclosed icon after a class";
}
