// D-451 c4 closed styling and accessibility admission gate.
//
// Selection rationale: the six c4 fallbacks whose only blockers are the closed
// `UpdateElementStyle`/`UpdateRelStyle` calls and scalar `accTitle:`/`accDescr:`
// no-ops. The native c4 projection is an outline with no colour or geometry, so
// every admitted attribute is inert and disclosed line by line; the
// accessibility scalars are validated no-ops like the state/usecase families.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/c4-styling-d451-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP451_SENTINEL";
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
        // Disclosure footer lines also begin with the call prefix; a source
        // echo is any such line that is not a disclosure.
        if ((line.starts_with("UpdateElementStyle(") || line.starts_with("UpdateRelStyle(")) &&
            line.find("not applied") == std::string::npos)
            return false;
        if (line.starts_with("accTitle") || line.starts_with("accDescr")) return false;
    }
    return got.back() == "P451_SENTINEL";
}
// The same fixture with the admitted styling and accessibility lines removed.
std::string styling_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string head = source.substr(0, close + 5);
    std::string out;
    const std::string body = source.substr(close + 5);
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        const auto end = eol == std::string::npos ? body.size() : eol;
        const std::string line = body.substr(pos, end - pos);
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) first = 0;
        const std::string stripped = line.substr(first);
        const bool drop = stripped.starts_with("UpdateElementStyle") ||
                          stripped.starts_with("UpdateRelStyle") ||
                          stripped.starts_with("accTitle") ||
                          stripped.starts_with("accDescr");
        if (!drop) {
            out += line;
            if (eol != std::string::npos) out += '\n';
        }
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return head + out;
}
constexpr std::string_view kBody =
    "C4Context\n"
    "title Probe\n"
    "System(a, \"A\")\n"
    "System(b, \"B\")\n"
    "Rel(a, b, \"Uses\")\n";
std::string probe(std::string_view extra) {
    return "---\ndescription: >-\n  c4 styling probe\n---\n" + std::string(kBody) +
           std::string(extra);
}
}  // namespace

TEST(MermaidC4StylingD451Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_c4_styling_d451_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-451");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("c4"), 6);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "abc1c6ba8d85e9874dc9a9fbf20caf43526ba6e84d7dfb1e0cdd1f2c8f2d150f");
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
    EXPECT_EQ(digest_input.size(), 6u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidC4StylingD451Cohort, StylingIsInertAgainstStylingFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = styling_free_twin(source);
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
               (twin_lines.back().empty() || twin_lines.back() == "P451_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the statements changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 6);
}

TEST(MermaidC4StylingD451Cohort, ClosedStatementNegatives) {
    // Admitted positives: one element-style call, one relationship-style call,
    // and the accessibility scalars (which keep no footer).
    {
        const auto source = probe("UpdateElementStyle(a, $fontColor=\"red\")\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("C4 native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("UpdateElementStyle(a).$fontColor -> SVG styling; not applied"),
                  std::string::npos);
    }
    {
        const auto source = probe("UpdateRelStyle(a, b, $offsetX=\"-10\")\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "UpdateRelStyle(a, b).$offsetX -> SVG styling; not applied"),
                  std::string::npos);
    }
    {
        const auto source =
            probe("accTitle: Probe\naccDescr: A probe description.\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_EQ(capture(source, "mermaid", kWide).text.find("C4 native projection v1"),
                  std::string::npos);
    }
    for (const auto* extra : {
             // Identifier counts, the closed attribute shape and duplicates.
             "UpdateElementStyle(a)\n",
             "UpdateElementStyle(a, fontColor=\"red\")\n",
             "UpdateElementStyle(a, $fontColor)\n",
             "UpdateElementStyle(a, $fontColor=\"red\", $fontColor=\"blue\")\n",
             "UpdateElementStyle(a, $fontColor=)\n",
             "UpdateElementStyle(a, $fontColor=\"a(b)\")\n",
             "UpdateElementStyle(a, $fontColor=\"\")\n",
             "UpdateRelStyle(a, $offsetX=\"1\")\n",
             "UpdateRelStyle(a, b)\n",
             "UpdateElementStyle(a, $fontColor=\"red\") {\n}\n",
             "UpdateUnknownStyle(a, $fontColor=\"red\")\n",
             // Accessibility duplicates and empty payloads.
             "accTitle: Probe\naccTitle: Probe\n",
             "accDescr:\n",
             "accDescr {\n text\n}\n",
         }) {
        const auto source = probe(extra);
        EXPECT_FALSE(draws(source, kWide)) << extra;
        EXPECT_FALSE(native_frame(source, kWide)) << extra;
    }
    // A statement-free source keeps no disclosure footer.
    {
        const auto frame = capture(probe(""), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("C4 native projection v1"), std::string::npos);
    }
}
