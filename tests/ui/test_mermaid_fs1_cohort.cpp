// RFC 0031 FS-1 durable cohort gate. Public API only (no module declaration):
// it enforces the pre-registration ledger at tools/mermaid/fs1-cohort.json
// against the live renderer and the pinned manifest. Every admitted draw row
// must render its backtick Markdown node/edge labels natively (bold/underline
// spans, markup stripped) at 240 columns, keep the whole source at 40, and
// carry the non-terminal config.htmlLabels disclosure footer; its frontmatter
// -free body paints natively at every width without a footer. Every blocked
// row keeps the exact source surface at both tracks and all three widths.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs1-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS1_COHORT_SENTINEL";
}

struct CellCapture {
    std::vector<std::string> chars;
    std::vector<bool> bold;
    std::vector<bool> underlined;
    std::string text;
};

CellCapture capture(const std::string& source, const char* language, int width) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.chars.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x) {
            const auto& pixel = screen.PixelAt(x, y);
            out.chars.push_back(pixel.character);
            out.bold.push_back(pixel.bold);
            out.underlined.push_back(pixel.underlined);
            out.text += pixel.character;
        }
    return out;
}

bool same_surface(const CellCapture& a, const CellCapture& b) {
    return a.chars == b.chars;
}

JsonDoc parse_ledger() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse ledger");
    return std::move(*parsed);
}

struct Row {
    std::string path;
    bool draw;
    bool html_labels;
};

std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({value.get_string("path"),
                        value.get_string("prediction") == "predicted-draw",
                        value.get("html_labels").as_bool()});
    });
    return rows;
}

// Drop the frontmatter envelope: everything after the closing column-0 fence.
std::string body_only(const std::string& source) {
    const std::string fence = "\n---\n";
    const auto close = source.find(fence, 4);
    if (close == std::string::npos) return source;
    return source.substr(close + fence.size());
}

std::string description_token(const std::string& source) {
    std::istringstream in(source);
    std::string line;
    bool in_frontmatter = false, seen_description = false;
    while (std::getline(in, line)) {
        if (line == "---") {
            if (!in_frontmatter) {
                in_frontmatter = true;
                continue;
            }
            break;
        }
        if (in_frontmatter) {
            if (line.starts_with("description:")) {
                seen_description = true;
                continue;
            }
            if (seen_description && line.size() > 2 && line[0] == ' ' && line[1] == ' ')
                return line.substr(2);
        }
    }
    return {};
}

std::string footer_line(bool html_labels) {
    return "config.htmlLabels=" + std::string(html_labels ? "true" : "false") +
           " -> native text labels; not applied";
}
}  // namespace

TEST(MermaidFs1Cohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs1_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 7);
    EXPECT_EQ(counts.get_int("predicted_draw"), 2);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 5);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 3u);
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-blocked")
            EXPECT_TRUE(vocabulary.contains(row.get("blocker").get_string("kind")));
        else
            EXPECT_TRUE(row.get("blocker").is_null());
    });
}

TEST(MermaidFs1Cohort, RowsPinFixtureBytesAndPriorStatus) {
    auto manifest = json::parse(read_file(kManifest));
    ASSERT_TRUE(manifest.has_value());
    std::map<std::string, JsonVal> manifest_rows;
    manifest->root().get("fixtures").iter([&](const JsonVal fixture) {
        manifest_rows.emplace(fixture.get_string("path"), fixture);
    });
    for (const auto& row : read_rows(parse_ledger().root())) {
        const auto found = manifest_rows.find(row.path);
        ASSERT_NE(found, manifest_rows.end()) << row.path;
        const auto bytes = read_file(kCases / row.path);
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), found->second.get_string("sha256"))
            << row.path;
        EXPECT_EQ(found->second.get("raw").get_string("expected"), "source") << row.path;
    }
}

