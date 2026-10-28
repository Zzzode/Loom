// Direct assertion port from the complete pinned Termaid GitGraph test file.
// The independent Native30 cell oracle owns geometry; this unit preserves the
// original parser/output predicates and reports unmapped API options as debt.
// Parser reach comes solely from the loom.ui.visual.mermaid.testing seam
// facade (RFC 0031 E6a).
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

import std;
import loom.ui.visual.markdown;
import loom.ui.visual.mermaid.testing;
import loom.serdes.json;
import loom.crypto.crypto;

namespace {
namespace original_git = loom::ui::mermaid::testing;
namespace original_json = loom::utils::json;
using OriginalValue = original_json::JsonVal;
const std::filesystem::path kOriginalTests{LOOM_TESTS_DIR};
constexpr std::string_view kOriginalAsset =
    "fixtures/mermaid_native/termaid_gitgraph39_original_assertions.v1.json";
constexpr std::string_view kOriginalAssetSha =
    "d32e6b4bd15ffe93f38d03a4e42b08beed9b0ea3beb3e201fe5cfca02d3e4140";
constexpr std::string_view kOriginalSource =
    "fixtures/mermaid_terminal/termaid/upstream/tests/test_gitgraph.py";
constexpr std::string_view kOriginalSourceSha =
    "a235082e5e9b5172b4cd1a883fd5f3015fd84f168266adc494b359774029eec8";
constexpr std::string_view kOriginalFollowing = "AFTER_GITGRAPH_ORIGINAL";

void original_need(bool value, std::string_view why) {
    if (!value) throw std::runtime_error(std::string(why));
}

struct OriginalWork {
    std::size_t remaining;

