// RFC 0035 P1 cohort gate: description+config xychart envelope admission.
// Public API only (no module declaration). The reviewed ledger
// tools/mermaid/xychart-0035-cohort.json is pre-registered by
// tools/mermaid/generate_xychart_0035_cohort.py and proven here against the
// live renderer; the matrix records the declared state in test_mermaid_official.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
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
const fs::path kLedger = kTestsDir / "../tools/mermaid/xychart-0035-cohort.json";
constexpr int kWide = 240;
constexpr int kHeight = 400;

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::string fenced(const std::string& source, const char* language) {
    return "```" + std::string(language) + "\n" + source + "\n```\n\nXY35_COHORT_SENTINEL";
}

struct CellCapture {
    std::vector<std::pair<std::string, std::string>> cells;
};

CellCapture capture_cells(const std::string& source, const char* language, int width = kWide) {
    auto element = loom::ui::render_markdown(fenced(source, language));
    CellCapture out;
    if (!element) return out;
    auto screen =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(kHeight));
    ftxui::Render(screen, element);
    out.cells.reserve(static_cast<std::size_t>(width) * kHeight);
    for (int y = 0; y < kHeight; ++y)
        for (int x = 0; x < width; ++x) {
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
    return same_source_surface(native, capture_cells(source, "mermaid"));
}

std::vector<std::string> frame_lines(const CellCapture& capture, int width) {
    std::vector<std::string> lines;
    std::string line;
    for (int index = 0; index < static_cast<int>(capture.cells.size()); ++index) {
        line += capture.cells[static_cast<std::size_t>(index)].first;
        if ((index + 1) % width == 0) {
            lines.push_back(std::move(line));
            line.clear();
        }
    }
    return lines;
}

JsonDoc parse_ledger() {
    auto parsed = json::parse(read_file(kLedger));
    if (!parsed) throw std::runtime_error("cannot parse RFC 0035 ledger");
    return std::move(*parsed);
}

struct Row {
    std::string path, phase, prediction, wide;
};

std::vector<Row> read_rows(const JsonVal& root) {
    std::vector<Row> rows;
    root.get("rows").iter([&](const JsonVal value) {
        rows.push_back({
            value.get_string("path"),
            value.get_string("phase"),
            value.get_string("prediction"),
            value.get("expected_states").get_string("wide_240"),
        });
    });
    return rows;
}

// Remove the folded description from the frontmatter, keeping the config block
// and the body: the description is documentation-only, so the projected chart
// must be byte-identical.
std::string config_only_twin(const std::string& source) {
    std::istringstream in(source);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) lines.push_back(line);
    std::size_t config_at = lines.size();
    for (std::size_t n = 1; n < lines.size(); ++n)
        if (lines[n] == "config:") {
            config_at = n;
            break;
        }
    if (config_at == lines.size()) return {};
    std::string twin = "---\n";
    for (std::size_t n = config_at; n < lines.size(); ++n) twin += lines[n] + "\n";
    return twin;
}

std::string description_token(const std::string& source) {
    std::istringstream in(source);
    std::string line, first;
    bool in_frontmatter = false, seen_description = false;
    while (std::getline(in, line)) {
        if (line == "---") {
            if (!in_frontmatter) {
                in_frontmatter = true;
                continue;
            }
            break;
        }
        if (!in_frontmatter) continue;
        if (line.starts_with("description:")) {
            seen_description = true;
            continue;
        }
        if (seen_description && line.size() > 2 && line[0] == ' ' && line[1] == ' ') {
            first = line.substr(2);
            break;
        }
    }
    return first;
}

std::string with_config(const std::string& config_lines, const std::string& body) {
    return "---\ndescription: >-\n  documentation only\nconfig:\n" + config_lines + "---\n" + body;
}

const std::string kBody =
    "xychart\n  x-axis [a, b]\n  y-axis 0 --> 10\n  bar [3, 6]\n";

