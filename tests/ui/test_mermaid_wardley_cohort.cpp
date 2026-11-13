// RFC 0031 wardley-family renderer durable cohort gate. Public API only:
// enforces tools/mermaid/wardley-cohort.json against the live renderer and
// the pinned manifest. The six clean wardley originals render natively; the
// four frontmatter theme-gated fixtures stay on source.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/wardley-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nWARDLEY_SENTINEL";
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

TEST(MermaidWardleyCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_wardley_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 6);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 6);
    EXPECT_EQ(root.get("selection").get_int("frontmatter_gated_count"), 4);
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

TEST(MermaidWardleyCohort, CleanWardleysDrawOnWholeAndBodyTracks) {
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

TEST(MermaidWardleyCohort, GatedWardleysStayWholeSource) {
    const auto ledger = parse_doc(kLedger);
    const auto& selection = ledger.root().get("selection");
    for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
        selection.get(key).iter([&](JsonVal value) {
            const auto rel = value.as_str();
            EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
        });
    }
}

// Key-based gate: prose mentioning "theme"/"config" inside a note or title is
// not a gate; a frontmatter visual key or a body style statement closes the
// renderer.
TEST(MermaidWardleyCohort, VisualGateIsKeyBased) {
    const std::string plain =
        "wardley-beta\n"
        "  title config drift and theme fallout\n"
        "  component App [0.5, 0.5]\n"
        "  note \"a themed config note\" [0.2, 0.2]\n";
    EXPECT_TRUE(draws(plain, kWide));

    EXPECT_FALSE(draws(
        "---\nconfig : 42\n---\nwardley-beta\n  component App [0.5, 0.5]\n",
        kWide));
    EXPECT_FALSE(draws("---\ntheme: dark\n---\nwardley-beta\n  component A [0.1, 0.1]\n",
                       kWide));
    EXPECT_FALSE(draws("---\nlook: handDrawn\n---      \nwardley-beta\n  component A [0.1, 0.1]\n",
                       kWide));
    EXPECT_FALSE(draws("wardley-beta\n  component A [0.1, 0.1] :::critical\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  classDef c color:#f00\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  %%{init: {}}%%\n  component A [0.1, 0.1]\n",
                       kWide));
    EXPECT_FALSE(draws("wardley-beta\n  style A fill:#f00\n", kWide));
}

// The grammar is a strict two-space map frame. Every statement type is
// validated and transcribed verbatim with its indentation preserved.
TEST(MermaidWardleyCohort, AcceptedGrammarDrawsAndTranscribes) {
    const std::string source =
        "wardley-beta\n"
        "  title Demo Map\n"
        "  size [1100, 800]\n"
        "  anchor Business [0.95, 0.63]\n"
        "  component Cup of Tea [0.79, 0.61] label [19, -4]\n"
        "  component Kettle [0.43, 0.35]\n"
        "  Business -> Cup of Tea\n"
        "  Cup of Tea +> Kettle; +>\n"
        "  Cup of Tea +<> Kettle; +<>\n"
        "  Kettle +< Business; +<\n"
        "  Kettle -> Business; constraint\n"
        "  Kettle +'backup'> Business\n"
        "  evolve Kettle 0.62\n"
        "  note \"A generic note, with punctuation: yes\" [0.23, 0.33]\n"
        "  evolution Unmodelled -> Divergent -> Convergent -> Modelled\n"
        "  deaccelerator License Play [0.13, 0.78]\n"
        "  annotations [1, 0]\n"
        "  annotation 1,[0.57, 0.16] \"Alternative algos in research\"\n"
        "  pipeline Kettle {\n"
        "  component Electric Kettle [0.53] label [-60, 35]\n"
        "  }\n";
    const CellCapture cap = capture(source, "mermaid", kWide);
    ASSERT_TRUE(draws(source, kWide));
    EXPECT_NE(cap.text.find("Cup of Tea"), std::string::npos);
    EXPECT_NE(cap.text.find("Electric Kettle"), std::string::npos);
    EXPECT_NE(cap.text.find("Alternative algos in research"), std::string::npos);
    EXPECT_NE(cap.text.find("+"), std::string::npos);  // flow operator transcribed
    // The two-space indentation is preserved on the emitted statement rows.
    EXPECT_NE(cap.text.find("  anchor Business"), std::string::npos);

    // A header-only diagram is a valid frame (markdown supplies the newline).
    EXPECT_TRUE(draws("wardley-beta\n", kWide));
    // Blank and whitespace-only lines are skipped, not statements.
    EXPECT_TRUE(draws("wardley-beta\n\n  component A [0.1, 0.1]\n     \n", kWide));
}

