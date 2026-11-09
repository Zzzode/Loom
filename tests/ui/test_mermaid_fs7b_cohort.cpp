// RFC 0031 FS-7b collapsed-subgraph durable cohort gate. Public API only:
// enforces tools/mermaid/fs7b-collapsed-cohort.json against the live
// renderer and the pinned manifest. Every row draws its BODY at 240 and
// restored; the narrow frame is measured per row (a collapsed graph can be
// narrow enough to draw at 40 columns) and pinned explicitly. The two
// whole-and-body rows also draw the full frontmatter original per their
// pinned states; the handDrawn body-only row keeps its whole original on
// ordinary source behind the look:handDrawn frontmatter gate.
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs7b-collapsed-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nFS7B_SENTINEL";
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
std::array<std::string, 3> load_states(const JsonVal& states) {
    return {states.get_string("wide_240"), states.get_string("narrow_40"),
            states.get_string("restored_240")};
}
}  // namespace

TEST(MermaidFs7bCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs7b_collapsed_subgraph_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("counts").get_int("predicted_draw"), 3);
    EXPECT_EQ(root.get("counts").get_int("predicted_blocked"), 0);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 2);
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

TEST(MermaidFs7bCohort, RenderStatesMatchLedger) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        const auto body = body_after_frontmatter(whole);
        const auto whole_states = load_states(row.get("expected_states"));
        const auto body_states = load_states(row.get("expected_body_states"));

        // Body track against its pinned triple (wide / narrow / restored).
        EXPECT_EQ(draws(body, kWide), body_states[0] == "drawn") << rel << " body wide";
        EXPECT_EQ(draws(body, kNarrow), body_states[1] == "drawn") << rel << " body narrow";
        const CellCapture body_wide = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(body_wide, capture(body, "mermaid", kWide)))
            << rel << " body restored determinism";
        ASSERT_FALSE(same_surface(body_wide, capture(body, "text", kWide)))
            << rel << " body wide is a native frame";

        // Whole original against its pinned triple.
        EXPECT_EQ(draws(whole, kWide), whole_states[0] == "drawn") << rel << " whole wide";
        EXPECT_EQ(draws(whole, kNarrow), whole_states[1] == "drawn") << rel << " whole narrow";
        const CellCapture whole_wide = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(whole_wide, capture(whole, "mermaid", kWide)))
            << rel << " whole restored determinism";
    });
}
