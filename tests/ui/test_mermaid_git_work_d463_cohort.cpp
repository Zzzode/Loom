// D-463 git Work-envelope admission gate.
//
// Eight complete gitGraph originals (two 9-branch graphs and the six
// cherry-pick graphs) exceed the fixed 1M Work primary; a bounded
// canvas-derived pool funds them from the planned canvas. The art contract is
// unchanged: the gate pins the wide/narrow/restored states, restored cell
// equality, the commit markers and the branch refs.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-envelope-d463-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nS463_SENTINEL";
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

TEST(MermaidGitWorkD463Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_envelope_d463_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-463");
    EXPECT_EQ(root.get("counts").get_int("total"), 8);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("git"), 8);
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
    EXPECT_EQ(digest_input.size(), 8u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidGitWorkD463Cohort, AdmittedRowsDrawAndPinTheirStates) {
    const auto ledger = parse_doc(kLedger);
    const auto version = ledger.root().get("admission").get_string("bound");
    EXPECT_FALSE(version.empty());
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
        // The projection is a commit graph: markers and branch refs are drawn.
        EXPECT_NE(wide.text.find("●"), std::string::npos) << rel << " no commit marker";
        row.get("expected_branch_refs").iter([&](JsonVal branch) {
            const auto text = branch.as_str();
            if (!text.empty()) EXPECT_NE(wide.text.find(text), std::string::npos) << rel << text;
        });
        ++checked;
    });
    EXPECT_EQ(checked, 8);
}

TEST(MermaidGitWorkD463Cohort, MalformedGitStaysClosed) {
    // Paired negative: the envelope never admits a malformed graph; the same
    // grammar errors keep whole source.
    EXPECT_FALSE(draws("gitGraph\n  commit\n  merge nonexistent\n", kWide));
    EXPECT_FALSE(draws("gitGraph\n  branch\n", kWide));
    EXPECT_TRUE(draws("gitGraph\n  commit id: \"a\"\n", kWide));
}
