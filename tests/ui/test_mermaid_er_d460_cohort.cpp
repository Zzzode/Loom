// D-460 ER body style-admission cohort gate.
//
// Selection rationale: every remaining er-diagram fallback whose only
// non-admitted inputs are the closed ER style surface — `classDef` / `class` /
// `style` statements over the corpus-backed CSS table (hex or named colour,
// `stroke-width`, `font-size`, `font-weight`) and inline `:::class` entity
// annotations. The ER projection paints no class surface, so every statement
// is disclosed once in the footer and dropped; the style-stripped twin must
// render identical art above the footer.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
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
using json::JsonDoc;
using json::JsonVal;

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/er-d460-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "ER native projection v1";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP460_SENTINEL";
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
// The drawing rows above the ER disclosure footer (or the whole body when the
// source carries none).
std::vector<std::string> art_above_footer(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    std::size_t header = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i] == kHeader) {
            header = i;
            break;
        }
    if (header != lines.size() && header > 0 && lines[header - 1].empty()) --header;
    std::vector<std::string> art(lines.begin(),
                                 lines.begin() + static_cast<std::ptrdiff_t>(header));
    while (!art.empty() && (art.back().empty() || art.back() == "P460_SENTINEL")) art.pop_back();
    return art;
}
// The number of rendered style records anywhere in the capture: the proof
// that each accepted statement is disclosed exactly once (a duplicated footer
// would double this, a dropped record would lower it).
std::size_t style_record_count(const CellCapture& capture, int width) {
    std::size_t count = 0;
    for (const auto& line : trimmed_lines(capture, width))
        if (line.find(" -> class surface; not applied") != std::string::npos) ++count;
    return count;
}
// The footer tail: the style records in source order, ending the block.
std::vector<std::string> footer_tail(const CellCapture& capture, int width, std::size_t count) {
    auto lines = trimmed_lines(capture, width);
    while (!lines.empty() && lines.back() == "P460_SENTINEL") lines.pop_back();
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    if (lines.size() < count) return {};
    return std::vector<std::string>(lines.end() - static_cast<std::ptrdiff_t>(count), lines.end());
}
// The same fixture with the closed style surface removed: `classDef` / `class`
// / `style` statement lines are deleted and one or more trailing `:::class`
// suffixes are cut from a declaration.
std::string styling_stripped(std::string_view source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        std::string line{source.substr(
            pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos)};
        pos = eol == std::string_view::npos ? source.size() + 1 : eol + 1;
        const auto first = line.find_first_not_of(" \t");
        const std::string_view content = first == std::string::npos
                                             ? std::string_view{}
                                             : std::string_view{line}.substr(first);
        if (content.starts_with("classDef ") || content.starts_with("style ") ||
            (content.starts_with("class ") && !content.starts_with("classDef "))) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        auto mark = line.find(":::");
        while (mark != std::string::npos) {
            const auto end = line.find_first_of(" \t", mark + 3u);
            line.erase(mark, end == std::string::npos ? std::string::npos : end - mark);
            mark = line.find(":::");
        }
        out += line;
        if (eol == std::string_view::npos) break;
        out += '\n';
    }
    return out;
}
}  // namespace

TEST(MermaidErD460Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_er_d460_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-460");
    EXPECT_EQ(root.get("counts").get_int("total"), 15);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("er-diagram"), 15);
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
    EXPECT_EQ(digest_input.size(), 15u);
    EXPECT_EQ(unique_paths.size(), 15u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidErD460Cohort, StylesAreInertAgainstStrippedTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = styling_stripped(source);
        EXPECT_NE(twin, source) << rel << " (the twin must strip the style surface)";
        const auto residue = [](const std::string& text, std::string_view needle) {
            const auto body = text.find("\n---\n", 4);
            const auto begin = body == std::string::npos ? 0u : body + 5u;
            return text.find(needle, begin) != std::string::npos;
        };
        EXPECT_FALSE(residue(twin, "classDef ")) << rel;
        EXPECT_FALSE(residue(twin, ":::")) << rel;
        EXPECT_FALSE(residue(twin, "style ")) << rel;
        EXPECT_FALSE(residue(twin, "class ")) << rel;
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto wide = capture(source, "mermaid", kWide);
        static_cast<void>(capture(source, "mermaid", kNarrow));
        EXPECT_EQ(draws(source, kWide), states.get_string("restored_240") == "drawn")
            << rel << " restored state";
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        EXPECT_EQ(row.get_string("expected_footer_header"), std::string{kHeader}) << rel;
        std::vector<std::string> expected;
        row.get("expected_style_notices").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        EXPECT_EQ(footer_tail(wide, kWide, expected.size()), expected) << rel << " footer tail";
        EXPECT_EQ(style_record_count(wide, kWide), expected.size())
            << rel << " every accepted statement is disclosed exactly once";
        EXPECT_EQ(style_record_count(capture(twin, "mermaid", kWide), kWide), 0u)
            << rel << " the stripped twin carries no disclosure record";
        std::size_t headers = 0;
        for (const auto& line : trimmed_lines(wide, kWide))
            if (line == kHeader) ++headers;
        EXPECT_EQ(headers, 1u) << rel << " a single projection header";
        EXPECT_EQ(art_above_footer(wide, kWide),
                  art_above_footer(capture(twin, "mermaid", kWide), kWide))
            << rel << " the style surface changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 15);
}

