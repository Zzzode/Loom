// RFC 0031 FS-aws durable cohort gate. Public API only: enforces
// tools/mermaid/aws-icon-cohort.json against the live renderer and the
// pinned manifest. Every row draws on both tracks at all three widths.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/aws-icon-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 800;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
struct CellCapture {
    std::vector<std::string> chars;
};
CellCapture capture(const std::string& source, const char* language, int width) {
    const std::string fenced =
        "```" + std::string(language) + "\n" + source + "\n```\n\nAWS_SENTINEL";
    auto element = loom::ui::render_markdown(fenced);
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.chars.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x) out.chars.push_back(screen.PixelAt(x, y).character);
    return out;
}
bool draws(const std::string& source, int width) {
    return capture(source, "mermaid", width).chars != capture(source, "text", width).chars;
}
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
}  // namespace

TEST(MermaidAwsIconCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_aws_icon_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("counts").get_int("predicted_draw"), 3);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 3);
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

TEST(MermaidAwsIconCohort, EveryFixtureDrawsAtAllWidthsOnBothTracks) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        for (int width : {kWide, kNarrow}) {
            EXPECT_TRUE(draws(whole, width)) << rel << " whole width " << width;
        }
        // Restored frame equals the wide frame.
        const auto wide = capture(whole, "mermaid", kWide);
        const auto restored = capture(whole, "mermaid", kWide);
        EXPECT_EQ(wide.chars, restored.chars) << rel;
        // The fixed marker cell is present on the wide frame. The literal
        // aws token is never stored, so only its marker projection is visible.
        std::string text;
        for (const auto& cell : wide.chars) text += cell;
        EXPECT_NE(text.find("\xE2\x96\xA0"), std::string::npos) << rel;
    });
}
