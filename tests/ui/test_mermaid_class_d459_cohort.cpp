// D-459 class-diagram shared style-admission cohort gate.
//
// Selection rationale: every remaining class-diagram fallback. Their only
// non-admitted inputs are a `cssClass "<id-list>" <class>` statement naming a
// stylesheet class the source never declares, one trailing `:::class`
// annotation on a `class` declaration (optionally with the member-body
// opener), and `link` / `callback` interaction bindings. An assignment naming
// an absent classDef is inert and disclosed once in the reviewed legend; the
// binding has no terminal rendering surface (FS-2). The stripped twin must
// render identical art above the legend.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/class-d459-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kLegendKey = "key: hue roles use palette;";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP459_SENTINEL";
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
// The drawing rows above the disclosure legend (or the whole body when the
// source carries no legend). Every legend record contains " -> " (the rendered
// record is `<verbatim> -> <projection>`), so the legend block is the maximal
// run of such rows immediately above the `key:` terminator.
std::vector<std::string> art_above_legend(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    std::size_t end = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i].starts_with(kLegendKey)) {
            end = i;
            break;
        }
    if (end != lines.size())
        while (end > 0 && lines[end - 1].find(" -> ") != std::string::npos) --end;
    std::vector<std::string> art(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(end));
    while (!art.empty() && (art.back().empty() || art.back() == "P459_SENTINEL")) art.pop_back();
    return art;
}
// The same fixture with the inert surface removed: `cssClass` / `link` /
// `callback` lines are deleted and one trailing `:::class` annotation is cut
// from a `class` declaration (the member-body opener stays).
std::string inert_styling_stripped(std::string_view source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        std::string line{source.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos)};
        pos = eol == std::string_view::npos ? source.size() + 1 : eol + 1;
        const auto first = line.find_first_not_of(' ');
        const std::string_view content =
            first == std::string::npos ? std::string_view{} : std::string_view{line}.substr(first);
        if (content.starts_with("cssClass ") || content.starts_with("link ") ||
            content.starts_with("callback ")) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        if (content.starts_with("class ")) {
            const auto mark = line.find(":::");
            if (mark != std::string::npos) {
                const auto name_end = line.find_first_of(" {", mark + 3u);
                line.erase(mark, name_end == std::string::npos ? std::string::npos
                                                               : name_end - mark);
            }
        }
        out += line;
        if (eol == std::string_view::npos) break;
        out += '\n';
    }
    return out;
}
std::vector<std::string> legend_block(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    std::size_t key = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i].starts_with(kLegendKey)) {
            key = i;
            break;
        }
    if (key == lines.size()) return {};
    std::size_t begin = key;
    while (begin > 0 && lines[begin - 1].find(" -> ") != std::string::npos) --begin;
    return std::vector<std::string>(lines.begin() + static_cast<std::ptrdiff_t>(begin),
                                    lines.begin() + static_cast<std::ptrdiff_t>(key + 1u));
}
}  // namespace

