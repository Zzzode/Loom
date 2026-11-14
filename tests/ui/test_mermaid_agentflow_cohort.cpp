// RFC 0031 D-211 agentflow-family cohort gate. Public API only: enforces
// tools/mermaid/agentflow-cohort.json against the live renderer and the
// pinned manifest. All 27 description-only agentflow originals render
// natively as a bounded outline; the 11 config/theme frontmatter originals
// keep their whole-source fallback.
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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/agentflow-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nEM_SENTINEL";
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
    return close == std::string_view::npos ? source : source.substr(close + 5);
}
}  // namespace

TEST(MermaidAgentflowCohort, LedgerSchemaReviewCountsAndManifest) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_agentflow_cohort/v1");
    EXPECT_EQ(root.get("counts").get_int("total"), 27);
    EXPECT_EQ(root.get("counts").get_int("predicted_original_draw"), 27);
    EXPECT_EQ(root.get("selection").get_int("family_total"), 38);
    EXPECT_EQ(root.get("selection").get_int("native_draw_count"), 27);
    EXPECT_EQ(root.get("selection").get_int("gated_count"), 11);
    EXPECT_EQ(root.get("selection").get_int("narrow_40_draw_count"), 19);
    EXPECT_EQ(root.get("selection").get_int("narrow_40_fallback_count"), 8);
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

TEST(MermaidAgentflowCohort, NativeFixturesDrawOnWholeAndBodyTracks) {
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
        const CellCapture cap = capture(whole, "mermaid", kWide);
        EXPECT_TRUE(same_surface(cap, capture(whole, "mermaid", kWide)))
            << rel << " restored determinism";
        ASSERT_FALSE(same_surface(cap, capture(whole, "text", kWide)))
            << rel << " whole wide is a native frame";
        EXPECT_EQ(whole_wide, whole_states[0] == "drawn") << rel << " whole wide ledger";
        EXPECT_EQ(whole_narrow, whole_states[1] == "drawn") << rel << " whole narrow ledger";
        EXPECT_EQ(true, whole_states[2] == "drawn") << rel << " whole restored ledger";

        const bool body_wide = draws(body, kWide);
        const bool body_narrow = draws(body, kNarrow);
        EXPECT_EQ(body_wide, body_states[0] == "drawn") << rel << " body wide ledger";
        EXPECT_EQ(body_narrow, body_states[1] == "drawn") << rel << " body narrow ledger";
        EXPECT_EQ(body_wide, body_states[2] == "drawn") << rel << " body restored ledger";
    });
}

// The renderer transcribes the real grammar: node shapes, the four edge
// markers, container hierarchy and validated metadata, rather than drawing an
// approximate flowchart.
TEST(MermaidAgentflowCohort, GrammarIsTranscribedFaithfully) {
    const std::string source =
        "agentflow-beta TB\n"
        "connector github[\"GitHub API\"]\n"
        "github@{ protocol: \"http\", endpoint: \"https://api.github.com\" }\n"
        "flow triage[\"Triage Agent\"]\n"
        "  ticket[\"ticket\"]@{ shape: input, value: \"INC-1\" }\n"
        "  classify[\"classify\"]@{ shape: tool, connectorRef: \"github.list_issues\" }\n"
        "  severity[\"Severity?\"]@{ shape: decision }\n"
        "  ticket --> classify --> severity\n"
        "end\n"
        "flow audit[\"Audit\"]\n"
        "  record[\"record_outcome\"]@{ shape: task }\n"
        "end\n"
        "audit@{ view: \"collapsed\" }\n"
        "ref[\"Policy\"]@{ shape: refdoc }\n"
        "classify -.- ref\n"
        "severity -- yes --> record\n"
        "severity --x audit\n";
    ASSERT_TRUE(draws(source, kWide));
    const auto cap = capture(source, "mermaid", kWide);
    for (const std::string token :
         {"connector github", "flow triage", "flow audit", "[collapsed]",
          "[input]", "[tool]", "[decision]", "[task]", "[refdoc]",
          "-->", "-.-", "--x", "-- yes -->", "connectorRef", "INC-1"})
        EXPECT_NE(cap.text.find(token), std::string::npos) << token;
}

