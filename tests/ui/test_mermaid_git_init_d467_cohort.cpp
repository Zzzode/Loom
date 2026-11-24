// D-467 gitGraph pre-header init cohort gate.
//
// Eight gitGraph originals carry a `%%{init: ...}%%` directive spelled with
// single quotes and spanning physical lines. Its keys are inert for the
// terminal projection (logLevel, theme, rotateCommitLabel, branch-label
// colours); the projection draws labels horizontally and no colour, discloses
// every key once on the canonical footer, and leaves the graph art unchanged.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-init-d467-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nS467_SENTINEL";
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

TEST(MermaidGitInitD467Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_init_d467_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-467");
    EXPECT_EQ(root.get("counts").get_int("total"), 9);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("git"), 9);
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
    EXPECT_EQ(digest_input.size(), 9u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidGitInitD467Cohort, AdmittedRowsDrawAndPinTheirStates) {
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
        EXPECT_TRUE(same_surface(capture(source, "mermaid", kNarrow),
                                 capture(source, "text", kNarrow)))
            << rel << " narrow render is not the whole source";
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        EXPECT_NE(wide.text.find("GitGraph native projection v1"), std::string::npos) << rel;
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
        EXPECT_EQ(wide.text.find("%%{init"), std::string::npos) << rel << " directive drawn";
        ++checked;
    });
    EXPECT_EQ(checked, 9);
}

TEST(MermaidGitInitD467Cohort, MalformedInitDirectivesStayClosed) {
    const std::string body = "gitGraph\n  commit id: \"a\"\n";
    // Pair posited positives: the admitted spellings draw.
    EXPECT_TRUE(draws("%%{init: {'logLevel': 'debug'}}%%\n" + body, kWide));
    EXPECT_TRUE(draws("%%{init: {'logLevel': 'debug',}}%%\n" + body, kWide))
        << "one trailing comma before the closing brace";
    EXPECT_TRUE(draws("%%{init: {'gitGraph': {'rotateCommitLabel': true}}}%%\n" + body, kWide))
        << "closed nested profile";
    EXPECT_TRUE(draws("%%{init: {'themeVariables': {\n 'gitBranchLabel0': '#ffffff'\n } }}%%\n" +
                          body,
                      kWide))
        << "multi-line directive";
    // Paired negatives: every other spelling keeps the whole source.
    EXPECT_FALSE(draws("%%{init: {'colour': 'red'}}%%\n" + body, kWide)) << "unknown key";
    EXPECT_FALSE(draws("%%{init: {'gitGraph': {}}}%%\n" + body, kWide)) << "empty gitGraph block";
    EXPECT_FALSE(draws("%%{init: {}}%%\n" + body, kWide)) << "empty payload";
    EXPECT_FALSE(draws("%%{init: {'logLevel': 'loud'}}%%\n" + body, kWide)) << "unknown level";
    EXPECT_FALSE(draws("%%{init: {'theme': 'neo'}}%%\n" + body, kWide)) << "unknown theme";
    EXPECT_FALSE(draws("%%{init: {'themeVariables': {'gitBranchLabel0': 'red'}}%%\n" + body,
                       kWide))
        << "non-hex colour";
    EXPECT_FALSE(draws("%%{init: {'themeVariables': {'gitBranchLabel8': '#ffffff'}}%%\n" + body,
                       kWide))
        << "label index out of range";
    EXPECT_FALSE(draws("%%{init: {'logLevel': 'debug', 'logLevel': 'info'}}%%\n" + body, kWide))
        << "duplicate key";
    EXPECT_FALSE(draws("%%{init: {'logLevel': 'debug}%%\n" + body, kWide)) << "unterminated quote";
    EXPECT_FALSE(draws("%%{init: {'logLevel': 'debug',,}}%%\n" + body, kWide))
        << "two trailing commas";
    EXPECT_FALSE(draws("%%{init: {'theme': 'def\"ault'}}%%\n" + body, kWide))
        << "double quote inside a single-quoted string";
    EXPECT_TRUE(draws("%%{init: {\"logLevel\": 'debug'}}%%\n" + body, kWide))
        << "mixed quote spellings";
    EXPECT_FALSE(draws("%%{init: {\"logLevel\": \"debug}}%%\n" + body, kWide))
        << "unterminated double quote";
    EXPECT_FALSE(draws("%%{init: {'logLevel': 'de\\'bug'}}%%\n" + body, kWide)) << "escape";
    EXPECT_FALSE(draws(body + "%%{init: {'logLevel': 'debug'}}%%\n", kWide))
        << "directive after the header";
    std::string long_directive = "%%{init: {\n";
    for (int n = 0; n < 20; ++n) long_directive += "  'x': 1,\n";
    long_directive += "}}%%\n";
    EXPECT_FALSE(draws(long_directive + body, kWide)) << "over 16 continuation lines";
}
