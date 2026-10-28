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
using json::JsonVal;

const fs::path kTestsDir{LOOM_TESTS_DIR};
const fs::path kCases = kTestsDir / "fixtures/mermaid_official/cases";
const fs::path kManifest = kTestsDir / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = kTestsDir / "../tools/mermaid/e2a-cohort.json";
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
    const auto text = render_text(source, "mermaid");
    return text.find("stateDiagram") == std::string::npos &&
           text.find("gitGraph") == std::string::npos;
}

JsonVal ledger_root() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse ledger");
    return parsed->root();
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
    const auto root = ledger_root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_e2a_cohort/v1");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 69);
    EXPECT_EQ(counts.get_int("git"), 64);
    EXPECT_EQ(counts.get_int("state_v2"), 5);
    EXPECT_EQ(counts.get_int("state_v1_qualified"), 0);
    EXPECT_EQ(counts.get_int("predicted_draw"), 56);
    EXPECT_EQ(counts.get_int("predicted_blocked"), 13);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");

    std::set<std::string> vocabulary;
    root.get("blocker_vocabulary").iter([&](const JsonVal value) {
        vocabulary.emplace(value.as_str());
    });
    EXPECT_EQ(vocabulary.size(), 7u);
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
    ledger_root().get("rows").iter([&](const JsonVal row) {
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
    const auto rows = read_rows(ledger_root());
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
    EXPECT_EQ(drawn, 56);
    EXPECT_EQ(blocked, 13);
}

TEST(MermaidE2aCohort, DescriptionIsNeverRendered) {
    for (const auto& row : read_rows(ledger_root())) {
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
    ledger_root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    static constexpr const char* kRoots[] = {"git", "state-diagram", "state-diagram-v2"};
    int checked = 0;
    for (const char* root_name : kRoots) {
        for (auto it = fs::recursive_directory_iterator(kCases / root_name);
             it != fs::recursive_directory_iterator(); ++it) {
            if (!it->is_regular_file() || it->path().extension() != ".mmd") continue;
            const auto rel = fs::relative(it->path(), kCases).generic_string();
            if (cohort.contains(rel)) continue;
            const auto source = read_file(it->path());
            if (!source.starts_with("---\n") || !has_config_or_init(source)) continue;
            // Out-of-E2a-scope frontmatter rows must not be admitted by the
            // routing correction; they keep the same source surface as text.
            EXPECT_TRUE(same_source_surface(capture_cells(source, "mermaid"),
                                            capture_cells(source, "text")))
                << "config/init row changed surface: " << rel;
            ++checked;
        }
    }
    EXPECT_GT(checked, 100);
}
