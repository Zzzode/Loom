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

bool drawn_native(const std::string& source) {
    const auto native = capture_cells(source, "mermaid");
    const auto control = capture_cells(source, "text");
    if (native.cells.empty() || same_source_surface(native, control)) return false;
    if (!same_source_surface(native, capture_cells(source, "mermaid"))) return false;
    for (const auto& line : frame_lines(native, kWide))
        if (line.find("xychart") != std::string::npos) return false;
    return true;
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
    EXPECT_EQ(counts.get_int("total"), 24);
    EXPECT_EQ(counts.get_int("predicted_draw"), 24);
    EXPECT_EQ(counts.get("by_phase").get_int("P1-envelope"), 17);
    EXPECT_EQ(counts.get("by_phase").get_int("P2-show-false"), 5);
    EXPECT_EQ(counts.get("by_phase").get_int("P3-data-label-color"), 1);
    EXPECT_EQ(counts.get("by_phase").get_int("P4-init"), 1);
    const auto phases = root.get("selection").get("enabled_phases");
    std::set<std::string> enabled;
    phases.iter([&](const JsonVal value) { enabled.emplace(value.as_str()); });
    EXPECT_EQ(enabled, (std::set<std::string>{"P1-envelope", "P2-show-false",
                                            "P3-data-label-color", "P4-init"}));
    // Every enabled phase must carry its own approved review record; a
    // missing record is not a silent pass.
    const auto reviews = root.get("review").get("phases");
    for (const auto& phase : enabled) {
        const auto record = reviews.get(phase);
        ASSERT_TRUE(record.is_obj()) << phase;
        EXPECT_EQ(record.get_string("verdict"), "approved") << phase;
        EXPECT_FALSE(record.get_string("reviewer_agent").empty()) << phase;
    }
    for (const auto& phase : enabled)
        EXPECT_TRUE(reviews.get(phase).is_obj()) << phase;
    int rows = 0, p1 = 0, p2 = 0;
    root.get("rows").iter([&](const JsonVal row) {
        ++rows;
        const auto phase = row.get_string("phase");
        if (phase == "P1-envelope") ++p1;
        if (phase == "P2-show-false") ++p2;
    });
    EXPECT_EQ(rows, 24);
    EXPECT_EQ(p1, 17);
    EXPECT_EQ(p2, 5);
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
        const std::string phase = row.get_string("phase");
        EXPECT_TRUE(phase == "P1-envelope" || phase == "P2-show-false" ||
                    phase == "P3-data-label-color" || phase == "P4-init")
            << path;
    });
}

