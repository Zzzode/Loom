// RFC 0031 E2b durable cohort gate. Public API only (no module declaration):
// it enforces the pre-registration ledger at tools/mermaid/e2b-cohort.json
// against the live renderer and the pinned manifest. Every admitted draw row
// must also carry the state non-terminal config disclosure footer.
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/e2b-cohort.json";
constexpr int kWide = 240;
constexpr int kHeight = 400;
constexpr std::string_view kFooterHeader = "State native projection v1";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\nE2B_COHORT_SENTINEL";
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
    const auto text = render_text(source, "mermaid");
    return text.find("stateDiagram") == std::string::npos;
}

JsonDoc parse_ledger() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse ledger");
    return std::move(*parsed);
}

struct Row {
    std::string path, prediction, wide;
    std::vector<std::pair<std::string, std::string>> leaves;
};

std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    root.get("rows").iter([&](const JsonVal value) {
        Row row{value.get_string("path"), value.get_string("prediction"),
                value.get("expected_states").get_string("wide_240"), {}};
        value.get("config_leaves").iter([&](const JsonVal leaf) {
            row.leaves.emplace_back(leaf.get_string("key"), leaf.get_string("value"));
        });
        rows.push_back(std::move(row));
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

// The exact footer line the production state path appends for a whitelist leaf.
std::string footer_line(const std::string& key, const std::string& value) {
    if (key == "logLevel")
        return "config.logLevel=" + value + " -> logger only; not applied";
    if (key == "fontFamily")
        return "config.fontFamily=\"" + value + "\" -> terminal font; not applied";
    return "config.securityLevel=\"" + value + "\" -> no interaction layer; not applied";
}
}  // namespace

TEST(MermaidE2bCohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_e2b_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 61);
    EXPECT_EQ(counts.get_int("state_v2"), 38);
    EXPECT_EQ(counts.get_int("state_v1"), 23);
    EXPECT_EQ(counts.get_int("predicted_draw"), 40);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 21);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter([&](const JsonVal value) {
        vocabulary.emplace(value.as_str());
    });
    EXPECT_EQ(vocabulary.size(), 10u);
    root.get("rows").iter([&](const JsonVal row) {
        if (row.get_string("prediction") == "predicted-blocked")
            EXPECT_TRUE(vocabulary.contains(row.get("blocker").get_string("kind")));
        else
            EXPECT_TRUE(row.get("blocker").is_null());
    });
}

TEST(MermaidE2bCohort, RowsPinFixtureBytesAndPriorStatus) {
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

TEST(MermaidE2bCohort, EveryDrawRowDrawsWithFooterAndBlockedRowKeepsSourceAt240) {
    auto ledger = parse_ledger();
    const auto rows = read_rows(ledger.root());
    int drawn = 0, blocked = 0;
    for (const auto& row : rows) {
        const auto source = read_file(kCases / row.path);
        const bool is_drawn = drawn_native(source);
        if (row.prediction == "predicted-draw") {
            ++drawn;
            ASSERT_TRUE(is_drawn) << "registered draw did not render: " << row.path;
            // E2b-3: the disclosure footer names every accepted leaf.
            const auto text = render_text(source, "mermaid");
            EXPECT_NE(text.find(kFooterHeader), std::string::npos) << row.path;
            for (const auto& [key, value] : row.leaves)
                EXPECT_NE(text.find(footer_line(key, value)), std::string::npos)
                    << row.path << " missing " << key;
        } else {
            ++blocked;
            EXPECT_FALSE(is_drawn) << "registered blocked row rendered: " << row.path;
        }
        EXPECT_EQ(row.wide, is_drawn ? "drawn" : "source");
    }
    EXPECT_EQ(drawn, 40);
    EXPECT_EQ(blocked, 21);
}

TEST(MermaidE2bCohort, DescriptionIsNeverRendered) {
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

TEST(MermaidE2bCohort, ConfigFreeStateRowsHaveNoFooter) {
    // The zero-notice byte-identity guarantee: an E2a config-free state
    // original draws without any E2b disclosure line.
    static constexpr const char* kConfigFree =
        "state-diagram-v2/nightly-check/"
        "1433-should-render-a-simple-state-diagram-with-a-title.mmd";
    const auto source = read_file(kCases / kConfigFree);
    ASSERT_TRUE(drawn_native(source));
    const auto text = render_text(source, "mermaid");
    EXPECT_EQ(text.find(kFooterHeader), std::string::npos);
    EXPECT_EQ(text.find("config.logLevel"), std::string::npos);
}

TEST(MermaidE2bCohort, NonWhitelistedConfigRowsKeepWholeSource) {
    // State frontmatter rows outside the E2b whitelist (theme/look/layout/
    // state.* keys, other values, nested maps) must keep whole source.
    auto ledger = parse_ledger();
    std::set<std::string> cohort;
    ledger.root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    auto declares_config = [](const std::string& source) {
        if (!source.starts_with("---\n")) return false;
        std::istringstream in(source);
        std::string line;
        bool skipped_open = false, inside = false;
        while (std::getline(in, line)) {
            if (!skipped_open) {
                skipped_open = true;
                inside = true;
                continue;
            }
            if (inside) {
                if (line == "---") {
                    inside = false;
                    break;
                }
                if (line == "config:") return true;
            }
        }
        return false;
    };
    static constexpr const char* kRoots[] = {"state-diagram", "state-diagram-v2"};
    int checked = 0;
    for (const char* root_name : kRoots) {
        for (auto it = fs::recursive_directory_iterator(kCases / root_name);
             it != fs::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file() || it->path().extension() != ".mmd") continue;
            const auto rel = fs::relative(it->path(), kCases).generic_string();
            if (cohort.contains(rel)) continue;
            const auto source = read_file(it->path());
            if (!declares_config(source)) continue;
            EXPECT_TRUE(same_source_surface(capture_cells(source, "mermaid"),
                                            capture_cells(source, "text")))
                << "non-whitelisted config row changed surface: " << rel;
            ++checked;
        }
    }
    EXPECT_GT(checked, 50);
}