    void spend(std::size_t amount) {
        original_need(amount <= remaining, "Termaid GitGraph assertion-port Work exhausted");
        remaining -= amount;
    }
};

std::string original_sha(std::string_view bytes) {
    return loom::utils::crypto::sha256(bytes);
}

std::string original_read(const std::filesystem::path& path, std::size_t cap, OriginalWork& work) {
    const auto bytes = std::filesystem::file_size(path);
    original_need(bytes <= cap, "Termaid GitGraph immutable input exceeds bound");
    work.spend(static_cast<std::size_t>(bytes) * 8u + 2048u);
    std::ifstream input(path, std::ios::binary);
    original_need(input.good(), "Termaid GitGraph immutable input cannot be opened");
    std::string data{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    original_need(data.size() == bytes, "Termaid GitGraph immutable input changed while read");
    return data;
}

std::size_t original_number(OriginalValue value) {
    original_need(value.is_int() && value.as_int() >= 0,
                  "Termaid GitGraph source interval integer");
    return static_cast<std::size_t>(value.as_int());
}

auto original_document() -> OriginalValue {
    static const auto document = [] {
        OriginalWork setup{8u * 1024u * 1024u};
        const auto source = original_read(kOriginalTests / kOriginalSource, 65536u, setup);
        const auto asset = original_read(kOriginalTests / kOriginalAsset, 512u * 1024u, setup);
        original_need(original_sha(source) == kOriginalSourceSha &&
                          original_sha(asset) == kOriginalAssetSha,
                      "Termaid GitGraph immutable source/asset digest");
        setup.spend(asset.size() * 32u + source.size() * 8u + 16384u);
        auto parsed = original_json::parse(asset);
        original_need(parsed.has_value(), "Termaid GitGraph source-contract JSON");
        const auto root = parsed->root();
        original_need(root.get_string("schema") ==
                              "loom_termaid_gitgraph39_original_assertions/v1" &&
                          root.get_string("upstream_source_sha256") == kOriginalSourceSha &&
                          root.get("cases").is_arr() && root.get("cases").size() == 39u,
                      "Termaid GitGraph complete source-contract schema");
        std::set<std::string> functions;
        std::size_t sites = 0u;
        const auto interval = [&](OriginalValue row, std::string_view first, std::string_view last,
                                  std::string_view payload, std::string_view digest) {
            const auto begin = original_number(row.get(first)),
                       end = original_number(row.get(last));
            original_need(begin <= end && end <= source.size(),
                          "Termaid GitGraph complete source interval");
            const auto bytes = std::string_view(source).substr(begin, end - begin);
            original_need(bytes == row.get_string(payload) &&
                              original_sha(bytes) == row.get_string(digest),
                          "Termaid GitGraph source interval payload/digest");
        };
        root.get("cases").iter([&](OriginalValue row) {
            original_need(functions.insert(row.get_string("id")).second,
                          "Termaid GitGraph duplicate original function");
            interval(row, "function_start_byte", "function_end_byte", "function_source",
                     "function_sha256");
            const auto raw = row.get_string("raw_source");
            original_need(raw.size() == original_number(row.get("raw_bytes")) &&
                              original_sha(raw) == row.get_string("raw_sha256"),
                          "Termaid GitGraph exact literal API argument");
            const auto assertions = row.get("assertion_sites");
            original_need(assertions.is_arr(), "Termaid GitGraph original assertion ledger");
            assertions.iter([&](OriginalValue assertion) {
                interval(assertion, "start_byte", "end_byte", "source", "sha256");
                ++sites;
            });
        });
        original_need(functions.size() == 39u && sites == 81u,
                      "Termaid GitGraph whole-file function/assertion conservation");
        return parsed;
    }();
    return document->root();
}

OriginalValue original_case(std::string_view family, std::string_view function) {
    const auto id = std::string(family) + "." + std::string(function);
    OriginalValue selected{};
    original_document().get("cases").iter([&](OriginalValue row) {
        if (row.get_string("id") == id) selected = row;
    });
    original_need(selected.is_obj(), "Termaid GitGraph original function is missing");
    ::testing::Test::RecordProperty("upstream_function", id);
    ::testing::Test::RecordProperty("upstream_source_sha256", std::string(kOriginalSourceSha));
    ::testing::Test::RecordProperty("original_assertion_sites",
                                    static_cast<int>(selected.get("assertion_sites").size()));
    return selected;
}

original_git::GitGraph original_parse(std::string_view function) {
    const auto row = original_case("TestGitGraphParser", function);
    original_need(row.get_string("api") == "parse_git_graph" &&
                      row.get("original_options").size() == 0u,
                  "Termaid GitGraph parser call/options identity");
    original_git::Work work;
    auto graph = original_git::parse_gitgraph(row.get_string("raw_source"), work);
    original_need(graph.has_value(), "Termaid GitGraph original parser argument was rejected");
    return std::move(*graph);
}

struct OriginalOutput {
    std::vector<std::string> rows;
    std::string text;
    std::size_t remaining = 0u;
};

std::vector<std::string> original_capture(ftxui::Element element, OriginalWork& work) {
    constexpr int width = 240, height = 128;
    constexpr auto cells = static_cast<std::size_t>(width * height);
    work.spend(cells * (sizeof(ftxui::Pixel) + 8u * sizeof(std::string) + 256u) +
               static_cast<std::size_t>(height) * (sizeof(std::string) + 4u * width) + 16384u);
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, std::move(element));
    std::vector<std::string> rows;
    rows.reserve(height);
    for (int y = 0; y < height; ++y) {
        std::string row;
        row.reserve(4u * width);
        for (int x = 0; x < width; ++x)
            row += screen.PixelAt(x, y).character;
        rows.push_back(std::move(row));
    }
    return rows;
}

OriginalOutput original_render_source(std::string_view source, bool use_ascii = false) {
    OriginalWork work{128u * 1024u * 1024u};
    work.spend((source.size() + 256u) * (8u * sizeof(std::string) + 128u) + 16384u);
    const auto fence = [&](std::string_view language) {
        std::string text;
        text.reserve(source.size() + language.size() + kOriginalFollowing.size() + 16u);
        text.append("```")
            .append(language)
            .append("\n")
            .append(source)
            .append("\n```\n\n")
            .append(kOriginalFollowing);
        return text;
    };
    std::vector<std::string> drawn, ordinary;
    if (!use_ascii) {
        drawn = original_capture(loom::ui::render_markdown(fence("mermaid")), work);
        ordinary = original_capture(loom::ui::render_markdown(fence("text")), work);
    } else {
        std::string source_fence;
        source_fence.reserve(source.size() + 24u);
        source_fence.append("```text\n").append(source).append("\n```");
        auto fallback = loom::ui::render_markdown(source_fence);
        auto diagram = loom::ui::mermaid::render_mermaid(source, fallback, true);
        original_need(diagram.has_value(),
                      "Termaid GitGraph original ASCII option has no admitted public profile");
        drawn = original_capture(
            ftxui::vbox({*diagram, ftxui::text(std::string(kOriginalFollowing))}), work);
        ordinary = original_capture(
            ftxui::vbox({fallback, ftxui::text(std::string(kOriginalFollowing))}), work);
    }
    original_need(drawn != ordinary,
                  "Termaid GitGraph original renderer argument fell back to source");
    auto following = drawn.end();
    std::size_t following_count = 0u;
    for (auto it = drawn.begin(); it != drawn.end(); ++it)
        if (it->find(kOriginalFollowing) != std::string::npos) {
            following = it;
            ++following_count;
        }
    original_need(following_count == 1u,
                  "Termaid GitGraph public capture lost/duplicated following content");
    drawn.erase(following, drawn.end());
    for (auto& line : drawn)
        while (!line.empty() && line.back() == ' ')
            line.pop_back();
    while (!drawn.empty() && drawn.back().empty())
        drawn.pop_back();
    work.spend(4u * 240u * 128u + 4096u);
    OriginalOutput output{std::move(drawn), {}};
    for (const auto& line : output.rows) {
        if (!output.text.empty()) output.text += '\n';
        output.text += line;
    }
    output.remaining = work.remaining;
    return output;
}

OriginalOutput original_render(std::string_view function) {
    const auto row = original_case("TestGitGraphRendering", function);
    const auto options = row.get("original_options");
    const bool use_ascii = options.size() == 1u && options.get("use_ascii").is_bool() &&
                           options.get("use_ascii").as_bool();
    original_need(row.get_string("api") == "render" && (options.size() == 0u || use_ascii),
                  "Termaid GitGraph renderer API option has no mapped Loom public path");
    return original_render_source(row.get("raw_source").as_str(), use_ascii);
}

bool original_contains(std::string_view output, std::string_view needle) {
    return output.find(needle) != std::string_view::npos;
}

std::size_t original_row(const OriginalOutput& output, std::string_view needle) {
    for (std::size_t n = 0; n < output.rows.size(); ++n)
        if (original_contains(output.rows[n], needle)) return n;
    throw std::runtime_error("Termaid GitGraph original output row is absent");
}

TEST(TermaidGitGraphParser, BasicCommits) {
    const auto d = original_parse("test_basic_commits");
    EXPECT_EQ(d.commits.size(), 3u);
    EXPECT_TRUE(std::ranges::all_of(d.commits, [](const auto& c) { return c.branch == "main"; }));
}

TEST(TermaidGitGraphParser, CommitAutoIds) {
    const auto d = original_parse("test_commit_auto_ids");
    EXPECT_EQ(d.commits.at(0).id, "0");
    EXPECT_EQ(d.commits.at(1).id, "1");
}

TEST(TermaidGitGraphParser, CommitExplicitId) {
    const auto d = original_parse("test_commit_explicit_id");
    EXPECT_EQ(d.commits.at(0).id, "abc");
}

TEST(TermaidGitGraphParser, CommitType) {
    const auto d = original_parse("test_commit_type");
    EXPECT_EQ(d.commits.at(0).type, original_git::GitType::Normal);
    EXPECT_EQ(d.commits.at(1).type, original_git::GitType::Reverse);
    EXPECT_EQ(d.commits.at(2).type, original_git::GitType::Highlight);
}

TEST(TermaidGitGraphParser, CommitTag) {
    const auto d = original_parse("test_commit_tag");
    EXPECT_EQ(d.commits.at(0).tags, (std::vector<std::string>{"v1.0"}));
}

TEST(TermaidGitGraphParser, Branch) {
    const auto d = original_parse("test_branch");
    EXPECT_EQ(d.branches.size(), 2u);
    EXPECT_EQ(d.branches.at(0).name, "main");
    EXPECT_EQ(d.branches.at(1).name, "develop");
    EXPECT_EQ(d.commits.at(1).branch, "develop");
}

TEST(TermaidGitGraphParser, Checkout) {
    const auto d = original_parse("test_checkout");
    EXPECT_EQ(d.commits.at(0).branch, "main");
    EXPECT_EQ(d.commits.at(1).branch, "develop");
    EXPECT_EQ(d.commits.at(2).branch, "main");
}

TEST(TermaidGitGraphParser, Switch) {
    const auto d = original_parse("test_switch");
    EXPECT_EQ(d.commits.at(2).branch, "main");
}

TEST(TermaidGitGraphParser, Merge) {
    const auto d = original_parse("test_merge");
    ASSERT_FALSE(d.commits.empty());
    const auto& merge = d.commits.back();
    EXPECT_EQ(merge.id, "m");
    EXPECT_EQ(merge.branch, "main");
    EXPECT_EQ(merge.parents.size(), 2u);
}

TEST(TermaidGitGraphParser, MergeParents) {
    const auto d = original_parse("test_merge_parents");
    ASSERT_FALSE(d.commits.empty());
    const auto& merge = d.commits.back();
    EXPECT_TRUE(std::ranges::find(merge.parents, "a") != merge.parents.end());
    EXPECT_TRUE(std::ranges::find(merge.parents, "b") != merge.parents.end());
}

TEST(TermaidGitGraphParser, CherryPick) {
    const auto d = original_parse("test_cherry_pick");
    ASSERT_FALSE(d.commits.empty());
    const auto& cp = d.commits.back();
    EXPECT_TRUE(std::ranges::find(cp.parents, "B") != cp.parents.end());
    EXPECT_EQ(cp.branch, "main");
}

TEST(TermaidGitGraphParser, DirectionLR) {
    const auto d = original_parse("test_direction_lr");
    EXPECT_EQ(d.direction, original_git::GitDirection::LR);
}

TEST(TermaidGitGraphParser, DirectionTB) {
    const auto d = original_parse("test_direction_tb");
    EXPECT_EQ(d.direction, original_git::GitDirection::TB);
}

TEST(TermaidGitGraphParser, DirectionBT) {
    const auto d = original_parse("test_direction_bt");
    EXPECT_EQ(d.direction, original_git::GitDirection::BT);
}

TEST(TermaidGitGraphParser, BranchOrder) {
    const auto d = original_parse("test_branch_order");
    EXPECT_EQ(d.branches.at(1).order, 1);
}

TEST(TermaidGitGraphParser, InitConfig) {
    const auto d = original_parse("test_init_config");
    EXPECT_EQ(d.main_branch, "master");
    EXPECT_EQ(d.commits.at(0).branch, "master");
}

TEST(TermaidGitGraphParser, CommitSequenceNumbers) {
    const auto d = original_parse("test_commit_sequence_numbers");
    EXPECT_EQ(d.commits.at(0).sequence, 0u);
    EXPECT_EQ(d.commits.at(1).sequence, 1u);
    EXPECT_EQ(d.commits.at(2).sequence, 2u);
}

TEST(TermaidGitGraphParser, BranchStartCommit) {
    const auto d = original_parse("test_branch_start_commit");
    EXPECT_EQ(d.branches.at(1).start, "a");
}

TEST(TermaidGitGraphParser, CommentsIgnored) {
    const auto d = original_parse("test_comments_ignored");
    EXPECT_EQ(d.commits.size(), 2u);
}

TEST(TermaidGitGraphParser, FrontmatterStripped) {
    const auto d = original_parse("test_frontmatter_stripped");
    EXPECT_EQ(d.commits.size(), 1u);
}

TEST(TermaidGitGraphParser, MultipleBranches) {
    const auto d = original_parse("test_multiple_branches");
    EXPECT_EQ(d.branches.size(), 3u);
    const auto has = [&](std::string_view name) {
        return std::ranges::any_of(d.branches, [&](const auto& b) { return b.name == name; });
    };
    EXPECT_TRUE(has("main"));
    EXPECT_TRUE(has("feature"));
    EXPECT_TRUE(has("hotfix"));
}

TEST(TermaidGitGraphParser, ResetToBranch) {
    const auto d = original_parse("test_reset_to_branch");
    std::optional<std::string> dev_head;
    for (const auto& c : d.commits)
        if (c.branch == "dev") dev_head = c.id;
    ASSERT_FALSE(d.commits.empty());
    const auto& last = d.commits.back();
    EXPECT_EQ(last.branch, "main");
    ASSERT_TRUE(dev_head);
    EXPECT_TRUE(std::ranges::find(last.parents, *dev_head) != last.parents.end());
}

TEST(TermaidGitGraphParser, ResetAncestor) {
    const auto d = original_parse("test_reset_ancestor");
    const auto found = std::ranges::find_if(d.commits, [](const auto& c) { return c.id == "c4"; });
    ASSERT_NE(found, d.commits.end());
    EXPECT_EQ(found->parents, (std::vector<std::string>{"c1"}));
}

TEST(TermaidGitGraphRendering, BasicLinear) {
    const auto o = original_render("test_basic_linear");
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "●"));
}

