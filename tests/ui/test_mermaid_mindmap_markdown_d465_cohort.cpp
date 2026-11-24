// D-465 mindmap markdown-string and escaped-break cohort gate.
//
// Two mindmap originals were the shaped-tree parser's last body rows: one
// carries the escaped break `&lt;br/>` inside a default-node caption, the
// other uses backtick markdown-string labels carrying `**bold**`/`*italic*`
// spans and a physical line break. The terminal projection keeps the text,
// drops the emphasis (disclosed) and draws one row per explicit line. The
// first row's shaped canvas also exhausted the fixed 1M Work primary, so the
// shaped layout funds its canvas from a bounded canvas-derived pool.
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
import loom.ui.visual.mermaid.testing;
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/mindmap-markdown-d465-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nS465_SENTINEL";
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

TEST(MermaidMindmapMarkdownD465Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_mindmap_markdown_d465_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-465");
    EXPECT_EQ(root.get("counts").get_int("total"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("mindmap"), 2);
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
    EXPECT_EQ(digest_input.size(), 2u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidMindmapMarkdownD465Cohort, AdmittedRowsDrawAndPinTheirStates) {
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
        // Real re-entry: render the narrow width in between.
        static_cast<void>(capture(source, "mermaid", kNarrow));
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        row.get("expected_captions").iter([&](JsonVal caption) {
            const auto text = caption.as_str();
            if (!text.empty()) EXPECT_NE(wide.text.find(text), std::string::npos) << rel << text;
        });
        std::size_t previous = 0;
        row.get("expected_notices").iter([&](JsonVal notice) {
            const auto text = notice.as_str();
            const auto at = wide.text.find(text);
            ASSERT_NE(at, std::string::npos) << rel << " notice: " << text;
            EXPECT_GT(at, previous) << rel << " notice order: " << text;
            previous = at;
        });
        // The emphasis markers themselves never reach the art.
        EXPECT_EQ(wide.text.find("**"), std::string::npos) << rel << " emphasis marker drawn";
        ++checked;
    });
    EXPECT_EQ(checked, 2);
}

TEST(MermaidMindmapMarkdownD465Cohort, MalformedMarkdownAndBreaksStayClosed) {
    // Paired negatives: the wider caption grammar never admits a malformed
    // markdown string or an unknown escape.
    EXPECT_FALSE(draws("mindmap\nid1[`**a*`]\n", kWide)) << "unclosed strong span";
    EXPECT_FALSE(draws("mindmap\nid1[`a*`]\n", kWide)) << "unclosed italic span";
    EXPECT_FALSE(draws("mindmap\nid1[`** a**`]\n", kWide)) << "edge space inside emphasis";
    EXPECT_FALSE(draws("mindmap\nid1[`a* b`]\n", kWide)) << "stray marker";
    EXPECT_FALSE(draws("mindmap\nid1[`a\n\nb`]\n", kWide)) << "blank line inside the label";
    EXPECT_FALSE(draws("mindmap\nid1[`a\nb\n", kWide)) << "unclosed markdown string";
    EXPECT_FALSE(draws("mindmap\nroot((r))\n  A&lt;br&gt;B\n", kWide)) << "unknown escape";
    EXPECT_FALSE(draws("mindmap\nid1[`A&lt;br/>B`]\n", kWide)) << "escape inside a markdown string";
    EXPECT_FALSE(draws("mindmap\nroot((r))\n  A&lt;br/&gt\n", kWide)) << "truncated escape";
    EXPECT_FALSE(draws("mindmap\nroot((r))\n  &lt;br/>B\n", kWide)) << "leading escaped break";
    EXPECT_FALSE(draws("mindmap\nroot((r))\n  A&lt;br/>\n", kWide)) << "trailing escaped break";
    // A label with more physical lines than the row cap rejects (the bytes
    // stay under the 1024-byte caption bound so the row cap is the witness).
    std::string lines = "mindmap\nroot((r))\n  id1[`";
    for (int n = 0; n < 257; ++n) lines += "a\n";
    lines += "z`]\n";
    EXPECT_FALSE(draws(lines, kWide)) << "over 256 caption rows";
    // Paired positives: the admitted forms draw.
    EXPECT_TRUE(draws("mindmap\nid1[`**a** b`]\n", kWide)) << "strong";
    EXPECT_TRUE(draws("mindmap\nid1[`a *b* c`]\n", kWide)) << "italic";
    EXPECT_TRUE(draws("mindmap\nid1[`a\nb`]\n", kWide)) << "multi-line markdown";
    EXPECT_TRUE(draws("mindmap\nid1[`a\r\nb`]\n", kWide)) << "CRLF multi-line markdown";
    EXPECT_TRUE(draws("mindmap\nroot((r))\n  A&lt;br/>B\n", kWide)) << "escaped break";
    EXPECT_TRUE(draws("mindmap\nroot((r))\n  A&lt;br/>B&lt;br/>C\n", kWide))
        << "two interior escaped breaks";
}