TEST(MermaidErD460Cohort, ClosedNegatives) {
    // Admitted positives. With an active presentation profile every accepted
    // statement and annotation is disclosed once.
    {
        const auto source = std::string{
            "---\nconfig:\n  htmlLabels: false\n---\n"
            "erDiagram\n"
            "  c[CUSTOMER]\n"
            "  p[PERSON]:::blue\n"
            "  classDef blue stroke:lightblue, color: #0000FF\n"
            "  classDef bold font-size:24px, font-weight: bold\n"
            "  class c,p bold\n"
            "  style c color:green\n"
            "  c ||--o{ p : owns\n"};
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        for (const auto record :
             {"p[PERSON]:::blue -> class surface; not applied",
              "classDef blue stroke:lightblue, color: #0000FF -> class surface; not applied",
              "classDef bold font-size:24px, font-weight: bold -> class surface; not applied",
              "class c,p bold -> class surface; not applied",
              "style c color:green -> class surface; not applied"})
            EXPECT_NE(frame.find(record), std::string::npos) << record;
    }
    // D-460 preserves the pre-D-460 profile-free surface byte-for-byte: a
    // source without a presentation profile keeps the silently-admitted old
    // surface (inline `:::` and `class` bindings carry no CSS) and discloses
    // only the newly admissible vocabulary.
    {
        const auto source = std::string{
            "erDiagram\n"
            "  c[CUSTOMER]\n"
            "  p[PERSON]:::blue\n"
            "  classDef blue stroke:lightblue, color: #0000FF\n"
            "  class c,p blue\n"
            "  c ||--o{ p : owns\n"};
        ASSERT_TRUE(draws(source, kWide));
        const auto wide = capture(source, "mermaid", kWide);
        EXPECT_EQ(style_record_count(wide, kWide), 1u);
        EXPECT_NE(wide.text.find("classDef blue stroke:lightblue, color: #0000FF -> class "
                                 "surface; not applied"),
                  std::string::npos);
        EXPECT_EQ(wide.text.find("p[PERSON]:::blue -> class surface; not applied"),
                  std::string::npos);
        EXPECT_EQ(wide.text.find("class c,p blue -> class surface; not applied"),
                  std::string::npos);
        EXPECT_EQ(art_above_footer(wide, kWide),
                  art_above_footer(capture(styling_stripped(source), "mermaid", kWide), kWide));
    }
    for (const auto& source : {
             // The closed CSS table: unknown key, unknown named colour, a
             // decimal or unit-less size, and an empty value all gate.
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a font-family:serif\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a fill:teal\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a font-size:24.5px\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a font-size:24\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a fill:\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a stroke-width:0px\n"},
             // A malformed annotation, a trailing CSS comma and a trailing
             // target comma all stay gated.
             std::string{"erDiagram\n  c[CUSTOMER]:::a::b\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  classDef a fill:red,\n"},
             std::string{"erDiagram\n  c[CUSTOMER]\n  style c, fill:red\n"},
             // A source carrying a subgraph rejects style statements (the
             // dispatcher's `groups` guard), whether or not the statement
             // follows `end`.
             std::string{"erDiagram\n  subgraph G\n  c[CUSTOMER]\n  classDef a fill:red\n  end\n"},
             std::string{"erDiagram\n  subgraph G\n  c[CUSTOMER]\n  end\n"
                         "  classDef a fill:red\n"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
