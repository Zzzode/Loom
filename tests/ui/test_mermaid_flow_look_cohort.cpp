// D-814 durable gate for the reviewed `config.look: handDrawn` disclosure
// cohort. Public API only: every ledger row must render its structure natively
// at 240, keep the exact ordinary-source fallback at 40, restore byte-exactly,
// carry the disclosed notice lines, and pin the official manifest bytes.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/flow-look-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1200;
constexpr const char* kSentinel = "D814_LOOK_SENTINEL";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\n" + kSentinel;
}

struct Capture {
    std::vector<std::string> chars;
    std::vector<std::string> lines;
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
    for (int y = 0; y < kHeight; ++y) {
        std::string line;
        for (int x = 0; x < width; ++x) {
            out.chars.push_back(screen.PixelAt(x, y).character);
            line += screen.PixelAt(x, y).character;
        }
        out.lines.push_back(std::move(line));
        out.text += out.lines.back();
    }
    return out;
}

JsonDoc parse_file(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
}  // namespace

TEST(MermaidFlowLookCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_file(kLedger);
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_flow_look_cohort/v1");
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto reviewer = std::string(root.get("review").get_string("reviewer_agent"));
    EXPECT_FALSE(reviewer.empty());
    EXPECT_EQ(reviewer.find("PENDING"), std::string::npos)
        << "an approved ledger must name its reviewer";
    EXPECT_FALSE(std::string(root.get("review").get_string("date")).empty());
    EXPECT_EQ(root.get("selection").get_int("rows"), 22);
    std::size_t rows = 0, with_seed = 0;
    root.get("rows").iter([&](const JsonVal row) {
        EXPECT_FALSE(row.get_string("cause").empty()) << row.get_string("path");
        EXPECT_FALSE(row.get("notices").is_arr() && row.get("notices").size() == 0u);
        std::size_t notices = 0;
        row.get("notices").iter([&](const JsonVal notice) {
            EXPECT_NE(std::string(notice.as_str()).find("not applied"), std::string::npos);
            ++notices;
        });
        if (notices == 2u) ++with_seed;
        EXPECT_EQ(row.get("expected_states").get_string("narrow_40"), "source_fallback");
        ++rows;
    });
    EXPECT_EQ(rows, 22u);
    EXPECT_EQ(with_seed, 2u);  // the two handDrawnSeed rows
}

TEST(MermaidFlowLookCohort, RowsDrawWithDisclosureAndExactNarrowFallback) {
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
        std::size_t notices = 0;
        row.get("notices").iter([&](const JsonVal notice) {
            EXPECT_NE(wide.text.find(notice.as_str()), std::string::npos)
                << "row is missing its disclosure footer: " << notice.as_str();
            ++notices;
        });
        EXPECT_GE(notices, 1u);
        // Inertness: every art row above the disclosure footer must equal the
        // twin rendered from the same source without the frontmatter block.
        const auto frontmatter_end = source.find("\n---", 3u);
        ASSERT_NE(frontmatter_end, std::string::npos);
        const auto body_source = source.substr(frontmatter_end + 5u);
        const auto twin = capture(body_source, "mermaid", kWide);
        ASSERT_FALSE(twin.lines.empty());
        EXPECT_EQ(twin.text.find("not applied"), std::string::npos)
            << "the config-free twin must not disclose anything";
        EXPECT_NE(twin.chars, capture(body_source, "text", kWide).chars)
            << "the config-free twin fell back";
        std::size_t footer_line = wide.lines.size();
        for (std::size_t y = 0; y < wide.lines.size(); ++y)
            if (wide.lines[y].find("not applied") != std::string::npos) {
                footer_line = y;
                break;
            }
        ASSERT_LT(footer_line, wide.lines.size()) << "footer is not in the frame";
        for (std::size_t y = 0; y < footer_line; ++y)
            EXPECT_EQ(wide.lines[y], twin.lines[y])
                << "art row " << y << " differs from the config-free twin: " << path;
        EXPECT_NE(wide.text.find(kSentinel), std::string::npos) << "following content disappeared";
        ++checked;
    });
    EXPECT_EQ(checked, 22u);
}
