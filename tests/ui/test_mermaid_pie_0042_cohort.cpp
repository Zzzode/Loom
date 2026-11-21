// RFC 0042 pie look/theme admission cohort gate.
//
// Public API only, mirroring tools/mermaid/pie-0042-cohort.json against the
// live renderer. Every row's frame must equal its twin's frame (a body-identical
// drawn Pie7 fixture for the `dark` row, the config-free synthetic twin for the
// four twelve-slice redux rows) plus exactly the literal disclosure footer the
// ledger pins, at wide width. The batteries below are an independent oracle:
// their expected verdicts and footer strings are hardcoded literals, never
// derived from the ledger or the renderer's table.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/pie-0042-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 1600;
constexpr std::string_view kHeader = "Pie native projection v1";

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct CellCapture {
    std::vector<std::string> chars;
    std::string text;
};

// Both sides of every comparison go through this same sentinel-bearing
// wrapper: the sentinel makes a lone trailing blank row visible.
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
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "  theme:") return false;
    }
    return got.back() == "EM_SENTINEL";
}
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
std::string theme_line(std::string_view value) {
    return "config.theme=" + std::string(value) + " -> colour palette; not applied";
}

constexpr std::string_view kBody = "pie title Probe\n  \"Alpha\": 20\n  \"Bravo\": 16\n";
// A boundary probe: the reviewed folded description plus the given config.
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  pie theme boundary probe\n" + std::string(config) +
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
// The seven reviewed Pie7 whole-scene paths (hardcoded in the matrix test).
const std::set<std::string> kPie7 = {
    "pie/nightly-check/should-render-a-simple-pie-diagram-with-long-labels.mmd",
    "pie/nightly-check/should-render-pie-slices-only-for-non-zero-values-but-shows-all-legends.mmd",
    "pie/pr-check/should-render-a-pie-diagram-with-a-long-title-without-clipping.mmd",
    "pie/pr-check/should-render-a-pie-diagram-with-showdata.mmd",
    "pie/pr-check/should-render-a-simple-pie-diagram-with-capital-letters-for-labels.mmd",
    "pie/pr-check/should-render-a-simple-pie-diagram.mmd",
    "pie/pr-check/simple-sports.mmd",
};
}  // namespace

TEST(MermaidPie0042Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_pie_0042_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 42);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 5);
    EXPECT_EQ(counts.get_int("predicted_draw"), 5);
    EXPECT_EQ(counts.get_int("narrow_40_draw"), 0);
    const auto selection = root.get("selection");
    EXPECT_EQ(selection.get_int("family_total"), 24);
    EXPECT_EQ(selection.get_int("native_draw_count"), 7);
    EXPECT_EQ(selection.get_int("admitted_count"), 5);
    EXPECT_EQ(selection.get_int("other_count"), 12);

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

    int rows = 0, corpus_twins = 0, synthetic_twins = 0;
    std::vector<std::pair<std::string, std::string>> digest_input;
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
        const std::string kind = twin.get_string("kind");
        if (kind == "corpus") {
            ++corpus_twins;
            const std::string twin_rel = twin.get_string("path");
            ASSERT_NE(twin_rel, rel) << rel;
            ASSERT_TRUE(kPie7.contains(twin_rel)) << twin_rel;
            EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / twin_rel)),
                      twin.get_string("sha256"))
                << rel << " twin pin";
            EXPECT_EQ(frontmatter_stripped(read_file(kCases / rel)),
                      frontmatter_stripped(read_file(kCases / twin_rel)))
                << rel;
        } else {
            EXPECT_EQ(kind, "synthetic") << rel;
            ++synthetic_twins;
        }
        const auto states = load_states(row.get("expected_states"));
        EXPECT_EQ(states[0], "drawn") << rel;
        EXPECT_EQ(states[2], "drawn") << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(rows, 5);
    EXPECT_EQ(corpus_twins, 1);
    EXPECT_EQ(synthetic_twins, 4);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));
}

TEST(MermaidPie0042Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin_info = row.get("twin");
        std::string twin;
        if (twin_info.get_string("kind") == "corpus") {
            twin = read_file(kCases / twin_info.get_string("path"));
        } else {
            twin = frontmatter_stripped(source);
            EXPECT_LT(twin.size(), source.size()) << rel;
        }
        const auto states = load_states(row.get("expected_states"));

        // 1. The twin must itself draw.
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(native_frame(twin, kWide)) << rel;

        // 2. The admitted source draws and is a native frame.
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        EXPECT_TRUE(native_frame(source, kWide)) << rel;

        // 3. Per-width ledger states; narrow is live (the footer changes the
        //    whole-surface predicate).
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";

        // 4. Determinism.
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // 5. Wide-only frame composition: twin frame + the ledger's footer.
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
    EXPECT_EQ(checked, 5);
}

