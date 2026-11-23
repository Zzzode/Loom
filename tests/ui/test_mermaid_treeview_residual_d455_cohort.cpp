// D-455 tree-view residual admission gate.
//
// Selection rationale: the last two tree-view fallbacks. One carries the closed
// `config.treeView` / `config.themeVariables.treeView` leaves under a frontmatter
// whose closing fence has trailing spaces; the other uses the `icon(none)` and
// empty `icon()` suppression suffixes. The native projection transcribes node
// rows verbatim and never draws icons, colour, line weight or pixel indentation,
// so every accepted leaf is inert and disclosed once in the footer.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/treeview-residual-d455-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP455_SENTINEL";
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
    if (capture.chars.size() !=
        static_cast<std::size_t>(width) * static_cast<std::size_t>(kHeight))
        return {};
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
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:" || line == "---") return false;
        if (line.starts_with("config.treeView.") &&
            line.find("not applied") == std::string::npos)
            return false;
    }
    return got.back() == "P455_SENTINEL";
}
// The same fixture with the frontmatter removed (the config is the only
// admitted frontmatter surface, so the body art must be identical).
std::string frontmatter_free_twin(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---", 4);
    if (close == std::string::npos) return source;
    const auto eol = source.find('\n', close + 1);
    if (eol == std::string::npos) return "";
    return source.substr(eol + 1);
}
constexpr std::string_view kBody = "treeView-beta\n  folder\n      \"leaf\"\n";
std::string probe(std::string_view frontmatter, std::string_view body) {
    return "---\n" + std::string{frontmatter} + "---\n" + std::string{body};
}
}  // namespace

TEST(MermaidTreeviewResidualD455Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_treeview_residual_d455_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-455");
    EXPECT_EQ(root.get("counts").get_int("total"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("tree-view"), 2);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "662bf3072e6c31b7f10b87a955172f01a79bea12c0baee927e623766354bfa1d");
    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
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

TEST(MermaidTreeviewResidualD455Cohort, LeavesAreInertAgainstFrontmatterFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = frontmatter_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        // Rule 34: the 40-column state is measured live, never inherited.
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        std::vector<std::string> art(lines.begin(),
                                     lines.begin() + static_cast<std::ptrdiff_t>(begin));
        while (!art.empty() && art.back().empty()) art.pop_back();
        auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        while (!twin_lines.empty() &&
               (twin_lines.back().empty() || twin_lines.back() == "P455_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the leaves changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 2);
}

TEST(MermaidTreeviewResidualD455Cohort, ClosedNegatives) {
    // Admitted positives: the empty icon() call, icon(none), and a namespace
    // leaf under a trailing-space closing fence.
    {
        const auto source = "treeView-beta\n  a icon()\n  b icon(none)\n";
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_TRUE(native_frame(source, kWide));
        EXPECT_EQ(capture(source, "mermaid", kWide).text.find("TreeView native projection v1"),
                  std::string::npos);
    }
    {
        const auto source =
            "---\ndescription: >-\n  probe\nconfig:\n  treeView:\n    showIcons: true\n---   \n"
            "treeView-beta\n  a\n";
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "config.treeView.showIcons=true -> icons are not drawn; not applied"),
                  std::string::npos);
    }
    for (const auto* body : {
             // The icon() suffix stays single and outside a description.
             "treeView-beta\n  a icon() icon()\n",
             "treeView-beta\n  a icon(\n",
             "treeView-beta\n  a ## note icon()\n",
             "treeView-beta\n  \"a\" junk\n",
         }) {
        EXPECT_FALSE(draws(body, kWide)) << body;
    }
    for (const auto& source : {
             probe("description: >-\n  x\nconfig:\n  treeView:\n    unknown: 1\n", kBody),
             probe("look: handDrawn\n", kBody),
             // A closing fence with trailing text is not a fence.
             std::string{"---\ndescription: >-\n  x\nconfig:\n  treeView:\n    showIcons: true\n---x\n"} +
                 std::string{kBody},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
