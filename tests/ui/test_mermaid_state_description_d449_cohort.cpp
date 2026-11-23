// D-449 state `state X: Description` admission gate.
//
// Selection rationale: the four `states-with-descriptions-neo-look-*` rows
// whose only remaining blocker was the upstream declaration spelling
// `state X: Description`, which attaches a description fragment to a state
// already introduced by a transition.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-description-d449-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP449_SENTINEL";
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
        if (line.starts_with("state State1:") || line.starts_with("  look: neo"))
            return false;
    }
    return got.back() == "P449_SENTINEL";
}
// The same fixture with the whole config block removed.
std::string config_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
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
        kept += '\n';
    }
    return "---\n" + kept + "---\n" + source.substr(close + 5);
}
constexpr std::string_view kBody =
    "stateDiagram-v2\n  [*] --> State1\n  State1 --> State2\n  State1 --> State3\n"
    "  State2 --> State3\n  state State1: Description 1\n  state State2: Description 2\n"
    "  State3 --> [*]\n";
std::string probe(std::string_view config, std::string_view body) {
    return "---\ndescription: >-\n  state description probe\n" + std::string(config) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidStateDescriptionD449Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_state_description_d449_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-449");
    EXPECT_EQ(root.get("counts").get_int("total"), 4);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "c7672e44c1df3247efb94f18ef7ca74edd6554f44481376c222d899eee172eab");
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
    EXPECT_EQ(digest_input.size(), 4u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidStateDescriptionD449Cohort, DescriptionsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = config_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
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
               (twin_lines.back().empty() || twin_lines.back() == "P449_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the description changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 4);
}

TEST(MermaidStateDescriptionD449Cohort, DescriptionDeclarationShape) {
    // The declaration form draws for a state introduced by a transition and
    // for one introduced here, and the description text is transcribed.
    {
        const auto source = probe("", kBody);
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("Description 1"),
                  std::string::npos);
    }
    EXPECT_TRUE(draws(probe("", "stateDiagram-v2\n  state S1: Solo text\n  S1 --> [*]\n"),
                      kWide));
    // Repeated declarations accumulate: both description fragments render, for
    // a state already declared here and for one introduced by a transition.
    {
        const auto source = probe("", "stateDiagram-v2\n  state S1: First text\n"
                                      "  S1 --> [*]\n  state S1: Second text\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("First text"), std::string::npos);
        EXPECT_NE(frame.find("Second text"), std::string::npos);
    }
    {
        const auto source = probe("", "stateDiagram-v2\n  A --> B : go\n"
                                      "  state A\n  state A: Later text\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("Later text"),
                  std::string::npos);
    }
    // Shape rules: empty payload, unbalanced quote and brace markers stay out.
    for (const auto* body : {
             "stateDiagram-v2\n  state S1:\n  S1 --> [*]\n",
             "stateDiagram-v2\n  state S1: \n  S1 --> [*]\n",
             "stateDiagram-v2\n  state S1: {desc}\n  S1 --> [*]\n",
         }) {
        EXPECT_FALSE(draws(probe("", body), kWide)) << body;
    }
    // A config-free source keeps no disclosure footer.
    {
        const auto frame = capture(probe("", kBody), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("State native projection v1"), std::string::npos);
    }
}