TEST(TermaidGitGraphRendering, CommitIdsDisplayed) {
    const auto o = original_render("test_commit_ids_displayed");
    EXPECT_TRUE(original_contains(o.text, "init"));
    EXPECT_TRUE(original_contains(o.text, "feat"));
}

TEST(TermaidGitGraphRendering, BranchLabelDisplayed) {
    const auto o = original_render("test_branch_label_displayed");
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "develop"));
}

TEST(TermaidGitGraphRendering, BranchLineDrawn) {
    const auto o = original_render("test_branch_line_drawn");
    EXPECT_TRUE(original_contains(o.text, "─"));
}

TEST(TermaidGitGraphRendering, MergeVerticalLine) {
    const auto o = original_render("test_merge_vertical_line");
    EXPECT_TRUE(original_contains(o.text, "│") || original_contains(o.text, "┼"));
}

TEST(TermaidGitGraphRendering, CommitTypeMarkers) {
    const auto o = original_render("test_commit_type_markers");
    EXPECT_TRUE(original_contains(o.text, "●"));
    EXPECT_TRUE(original_contains(o.text, "✖"));
    EXPECT_TRUE(original_contains(o.text, "■"));
}

TEST(TermaidGitGraphRendering, TagDisplayed) {
    const auto o = original_render("test_tag_displayed");
    EXPECT_TRUE(original_contains(o.text, "[v1.0]"));
}

