// RFC 0031 stadium multiline-label durable cohort gate. Public API only:
// enforces tools/mermaid/multiline-stadium-cohort.json against the live
// renderer and the pinned manifest. The single row draws BODY ONLY (its
// whole original is held by the nested-config gate); the body renders at
// 240 and restored while staying source at 40 columns.
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/multiline-stadium-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 800;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nML_STADIUM_SENTINEL";
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
std::string body_after_frontmatter(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidMultilineStadiumCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_multiline_stadium_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("counts").get_int("predicted_draw"), 1);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 0);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](const JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    root.get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
    });
}

TEST(MermaidMultilineStadiumCohort, BodyDrawsButWholeOriginalStaysSource) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        const auto body = body_after_frontmatter(whole);
        // Body track: draws wide and restored, source narrow.
        EXPECT_TRUE(draws(body, kWide)) << rel;
        EXPECT_FALSE(draws(body, kNarrow)) << rel;
        const CellCapture wide = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, capture(body, "mermaid", kWide))) << rel;
        ASSERT_FALSE(same_surface(wide, capture(body, "text", kWide))) << rel;
        // All authored break lines survive as separate visible label rows.
        EXPECT_NE(wide.text.find("Twelve"), std::string::npos) << rel;
        // Whole original stays source: top-level htmlLabels=false (RootHtml)
        // plus fontFamily=courier (Font) matches no whole-source profile.
        EXPECT_FALSE(draws(whole, kWide)) << rel;
        EXPECT_FALSE(draws(whole, kNarrow)) << rel;
    });
}
