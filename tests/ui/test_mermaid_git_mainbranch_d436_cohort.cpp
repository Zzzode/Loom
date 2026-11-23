// D-436 gitGraph main-branch gate.
//
// Selection rationale: fourteen git fallbacks differ only by
// `config.gitGraph.{mainBranchName, mainBranchOrder}` (two bare, twelve that
// also carry the closed look: neo / theme tables). Both leaves are honoured:
// the trunk is renamed (`@trunk` label, `checkout trunk` resolves to it) and
// its lane order moves the trunk column right of the sibling branches.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-mainbranch-d436-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP436_SENTINEL";
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
        if (line == "description:" || line == "config:") return false;
        if (line == "---") return false;
    }
    return got.back() == "P436_SENTINEL";
}
constexpr std::string_view kBody = "gitGraph LR:\n  commit id: \"a\"\n  commit id: \"b\"\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body = kBody) {
    return "---\ndescription: >-\n  git title probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidGitMainBranchD436Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_mainbranch_d436_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-436");
    EXPECT_EQ(root.get("counts").get_int("total"), 14);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "115731960e15c33ab6e779b2f2874ceaa838861a6f29115640d303c2d2c0ecb1");
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
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 14u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidGitMainBranchD436Cohort, TrunkIdentityAndLaneOrderAreHonoured) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel << " narrow stays source";
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        const auto text = capture(source, "mermaid", kWide).text;
        const auto config = row.get("config");
        const bool renamed = config.has("mainBranchName");
        const std::string trunk =
            renamed ? config.get_string("mainBranchName") : std::string("main");
        if (renamed) {
            EXPECT_NE(text.find("*@" + trunk + " "), std::string::npos)
                << rel << " configured trunk label";
            EXPECT_EQ(text.find("@main"), std::string::npos) << rel << " old trunk label";
        } else {
            EXPECT_NE(text.find("*@main "), std::string::npos) << rel << " trunk label";
        }
        ++checked;
    });
    EXPECT_EQ(checked, 14);
}

// The exact geometric claim, pinned on the canonical order row: with
// `mainBranchOrder: 2` the trunk's later commit column sits right of both
// sibling branches (feature2 forks after feature1, so its column is right of
// feature1's; the trunk is right of both).
TEST(MermaidGitMainBranchD436Cohort, LaneOrderMovesTheTrunkColumn) {
    const auto source = read_file(
        kCases / "git/nightly-check/96-should-render-with-custom-mainbranchorder.mmd");
    const auto text = capture(source, "mermaid", kWide).text;
    const auto column = [&](const std::string& needle) {
        const auto at = text.find(needle);
        if (at == std::string::npos) return -1;
        return static_cast<int>(at % static_cast<std::size_t>(kWide));
    };
    const int feature1 = column("\u25cf  2");
    const int feature2 = column("\u25cf  3");
    const int trunk = column("\u25cf  4");
    EXPECT_GT(feature1, 0);
    EXPECT_GT(feature2, feature1) << "feature2 is right of feature1";
    EXPECT_GT(trunk, feature2) << "order 2 puts the trunk right of both";
    const auto trunk_row = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    bool trunk_caption = false;
    for (const auto& line : trunk_row)
        if (line.find("*@main \u2192 4") != std::string::npos) trunk_caption = true;
    EXPECT_TRUE(trunk_caption);
}

TEST(MermaidGitMainBranchD436Cohort, ClosedMainBranchNegativesStayWholeSource) {
    // A closed leaf alone draws and shows its notice.
    {
        const auto source = probe("config:\n  gitGraph:\n    mainBranchName: 'trunk'\n",
                                  "gitGraph\n  commit id: \"1\"\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "config.gitGraph.mainBranchName=trunk -> trunk branch label; honoured"),
                  std::string::npos);
    }
    {
        const auto source = probe("config:\n  gitGraph:\n    mainBranchOrder: 2\n",
                                  "gitGraph\n  commit id: \"1\"\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "config.gitGraph.mainBranchOrder=2 -> trunk lane order; honoured"),
                  std::string::npos);
    }
    // The closed look/theme tables compose with the trunk leaves.
    {
        const auto source = probe(
            "config:\n  gitGraph:\n    mainBranchName: trunk\n  look: neo\n  theme: neo-dark\n",
            "gitGraph\n  commit id: \"1\"\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto text = capture(source, "mermaid", kWide).text;
        EXPECT_NE(text.find("config.look=neo -> source node style; not applied"),
                  std::string::npos);
        EXPECT_NE(text.find("config.theme=neo-dark -> colour palette; not applied"),
                  std::string::npos);
        EXPECT_NE(text.find("config.gitGraph.mainBranchName=trunk -> trunk branch label; "
                            "honoured"),
                  std::string::npos);
    }
    for (const auto* tail : {
             "config:\n  gitGraph:\n    mainBranchName: trunk\n  look: handDrawn\n",
             "config:\n  gitGraph:\n    mainBranchName: trunk\n  theme: default\n",
             "config:\n  gitGraph:\n    mainBranchName: 'two words'\n",
             "config:\n  gitGraph:\n    mainBranchName: ''\n",
             "config:\n  gitGraph:\n    mainBranchName: trunk\n    mainBranchName: dev\n",
             "config:\n  gitGraph:\n    mainBranchOrder: -1\n",
             "config:\n  gitGraph:\n    mainBranchOrder: 2.5\n",
             "config:\n  gitGraph:\n    mainBranchOrder: x\n",
             "config:\n  gitGraph:\n    mainBranchOrder: 1234\n",
             "config:\n  gitGraph:\n    colour: red\n",
             "config:\n  mainBranchName: trunk\n",
         }) {
        const auto source = probe(tail, "gitGraph\n  commit id: \"1\"\n");
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    // The renamed trunk must be what `checkout` resolves; checking out an
    // unknown branch is still a body error.
    EXPECT_FALSE(draws(probe("config:\n  gitGraph:\n    mainBranchName: trunk\n",
                             "gitGraph\n  checkout main\n  commit id: \"1\"\n"),
                       kWide));
    // Neither leaf bypasses the body style gates.
    EXPECT_FALSE(draws(probe("config:\n  gitGraph:\n    mainBranchOrder: 2\n",
                             "gitGraph\n  commit id: \"1\"\n  %%{init: {}}%%\n"),
                       kWide));
}
