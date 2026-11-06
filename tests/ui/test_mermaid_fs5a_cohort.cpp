// RFC 0031 FS-5a durable label-only shape-data cohort gate. Public API only:
// enforces tools/mermaid/fs5a-cohort.json against the live renderer and the
// pinned manifest. Every row renders body and whole-original at 240 wide and
// restored, with the @{ label: } syntax consumed (no "@{" leak, label text
// retained); only the pinned edge-link graph also paints at narrow_40.
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

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs5a-cohort.json";
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
        "\n```\n\nFS5A_COHORT_SENTINEL";
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
    std::array<std::string, 3> original_states;
    std::array<std::string, 3> body_states;
};
std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    auto load = [](JsonVal states) {
        return std::array<std::string, 3>{states.get_string("wide_240"),
                                          states.get_string("narrow_40"),
                                          states.get_string("restored_240")};
    };
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({value.get_string("path"), load(value.get("expected_states")),
                        load(value.get("expected_body_states"))});
    });
    return rows;
}
std::string body_after_frontmatter(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidFs5aCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs5a_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 4);
    EXPECT_EQ(counts.get_int("predicted_draw"), 4);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 0);
    EXPECT_EQ(counts.get_int("predicted_original_draw"), 4);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](const JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    for (const auto& row : read_rows(root)) {
        ASSERT_NE(manifest_rows.find(row.path), manifest_rows.end()) << row.path;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / row.path)),
                  manifest_rows.find(row.path)->second.get_string("sha256"))
            << row.path;
    }
}

TEST(MermaidFs5aCohort, BodyStatesMatchLedgerAndSyntaxConsumed) {
    const auto ledger = parse_doc(kLedger);
    for (const auto& row : read_rows(ledger.root())) {
        const auto body = body_after_frontmatter(read_file(kCases / row.path));
        EXPECT_TRUE(draws(body, kWide)) << row.path;
        EXPECT_EQ(draws(body, kNarrow), row.body_states[1] == "drawn") << row.path;
        const CellCapture wide = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, capture(body, "mermaid", kWide))) << row.path;
        EXPECT_EQ(wide.text.find("@{"), std::string::npos) << row.path;
        // The label text carried by the metadata block survives (the two
        // multi-node chains say "label for N"; the edge-link fixtures use
        // "for c"/"for E"/"for D" labels).
        const bool has_text = wide.text.find("label for") != std::string::npos ||
                              wide.text.find("for ") != std::string::npos;
        EXPECT_TRUE(has_text) << row.path;
    }
}

TEST(MermaidFs5aCohort, WholeOriginalStatesMatchLedger) {
    const auto ledger = parse_doc(kLedger);
    int narrow = 0;
    for (const auto& row : read_rows(ledger.root())) {
        const auto bytes = read_file(kCases / row.path);
        EXPECT_TRUE(draws(bytes, kWide)) << row.path;
        const bool narrow_draws = draws(bytes, kNarrow);
        EXPECT_EQ(narrow_draws, row.original_states[1] == "drawn") << row.path;
        if (narrow_draws) ++narrow;
        const CellCapture wide = capture(bytes, "mermaid", kWide);
        EXPECT_TRUE(same_surface(wide, capture(bytes, "mermaid", kWide))) << row.path;
        EXPECT_EQ(wide.text.find("@{"), std::string::npos) << row.path;
    }
    EXPECT_EQ(narrow, 1);
}