TEST(TermaidGitGraphRendering, TBOrientation) {
    const auto o = original_render("test_tb_orientation");
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "develop"));
    EXPECT_GT(original_row(o, "●"), original_row(o, "main"));
}

TEST(TermaidGitGraphRendering, BTOrientation) {
    const auto o = original_render("test_bt_orientation");
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "develop"));
    EXPECT_LT(original_row(o, "●"), original_row(o, "main"));
}

TEST(TermaidGitGraphRendering, CherryPickRendered) {
    const auto o = original_render("test_cherry_pick_rendered");
    EXPECT_TRUE(original_contains(o.text, "A"));
    EXPECT_TRUE(original_contains(o.text, "B"));
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "develop"));
}

TEST(TermaidGitGraphRendering, InitConfigRendering) {
    const auto o = original_render("test_init_config_rendering");
    EXPECT_TRUE(original_contains(o.text, "master"));
    EXPECT_TRUE(original_contains(o.text, "dev"));
}

TEST(TermaidGitGraphRendering, AdaptiveSpacing) {
    const auto o = original_render("test_adaptive_spacing");
    EXPECT_TRUE(original_contains(o.text, "a"));
    EXPECT_TRUE(original_contains(o.text, "this-is-a-very-long-label"));
    EXPECT_TRUE(original_contains(o.text, "b"));
}

