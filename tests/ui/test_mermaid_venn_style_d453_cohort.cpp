// D-453 venn closed `style` admission gate.
//
// Selection rationale: the five venn fallbacks whose only remaining blocker is
// the in-body `style <targets> <key>:<value>` statement (two rows also use the
// flat `text` child form and the `text id["Label"]` form). The venn projection
// paints its own labelled outline and draws no colour, so every style statement
// is inert and disclosed once in the footer, in source order.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/venn-style-d453-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP453_SENTINEL";
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
        if (line.starts_with("style ") && line.find("not applied") == std::string::npos)
            return false;
        if (line.starts_with("classDef") || line.find(":::") != std::string::npos) return false;
    }
    return got.back() == "P453_SENTINEL";
}
// The same fixture with the style statements removed and `text id["Label"]`
// collapsed to `text "Label"` (the same rendered outline).
std::string style_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string head = source.substr(0, close + 5);
    std::string out;
    const std::string body = source.substr(close + 5);
    for (std::size_t pos = 0;;) {
        const auto eol = body.find('\n', pos);
        const auto end = eol == std::string::npos ? body.size() : eol;
        std::string line = body.substr(pos, end - pos);
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) first = 0;
        const std::string stripped = line.substr(first);
        bool drop = stripped.starts_with("style ");
        if (!drop && stripped.starts_with("text ")) {
            const std::string rest = stripped.substr(5);
            const auto open = rest.find('[');
            const auto close = rest.rfind(']');
            // Only the `text id["Label"]` form carries an id: a quoted label
            // may itself contain brackets and is rendered verbatim.
            if (open != std::string::npos && close != std::string::npos && close > open &&
                !rest.empty() && rest.front() != '"' && rest.front() != '\'')
                line = std::string(first, ' ') + "text " +
                       rest.substr(open + 1, close - open - 1);
        }
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
    "venn-beta\n  set A\n  set B\n  union A, B\n";
std::string probe(std::string_view extra) {
    return "---\ndescription: >-\n  venn style probe\n---\n" + std::string(kBody) +
           std::string(extra);
}
}  // namespace

TEST(MermaidVennStyleD453Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_venn_style_d453_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-453");
    EXPECT_EQ(root.get("counts").get_int("total"), 5);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("venn"), 5);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "298ebaada6c3203ca5fe393ca4aa4e707825a774628db36c4e39cf0dd1bc7a1f");
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

TEST(MermaidVennStyleD453Cohort, StylesAreInertAgainstStyleFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = style_free_twin(source);
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
        // The twin keeps its frontmatter and therefore its own footer; compare
        // the art above each frame's footer.
        auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        while (!twin_lines.empty() &&
               (twin_lines.back().empty() || twin_lines.back() == "P453_SENTINEL"))
            twin_lines.pop_back();
        const auto header_at = [](const std::vector<std::string>& rows) {
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (rows[i] == "Venn native projection v1") return i;
            return rows.size();
        };
        const auto twin_art_end = header_at(twin_lines);
        std::vector<std::string> twin_art(twin_lines.begin(),
                                          twin_lines.begin() +
                                              static_cast<std::ptrdiff_t>(twin_art_end));
        while (!twin_art.empty() && twin_art.back().empty()) twin_art.pop_back();
        EXPECT_EQ(art, twin_art) << rel << " the styles changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 5);
}

TEST(MermaidVennStyleD453Cohort, ClosedStyleNegatives) {
    // Admitted positives: single- and multi-target styles, flat text children
    // and the text id form.
    {
        const auto source = probe("  style A fill:#ff0000\n  style A,B color:#00ff00\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("Venn native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("style A [fill] -> SVG colour; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("style A,B [color] -> SVG colour; not applied"),
                  std::string::npos);
    }
    {
        const auto source =
            "venn-beta\n  set A\n  text \"Flat\"\n  set B\n  union A, B\n"
            "  text X1[\"Labelled\"]\n";
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("- text Flat"), std::string::npos);
        EXPECT_NE(frame.find("- text Labelled"), std::string::npos);
    }
    {
        // Distinct values under the same target are distinct statements, not
        // duplicates; the rendered disclosure legitimately repeats the key set.
        const auto source = probe("  style A fill:#ffffff\n  style A fill:#000000\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("style A [fill]"),
                  std::string::npos);
    }
    {
        // A quoted label may itself contain brackets: the whole quoted string
        // is the label, not an `id["Label"]` pair.
        const auto source =
            "venn-beta\n  set A\n    text \"a [b] c\"\n  set B\n  union A, B\n";
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("text a [b] c"),
                  std::string::npos);
    }
    for (const auto* extra : {
             // The closed property set and value shape.
             "  style A unknown:#ff0000\n",
             "  style A fill:\"#ff0000\"\n",
             "  style A fill:#ff0000(x)\n",
             "  style A fill:#ff0000,fill:#00ff00\n",
             "  style 1A fill:#ff0000\n",
             // Identifiers match the set-name charset (no dashes).
             "  style A-B fill:#ff0000\n",
             "  style A fill:#ff0000\n  style A fill:#ff0000\n",
             // classDef/::: remain whole-source gates.
             "  classDef red fill:#ff0000\n",
             "  set C:::red\n",
         }) {
        const auto source = probe(extra);
        EXPECT_FALSE(draws(source, kWide)) << extra;
        EXPECT_FALSE(native_frame(source, kWide)) << extra;
    }
    // A style-free source keeps no disclosure footer.
    {
        const auto frame = capture(probe(""), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Venn native projection v1"), std::string::npos);
    }
}
