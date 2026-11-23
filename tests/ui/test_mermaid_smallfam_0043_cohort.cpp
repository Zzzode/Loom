// RFC 0043 small-family theme admission cohort gate.
//
// Public API only, mirroring tools/mermaid/smallfam-0043-cohort.json against
// the live renderer. Every row's frame must equal its body-identical corpus
// twin's frame (or the config-free synthetic twin's frame) plus exactly the
// literal disclosure footer the ledger pins. The closed-table and grammar
// batteries below are an independent oracle: their expected verdicts and
// footer strings are hardcoded literals, never derived from the ledger or the
// renderer's own table. The last test is the drawn-row frame-identity oracle
// over the three families' already-drawn rows (7 + 11 + 8).
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

const fs::path kFixtures = fs::path(LOOM_TESTS_DIR) / "fixtures/mermaid_official";
const fs::path kCases = kFixtures / "cases";
const fs::path kManifest = kFixtures / "manifest.json";
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/smallfam-0043-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 800;

struct FamilySpec {
    const char* name;
    fs::path ledger;
    std::vector<std::string> themes;
    std::string header;
    std::string body;
    int expected_originals;
    int expected_draw;
    int expected_admitted;
    int expected_layout_admitted;
    int expected_frontmatter_gated;
    int expected_body_style_gated;
    // D-443 adds a dedicated bucket for the closed look/seed/font rows so the
    // five-way partition stays literal; D-453 adds the venn closed-style rows.
    int expected_d443 = 0;
    int expected_d453 = 0;
};

const std::vector<FamilySpec>& families() {
    static const std::vector<FamilySpec> specs = {
        {"ishikawa",
         fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/ishikawa-cohort.json",
         {"dark", "forest"},
         "Ishikawa native projection v1",
         "ishikawa-beta\n  Effect\n      Cause\n",
         13, 7, 2, 3, 0, 0, 1},
        {"venn",
         fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/venn-cohort.json",
         {"dark", "redux-color", "redux-dark-color"},
         "Venn native projection v1",
         "venn-beta\n  set A\n  set B\n  union A, B\n",
         21, 11, 3, 0, 0, 0, 2, 5},
        {"railroad",
         fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/railroad-cohort.json",
         {"dark"},
         "Railroad native projection v1",
         "railroad-beta\nvalue = terminal(\"x\") ;\n",
         9, 8, 1, 0, 0, 0},
    };
    return specs;
}

const FamilySpec& family_spec(const std::string& name) {
    for (const auto& spec : families())
        if (name == spec.name) return spec;
    throw std::runtime_error("unknown family " + name);
}

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nSF_SENTINEL";
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
// A native projection never echoes the fenced source envelope. Line equality
// only; a bare family-name substring is not a valid marker.
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "theme:" || line == "  theme:") return false;
    }
    return got.back() == "SF_SENTINEL";
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
// A boundary probe: a folded description envelope plus the given config block.
std::string probe(const FamilySpec& spec, std::string_view config) {
    return "---\ndescription: >-\n  small-family theme boundary probe\n" +
           std::string(config) + "---\n" + spec.body;
}
std::string config_probe(const FamilySpec& spec, std::string_view config) {
    return probe(spec, std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
void expect_footer(const std::string& source, const FamilySpec& spec, std::string_view theme,
                   const std::string& label) {
    const std::vector<std::string> footer{"", spec.header, theme_line(theme)};
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), footer.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "SF_SENTINEL") << label;
    ASSERT_TRUE(lines[lines.size() - 2u].empty()) << label << " (expected a trailing blank row)";
    const auto begin = lines.size() - 2u - footer.size();
    for (std::size_t i = 0; i < footer.size(); ++i)
        EXPECT_EQ(lines[begin + i], footer[i]) << label << " footer line " << i;
}
}  // namespace

