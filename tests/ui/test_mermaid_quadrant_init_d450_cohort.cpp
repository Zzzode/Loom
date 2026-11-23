// D-450 quadrant leading-init admission gate.
//
// Selection rationale: the five quadrant-chart fallbacks whose only blocker is
// a leading body `%%{init: {...}}%%` directive carrying the closed
// `quadrantChart` sizing/axis-position knobs or the quadrant `themeVariables`
// colours. The projection draws no axes, points or colours, so every admitted
// key is inert and disclosed in a footer in source order.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/quadrant-init-d450-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP450_SENTINEL";
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
bool draws(const std::string& source, int width) {
    return !same_surface(capture(source, "mermaid", width), capture(source, "text", width));
}
JsonDoc parse_doc(const fs::path& path) {
    auto parsed = json::parse(read_file(path));
    if (!parsed) throw std::runtime_error("cannot parse " + path.string());
    return std::move(*parsed);
}
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
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:" || line == "---") return false;
        // Disclosure footer lines also begin with the directive marker; a
        // directive echo is any such line that is not a disclosure.
        if (line.starts_with("%%{init") && line.find("not applied") == std::string::npos)
            return false;
    }
    return got.back() == "P450_SENTINEL";
}
// The same fixture with the admitted leading directive line removed.
std::string init_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string head = source.substr(0, close + 5);
    std::string body = source.substr(close + 5);
    const auto first_end = body.find('\n');
    const std::string first = body.substr(0, first_end);
    if (first.starts_with("%%{init"))
        body = first_end == std::string::npos ? std::string{} : body.substr(first_end + 1);
    return head + body;
}
constexpr std::string_view kBody =
    "quadrantChart\n"
    "  title Probe\n"
    "  x-axis Low Reach --> High Reach\n"
    "  y-axis Low Engagement --> High Engagement\n"
    "  quadrant-1 A\n"
    "  quadrant-2 B\n"
    "  quadrant-3 C\n"
    "  quadrant-4 D\n";
std::string probe(std::string_view directive) {
    return "---\ndescription: >-\n  quadrant init probe\n---\n" + std::string(directive) +
           std::string(kBody);
}
}  // namespace

TEST(MermaidQuadrantInitD450Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_quadrant_init_d450_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-450");
    EXPECT_EQ(root.get("counts").get_int("total"), 5);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("quadrant-chart"), 5);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "997dd7d0a56a06d5824d4979ff2a42b3bc37d63f32bc06d4cee739da88dade62");
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
    EXPECT_EQ(digest_input.size(), 5u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidQuadrantInitD450Cohort, InitIsInertAgainstInitFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = init_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        // Rule 34: the 40-column state is measured live, never inherited.
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        std::vector<std::string> art(lines.begin(),
                                     lines.begin() + static_cast<std::ptrdiff_t>(begin));
        while (!art.empty() && art.back().empty()) art.pop_back();
        auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        while (!twin_lines.empty() &&
               (twin_lines.back().empty() || twin_lines.back() == "P450_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the directive changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 5);
}

TEST(MermaidQuadrantInitD450Cohort, ClosedDirectiveNegatives) {
    // Admitted positives: a bare no-op directive, an empty object, and the
    // closed tuning/theme keys in source order.
    {
        const auto bare = probe("%%{init}%%\n");
        ASSERT_TRUE(draws(bare, kWide));
        EXPECT_EQ(capture(bare, "mermaid", kWide).text.find("Quadrant native projection v1"),
                  std::string::npos);
    }
    {
        const auto empty = probe("%%{init: {}}%%\n");
        ASSERT_TRUE(draws(empty, kWide));
        EXPECT_EQ(capture(empty, "mermaid", kWide).text.find("Quadrant native projection v1"),
                  std::string::npos);
    }
    {
        const auto tuned = probe(
            "%%{init: {\"quadrantChart\": {\"yAxisPosition\": \"right\", "
            "\"pointRadius\": 10}, \"themeVariables\": "
            "{\"quadrant1Fill\": \"#b4dcff\"}}}%%\n");
        ASSERT_TRUE(draws(tuned, kWide));
        const auto frame = capture(tuned, "mermaid", kWide).text;
        EXPECT_NE(frame.find("Quadrant native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("quadrantChart.yAxisPosition=right"), std::string::npos);
        EXPECT_NE(frame.find("themeVariables.quadrant1Fill=#b4dcff"), std::string::npos);
        EXPECT_LT(frame.find("quadrantChart.yAxisPosition=right"),
                  frame.find("quadrantChart.pointRadius=10"));
        EXPECT_LT(frame.find("quadrantChart.pointRadius=10"),
                  frame.find("themeVariables.quadrant1Fill"));
    }
    for (const auto* directive : {
             // Closed tables and the exact leading-directive shape.
             "%%{init: {\"quadrantChart\": {\"xAxisPosition\": \"left\"}}}%%\n",
             "%%{init: {\"quadrantChart\": {\"chartWidth\": -1}}}%%\n",
             "%%{init: {\"quadrantChart\": {\"chartWidth\": 1.5}}}%%\n",
             "%%{init: {\"quadrantChart\": {\"unknownKey\": 1}}}%%\n",
             "%%{init: {\"quadrantChart\": {\"pointRadius\": 1, \"pointRadius\": 2}}}%%\n",
             "%%{init: {\"themeVariables\": {\"quadrant1Fill\": \"#b4dcf\"}}}%%\n",
             "%%{init: {\"themeVariables\": {\"quadrant1Fill\": \"#zzzzzz\"}}}%%\n",
             "%%{init: {\"themeVariables\": {\"unknownFill\": \"#b4dcff\"}}}%%\n",
             "%%{init: {\"unknownSection\": {\"pointRadius\": 1}}}%%\n",
             "%%{init: {\"quadrantChart\": 1}}%%\n",
             "%%{init: {\"quadrantChart\": {\"pointRadius\": {\"inner\": 1}}}}%%\n",
             "%%{init: {\"quadrantChart\": {\"pointRadius\": 1}}%%\n",
             "%%{init: {\"quadrantChart\": {}}}%%\n"
             "%%{init: {\"quadrantChart\": {}}}%%\n",
         }) {
        const auto source = probe(directive);
        EXPECT_FALSE(draws(source, kWide)) << directive;
        EXPECT_FALSE(native_frame(source, kWide)) << directive;
    }
    // A non-leading directive keeps the whole source.
    {
        const auto source = std::string{"---\ndescription: >-\n  probe\n---\nquadrantChart\n"} +
                            "%%{init: {\"quadrantChart\": {\"pointRadius\": 1}}}%%\n" +
                            "  quadrant-1 A\n";
        EXPECT_FALSE(draws(source, kWide));
    }
    // A directive-free source keeps no disclosure footer.
    {
        const auto frame = capture(probe(""), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Quadrant native projection v1"), std::string::npos);
    }
}
