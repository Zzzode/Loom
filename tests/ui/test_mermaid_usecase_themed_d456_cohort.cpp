// D-456 usecase themed-envelope cohort gate.
//
// Selection rationale: the nine themed usecase originals whose only blockers are
// the closed `config.theme` / `config.look` / `config.usecase` leaves, the quoted
// `systemBoundary "Title"` label and the `json <id>@{ ... }` block. The usecase
// projection is a verbatim text outline that never reads a palette or a source
// node style, so every accepted leaf is inert and disclosed; the boundary title
// and the JSON block are transcribed as authored.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-themed-d456-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP456_SENTINEL";
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
        if (line.starts_with("config.") && line.find("not applied") == std::string::npos)
            return false;
    }
    return got.back() == "P456_SENTINEL";
}
// The same fixture with the frontmatter removed (the envelope is the only
// admitted frontmatter surface, so the body art must be identical).
std::string frontmatter_free_twin(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
constexpr std::string_view kBody =
    "usecase-beta\n"
    "  actor A\n"
    "  B(\"Use case\")\n"
    "  A --> B\n";
std::string probe(std::string_view config, std::string_view body = kBody) {
    return "---\ndescription: >-\n  probe\n" + std::string{config} + "---\n" +
           std::string{body};
}
}  // namespace

TEST(MermaidUsecaseThemedD456Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_usecase_themed_d456_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-456");
    EXPECT_EQ(root.get("counts").get_int("total"), 9);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("usecase"), 9);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "ca560262b56299475c67f52f7e1f9695736bb5fcf6391ab3cd614f6dca8056aa");
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
    EXPECT_EQ(digest_input.size(), 9u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidUsecaseThemedD456Cohort, LeavesAreInertAgainstFrontmatterFreeTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = frontmatter_free_twin(source);
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
               (twin_lines.back().empty() || twin_lines.back() == "P456_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the leaves changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 9);
}

TEST(MermaidUsecaseThemedD456Cohort, ClosedNegatives) {
    // Admitted positives: theme, look, the extended leaves, a quoted boundary
    // title and a JSON block.
    {
        const auto source = probe("config:\n  theme: forest\n  usecase:\n"
                                  "    diagramPadding: 24\n    useMaxWidth: true\n"
                                  "    colorScheme: rotate\n");
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.theme=forest -> colour palette; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.usecase.colorScheme=rotate -> SVG layout tuning; "
                             "not applied"),
                  std::string::npos);
    }
    {
        const auto source = probe("config:\n  look: handDrawn\n",
                                  "usecase-beta\n  systemBoundary \"Auth System\"\n"
                                  "    A(\"Sign in\")\n  end\n"
                                  "  json Payload@{\n    \"k\": \"v\",\n"
                                  "    \"n\": [\"x\"]\n  }\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.look=handDrawn -> source node style; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("systemBoundary \"Auth System\""), std::string::npos);
    }
    {
        // The accepted look leaf is inert: the with-key art equals the art of
        // the same body without the leaf (footer = blank, header, look line,
        // sentinel paragraph).
        const auto source = probe("config:\n  look: handDrawn\n");
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_GE(lines.size(), 6u) << source;
        const auto begin = lines.size() - 5u;
        ASSERT_EQ(lines[begin], "");
        ASSERT_EQ(lines[begin + 1], "Usecase native projection v1");
        std::vector<std::string> art(lines.begin(),
                                     lines.begin() + static_cast<std::ptrdiff_t>(begin));
        while (!art.empty() && art.back().empty()) art.pop_back();
        auto body_lines = trimmed_lines(capture(std::string{kBody}, "mermaid", kWide), kWide);
        while (!body_lines.empty() &&
               (body_lines.back().empty() || body_lines.back() == "P456_SENTINEL"))
            body_lines.pop_back();
        EXPECT_EQ(art, body_lines) << "the look leaf changed the art";
    }
    {
        // A leaf under `usecase` followed by the top-level theme is admitted.
        const auto source = probe("config:\n  usecase:\n    actorFontSize: 16\n"
                                  "  theme: dark\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.theme=dark -> colour palette; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("config.usecase.actorFontSize=16 -> SVG layout tuning; "
                             "not applied"),
                  std::string::npos);
    }
    {
        // `json` as an edge endpoint is not a block opener.
        const auto source = std::string{"usecase-beta\n  json --> Foo\n"};
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("json --> Foo"),
                  std::string::npos);
    }
    for (const auto& source : {
             // The closed leaf tables.
             probe("config:\n  theme: unknown\n"),
             probe("config:\n  theme: forest\n  theme: dark\n"),
             probe("config:\n  look: classic\n"),
             probe("config:\n  usecase:\n    useMaxWidth: false\n    useMaxWidth: true\n"),
             probe("config:\n  usecase:\n    diagramPadding: 30\n"),
             probe("theme: forest\n"),
             // A classDef body statement still gates the whole source.
             std::string{"usecase-beta\n  classDef red stroke:#f00\n  actor A\n"},
             // The quoted boundary title stays closed.
             std::string{"usecase-beta\n  systemBoundary \"\"\n  end\n"},
             std::string{"usecase-beta\n  systemBoundary \"A\" junk\n  end\n"},
             // A longer identifier is not the boundary keyword.
             std::string{"usecase-beta\n  systemBoundaryX \"Auth System\"\n  end\n"},
             // The JSON block stays closed.
             std::string{"usecase-beta\n  json P@{\n    \"k\": \"v\"\n"},
             std::string{"usecase-beta\n  json P@{\n    k: v\n  }\n"},
             std::string{"usecase-beta\n  json P@{\n    \"k\": {\"n\": 1}\n  }\n"},
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
