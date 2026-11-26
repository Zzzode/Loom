// RFC 0041 agentflow look/theme admission cohort gate.
//
// Public API only, mirroring tools/mermaid/agentflow-0041-cohort.json against
// the live renderer. Every row's frame must equal its body-identical corpus
// twin's frame plus exactly the literal disclosure footer the ledger pins. The
// closed-table and grammar batteries below are an independent oracle: their
// expected verdicts and footer strings are hardcoded literals, never derived
// from the ledger or the renderer's own table. The last test is the drawn-row
// frame-identity oracle over the family's 27 already-drawn rows.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/agentflow-0041-cohort.json";
const fs::path kFamilyLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/agentflow-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 800;
constexpr std::string_view kHeader = "Agentflow native projection v1";
constexpr std::string_view kBody = "agentflow-beta TB\nA[\"x\"]\n";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};

// Both sides of every comparison go through this same sentinel-bearing
// wrapper: the sentinel is what makes a lone trailing blank row visible in the
// full untrimmed surface.
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
// A native agentflow projection never echoes the fenced source envelope.
// Line equality only; a bare "agentflow" substring is not a valid marker.
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "theme:" || line == "  theme:") return false;
    }
    return got.back() == "EM_SENTINEL";
}
// What the renderer's body helper returns: the source after the closing fence.
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
std::string theme_line(std::string_view value) {
    return "config.theme=" + std::string(value) + " -> colour palette; not applied";
}

// A boundary probe: a description-only envelope plus the given config block.
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  agentflow theme boundary probe\n" + std::string(config) +
           "---\n" + std::string(kBody);
}
std::string config_probe(std::string_view config) {
    return probe(std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
// Compare the literal footer block (blank separator, header, theme line)
// immediately above the fenced block's trailing blank row.
void expect_footer(const std::string& source, std::string_view theme_value,
                   const std::string& label) {
    const std::vector<std::string> footer{"", std::string(kHeader), theme_line(theme_value)};
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), footer.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "EM_SENTINEL") << label;
    ASSERT_TRUE(lines[lines.size() - 2u].empty()) << label << " (expected a trailing blank row)";
    const auto begin = lines.size() - 2u - footer.size();
    for (std::size_t i = 0; i < footer.size(); ++i)
        EXPECT_EQ(lines[begin + i], footer[i]) << label << " footer line " << i;
}
}  // namespace