TEST(MermaidXychart0035Cohort, EveryRowDrawsNativelyAndMatchesItsUnstyledTwin) {
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
        if (row.phase == "P4-init") {
            // The init row has no config block: its twin is the same source
            // without the leading init directive (D236 description-only).
            // The folded description may mention a directive as text, so the
            // init is located after the closing frontmatter fence.
            const auto fence = source.find("\n---\n");
            ASSERT_NE(fence, std::string::npos);
            const auto at = source.find("%%{", fence);
            ASSERT_NE(at, std::string::npos);
            const auto line_end = source.find('\n', at);
            ASSERT_NE(line_end, std::string::npos);
            std::string twin = source;
            twin.erase(at, line_end + 1 - at);
            EXPECT_TRUE(drawn_native(twin)) << "description-only twin must stay admitted";
            EXPECT_NE(capture_cells(source, "mermaid").cells,
                      capture_cells(twin, "mermaid").cells)
                << "chartOrientation from the init directive must apply";
        } else {
            const auto twin = config_only_twin(source);
            ASSERT_FALSE(twin.empty()) << "twin construction failed";
            EXPECT_TRUE(drawn_native(twin)) << "config-only twin must stay admitted";
            EXPECT_EQ(capture_cells(source, "mermaid").cells,
                      capture_cells(twin, "mermaid").cells)
                << "description must not change the projected chart";
        }
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

TEST(MermaidXychart0035Cohort, EnvelopeBoundaryKeepsWholeSource) {
    EXPECT_FALSE(xy_is_fallback(with_config("  xyChart:\n    width: 400\n", kBody)));

    const std::vector<std::string> rejected{
        // Widened keys that belong to later phases stay closed.
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
        // P4 init walker closure: description+config+init stays out of scope,
        // and the sibling leaf set rejects unknown keys, wrong types, empty or
        // negative values and non-numeric strings.
        "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    width: 400\n---\n%%{init: "
        "{\"xyChart\": {\"width\": 400}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"unknown\": 1}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": \"wide\"}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"plotBorderWidth\": "
        "-5}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"chartOrientation\": "
        "\"diagonal\"}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"xAxis\": []}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"other\": {}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"showTitle\": false}}}%%\n" +
            kBody,
        // Valid empty object, wrong scalar types, out-of-range metric, flags.
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": null}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": {}}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": []}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": "
        "{\"plotReservedSpacePercent\": 101}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"showLegend\": false}}}%%\n" +
            kBody,
        // Duplicate keys at root, option and axis level.
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {}, \"xyChart\": {}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": 1, "
        "\"width\": 2}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"xAxis\": {\"tickWidth\": "
        "1, \"tickWidth\": 2}}}}%%\n" +
            kBody,
        // Nested depth, empty axis and unknown axis key.
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"xAxis\": {\"showTitle\": "
        "{\"deep\": true}}}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"xAxis\": {}}}}%%\n" + kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"xAxis\": "
        "{\"unknown\": 1}}}}%%\n" +
            kBody,
        // Title coexistence and pie bodies stay out of Wave B.
        "---\ndescription: >-\n  d\ntitle: t\n---\n%%{init: {\"xyChart\": {\"width\": "
        "400}}}%%\n" +
            kBody,
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": 400}}}%%\n"
        "pie\n  \"A\" : 1\n  \"B\" : 2\n",
    };
    const std::string init_ok =
        "---\ndescription: >-\n  d\n---\n%%{init: {\"xyChart\": {\"width\": \"400\", "
        "\"plotBorderWidth\": 5, \"xAxis\": {\"tickWidth\": 5, \"axisLineWidth\": 2}, "
        "\"chartOrientation\": "
        "\"horizontal\", \"showDataLabel\": true}}}%%\n" + kBody;
    EXPECT_FALSE(xy_is_fallback(init_ok));
    for (const auto& source : rejected) {
        SCOPED_TRACE(source);
        EXPECT_TRUE(xy_is_fallback(source)) << "source must stay on fallback";
    }
}

namespace {
std::string flip_suppression_to_true(const std::string& source) {
    std::string flipped = source;
    for (const char* key : {"showLabel", "showTitle", "showTick", "showAxisLine"}) {
        const std::string from = std::string(key) + ": false";
        const std::string to = std::string(key) + ": true";
        for (std::size_t at = 0; (at = flipped.find(from, at)) != std::string::npos; at += to.size())
            flipped.replace(at, from.size(), to);
    }
    return flipped;
}

int non_space_cells(const CellCapture& capture) {
    int count = 0;
    for (const auto& cell : capture.cells)
        if (cell.first != " ") ++count;
    return count;
}

bool frame_text_contains(const CellCapture& capture, int width, const std::string& token) {
    for (const auto& line : frame_lines(capture, width))
        if (line.find(token) != std::string::npos) return true;
    return false;
}
}  // namespace