TEST(MermaidSmallfam0043Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_smallfam_0043_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 43);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("originals"), 43);
    EXPECT_EQ(counts.get_int("drawn"), 26);
    EXPECT_EQ(counts.get_int("admitted"), 6);
    EXPECT_EQ(counts.get_int("other"), 11);

    // The review gate is agent-run and must be recorded, not asserted.
    const auto review = root.get("review");
    EXPECT_EQ(review.get_string("verdict"), "approved");
    EXPECT_FALSE(review.get_string("reviewer_agent").empty());
    EXPECT_FALSE(review.get_string("note").empty());

    const auto selection = root.get("selection");
    std::set<std::string> pinned;
    selection.get("path_pin").iter(
        [&](JsonVal value) { pinned.insert(std::string(value.as_str())); });
    EXPECT_EQ(pinned.size(), 6u);

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });

    std::map<std::string, int> per_family;
    std::map<std::string, int> twin_kinds;
    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> admitted;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto family = row.get_string("family");
        const auto& spec = family_spec(family);
        admitted.insert(rel);
        ++per_family[family];
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        // The theme is a member of the family's literal closed table.
        const std::string theme = row.get_string("theme");
        EXPECT_NE(std::ranges::find(spec.themes, theme), spec.themes.end()) << rel;
        std::vector<std::string> footer;
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { footer.emplace_back(line.as_str()); });
        ASSERT_EQ(footer.size(), 2u) << rel;
        EXPECT_EQ(footer[0], spec.header) << rel;
        EXPECT_EQ(footer[1], theme_line(theme)) << rel;
        const auto twin = row.get("twin");
        ++twin_kinds[twin.get_string("kind")];
        if (twin.get_string("kind") == "corpus") {
            const std::string twin_rel = twin.get_string("path");
            ASSERT_TRUE(fs::exists(kCases / twin_rel)) << twin_rel;
            EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / twin_rel)),
                      twin.get_string("sha256"))
                << rel << " twin pin";
            EXPECT_EQ(frontmatter_stripped(read_file(kCases / rel)),
                      frontmatter_stripped(read_file(kCases / twin_rel)))
                << rel << " body equivalence";
        } else {
            EXPECT_EQ(twin.get_string("kind"), "synthetic") << rel;
        }
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(admitted, pinned);
    EXPECT_EQ(per_family["ishikawa"], 2);
    EXPECT_EQ(per_family["venn"], 3);
    EXPECT_EQ(per_family["railroad"], 1);
    EXPECT_EQ(twin_kinds["corpus"], 4);
    EXPECT_EQ(twin_kinds["synthetic"], 2);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));

    // Sibling ledger <-> family ledgers: same admitted set, same digest,
    // disjoint from the family's drawn rows.
    for (const auto& spec : families()) {
        const auto family = parse_doc(spec.ledger);
        const auto family_selection = family.root().get("selection");
        std::set<std::string> family_admitted, family_drawn;
        family_selection.get("admitted").iter(
            [&](JsonVal value) { family_admitted.insert(std::string(value.as_str())); });
        family.root().get("rows").iter(
            [&](JsonVal row) { family_drawn.insert(row.get_string("path")); });
        std::set<std::string> expected;
        for (const auto& rel : admitted)
            if (rel.starts_with(std::string(spec.name) + "/")) expected.insert(rel);
        EXPECT_EQ(family_admitted, expected) << spec.name;
        EXPECT_FALSE(family_selection.get_string("admitted_digest").empty()) << spec.name;
        for (const auto& rel : expected) EXPECT_FALSE(family_drawn.contains(rel)) << rel;
    }
}

