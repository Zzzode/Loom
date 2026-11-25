// D-470 round 809 durable projection-draw gate. Public API only: enforces
// tools/mermaid/d470-projection-draws-cohort.json against the live renderer
// and the pinned manifest. Each row must render its whole source natively at
// 240, keep the exact ordinary-source fallback at 40, restore byte-exactly,
// and carry exactly the disclosure its ledger row names (theme palette,
// root htmlLabels, or none).
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/d470-projection-draws-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1200;
constexpr const char* kSentinel = "D470_COHORT_SENTINEL";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\n" + kSentinel;
}

struct Capture {
    std::vector<std::string> chars;
    std::string text;
};

Capture capture(const std::string& source, const char* language, int width) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    Capture out;
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

JsonDoc parse_file(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}

std::string footer_text(const JsonVal& footer) {
    if (footer.is_obj() && footer.get("theme").is_str())
        return "config.theme=" + std::string(footer.get_string("theme")) +
               " -> colour palette; not applied";
    if (footer.is_obj() && footer.get("html_labels").is_bool() && footer.get("html_labels").as_bool())
        return "config.htmlLabels=true -> native text labels; not applied";
    return {};
}
}  // namespace

TEST(MermaidD470ProjectionDrawsCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_file(kLedger);
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_d470_projection_draws_cohort/v1");
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_FALSE(root.get("review").get_string("reviewer_agent").empty());
    EXPECT_EQ(root.get("selection").get_int("rows"), 18);
    std::size_t rows = 0, with_footer = 0;
    root.get("rows").iter([&](const JsonVal row) {
        EXPECT_FALSE(row.get_string("cause").empty()) << row.get_string("path");
        if (!footer_text(row.get("footer")).empty()) ++with_footer;
        ++rows;
    });
    EXPECT_EQ(rows, 18u);
    EXPECT_EQ(with_footer, 13u);  // twelve redux rows plus the forest palette row
}

TEST(MermaidD470ProjectionDrawsCohort, RowsAreManifestPinnedAndDrawAsReviewed) {
    auto ledger = parse_file(kLedger);
    auto manifest = parse_file(kManifest);
    std::map<std::string, std::string> manifest_sha;
    manifest.root().get("fixtures").iter([&](const JsonVal fixture) {
        manifest_sha.emplace(fixture.get_string("path"), fixture.get_string("sha256"));
    });
    std::size_t checked = 0;
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto path = row.get_string("path");
        SCOPED_TRACE(path);
        const auto source = read_file(kCases / path);
        EXPECT_EQ(loom::utils::crypto::sha256(source), row.get_string("sha256"));
        EXPECT_EQ(manifest_sha.at(path), row.get_string("manifest_sha256"));
        const auto wide = capture(source, "mermaid", kWide);
        const auto restored = capture(source, "mermaid", kWide);
        const auto narrow = capture(source, "mermaid", kNarrow);
        const auto ordinary_wide = capture(source, "text", kWide);
        const auto ordinary_narrow = capture(source, "text", kNarrow);
        ASSERT_FALSE(wide.chars.empty());
        EXPECT_NE(wide.chars, ordinary_wide.chars) << "whole source fell back at 240";
        EXPECT_EQ(wide.chars, restored.chars) << "restore changed the art";
        EXPECT_EQ(narrow.chars, ordinary_narrow.chars) << "narrow width is not the exact fallback";
        const auto footer = footer_text(row.get("footer"));
        if (footer.empty())
            EXPECT_EQ(wide.text.find("not applied"), std::string::npos)
                << "row carries an unexpected disclosure footer";
        else
            EXPECT_NE(wide.text.find(footer), std::string::npos)
                << "row is missing its disclosure footer";
        EXPECT_NE(wide.text.find(kSentinel), std::string::npos) << "following content disappeared";
        ++checked;
    });
    EXPECT_EQ(checked, 18u);
}