bool xy_is_fallback(const std::string& source) {
    const auto native = capture_cells(source, "mermaid");
    const auto control = capture_cells(source, "text");
    return native.cells.empty() || same_source_surface(native, control);
}
}  // namespace

TEST(MermaidXychart0035Cohort, LedgerMatchesSchemaAndReviewed) {
    auto ledger = parse_ledger();
    const auto root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_xychart_0035_cohort/v1");
    EXPECT_EQ(root.get_int("rfc"), 35);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 17);
    EXPECT_EQ(counts.get_int("predicted_draw"), 17);
    EXPECT_EQ(counts.get("by_phase").get_int("P1-envelope"), 17);
    const auto phases = root.get("selection").get("enabled_phases");
    int phase_count = 0;
    phases.iter([&](const JsonVal value) {
        ++phase_count;
        EXPECT_EQ(value.as_str(), "P1-envelope");
    });
    EXPECT_EQ(phase_count, 1);
    int rows = 0;
    root.get("rows").iter([&](const JsonVal) { ++rows; });
    EXPECT_EQ(rows, 17);
}

TEST(MermaidXychart0035Cohort, RowsPinFixtureBytesAndPriorStatus) {
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
        EXPECT_EQ(row.get_string("phase"), "P1-envelope") << path;
    });
}

TEST(MermaidXychart0035Cohort, EveryRowDrawsNativelyAndMatchesItsConfigOnlyTwin) {
    auto ledger = parse_ledger();
    for (const auto& row : read_rows(ledger.root())) {
        SCOPED_TRACE(row.path);
        EXPECT_EQ(row.prediction, "predicted-draw");
        EXPECT_EQ(row.wide, "drawn");
        const auto source = read_file(kCases / row.path);
        ASSERT_TRUE(drawn_native(source)) << "registered row did not render natively";
        EXPECT_EQ(capture_cells(source, "mermaid").cells,
                  capture_cells(source, "mermaid").cells)
            << "capture is not deterministic";
        const auto twin = config_only_twin(source);
        ASSERT_FALSE(twin.empty()) << "twin construction failed";
        EXPECT_TRUE(drawn_native(twin)) << "config-only twin must stay admitted";
        EXPECT_EQ(capture_cells(source, "mermaid").cells, capture_cells(twin, "mermaid").cells)
            << "description must not change the projected chart";
        const auto token = description_token(source);
        ASSERT_FALSE(token.empty());
        bool found = false;
        for (const auto& frame_line : frame_lines(capture_cells(source, "mermaid"), kWide))
            if (frame_line.find(token) != std::string::npos) found = true;
        EXPECT_FALSE(found) << "description text rendered in the frame";
        // narrow_40 states are owned by the official matrix (E2c precedent);
        // this gate proves the wide-240 admission only.
    }
}

