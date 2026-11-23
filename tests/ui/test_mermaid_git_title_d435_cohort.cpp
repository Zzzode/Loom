// D-435 gitGraph frontmatter-title gate.
//
// Selection rationale: the last frontmatter-only git fallback is one row whose
// only extra key is a `title:`. The title is honoured (drawn as the frame's
// first row), so the oracle is exact — the frame minus that row equals the
// title-free twin's frame.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-title-d435-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP435_SENTINEL";
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
std::array<std::string, 3> load_states(const JsonVal& states) {
    return {states.get_string("wide_240"), states.get_string("narrow_40"),
            states.get_string("restored_240")};
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
    return got.back() == "P435_SENTINEL";
}
std::string title_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
    const std::string body = source.substr(close + 5);
    std::string kept;
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        if (line.starts_with("title:")) continue;
        kept += line;
        kept += "\n";
    }
    return "---\n" + kept + "---\n" + body;
}
constexpr std::string_view kBody = "gitGraph LR:\n  commit id: \"a\"\n  commit id: \"b\"\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body = kBody) {
    return "---\ndescription: >-\n  git title probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidGitTitleD435Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_title_d435_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-435");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "e420ab586fabe66365e606124409f3551085c3eaa73a6e6c0cfb7a5fb3d76271");
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
        EXPECT_FALSE(row.get_string("expected_title_row").empty()) << rel;
        // Cross-ledger consistency: the RFC 0031 E2a pre-registration also
        // governs this config-free frontmatter row and must agree it draws.
        {
            auto e2a = json::parse(read_file(fs::path(LOOM_TESTS_DIR) /
                                             "../tools/mermaid/e2a-cohort.json"));
            ASSERT_TRUE(e2a.has_value()) << "E2a cohort ledger must parse";
            bool found = false;
            e2a->root().get("rows").iter([&](const JsonVal entry) {
                if (entry.get_string("path") != rel) return;
                found = true;
                EXPECT_EQ(entry.get_string("prediction"), "predicted-draw") << rel;
            });
            EXPECT_TRUE(found) << rel;
        }
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 1u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidGitTitleD435Cohort, TitleLeadsTheFrameAndArtMatchesTitleFreeTwin) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = title_free_twin(source);
        const auto states = load_states(row.get("expected_states"));
        const auto expected_title = row.get_string("expected_title_row");
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_FALSE(lines.empty()) << rel;
        EXPECT_EQ(lines.front(), expected_title) << rel << " title must lead the frame";
        // Removing the title row must reproduce the title-free twin exactly.
        const auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        std::vector<std::string> without_title(lines.begin() + 1, lines.end());
        EXPECT_EQ(without_title, twin_lines) << rel << " title row changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidGitTitleD435Cohort, ClosedTitleNegativesStayWholeSource) {
    // Positives: a quoted title is the same title; a title combined with an
    // admitted config block draws and keeps the config footer after the art.
    {
        const auto source = probe("title: 'quoted title'\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_EQ(trimmed_lines(capture(source, "mermaid", kWide), kWide).front(),
                  "title quoted title");
    }
    {
        const auto source = probe("title: with config\nconfig:\n  look: neo\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_FALSE(lines.empty());
        EXPECT_EQ(lines.front(), "title with config");
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("GitGraph native projection v1"),
                  std::string::npos);
    }
    for (const auto* bad : {
             "title:\n",
             "title:   \n",
             "title: one\ntitle: two\n",
             "config:\n  look: neo\ntitle: late\n",
             "title: has\ttab\n",
             "title: \"unbalanced\n",
             "title: \"\"\n",
             "theme: dark\n",
             "look: handDrawn\n",
         }) {
        const auto source = probe(bad);
        EXPECT_FALSE(draws(source, kWide)) << bad;
        EXPECT_FALSE(native_frame(source, kWide)) << bad;
    }
    // A title alone (no description payload) stays whole-source: the envelope
    // requires the folded description.
    EXPECT_FALSE(draws("---\ntitle: orphan\n---\ngitGraph LR:\n  commit id:\"1\"\n", kWide));
    // The title does not bypass the body parser.
    EXPECT_FALSE(draws(probe("title: probe\n", "gitGraph LR:\n  unknown statement\n"), kWide));
}
