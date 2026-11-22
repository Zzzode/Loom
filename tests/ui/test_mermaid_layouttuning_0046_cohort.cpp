// RFC 0046 layout/display tuning namespace admission cohort gate.
//
// Public API only, mirroring tools/mermaid/layouttuning-0046-cohort.json
// against the live renderers for ishikawa, architecture and state (v1 + v2).
// Every row's frame must equal its twin's frame plus exactly the ledger's
// disclosure footer. The twin is a clean corpus twin where a body-identical
// drawn fixture exists (2 rows), otherwise a synthetic config-free twin whose
// "config-free" check is structural — never a whole-source substring search,
// because the descriptions legitimately mention the key names in prose (the
// RFC 0045 code-review trap).
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/layouttuning-0046-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;

constexpr std::string_view kIshikawaBody = "ishikawa-beta\n  Effect\n      Cause\n";
constexpr std::string_view kArchBody = "architecture-beta\n  service a(server)[A]\n";
constexpr std::string_view kStateBody =
    "stateDiagram-v2\n  [*] --> State1\n  State1 --> [*]\n";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nLT46_SENTINEL";
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
// Line-equality native-frame predicate: the disclosure footers carry the
// family config prefixes, so only envelope-echo lines are rejected.
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "  ishikawa:" || line == "  architecture:" || line == "  state:")
            return false;
        if (line.starts_with("    useMaxWidth:") || line.starts_with("    diagramPadding:"))
            return false;
        if (line.starts_with("    randomize:") || line.starts_with("    seed:") ||
            line.starts_with("    idealEdgeLengthMultiplier:"))
            return false;
    }
    return got.back() == "LT46_SENTINEL";
}
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
std::string config_free_twin(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    const std::string frontmatter = source.substr(4, close - 4);
    const std::string body = source.substr(close + 5);
    std::string kept = "---\n";
    bool skipping = false;
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        if (skipping && line.size() >= 2 && line[0] == ' ' && line[1] == ' ') continue;
        skipping = false;
        if (line.starts_with("config:")) {
            skipping = true;
            continue;
        }
        kept += line;
        kept += "\n";
    }
    kept += "---\n";
    return kept + body;
}
bool twin_is_config_free(const std::string& source) {
    if (!source.starts_with("---\n")) return false;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return false;
    const std::string frontmatter = source.substr(4, close - 4);
    for (std::size_t pos = 0; pos < frontmatter.size();) {
        const auto eol = frontmatter.find('\n', pos);
        const auto end = eol == std::string::npos ? frontmatter.size() : eol;
        const std::string line = frontmatter.substr(pos, end - pos);
        pos = end + 1;
        const auto first = line.find_first_not_of(' ');
        const std::string_view stripped =
            first == std::string::npos ? std::string_view{}
                                       : std::string_view(line).substr(first);
        static constexpr std::string_view kForbidden[] = {
            "ishikawa:", "architecture:", "state:", "useMaxWidth:", "diagramPadding:",
            "randomize:", "seed:", "idealEdgeLengthMultiplier:"};
        if (line.starts_with("config:")) return false;
        for (const auto key : kForbidden)
            if (stripped.starts_with(key)) return false;
    }
    return true;
}
std::string probe(std::string_view body, std::string_view config) {
    return "---\ndescription: >-\n  layout tuning boundary probe\n" + std::string(config) +
           "---\n" + std::string(body);
}
std::string config_probe(std::string_view body, std::string_view config) {
    return probe(body, std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
std::string header_for(std::string_view family) {
    if (family == "ishikawa") return "Ishikawa native projection v1";
    if (family == "architecture") return "Architecture native projection v1";
    return "State native projection v1";
}
void expect_footer(const std::string& source, const std::vector<std::string>& lines,
                   const std::string& label) {
    std::vector<std::string> expected{"", lines.front()};
    expected.insert(expected.end(), lines.begin() + 1, lines.end());
    const auto got = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(got.size(), expected.size() + 2u) << label;
    ASSERT_EQ(got.back(), "LT46_SENTINEL") << label;
    ASSERT_TRUE(got[got.size() - 2u].empty()) << label << " (trailing blank row)";
    const auto begin = got.size() - 2u - expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(got[begin + i], expected[i]) << label << " footer line " << i;
}
std::vector<std::string> string_list(const JsonVal& value) {
    std::vector<std::string> out;
    value.iter([&](JsonVal item) { out.emplace_back(item.as_str()); });
    return out;
}
std::string ishikawa_line(std::string_view value) {
    return "config.ishikawa.useMaxWidth=" + std::string(value) +
           " -> display sizing; not applied";
}
std::string padding_line() {
    return "config.ishikawa.diagramPadding=50 -> diagram padding; not applied";
}
std::string arch_line(std::string_view key, std::string_view value) {
    return "config.architecture." + std::string(key) + "=" + std::string(value) +
           " -> layout tuning; not applied";
}
std::string state_line(std::string_view value) {
    return "config.state.useMaxWidth=" + std::string(value) + " -> display sizing; not applied";
}
}  // namespace

TEST(MermaidLayouttuning0046Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_layouttuning_0046_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 46);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("originals"), 178);
    EXPECT_EQ(counts.get_int("drawn"), 105);
    EXPECT_EQ(counts.get_int("admitted"), 11);
    EXPECT_EQ(counts.get_int("remaining"), 62);
    const auto review = root.get("review");
    EXPECT_EQ(review.get_string("verdict"), "approved");
    EXPECT_FALSE(review.get_string("reviewer_agent").empty());

    const auto admission = root.get("admission");
    const auto tables = admission.get("tables");
    EXPECT_EQ(string_list(tables.get("config.ishikawa.useMaxWidth")),
              (std::vector<std::string>{"true", "false"}));
    EXPECT_EQ(string_list(tables.get("config.ishikawa.diagramPadding")),
              (std::vector<std::string>{"50"}));
    EXPECT_EQ(string_list(tables.get("config.architecture.randomize")),
              (std::vector<std::string>{"true", "false"}));
    EXPECT_EQ(string_list(tables.get("config.architecture.seed")),
              (std::vector<std::string>{"42"}));
    EXPECT_EQ(string_list(tables.get("config.architecture.idealEdgeLengthMultiplier")),
              (std::vector<std::string>{"3"}));
    EXPECT_EQ(string_list(tables.get("config.state.useMaxWidth")),
              (std::vector<std::string>{"true", "false"}));
    EXPECT_EQ(admission.get("footer").get_int("line_limit_bytes"), 96);

    const auto selection = root.get("selection");
    const auto families = selection.get("families");
    EXPECT_EQ(families.get("ishikawa").get_int("originals"), 13);
    EXPECT_EQ(families.get("ishikawa").get_int("drawn_before"), 9);
    EXPECT_EQ(families.get("ishikawa").get_int("admitted"), 3);
    EXPECT_EQ(families.get("architecture").get_int("originals"), 20);
    EXPECT_EQ(families.get("architecture").get_int("drawn_before"), 16);
    EXPECT_EQ(families.get("architecture").get_int("admitted"), 4);
    EXPECT_EQ(families.get("state-diagram").get_int("originals"), 42);
    EXPECT_EQ(families.get("state-diagram").get_int("drawn_before"), 28);
    EXPECT_EQ(families.get("state-diagram").get_int("admitted"), 2);
    EXPECT_EQ(families.get("state-diagram-v2").get_int("originals"), 103);
    EXPECT_EQ(families.get("state-diagram-v2").get_int("drawn_before"), 52);
    EXPECT_EQ(families.get("state-diagram-v2").get_int("admitted"), 2);
    const auto global = selection.get("global");
    EXPECT_EQ(global.get_int("originals"), 2145);
    EXPECT_EQ(global.get_int("drawn_before"), 1517);
    EXPECT_EQ(global.get_int("admitted"), 11);
    EXPECT_EQ(global.get_int("fallback_before"), 628);
    EXPECT_EQ(global.get_int("fallback_after"), 617);

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    const auto upstream = root.get("upstream");
    EXPECT_EQ(upstream.get_string("tag"), manifest.root().get("upstream").get_string("tag"));
    EXPECT_EQ(upstream.get_string("commit"),
              manifest.root().get("upstream").get_string("commit"));
    EXPECT_EQ(upstream.get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));

    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> admitted;
    std::map<std::string, int> kinds;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        admitted.insert(rel);
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        const auto family = row.get_string("family");
        auto footer = string_list(row.get("expected_footer_lines"));
        ASSERT_EQ(footer.size(), 2u) << rel;
        EXPECT_EQ(footer.front(), header_for(family)) << rel;
        const auto twin = row.get("twin");
        const auto kind = twin.get_string("kind");
        ++kinds[kind];
        if (kind == "corpus") {
            const auto twin_rel = twin.get_string("path");
            ASSERT_TRUE(fs::exists(kCases / twin_rel)) << twin_rel;
            EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / twin_rel)),
                      twin.get_string("sha256"))
                << rel << " twin pin";
            EXPECT_EQ(frontmatter_stripped(read_file(kCases / rel)),
                      frontmatter_stripped(read_file(kCases / twin_rel)))
                << rel << " body equivalence";
            EXPECT_TRUE(draws(read_file(kCases / twin_rel), kWide)) << twin_rel;
            // A clean twin carries no footer marker for its family.
            EXPECT_FALSE(has_text(read_file(kCases / twin_rel), header_for(family)))
                << twin_rel;
        } else {
            EXPECT_EQ(kind, "synthetic") << rel;
            const auto twin_source = config_free_twin(read_file(kCases / rel));
            EXPECT_EQ(loom::utils::crypto::sha256(twin_source), twin.get_string("sha256"))
                << rel;
            EXPECT_TRUE(twin_is_config_free(twin_source)) << rel << " twin still carries config";
            EXPECT_EQ(frontmatter_stripped(twin_source),
                      frontmatter_stripped(read_file(kCases / rel)))
                << rel << " body preserved";
            EXPECT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        }
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    std::set<std::string> pinned;
    selection.get("path_pin").iter(
        [&](JsonVal value) { pinned.insert(std::string(value.as_str())); });
    EXPECT_EQ(admitted, pinned);
    EXPECT_EQ(admitted.size(), 11u);
    EXPECT_EQ(kinds["corpus"], 2);
    EXPECT_EQ(kinds["synthetic"], 9);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));

    // Sibling agreement: the two corpus twins are drawn rows of their family
    // ledgers, and both rows are not drawn by those ledgers.
    const auto ishikawa = parse_doc(fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/ishikawa-cohort.json");
    const auto arch = parse_doc(
        fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/architecture-cohort.json");
    std::set<std::string> drawn_rows;
    ishikawa.root().get("rows").iter(
        [&](JsonVal row) { drawn_rows.insert(row.get_string("path")); });
    EXPECT_TRUE(drawn_rows.contains(
        "ishikawa/pr-check/1-should-render-a-simple-ishikawa-diagram.mmd"));
    const auto arch_twin = "architecture/visual-check/"
                           "should-render-a-linear-chain-with-default-fcose-knobs.mmd";
    std::set<std::string> arch_drawn;
    arch.root().get("rows").iter(
        [&](JsonVal row) { arch_drawn.insert(row.get_string("path")); });
    EXPECT_TRUE(arch_drawn.contains(arch_twin));
}

TEST(MermaidLayouttuning0046Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = row.get("twin");
        const auto twin_source = twin.get_string("kind") == "corpus"
                                     ? read_file(kCases / twin.get_string("path"))
                                     : config_free_twin(source);
        const auto states = load_states(row.get("expected_states"));

        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // Compose: twin frame, blank separator, then the ledger footer block.
        auto expected = trimmed_lines(capture(twin_source, "mermaid", kWide), kWide);
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "LT46_SENTINEL") << rel;
        expected.pop_back();
        std::size_t tail_blanks = 0;
        while (!expected.empty() && expected.back().empty()) {
            expected.pop_back();
            ++tail_blanks;
        }
        expected.emplace_back();
        row.get("expected_footer_lines").iter(
            [&](JsonVal line) { expected.emplace_back(line.as_str()); });
        for (std::size_t i = 0; i < tail_blanks; ++i) expected.emplace_back();
        expected.emplace_back("LT46_SENTINEL");
        const auto got = trimmed_lines(first, kWide);
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        ++checked;
    });
    EXPECT_EQ(checked, 11);
}