TEST(MermaidFs1Cohort, WholeSourceFramesFooterAndMarkupMatchPredictions) {
    int drawn = 0, blocked = 0;
    for (const auto& row : read_rows(parse_ledger().root())) {
        const auto source = read_file(kCases / row.path);
        const auto wide = capture(source, "mermaid", kWide);
        const auto plain_wide = capture(source, "text", kWide);
        const auto narrow = capture(source, "mermaid", kNarrow);
        const auto plain_narrow = capture(source, "text", kNarrow);
        const auto restored = capture(source, "mermaid", kWide);
        const bool wide_native = !same_surface(wide, plain_wide);
        const bool narrow_native = !same_surface(narrow, plain_narrow);
        if (row.draw) {
            ++drawn;
            ASSERT_TRUE(wide_native) << "registered draw did not render: " << row.path;
            EXPECT_FALSE(narrow_native) << "whole source must keep source at 40: " << row.path;
            ASSERT_TRUE(same_surface(restored, wide))
                << "wide/narrow/wide restoration changed pixels: " << row.path;
            // Disclosure footer for the non-terminal key.
            EXPECT_NE(wide.text.find(footer_line(row.html_labels)), std::string::npos) << row.path;
            // Markdown markup is stripped on the native surface.
            EXPECT_EQ(wide.text.find("**"), std::string::npos)
                << "literal bold markers leaked: " << row.path;
            // At least one bold and one underlined span cell ("**bold**" and
            // "*italic*" appear in both admitted fixtures).
            const bool has_bold = std::ranges::any_of(wide.bold, std::identity{});
            const bool has_underline = std::ranges::any_of(wide.underlined, std::identity{});
            EXPECT_TRUE(has_bold) << row.path;
            EXPECT_TRUE(has_underline) << row.path;
        } else {
            ++blocked;
            EXPECT_FALSE(wide_native) << "registered blocked row rendered: " << row.path;
            EXPECT_FALSE(narrow_native) << "registered blocked row rendered at 40: " << row.path;
            EXPECT_TRUE(same_surface(restored, plain_wide)) << row.path;
        }
    }
    EXPECT_EQ(drawn, 2);
    EXPECT_EQ(blocked, 5);
}

TEST(MermaidFs1Cohort, BodyTrackFramesMatchPredictionsWithoutFooter) {
    int drawn = 0, blocked = 0;
    for (const auto& row : read_rows(parse_ledger().root())) {
        const auto body = body_only(read_file(kCases / row.path));
        const auto wide = capture(body, "mermaid", kWide);
        const auto plain_wide = capture(body, "text", kWide);
        const auto narrow = capture(body, "mermaid", kNarrow);
        const auto plain_narrow = capture(body, "text", kNarrow);
        const bool wide_native = !same_surface(wide, plain_wide);
        const bool narrow_native = !same_surface(narrow, plain_narrow);
        if (row.draw) {
            ++drawn;
            ASSERT_TRUE(wide_native) << "registered draw body did not render: " << row.path;
            EXPECT_TRUE(narrow_native) << "draw body must paint at 40: " << row.path;
            // The body carries no frontmatter, hence no disclosure line.
            EXPECT_EQ(wide.text.find("config.htmlLabels"), std::string::npos) << row.path;
            EXPECT_EQ(wide.text.find("**"), std::string::npos) << row.path;
        } else {
            ++blocked;
            EXPECT_FALSE(wide_native) << "registered blocked body rendered: " << row.path;
            EXPECT_FALSE(narrow_native) << "registered blocked body rendered at 40: " << row.path;
        }
    }
    EXPECT_EQ(drawn, 2);
    EXPECT_EQ(blocked, 5);
}

TEST(MermaidFs1Cohort, DescriptionIsNeverRendered) {
    for (const auto& row : read_rows(parse_ledger().root())) {
        if (!row.draw) continue;
        const auto source = read_file(kCases / row.path);
        const auto token = description_token(source);
        if (token.empty()) continue;
        const auto native = capture(source, "mermaid", kWide).text;
        EXPECT_EQ(native.find(token), std::string::npos) << row.path << ": " << token;
    }
}
