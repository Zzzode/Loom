// D-461 state-v2 admission cohort gate, stages 1-2.
//
// Stage 1 (`native-frame` rows): ten originals whose only blocker was the fixed
// 1M layout Work primary; the bounded canvas-derived state envelope funds them.
// The art contract is unchanged, so the gate pins the wide/narrow/restored
// states, restored cell equality, and that every authored style statement
// appears verbatim in the state legend.
// Stage 2 (`noop-twin` / `style-twin` rows): the two `click` bindings and the
// one `font-style:italic` classDef. A `click` line paints nothing, so the
// click-deleted twin must be cell-identical; `font-style` is dropped and
// unprojectable, so replacing it with the equally unpainted `font-weight:
// normal` must leave the art above the legend cell-identical.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-d461-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP461_SENTINEL";
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
    if (capture.chars.size() != static_cast<std::size_t>(width) * kHeight) return {};
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

// The drawing rows above the state style legend. Legend rows are contiguous at
// the end and consist of the key line plus `<verbatim> -> <tokens>` records and
// `<verbatim> definition: <name> (applied)` lines.
std::vector<std::string> art_above_legend(const CellCapture& capture, int width) {
    auto lines = trimmed_lines(capture, width);
    while (!lines.empty() && lines.back() == "P461_SENTINEL") lines.pop_back();
    while (!lines.empty() && lines.back().empty()) lines.pop_back();
    std::size_t legend = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
        if (lines[i].starts_with("key: hue roles use palette")) {
            legend = i;
            break;
        }
    if (legend != lines.size()) {
        while (legend > 0 && (lines[legend - 1].find(" ->") != std::string::npos ||
                              lines[legend - 1].find(" definition: ") != std::string::npos))
            --legend;
    }
    std::vector<std::string> art(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(legend));
    while (!art.empty() && art.back().empty()) art.pop_back();
    return art;
}

// Remove the `click ...` directive lines (the non-visual binding). Mirrors the
// production keyword predicate: `click` followed by a space or a tab.
std::string click_stripped(const std::string& source) {
    std::string out;
    std::size_t pos = 0;
    while (pos <= source.size()) {
        const auto eol = source.find('\n', pos);
        const std::string line = source.substr(pos, eol == std::string::npos ? std::string::npos
                                                                            : eol - pos);
        pos = eol == std::string::npos ? source.size() + 1 : eol + 1;
        const auto first = line.find_first_not_of(" \t");
        const std::string_view content =
            first == std::string::npos ? std::string_view{} : std::string_view{line}.substr(first);
        if (content.starts_with("click") && content.size() > 6u &&
            (content[5] == ' ' || content[5] == '\t'))
            continue;
        out += line;
        if (eol == std::string::npos) break;
        out += '\n';
    }
    return out;
}

// Replace the dropped `font-style:italic;` with an equally unpainted property.
std::string font_style_twin(const std::string& source) {
    auto twin = source;
    const auto at = twin.find("font-style:italic;");
    if (at != std::string::npos) twin.replace(at, std::string("font-style:italic;").size(),
                                              "font-weight:normal");
    return twin;
}
}  // namespace

