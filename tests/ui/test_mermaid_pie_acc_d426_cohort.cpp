// D-426 pie accessibility (accTitle/accDescr) no-op gate.
//
// Selection rationale (TL directive): the last four pie fallbacks are the
// baseline-theme rows whose body carries a scalar `accTitle:`/`accDescr:`
// pair. Those keys are accessibility metadata — Mermaid renders them into the
// accessibility DOM only — and a text terminal has no separate accessibility
// tree, so they are validated no-ops (the RFC 0048 State P2 contract). The
// same slice closes the pie family: expected drop 4 rows (pie 4 -> 0, global
// 557 -> 553, formal 1402 -> 1406).
//
// Criterion (semantic, D-412): the drawn frame must equal the frame of the
// same source with the two acc lines deleted, byte for byte — the strongest
// possible statement that the lines change nothing. The three theme values
// these rows add to the closed table (`default`, `forest`, `neutral`) are
// inert and disclosed exactly like the RFC 0042 set.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/pie-acc-d426-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "Pie native projection v1";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP426_SENTINEL";
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
// The synthetic twin: the same source with the scalar acc lines deleted.
std::string acc_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string body = source.substr(close + 5);
    std::string kept;
    for (std::size_t pos = 0; pos < body.size();) {
        const auto eol = body.find('\n', pos);
        const auto end = eol == std::string::npos ? body.size() : eol;
        const std::string line = body.substr(pos, end - pos);
        const auto first = line.find_first_not_of(" \t");
        const std::string_view stripped =
            first == std::string::npos ? std::string_view{} : std::string_view(line).substr(first);
        if (stripped.starts_with("accTitle:") || stripped.starts_with("accDescr:")) {
            pos = end + 1;
            continue;
        }
        kept += line;
        if (eol != std::string::npos) kept += '\n';
        pos = end + 1;
    }
    return source.substr(0, close + 5) + kept;
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
        if (line == "  theme:" || line == "  accTitle:" || line == "  accDescr:") return false;
        if (line == "---") return false;
    }
    return got.back() == "P426_SENTINEL";
}
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    std::vector<std::string> expected{""};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "P426_SENTINEL") << label;
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
std::string theme_footer(const std::string& theme) {
    return "config.theme=" + theme + " -> colour palette; not applied";
}
std::string probe(std::string_view theme, std::string_view acc_lines,
                  std::string_view body = "pie title Probe\n  \"A\" : 1\n  \"B\" : 2\n") {
    return "---\ndescription: >-\n  pie accessibility probe\nconfig:\n  theme: " +
           std::string(theme) + "\n---\n" + std::string(body) + std::string(acc_lines);
}
}  // namespace

TEST(MermaidPieAccessibilityD426Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_pie_acc_d426_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-426");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 4);
    for (const auto* theme : {"dark", "default", "forest", "neutral"})
        EXPECT_EQ(counts.get("themes").get_int(theme), 1) << theme;
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "d0618087dabc24d2d7086b925076cb0cc732580ade49f5276fa25e591926b9c3");
    const auto table = string_list(root.get("selection").get("theme_table"));
    EXPECT_EQ(table.size(), 8u);
    for (const auto* theme : {"dark", "default", "forest", "neutral"})
        EXPECT_NE(std::ranges::find(table, theme), table.end()) << theme;

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
        const auto theme = row.get_string("theme");
        EXPECT_EQ(string_list(row.get("expected_footer_lines")),
                  (std::vector<std::string>{std::string(kHeader), theme_footer(theme)}))
            << rel;
        // The pinned twin is exactly the acc-line-deleted source.
        const auto twin = acc_free_twin(read_file(kCases / rel));
        EXPECT_EQ(loom::utils::crypto::sha256(twin), row.get_string("twin_sha256")) << rel;
        EXPECT_NE(twin, read_file(kCases / rel)) << rel << " twin differs";
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 4u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidPieAccessibilityD426Cohort, AccLinesChangeNothing) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = acc_free_twin(source);
        const auto states = load_states(row.get("expected_states"));
        const auto footer = string_list(row.get("expected_footer_lines"));

        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        expect_footer(source, footer, rel);

        // The whole frame — art and footer — is identical to the twin's.
        const CellCapture with_acc = capture(source, "mermaid", kWide);
        const CellCapture without_acc = capture(twin, "mermaid", kWide);
        EXPECT_TRUE(same_surface(with_acc, without_acc)) << rel << " acc lines changed the frame";
        EXPECT_TRUE(same_surface(with_acc, capture(source, "mermaid", kWide)))
            << rel << " frame is stable across captures";
        ++checked;
    });
    EXPECT_EQ(checked, 4);
}

TEST(MermaidPieAccessibilityD426Cohort, AccessibilityNegativesStayWholeSource) {
    // Positives: every closed-table theme composes with the acc no-ops, and a
    // quoted payload is the same scalar.
    for (const auto* theme : {"dark", "default", "forest", "neutral", "redux", "redux-dark",
                              "redux-color", "redux-dark-color"}) {
        const auto source =
            probe(theme, "  accTitle: This is a title\n  accDescr: This is a description\n");
        ASSERT_TRUE(draws(source, kWide)) << theme;
        ASSERT_TRUE(native_frame(source, kWide)) << theme;
        expect_footer(source, {std::string(kHeader), theme_footer(theme)}, theme);
    }
    {
        const auto source =
            probe("default", "  accTitle: \"quoted title\"\n  accDescr: 'quoted descr'\n");
        ASSERT_TRUE(draws(source, kWide));
    }
    // Negatives: block form, duplicates, empty payloads, oversized payloads,
    // control bytes, a bare identifier and the missing colon all stay source.
    for (const auto* acc : {
             "  accDescr {\n    A description block\n  }\n",
             "  accTitle: one\n  accTitle: two\n  accDescr: d\n",
             "  accTitle: one\n  accDescr: d\n  accDescr: again\n",
             "  accTitle:\n  accDescr: d\n",
             "  accTitle: one\n  accDescr:\n",
             "  accTitle: one\n  accDescr\n",
             "  accTitle\n",
             "  accTitle: one\n  accDescr: bad\x01byte\n",
         }) {
        const auto source = probe("default", acc);
        EXPECT_FALSE(draws(source, kWide)) << acc;
        EXPECT_FALSE(native_frame(source, kWide)) << acc;
    }
    // A payload longer than the reviewed 256-byte bound stays source.
    {
        std::string long_line = "  accTitle: ";
        long_line += std::string(300, 'x');
        long_line += "\n  accDescr: d\n";
        const auto source = probe("default", long_line);
        EXPECT_FALSE(draws(source, kWide)) << "oversized accTitle";
    }
    // A theme outside the closed table stays source with the acc pair present.
    {
        const auto source = probe("base", "  accTitle: t\n  accDescr: d\n");
        EXPECT_FALSE(draws(source, kWide));
        EXPECT_FALSE(native_frame(source, kWide));
    }
}