TEST(MermaidSmallfam0043Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto states = load_states(row.get("expected_states"));
        const auto twin = row.get("twin");

        // 1. The independent side of the comparison must itself draw: a
        //    "both fall back equal" pair is not inertness.
        std::string twin_source;
        if (twin.get_string("kind") == "corpus") {
            twin_source = read_file(kCases / twin.get_string("path"));
        } else {
            twin_source = frontmatter_stripped(source);
        }
        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(native_frame(twin_source, kWide)) << rel;

        // 2. The admitted source draws and is a native frame.
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        EXPECT_TRUE(native_frame(source, kWide)) << rel;

        // 3. Per-width ledger states, including the live narrow disposition
        //    (rule 34: the footer changes the whole-surface 40-column
        //    predicate; it is measured here, not inherited from the twin).
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";

        // 4. Determinism.
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // 5. Frame composition: twin frame + the ledger's exact footer.
        auto expected = trimmed_lines(capture(twin_source, "mermaid", kWide), kWide);
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "SF_SENTINEL") << rel;
        expected.pop_back();
        // The block keeps the source's own trailing blank run after the rows;
        // insert the footer (C++ blank separator + ledger lines) before it.
        std::size_t tail_blanks = 0;
        while (!expected.empty() && expected.back().empty()) {
            expected.pop_back();
            ++tail_blanks;
        }
        expected.emplace_back();
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        for (std::size_t i = 0; i < tail_blanks; ++i) expected.emplace_back();
        expected.emplace_back("SF_SENTINEL");
        const auto got = trimmed_lines(first, kWide);
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        ++checked;
    });
    EXPECT_EQ(checked, 6);
}

TEST(MermaidSmallfam0043Cohort, ClosedTablesAdmitWithExactFooter) {
    for (const auto& spec : families()) {
        for (const auto& value : spec.themes) {
            const auto source = config_probe(spec, "  theme: " + value + "\n");
            ASSERT_TRUE(draws(source, kWide)) << spec.name << " " << value;
            ASSERT_TRUE(native_frame(source, kWide)) << spec.name << " " << value;
            expect_footer(source, spec, value, std::string(spec.name) + " theme " + value);
        }
    }
    // A quoted scalar and a trailing comment normalize to the same value in
    // every family: the three scalar copies must not drift silently.
    for (const auto& spec : families()) {
        const auto& value = spec.themes.front();
        for (const auto* form : {"  theme: \"%s\"", "  theme: '%s'", "  theme: %s # note"}) {
            std::string line = form;
            const auto at = line.find("%s");
            ASSERT_NE(at, std::string::npos) << form;
            line.replace(at, 2, value);
            line += "\n";
            const auto source = config_probe(spec, line);
            ASSERT_TRUE(draws(source, kWide)) << spec.name << " " << line;
            expect_footer(source, spec, value, std::string(spec.name) + " scalar " + line);
        }
    }
    // A YAML comment or a blank line inside the block is not a leaf.
    for (const auto& spec : families()) {
        const auto& value = spec.themes.front();
        const auto* forms = "  # palette\n  theme: %s\n";
        std::string commented = forms;
        commented.replace(commented.find("%s"), 2, value);
        ASSERT_TRUE(draws(config_probe(spec, commented), kWide)) << spec.name;
        expect_footer(config_probe(spec, commented), spec, value,
                      std::string(spec.name) + " block comment");
        const auto blank = "  theme: " + value + "\n\n";
        ASSERT_TRUE(draws(config_probe(spec, blank), kWide)) << spec.name;
        expect_footer(config_probe(spec, blank), spec, value,
                      std::string(spec.name) + " block blank");
    }
}

