// D-458 usecase body-grammar cohort gate.
//
// Selection rationale: the last eight usecase fallbacks. Their only
// non-admitted inputs are the closed body styling statements (`classDef`,
// `class`, `style`), one trailing `:::class` annotation on a declaration /
// boundary title / json close, flat `@{ ... }` properties and single-line json
// payloads. The projection transcribes logical statements only: the styling is
// validated, dropped and disclosed, so the style-stripped twin must render
// identical art above the footer.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-body-d458-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "Usecase native projection v1";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP458_SENTINEL";
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
    return got.back() == "P458_SENTINEL";
}
// The art rows above the disclosure footer (or the whole body when there is
// no footer).
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
    while (!art.empty() && (art.back().empty() || art.back() == "P458_SENTINEL"))
        art.pop_back();
    return art;
}
// The same fixture with the styling surface removed: `classDef` / `class` /
// `style` lines are deleted, one trailing `:::class` annotation is cut from
// every line, and unquoted `@{...}` properties are cut (a quoted label that
// contains `@{` is authored text and is kept).
std::string body_styling_stripped(std::string_view source) {
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
        if (content.starts_with("classDef ") ||
            (content.starts_with("class ") && !content.starts_with("classDef ")) ||
            content.starts_with("style ")) {
            if (eol == std::string_view::npos) break;
            continue;
        }
        // Cut one trailing `:::identifier` that sits outside quotes.
        const auto mark = line.rfind(":::");
        if (mark != std::string::npos) {
            bool quoted = false;
            for (std::size_t i = 0; i < mark; ++i)
                if (line[i] == '"') quoted = !quoted;
            if (!quoted) line.erase(mark);
        }
        if (!content.starts_with("json ")) {
            std::string stripped;
            bool in_quote = false;
            std::size_t at = 0;
            while (at < line.size()) {
                const char c = line[at];
                if (c == '"') {
                    in_quote = !in_quote;
                    stripped.push_back(c);
                    ++at;
                    continue;
                }
                if (!in_quote && c == '@' && at + 1u < line.size() && line[at + 1u] == '{') {
                    const auto close = line.find('}', at + 2u);
                    if (close == std::string::npos) break;
                    while (!stripped.empty() && stripped.back() == ' ') stripped.pop_back();
                    at = close + 1u;
                    continue;
                }
                stripped.push_back(c);
                ++at;
            }
            line = std::move(stripped);
        }
        out += line;
        if (eol == std::string_view::npos) break;
        out += '\n';
    }
    return out;
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

TEST(MermaidUsecaseBodyD458Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_usecase_body_d458_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-458");
    EXPECT_EQ(root.get("counts").get_int("total"), 8);
    EXPECT_EQ(root.get("counts").get("by_family").get_int("usecase"), 8);
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
    EXPECT_EQ(digest_input.size(), 8u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidUsecaseBodyD458Cohort, StylingIsInertAgainstStrippedTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = body_styling_stripped(source);
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
        EXPECT_EQ(art_above_footer(capture(source, "mermaid", kWide), kWide),
                  art_above_footer(capture(twin, "mermaid", kWide), kWide))
            << rel << " the styling changed the art";
        ++checked;
    });
    EXPECT_EQ(checked, 8);
}

// The blast radius: four already-drawn usecase rows carry `@{...}` annotations,
// so their transcription and footer changed too. They must still draw and their
// art must equal the quote-aware style-stripped twin.
TEST(MermaidUsecaseBodyD458Cohort, DrawnRowsKeepTheirStrippedArt) {
    int checked = 0;
    for (const auto* rel : {
             "usecase/nightly-check/02-system-boundaries.mmd",
             "usecase/nightly-check/03-actor-variants.mmd",
             "usecase/nightly-check/09-business-and-stereotypes.mmd",
             "usecase/nightly-check/15-animation-variants.mmd",
         }) {
        const auto source = read_file(kCases / rel);
        const auto twin = body_styling_stripped(source);
        EXPECT_NE(twin, source) << rel;
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(draws(twin, kWide)) << rel;
        EXPECT_EQ(art_above_footer(capture(source, "mermaid", kWide), kWide),
                  art_above_footer(capture(twin, "mermaid", kWide), kWide))
            << rel << " the annotations changed the art";
        EXPECT_NE(capture(source, "mermaid", kWide).text.find(
                      "body @{} properties -> shape/type/animation; not applied"),
                  std::string::npos)
            << rel;
        ++checked;
    }
    EXPECT_EQ(checked, 4);
}

