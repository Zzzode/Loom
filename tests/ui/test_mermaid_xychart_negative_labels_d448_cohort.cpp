// D-448 xychart negative-bar data-label gate.
//
// Selection rationale: the vertical negative/decimal bar fixture whose only blocker
// was the compact data-label profile refusing bars on the negative side of the
// zero reference. The profile now labels the authored tip on either side and
// treats a one-cell bar as an external label; each value appears exactly once.
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/xychart-negative-labels-d448-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP448_SENTINEL";
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

bool same_surface(const CellCapture& a, const CellCapture& b) { return a.chars == b.chars; }
std::vector<std::string> trimmed_lines(const CellCapture& capture, int width) {
    if (capture.chars.size() !=
        static_cast<std::size_t>(width) * static_cast<std::size_t>(kHeight))
        return {};
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(kHeight));
    for (int y = 0; y < kHeight; ++y) {
        std::string line;
        line.reserve(static_cast<std::size_t>(width));
        for (int x = 0; x < width; ++x)
            line += capture.chars[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)];
        while (!line.empty() && line.back() == ' ') line.pop_back();
        lines.push_back(std::move(line));
    }
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
int count_of(const std::string& haystack, const std::string& needle) {
    int found = 0;
    for (std::size_t at = haystack.find(needle); at != std::string::npos;
         at = haystack.find(needle, at + needle.size()))
        ++found;
    return found;
}
constexpr std::string_view kProbeBody =
    "  xychart\n  title \"Probe\"\n  x-axis Categories [A, B, C]\n"
    "  y-axis -10 --> 10\n  bar [ -2.5, 0.75, 5.1 ]\n";
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  probe\n" + std::string(config) + "---\n" +
           std::string(kProbeBody);
}
}  // namespace

TEST(MermaidXychartNegativeLabelsD448Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_xychart_negative_labels_d448_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-448");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "365eb9ab89d2e60b41fac55e5327ba17103abdcae76e134d88629657ac94a11a");
    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 1u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidXychartNegativeLabelsD448Cohort, EveryValueLabelsExactlyOnce) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        ASSERT_TRUE(draws(source, kWide)) << rel;
        // Rule 34: the 40-column state is measured live, never inherited.
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("█"), std::string::npos) << rel << " bars must be painted";
        row.get("value_tokens").iter([&](JsonVal token) {
            const auto text = std::string{token.as_str()};
            EXPECT_EQ(count_of(frame, text), 1) << rel << " token " << text;
        });
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidXychartNegativeLabelsD448Cohort, ClosedProfileNegatives) {
    // The admitted profile draws and labels.
    {
        const auto source = probe("config:\n  xyChart:\n    showDataLabel: true\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_EQ(count_of(frame, "-2.5"), 1);
        EXPECT_EQ(count_of(frame, "0.75"), 1);
    }
    // Outside-bar labeling and a line series keep their own profiles.
    EXPECT_TRUE(draws(probe("config:\n  xyChart:\n    showDataLabel: true\n"
                            "    showDataLabelOutsideBar: true\n"),
                      kWide));
    // A horizontal one-cell bar keeps its pre-existing label profile: the
    // value sits just past the plot's right edge, not redirected to the bar
    // tip (the tiny-span redirect is vertical-only).
    {
        const auto source =
            std::string{"---\ndescription: >-\n  probe\nconfig:\n  xyChart:\n"
                        "    showDataLabel: true\n---\n  xychart horizontal\n"
                        "  x-axis Categories [A, B, C]\n  y-axis 0 --> 40\n"
                        "  bar [ 1, 40, 1 ]\n"};
        ASSERT_TRUE(draws(source, kWide));
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::size_t> outside_columns;
        for (const auto& line : lines) {
            const auto bar = line.rfind("█");
            if (bar == std::string::npos) continue;
            if (count_of(line, "█") != 1) continue;  // a one-cell bar row
            const auto digit = line.find_first_of("0123456789", bar + 3);
            ASSERT_NE(digit, std::string::npos) << line;
            for (std::size_t at = bar + 3; at < digit; ++at) ASSERT_EQ(line[at], ' ') << line;
            outside_columns.push_back(digit);
        }
        ASSERT_EQ(outside_columns.size(), 2u);
        EXPECT_EQ(outside_columns[0], outside_columns[1]);
    }
    // The horizontal signed-value label profile is still pending review: the
    // E6c merman_xy102 observation pins that row as source_fallback
    // (merman/f5f012ca4e7055701b83), so it stays whole-source.
    EXPECT_FALSE(draws(read_file(kCases /
                                 "xychart/pr-check/"
                                 "should-render-negative-and-decimal-values-with-correct-"
                                 "labels-for-horizontal-xy-chart.mmd"),
                       kWide));
    // A zero-valued bar series stays whole-source: its tip coincides with the
    // reference and the compact profile has no reviewed placement for it.
    const auto zero_body =
        std::string{"---\ndescription: >-\n  probe\nconfig:\n  xyChart:\n"
                    "    showDataLabel: true\n---\n  xychart\n  x-axis [A, B, C]\n"
                    "  y-axis -10 --> 10\n  bar [ 0, 1, 5 ]\n"};
    EXPECT_FALSE(draws(zero_body, kWide));
    // A value outside the authored domain stays whole-source.
    const auto outside_body =
        std::string{"---\ndescription: >-\n  probe\nconfig:\n  xyChart:\n"
                    "    showDataLabel: true\n---\n  xychart\n  x-axis [A, B, C]\n"
                    "  y-axis 0 --> 4\n  bar [ -2.5, 0.75, 5.1 ]\n"};
    EXPECT_FALSE(draws(outside_body, kWide));
}
