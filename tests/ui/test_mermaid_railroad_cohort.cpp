// RFC 0031 railroad-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/railroad-cohort.json against the live renderer and
// the pinned manifest. The eight clean railroad originals (four dialects)
// render natively; the one frontmatter theme-gated fixture stays on source.
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

const fs::path kCases = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases";
const fs::path kManifest = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/railroad-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1600;

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nRAILROAD_SENTINEL";
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
std::string body_of(const std::string& source) {
    if (source.rfind("---\n", 0) != 0) return source;
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidRailroadCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_railroad_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 8);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 8);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 1);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 0);
    ASSERT_EQ(root.get("review").get_string("verdict"), "approved");

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
    });
}

TEST(MermaidRailroadCohort, CleanRailroadsDrawOnWholeAndBodyTracks) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto whole = read_file(kCases / rel);
        const auto body = body_of(whole);
        const auto whole_states = load_states(row.get("expected_states"));
        const auto body_states = load_states(row.get("expected_body_states"));

        const bool whole_wide = draws(whole, kWide);
        const bool whole_narrow = draws(whole, kNarrow);
        RecordProperty(rel + " narrow", whole_narrow);

        EXPECT_TRUE(whole_wide) << rel << " whole wide";
        const CellCapture whole_wide_cap = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(whole_wide_cap, capture(whole, "mermaid", kWide)))
            << rel << " whole restored determinism";
        ASSERT_FALSE(same_surface(whole_wide_cap, capture(whole, "text", kWide)))
            << rel << " whole wide is a native frame";
        EXPECT_EQ(whole_wide, whole_states[0] == "drawn") << rel << " whole wide ledger";
        EXPECT_EQ(whole_narrow, whole_states[1] == "drawn") << rel << " whole narrow ledger";
        EXPECT_EQ(true, whole_states[2] == "drawn") << rel << " whole restored ledger";

        const bool body_wide = draws(body, kWide);
        const bool body_narrow = draws(body, kNarrow);
        const CellCapture body_wide_cap = capture(body, "mermaid", kWide);
        EXPECT_TRUE(same_surface(body_wide_cap, capture(body, "mermaid", kWide)))
            << rel << " body restored determinism";
        EXPECT_EQ(body_wide, body_states[0] == "drawn") << rel << " body wide ledger";
        EXPECT_EQ(body_narrow, body_states[1] == "drawn") << rel << " body narrow ledger";
        EXPECT_EQ(body_wide, body_states[2] == "drawn") << rel << " body restored ledger";
    });
}

TEST(MermaidRailroadCohort, GatedRailroadStaysWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

TEST(MermaidRailroadCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "railroad-beta\n"
        "digit = terminal(\"theme config style\") ;\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\nrailroad-beta\ndigit = terminal(\"0\") ;\n", kWide));
    EXPECT_FALSE(draws(
        "---\ntheme: dark\n---\nrailroad-beta\ndigit = terminal(\"0\") ;\n", kWide));
    EXPECT_FALSE(draws(
        "---\nlook: handDrawn\n---      \nrailroad-beta\ndigit = terminal(\"0\") ;\n",
        kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\") :::critical ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\nclassDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\n%%{init: {}}%%\ndigit = terminal(\"0\") ;\n",
                       kWide));
    EXPECT_FALSE(draws("railroad-beta\nstyle digit fill:#f00\n", kWide));
}

// All four dialects' real notation shapes are accepted and transcribed.
TEST(MermaidRailroadCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string ir =
        "railroad-beta\n"
        "expression = sequence(\n"
        "    nonterminal(\"term\"),\n"
        "    zeroOrMore(choice(\n"
        "        sequence(terminal(\"+\"), nonterminal(\"term\")),\n"
        "        sequence(terminal(\"-\"), nonterminal(\"term\"))\n"
        "    ))\n"
        ") ;\n"
        "digit = choice(terminal(\"0\"), terminal(\"1\")) ;\n";
    const CellCapture ir_cap = capture(ir, "mermaid", kWide);
    ASSERT_TRUE(draws(ir, kWide));
    EXPECT_NE(ir_cap.text.find("nonterminal"), std::string::npos);
    EXPECT_NE(ir_cap.text.find("zeroOrMore"), std::string::npos);
    // Continuation indentation is preserved verbatim.
    EXPECT_NE(ir_cap.text.find("    nonterminal(\"term\")"), std::string::npos);

    EXPECT_TRUE(draws(
        "railroad-abnf-beta\n"
        "scheme = ALPHA *( ALPHA / DIGIT / \"+\" ) ;\n", kWide));
    EXPECT_TRUE(draws(
        "railroad-ebnf-beta\n"
        "identifier = letter , { letter | digit | \"_\" } ;\n"
        "number = digit+ ;\n", kWide));
    EXPECT_TRUE(draws(
        "railroad-peg-beta\n"
        "Expression <- Term ((\"+\" / \"-\") Term)* ;\n", kWide));

    // A header-only frame draws (markdown supplies the trailing newline).
    EXPECT_TRUE(draws("railroad-beta\n", kWide));
    // Blank lines are skipped.
    EXPECT_TRUE(draws("railroad-beta\n\ndigit = terminal(\"0\") ;\n", kWide));
}

// Boundedness and fail-closed discipline across the four grammars.
TEST(MermaidRailroadCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-railroad\ndigit = terminal(\"0\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\n\tdigit = terminal(\"0\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\") a\rb ;\n", kWide));

    // IR: unknown function, unbalanced parens, bad arg, missing terminator.
    EXPECT_FALSE(draws("railroad-beta\ndigit = frobnicate(\"0\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\" ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = choice(terminal(\"0\"),) ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(0) ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\")\n", kWide));  // no ';'
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\") ; extra ;\n", kWide));
    // A continuation line is legal only inside an open IR rule.
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\") ;\n    orphan\n", kWide));
    // A text dialect rejects indented continuation lines.
    EXPECT_FALSE(draws("railroad-peg-beta\nExpression <- Term\n    orphan ;\n", kWide));

    // ABNF: a bare '*' must prefix a group; alternation uses '/'.
    EXPECT_FALSE(draws("railroad-abnf-beta\nscheme = * ALPHA ;\n", kWide));
    EXPECT_FALSE(draws("railroad-abnf-beta\ndigit = \"0\" | \"1\" ;\n", kWide));
    // EBNF: braces only here, dangling comma rejected.
    EXPECT_FALSE(draws("railroad-ebnf-beta\nx = a , ;\n", kWide));
    EXPECT_FALSE(draws("railroad-ebnf-beta\nx = { a ;\n", kWide));
    EXPECT_FALSE(draws("railroad-ebnf-beta\nx = a / b ;\n", kWide));  // '/' is not EBNF choice
    // PEG: the '<-' arrow is mandatory, '=' is not.
    EXPECT_FALSE(draws("railroad-peg-beta\nDigit = \"0\" ;\n", kWide));
    EXPECT_FALSE(draws("railroad-peg-beta\nDigit <- \"0\" | \"1\" ;\n", kWide));  // '|' not PEG

    // An unterminated rule at EOF fails closed.
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"0\")", kWide));
    // Non-ASCII and C0 control bytes inside a string literal fail closed.
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"caf\xc3\xa9\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"a\x00" "b\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"a\x1b" "b\") ;\n", kWide));
    EXPECT_FALSE(draws("railroad-beta\ndigit = terminal(\"a\x7f" "b\") ;\n", kWide));
}

TEST(MermaidRailroadCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "railroad-beta\r\n"
        "digit = choice(terminal(\"0\"), terminal(\"1\")) ;\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("terminal"), std::string::npos);
}