TEST(MermaidXychart0035Cohort, DescriptionPlusConfigBoundaryKeepsWholeSource) {
    EXPECT_FALSE(xy_is_fallback(with_config("  xyChart:\n    width: 400\n", kBody)));

    const std::vector<std::string> rejected{
        // Widened keys that belong to later phases stay closed at P1.
        with_config("  xyChart:\n    xAxis:\n      showLabel: false\n", kBody),
        with_config("  xyChart:\n    themeVariables:\n      xyChart:\n        dataLabelColor: "
                    "\"#eeeeee\"\n",
                    kBody),
        with_config("  xyChart:\n    xAxis:\n      labelRotation: 45\n", kBody),
        with_config("  xyChart:\n    plotBorderWidth: 5\n", kBody),
        with_config("  xyChart:\n    showTitle: false\n", kBody),
        // Visual/semantic keys outside the reviewed profile.
        with_config("  xyChart:\n    unknownMetric: 1\n", kBody),
        with_config("  theme: imaginary\n", kBody),
        with_config("  xyChart:\n    width: \"400\"\n", kBody),
        with_config("  xyChart:\n    width: &metric 400\n", kBody),
        with_config("  unknown:\n    width: 400\n", kBody),
        // Grammar violators around the description/config coexistence.
        "---\ndescription: >-\n  d\n  \nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ndescription: >-\n d\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ndescription: >-\n   d\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ndescription: >-\n  # indented comment\nconfig:\n  xyChart:\n    width: 400\n---\n" +
            kBody,
        "---\ndescription: >-\n  d\ntitle: t\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ntitle: t\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ndescription: >-\n  d\ndescription: >-\n  e\nconfig:\n  xyChart:\n    width: "
        "400\n---\n" + kBody,
        "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: 400\nconfig:\n  xyChart:\n  "
        "  height: 20\n---\n" + kBody,
        "---\ndescription: >-\n  d\nconfig:\n---\n" + kBody,
        "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n---\n" + kBody,
        "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: 400\n     height: 20\n---\n" +
            kBody,
        "---\ndescription: >\n  d\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        "---\ndescription: |\n  d\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        // A body init directive is a later phase (P4).
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": 400}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {}}}%%\n%%{init: "
        "{\"xyChart\": {}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n" + kBody +
            "%%{init: {\"xyChart\": {}}}%%\n",
        // Tab indentation, duplicate leaves and top-level ordering.
        "---\ndescription: >-\n\td\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody,
        with_config("  xyChart:\n    width: 400\n    width: 20\n", kBody),
        "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: 400\ntitle: t\n---\n" +
            kBody,
        "---\nconfig:\n  xyChart:\n    width: 400\ndescription: >-\n  d\n---\n" + kBody,
        // Negative metric and deeper nesting stay closed at P1.
        with_config("  xyChart:\n    plotBorderWidth: -5\n", kBody),
        with_config("  xyChart:\n    xAxis:\n      nested:\n        deep: 1\n", kBody),
    };
    for (const auto& source : rejected) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(xy_is_fallback(source)) << "source must stay on fallback";
    }
}

TEST(MermaidXychart0035Cohort, SignedValueCapabilityRowsStayOnFallback) {
    const std::vector<std::string> pinned{
        "xychart/nightly-check/should-render-negative-and-decimal-values-with-correct-labels-"
        "for-vertical-xy-chart.mmd",
        "xychart/pr-check/should-render-negative-and-decimal-values-with-correct-labels-for-"
        "horizontal-xy-chart.mmd",
    };
    for (const auto& path : pinned) {
        SCOPED_TRACE(path);
        EXPECT_TRUE(xy_is_fallback(read_file(kCases / path)));
    }
}

TEST(MermaidXychart0035Cohort, DescriptionOnlyAndConfigOnlyPathsAreUnchanged) {
    // Description-only xychart keeps the D236 admission.
    EXPECT_FALSE(xy_is_fallback("---\ndescription: >-\n  d\n---\n" + kBody));
    // Config-only xychart keeps the existing profile (including its negatives).
    EXPECT_FALSE(xy_is_fallback("---\nconfig:\n  xyChart:\n    width: 400\n---\n" + kBody));
    EXPECT_TRUE(xy_is_fallback("---\nconfig:\n  xyChart:\n    xAxis:\n      showLabel: false\n---\n" +
                               kBody));
    // CRLF is normalized before validation, matching the D236 pie path.
    EXPECT_FALSE(xy_is_fallback("---\r\ndescription: >-\r\n  d\r\nconfig:\r\n  "
                                "xyChart:\r\n    width: 400\r\n---\n" +
                                kBody));
    // A pie body with a description keeps its pie-only profile.
    const std::string pie = "pie\n  \"A\" : 1\n  \"B\" : 2\n";
    EXPECT_FALSE(xy_is_fallback("---\ndescription: >-\n  d\n---\n" + pie));
    EXPECT_TRUE(xy_is_fallback("---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: "
                               "400\n---\n" +
                               pie));
}