TEST(MermaidLayouttuning0046Cohort, IshikawaBattery) {
    const auto both = config_probe(kIshikawaBody, "  ishikawa:\n    useMaxWidth: false\n");
    ASSERT_TRUE(draws(both, kWide));
    ASSERT_TRUE(native_frame(both, kWide));
    expect_footer(both, {header_for("ishikawa"), ishikawa_line("false")}, "useMaxWidth false");
    const auto padding = config_probe(kIshikawaBody, "  ishikawa:\n    diagramPadding: 50\n");
    ASSERT_TRUE(draws(padding, kWide));
    expect_footer(padding, {header_for("ishikawa"), padding_line()}, "diagramPadding");
    // Scalar normalization and canonical footer order after the theme line.
    for (const auto* form : {"  ishikawa:\n    useMaxWidth: \"true\" # note\n"}) {
        const auto source = config_probe(kIshikawaBody, form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        EXPECT_TRUE(has_text(source, ishikawa_line("true"))) << form;
    }
    // Both tuning leaves compose in the canonical order.
    const auto composed_tuning = config_probe(
        kIshikawaBody, "  ishikawa:\n    useMaxWidth: true\n    diagramPadding: 50\n");
    ASSERT_TRUE(draws(composed_tuning, kWide));
    expect_footer(composed_tuning,
                  {header_for("ishikawa"), ishikawa_line("true"), padding_line()},
                  "useMaxWidth + diagramPadding");
    // The theme leaf and the tuning namespace are mutually exclusive shapes.
    EXPECT_FALSE(draws(config_probe(kIshikawaBody,
                                    "  theme: dark\n  ishikawa:\n    useMaxWidth: true\n"),
                       kWide))
        << "theme + namespace";
    EXPECT_FALSE(draws(config_probe(kIshikawaBody,
                                    "  ishikawa:\n    useMaxWidth: true\n  theme: dark\n"),
                       kWide))
        << "namespace + theme";
    for (const auto* form : {
             "  ishikawa:\n    useMaxWidth: yes\n",
             "  ishikawa:\n    useMaxWidth: 1\n",
             "  ishikawa:\n    useMaxWidth:\n",
             "  ishikawa:\n    diagramPadding: 40\n",
             "  ishikawa:\n    width: 100\n",
             "  ishikawa: foo\n",
             "  ishikawa:\n",
             "  ishikawa:\n    useMaxWidth: true\n  ishikawa:\n    useMaxWidth: false\n",
             "  ishikawa:\n    useMaxWidth: true\n    useMaxWidth: false\n",
             "  useMaxWidth: true\n",
             "    ishikawa:\n      useMaxWidth: true\n",
             "  theme: classic\n",
         }) {
        EXPECT_FALSE(draws(config_probe(kIshikawaBody, form), kWide)) << form;
    }
    EXPECT_FALSE(draws(probe(kIshikawaBody, "config:\n"), kWide)) << "empty config";
}

TEST(MermaidLayouttuning0046Cohort, ArchitectureAndStateBatteries) {
    for (const auto* form : {"  architecture:\n    randomize: false\n",
                             "  architecture:\n    randomize: true\n",
                             "  architecture:\n    seed: 42\n",
                             "  architecture:\n    idealEdgeLengthMultiplier: 3\n"}) {
        const auto source = config_probe(kArchBody, form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        ASSERT_TRUE(native_frame(source, kWide)) << form;
    }
    const auto arch_line0 =
        config_probe(kArchBody, "  architecture:\n    randomize: false\n");
    expect_footer(arch_line0, {header_for("architecture"), arch_line("randomize", "false")},
                  "randomize false");
    const auto ordered = config_probe(
        kArchBody, "  architecture:\n    randomize: true\n    seed: 42\n");
    ASSERT_TRUE(draws(ordered, kWide));
    expect_footer(ordered,
                  {header_for("architecture"), arch_line("randomize", "true"),
                   arch_line("seed", "42")},
                  "randomize + seed");
    const auto seed_edge = config_probe(
        kArchBody, "  architecture:\n    seed: 42\n    idealEdgeLengthMultiplier: 3\n");
    ASSERT_TRUE(draws(seed_edge, kWide));
    expect_footer(seed_edge,
                  {header_for("architecture"), arch_line("seed", "42"),
                   arch_line("idealEdgeLengthMultiplier", "3")},
                  "seed + edge length");
    const auto all_three = config_probe(
        kArchBody, "  architecture:\n    idealEdgeLengthMultiplier: 3\n    seed: 42\n"
                   "    randomize: false\n");
    ASSERT_TRUE(draws(all_three, kWide));
    expect_footer(all_three,
                  {header_for("architecture"), arch_line("randomize", "false"),
                   arch_line("seed", "42"), arch_line("idealEdgeLengthMultiplier", "3")},
                  "all three (source order differs)");
    for (const auto* form : {
             "  architecture:\n    randomize: yes\n",
             "  architecture:\n    seed: 43\n",
             "  architecture:\n    idealEdgeLengthMultiplier: 4\n",
             "  architecture:\n    width: 100\n",
             "  architecture: foo\n",
             "  architecture:\n",
             "  architecture:\n    randomize: true\n    randomize: false\n",
             "  theme: dark\n",
             "  layout: elk\n",
         }) {
        EXPECT_FALSE(draws(config_probe(kArchBody, form), kWide)) << form;
    }
    EXPECT_FALSE(draws(probe(kArchBody, "config:\n"), kWide)) << "empty config";
    // A bare `config` without a colon does not open the block.
    EXPECT_FALSE(draws(probe(kArchBody, "config\n  architecture:\n    seed: 42\n---\n" +
                                          std::string(kArchBody)),
                       kWide))
        << "bare config";
    // The body-side gate is unchanged: an init directive still falls back.
    EXPECT_FALSE(draws("---\ndescription: >-\n  x\nconfig:\n  architecture:\n    seed: 42\n"
                       "---\n" +
                           std::string(kArchBody) +
                           "  %%{init: { 'logLevel': 0 }}%%\n",
                       kWide));
    EXPECT_FALSE(draws("---\ndescription: >-\n  x\nconfig:\n  architecture:\n    seed: 42\n"
                       "---\narchitecture-beta\n  classDef foo\n",
                       kWide));

    // State: the namespace leaf and the existing E2b leaves keep their shape.
    const auto state_false = config_probe(kStateBody, "  state:\n    useMaxWidth: false\n");
    ASSERT_TRUE(draws(state_false, kWide));
    expect_footer(state_false, {header_for("state"), state_line("false")}, "state false");
    const auto state_true = config_probe(kStateBody, "  state:\n    useMaxWidth: true\n");
    ASSERT_TRUE(draws(state_true, kWide));
    expect_footer(state_true, {header_for("state"), state_line("true")}, "state true");
    // Existing leaves keep their footer order with the new line last.
    const auto composed = config_probe(
        kStateBody, "  logLevel: 0\n  state:\n    useMaxWidth: true\n");
    ASSERT_TRUE(draws(composed, kWide));
    expect_footer(composed,
                  {header_for("state"), "config.logLevel=0 -> logger only; not applied",
                   state_line("true")},
                  "logLevel + namespace");
    for (const auto* form : {
             "  state:\n    useMaxWidth: yes\n",
             "  state:\n    width: 100\n",
             "  state: foo\n",
             "  state:\n",
             "  state:\n    useMaxWidth: true\n    useMaxWidth: false\n",
             "  fontFamily: fira\n",
             "  themeVariables:\n    primaryColor: \"#ff0000\"\n",
         }) {
        EXPECT_FALSE(draws(config_probe(kStateBody, form), kWide)) << form;
    }
    EXPECT_FALSE(draws(probe(kStateBody, "config:\n"), kWide)) << "empty state config";
}

TEST(MermaidLayouttuning0046Cohort, LiteralPartition) {
    int ishikawa = 0, architecture = 0, state_v1 = 0, state_v2 = 0;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "ishikawa"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++ishikawa;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "architecture"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++architecture;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "state-diagram"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++state_v1;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "state-diagram-v2"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++state_v2;
    EXPECT_EQ(ishikawa, 13);
    EXPECT_EQ(architecture, 20);
    EXPECT_EQ(state_v1, 42);
    EXPECT_EQ(state_v2, 103);

    const auto ledger = parse_doc(kLedger);
    const auto counts = ledger.root().get("counts");
    EXPECT_EQ(counts.get_int("originals"), ishikawa + architecture + state_v1 + state_v2);
    EXPECT_EQ(counts.get_int("originals"),
              counts.get_int("drawn") + counts.get_int("admitted") +
                  counts.get_int("remaining"));
    const auto selection = ledger.root().get("selection");
    const auto families = selection.get("families");
    for (const auto& [family, files] : {std::pair{"ishikawa", ishikawa},
                                        std::pair{"architecture", architecture},
                                        std::pair{"state-diagram", state_v1},
                                        std::pair{"state-diagram-v2", state_v2}}) {
        const auto spec = families.get(family);
        const std::string_view name{family};
        EXPECT_EQ(spec.get_int("originals"), files) << family;
        EXPECT_EQ(spec.get_int("originals"),
                  spec.get_int("drawn_before") + spec.get_int("admitted") +
                      (name == "ishikawa"       ? 1
                       : name == "architecture" ? 0
                       : name == "state-diagram" ? 12
                                                 : 49))
            << family;
    }
    std::set<std::string> admitted;
    ledger.root().get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    EXPECT_EQ(admitted.size(), 11u);
    int remaining = 0;
    for (const auto* key : {"family_residual:ishikawa", "family_residual:architecture",
                            "family_residual:state-diagram",
                            "family_residual:state-diagram-v2", "dagre_layout_tuning"})
        remaining += static_cast<int>(selection.get("remaining_by_category").get_int(key));
    EXPECT_EQ(remaining, 44);
}
