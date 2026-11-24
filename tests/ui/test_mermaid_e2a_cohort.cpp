// RFC 0031 E2a durable cohort gate. Public API only (no module declaration):
// it enforces the pre-registration ledger at tools/mermaid/e2a-cohort.json
// against the live renderer and the pinned manifest.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/e2a-cohort.json";
const fs::path kE2bLedger = kTestsDir / "../tools/mermaid/e2b-cohort.json";
const fs::path kRfc0038Ledger = kTestsDir / "../tools/mermaid/state-0038-cohort.json";
const fs::path kRfc0039Ledger = kTestsDir / "../tools/mermaid/git-0039-cohort.json";
const fs::path kRfc0045Ledger = kTestsDir / "../tools/mermaid/git-0045-cohort.json";
const fs::path kRfc0046Ledger = kTestsDir / "../tools/mermaid/layouttuning-0046-cohort.json";
const fs::path kRfc0048Ledger = kTestsDir / "../tools/mermaid/state-region-0048-cohort.json";
const fs::path kD424Ledger = kTestsDir / "../tools/mermaid/git-bool-d424-cohort.json";
const fs::path kD436Ledger = kTestsDir / "../tools/mermaid/git-mainbranch-d436-cohort.json";
const fs::path kD441Ledger = kTestsDir / "../tools/mermaid/state-layout-d441-cohort.json";
const fs::path kD446Ledger =
    kTestsDir / "../tools/mermaid/state-leading-comment-d446-cohort.json";
const fs::path kD449Ledger = kTestsDir / "../tools/mermaid/state-description-d449-cohort.json";
const fs::path kD461Ledger = kTestsDir / "../tools/mermaid/state-d461-cohort.json";
constexpr int kWide = 240;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\nE2A_COHORT_SENTINEL";
}

std::string render_text(const std::string& source, const char* language) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    if (!element) return {};
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(kWide), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    return screen.ToString();
}

// Capture text+background per cell, ignoring foreground color: mermaid and
// text code blocks may highlight identical source with different token colors.
struct CellCapture {
    std::vector<std::pair<std::string, std::string>> cells;
};

CellCapture capture_cells(const std::string& source, const char* language) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(kWide), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.cells.reserve(static_cast<std::size_t>(kWide) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < kWide; ++x) {
            const auto& pixel = screen.PixelAt(x, y);
            out.cells.emplace_back(pixel.character, pixel.background_color.Print(true));
        }
    return out;
}

bool same_source_surface(const CellCapture& a, const CellCapture& b) {
    return a.cells == b.cells;
}

bool drawn_native(const std::string& source) {
    const auto native = capture_cells(source, "mermaid");
    const auto control = capture_cells(source, "text");
    if (native.cells.empty() || same_source_surface(native, control)) return false;
    // A source echo is proven above by the cell equality; this second guard
    // only rejects a frame whose *first* line is the echoed header. Substring
    // checks would misfire on a projected title (D-435 draws
    // `title simple gitGraph`, which contains the family name).
    const auto text = render_text(source, "mermaid");
    const auto first = text.substr(0, text.find('\n'));
    return first != "---" && first != "gitGraph" && first != "gitGraph BT:" &&
           first != "gitGraph TB:" && first != "stateDiagram" && first != "stateDiagram-v2";
}

// The returned JsonDoc owns the yyjson document; JsonVal is a non-owning
// view, so callers must keep this object alive while they use its root().
JsonDoc parse_ledger() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse ledger");
    return std::move(*parsed);
}

struct Row {
    std::string path, prediction, wide;
};

std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({
            value.get_string("path"),
            value.get_string("prediction"),
            value.get("expected_states").get_string("wide_240"),
        });
    });
    return rows;
}

// First non-empty payload line of the description folded block.
std::string description_token(const std::string& source) {
    std::istringstream in(source);
    std::string line;
    bool in_frontmatter = false, seen_description = false;
    while (std::getline(in, line)) {
        if (line == "---") {
            if (!in_frontmatter) {
                in_frontmatter = true;
                continue;
            }
            break;
        }
        if (in_frontmatter) {
            if (line.starts_with("description:")) {
                seen_description = true;
                continue;
            }
            if (seen_description && line.size() > 2 && line[0] == ' ' && line[1] == ' ')
                return line.substr(2);
        }
    }
    return {};
}
}  // namespace

