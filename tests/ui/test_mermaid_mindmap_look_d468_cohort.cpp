// D-468 mindmap closed look/theme frontmatter gate.
//
// Six mindmap originals carry the same body under the closed `config.look:
// neo` leaf and one of the six closed theme names. A cell grid has no sketch
// stroke and no colour, so both leaves are inert: the art equals the
// config-free twin and each leaf is disclosed once under `Mindmap native
// projection v1`.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/mindmap-look-d468-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP468_SENTINEL";
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

// Strip the top-level `config:` block and every line indented under it.
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
        kept += '\n';
    }
    return "---\n" + kept + "---\n" + body;
}

constexpr const char* kBody = R"(mindmap
  root((probe))
    Alpha
      A1
      A2
    Beta
)";

// Description-bearing probe: the envelope requires the description first.
std::string probe(std::string config) {
    std::string out = "---\ndescription: >-\n  probe\n";
    out += config;
    out += "---\n";
    out += kBody;
    return out;
}

std::size_t footer_begin(const std::vector<std::string>& lines) {
    // Footer = blank separator + `Mindmap native projection v1` + notices.
    for (std::size_t at = lines.size(); at-- > 0;)
        if (lines[at] == "Mindmap native projection v1") return at == 0u ? 0u : at - 1u;
    return lines.size();
}
}  // namespace

TEST(MermaidMindmapLookD468Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_mindmap_look_d468_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-468");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("mindmap"), 6);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_TRUE(root.get("review").get_string("reviewer_agent").starts_with("task "))
        << "the review record must name the reviewer agent";
    const auto manifest = parse_doc(kManifest);
    EXPECT_EQ(root.get("upstream").get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    std::vector<std::pair<std::string, std::string>> digest_input;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  row.get_string("sha256"))
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

TEST(MermaidMindmapLookD468Cohort, AdmittedRowsDrawDiscloseAndKeepTheTwinArt) {
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
        EXPECT_TRUE(same_surface(capture(source, "mermaid", kNarrow),
                                 capture(source, "text", kNarrow)))
            << rel << " narrow render is not the whole source";
        EXPECT_TRUE(same_surface(wide, capture(source, "mermaid", kWide)))
            << rel << " restored cells differ from the first wide capture";
        EXPECT_NE(wide.text.find("Mindmap native projection v1"), std::string::npos) << rel;
        row.get("expected_captions").iter([&](JsonVal caption) {
            EXPECT_NE(wide.text.find(caption.as_str()), std::string::npos)
                << rel << ' ' << caption.as_str();
        });
        std::size_t previous = 0;
        row.get("expected_notices").iter([&](JsonVal notice) {
            const auto text = notice.as_str();
            const auto at = wide.text.find(text);
            ASSERT_NE(at, std::string::npos) << rel << " notice: " << text;
            EXPECT_GT(at, previous) << rel << " notice order: " << text;
            previous = at;
        });
        EXPECT_EQ(wide.text.find("config:"), std::string::npos) << rel << " config drawn";
        EXPECT_EQ(wide.text.find("look: neo"), std::string::npos) << rel << " look drawn";
        // The disclosed art equals the config-free twin's art; only the footer
        // rows are new.
        auto lines = trimmed_lines(wide, kWide);
        const auto begin = footer_begin(lines);
        ASSERT_LT(begin, lines.size()) << rel << " no footer";
        const std::vector<std::string> art(lines.begin(),
                                           lines.begin() + static_cast<std::ptrdiff_t>(begin));
        auto twin = trimmed_lines(capture(config_free_twin(source), "mermaid", kWide), kWide);
        // The twin still discloses its own shape leaves, so its footer is
        // stripped the same way; the two artefacts must match row for row.
        const auto twin_begin = footer_begin(twin);
        std::vector<std::string> twin_art(twin.begin(),
                                          twin.begin() + static_cast<std::ptrdiff_t>(twin_begin));
        while (!twin_art.empty() &&
               (twin_art.back().empty() || twin_art.back() == "P468_SENTINEL"))
            twin_art.pop_back();
        EXPECT_EQ(art, twin_art) << rel << " look/theme changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 6);
}

TEST(MermaidMindmapLookD468Cohort, ClosedLookThemeGrammarStaysClosed) {
    // Paired positives: every admitted placement draws and discloses.
    for (const auto* config : {
             "config:\n  look: neo\n",
             "config:\n  theme: neo\n",
             "config:\n  theme: redux-dark-color\n",
             "config:\n  look: neo\n  theme: redux\n",
             "config:\n  layout: tidy-tree\n  look: neo\n  theme: neo-dark\n",
         }) {
        const auto source = probe(config);
        EXPECT_TRUE(draws(source, kWide)) << config;
        EXPECT_TRUE(draws(source, kNarrow) == false) << config;
        const auto text = capture(source, "mermaid", kWide).text;
        if (std::string_view{config}.find("look:") != std::string_view::npos)
            EXPECT_NE(text.find("config.look=neo -> source node style; not applied"),
                      std::string::npos)
                << config;
        if (std::string_view{config}.find("theme:") != std::string_view::npos)
            EXPECT_NE(text.find("-> colour palette; not applied"), std::string::npos) << config;
    }
    // Paired negatives: every other spelling stays whole source.
    for (const auto* config : {
             "config:\n  theme: base\n",
             "config:\n  theme: Neo\n",
             "config:\n  theme: neo \n",
             "config:\n  theme: neo # note\n",
             "config:\n  look: classic\n",
             "config:\n  look: neo\n  look: neo\n",
             "config:\n  theme: neo\n  theme: neo\n",
             "config:\n  layout: tidy-tree\n  theme: base\n",
             "config:\n  layout: elk\n  look: neo\n",
             "config:\n  mindmap:\n    colorBy: depth\n",
             "config:\n",
             "config:\n  look:neo\n",
             "config:\n   look: neo\n",
         }) {
        EXPECT_FALSE(draws(probe(config), kWide)) << config;
    }
    // The config block must follow the description.
    EXPECT_FALSE(draws("---\nconfig:\n  look: neo\ndescription: >-\n  probe\n---\n" +
                           std::string(kBody),
                       kWide));
}
