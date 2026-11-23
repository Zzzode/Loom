// D-443 inert look/htmlLabels/layout admission gate.
//
// Selection rationale: these are the last requirement, ishikawa and venn
// fallbacks whose bodies are byte-identical to their config-free twins and
// whose only unsupported frontmatter shape is a closed inert leaf group. Each
// family projection paints its own geometry and never reads the leaf, so the
// leaf is disclosed under the family footer and the art stays identical to the
// config-free twin.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/d443-inert-leaves-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP443_SENTINEL";
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
        if (line == "  look: handDrawn" || line == "  layout: elk" ||
            line == "    htmlLabels: false" || line == "  handDrawnSeed: 1" ||
            line == "  fontFamily: courier")
            return false;
    }
    return got.back() == "P443_SENTINEL";
}
// Strip the whole top-level `config:` block and every line indented under it.
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
        if (skipping && !line.empty() && line.front() == ' ') continue;
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
constexpr std::string_view kRequirementBody =
    "requirementDiagram\n  requirement req1 {\n    id: 1\n    text: probe\n"
    "    risk: low\n    verifymethod: test\n  }\n  element e1 {\n    type: probe\n  }\n"
    "  e1 - satisfies -> req1\n";
constexpr std::string_view kIshikawaBody =
    "ishikawa-beta\n  Blurry Photo\n  Process\n      Out of focus\n  User\n"
    "      Shaky hands\n";
constexpr std::string_view kVennBody =
    "venn-beta\n  title Probe Skills\n  set A\n  set B\n  union A, B\n";
std::string probe(std::string_view body, std::string_view frontmatter_tail) {
    return "---\ndescription: >-\n  D-443 probe\n" + std::string(frontmatter_tail) + "---\n" +
           std::string(body);
}
}  // namespace

TEST(MermaidD443InertLeavesCohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_d443_inert_leaves_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-443");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("requirement"), 3);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("ishikawa"), 1);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("venn"), 2);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "ead932f20b7a1ce70fa8287a26e1866320c398bde1040a3a1166fd3078a2e0df");
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
    EXPECT_EQ(digest_input.size(), 6u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidD443InertLeavesCohort, LeavesAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = config_free_twin(source);
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
               (twin_lines.back().empty() || twin_lines.back() == "P443_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the leaf changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 6);
}

TEST(MermaidD443InertLeavesCohort, ClosedLeavesStayWholeSource) {
    // Requirement: the namespace leaf composes with an already-admitted layout
    // and with a top-level htmlLabels leaf; both disclose in leaf order.
    {
        const auto source = probe(
            kRequirementBody,
            "config:\n  layout: elk\n  htmlLabels: true\n  flowchart:\n    htmlLabels: false\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.layout=elk -> external layout engine; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.htmlLabels=true -> native text labels; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.flowchart.htmlLabels=false -> native text labels; not applied"),
                  std::string::npos);
    }
    // Ishikawa: look composes with an admitted theme; the theme line stays first.
    {
        const auto source =
            probe(kIshikawaBody, "config:\n  theme: forest\n  look: handDrawn\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.theme=forest -> colour palette; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.look=handDrawn -> source node style; not applied"),
                  std::string::npos);
    }
    // Venn: the full look/seed/font trio.
    {
        const auto source = probe(kVennBody,
                                  "config:\n  look: handDrawn\n  handDrawnSeed: 7\n"
                                  "  fontFamily: courier\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.look=handDrawn -> source node style; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.handDrawnSeed=7 -> sketch jitter seed; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.fontFamily=\"courier\" -> terminal font; not applied"),
                  std::string::npos);
    }
    for (const auto* tail : {
             // Requirement: closed value tables and namespace shape rules.
             "config:\n  flowchart:\n    htmlLabels: maybe\n",
             "config:\n  flowchart:\n    htmlLabels:\n",
             "config:\n  flowchart:\n",
             "config:\n  flowchart:\n    useMaxWidth: false\n",
             "config:\n  flowchart:\n    htmlLabels: false\n  flowchart:\n    htmlLabels: false\n",
             "config:\n    flowchart:\n      htmlLabels: false\n",
             "config:\n  legacy: true\n",
         }) {
        const auto source = probe(kRequirementBody, tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    for (const auto* tail : {
             // Ishikawa: the look table is closed and one leaf only. These run
             // through the ishikawa body so the ishikawa rejection branch is
             // exercised directly.
             "config:\n  look: neo\n",
             "config:\n  look: HANDDRAWN\n",
             "config:\n  look:\n",
             "config:\n  look: handDrawn\n  look: handDrawn\n",
         }) {
        const auto source = probe(kIshikawaBody, tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    for (const auto* tail : {
             // Venn: closed look/seed/font tables.
             "config:\n  look: classic\n",
             "config:\n  look: neo\n",
             "config:\n  look:\n",
             "config:\n  handDrawnSeed: 0\n",
             "config:\n  handDrawnSeed: -1\n",
             "config:\n  handDrawnSeed: 1.5\n",
             "config:\n  handDrawnSeed: abc\n",
             "config:\n  handDrawnSeed:\n",
             "config:\n  fontFamily: inter\n",
             "config:\n  fontFamily:\n",
             "config:\n  look: handDrawn\n  fontFamily: courier\n  fontFamily: courier\n",
         }) {
        const auto source = probe(std::string_view{kVennBody}, tail);
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    // An empty config block remains whole-source in all three families.
    for (const auto& source :
         {probe(kRequirementBody, "config:\n"), probe(kIshikawaBody, "config:\n"),
          probe(kVennBody, "config:\n")}) {
        EXPECT_FALSE(draws(source, kWide));
    }
    // Config-free sources keep no disclosure footer.
    for (const auto& source : {std::string{kRequirementBody}, std::string{kIshikawaBody},
                               std::string{kVennBody}}) {
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_EQ(frame.find("native projection v1"), std::string::npos);
    }
}
