// D-439 mindmap closed `config.layout` gate.
//
// Selection rationale: three mindmap originals carry a folded description plus
// exactly `config.layout: tidy-tree`, and their bodies stay inside the admitted
// tree grammar. The tidy-tree engine is an external layout engine the terminal
// projection does not run, so the leaf is inert and disclosed under
// `Mindmap native projection v1`; the body is drawn as the native tree.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/mindmap-tidy-d439-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP439_SENTINEL";
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
        if (line == "  layout: tidy-tree") return false;
        if (line == "---") return false;
    }
    return got.back() == "P439_SENTINEL";
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
constexpr std::string_view kBody = "mindmap\n  root((probe))\n    A\n    B\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body = kBody) {
    return "---\ndescription: >-\n  mindmap layout probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidMindmapTidyD439Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_mindmap_tidy_d439_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-439");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "36d3a7cf96fba3cd978cf781e3d3dce98c8b0d8ce7dec6611ef0f5cfbead3193");
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
    EXPECT_EQ(digest_input.size(), 3u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidMindmapTidyD439Cohort, LayoutLeafIsInertAgainstTwins) {
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
               (twin_lines.back().empty() || twin_lines.back() == "P439_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " layout leaf changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 3);
}

TEST(MermaidMindmapTidyD439Cohort, ClosedLayoutNegativesStayWholeSource) {
    // The admitted leaf draws and discloses.
    {
        const auto source = probe("config:\n  layout: tidy-tree\n");
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide)
                      .text.find("config.layout=tidy-tree -> external layout engine; not applied"),
                  std::string::npos);
    }
    for (const auto* tail : {
             // Other layout engines and other config leaves stay unadmitted.
             "config:\n  layout: elk\n",
             "config:\n  layout: dagre\n",
             // D-468 admits the closed `theme: neo` leaf and the closed six-name
             // palette, so the former negatives use an unadmitted theme.
             "config:\n  theme: base\n",
             "config:\n  look: classic\n",
             "config:\n  layout: tidy-tree\n  theme: base\n",
             // Shape and ordering.
             "config:\n   layout: tidy-tree\n",
             "config:\n    layout: tidy-tree\n",
             "config:\n  layout:tidy-tree\n",
             "config:\n  layout: tidy-tree # note\n",
             "config:\n  layout: tidy-tree\n  layout: tidy-tree\n",
             "config:\n",
             "layout: tidy-tree\n",
             "theme: dark\n",
         }) {
        const auto source = probe(tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    // The config block must follow the description, and the body grammar is
    // unchanged: a malformed icon and an anonymous shorthand outside the root
    // stay whole-source (D-464 admits the well-formed icon line and the
    // anonymous root itself).
    EXPECT_FALSE(draws("---\nconfig:\n  layout: tidy-tree\ndescription: >-\n  probe\n---\n" +
                           std::string(kBody),
                       kWide));
    EXPECT_FALSE(draws(probe("config:\n  layout: tidy-tree\n",
                             "mindmap\n  root((probe))\n    A\n      ::icon()\n"),
                       kWide));
    EXPECT_FALSE(draws(probe("config:\n  layout: tidy-tree\n",
                             "mindmap\n  root((probe))\n    ((probe))\n"),
                       kWide));
    // D-468 paired positive: the leaf that used to be a negative now draws
    // and discloses.
    {
        const auto source = probe("config:\n  theme: neo\n");
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide)
                      .text.find("config.theme=neo -> colour palette; not applied"),
                  std::string::npos);
    }
    // A config-free mindmap keeps no disclosure footer.
    EXPECT_EQ(capture("mindmap\n  root((probe))\n    A\n", "mermaid", kWide)
                  .text.find("Mindmap native projection v1"),
              std::string::npos);
}
