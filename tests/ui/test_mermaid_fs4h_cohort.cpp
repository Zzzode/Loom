// RFC 0031 FS-4h durable inline-HTML label cohort gate. Public API only (no
// module declaration): enforces tools/mermaid/fs4h-cohort.json against the
// live renderer and the pinned manifest. Draw bodies render at every frame
// (240 wide, 40 narrow and restored wide) with <strong>/<b> wrappers removed
// and inner text retained; the tag markup never leaks. The two <img>
// fixtures and the non-admitted whole originals keep ordinary source.
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs4h-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 500;

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
        "```" + std::string(language) + "\n" + source +
        "\n```\n\nFS4H_COHORT_SENTINEL";
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
bool same_surface(const CellCapture& a, const CellCapture& b) {
    return a.chars == b.chars;
}
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
struct Row {
    std::string path;
    bool draw = false;
    std::array<std::string, 3> original_states;
    std::array<std::string, 3> body_states;
    std::vector<std::string> tags;
};
std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    auto load = [](JsonVal states) {
        return std::array<std::string, 3>{states.get_string("wide_240"),
                                          states.get_string("narrow_40"),
                                          states.get_string("restored_240")};
    };
    root.get("rows").iter([&](const JsonVal value) {
        Row row;
        row.path = value.get_string("path");
        row.draw = value.get_string("prediction") == "predicted-draw";
        row.original_states = load(value.get("expected_states"));
        row.body_states = load(value.get("expected_body_states"));
        value.get("tags").iter(
            [&](const JsonVal tag) { row.tags.emplace_back(std::string{tag.as_str()}); });
        rows.push_back(std::move(row));
    });
    return rows;
}
std::string body_after_frontmatter(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidFs4hCohort, LedgerMatchesSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs4h_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 10);
    EXPECT_EQ(counts.get_int("predicted_draw"), 8);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 2);
    EXPECT_EQ(counts.get_int("predicted_original_draw"), 5);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](const JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary, (std::set<std::string>{"flow-html-image-label"}));

    int draw = 0, blocked = 0, original_draw = 0;
    for (const auto& row : read_rows(root)) {
        ASSERT_NE(manifest_rows.find(row.path), manifest_rows.end()) << row.path;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / row.path)),
                  manifest_rows.find(row.path)->second.get_string("sha256"))
            << row.path;
        if (row.draw) {
            ++draw;
            EXPECT_EQ(row.tags, (std::vector<std::string>{"strong"})) << row.path;
            if (row.original_states[0] == "drawn") ++original_draw;
        } else {
            ++blocked;
            EXPECT_TRUE(std::ranges::find(row.tags, "img") != row.tags.end()) << row.path;
        }
    }
    EXPECT_EQ(draw, 8);
    EXPECT_EQ(blocked, 2);
    EXPECT_EQ(original_draw, 5);
}

TEST(MermaidFs4hCohort, BodyStatesMatchLedgerAndMarkupIsConsumed) {
    const auto ledger = parse_doc(kLedger);
    for (const auto& row : read_rows(ledger.root())) {
        const auto body = body_after_frontmatter(read_file(kCases / row.path));
        EXPECT_EQ(draws(body, kWide), row.body_states[0] == "drawn") << row.path;
        EXPECT_EQ(draws(body, kNarrow), row.body_states[1] == "drawn") << row.path;
        const CellCapture wide = capture(body, "mermaid", kWide);
        const CellCapture restored = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, restored)) << row.path;
        if (row.draw) {
            ASSERT_FALSE(same_surface(wide, capture(body, "text", kWide))) << row.path;
            // Inner text retained, tag markup consumed.
            EXPECT_NE(wide.text.find("Haiya"), std::string::npos);
            EXPECT_EQ(wide.text.find("<strong"), std::string::npos) << row.path;
            EXPECT_EQ(wide.text.find("</strong>"), std::string::npos) << row.path;
        } else {
            EXPECT_TRUE(same_surface(wide, capture(body, "text", kWide))) << row.path;
        }
    }
}

TEST(MermaidFs4hCohort, WholeOriginalStatesMatchLedger) {
    const auto ledger = parse_doc(kLedger);
    int original_draw = 0;
    for (const auto& row : read_rows(ledger.root())) {
        const auto bytes = read_file(kCases / row.path);
        EXPECT_EQ(draws(bytes, kWide), row.original_states[0] == "drawn") << row.path;
        EXPECT_EQ(draws(bytes, kNarrow), row.original_states[1] == "drawn") << row.path;
        const CellCapture wide = capture(bytes, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, capture(bytes, "mermaid", kWide))) << row.path;
        if (row.original_states[0] == "drawn") {
            ++original_draw;
            EXPECT_EQ(wide.text.find("<strong"), std::string::npos) << row.path;
        } else {
            EXPECT_TRUE(same_surface(wide, capture(bytes, "text", kWide))) << row.path;
        }
    }
    EXPECT_EQ(original_draw, 5);
}
