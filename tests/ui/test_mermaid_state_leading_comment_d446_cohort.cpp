// D-446 state leading-comment admission gate.
//
// Selection rationale: the last state fallback whose only remaining blocker is
// a body that opens with `%%` comment lines before `stateDiagram-v2`. The body
// parser always ignored those lines; the envelope header check was the only
// rejection, and it now skips leading comments (Work-charged).
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-leading-comment-d446-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP446_SENTINEL";
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
        if (line.starts_with("%%")) return false;
    }
    return got.back() == "P446_SENTINEL";
}
// The same fixture with the leading body comment lines removed.
std::string comment_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string head = source.substr(0, close + 5);
    std::string body = source.substr(close + 5);
    while (body.starts_with("%%")) {
        const auto eol = body.find('\n');
        if (eol == std::string::npos) {
            body.clear();
            break;
        }
        body.erase(0, eol + 1);
    }
    return head + body;
}
constexpr std::string_view kBody =
    "stateDiagram-v2\n  State1 --> State2 : go\n  State2 --> [*]\n";
std::string probe(std::string_view comments, std::string_view config) {
    return "---\ndescription: >-\n  state comment probe\n" + std::string(config) + "---\n" +
           std::string(comments) + std::string(kBody);
}
}  // namespace

TEST(MermaidStateLeadingCommentD446Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"),
              "loom_mermaid_state_leading_comment_d446_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-446");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
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
    EXPECT_EQ(digest_input.size(), 1u);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidStateLeadingCommentD446Cohort, CommentsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = comment_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel << " narrow stays source";
        // The twin keeps the admitted config, so its footer is identical; the
        // comment lines must therefore change nothing at all, footer included.
        EXPECT_TRUE(same_surface(capture(source, "mermaid", kWide),
                                 capture(twin, "mermaid", kWide)))
            << rel << " the comment lines changed the frame";
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidStateLeadingCommentD446Cohort, OnlyLeadingCommentsAreSkipped) {
    // Admitted: leading `%%` lines under an admitted config leaf.
    {
        const auto source =
            probe("%% leading comment\n", "config:\n  layout: elk\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("config.layout=elk -> external layout engine; not applied"),
                  std::string::npos);
    }
    // An indented comment before the header is not leading and still rejects;
    // so does a non-comment line, a body with no header, and a bare comment
    // body under no config (which keeps no footer).
    for (const auto* comments : {"  %% indented comment\n", "%%!note\nstateDiagram\n",
                                 "%%comment\nnotAHeader\n"}) {
        const auto source = probe(comments, "config:\n  layout: elk\n");
        EXPECT_FALSE(draws(source, kWide)) << comments;
        EXPECT_FALSE(native_frame(source, kWide)) << comments;
    }
    // After the leading comments the header must still be stateDiagram.
    EXPECT_FALSE(draws(probe("%% c\n", "config:\n  layout: elk\n") +
                           "graph TD\n  A --> B\n",
                       kWide));
    // A comment-free source keeps no disclosure footer.
    {
        const auto frame = capture(std::string{kBody}, "mermaid", kWide).text;
        EXPECT_EQ(frame.find("State native projection v1"), std::string::npos);
    }
}