TEST(TermaidGitGraphRendering, FeatureBranchWorkflow) {
    const auto o = original_render("test_feature_branch_workflow");
    for (const std::string_view id : {"1", "2", "3", "4", "5", "6", "7"})
        EXPECT_TRUE(original_contains(o.text, id));
    EXPECT_TRUE(original_contains(o.text, "main"));
    EXPECT_TRUE(original_contains(o.text, "develop"));
}

TEST(TermaidGitGraphRendering, EmptyGraph) {
    const auto o = original_render("test_empty_graph");
    static_assert(std::is_same_v<decltype(o.text), std::string>);
    EXPECT_TRUE(o.text.empty());
}

TEST(TermaidGitGraphRendering, AsciiMode) {
    const auto o = original_render("test_ascii_mode");
    EXPECT_TRUE(original_contains(o.text, "o"));
    EXPECT_TRUE(original_contains(o.text, "main"));
    constexpr std::string_view forbidden_text = "┌┐└┘─│├┤┬┴┼╭╮╰╯►◄▲▼┄┆━┃╋●✖■";
    OriginalWork scan{o.remaining};
    scan.spend((o.text.size() + forbidden_text.size() + 2u) * (2u * sizeof(std::string) + 128u) +
               4096u);
    const auto forbidden = ftxui::Utf8ToGlyphs(std::string(forbidden_text));
    for (const auto& glyph : ftxui::Utf8ToGlyphs(o.text))
        EXPECT_TRUE(std::ranges::find(forbidden, glyph) == forbidden.end()) << glyph;
}