TEST(MermaidXychart0035Cohort, SuppressionRowsHideExactlyTheirElement) {
    auto ledger = parse_ledger();
    int checked = 0;
    for (const auto& row : read_rows(ledger.root())) {
        if (row.phase != "P2-show-false") continue;
        ++checked;
        SCOPED_TRACE(row.path);
        const auto source = read_file(kCases / row.path);
        const auto flipped = flip_suppression_to_true(source);
        ASSERT_NE(source, flipped) << "row must carry a show*: false flag";
        ASSERT_TRUE(drawn_native(source));
        ASSERT_TRUE(drawn_native(flipped));
        const auto hidden = capture_cells(source, "mermaid");
        const auto shown = capture_cells(flipped, "mermaid");
        EXPECT_NE(hidden.cells, shown.cells) << "suppression must change the frame";
        EXPECT_EQ(frame_lines(hidden, kWide).size(), frame_lines(shown, kWide).size())
            << "suppression must not reflow the frame";
        // Tick marks and axis lines replace glyphs in place, so a cell-count
        // drop is only expected where label/title text disappears.
        if (row.path.find("show-axis-label-false") != std::string::npos ||
            row.path.find("show-axis-title-false") != std::string::npos ||
            row.path.find("spark-bar") != std::string::npos)
            EXPECT_LT(non_space_cells(hidden), non_space_cells(shown))
                << "suppression must remove content";
        if (row.path.find("show-axis-label-false") != std::string::npos) {
            EXPECT_TRUE(frame_text_contains(shown, kWide, "jan"));
            EXPECT_FALSE(frame_text_contains(hidden, kWide, "jan"));
        }
        if (row.path.find("show-axis-title-false") != std::string::npos) {
            EXPECT_TRUE(frame_text_contains(shown, kWide, "Months"));
            EXPECT_FALSE(frame_text_contains(hidden, kWide, "Months"));
            EXPECT_FALSE(frame_text_contains(hidden, kWide, "(in $)"));
        }
    }
    EXPECT_EQ(checked, 5);
}

namespace {
bool is_axis_cell(const std::string& cell) {
    if (cell == " ") return true;
    static const std::set<std::string> glyphs{"─", "│", "┌", "┐", "└", "┘", "├", "┤", "┬", "┴",
                                             "┼", "━", "┃", "┏", "┓", "┗", "┛", "┣", "┫", "┳",
                                             "┻", "╋"};
    return glyphs.contains(cell);
}

// Tick/line suppression may only reshape axis cells; every other differing
// cell means content was removed or moved.
void expect_only_axis_cells_differ(const CellCapture& off, const CellCapture& on,
                                   const char* label) {
    ASSERT_EQ(off.cells.size(), on.cells.size()) << label;
    int other = 0;
    for (std::size_t i = 0; i < off.cells.size(); ++i) {
        if (off.cells[i] == on.cells[i]) continue;
        if (!is_axis_cell(off.cells[i].first) || !is_axis_cell(on.cells[i].first)) ++other;
    }
    EXPECT_EQ(other, 0) << label << ": non-axis cells changed";
}
}  // namespace