TEST(MermaidAgentflow0041Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_agentflow_0041_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 41);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 9);
    EXPECT_EQ(counts.get_int("predicted_draw"), 9);
    EXPECT_EQ(counts.get_int("narrow_40_draw"), 0);
    const auto selection = root.get("selection");
    EXPECT_EQ(selection.get_int("family_total"), 38);
    EXPECT_EQ(selection.get_int("native_draw_count"), 27);
    EXPECT_EQ(selection.get_int("admitted_count"), 9);
    EXPECT_EQ(selection.get_int("gated_count"), 0);
    EXPECT_EQ(selection.get_int("delegated_count"), 2);

    // The review verdict is approved only when the enabled phase carries an
    // approved record; while pending the matrix branch must refuse the ledger.
    const auto reviews = root.get("review").get("phases");
    const std::string verdict = root.get("review").get_string("verdict");
    bool all_approved = true;
    for (const auto& phase : {"P1-theme-admission"}) {
        if (!reviews.has(phase)) {
            all_approved = false;
            continue;
        }
        const auto record = reviews.get(phase);
        if (!record.has("verdict") || record.get_string("verdict") != "approved")
            all_approved = false;
        else
            EXPECT_FALSE(record.get_string("reviewer_agent").empty()) << phase;
    }
    EXPECT_EQ(verdict == "approved", all_approved);

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });

    // The family ledger's admitted partition must equal this ledger's row set
    // exactly and stay disjoint from its drawn rows; its digest must agree.
    const auto family = parse_doc(kFamilyLedger);
    std::set<std::string> family_admitted, family_drawn;
    family.root().get("selection").get("admitted").iter(
        [&](JsonVal value) { family_admitted.insert(std::string(value.as_str())); });
    family.root().get("rows").iter(
        [&](JsonVal row) { family_drawn.insert(row.get_string("path")); });
    std::set<std::string> admitted;
    root.get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    EXPECT_EQ(family_admitted, admitted);
    EXPECT_EQ(selection.get_string("admitted_digest"),
              family.root().get("selection").get_string("admitted_digest"));
    for (const auto& rel : admitted) EXPECT_FALSE(family_drawn.contains(rel)) << rel;
    // The two rows RFC 0041 listed as gated are the seed rows; D-442 admits
    // them through the closed inert look + seed pair, so the family ledger now
    // records them as delegated to its reviewed ledger. They draw and disclose
    // under that ledger's own gate (test_mermaid_agentflow_handdrawn_d442_cohort),
    // and the family `gated` list must stay empty.
    int gated = 0;
    family.root().get("selection").get("gated").iter(
        [&](JsonVal) { ++gated; });
    EXPECT_EQ(gated, 0);
    const auto d442 = parse_doc(
        fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/agentflow-handdrawn-d442-cohort.json");
    std::set<std::string> d442_rows;
    d442.root().get("rows").iter([&](JsonVal row) {
        d442_rows.insert(row.get_string("path"));
        EXPECT_EQ(row.get("expected_states").get_string("wide_240"), "drawn")
            << row.get_string("path");
    });
    std::set<std::string> delegated;
    family.root().get("selection").get("delegated").iter(
        [&](JsonVal value) { delegated.insert(std::string(value.as_str())); });
    EXPECT_EQ(delegated, d442_rows);
    for (const auto& rel : delegated) {
        EXPECT_TRUE(read_file(kCases / rel).find("handDrawnSeed") != std::string::npos) << rel;
        EXPECT_TRUE(draws(read_file(kCases / rel), kWide)) << rel;
    }

    int rows = 0;
    std::vector<std::pair<std::string, std::string>> digest_input;
    std::map<std::string, int> twin_counts;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        ++rows;
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        const auto twin = row.get("twin");
        ASSERT_EQ(twin.get_string("kind"), "corpus") << rel;
        const std::string twin_rel = twin.get_string("path");
        ASSERT_NE(twin_rel, rel) << rel;
        ASSERT_TRUE(fs::exists(kCases / twin_rel)) << rel << " -> " << twin_rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / twin_rel)),
                  twin.get_string("sha256"))
            << rel << " twin pin";
        // Corpus twin means body-identical after frontmatter removal.
        EXPECT_EQ(frontmatter_stripped(read_file(kCases / rel)),
                  frontmatter_stripped(read_file(kCases / twin_rel)))
            << rel;
        ++twin_counts[twin_rel];
        const auto states = load_states(row.get("expected_states"));
        EXPECT_EQ(states[0], "drawn") << rel;
        EXPECT_EQ(states[2], "drawn") << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(rows, 9);
    EXPECT_EQ(twin_counts["agentflow/visual-check/10-every-container-kind.mmd"], 5);
    EXPECT_EQ(twin_counts["agentflow/visual-check/15-support-triage.mmd"], 4);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidAgentflow0041Cohort, RowsAreInertAgainstCorpusTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto twin_rel = row.get("twin").get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = read_file(kCases / twin_rel);
        const auto states = load_states(row.get("expected_states"));

        // 1. The corpus twin must itself draw: "both fall back equal" is not
        //    inertness.
        ASSERT_TRUE(draws(twin, kWide)) << twin_rel << " (twin must draw)";
        ASSERT_TRUE(native_frame(twin, kWide)) << twin_rel;

        // 2. The admitted source draws and is a native frame.
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        EXPECT_TRUE(native_frame(source, kWide)) << rel;

        // 3. Per-width ledger states, including the live narrow disposition
        //    (the footer can flip a whole-surface 40-column predicate).
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";

        // 4. Determinism.
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // 5. Frame composition: corpus twin frame + the ledger's exact footer.
        auto expected = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "EM_SENTINEL") << rel;
        expected.pop_back();
        const bool tail_blank = !expected.empty() && expected.back().empty();
        if (tail_blank) expected.pop_back();
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        if (tail_blank) expected.emplace_back();
        expected.emplace_back("EM_SENTINEL");
        const auto got = trimmed_lines(first, kWide);
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        ++checked;
    });
    EXPECT_EQ(checked, 9);
}

