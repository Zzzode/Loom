// RFC 0031 FS-1 group-title durable cohort gate. Public API only (no module
// declaration): enforces tools/mermaid/fs1g-cohort.json against the live
// renderer and the pinned manifest. The single draw row must render its
// backtick Markdown subgraph heading natively (bold span, markup stripped)
// at every width on both the whole-source and frontmatter-free body tracks,
// with no non-terminal config disclosure footer (its only config key is the
// CSS-only fontFamily).
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs1g-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS1G_COHORT_SENTINEL";
}

struct CellCapture {
    std::vector<std::string> chars;
    std::vector<bool> bold;
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
};

std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({value.get_string("path"),
                        value.get_string("prediction") == "predicted-draw"});
    });
    return rows;
}

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
}  // namespace

TEST(MermaidFs1gCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs1g_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 1);
    EXPECT_EQ(counts.get_int("predicted_draw"), 1);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 0);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 2u);
}

TEST(MermaidFs1gCohort, RowPinsFixtureBytesAndPriorStatus) {
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

TEST(MermaidFs1gCohort, DrawRowRendersBoldHeadingEveryWidthWithoutFooter) {
    int drawn = 0;
    for (const auto& row : read_rows(parse_ledger().root())) {
        ASSERT_TRUE(row.draw) << "batch cohort holds only the draw row";
        ++drawn;
        const auto source = read_file(kCases / row.path);
        for (const int width : {kWide, kNarrow}) {
            const auto native = capture(source, "mermaid", width);
            const auto plain = capture(source, "text", width);
            ASSERT_FALSE(same_surface(native, plain))
                << "registered draw did not render at " << width << ": " << row.path;
            EXPECT_EQ(native.text.find("**"), std::string::npos)
                << "bold markers leaked at " << width << ": " << row.path;
            // The bold Markdown heading "strong" must carry a bold cell.
            const bool has_bold = std::ranges::any_of(native.bold, std::identity{});
            EXPECT_TRUE(has_bold) << "no bold heading cell at " << width << ": " << row.path;
            // The stripped heading text is visible.
            EXPECT_NE(native.text.find("strong"), std::string::npos) << row.path;
            // fontFamily is the only config key: no disclosure footer.
            EXPECT_EQ(native.text.find("not applied"), std::string::npos)
                << "unexpected disclosure footer at " << width << ": " << row.path;
        }
        const auto wide = capture(source, "mermaid", kWide);
        const auto restored = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, restored)) << "restore changed pixels: " << row.path;
        // The frontmatter-free body draws identically and still without a
        // footer at every width.
        const auto body = body_only(source);
        for (const int width : {kWide, kNarrow}) {
            const auto native = capture(body, "mermaid", width);
            const auto plain = capture(body, "text", width);
            EXPECT_FALSE(same_surface(native, plain))
                << "draw body did not render at " << width << ": " << row.path;
            EXPECT_EQ(native.text.find("not applied"), std::string::npos) << row.path;
        }
    }
    EXPECT_EQ(drawn, 1);
}

TEST(MermaidFs1gCohort, DescriptionIsNeverRendered) {
    for (const auto& row : read_rows(parse_ledger().root())) {
        if (!row.draw) continue;
        const auto token = description_token(read_file(kCases / row.path));
        if (token.empty()) continue;
        const auto text = capture(read_file(kCases / row.path), "mermaid", kWide).text;
        EXPECT_EQ(text.find(token), std::string::npos) << row.path << ": " << token;
    }
}
