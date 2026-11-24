// D-461 stage-4 `info` directive admission gate.
//
// The bare `info` directive has no body content: upstream renders a fixed
// version panel. The projection is one banner line naming the pinned upstream
// version (read from the fixture manifest, pinned by the ledger's
// `panel_version`) plus disclosure lines for accepted frontmatter config
// leaves. Every other shape stays closed: a second body line, an unknown
// frontmatter key, a config leaf outside the E2b whitelist, an unterminated
// fence.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/info-d461-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 200;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nI461_SENTINEL";
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

std::vector<std::string> trimmed_lines(const CellCapture& capture, int width) {
    if (capture.chars.size() != static_cast<std::size_t>(width) * kHeight) return {};
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(kHeight));
    for (int y = 0; y < kHeight; ++y) {
        std::string line;
        line.reserve(static_cast<std::size_t>(width));
        for (int x = 0; x < width; ++x)
            line += capture.chars[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                  static_cast<std::size_t>(x)];
        while (!line.empty() && line.back() == ' ') line.pop_back();
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

// The panel is the three leading rows; every later row is a disclosure line.
std::vector<std::string> panel_frame(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    while (!lines.empty() && lines.back() == "I461_SENTINEL") lines.pop_back();
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    if (lines.size() < 3u) return lines;
    return {lines.begin(), lines.begin() + 3};
}

std::vector<std::string> notices(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    while (!lines.empty() && lines.back() == "I461_SENTINEL") lines.pop_back();
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    if (lines.size() <= 3u) return {};
    return {lines.begin() + 3, lines.end()};
}
}  // namespace

TEST(MermaidInfoD461Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_info_d461_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-461");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("info"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("state-diagram-v2"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto manifest = parse_doc(kManifest);
    const auto tag = manifest.root().get("upstream").get_string("tag");
    const auto version = tag.substr(tag.find('@') + 1);
    EXPECT_EQ(root.get_string("panel_version"), version);
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> unique_paths;
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
        unique_paths.insert(rel);
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 3u);
    EXPECT_EQ(unique_paths.size(), 3u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidInfoD461Cohort, AdmittedRowsDrawTheVersionBanner) {
    const auto ledger = parse_doc(kLedger);
    const auto version = ledger.root().get_string("panel_version");
    const auto expected_frame = std::vector<std::string>{
        "┌────────────────┐", "│ Mermaid " + version + " │", "└────────────────┘"};
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        ASSERT_TRUE(draws(source, kWide)) << rel;
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto wide = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        EXPECT_EQ(panel_frame(wide, kWide), expected_frame) << rel;
        // The directive keyword is input, never output.
        EXPECT_EQ(wide.text.find("showInfo"), std::string::npos) << rel;
        std::vector<std::string> expected_notices;
        row.get("expected_notices").iter([&](JsonVal notice) {
            expected_notices.emplace_back(notice.as_str());
        });
        EXPECT_EQ(notices(wide, kWide), expected_notices) << rel;
        ++checked;
    });
    EXPECT_EQ(checked, 3);
}

TEST(MermaidInfoD461Cohort, DirectiveTwinsAreCellEqual) {
    // Upstream renders `info` and `info showInfo` identically; the twin proof
    // keeps the projection from keying on the keyword.
    EXPECT_TRUE(same_surface(capture("info", "mermaid", kWide),
                             capture("info showInfo", "mermaid", kWide)));
    // The frontmatter fence does not change the panel: only the disclosure
    // lines are added below it.
    const std::string with_frontmatter =
        "---\ndescription: plain\n---\ninfo";
    EXPECT_EQ(panel_frame(capture(with_frontmatter, "mermaid", kWide), kWide),
              panel_frame(capture("info", "mermaid", kWide), kWide));
    EXPECT_TRUE(notices(capture(with_frontmatter, "mermaid", kWide), kWide).empty());
}

TEST(MermaidInfoD461Cohort, DisclosureFooterGovernsNarrowWidth) {
    // Accepted config leaves are disclosed below the panel; that disclosure
    // line is wider than the 40-column fence, so the row keeps whole source at
    // 40 while every config-free `info` directive draws at both widths. This
    // is the same narrow policy the E2b footer rows pre-register.
    EXPECT_TRUE(draws("info", kNarrow));
    EXPECT_TRUE(draws("---\ndescription: plain\n---\ninfo showInfo", kNarrow));
    EXPECT_FALSE(draws("---\nconfig:\n  logLevel: 1\n---\ninfo", kNarrow));
}

TEST(MermaidInfoD461Cohort, ClosedNegatives) {
    for (const auto& source : {
             // Body grammar: exactly one directive line, exact spelling.
             std::string{"info extra"},
             std::string{"info showInfo extra"},
             std::string{"infox"},
             std::string{"info\ninfo"},
             std::string{"info\nstateDiagram-v2\nA"},
             std::string{"info showinfo"},
             // Frontmatter: closed key set, closed config leaf set.
             std::string{"---\ntitle: x\n---\ninfo"},
             std::string{"---\nconfig:\n  theme: forest\n---\ninfo"},
             std::string{"---\nconfig:\n  logLevel: 4\n---\ninfo"},
             std::string{"---\nconfig:\n  fontFamily: monaco\n---\ninfo"},
             std::string{"---\nconfig:\n    logLevel: 1\n---\ninfo"},
             std::string{"---\nconfig:\n  look: neon\n---\ninfo"},
             // Unterminated fence.
             std::string{"---\ndescription: x\ninfo"},
             // An empty config map has no accepted leaf: closed, not a no-op.
             std::string{"---\nconfig:\n---\ninfo"},
             // A body init/directive has no reviewed rule for this family.
             std::string{"%%{init: {\"theme\": \"dark\"}}%%\ninfo"},
             std::string{"---\ndescription: x\n---\n%%{init: {}}%%\ninfo"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
    // The paired admitted positives for the same grammar.
    EXPECT_TRUE(draws("info", kWide));
    EXPECT_TRUE(draws("info showInfo", kWide));
    EXPECT_TRUE(draws("---\nconfig:\n  logLevel: 1\n---\ninfo", kWide));
    EXPECT_TRUE(draws("---\nconfig:\n  fontFamily: courier\n---\ninfo showInfo", kWide));
}
