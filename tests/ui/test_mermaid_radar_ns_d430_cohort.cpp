// D-430 radar closed-frontmatter namespace gate.
//
// Selection rationale (TL directive, D-428 census): radar has 3 config-only
// fallbacks, all closed frontmatter shapes (theme base + themeVariables, or
// config.radar layout). Expected drop: 3 rows (radar 3 -> 0, global 545 -> 542,
// formal 1414 -> 1417).
//
// Criterion (semantic, D-412): the native projection transcribes an outline
// and draws no polygon, colour or font size, so every admitted leaf is inert
// and disclosed under `Radar native projection v1`. The oracle is exact: the
// art above the footer equals the config-stripped twin's art.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <array>
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/radar-ns-d430-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "Radar native projection v1";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP430_SENTINEL";
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
std::array<std::string, 3> load_states(const JsonVal& states) {
    return {states.get_string("wide_240"), states.get_string("narrow_40"),
            states.get_string("restored_240")};
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
        if (line == "description:" || line == "config:") return false;
        if (line == "---") return false;
    }
    return got.back() == "P430_SENTINEL";
}
// The art above the disclosure footer; a footer-free frame keeps everything.
std::vector<std::string> canvas_lines(const std::string& source) {
    auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    std::size_t header = lines.size();
    for (std::size_t i = lines.size(); i-- > 0;)
        if (lines[i] == kHeader) {
            header = i;
            break;
        }
    std::vector<std::string> out(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(header));
    while (!out.empty() && (out.back().empty() || out.back() == "P430_SENTINEL")) out.pop_back();
    return out;
}
std::string config_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
    const std::string body = source.substr(close + 5);
    std::string kept;
    bool skipping = false;
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        if (skipping && line.size() >= 2 && line[0] == ' ' && line[1] == ' ') continue;
        skipping = false;
        if (line.starts_with("config:")) {
            skipping = true;
            continue;
        }
        kept += line;
        kept += "\n";
    }
    return "---\n" + kept + "---\n" + body;
}
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    std::vector<std::string> expected{""};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "P430_SENTINEL") << label;
    ASSERT_TRUE(lines[lines.size() - 2u].empty()) << label << " (trailing blank row)";
    const auto begin = lines.size() - 2u - expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(lines[begin + i], expected[i]) << label << " footer line " << i;
}
std::vector<std::string> string_list(const JsonVal& value) {
    std::vector<std::string> out;
    value.iter([&](JsonVal item) { out.emplace_back(item.as_str()); });
    return out;
}
constexpr std::string_view kBody =
    "radar-beta\n  axis A,B,C\n  curve mycurve{1,2,3}\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body = kBody) {
    return "---\ndescription: >-\n  radar namespace probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidRadarNamespaceD430Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_radar_ns_d430_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-430");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "2f0919506f544b339a0d2ebf70a367b59e1ad3fc33fc9d0d5b36ab2b4c23abb0");
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
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        EXPECT_GE(string_list(row.get("expected_footer_lines")).size(), 2u) << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 3u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidRadarNamespaceD430Cohort, LeavesAreInertAgainstConfigFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = config_free_twin(source);
        const auto states = load_states(row.get("expected_states"));
        const auto footer = string_list(row.get("expected_footer_lines"));
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger";
        expect_footer(source, footer, rel);
        EXPECT_EQ(canvas_lines(source), canvas_lines(twin)) << rel << " leaf changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 3);
}

TEST(MermaidRadarNamespaceD430Cohort, ClosedFrontmatterNegativesStayWholeSource) {
    // Positives: each closed leaf alone draws and discloses the normalized value.
    for (const auto& [tail, line] :
         {std::pair{"config:\n  theme: base\n", "config.theme=base -> colour palette; not applied"},
          std::pair{"config:\n  themeVariables:\n    fontSize: 80\n",
                    "config.themeVariables.fontSize=80 -> label font size; not applied"},
          std::pair{"config:\n  themeVariables:\n    cScale0: '#FF0000'\n",
                    "config.themeVariables.cScale0=#FF0000 -> series palette colour; not applied"},
          std::pair{"config:\n  themeVariables:\n    radar:\n      axisColor: '#00FF00'\n",
                    "config.themeVariables.radar.axisColor=#00FF00 -> axis colour; not applied"},
          std::pair{"config:\n  radar:\n    marginTop: 100\n",
                    "config.radar.marginTop=100 -> layout margin; not applied"},
          std::pair{"config:\n  radar:\n    axisScaleFactor: 0.5\n",
                    "config.radar.axisScaleFactor=0.5 -> axis scale factor; not applied"}}) {
        const auto source = probe(tail);
        ASSERT_TRUE(draws(source, kWide)) << line;
        ASSERT_TRUE(native_frame(source, kWide)) << line;
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(line), std::string::npos) << line;
    }
    // Negatives: look, out-of-table themes, unknown keys, bad values, wrong
    // indents and body styles all stay whole-source.
    for (const auto* tail : {
             "config:\n  look: neo\n",
             "config:\n  theme: dark\n",
             "config:\n  themeVariables:\n    color: red\n",
             "config:\n  themeVariables:\n    cScale0: red\n",
             "config:\n  themeVariables:\n    fontSize: big\n",
             "config:\n  radar:\n    marginTop: -1\n",
             "config:\n  radar:\n    axisScaleFactor: 11\n",
             "config:\n  radar:\n    marginTop: 100\n    marginTop: 200\n",
             "config:\n  radar:\n      marginTop: 100\n",
             "config:\n  themeVariables:\n    radar:\n      marginTop: 100\n",
             "config:\n  radar:\n    axisColor: '#FF0000'\n",
         }) {
        const auto source = probe(tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    for (const auto* body : {"radar-beta\n  axis A\n  curve c{1}:::red\n",
                             "radar-beta\n  classDef red fill:#fff;\n",
                             "radar-beta\n  %%{init: {}}%%\n"}) {
        EXPECT_FALSE(draws(std::string(body), kWide)) << body;
        EXPECT_FALSE(draws(probe("config:\n  theme: base\n", body), kWide))
            << body << " (namespace does not bypass the body style gate)";
    }
}