TEST(MermaidSmallfam0043Cohort, OutOfTableAndShapeRejects) {
    // Closed tables: another family's value, a case variant, an unknown value,
    // an empty value and an extra token.
    EXPECT_FALSE(draws(config_probe(families()[0], "  theme: redux-color\n"), kWide));
    EXPECT_FALSE(draws(config_probe(families()[1], "  theme: forest\n"), kWide));
    EXPECT_FALSE(draws(config_probe(families()[2], "  theme: redux-color\n"), kWide));
    for (const auto& spec : families()) {
        EXPECT_FALSE(draws(config_probe(spec, "  theme: Dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: chartreuse\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme:\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: dark extra\n"), kWide)) << spec.name;
        // Not the admitted position: column 0, indent 3, indent 4, a space
        // before the colon, a colon-less leaf, a duplicate, a second block.
        EXPECT_FALSE(draws(probe(spec, "theme: dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "   theme: dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "    theme: dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme : dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: dark\n  theme: dark\n"), kWide))
            << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: dark\nconfig:\n  theme: dark\n"),
                          kWide))
            << spec.name;
        // D-443 admits the closed look/seed/font trio for ishikawa and venn
        // (that gate owns the positive frames); railroad keeps every look leaf
        // gated, and out-of-table values stay gated in every family.
        const bool d443_family =
            std::string_view(spec.name) == "ishikawa" || std::string_view(spec.name) == "venn";
        EXPECT_FALSE(draws(config_probe(spec, "  look: classic\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: dark\n  look: classic\n"), kWide))
            << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  theme: dark\n  handDrawnSeed: 0\n"), kWide))
            << spec.name;
        if (d443_family) {
            EXPECT_TRUE(draws(config_probe(spec, "  look: handDrawn\n"), kWide)) << spec.name;
            EXPECT_TRUE(
                draws(config_probe(spec, "  theme: dark\n  look: handDrawn\n"), kWide))
                << spec.name;
        } else {
            EXPECT_FALSE(draws(config_probe(spec, "  look: handDrawn\n"), kWide)) << spec.name;
            EXPECT_FALSE(
                draws(config_probe(spec, "  theme: dark\n  look: handDrawn\n"), kWide))
                << spec.name;
            EXPECT_FALSE(
                draws(config_probe(spec, "  theme: dark\n  handDrawnSeed: 1\n"), kWide))
                << spec.name;
        }
        EXPECT_FALSE(draws(config_probe(spec, "  htmlLabels: false\n"), kWide)) << spec.name;
        // RFC 0046 admits the closed ishikawa tuning namespace for the
        // ishikawa spec only; the sibling families keep it gated, and an
        // out-of-table value stays gated everywhere.
        if (std::string_view(spec.name) == "ishikawa") {
            EXPECT_TRUE(draws(config_probe(spec, "  ishikawa:\n    useMaxWidth: false\n"),
                              kWide))
                << spec.name;
            EXPECT_FALSE(draws(config_probe(spec, "  ishikawa:\n    useMaxWidth: maybe\n"),
                               kWide))
                << spec.name;
        } else {
            EXPECT_FALSE(
                draws(config_probe(spec, "  ishikawa:\n    useMaxWidth: false\n"), kWide))
                << spec.name;
        }
        EXPECT_FALSE(draws(probe(spec, "config:\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(probe(spec, "config: {theme: dark}\n"), kWide)) << spec.name;
        // Indented config is not a column-0 block header.
        EXPECT_FALSE(draws(probe(spec, "  config:\n    theme: dark\n"), kWide)) << spec.name;
        // An unmatched quote or a duplicate theme inside a nested description.
        EXPECT_FALSE(draws(config_probe(spec, "  theme: \"dark\n"), kWide)) << spec.name;
        EXPECT_FALSE(draws(config_probe(spec, "  description: x\n"), kWide)) << spec.name;
    }
    // Column-0 `config : 42` spacing: ishikawa/venn compare the raw key so the
    // shape is ordinary passthrough metadata and the body still draws;
    // railroad trims the key and gates it, exactly as before RFC 0043.
    EXPECT_TRUE(draws(probe(families()[0], "config : 42\n"), kWide));
    EXPECT_TRUE(draws(probe(families()[1], "config : 42\n"), kWide));
    EXPECT_FALSE(draws(probe(families()[2], "config : 42\n"), kWide));

    // Preserved passthrough: unrelated column-0 metadata and a colon-less
    // `description` line are ignored, as before.
    const auto passthrough = probe(families()[1], "title: x\n");
    ASSERT_TRUE(draws(passthrough, kWide));
    EXPECT_FALSE(has_text(passthrough, families()[1].header));
    ASSERT_TRUE(draws("---\ndescription\n---\n" + families()[1].body, kWide));
    // An admitted theme with column-0 description AFTER the block is admitted
    // (description closes the block; the folded payload is outside).
    const auto after = "---\nconfig:\n  theme: dark\ndescription: >-\n  payload\n---\n" +
                       families()[1].body;
    ASSERT_TRUE(draws(after, kWide));
    expect_footer(after, families()[1], "dark", "description after config");
    // The legacy flat scan strips leading spaces and compares the key, so a
    // folded payload line that looks like a visual key still gates; a payload
    // line that is not a visual key stays documentation.
    const auto visual_payload =
        "---\ndescription: >-\n  theme: dark\n---\n" + families()[1].body;
    EXPECT_FALSE(draws(visual_payload, kWide));
    const auto prose_payload =
        "---\ndescription: >-\n  rendered under the dark palette\n---\n" +
        families()[1].body;
    ASSERT_TRUE(draws(prose_payload, kWide));
    EXPECT_FALSE(has_text(prose_payload, families()[1].header));

    // Body gates are unchanged: a style assignment closes every family, the
    // %%{init} body gate belongs to ishikawa and railroad only (venn ignores
    // body comments), and an admitted theme does not bypass them.
    for (const auto& spec : families()) {
        // D-453 admits the closed venn `style` statement; ishikawa/railroad
        // still keep the whole source, as does every family for an off-table
        // style property.
        if (std::string_view(spec.name) == "venn") {
            EXPECT_TRUE(draws("---\nconfig:\n  theme: dark\n---\n" + spec.body +
                                  "  style A fill:#ff0000\n",
                              kWide))
                << spec.name;
            EXPECT_FALSE(draws("---\nconfig:\n  theme: dark\n---\n" + spec.body +
                                   "  style A unknown:#ff0000\n",
                               kWide))
                << spec.name;
            continue;
        }
        EXPECT_FALSE(draws("---\nconfig:\n  theme: dark\n---\n" + spec.body +
                               "  style A fill:#ff0000\n",
                           kWide))
            << spec.name;
    }
    EXPECT_FALSE(draws("---\nconfig:\n  theme: dark\n---\n%%{init: {\"theme\": \"dark\"}}%%\n" +
                           families()[0].body,
                       kWide));
    EXPECT_FALSE(draws("---\nconfig:\n  theme: dark\n---\n%%{init: {\"theme\": \"dark\"}}%%\n" +
                           families()[2].body,
                       kWide));
    EXPECT_TRUE(draws("%%{init: {\"theme\": \"dark\"}}%%\n" + families()[1].body, kWide));
}

TEST(MermaidSmallfam0043Cohort, FamilyLedgersPartitionFiveWays) {
    // Literal arithmetic against the filesystem enumeration, not a
    // recomputation from the ledger's own lists: a row dropped from every list
    // must be caught.
    for (const auto& spec : families()) {
        int files = 0;
        for (const auto& entry : fs::recursive_directory_iterator(kCases / spec.name))
            if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++files;
        EXPECT_EQ(files, spec.expected_originals) << spec.name;

        const auto family = parse_doc(spec.ledger);
        const auto selection = family.root().get("selection");
        EXPECT_EQ(selection.get_int("draw_count"), spec.expected_draw) << spec.name;
        EXPECT_EQ(selection.get_int("admitted_count"), spec.expected_admitted) << spec.name;
        EXPECT_EQ(selection.get_int("frontmatter_gated_count"),
                  spec.expected_frontmatter_gated)
            << spec.name;
        EXPECT_EQ(selection.get_int("body_style_gated_count"),
                  spec.expected_body_style_gated)
            << spec.name;
        EXPECT_EQ(selection.get_int("layout_admitted_count"),
                  spec.expected_layout_admitted)
            << spec.name;
        EXPECT_EQ(selection.get_int("d443_admitted_count"), spec.expected_d443) << spec.name;
        EXPECT_EQ(selection.get_int("d453_admitted_count"), spec.expected_d453) << spec.name;
        EXPECT_EQ(selection.get_int("family_total"), spec.expected_originals) << spec.name;
        EXPECT_EQ(selection.get_int("draw_count") + selection.get_int("admitted_count") +
                      selection.get_int("layout_admitted_count") +
                      selection.get_int("frontmatter_gated_count") +
                      selection.get_int("body_style_gated_count") +
                      selection.get_int("d443_admitted_count") +
                      selection.get_int("d453_admitted_count"),
                  spec.expected_originals)
            << spec.name;
        EXPECT_EQ(selection.get_int("admission_rfc"), 43) << spec.name;

        // The four lists are disjoint and cover the enumerated files exactly.
        std::set<std::string> covered;
        int drawn = 0;
        family.root().get("rows").iter([&](JsonVal row) {
            ++drawn;
            covered.insert(row.get_string("path"));
        });
        EXPECT_EQ(drawn, spec.expected_draw) << spec.name;
        for (const auto* key : {"admitted", "layout_admitted", "frontmatter_gated",
                                "body_style_gated", "d443_admitted",
                                "d453_admitted"}) {
            int listed = 0;
            selection.get(key).iter([&](JsonVal value) {
                ++listed;
                EXPECT_TRUE(covered.insert(std::string(value.as_str())).second)
                    << spec.name << " " << key << " duplicate " << value.as_str();
            });
            if (std::string_view(key) == "admitted")
                EXPECT_EQ(listed, spec.expected_admitted) << spec.name;
            if (std::string_view(key) == "layout_admitted")
                EXPECT_EQ(listed, spec.expected_layout_admitted) << spec.name;
            if (std::string_view(key) == "frontmatter_gated")
                EXPECT_EQ(listed, spec.expected_frontmatter_gated) << spec.name;
            if (std::string_view(key) == "body_style_gated")
                EXPECT_EQ(listed, spec.expected_body_style_gated) << spec.name;
            if (std::string_view(key) == "d443_admitted")
                EXPECT_EQ(listed, spec.expected_d443) << spec.name;
            if (std::string_view(key) == "d453_admitted")
                EXPECT_EQ(listed, spec.expected_d453) << spec.name;
        }
        EXPECT_EQ(static_cast<int>(covered.size()), spec.expected_originals) << spec.name;
        // The remaining gated rows stay whole-source; an empty list is
        // asserted explicitly so the check cannot pass vacuously.
        for (const auto* key : {"frontmatter_gated", "body_style_gated"}) {
            selection.get(key).iter([&](JsonVal value) {
                EXPECT_FALSE(draws(read_file(kCases / value.as_str()), kWide))
                    << value.as_str();
            });
        }
    }
    // The sibling ledger's admitted digests equal the family ledgers'.
    const auto ledger = parse_doc(kLedger);
    for (const auto& spec : families()) {
        const auto family = parse_doc(spec.ledger);
        std::vector<std::pair<std::string, std::string>> digest_input;
        int admitted = 0;
        ledger.root().get("rows").iter([&](JsonVal row) {
            if (row.get_string("family") != spec.name) return;
            ++admitted;
            digest_input.emplace_back(row.get_string("path"), row.get_string("sha256"));
        });
        std::ranges::sort(digest_input);
        std::string payload;
        for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
        EXPECT_EQ(family.root().get("selection").get_string("admitted_digest"),
                  loom::utils::crypto::sha256(payload))
            << spec.name;
        EXPECT_EQ(admitted, spec.expected_admitted) << spec.name;
    }
}

// Regression oracle: every row the three families already draw must render
// cell-identically with and without its frontmatter. A footer or blank
// separator emitted merely because a frontmatter is present breaks the full
// untrimmed surface equality, and no footer marker may appear.
TEST(MermaidSmallfam0043Cohort, DrawnRowsKeepFrameIdentity) {
    int checked = 0;
    for (const auto& spec : families()) {
        const auto family = parse_doc(spec.ledger);
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
            for (const auto* marker : {spec.header.c_str(), "config.theme="})
                EXPECT_EQ(with.text.find(marker), std::string::npos) << rel << " " << marker;
            ++checked;
        });
    }
    EXPECT_EQ(checked, 26);
}
