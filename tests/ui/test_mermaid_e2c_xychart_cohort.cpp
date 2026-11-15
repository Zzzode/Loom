// RFC 0031 E2c durable cohort gate. Public API only (no module declaration):
// enforces the xychart folded-description pre-registration ledger at
// tools/mermaid/e2c-xychart-cohort.json against the live renderer and the
// pinned manifest.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/e2c-xychart-cohort.json";
constexpr int kWide = 240;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\nE2C_COHORT_SENTINEL";
}

std::string render_text(const std::string& source, const char* language) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    if (!element) return {};
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(kWide), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    return screen.ToString();
}

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
    return render_text(source, "mermaid").find("xychart") == std::string::npos;
}

JsonDoc parse_ledger() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse E2c ledger");
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

TEST(MermaidE2cXychartCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_e2c_xychart_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 16);
    EXPECT_EQ(counts.get_int("predicted_draw"), 11);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 5);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter(
        [&](const JsonVal value) { vocabulary.emplace(value.as_str()); });
    EXPECT_EQ(vocabulary.size(), 1u);
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-blocked")
            EXPECT_TRUE(vocabulary.contains(row.get("blocker").get_string("kind")));
        else
            EXPECT_TRUE(row.get("blocker").is_null());
    });
}

TEST(MermaidE2cXychartCohort, RowsPinFixtureBytesAndPriorStatus) {
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
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), row.get_string("sha256")) << path;
        EXPECT_EQ(loom::utils::crypto::sha256(bytes), found->second.get_string("sha256"))
            << path;
        EXPECT_EQ(found->second.get("raw").get_string("expected"), "source") << path;
        EXPECT_EQ(found->second.get("raw").get_string("reason"), "frontmatter") << path;
    });
}

TEST(MermaidE2cXychartCohort, EveryDrawRowDrawsAndEveryBlockedRowKeepsSourceAt240) {
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
    EXPECT_EQ(drawn, 11);
    EXPECT_EQ(blocked, 5);
}

TEST(MermaidE2cXychartCohort, DescriptionIsNeverRendered) {
    auto ledger = parse_ledger();
    for (const auto& row : read_rows(ledger.root())) {
        if (row.prediction != "predicted-draw") continue;
        const auto source = read_file(kCases / row.path);
        const auto token = description_token(source);
        if (token.empty()) continue;
        EXPECT_EQ(render_text(source, "mermaid").find(token), std::string::npos)
            << row.path << ": " << token;
    }
}

namespace {
// An xychart frontmatter row whose folded-description envelope also carries a
// title, or whose body carries an init directive, is outside the E2c
// description-only scope and must keep whole source. (Config-only rows are a
// separate, already-admitted xy_visual_envelope path and are intentionally not
// asserted here.)
bool has_title_or_init(const std::string& source) {
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
            if (line.starts_with("title:")) return true;
        } else if (line.find("%%{") != std::string::npos) {
            return true;
        }
    }
    return false;
}
}  // namespace

TEST(MermaidE2cXychartCohort, TitleAndInitRowsKeepWholeSource) {
    std::set<std::string> cohort;
    auto ledger = parse_ledger();
    ledger.root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    int checked = 0;
    for (auto it = fs::recursive_directory_iterator(kCases / "xychart");
         it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file() || it->path().extension() != ".mmd") continue;
        const auto rel = fs::relative(it->path(), kCases).generic_string();
        const auto source = read_file(it->path());
        if (!has_title_or_init(source)) continue;
        ++checked;
        EXPECT_FALSE(drawn_native(source))
            << "title/init xychart row changed surface outside E2c scope: " << rel;
    }
    EXPECT_GT(checked, 0);
}
