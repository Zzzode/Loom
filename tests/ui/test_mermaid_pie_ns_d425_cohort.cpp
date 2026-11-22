// D-425 pie closed config-namespace gate.
//
// Selection rationale (TL directive: state it in the gate, not a design doc):
// the body-only census over all 565 fallbacks found 133 that draw once the
// frontmatter is removed; pie is the largest single-family clean seam (8 of
// its 12 fallbacks) and they share one closed namespace. Expected drop: 8
// rows (pie 12 -> 4, global 565 -> 557, formal 1394 -> 1402).
//
// Criterion (semantic, D-412): every admitted leaf is the source's own state
// marker for a pixel-only presentation the terminal cannot express
// (highlight, legend placement, donut hole, label radius, responsive scaling)
// or a logger-only key. Therefore the drawn art must equal the config-free
// twin's art byte for byte and only the disclosure footer may differ — the
// strongest form of "inert", not a render-difference proxy. The four pie rows
// that still fall back carry body `accTitle`/`accDescr` lines and stay
// whole-source until the pie accessibility no-op lands.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/pie-ns-d425-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "Pie native projection v1";
constexpr std::string_view kBody = "pie title Probe\n  \"A\" : 1\n";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP425_SENTINEL";
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
        if (line == "  pie:" || line == "pie title Probe") return false;
    }
    return got.back() == "P425_SENTINEL";
}
std::vector<std::string> canvas_lines(const std::string& source) {
    auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    std::size_t header = lines.size();
    for (std::size_t i = lines.size(); i-- > 0;)
        if (lines[i] == kHeader) {
            header = i;
            break;
        }
    std::vector<std::string> out(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(header));
    while (!out.empty() && (out.back().empty() || out.back() == "P425_SENTINEL")) out.pop_back();
    return out;
}
// The synthetic twin: delete the `config:` line and every following line
// indented at least two spaces.
std::string config_free_twin(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
    const std::string body = source.substr(close + 5);
    std::string kept = "---\n";
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
    kept += "---\n";
    return kept + body;
}
bool twin_is_config_free(const std::string& source) {
    if (!source.starts_with("---\n")) return false;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return false;
    const std::string frontmatter = source.substr(4, close - 4);
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        if (line.starts_with("config:")) return false;
    }
    return true;
}
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    // `footer` starts with the header line; the frame prepends one blank row.
    std::vector<std::string> expected{""};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "P425_SENTINEL") << label;
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
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
std::vector<std::string> expected_footer_from_leaves(const JsonVal& leaves) {
    std::vector<std::string> footer{std::string(kHeader)};
    const auto append = [&](const char* key, const char* prefix, const char* suffix) {
        if (leaves.has(key))
            footer.push_back(std::string(prefix) + leaves.get_string(key) + suffix);
    };
    append("logLevel", "config.logLevel=", " -> logger only; not applied");
    append("pie.donutHole", "config.pie.donutHole=", " -> donut hole; not applied");
    append("pie.highlightSlice", "config.pie.highlightSlice=", " -> slice highlight; not applied");
    append("pie.legendPosition", "config.pie.legendPosition=", " -> legend placement; not applied");
    append("pie.textPosition", "config.pie.textPosition=", " -> label radius; not applied");
    append("pie.useMaxWidth", "config.pie.useMaxWidth=", " -> responsive scaling; not applied");
    return footer;
}
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  pie namespace boundary probe\n" + std::string(config) +
           "---\n" + std::string(kBody);
}
}  // namespace

TEST(MermaidPieNamespaceD425Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_pie_ns_d425_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-425");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 8);
    EXPECT_EQ(counts.get_int("pie.donutHole"), 3);
    EXPECT_EQ(counts.get_int("pie.highlightSlice"), 2);
    EXPECT_EQ(counts.get_int("pie.legendPosition"), 1);
    EXPECT_EQ(counts.get_int("pie.textPosition"), 1);
    EXPECT_EQ(counts.get_int("pie.useMaxWidth"), 1);
    EXPECT_EQ(counts.get_int("logLevel"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_FALSE(root.get("review").get_string("reviewer_agent").empty());

    const auto selection = root.get("selection");
    EXPECT_EQ(selection.get_string("cohort_digest"),
              "1a8a8a667f715cdede98e665e19cd83db198a3e9a1fe064639f14badbcff4f6f");
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
        // The pinned footer is exactly what the closed namespace implies.
        EXPECT_EQ(string_list(row.get("expected_footer_lines")),
                  expected_footer_from_leaves(row.get("leaves")))
            << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 8u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));
}