TEST(MermaidPie0042Cohort, ThemeTableIsClosed) {
    for (const auto* value : {"dark", "redux", "redux-dark", "redux-color", "redux-dark-color"}) {
        const auto source = config_probe(std::string("  theme: ") + value + "\n");
        ASSERT_TRUE(draws(source, kWide)) << value;
        ASSERT_TRUE(native_frame(source, kWide)) << value;
        expect_footer(source, value, std::string("theme: ") + value);
    }
    // Out-of-table, sibling-blocked, case and empty values reject.
    for (const auto* value : {"default", "forest", "neutral", "neo", "base", "chartreuse", "Dark",
                              ""}) {
        EXPECT_FALSE(draws(config_probe(std::string("  theme: ") + value + "\n"), kWide)) << value;
    }
    for (const auto* config :
         {"  theme: \"redux\"\n", "  theme: redux # palette\n", "  theme: \"redux\n",
          "  theme: redux\n  theme: redux\n", "  theme : redux\n", "  theme:    redux\n",
          "  theme:\n", "  theme:\n  pie:\n    textPosition: 0.9\n"}) {
        EXPECT_FALSE(draws(config_probe(config), kWide)) << config;
    }
    EXPECT_FALSE(draws(config_probe("    theme: redux\n"), kWide)) << "indent 4";
    EXPECT_FALSE(draws(config_probe("  # palette\n  theme: redux\n"), kWide))
        << "indented comment in config";
    // theme plus any other leaf, or a second config block, rejects.
    EXPECT_FALSE(draws(config_probe("  theme: redux\n  pie:\n    textPosition: 0.9\n"), kWide))
        << "theme + pie leaf";
    EXPECT_FALSE(draws(config_probe("  theme: redux\nconfig:\n  theme: redux\n"), kWide))
        << "second config";
}

TEST(MermaidPie0042Cohort, GrammarBattery) {
    // A description-only pie keeps drawing with no footer (today's contract).
    const auto plain = probe("");
    ASSERT_TRUE(draws(plain, kWide));
    EXPECT_FALSE(has_text(plain, std::string(kHeader)));
    // A payload line that looks like a visual key is documentation.
    const auto payload = "---\ndescription: >-\n  theme: redux\n---\n" + std::string(kBody);
    ASSERT_TRUE(draws(payload, kWide));
    EXPECT_FALSE(has_text(payload, std::string(kHeader)));
    // A blank payload line and an indented comment inside the description
    // reject (the stricter PieTheme rule).
    EXPECT_FALSE(draws("---\ndescription: >-\n  one\n\n  two\nconfig:\n  theme: redux\n---\n" +
                           std::string(kBody),
                       kWide))
        << "blank payload line";
    EXPECT_FALSE(draws("---\ndescription: >-\n  one\n  # note\nconfig:\n  theme: redux\n---\n" +
                           std::string(kBody),
                       kWide))
        << "indented comment in payload";
    // A theme leaf without the folded description keeps today's rejection;
    // the in-table value additionally pins the body-family guard.
    EXPECT_FALSE(draws("---\nconfig:\n  theme: redux\n---\n" + std::string(kBody), kWide));
    EXPECT_FALSE(draws("---\nconfig:\n  theme: dark\n---\npie title X\n", kWide));
    // An admitted leaf plus an in-body initializer rejects.
    EXPECT_FALSE(draws("---\ndescription: >-\n  x\nconfig:\n  theme: redux\n---\n"
                       "%%{init: {\"theme\": \"redux\"}}%%\n" +
                           std::string(kBody),
                       kWide));
    // The four body-blocked theme rows stay whole source: dark is in-table and
    // excluded only by its accTitle:/accDescr: body; the rest also by value.
    for (const auto* rel :
         {"pie/nightly-check/dark-theme.mmd", "pie/nightly-check/default-theme.mmd",
          "pie/nightly-check/forest-theme.mmd", "pie/nightly-check/neutral-theme.mmd"}) {
        EXPECT_FALSE(draws(read_file(kCases / rel), kWide)) << rel;
    }
    // The degenerate title-only chart (no slices) still discloses.
    const auto title_only =
        "---\ndescription: >-\n  x\nconfig:\n  theme: dark\n---\npie title X\n";
    ASSERT_TRUE(draws(title_only, kWide));
    expect_footer(title_only, "dark", "title-only degeneracy");
}

TEST(MermaidPie0042Cohort, FamilyPartition) {
    EXPECT_EQ(kPie7.size(), 7u);
    const auto ledger = parse_doc(kLedger);
    std::set<std::string> admitted;
    ledger.root().get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    EXPECT_EQ(admitted.size(), 5u);
    for (const auto& rel : admitted) EXPECT_FALSE(kPie7.contains(rel)) << rel;
    // 7 drawn + 5 admitted + 12 other = 24 originals.
    std::size_t originals = 0;
    for (auto it = fs::recursive_directory_iterator(kCases / "pie");
         it != fs::recursive_directory_iterator(); ++it)
        if (it->is_regular_file() && it->path().extension() == ".mmd") ++originals;
    EXPECT_EQ(originals, 24u);
}

TEST(MermaidPie0042Cohort, DrawnRowsKeepFrameIdentity) {
    int checked = 0;
    for (const auto& rel : kPie7) {
        const auto source = read_file(kCases / rel);
        const auto stripped = frontmatter_stripped(source);
        ASSERT_TRUE(draws(source, kWide)) << rel << " (drawn row must draw)";
        const auto with = capture(source, "mermaid", kWide);
        const auto without = capture(stripped, "mermaid", kWide);
        ASSERT_EQ(with.chars.size(), without.chars.size()) << rel;
        for (std::size_t i = 0; i < with.chars.size(); ++i)
            ASSERT_EQ(with.chars[i], without.chars[i])
                << rel << " cell " << i << " (frontmatter changed the frame)";
        for (const auto* marker : {"Pie native projection v1", "config.theme="})
            EXPECT_EQ(with.text.find(marker), std::string::npos) << rel << " " << marker;
        ++checked;
    }
    EXPECT_EQ(checked, 7);
}
