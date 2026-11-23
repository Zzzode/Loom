// D-457 quadrant/tree-view body-style gate.
//
// Selection rationale: the last body-style fallbacks of two families whose
// projections plot labels only. A quadrant `classDef` / `:::class` / inline
// point-style suffix and a tree-view `:::class` node annotation are validated
// against closed grammars, dropped from the outline and disclosed; the
// style-stripped twin must render identical art.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/style-d457-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP457_SENTINEL";
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
    }
    return got.back() == "P457_SENTINEL";
}
// The same fixture with the styling surface removed: a quadrant `classDef`
// line is deleted, the `:::class` suffix is cut from a point name, and the
// inline style text after the coordinate bracket is dropped; a tree-view
// `:::class` annotation is cut from the node row.
std::string quadrant_style_stripped(std::string_view source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        std::string line{source.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos)};
        pos = eol == std::string_view::npos ? source.size() + 1 : eol + 1;
        const auto first = line.find_first_not_of(' ');
        if (first != std::string::npos && line.substr(first).starts_with("classDef "))
            continue;  // dropped statement
        const auto open = line.find('[');
        const auto close = line.find(']');
        if (open != std::string::npos && close != std::string::npos && open < close) {
            line.erase(close + 1);  // drop the inline style suffix
            const auto mark = line.rfind(":::", open);
            if (mark != std::string::npos && mark < open) {
                const auto sep = line.find(':', mark + 3);
                if (sep != std::string::npos && sep < open)
                    line.erase(mark, sep - mark);  // drop only `:::class`
            }
        }
        out += line;
        if (eol == std::string_view::npos) break;
        out += '\n';
    }
    return out;
}
std::string treeview_class_stripped(std::string_view source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        std::string line{source.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos)};
        pos = eol == std::string_view::npos ? source.size() + 1 : eol + 1;
        const auto mark = line.rfind(" :::");
        if (mark != std::string::npos) line.erase(mark);
        out += line;
        if (eol == std::string_view::npos) break;
        out += '\n';
    }
    return out;
}
}  // namespace

TEST(MermaidStyleD457Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_style_d457_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-457");
    EXPECT_EQ(root.get("counts").get_int("total"), 3);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("quadrant-chart"), 2);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("tree-view"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
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
    EXPECT_EQ(digest_input.size(), 3u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidStyleD457Cohort, StylingIsInertAgainstStrippedTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto family = row.get_string("family");
        const auto twin = family == "quadrant-chart" ? quadrant_style_stripped(source)
                                                     : treeview_class_stripped(source);
        EXPECT_NE(twin, source) << rel << " (the twin must strip styling)";
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
               (twin_lines.back().empty() || twin_lines.back() == "P457_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the styling changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 3);
}

TEST(MermaidStyleD457Cohort, ClosedNegatives) {
    // Admitted positives: quadrant classDef + `:::class` + inline styles, and
    // a tree-view `:::class` node annotation.
    {
        const auto source =
            "quadrantChart\n  title T\n  x-axis X\n  y-axis Y\n"
            "  quadrant-1 Q1\n  quadrant-2 Q2\n  quadrant-3 Q3\n  quadrant-4 Q4\n"
            "  P one:::c1: [0.1, 0.2] radius: 20, color: #ff0000\n"
            "  P two: [0.3, 0.4]\n"
            "  classDef c1 stroke-width: 3px\n";
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("point P one -> class=c1, styles=2; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body classDef=c1 -> colour/radius; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("P one (0.1, 0.2)"), std::string::npos);
        EXPECT_EQ(frame.find("radius: 20"), std::string::npos);
    }
    {
        // A long free-text point name is truncated in the disclosure rather
        // than turning the row into a fallback.
        std::string long_name;
        for (int i = 0; i < 20; ++i) long_name += (i == 0 ? "" : " ") + std::string("aaaa");
        const auto source = "quadrantChart\n  " + long_name +
                            ": [0.1, 0.2] radius: 20\n";

        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        ASSERT_NE(frame.find("point aaaa"), std::string::npos);
        EXPECT_NE(frame.find("... -> styles=1; not applied"), std::string::npos);
    }
    {
        // A multibyte name cut mid-codepoint is truncated at a codepoint
        // boundary (never mid-sequence).
        std::string multibyte;
        for (int i = 0; i < 40; ++i)
            multibyte += (i == 0 ? "" : " ") + std::string("\xc3\xa9\xc3\xa9");
        const auto source = "quadrantChart\n  " + multibyte +
                            ": [0.1, 0.2] radius: 20\n";
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        ASSERT_NE(frame.find("point "), std::string::npos);
        EXPECT_NE(frame.find("... -> styles=1; not applied"), std::string::npos);
        EXPECT_NE(frame.find("\xc3\xa9"), std::string::npos);
    }
    {
        const auto source = "treeView-beta\n  \"a b\" :::highlight\n";
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("body class annotations -> highlight colour; not applied"),
                  std::string::npos);
        EXPECT_EQ(frame.find(":::highlight"), std::string::npos);
    }
    for (const auto& source : {
             // The closed property list.
             std::string{"quadrantChart\n  P a: [0.1, 0.2] fillcolor: #ff0000\n"},
             std::string{"quadrantChart\n  P a: [0.1, 0.2] color: red\n"},
             std::string{"quadrantChart\n  P a: [0.1, 0.2] radius: 20, radius: 30\n"},
             std::string{"quadrantChart\n  P a: [0.1, 0.2] stroke-width: 3\n"},
             std::string{"quadrantChart\n  P a: [0.1, 0.2] color #ff0000\n"},
             std::string{"quadrantChart\n  classDef\n"},
             std::string{"quadrantChart\n  classDef c1\n"},
             std::string{"quadrantChart\n  classDef 1c color: #ff0000\n"},
             std::string{"quadrantChart\n  classDef c1 fill: #ff0000\n"},
             // A `class` statement stays closed.
             std::string{"quadrantChart\n  P a: [0.1, 0.2]\n  class P a c1\n"},
             // The class annotation stays closed.
             std::string{"quadrantChart\n  P a:::1c: [0.1, 0.2]\n"},
             std::string{"quadrantChart\n  P a:::c1:::c2: [0.1, 0.2]\n"},
             // A point name is one field: an inner ':' is outside the grammar.
             std::string{"quadrantChart\n  A: B: [0.1, 0.2]\n"},
             // An annotated label line keeps the whole source gated.
             std::string{"quadrantChart\n  title T :::x\n"},
             std::string{"quadrantChart\n  quadrant-1 Q :::x\n"},
             // Tree-view annotation shape: needs the separating space and a
             // single identifier.
             std::string{"treeView-beta\n  a:::highlight\n"},
             std::string{"treeView-beta\n  \"a\" :::\n"},
             std::string{"treeView-beta\n  a :::c1 :::c2\n"},
             std::string{"treeView-beta\n  a :::1c\n"},
             // The annotation must attach to the label, not follow an
             // icon(...) or `## description` suffix.
             std::string{"treeView-beta\n  a icon(bell) :::highlight\n"},
             std::string{"treeView-beta\n  a ## note :::highlight\n"},
             // classDef/style statements stay closed in tree-view.
             std::string{"treeView-beta\n  a\n  classDef c1 fill:#f00\n"},
             std::string{"treeView-beta\n  a\n  style a color:#f00\n"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