TEST(MermaidPieNamespaceD425Cohort, RowsAreInertAgainstConfigFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin_source = config_free_twin(source);
        const auto states = load_states(row.get("expected_states"));
        const auto footer = string_list(row.get("expected_footer_lines"));

        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        EXPECT_TRUE(twin_is_config_free(twin_source)) << rel << " twin still carries config";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        expect_footer(source, footer, rel);

        // Inertness, in the strongest form: the art above the footer is the
        // config-free twin's art, line for line. No leaf may reflow or relabel.
        const auto got_canvas = canvas_lines(source);
        const auto twin_canvas = canvas_lines(twin_source);
        ASSERT_FALSE(twin_canvas.empty()) << rel;
        EXPECT_EQ(got_canvas, twin_canvas) << rel << " leaf changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 8);
}

TEST(MermaidPieNamespaceD425Cohort, ClosedNamespaceNegativesStayWholeSource) {
    // Positives: one leaf at a time, each value forming the closed set.
    for (const auto* leaf : {"    donutHole: 0.4\n", "    highlightSlice: hover\n",
                             "    legendPosition: bottom\n", "    textPosition: 0.5\n",
                             "    useMaxWidth: true\n", "    useMaxWidth: false\n",
                             "    donutHole: -0.3\n", "    donutHole: 1.2\n"}) {
        const auto source = probe(std::string("config:\n  pie:\n") + leaf);
        ASSERT_TRUE(draws(source, kWide)) << leaf;
        ASSERT_TRUE(native_frame(source, kWide)) << leaf;
    }
    // A logger-only top-level leaf composes with the namespace.
    {
        const auto source =
            probe("config:\n  logLevel: 2\n  pie:\n    donutHole: 0.4\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto text = capture(source, "mermaid", kWide).text;
        const auto level_at = text.find("config.logLevel=2 -> logger only; not applied");
        const auto hole_at = text.find("config.pie.donutHole=0.4 -> donut hole; not applied");
        ASSERT_NE(level_at, std::string::npos);
        ASSERT_NE(hole_at, std::string::npos);
        EXPECT_LT(level_at, hole_at);
    }
    // A quoted scalar is the same YAML value (`chart_yaml_scalar` unquotes, as
    // the git `git_inert_scalar` does), so it is admitted and disclosed with
    // the normalized value.
    {
        const auto source = probe("config:\n  pie:\n    textPosition: \"0.5\"\n");
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_TRUE(has_text(source, "config.pie.textPosition=0.5 -> label radius; not applied"));
    }
    // Negatives: anything outside the closed namespace stays whole-source.
    for (const auto* form : {
             "config:\n  pie:\n    showValues: true\n",       // unknown leaf
             "config:\n  pie:\n    legendPosition: side\n",    // closed enum
             "config:\n  pie:\n    textPosition: 1.4\n",       // reviewed range
             "config:\n  pie:\n    useMaxWidth: yes\n",        // not a boolean
             "config:\n  pie:\n    donutHole: 0.4\n    donutHole: 0.5\n",  // duplicate
             "config:\n  pie:\n      donutHole: 0.4\n",        // wrong indent
             "config:\n  logLevel: 9\n",                       // log level vocabulary
             "config:\n  theme: redux\n  pie:\n    donutHole: 0.4\n",  // theme+namespace
             "config:\n  pie: {}\n",                           // empty namespace
         }) {
        const auto source = probe(form);
        EXPECT_FALSE(draws(source, kWide)) << form;
        EXPECT_FALSE(native_frame(source, kWide)) << form;
    }
    // A second config block and a non-description frontmatter stay out.
    EXPECT_FALSE(draws("---\nconfig:\n  pie:\n    donutHole: 0.4\n---\n" + std::string(kBody),
                       kWide));
}
