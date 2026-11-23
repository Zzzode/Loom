// D-445 c4 UpdateLayoutConfig admission gate.
//
// Selection rationale: the two remaining c4 originals carry only the in-body
// `UpdateLayoutConfig($c4ShapeInRow="2"|"4", $c4BoundaryInRow="1")` call. The
// native c4 projection is an outline and draws no geometry, so the SVG
// row-shaping knobs are inert; the call's only effect is its disclosure line,
// and the outline equals the call-free twin's.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/c4-layout-config-d445-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP445_SENTINEL";
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
        if (line == "description:" || line == "title:" || line == "---") return false;
        if (line.starts_with("UpdateLayoutConfig(")) return false;
    }
    return got.back() == "P445_SENTINEL";
}
// The same fixture with the admitted call line removed.
std::string call_free_twin(const std::string& source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        const auto end = eol == std::string::npos ? source.size() : eol;
        const std::string line = source.substr(pos, end - pos);
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) first = 0;
        if (!line.substr(first).starts_with("UpdateLayoutConfig")) {
            out += line;
            if (eol != std::string::npos) out += '\n';
        }
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return out;
}
constexpr std::string_view kBody =
    "C4Context\n"
    "title UpdateLayoutConfig ($c4ShapeInRow=2)\n"
    "System(a, \"A\")\n"
    "System(b, \"B\")\n"
    "System(c, \"C\")\n"
    "System(d, \"D\")\n";
std::string probe(std::string_view call) {
    return "---\ndescription: >-\n  c4 layout probe\n---\n" + std::string(kBody) +
           std::string(call);
}
}  // namespace

TEST(MermaidC4LayoutConfigD445Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_c4_layout_config_d445_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-445");
    EXPECT_EQ(root.get("counts").get_int("total"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("c4"), 2);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "6d1a92eec4de61b04fb2b77fc58abf0c60dac746935effa48ba8334cd51f39c6");
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
        EXPECT_EQ(row.get_string("manifest_sha256"), it->second.get_string("sha256")) << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 2u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidC4LayoutConfigD445Cohort, CallIsInertAgainstCallFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = call_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel << " narrow stays source";
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
               (twin_lines.back().empty() || twin_lines.back() == "P445_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the call changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 2);
}

TEST(MermaidC4LayoutConfigD445Cohort, ClosedCallsStayWholeSource) {
    // The admitted calls draw and disclose in canonical order; with a wrap leaf
    // the wrap line stays first.
    {
        const auto source = probe("UpdateLayoutConfig($c4ShapeInRow=\"4\", "
                                  "$c4BoundaryInRow=\"1\")\n");
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("C4 native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("UpdateLayoutConfig $c4ShapeInRow=4 $c4BoundaryInRow=1 -> "
                             "SVG layout tuning; not applied"),
                  std::string::npos);
    }
    for (const auto* call : {
             // Closed tables and the exact two-argument shape.
             "UpdateLayoutConfig($c4ShapeInRow=\"3\", $c4BoundaryInRow=\"1\")\n",
             "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"2\")\n",
             "UpdateLayoutConfig($c4ShapeInRow=\"2\")\n",
             "UpdateLayoutConfig($c4BoundaryInRow=\"1\", $c4ShapeInRow=\"2\")\n",
             "UpdateLayoutConfig($c4ShapeInRow=2, $c4BoundaryInRow=1)\n",
             "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\", $x=\"1\")\n",
             "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\")\n"
             "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\")\n",
             "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\") {\n}\n",
         }) {
        const auto source = probe(call);
        EXPECT_FALSE(draws(source, kWide)) << call;
        EXPECT_FALSE(native_frame(source, kWide)) << call;
    }
    // An empty call and the other Update* statements stay whole-source.
    for (const auto* call : {"UpdateLayoutConfig()\n", "UpdateElementStyle(a, $bgColor=\"red\")\n",
                             "UpdateRelStyle(a, b, $textColor=\"red\")\n"}) {
        const auto source = probe(call);
        EXPECT_FALSE(draws(source, kWide)) << call;
    }
    // The call is a top-level statement only: nested inside an element block
    // it is not the admitted form.
    {
        const auto source = std::string{
            "---\ndescription: >-\n  c4 layout probe\n---\nC4Context\n"
            "System_Boundary(b, \"B\") {\n"
            "UpdateLayoutConfig($c4ShapeInRow=\"2\", $c4BoundaryInRow=\"1\")\n"
            "}\n"};
        EXPECT_FALSE(draws(source, kWide));
        EXPECT_FALSE(native_frame(source, kWide));
    }
    // A call-free source keeps no disclosure footer.
    {
        const auto frame = capture("---\ndescription: >-\n  probe\n---\n" + std::string(kBody),
                                   "mermaid", kWide)
                               .text;
        EXPECT_EQ(frame.find("C4 native projection v1"), std::string::npos);
    }
}
