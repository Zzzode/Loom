// RFC 0031 FS-2 durable click-no-op cohort gate. Public API only (no module
// declaration): enforces tools/mermaid/fs2-cohort.json against the live
// renderer and the pinned manifest. Every predicted-draw row must render its
// frontmatter-free body natively at 240 columns with the click bindings
// consumed as non-visual no-ops (no click text/URL leaks into the art) and
// stay on ordinary source fallback at 40 columns; the whole original (whose
// frontmatter the flow envelope does not admit) stays source at every width.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs2-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS2_COHORT_SENTINEL";
}

struct CellCapture {
    std::vector<std::string> chars;
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
            out.chars.push_back(screen.PixelAt(x, y).character);
            out.text += screen.PixelAt(x, y).character;
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
}  // namespace

TEST(MermaidFs2Cohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs2_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 16);
    EXPECT_EQ(counts.get_int("predicted_draw"), 2);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 14);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 1u);
    EXPECT_TRUE(vocabulary.contains("flow-click-style-classdef"));
}

TEST(MermaidFs2Cohort, RowsPinFixtureBytesAndWholeSourceStaysFallback) {
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
        // The whole original keeps ordinary source fallback at every width:
        // its frontmatter (securityLevel+fontFamily) is not an admitted
        // envelope profile.
        for (const int width : {kWide, kNarrow})
            EXPECT_TRUE(same_surface(capture(bytes, "mermaid", width),
                                     capture(bytes, "text", width)))
                << "whole original unexpectedly drew: " << row.path;
    }
}

TEST(MermaidFs2Cohort, DrawBodiesRenderAtWideWithoutClickLeakAndStaySourceAtNarrow) {
    int drawn = 0;
    for (const auto& row : read_rows(parse_ledger().root())) {
        if (!row.draw) continue;
        ++drawn;
        const auto body = body_only(read_file(kCases / row.path));
        // Wide: native, and no binding PAYLOAD leaks into the art. The word
        // "click" itself legitimately appears in the TITLE node label
        // ("(click the nodes below)"), so assert on the inert binding payload
        // (URL schemes, anchors, link targets), never the ordinary word.
        const auto wide = capture(body, "mermaid", kWide);
        ASSERT_FALSE(same_surface(wide, capture(body, "text", kWide)))
            << "registered draw body did not render: " << row.path;
        for (const char* token : {"https://", "mailto:", "notes://", "javascript:",
                                  "#link-clicked", "index.html", "_blank"})
            EXPECT_EQ(wide.text.find(token), std::string::npos)
                << "binding payload '" << token << "' leaked: " << row.path;
        // The six child labels and the title are still painted.
        EXPECT_NE(wide.text.find("anchor test"), std::string::npos) << row.path;
        EXPECT_NE(wide.text.find("script test"), std::string::npos) << row.path;
        // Narrow: the wide fan-out keeps ordinary source fallback.
        EXPECT_TRUE(same_surface(capture(body, "mermaid", kNarrow),
                                 capture(body, "text", kNarrow)))
            << "draw body should stay source at narrow width: " << row.path;
        // Restored wide frame is pixel-identical to the first wide frame.
        EXPECT_TRUE(same_surface(capture(body, "mermaid", kWide), wide))
            << "restore changed pixels: " << row.path;
    }
    EXPECT_EQ(drawn, 2);
}

TEST(MermaidFs2Cohort, BlockedBodiesStaySourceBecauseOfStyleDebt) {
    int blocked = 0;
    for (const auto& row : read_rows(parse_ledger().root())) {
        if (row.draw) continue;
        ++blocked;
        const auto body = body_only(read_file(kCases / row.path));
        // The body carries a classDef/style/class/linkStyle statement that
        // forces fallback independently of the click no-op.
        bool has_style_debt = false;
        {
            std::istringstream in(body);
            std::string line;
            while (std::getline(in, line)) {
                const auto t = line | std::views::drop_while([](char c) {
                                   return c == ' ' || c == '\t';
                               }) |
                               std::ranges::to<std::string>();
                if (t.starts_with("classDef") || t.starts_with("linkStyle") ||
                    t.starts_with("style ") || t.starts_with("style\t")) {
                    has_style_debt = true;
                    break;
                }
                // `class <nodeList> <className>`.
                if (t.starts_with("class ")) {
                    std::istringstream ls(t.substr(6));
                    std::string a, b;
                    if (ls >> a >> b) {
                        has_style_debt = true;
                        break;
                    }
                }
            }
        }
        EXPECT_TRUE(has_style_debt)
            << "ledger blocker disagrees with source: " << row.path;
        for (const int width : {kWide, kNarrow})
            EXPECT_TRUE(same_surface(capture(body, "mermaid", width),
                                     capture(body, "text", width)))
                << "blocked body unexpectedly drew: " << row.path;
    }
    EXPECT_EQ(blocked, 14);
}
