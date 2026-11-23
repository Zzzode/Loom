// D-440 flowchart closed `config.theme` gate.
//
// Selection rationale: nine flowchart originals carry only a lone indent-2
// `theme` leaf from the corpus-backed closed palette table. The terminal draws
// no palette, so the leaf is inert and disclosed on the flow notice footer; the
// body is drawn exactly as the config-free twin.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/flowchart-theme-d440-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP440_SENTINEL";
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
    return got.back() == "P440_SENTINEL";
}
// Strip the top-level `config:` block and every line indented under it.
std::string config_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
    const std::string body = source.substr(close + 5);
    std::string kept;
    bool skipping = false;
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        if (skipping && !line.empty() && line.front() == ' ') continue;
        skipping = false;
        if (line.starts_with("config:")) {
            skipping = true;
            continue;
        }
        kept += line;
        kept += "\n";
    }
    return "---\n" + kept + "---\n" + body;
}
constexpr std::string_view kBody = "flowchart LR\n  A[one] --> B(two)\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body = kBody) {
    return "---\ndescription: >-\n  flowchart theme probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidFlowchartThemeD440Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_flowchart_theme_d440_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-440");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "fee3e64bd8ff500fb1b6183e002228b7406c5d328ecb8dbf35d2c7a28181363d");
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
        EXPECT_EQ(row.get_string("manifest_sha256"), it->second.get_string("sha256")) << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 1u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidFlowchartThemeD440Cohort, ThemeIsInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = config_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel << " narrow stays source";
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
               (twin_lines.back().empty() || twin_lines.back() == "P440_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " theme changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidFlowchartThemeD440Cohort, ClosedPaletteNegativesStayWholeSource) {
    // Every admitted palette draws, bare or double-quoted, and discloses the
    // dequoted name.
    for (const auto* theme : {"forest"}) {
        for (const auto& form : {std::string("  theme: ") + theme + "\n",
                                 std::string("  theme: \"") + theme + "\"\n"}) {
            const auto source = probe("config:\n" + form);
            ASSERT_TRUE(draws(source, kWide)) << form;
            EXPECT_NE(capture(source, "mermaid", kWide)
                          .text.find(std::string("config.theme=") + theme +
                                     " -> colour palette; not applied"),
                      std::string::npos)
                << form;
        }
    }
    for (const auto* tail : {
             // Palettes outside the corpus-backed flowchart table (every
             // `flowchart/` theme fixture is Flow400-frozen; D-440 finding).
             "config:\n  theme: redux\n",
             "config:\n  theme: \"redux\"\n",
             "config:\n  theme: redux-dark\n",
             "config:\n  theme: redux-color\n",
             "config:\n  theme: redux-dark-color\n",
             "config:\n  theme: dark\n",
             "config:\n  theme: default\n",
             "config:\n  theme: neutral\n",
             "config:\n  theme: neo\n",
             // Malformed scalars and ordering.
             "config:\n  theme:\n",
             "config:\n  theme: 'forest'\n",
             "config:\n  theme: \"forest\n",
             "config:\n  theme: forest # note\n",
             "config:\n  theme: forest\n  theme: forest\n",
             "config:\n    theme: forest\n",
             "theme: forest\n",
             // A sibling leaf or namespace keeps the profile closed.
             "config:\n  theme: forest\n  look: handDrawn\n",
             "config:\n  theme: forest\n  htmlLabels: true\n",
             "config:\n  theme: forest\n  flowchart:\n    nodeSpacing: 100\n",
         }) {
        const auto source = probe(tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    // The config block must follow the description, and a config-free flow
    // keeps no notice footer.
    EXPECT_FALSE(draws("---\nconfig:\n  theme: forest\ndescription: >-\n  probe\n---\n" +
                           std::string(kBody),
                       kWide));
    EXPECT_EQ(capture("flowchart LR\n  A --> B\n", "mermaid", kWide)
                  .text.find("config.theme="),
              std::string::npos);
}
