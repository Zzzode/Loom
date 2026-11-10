// RFC 0031 FS-6 durable FontAwesome-icon cohort gate. Public API only (no
// module declaration): enforces tools/mermaid/fs6-cohort.json against the live
// renderer and the pinned manifest. Every predicted-draw body renders
// natively with the fa:fa-<name> marker projected to one fixed marker cell
// (U+25A0) and the human label text retained; the marker syntax never leaks.
// Per-width outcomes on both tracks must match the ledger exactly (the
// generator pins which simple graphs also paint at narrow_40), and the two
// wide frames must be pixel-identical. Blocked bodies and non-admitted whole
// originals keep ordinary source.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/fs6-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 500;
constexpr const char* kMarker = "\xE2\x96\xA0";  // U+25A0

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source +
           "\n```\n\nFS6_COHORT_SENTINEL";
}
struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};
CellCapture capture(const std::string& source, const char* language, int width) {
    auto element = loom::ui::render_markdown(fenced(source, language));
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
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
struct Row {
    std::string path;
    bool draw = false;
    bool body_only = false;
    std::array<std::string, 3> original_states;
    std::array<std::string, 3> body_states;
    std::vector<std::string> icons;
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
        row.body_only = value.has("track_scope") &&
                        value.get_string("track_scope") == "body-only";
        row.original_states = load(value.get("expected_states"));
        row.body_states = load(value.get("expected_body_states"));
        value.get("icons").iter([&](const JsonVal icon) {
            row.icons.emplace_back(std::string{icon.as_str()});
        });
        rows.push_back(std::move(row));
    });
    return rows;
}
std::string body_after_frontmatter(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}
}  // namespace

TEST(MermaidFs6Cohort, LedgerMatchesSchemaReviewAndCounts) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_fs6_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 54);
    EXPECT_EQ(counts.get_int("predicted_draw"), 38);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 16);
    EXPECT_EQ(counts.get_int("predicted_original_draw"), 21);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 2u);
    EXPECT_TRUE(vocabulary.contains("flow-fa-unknown-icon"));
    EXPECT_TRUE(vocabulary.contains("flow-fa-theme-config"));

    // RFC 0031 FS-init: the four legacy theme fixtures migrated from
    // blocked to body-only draw; their whole originals stay source.
    const std::set<std::string> kFsInitRows{
        "flowchart/visual-check/dark-theme-legacy-graph.mmd",
        "flowchart/visual-check/default-theme-legacy-graph.mmd",
        "flowchart/visual-check/forest-theme-legacy-graph.mmd",
        "flowchart/visual-check/neutral-theme-legacy-graph.mmd"};

    // Every draw row binds only the closed icon vocabulary; blocked rows
    // carrying the unknown-icon blocker must name at least one other icon.
    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](const JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    int draw = 0, blocked = 0, original_draw = 0, body_only = 0, fs_init = 0;
    for (const auto& row : read_rows(root)) {
        ASSERT_NE(manifest_rows.find(row.path), manifest_rows.end()) << row.path;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / row.path)),
                  manifest_rows.find(row.path)->second.get_string("sha256"))
            << row.path;
        if (row.draw) {
            ++draw;
            for (const auto& icon : row.icons)
                EXPECT_TRUE(icon == "car" || icon == "code") << icon << " " << row.path;
            if (row.original_states[0] == "drawn") ++original_draw;
            if (row.body_only) {
                ++body_only;
                // A body-only row keeps the whole original source at every width.
                EXPECT_EQ(row.original_states[0], "source") << row.path;
                EXPECT_EQ(row.original_states[1], "source") << row.path;
                EXPECT_EQ(row.original_states[2], "source") << row.path;
            }
            if (kFsInitRows.contains(row.path)) {
                ++fs_init;
                EXPECT_TRUE(row.body_only) << row.path;
            }
        } else {
            ++blocked;
            EXPECT_FALSE(row.body_only) << row.path;
        }
    }
    EXPECT_EQ(draw, 38);
    EXPECT_EQ(blocked, 16);
    EXPECT_EQ(original_draw, 21);
    EXPECT_EQ(body_only, 17);
    EXPECT_EQ(fs_init, 4);
}

TEST(MermaidFs6Cohort, BodyStatesMatchLedgerWithMarkerProjection) {
    const auto ledger = parse_doc(kLedger);
    int draw = 0;
    for (const auto& row : read_rows(ledger.root())) {
        const auto body = body_after_frontmatter(read_file(kCases / row.path));
        const CellCapture wide = capture(body, "mermaid", kWide);
        const CellCapture narrow = capture(body, "mermaid", kNarrow);
        const CellCapture restored = capture(body, "mermaid", kWide);
        EXPECT_EQ(draws(body, kWide), row.body_states[0] == "drawn") << row.path;
        EXPECT_EQ(draws(body, kNarrow), row.body_states[1] == "drawn") << row.path;
        if (row.draw) {
            ++draw;
            ASSERT_FALSE(same_surface(wide, capture(body, "text", kWide))) << row.path;
            // The marker syntax is consumed and replaced by the fixed cell.
            EXPECT_EQ(wide.text.find("fa:fa-"), std::string::npos) << row.path;
            EXPECT_NE(wide.text.find(kMarker), std::string::npos) << row.path;
            EXPECT_TRUE(same_surface(wide, restored)) << "restore changed pixels: " << row.path;
        } else {
            EXPECT_TRUE(same_surface(wide, capture(body, "text", kWide))) << row.path;
        }
    }
    EXPECT_EQ(draw, 38);
}

TEST(MermaidFs6Cohort, WholeOriginalStatesMatchLedger) {
    const auto ledger = parse_doc(kLedger);
    int original_draw = 0;
    for (const auto& row : read_rows(ledger.root())) {
        const auto bytes = read_file(kCases / row.path);
        const CellCapture wide = capture(bytes, "mermaid", kWide);
        const CellCapture narrow = capture(bytes, "mermaid", kNarrow);
        const CellCapture restored = capture(bytes, "mermaid", kWide);
        EXPECT_EQ(draws(bytes, kWide), row.original_states[0] == "drawn") << row.path;
        EXPECT_EQ(draws(bytes, kNarrow), row.original_states[1] == "drawn") << row.path;
        EXPECT_TRUE(same_surface(wide, restored)) << row.path;
        if (row.original_states[0] == "drawn") {
            ++original_draw;
            // A drawn whole original never leaks the marker syntax.
            EXPECT_EQ(wide.text.find("fa:fa-"), std::string::npos) << row.path;
        } else {
            EXPECT_TRUE(same_surface(wide, capture(bytes, "text", kWide))) << row.path;
        }
    }
    EXPECT_EQ(original_draw, 21);
}