// Boundedness and fail-closed discipline across the map statement shapes.
TEST(MermaidWardleyCohort, MalformedInputFailsClosed) {
    EXPECT_FALSE(draws("not-wardley\n  component A [0.1, 0.1]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n\tcomponent A [0.1, 0.1]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  a\rb\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\ncomponent A [0.1, 0.1]\n", kWide));   // indent 0
    EXPECT_FALSE(draws("wardley-beta\n component A [0.1, 0.1]\n", kWide));  // indent 1
    EXPECT_FALSE(draws("wardley-beta\n    component A [0.1, 0.1]\n", kWide));  // indent 4
    // Coordinate shapes: pair outside a pipeline, well-formed decimals only.
    EXPECT_FALSE(draws("wardley-beta\n  component A [0.1]\n", kWide));  // single outside pipeline
    EXPECT_FALSE(draws("wardley-beta\n  anchor A [.5, 0.1]\n", kWide));  // bad decimal
    EXPECT_FALSE(draws("wardley-beta\n  component A [0.1, .2]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  size [abc, 800]\n", kWide));
    // MINOR: a sign is legal only on label offsets, never canvas size or the
    // annotation grid, so negative size/annotation integers fail closed.
    EXPECT_FALSE(draws("wardley-beta\n  size [-1, 800]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  annotations [-1, 0]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  component A [0.1, 0.1] label [x, 2]\n", kWide));
    // Structural characters are not legal in a name.
    EXPECT_FALSE(draws("wardley-beta\n  component A; B [0.1, 0.1]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  component A (unbalanced [0.1, 0.1]\n", kWide));
    // Link operator and semicolon continuation shapes.
    EXPECT_FALSE(draws("wardley-beta\n  A => B\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  A -> B; nonsense\n", kWide));
    // Pipeline blocks must balance and must not nest.
    EXPECT_FALSE(draws("wardley-beta\n  pipeline K {\n  component X [0.5]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  }\n", kWide));
    EXPECT_FALSE(draws(
        "wardley-beta\n  pipeline K {\n  pipeline N {\n  component X [0.5]\n  }\n  }\n",
        kWide));
    EXPECT_FALSE(draws("wardley-beta\n  pipeline K {\n  anchor A [0.1, 0.1]\n  }\n", kWide));
    // evolve/note/annotation shapes.
    EXPECT_FALSE(draws("wardley-beta\n  evolve A\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  note \"x\" [0.1]\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  annotation 1,[0.5, 0.1] text\n", kWide));
    EXPECT_FALSE(draws("wardley-beta\n  evolution Only One Stage\n", kWide));
    // Non-ASCII body text fails closed.
    EXPECT_FALSE(draws("wardley-beta\n  component caf\xc3\xa9 [0.1, 0.1]\n", kWide));
}

// CRLF pairs are accepted line endings; only a bare CR is structural.
TEST(MermaidWardleyCohort, CrlfLineEndingsDraw) {
    const std::string crlf =
        "wardley-beta\r\n"
        "  component App [0.75, 0.75]\r\n"
        "  anchor User [0.95, 0.90]\r\n"
        "  User -> App\r\n";
    EXPECT_TRUE(draws(crlf, kWide));
    const CellCapture cap = capture(crlf, "mermaid", kWide);
    EXPECT_NE(cap.text.find("App"), std::string::npos);
    EXPECT_NE(cap.text.find("User"), std::string::npos);
}