TEST(TermaidGitGraphRendering, AsciiCommitTypes) {
    const auto o = original_render("test_ascii_commit_types");
    EXPECT_TRUE(original_contains(o.text, "o"));
    EXPECT_TRUE(original_contains(o.text, "X"));
    EXPECT_TRUE(original_contains(o.text, "#"));
}

TEST(MermaidGitGraphAscii, LiteralUnicodeMarkerLabelIsPreserved) {
    const auto o = original_render_source("gitGraph\ncommit id: \"●\"", true);
    EXPECT_TRUE(original_contains(o.text, "o"));
    EXPECT_TRUE(original_contains(o.text, "●"));
}

// These are additional product regressions, separate from the 39-function
// original Termaid denominator. The visible branch rows expose actual layout
// order through the public renderer, independently of the private parser Model.
void original_branch_order(std::string_view source,
                           std::initializer_list<std::string_view> expected) {
    const auto output = original_render_source(source);
    std::optional<std::size_t> previous;
    for (const auto name : expected) {
        const auto row = original_row(output, name);
        if (previous) EXPECT_GT(row, *previous) << name;
        previous = row;
    }
}

TEST(MermaidGitGraphOrder, ExplicitPositiveFollowsUnspecifiedBranches) {
    original_branch_order("gitGraph:\ncommit\nbranch test1 order: 1\nbranch test2\nbranch test3",
                          {"main", "test2", "test3", "test1"});
}

TEST(MermaidGitGraphOrder, ExplicitZeroStablyTiesDefaultMainOrder) {
    original_branch_order(
        "gitGraph:\ncommit\nbranch test1 order: 1\nbranch test2\nbranch zero order: 0",
        {"main", "zero", "test2", "test1"});
}

TEST(MermaidGitGraphOrder, UnspecifiedDecimalIndicesMatchPinnedAst) {
    original_branch_order(
        "gitGraph:\ncommit\nbranch b01\nbranch b02\nbranch b03\nbranch b04\nbranch b05\nbranch "
        "b06\nbranch b07\nbranch b08\nbranch b09\nbranch b10\nbranch b11\nbranch b12",
        {"main", "b01", "b10", "b11", "b12", "b02", "b03", "b04", "b05", "b06", "b07", "b08",
         "b09"});
}

TEST(TermaidGitGraphComplete, EveryOriginalApiOptionRemainsMandatory) {
    std::size_t mapped = 0u, mapped_sites = 0u, pending = 0u, sites = 0u;
    original_document().get("cases").iter([&](OriginalValue row) {
        const auto assertions = row.get("assertion_sites").size();
        sites += assertions;
        const auto options = row.get("original_options");
        const bool ascii = options.size() == 1u && options.get("use_ascii").is_bool() &&
                           options.get("use_ascii").as_bool();
        if (options.size() == 0u || ascii) {
            ++mapped;
            mapped_sites += assertions;
        } else {
            ++pending;
            ADD_FAILURE() << row.get_string("id")
                          << ": original renderer option has no mapped Loom public path; its "
                             "assertions remain mandatory";
        }
    });
    EXPECT_EQ(sites, 81u);
    EXPECT_EQ(mapped + pending, 39u);
    EXPECT_EQ(mapped, 39u);
    EXPECT_EQ(mapped_sites, 81u);
}
}  // namespace
