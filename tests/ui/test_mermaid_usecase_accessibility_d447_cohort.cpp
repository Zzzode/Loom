// D-447 usecase accessibility + closed-namespace admission gate.
//
// Selection rationale: the last usecase fallback carries only the closed
// `config.usecase` tuning namespace and in-body `accTitle:`/block-form
// `accDescr { ... }` accessibility declarations. The projection is a statement
// outline, so the tuning leaves are inert (disclosed) and the accessibility
// declarations are validated no-ops, like the state family's.
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
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-accessibility-d447-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nP447_SENTINEL";
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
        if (line.starts_with("accTitle") || line.starts_with("accDescr")) return false;
        if (line.starts_with("    actorFontSize") || line.starts_with("  usecase"))
            return false;
    }
    return got.back() == "P447_SENTINEL";
}
// The same fixture minus the config block and the accessibility declarations.
std::string acc_free_twin(const std::string& source) {
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
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
    std::string body = source.substr(close + 5);
    std::string out_body;
    bool in_descr = false;
    for (std::size_t pos = 0; pos <= body.size();) {
        const auto eol = body.find('\n', pos);
        const auto end = eol == std::string::npos ? body.size() : eol;
        const std::string line = body.substr(pos, end - pos);
        std::size_t first = line.find_first_not_of(' ');
        if (first == std::string::npos) first = 0;
        const std::string stripped = line.substr(first);
        bool drop = false;
        if (in_descr) {
            if (stripped == "}") in_descr = false;
            drop = true;
        } else if (stripped.starts_with("accTitle")) {
            drop = true;
        } else if (stripped.starts_with("accDescr")) {
            if (stripped == "accDescr {") in_descr = true;
            drop = true;
        }
        if (!drop) {
            out_body += line;
            if (eol != std::string::npos) out_body += '\n';
        }
        if (eol == std::string::npos) break;
        pos = eol + 1;
    }
    return "---\n" + kept + "---\n" + out_body;
}
constexpr std::string_view kBody =
    "usecase-beta\n"
    "  direction LR\n"
    "  actor Customer(\"Customer\")\n"
    "  Browse(\"Browse catalog\")\n"
    "  Customer --> Browse\n";
std::string probe(std::string_view frontmatter_tail, std::string_view body) {
    return "---\ndescription: >-\n  usecase probe\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
}  // namespace

TEST(MermaidUsecaseAccessibilityD447Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_usecase_accessibility_d447_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-447");
    EXPECT_EQ(root.get("counts").get_int("total"), 1);
    EXPECT_EQ(root.get("review").get_string("verdict"), "approved");
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              "dcca77031cdb17b23ad77495c126ee3baf20da089dc9b2643006def3b98748b6");
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

TEST(MermaidUsecaseAccessibilityD447Cohort, LeavesAndAccAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = acc_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel << " narrow stays source";
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        std::vector<std::string> expected{""};
        row.get("expected_footer_lines").iter([&](JsonVal line) {
            if (expected.size() > 1u || !line.as_str().empty())
                expected.emplace_back(line.as_str());
        });
        ASSERT_GE(lines.size(), expected.size() + 2u) << rel;
        const auto begin = lines.size() - 2u - expected.size();
        for (std::size_t i = 0; i < expected.size(); ++i)
            EXPECT_EQ(lines[begin + i], expected[i]) << rel << " footer line " << i;
        std::vector<std::string> art(lines.begin(),
                                     lines.begin() + static_cast<std::ptrdiff_t>(begin));
        while (!art.empty() && art.back().empty()) art.pop_back();
        auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        while (!twin_lines.empty() &&
               (twin_lines.back().empty() || twin_lines.back() == "P447_SENTINEL"))
            twin_lines.pop_back();
        EXPECT_EQ(art, twin_lines) << rel << " the leaves changed the outline";
        ++checked;
    });
    EXPECT_EQ(checked, 1);
}

TEST(MermaidUsecaseAccessibilityD447Cohort, ClosedNamespaceAndAccNegatives) {
    // Scalar accTitle/accDescr and block accDescr draw without any footer.
    {
        const auto source = probe("",
                                  "usecase-beta\n  direction LR\n  accTitle: Probe\n"
                                  "  accDescr {\n    A probe description.\n  }\n"
                                  "  actor A(\"A\")\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Usecase native projection v1"), std::string::npos);
    }
    // The canonical quoted spellings are accepted too: the value is read from
    // the raw statement, not from the quote-blanked shape view.
    {
        const auto source = probe("",
                                  "usecase-beta\n  direction LR\n  accTitle: \"A probe title\"\n"
                                  "  accDescr: \"A probe description.\"\n  actor A(\"A\")\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Usecase native projection v1"), std::string::npos);
    }
    // The admitted namespace draws and discloses every leaf in table order.
    {
        const auto source = probe(
            "config:\n  usecase:\n    actorFontSize: 16\n    useMaxWidth: false\n",
            std::string{kBody});
        ASSERT_TRUE(draws(source, kWide));
        const auto frame = capture(source, "mermaid", kWide).text;
        EXPECT_NE(frame.find("Usecase native projection v1"), std::string::npos);
        EXPECT_NE(frame.find("config.usecase.actorFontSize=16 -> SVG layout tuning; not applied"),
                  std::string::npos);
        EXPECT_LT(frame.find("config.usecase.actorFontSize=16"),
                  frame.find("config.usecase.useMaxWidth=false"));
    }
    for (const auto* tail : {
             // Closed leaf table.
             "config:\n  usecase:\n    actorFontSize: 17\n",
             "config:\n  usecase:\n    actorFontFamily: Inter\n",
             "config:\n  usecase:\n    useMaxWidth: true\n",
             "config:\n  usecase:\n    diagramPadding: \"24\"\n",
             "config:\n  usecase:\n    actorFontSize: 16\n    actorFontSize: 16\n",
             "config:\n  usecase:\n",
             "config:\n",
             "config:\n  theme: dark\n",
             "config:\n  look: handDrawn\n",
             "config:\n  usecase:\n    actorFontSize: 16\n  theme: dark\n",
             "config:\n    usecase:\n      actorFontSize: 16\n",
         }) {
        const auto source = probe(tail, std::string{kBody});
        EXPECT_FALSE(draws(source, kWide)) << tail;
        EXPECT_FALSE(native_frame(source, kWide)) << tail;
    }
    for (const auto* body : {
             // Accessibility no-op shape rules.
             "usecase-beta\n  accTitle: Probe\n  accTitle: Probe\n  actor A(\"A\")\n",
             "usecase-beta\n  accDescr: \n  actor A(\"A\")\n",
             "usecase-beta\n  accDescr {\n    open never closed\n  actor A(\"A\")\n",
             "usecase-beta\n  accTitle Probe\n  actor A(\"A\")\n",
         }) {
        const auto source = probe("", body);
        EXPECT_FALSE(draws(source, kWide)) << body;
    }
    // A config-free, acc-free source keeps no footer.
    {
        const auto frame = capture(probe("", std::string{kBody}), "mermaid", kWide).text;
        EXPECT_EQ(frame.find("Usecase native projection v1"), std::string::npos);
    }
}