TEST(MermaidClassD459Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_class_d459_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-459");
    EXPECT_EQ(root.get("counts").get_int("total"), 25);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("class-diagram"), 25);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> unique_paths;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        unique_paths.insert(rel);
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 25u);
    // A duplicated ledger path would shrink the reached set silently.
    EXPECT_EQ(unique_paths.size(), 25u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidClassD459Cohort, InertStylingIsInvisibleAgainstStrippedTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = inert_styling_stripped(source);
        EXPECT_NE(twin, source) << rel << " (the twin must strip something)";
        // A partial strip would leave the removed surface in the twin and make
        // the art comparison vacuous, so pin the stripped residue directly.
        // Only the body counts: the fixture descriptions quote the syntax.
        const auto residue = [](const std::string& text, std::string_view needle) {
            const auto body = text.find("\n---\n", 4);
            const auto begin = body == std::string::npos ? 0u : body + 5u;
            return text.find(needle, begin) != std::string::npos;
        };
        EXPECT_FALSE(residue(twin, "cssClass ")) << rel;
        EXPECT_FALSE(residue(twin, ":::")) << rel;
        EXPECT_FALSE(residue(twin, "link ")) << rel;
        EXPECT_FALSE(residue(twin, "callback ")) << rel;
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_EQ(capture(source, "mermaid", kWide).text.find("```mermaid"), std::string::npos)
            << rel;
        // Rule 34: the 40-column state is measured live, never inherited.
        const auto states = row.get("expected_states");
        const auto wide = capture(source, "mermaid", kWide);
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        // The restored state is a wide render after the narrow one and must be
        // cell-identical to the first wide render, not merely "also drawn".
        const auto narrow = capture(source, "mermaid", kNarrow);
        static_cast<void>(narrow);
        EXPECT_EQ(states.get_string("restored_240") == "drawn", draws(source, kWide))
            << rel << " restored state";
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        std::vector<std::string> expected;
        row.get("expected_legend_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        EXPECT_EQ(legend_block(capture(source, "mermaid", kWide), kWide), expected)
            << rel << " legend block";
        EXPECT_EQ(art_above_legend(capture(source, "mermaid", kWide), kWide),
                  art_above_legend(capture(twin, "mermaid", kWide), kWide))
            << rel << " the inert surface changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 25);
}

// The blast radius: already-drawn class rows whose classDefs are declared keep
// painting and must never report an inert assignment. All six
// `classdefs-being-applied` variants pass through the changed `:::`/`cssClass`
// seam; `classdef-definitions` and `style-definition` cover the definition and
// direct-style paths. A definition-only source carries no direct styles
// (definitions stay inactive until assigned), so only the assigned rows pin a
// non-empty legend.
TEST(MermaidClassD459Cohort, DrawnRowsKeepTheirPaintedLegend) {
    int checked = 0;
    for (const auto* rel : {
             "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
             "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels.mmd",
             "class-diagram/nightly-check/dagre/dagre-should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
             "class-diagram/nightly-check/dagre/dagre-should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels.mmd",
             "class-diagram/nightly-check/handdrawn/hd-should-render-a-simple-class-diagram-with-classdefs-being-applied.mmd",
             "class-diagram/nightly-check/handdrawn/hd-should-render-a-simple-class-diagram-with-classdefs-being-applied-without-htmllabels.mmd",
             "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-classdef-definitions.mmd",
             "class-diagram/nightly-check/should-render-a-simple-class-diagram-with-style-definition.mmd",
         }) {
        const auto source = read_file(kCases / rel);
        ASSERT_TRUE(draws(source, kWide)) << rel;
        const auto legend = legend_block(capture(source, "mermaid", kWide), kWide);
        if (std::string_view{rel}.find("classdefs-being-applied") != std::string_view::npos) {
            ASSERT_FALSE(legend.empty()) << rel << " (the assignment keeps its legend)";
            EXPECT_NE(capture(source, "mermaid", kWide).text.find("definition:"),
                      std::string::npos)
                << rel;
        }
        EXPECT_EQ(capture(source, "mermaid", kWide).text.find("undefined"), std::string::npos)
            << rel;
        ++checked;
    }
    EXPECT_EQ(checked, 8);
}

TEST(MermaidClassD459Cohort, ClosedNegatives) {
    // Admitted positives: an undefined cssClass target list, an inline `:::`
    // annotation with a member body, and an interaction binding.
    {
        const auto source = std::string{
            "classDiagram\n"
            "  class A:::ghost {\n"
            "  int id\n"
            "  }\n"
            "  class B:::ghost {}\n"
            "  class C:::ghost{}\n"
            "  cssClass \"A, B\" phantom\n"
            "  link A \"https://example.com\" \"tip\"\n"
            "  callback B \"fn\" \"tip\"\n"
            "  A <|-- B\n"};
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("class A:::ghost { -> class:ghost undefined; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("cssClass \"A, B\" phantom -> class:phantom undefined; not applied"),
                  std::string::npos);
        EXPECT_EQ(frame.find("link A"), std::string::npos);
        EXPECT_EQ(frame.find("callback B"), std::string::npos);
    }
    // Every name in the closed built-in-selector set is never "undefined":
    // the implicit `default` class and the theme's own selectors always exist,
    // so an assignment naming one keeps the whole-source fallback.
    for (const auto* name : {"default", "node", "label", "cluster", "edgePath", "edgeLabel",
                             "relation", "relationLabel", "classGroup", "classTitle",
                             "annotation", "members", "methods", "divider", "title", "marker",
                             "root"}) {
        const auto source = "classDiagram\n  class A\n  cssClass \"A\" " + std::string{name} +
                            "\n";
        EXPECT_FALSE(draws(source, kWide)) << name;
    }
    for (const auto& source : {
             // Closed cssClass grammar: unquoted, empty and trailing-comma
             // target lists stay gated.
             std::string{"classDiagram\n  class A\n  cssClass A ghost\n"},
             std::string{"classDiagram\n  class A\n  cssClass \"\" ghost\n"},
             std::string{"classDiagram\n  class A\n  cssClass \"A,\" ghost\n"},
             std::string{"classDiagram\n  class A\n  cssClass \"A\" \n"},
             // Closed `:::class` grammar: an empty, numeric-leading or second
             // annotation gates.
             std::string{"classDiagram\n  class A:::\n"},
             std::string{"classDiagram\n  class A:::1c\n"},
             std::string{"classDiagram\n  class A:::c1:::c2\n"},
             std::string{"classDiagram\n  class A:::c1 junk\n"},
             // A declared classDef differing only by letter case may still match
             // upstream, so it keeps the fallback instead of going inert.
             std::string{"classDiagram\n  class A\n  classDef ExClass2 fill:#f00\n"
                         "  cssClass \"A\" exClass2\n"},
             // A malformed interaction binding is not a shape: the separator
             // between the id and the first quoted argument is required.
             std::string{"classDiagram\n  class A\n  link A \"https://example.com\"\n"},
             std::string{"classDiagram\n  class A\n  link A \"https://example.com\" \"tip\" junk\n"},
             std::string{"classDiagram\n  class A\n  link A\"https://example.com\" \"tip\"\n"},
             // The assignment table itself stays closed: a target list is the
             // only new shape, never an unquoted name list.
             std::string{"classDiagram\n  class A\n  cssClass \"A\" c1 c2\n"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
