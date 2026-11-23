// RFC 0031 usecase-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/usecase-cohort.json against the live renderer and
// the pinned manifest. The fourteen clean usecase originals render natively;
// the twelve frontmatter-gated and six body-style-gated fixtures stay source.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <array>
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

const fs::path kCases = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/cases";
const fs::path kManifest = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official/manifest.json";
const fs::path kD456Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-themed-d456-cohort.json";
const fs::path kD447Ledger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-accessibility-d447-cohort.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/usecase-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nUSECASE_SENTINEL";
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

TEST(MermaidUsecaseCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_usecase_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 14);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 14);
    // D-456 moves the nine themed rows into their own decision cohort.
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 3);
    EXPECT_EQ(root.get("selection").get_int("d456_admitted_count"), 9);
    EXPECT_EQ(root.get("selection").get_int("body_style_gated_count"), 6);
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

TEST(MermaidUsecaseCohort, CleanUsecasesDrawOnWholeAndBodyTracks) {
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

TEST(MermaidUsecaseCohort, GatedUsecasesStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    // D-447 admits the closed `config.usecase` namespace + accessibility row;
    // its positive is owned by test_mermaid_usecase_accessibility_d447_cohort.
    std::set<std::string> admitted;
    for (const auto& ledger_path : {kD447Ledger, kD456Ledger}) {
        auto admitted_ledger = json::parse(read_file(ledger_path));
        ASSERT_TRUE(admitted_ledger.has_value()) << "cohort ledger must parse";
        admitted_ledger->root().get("rows").iter(
            [&](const JsonVal row) { admitted.insert(row.get_string("path")); });
    }
    const auto& selection = ledger.root().get("selection");
    int skipped = 0;
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            if (admitted.contains(std::string(rel))) {
                ++skipped;
                return;
            }
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
    EXPECT_EQ(skipped, 1);
    EXPECT_EQ(admitted.size(), 10u);
}

// Key-based gate: prose mentioning "config"/"theme" is not a gate; a
// frontmatter visual key or a body style statement closes the renderer.
TEST(MermaidUsecaseCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "usecase-beta\n  actor Theme[Config actor]\n  Theme --> Manage(\"theme work\")\n";
    EXPECT_TRUE(draws(plain, kWide));

    const std::string with_config =
        "---\ndescription:\n  x\nconfig:\n  usecase:\n    foo: 1\n---\n"
        "usecase-beta\n  A(\"A\")\n";
    EXPECT_FALSE(draws(with_config, kWide));
    EXPECT_FALSE(draws("---\ntheme: dark\n---\nusecase-beta\n  A(\"A\")\n", kWide));

    EXPECT_FALSE(draws("usecase-beta\n  A(\"A\")\n%%{init: {}}%%\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A(\"A\")\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A(\"A\"):::critical\n", kWide));
}

// All grammar shapes: directions, actors with @{...}/icon metadata, use
// cases with parenthesized/bracketed labels, a system boundary, notes, every
// edge connector, a labeled and multi-dash edge, a named edge with its own
// annotation, a multiline quoted label and an anonymous quoted use case.
TEST(MermaidUsecaseCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "usecase-beta\n"
        "  direction LR\n"
        "  actor Customer(\"Customer\")\n"
        "  actor Hollow(\"Hollow\")@{ type: hollow }\n"
        "  actor Icon(\"Icon\")@{ icon: \"fa:user\" }\n"
        "  systemBoundary Storefront\n"
        "    Browse(\"Browse catalog\")\n"
        "    Manage[Manage catalog]\n"
        "  end\n"
        "  \"Anonymous request\"\n"
        "  note for Browse \"Starts the session\"\n"
        "  Customer --> Browse\n"
        "  Browse <-- Customer\n"
        "  Customer -- Manage\n"
        "  Customer --o Browse\n"
        "  Browse o-- Customer\n"
        "  Customer --x Manage\n"
        "  Manage x-- Customer\n"
        "  Customer --|> Browse\n"
        "  Browse ..> : include Manage\n"
        "  Customer -- \"opens form\" --> Manage\n"
        "  Browse ---> Manage\n"
        "  Customer trueEdge@--> Manage\n"
        "  trueEdge@{ animate: true }\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    for (const char* needle :
         {"direction LR", "actor Customer(\"Customer\")",
          "actor Hollow(\"Hollow\")@{ type: hollow }",
          "systemBoundary Storefront", "Manage[Manage catalog]",
          "\"Anonymous request\"", "note for Browse", "Customer --> Browse",
          "Browse <-- Customer", "Customer --o Browse", "Customer --|> Browse",
          "Browse ..> : include Manage",
          "Customer -- \"opens form\" --> Manage", "Browse ---> Manage",
          "Customer trueEdge@--> Manage", "trueEdge@{ animate: true }"}) {
        EXPECT_NE(cap.text.find(needle), std::string::npos) << needle;
    }

    // A physical line break inside a quoted label is one logical statement.
    const std::string multiline =
        "usecase-beta\n  F(\"Line one\n    continues here\")\n  A(\"A\")\n";
    const CellCapture ml = capture(multiline, "mermaid", kWide);
    ASSERT_TRUE(draws(multiline, kWide));
    EXPECT_NE(ml.text.find("Line one"), std::string::npos);
    EXPECT_NE(ml.text.find("continues here"), std::string::npos);

    // RL and BT directions and a stereotype suffix on an actor.
    const std::string dirs =
        "usecase-beta\n  direction BT\n  actor A(\"A\") <<Worker>>\n  B(\"B\")\n  A --> B\n";
    EXPECT_TRUE(draws(dirs, kWide));
}

// Boundedness and fail-closed discipline: wrong header, a bad direction, an
// unclosed quote, an end without a boundary, an unknown statement, a broken
// edge, a structural tab and an unclosed annotation return source.
TEST(MermaidUsecaseCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-usecase\n  A(\"A\")\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  direction SIDEWAYS\n  A(\"A\")\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A(\"unclosed)\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  end\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  unknownStmt x y\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A --> \n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n\tA(\"A\")\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A(\"A\")@{ animate: \n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  systemBoundary X\n  A(\"A\")\n", kWide));  // no end
    // MINOR 1: a quote inside a %% comment is inert and must not swallow the
    // following statements.
    const std::string comment_quote =
        "usecase-beta\n  %% \"open comment\n  A(\"A\")\n  B(\"B\")\n  A --> B\n";
    const CellCapture cq = capture(comment_quote, "mermaid", kWide);
    ASSERT_TRUE(draws(comment_quote, kWide));
    EXPECT_NE(cq.text.find("A --> B"), std::string::npos);
    // MINOR 2: tightened edge whitelist rejects a bare dependency arrow and a
    // stray semicolon (a chained edge is still an accepted verbatim outline).
    EXPECT_FALSE(draws("usecase-beta\n  A ..> B\n", kWide));
    EXPECT_FALSE(draws("usecase-beta\n  A --> B; C\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidUsecaseCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "usecase-beta\r\n  direction LR\r\n  actor A(\"A\")\r\n  A --> B(\"B\")\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("direction LR"), std::string::npos);
    EXPECT_NE(cap.text.find("A --> B"), std::string::npos);
}