TEST(MermaidE2aCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_e2a_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 69);
    EXPECT_EQ(counts.get_int("git"), 64);
    EXPECT_EQ(counts.get_int("state_v2"), 5);
    EXPECT_EQ(counts.get_int("state_v1_qualified"), 0);
    // D-461 retires the two budget-blocked next-line-brace draws (61/8 ->
    // 63/6); stage 3's crossing retry retires the last one (64/5).
    EXPECT_EQ(counts.get_int("predicted_draw"), 64);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 5);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter([&](const JsonVal value) {
        vocabulary.emplace(value.as_str());
    });
    // D-435 retired `gitgraph-title-not-projected` (the title is projected).
    EXPECT_EQ(vocabulary.size(), 5u);
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-blocked")
            EXPECT_TRUE(vocabulary.contains(row.get("blocker").get_string("kind")));
        else
            EXPECT_TRUE(row.get("blocker").is_null());
    });
}

TEST(MermaidE2aCohort, RowsPinFixtureBytesAndPriorStatus) {
    auto manifest = json::parse(read_file(kManifest));
    ASSERT_TRUE(manifest.has_value());
    std::map<std::string, JsonVal> manifest_rows;
    manifest->root().get("fixtures").iter([&](const JsonVal fixture) {
        manifest_rows.emplace(fixture.get_string("path"), fixture);
    });
    auto ledger = parse_ledger();
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto path = row.get_string("path");
        const auto found = manifest_rows.find(std::string(path));
        ASSERT_NE(found, manifest_rows.end()) << path;
        const auto bytes = read_file(kCases / std::string(path));
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), found->second.get_string("sha256")) << path;
        EXPECT_EQ(found->second.get("raw").get_string("expected"), "source") << path;
        EXPECT_EQ(found->second.get("raw").get_string("reason"), "frontmatter") << path;
    });
}

TEST(MermaidE2aCohort, EveryDrawRowDrawsAndEveryBlockedRowKeepsSourceAt240) {
    auto ledger = parse_ledger();
    const auto rows = read_rows(ledger.root());
    int drawn = 0, blocked = 0;
    for (const auto& row : rows) {
        const auto source = read_file(kCases / row.path);
        const bool is_drawn = drawn_native(source);
        if (row.prediction == "predicted-draw") {
            ++drawn;
            EXPECT_TRUE(is_drawn) << "registered draw did not render: " << row.path;
        } else {
            ++blocked;
            EXPECT_FALSE(is_drawn) << "registered blocked row rendered: " << row.path;
        }
        EXPECT_EQ(row.wide, is_drawn ? "drawn" : "source");
    }
    // D-435 moved the titled gitGraph row from blocked to drawn; D-461 moved
    // the two layout-budget next-line-brace rows and stage 3 the last one.
    EXPECT_EQ(drawn, 64);
    // D-435 retired the title blocker; D-461 retired the three brace draws,
    // so exactly five rows stay blocked.
    EXPECT_EQ(blocked, 5);
}

TEST(MermaidE2aCohort, DescriptionIsNeverRendered) {
    auto ledger = parse_ledger();
    for (const auto& row : read_rows(ledger.root())) {
        if (row.prediction != "predicted-draw") continue;
        const auto source = read_file(kCases / row.path);
        const auto token = description_token(source);
        if (token.empty()) continue;
        const auto native = render_text(source, "mermaid");
        EXPECT_EQ(native.find(token), std::string::npos) << row.path << ": " << token;
    }
}