// Strict fail-closed discipline: anything outside the closed grammar rejects
// the whole source instead of producing a half-rendered or approximate graph.
TEST(MermaidAgentflowCohort, MalformedInputFailsClosed) {
    const std::string base =
        "agentflow-beta TB\n"
        "A[\"A\"]@{ shape: input }\n"
        "B[\"B\"]@{ shape: tool }\n"
        "A --> B\n";
    // Header and direction.
    EXPECT_FALSE(draws("not-agentflow\nA[\"x\"]\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta XX\nA[\"x\"]\n", kWide));
    // Unknown node shape and metadata key.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ shape: widget }\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ bogus: 1 }\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ shape: }\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ view: \"open\" }\n", kWide));
    // Unresolved edge endpoint; bare id must be declared first.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]\nA --> ghost\n", kWide));
    // Unknown arrow token.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]\nB[\"y\"]\nA ==> B\n", kWide));
    // Container balance and indentation.
    EXPECT_FALSE(draws("agentflow-beta TB\nflow f[\"F\"]\n  A[\"x\"]\n", kWide));  // no end
    EXPECT_FALSE(draws("agentflow-beta TB\nend\nA[\"x\"]\n", kWide));               // stray end
    EXPECT_FALSE(draws("agentflow-beta TB\n A[\"x\"]\n", kWide));   // odd indent
    EXPECT_FALSE(draws("agentflow-beta TB\n\tA[\"x\"]\n", kWide));  // tab
    EXPECT_FALSE(draws("agentflow-beta TB\r\nA[\"x\"]\n", kWide));  // CR
    // Inline redeclaration with a conflicting label.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"one\"]\nA[\"two\"] --> B[\"b\"]\n", kWide));
    // Control byte in a quoted label fails closed.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\x01y\"]\n", kWide));
    // connector cannot be nested in a flow.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nflow f[\"F\"]\n  connector g[\"G\"]\nend\n", kWide));
    // M1 regression: a duplicate key across separate metadata blocks rejects.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nA[\"x\"]@{ shape: input }\nA@{ shape: task }\n", kWide));
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nA[\"x\"]@{ value: \"a\" }\nA@{ value: \"b\" }\n", kWide));
    // A duplicate key inside one block still rejects.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nA[\"x\"]@{ shape: input, shape: tool }\n", kWide));
    // Unquoted value where a quoted scalar is required; non-bool boolean.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ value: bare }\n", kWide));
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nconnector g[\"G\"]\ng@{ token_required: yes }\n", kWide));
    // Duplicate element id; metadata attached to a nonexistent owner.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]\nA[\"y\"]\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta TB\nghost@{ shape: input }\n", kWide));
    // Unterminated quoted value; a comma inside a quote does not split, but a
    // dangling quote fails closed.
    EXPECT_FALSE(draws("agentflow-beta TB\nA[\"x\"]@{ value: \"open }\n", kWide));
    // An edge cannot terminate at a connector (it binds via connectorRef).
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nconnector g[\"G\"]\nA[\"x\"]\nA --> g\n", kWide));
    // A connector/flow needs a non-empty label.
    EXPECT_FALSE(draws("agentflow-beta TB\nconnector g\n", kWide));
    EXPECT_FALSE(draws("agentflow-beta TB\nflow f[\"\"]\nend\n", kWide));
    // shape is node-only; attaching it to a flow or connector is rejected.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nflow f[\"F\"]\n  A[\"x\"]\nend\nf@{ shape: task }\n", kWide));
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nconnector g[\"G\"]\ng@{ shape: tool }\n", kWide));
    // An implicit dedent cannot close a container; an explicit end is required.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\nflow f[\"F\"]\n  A[\"x\"]\nB[\"y\"]\n", kWide));
    // An engine init directive is an unsupported visual contract, not a comment.
    EXPECT_FALSE(draws(
        "agentflow-beta TB\n%%{init: {\"theme\":\"dark\"}}%%\nA[\"x\"]\n", kWide));
    // A comma inside a quoted metadata value is legal and still draws.
    EXPECT_TRUE(draws(
        "agentflow-beta TB\nA[\"x\"]@{ description: \"a, b, c\" }\n", kWide));
    // A column-0 frontmatter line without a colon, and a space-before-colon
    // description key, follow the envelope semantics.
    EXPECT_FALSE(draws(
        "---\ndescription\n---\nagentflow-beta TB\nA[\"x\"]\n", kWide));
    EXPECT_TRUE(draws(
        "---\ndescription : ok\n---\nagentflow-beta TB\nA[\"x\"]\n", kWide));
}

// The frozen frontmatter gate is untouched: config/theme/look keys keep the
// whole source on fallback regardless of a valid body.
TEST(MermaidAgentflowCohort, FrontmatterGatesStayClosed) {
    const std::array<std::string, 4> gated{{
        "agentflow/pr-check/11-should-render-every-container-kind-with-the-base-theme.mmd",
        "agentflow/visual-check/11-should-render-every-container-kind-with-the-forest-theme.mmd",
        "agentflow/visual-check/15-support-triage-default-theme.mmd",
        "agentflow/pr-check/should-render-several-collapsed-containers.mmd",
    }};
    for (const auto& rel : gated) {
        const auto source = read_file(kCases / rel);
        EXPECT_FALSE(draws(source, kWide)) << rel;
        EXPECT_FALSE(draws(source, kNarrow)) << rel;
    }
    // A synthetic config gate fails closed even with a clean body.
    EXPECT_FALSE(draws(
        "---\ndescription: x\nconfig:\n  theme: dark\n---\nagentflow-beta TB\nA[\"x\"]\n",
        kWide));
}