TEST(MermaidUsecaseBodyD458Cohort, ClosedNegatives) {
    // Admitted positives: styling statements, a class annotation, an
    // annotation property and an inline json payload.
    {
        const auto source = probe(
            "config:\n  usecase:\n    diagramPadding: 24\n",
            "usecase-beta\n"
            "  actor A:::external\n"
            "  systemBoundary \"S\":::frame\n"
            "    B(\"Use case\")@{ type: package }\n"
            "  end\n"
            "  json P@{ \"k\": \"v\" }:::data\n"
            "  A --> B\n"
            "  classDef default fill:#ffffff,stroke:#333333\n"
            "  classDef external,frame stroke-width:3px\n"
            "  class A,B external,frame\n"
            "  style A stroke:#1e40af\n");
        ASSERT_TRUE(draws(source, kWide));
        ASSERT_TRUE(native_frame(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("body :::class annotations -> colour/line weight; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body @{} properties -> shape/type/animation; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body classDef=default -> colour/line weight; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body classDef=external,frame -> colour/line weight; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body class=A,B -> colour/line weight; not applied"),
                  std::string::npos);
        EXPECT_NE(frame.find("body style=A -> colour/line weight; not applied"),
                  std::string::npos);
        // The logical statements are transcribed without the styling surface.
        EXPECT_NE(frame.find("actor A"), std::string::npos);
        EXPECT_EQ(frame.find(":::external"), std::string::npos);
        EXPECT_EQ(frame.find("classDef default"), std::string::npos);
    }
    {
        // A quoted label that contains `@{...}` is authored text: only an
        // unquoted property block is stripped.
        const auto source =
            "usecase-beta\n  actor A\n  A(\"login @{x}\") --> B(\"go @{z}\")\n"
            "  A --> B\n";
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("login @{x}"), std::string::npos);
        EXPECT_NE(frame.find("go @{z}"), std::string::npos);
        EXPECT_EQ(frame.find("body @{} properties"), std::string::npos);
    }
    // A class annotation on an edge is admitted and dropped (recorded in the
    // ledger as an admitted shape).
    {
        const auto source = "usecase-beta\n  A --> B:::c1\n";
        ASSERT_TRUE(draws(source, kWide));
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("A --> B"),
                  std::string::npos);
    }
    for (const auto& source : {
             // The closed styling property table.
             probe("", "usecase-beta\n  classDef c1 fill:red\n"),
             probe("", "usecase-beta\n  classDef c1 fill:#ff00\n"),
             probe("", "usecase-beta\n  classDef c1 fill:#ffffff,fill:#000000\n"),
             probe("", "usecase-beta\n  classDef c1 stroke-width:3\n"),
             probe("", "usecase-beta\n  classDef c1\n"),
             probe("", "usecase-beta\n  classDef 1c fill:#ffffff\n"),
             probe("", "usecase-beta\n  class A 1class\n"),
             probe("", "usecase-beta\n  class A\n"),
             probe("", "usecase-beta\n  style A fillcolor:#ffffff\n"),
             probe("", "usecase-beta\n  style 1A fill:#ffffff\n"),
             // A property list needs at least one entry and no trailing comma.
             probe("", "usecase-beta\n  actor A@{} \n"),
             probe("", "usecase-beta\n  actor A@{ }\n"),
             probe("", "usecase-beta\n  classDef c fill:#ffffff,\n"),
             probe("", "usecase-beta\n  style A fill:#ffffff,\n"),
             // A class annotation on a styling statement is not a shape.
             probe("", "usecase-beta\n  classDef c fill:#ffffff:::x\n"),
             // One trailing class annotation only.
             probe("", "usecase-beta\n  actor A:::c1:::c2\n"),
             probe("", "usecase-beta\n  actor A:::1c\n"),
             probe("", "usecase-beta\n  actor A:::\n"),
             // A class annotation is a shape annotation, not a line suffix.
             probe("", "usecase-beta\n  direction LR:::c1\n"),
             probe("", "usecase-beta\n  end:::c1\n"),
             probe("", "usecase-beta\n  systemBoundary S\n    A(\"a\")\n  end\n"
                       "  note for A \"x\":::c1\n"),
             // Flat annotations stay closed.
             probe("", "usecase-beta\n  actor A@{ type: hollow\n"),
             probe("", "usecase-beta\n  actor A@{ type: {nested} }\n"),
             // Inline json payloads stay closed.
             probe("", "usecase-beta\n  json P@{ k: v }\n"),
             probe("", "usecase-beta\n  json P@{ \"k\": {\"n\": 1 }\n"),
             probe("", "usecase-beta\n  json P@{ \"k\": \"v\" } junk\n"),
         }) {
        EXPECT_FALSE(draws(source, kWide)) << source;
    }
}