TEST(MermaidXychart0035Cohort, SuppressionIsPerAxisAndKeepsOtherElements) {
    auto synthetic = [&](const std::string& key, const std::string& value) {
        return "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    xAxis:\n      " + key +
               ": " + value + "\n---\nxychart\n  x-axis \"CatTitle\" [catA, catB]\n  y-axis "
               "\"ValTitle\" 0 --> 10\n  bar [3, 6]\n";
    };
    // Hiding x labels removes the category text but keeps the axis titles.
    const auto labels_off = capture_cells(synthetic("showLabel", "false"), "mermaid");
    const auto labels_on = capture_cells(synthetic("showLabel", "true"), "mermaid");
    EXPECT_FALSE(frame_text_contains(labels_off, kWide, "catA"));
    EXPECT_TRUE(frame_text_contains(labels_on, kWide, "catA"));
    EXPECT_TRUE(frame_text_contains(labels_off, kWide, "CatTitle"));
    EXPECT_TRUE(frame_text_contains(labels_off, kWide, "ValTitle"));
    // Hiding the x title keeps the category labels.
    const auto title_off = capture_cells(synthetic("showTitle", "false"), "mermaid");
    EXPECT_FALSE(frame_text_contains(title_off, kWide, "CatTitle"));
    EXPECT_TRUE(frame_text_contains(title_off, kWide, "ValTitle"));
    EXPECT_TRUE(frame_text_contains(title_off, kWide, "catA"));
    // Hiding ticks and the axis line only reshapes axis cells: labels, titles
    // and every data cell stay untouched (no reflow, no wrong-element removal).
    for (const char* key : {"showTick", "showAxisLine"}) {
        const auto off = capture_cells(synthetic(key, "false"), "mermaid");
        const auto on = capture_cells(synthetic(key, "true"), "mermaid");
        EXPECT_NE(off.cells, on.cells) << key;
        EXPECT_EQ(frame_lines(off, kWide).size(), frame_lines(on, kWide).size()) << key;
        EXPECT_TRUE(frame_text_contains(off, kWide, "catA")) << key;
        EXPECT_TRUE(frame_text_contains(off, kWide, "CatTitle")) << key;
        expect_only_axis_cells_differ(off, on, key);
    }
    // Horizontal orientation: x stays the category axis and y the value axis.
    auto horizontal = [&](const std::string& key, const std::string& value) {
        return "---\ndescription: >-\n  d\nconfig:\n  xyChart:\n    chartOrientation: "
               "horizontal\n    xAxis:\n      " + key + ": " + value +
               "\n---\nxychart\n  x-axis \"CatTitle\" [catA, catB]\n  y-axis "
               "\"ValTitle\" 0 --> 10\n  bar [3, 6]\n";
    };
    const auto h_labels_off = capture_cells(horizontal("showLabel", "false"), "mermaid");
    EXPECT_FALSE(frame_text_contains(h_labels_off, kWide, "catA"));
    EXPECT_TRUE(frame_text_contains(h_labels_off, kWide, "CatTitle"));
    EXPECT_TRUE(frame_text_contains(h_labels_off, kWide, "ValTitle"));
    EXPECT_LT(non_space_cells(h_labels_off),
              non_space_cells(capture_cells(horizontal("showLabel", "true"), "mermaid")))
        << "horizontal label suppression must remove content";
    for (const char* key : {"showTick", "showAxisLine"}) {
        const auto off = capture_cells(horizontal(key, "false"), "mermaid");
        const auto on = capture_cells(horizontal(key, "true"), "mermaid");
        EXPECT_NE(off.cells, on.cells) << key;
        EXPECT_EQ(frame_lines(off, kWide).size(), frame_lines(on, kWide).size()) << key;
        EXPECT_TRUE(frame_text_contains(off, kWide, "catA")) << key;
        expect_only_axis_cells_differ(off, on, key);
    }
    // A false value outside the four flags stays closed.
    EXPECT_TRUE(xy_is_fallback(synthetic("showLegend", "false")));
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
    EXPECT_FALSE(xy_is_fallback("---\nconfig:\n  xyChart:\n    xAxis:\n      showLabel: false\n---\n" +
                                kBody));
    // P3: dataLabelColor joins the validated-and-ignored color roles.
    EXPECT_FALSE(xy_is_fallback(
        with_config("  themeVariables:\n    xyChart:\n      dataLabelColor: \"#eeeeee\"\n",
                    kBody)));
    EXPECT_TRUE(xy_is_fallback(
        with_config("  themeVariables:\n    xyChart:\n      unknownColor: \"#eeeeee\"\n",
                    kBody)));
    // The role uses the same color validation as the existing ten, and a
    // misnested themeVariables block stays closed.
    EXPECT_TRUE(xy_is_fallback(
        with_config("  themeVariables:\n    xyChart:\n      dataLabelColor: red\n", kBody)));
    EXPECT_TRUE(xy_is_fallback(
        with_config("  themeVariables:\n    xyChart:\n      dataLabelColor: \"#zzzzzz\"\n",
                    kBody)));
    EXPECT_TRUE(xy_is_fallback(
        with_config("  xyChart:\n    themeVariables:\n      xyChart:\n        dataLabelColor: "
                    "\"#eeeeee\"\n",
                    kBody)));
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