TEST(MermaidStateD461Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_state_d461_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-461");
    EXPECT_EQ(root.get("counts").get_int("total"), 13);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("state-diagram-v2"), 13);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    // The deferred sets stay explicit: seven routing-geometry rows and the
    // bare `info` directive row.
    EXPECT_EQ(root.get("deferred").get("stage3_routing_geometry").size(), 7u);
    EXPECT_EQ(root.get("deferred").get("stage4_info_directive").size(), 1u);
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
        // The row hash is what the cohort digest binds, so verify it directly
        // against the bytes (not only transitively through the manifest).
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  row.get_string("sha256"))
            << rel;
        unique_paths.insert(rel);
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 13u);
    EXPECT_EQ(unique_paths.size(), 13u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidStateD461Cohort, AdmittedRowsDrawAndPinTheirStates) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto states = row.get("expected_states");
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow state";
        const auto wide = capture(source, "mermaid", kWide);
        static_cast<void>(capture(source, "mermaid", kNarrow));
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        // A native projection never leaves the raw diagram header in the art.
        EXPECT_EQ(wide.text.find("stateDiagram-v2"), std::string::npos) << rel;
        // The art above the legend must be a real box graph, not a legend-only
        // render: at least four rows plus a frame glyph. A row that regressed
        // to a legend-only projection would otherwise still differ from the
        // text fence and pass `draws()`.
        const auto art = art_above_legend(wide, kWide);
        EXPECT_GE(art.size(), 4u) << rel;
        const bool framed = std::ranges::any_of(art, [](const std::string& line) {
            return line.find("─") != std::string::npos ||
                   line.find("│") != std::string::npos;
        });
        EXPECT_TRUE(framed) << rel << " the art carries no frame glyph";
        // Each declared state label must appear above the legend, so a render
        // that keeps only the disclosure payload cannot pass.
        std::string art_text;
        for (const auto& line : art) art_text += line + '\n';
        row.get("expected_art_labels").iter([&](JsonVal label) {
            const auto text = label.as_str();
            if (!text.empty()) EXPECT_NE(art_text.find(text), std::string::npos) << rel << text;
        });
        const auto oracle = row.get_string("oracle");
        if (oracle == "noop-twin") {
            const auto twin = click_stripped(source);
            EXPECT_NE(twin, source) << rel << " (the twin must strip the click binding)";
            EXPECT_TRUE(same_surface(wide, capture(twin, "mermaid", kWide)))
                << rel << " the click binding changed the frame";
        } else if (oracle == "style-twin") {
            const auto twin = font_style_twin(source);
            EXPECT_NE(twin, source) << rel << " (the twin must replace font-style)";
            EXPECT_TRUE(draws(twin, kWide)) << rel << " (the twin must draw)";
            EXPECT_EQ(art_above_legend(wide, kWide),
                      art_above_legend(capture(twin, "mermaid", kWide), kWide))
                << rel << " the dropped font-style changed the art";
        } else {
            EXPECT_EQ(oracle, "native-frame") << rel;
        }
        // Every authored style statement is echoed in the legend.
        row.get("expected_style_statements").iter([&](JsonVal statement) {
            const auto text = statement.as_str();
            if (!text.empty()) EXPECT_NE(wide.text.find(text), std::string::npos) << rel << text;
        });
        ++checked;
    });
    EXPECT_EQ(checked, 13);
}

TEST(MermaidStateD461Cohort, ClosedNegatives) {
    for (const auto& source : {
             // The click grammar stays closed: unquoted arguments, no
             // arguments, an unknown target, and the `href`/`call` forms.
             std::string{"stateDiagram-v2\nA\nclick A https://example.com\n"},
             std::string{"stateDiagram-v2\nA\nclick A\n"},
             std::string{"stateDiagram-v2\nA\nclick Ghost \"https://example.com\"\n"},
             std::string{"stateDiagram-v2\nA\nclick A href \"https://example.com\"\n"},
             std::string{"stateDiagram-v2\nA\nclick A call callback()\n"},
             std::string{"stateDiagram-v2\nA\nclick A \"\"\n"},
             std::string{"stateDiagram-v2\nA\nclick A \"a\" \"b\" \"c\"\n"},
             // font-style only accepts italic|normal, and still one value.
             std::string{"stateDiagram-v2\nclassDef c font-style:oblique\nA\nclass A c\n"},
             std::string{"stateDiagram-v2\nclassDef c font-style:italic,font-style:normal\nA\n"
                         "class A c\n"},
             std::string{"stateDiagram-v2\nclassDef c font-style:\nA\nclass A c\n"},
             // `click` is reserved as a statement keyword: a state that uses
             // it as a transition endpoint is intercepted and rejected
             // (fail-closed), never silently routed as an edge.
             std::string{"stateDiagram-v2\nclick --> B\nB\n"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
    // Paired admitted positives for the same grammar.
    EXPECT_TRUE(
        draws("stateDiagram-v2\nA\nclick A \"https://example.com\" \"title\"\n", kWide));
    EXPECT_TRUE(draws("stateDiagram-v2\nA\nclick A \"https://example.com\"\n", kWide));
    EXPECT_TRUE(draws("stateDiagram-v2\nclassDef c font-style:italic\nA\nclass A c\n", kWide));
    EXPECT_TRUE(draws("stateDiagram-v2\nclassDef c font-style:normal\nA\nclass A c\n", kWide));
}