namespace {
// A frontmatter original whose envelope carries config or a body init
// directive is not in the E2a scope; it must keep whole source.
bool has_config_or_init(const std::string& source) {
    if (!source.starts_with("---\n")) return false;
    std::istringstream in(source);
    std::string line;
    bool skipped_open = false, closed = false;
    while (std::getline(in, line)) {
        if (!skipped_open) {
            skipped_open = true;
            continue;
        }
        if (!closed) {
            if (line == "---") {
                closed = true;
                continue;
            }
            if (line == "config:") return true;
        } else if (line.find("%%{") != std::string::npos) {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST(MermaidE2aCohort, ConfigAndInitRowsKeepWholeSource) {
    std::set<std::string> cohort;
    auto ledger = parse_ledger();
    ledger.root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    // RFC 0031 E2b admits a reviewed set of state inert-config originals to
    // native drawing; those rows are expected to change surface, so they are
    // outside this whole-source scan just like the E2a ledger rows. RFC 0038
    // adds the reviewed state look/theme cohort (test_mermaid_state_0038_cohort
    // is its per-row gate).
    auto e2b = json::parse(read_file(kE2bLedger));
    ASSERT_TRUE(e2b.has_value()) << "E2b cohort ledger must parse";
    e2b->root().get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-draw")
            cohort.insert(row.get_string("path"));
    });
    auto rfc0038 = json::parse(read_file(kRfc0038Ledger));
    ASSERT_TRUE(rfc0038.has_value()) << "RFC 0038 cohort ledger must parse";
    rfc0038->root().get("rows").iter([&](const JsonVal row) {
        cohort.insert(row.get_string("path"));
    });
    // RFC 0039 adds the reviewed gitGraph look/theme cohort; the per-row gate is
    // test_mermaid_gitgraph_0039_cohort. Track the config/init-bearing git rows
    // it removes so the skipped set can be pinned to the ledger below.
    auto rfc0039 = json::parse(read_file(kRfc0039Ledger));
    ASSERT_TRUE(rfc0039.has_value()) << "RFC 0039 cohort ledger must parse";
    std::set<std::string> git_admitted;
    rfc0039->root().get("rows").iter(
        [&](const JsonVal row) { git_admitted.insert(row.get_string("path")); });
    // RFC 0045 adds the reviewed gitGraph `parallelCommits` namespace cohort;
    // the per-row gate is test_mermaid_git_0045_cohort. Same treatment: these
    // config-bearing git rows leave the whole-source scan.
    auto rfc0045 = json::parse(read_file(kRfc0045Ledger));
    ASSERT_TRUE(rfc0045.has_value()) << "RFC 0045 cohort ledger must parse";
    rfc0045->root().get("rows").iter(
        [&](const JsonVal row) { git_admitted.insert(row.get_string("path")); });
    // D-424 admits the reviewed gitGraph closed-boolean namespace cohort; the
    // per-row gate is test_mermaid_git_bool_d424_cohort. Same treatment: these
    // config-bearing git rows leave the whole-source scan, and the set
    // equality below pins them exactly.
    auto d424 = json::parse(read_file(kD424Ledger));
    ASSERT_TRUE(d424.has_value()) << "D-424 cohort ledger must parse";
    d424->root().get("rows").iter(
        [&](const JsonVal row) { git_admitted.insert(row.get_string("path")); });
    // D-436 admits 14 reviewed gitGraph trunk rows (`config.gitGraph.mainBranchName`
    // / `mainBranchOrder`, honoured); the per-row gate is
    // test_mermaid_git_mainbranch_d436_cohort. All 14 are config-bearing, so
    // they leave the whole-source scan and the set equality below pins them.
    auto d436 = json::parse(read_file(kD436Ledger));
    ASSERT_TRUE(d436.has_value()) << "D-436 cohort ledger must parse";
    d436->root().get("rows").iter(
        [&](const JsonVal row) { git_admitted.insert(row.get_string("path")); });
    cohort.insert(git_admitted.begin(), git_admitted.end());
    // RFC 0048 admits the reviewed State concurrent-region cohort; the per-row
    // gate is test_mermaid_state_region_0048_cohort. Only the config/init-bearing
    // subset is scanned here (the divider-only row declares neither), so the
    // skip-set pin below is over that subset, never the full ledger.
    auto rfc0048 = json::parse(read_file(kRfc0048Ledger));
    ASSERT_TRUE(rfc0048.has_value()) << "RFC 0048 cohort ledger must parse";
    std::set<std::string> region_admitted;
    rfc0048->root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        cohort.insert(rel);
        if (rel.rfind("state-diagram", 0) == 0) region_admitted.insert(rel);
    });
    // RFC 0046 admits the 4 state `state.useMaxWidth` tuning rows; the state
    // side of the skip set is pinned to the ledger below.
    auto rfc0046 = json::parse(read_file(kRfc0046Ledger));
    ASSERT_TRUE(rfc0046.has_value()) << "RFC 0046 cohort ledger must parse";
    std::set<std::string> state_admitted;
    rfc0046->root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        cohort.insert(rel);
        if (rel.rfind("state-diagram", 0) == 0) state_admitted.insert(rel);
    });
    // D-441 admits the 3 reviewed state `config.layout: elk` rows; the per-row
    // gate is test_mermaid_state_layout_d441_cohort. They leave this scan and
    // the state side of the skip set is pinned to that ledger below.
    auto d441 = json::parse(read_file(kD441Ledger));
    ASSERT_TRUE(d441.has_value()) << "D-441 cohort ledger must parse";
    d441->root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        cohort.insert(rel);
        if (rel.rfind("state-diagram", 0) == 0) state_admitted.insert(rel);
    });
    // D-446 admits the state row whose body opens with `%%` comment lines; the
    // per-row gate is test_mermaid_state_leading_comment_d446_cohort.
    auto d446 = json::parse(read_file(kD446Ledger));
    ASSERT_TRUE(d446.has_value()) << "D-446 cohort ledger must parse";
    d446->root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        cohort.insert(rel);
        if (rel.rfind("state-diagram", 0) == 0) state_admitted.insert(rel);
    });
    // D-449 admits the four `state X: Description` originals; the per-row gate
    // is test_mermaid_state_description_d449_cohort.
    auto d449 = json::parse(read_file(kD449Ledger));
    ASSERT_TRUE(d449.has_value()) << "D-449 cohort ledger must parse";
    d449->root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        cohort.insert(rel);
        if (rel.rfind("state-diagram", 0) == 0) state_admitted.insert(rel);
    });
    // D-461 admits the ten budget-funded state originals plus the two `click`
    // and one `font-style` rows; the per-row gate is
    // test_mermaid_state_d461_cohort. The config-bearing subset leaves this
    // whole-source scan; `checked` below is pinned to the post-promotion value.
    auto d461 = json::parse(read_file(kD461Ledger));
    ASSERT_TRUE(d461.has_value()) << "D-461 cohort ledger must parse";
    d461->root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    std::set<std::string> git_skipped;
    std::set<std::string> state_skipped;
    std::set<std::string> skipped_by_rfc0048;
    static constexpr const char* kRoots[] = {"git", "state-diagram", "state-diagram-v2"};
    int checked = 0;
    for (const char* root_name : kRoots) {
        for (auto it = fs::recursive_directory_iterator(kCases / root_name);
             it != fs::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file() || it->path().extension() != ".mmd") continue;
            const auto rel = fs::relative(it->path(), kCases).generic_string();
            const auto source = read_file(it->path());
            if (!source.starts_with("---\n") || !has_config_or_init(source)) continue;
            if (cohort.contains(rel)) {
                if (region_admitted.contains(rel)) skipped_by_rfc0048.insert(rel);
                if (std::string_view(root_name) == "git" && git_admitted.contains(rel))
                    git_skipped.insert(rel);
                if (std::string_view(root_name) != "git" && state_admitted.contains(rel))
                    state_skipped.insert(rel);
                continue;
            }
            // Out-of-E2a-scope frontmatter rows must not be admitted by the
            // routing correction; they keep the same source surface as text.
            EXPECT_TRUE(same_source_surface(capture_cells(source, "mermaid"),
                                            capture_cells(source, "text")))
                << "config/init row changed surface: " << rel;
            ++checked;
        }
    }
    // RFC 0048 population migration: the bound is replaced by the exact
    // post-promotion value, which is strictly stronger than the old 100 bound.
    // Measured 80 = 104 - 24: 16 config-bearing rows leave at P1 (the two E2b
    // concurrency rows are themselves cohort members, so they are counted
    // once, not twice) and the 8 init-bearing P2 rows leave at P2.
    // D-424 population migration: 18 config-bearing git rows leave for the
    // closed-boolean cohort, so the exact value drops 80 -> 62. D-436 moves
    // the 14 trunk-config rows out too: 62 -> 48. D-441 moves the 3 state
    // layout rows out: 48 -> 45. D-446/D-449 move their 5 state rows out:
    // 45 -> 40. D-461 moves 11 config-bearing state rows out (the eight
    // user-styled composite containers, the two clickable-tooltip rows and the
    // multiple-class row): 40 -> 29. D-461 stage 3 moves five more
    // config-bearing rows out (the four basic-states look/theme rows and the
    // labeled multiple-transitions row): 29 -> 24.
    EXPECT_EQ(checked, 24);
    EXPECT_EQ(skipped_by_rfc0048.size(), 24u);
    // Under-listing fails here (an admitted row that really changes surface is
    // scanned again); over-listing a missing or non-config-bearing path fails
    // the local set equality above, and an admitted-without-drawing row fails
    // test_mermaid_gitgraph_0039_cohort (or test_mermaid_git_0045_cohort).
    EXPECT_EQ(git_skipped, git_admitted);
    EXPECT_EQ(state_skipped, state_admitted);
}