TEST(MermaidAgentflow0041Cohort, ThemeTableIsClosed) {
    for (const auto* value : {"base", "dark", "default", "forest", "neutral", "redux-color",
                              "redux-dark-color"}) {
        const auto source = config_probe(std::string("  theme: ") + value + "\n");
        ASSERT_TRUE(draws(source, kWide)) << value;
        ASSERT_TRUE(native_frame(source, kWide)) << value;
        expect_footer(source, value, std::string("theme: ") + value);
    }
    // Sibling palettes, unknown values, a case variant and an empty value.
    for (const auto* value : {"redux", "redux-dark", "neo", "neo-dark", "chartreuse", "Dark", ""}) {
        EXPECT_FALSE(draws(config_probe(std::string("  theme: ") + value + "\n"), kWide)) << value;
    }
    EXPECT_FALSE(draws(config_probe("  theme:\n"), kWide)) << "empty value";
    EXPECT_FALSE(draws(config_probe("  theme: \"dark\n"), kWide)) << "unmatched quote";
    EXPECT_FALSE(draws(config_probe("  theme: dark\n  theme: dark\n"), kWide)) << "duplicate";
    EXPECT_FALSE(draws(config_probe("  theme : dark\n"), kWide)) << "space before colon";
    EXPECT_FALSE(draws(config_probe("    theme: dark\n"), kWide)) << "indent 4";
    EXPECT_FALSE(draws(probe("theme: dark\n"), kWide)) << "indent 0";
    // A quoted scalar and a trailing comment normalize to the same value.
    for (const auto* form : {"  theme: \"dark\"\n", "  theme: dark # note\n"}) {
        const auto source = config_probe(form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        expect_footer(source, "dark", std::string("scalar ") + form);
    }
    // A YAML comment inside the config block is not a leaf.
    const auto commented = config_probe("  # palette\n  theme: dark\n");
    ASSERT_TRUE(draws(commented, kWide));
    expect_footer(commented, "dark", "commented config");
}

// The strict widening is exactly one shape: description-only envelopes and
// indented non-config lines keep today's behavior, while any other config
// shape keeps the whole source.
TEST(MermaidAgentflow0041Cohort, GrammarBattery) {
    // Description-only envelopes draw without a footer (today's contract).
    const auto plain = probe("");
    ASSERT_TRUE(draws(plain, kWide));
    EXPECT_FALSE(has_text(plain, std::string(kHeader)));
    // A folded payload line that looks like a visual key stays documentation.
    const auto payload =
        "---\ndescription: >-\n  theme: dark\n---\n" + std::string(kBody);
    ASSERT_TRUE(draws(payload, kWide));
    EXPECT_FALSE(has_text(payload, std::string(kHeader)));
    // Top-level visual keys and any non-description column-0 key reject.
    EXPECT_FALSE(draws(probe("theme: dark\n"), kWide));
    EXPECT_FALSE(draws(probe("look: handDrawn\n"), kWide));
    EXPECT_FALSE(draws(probe("title: x\n"), kWide));
    EXPECT_FALSE(draws("---\ndescription\n---\n" + std::string(kBody), kWide));
    // A space-before-colon description key is the same key.
    EXPECT_TRUE(draws("---\ndescription : ok\n---\n" + std::string(kBody), kWide));
    // Config shapes other than exactly one admitted leaf reject.
    EXPECT_FALSE(draws(probe("config:\n"), kWide)) << "empty config";
    // D-442 admits the handDrawn look; the negative re-points at a leaf that
    // is still outside the closed table.
    EXPECT_FALSE(draws(config_probe("  fontFamily: courier\n"), kWide)) << "non-visual leaf";
    EXPECT_FALSE(draws(config_probe("  themeVariables:\n    primaryColor: red\n"), kWide))
        << "non-visual map";
    EXPECT_FALSE(draws(config_probe("  theme: dark\n  fontFamily: courier\n"), kWide))
        << "theme + unadmitted leaf";
    EXPECT_FALSE(draws(config_probe("  theme: dark\n  look: neo\n"), kWide))
        << "theme + unadmitted look";
    EXPECT_FALSE(draws(config_probe("  theme: dark\n  handDrawnSeed: 0\n"), kWide))
        << "theme + zero seed";
    EXPECT_FALSE(draws(config_probe("  theme\n"), kWide)) << "colon-less leaf";
    EXPECT_FALSE(draws(config_probe("  theme: dark\nconfig:\n  theme: dark\n"), kWide))
        << "second config with a duplicate theme";
    EXPECT_FALSE(draws(config_probe("  theme: dark\nconfig:\n"), kWide))
        << "second empty config";
    // An admitted leaf plus an in-body themed initializer rejects.
    EXPECT_FALSE(draws("---\ndescription: x\nconfig:\n  theme: dark\n---\n"
                       "%%{init: {\"theme\": \"dark\"}}%%\n" +
                           std::string(kBody),
                       kWide));
}

TEST(MermaidAgentflow0041Cohort, FamilyLedgerPartition) {
    const auto family = parse_doc(kFamilyLedger);
    const auto selection = family.root().get("selection");
    EXPECT_EQ(selection.get_int("family_total"), 38);
    EXPECT_EQ(selection.get_int("native_draw_count"), 27);
    EXPECT_EQ(selection.get_int("admitted_count"), 9);
    EXPECT_EQ(selection.get_int("gated_count"), 0);
    EXPECT_EQ(selection.get_int("delegated_count"), 2);
    // Every row is accounted for exactly once: drawn + admitted + gated +
    // delegated = the family total. D-442 moved the two seed rows from gated
    // to delegated; they are no longer on the family fallback list.
    EXPECT_EQ(selection.get_int("native_draw_count") +
                  selection.get_int("admitted_count") +
                  selection.get_int("gated_count") +
                  selection.get_int("delegated_count"),
              38);
    const auto ledger = parse_doc(kLedger);
    std::set<std::string> admitted;
    ledger.root().get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    std::set<std::string> family_admitted;
    selection.get("admitted").iter(
        [&](JsonVal value) { family_admitted.insert(std::string(value.as_str())); });
    EXPECT_EQ(family_admitted, admitted);
    int rows = 0;
    family.root().get("rows").iter([&](JsonVal row) {
        ++rows;
        EXPECT_FALSE(admitted.contains(row.get_string("path")));
    });
    EXPECT_EQ(rows, 27);
}

// Regression oracle: every row the family already draws must render
// cell-identically with and without its frontmatter. A footer or blank
// separator emitted merely because a frontmatter is present breaks the full
// untrimmed surface equality, and no footer marker may appear.
TEST(MermaidAgentflow0041Cohort, DrawnRowsKeepFrameIdentity) {
    const auto family = parse_doc(kFamilyLedger);
    int checked = 0;
    family.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto stripped = frontmatter_stripped(source);
        ASSERT_TRUE(draws(source, kWide)) << rel << " (drawn row must draw)";
        const auto with = capture(source, "mermaid", kWide);
        const auto without = capture(stripped, "mermaid", kWide);
        ASSERT_EQ(with.chars.size(), without.chars.size()) << rel;
        for (std::size_t i = 0; i < with.chars.size(); ++i)
            ASSERT_EQ(with.chars[i], without.chars[i])
                << rel << " cell " << i << " (frontmatter changed the frame)";
        for (const auto* marker : {"Agentflow native projection v1", "config.theme="})
            EXPECT_EQ(with.text.find(marker), std::string::npos) << rel << " " << marker;
        ++checked;
    });
    EXPECT_EQ(checked, 27);
}
