// RFC 0045 gitGraph `parallelCommits` admission cohort gate.
//
// Public API only, mirroring tools/mermaid/git-0045-cohort.json against the
// live renderer. Every row's frame must equal its synthetic config-free twin's
// frame plus exactly the ledger's disclosure footer. Native-frame predicates
// are LINE-EQUALITY checks (the RFC 0039 form), never the `find("gitGraph")`
// substring mnemonic: the new footer contains the literal `gitGraph`, which
// three other consumers use as a not-natively-drawn paraphrase. The RFC 0039
// git gate stays the drawn-row regression oracle.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-0045-cohort.json";
const fs::path kLedger0039 = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-0039-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 900;
constexpr std::string_view kHeader = "GitGraph native projection v1";
constexpr std::string_view kBody = "gitGraph\n  commit id:\"1\"\n";

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
        "```" + std::string(language) + "\n" + source + "\n```\n\nG45_SENTINEL";
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
// A native gitGraph projection never echoes the fenced source envelope. Line
// equality only: the disclosure footer legitimately contains the literal
// `gitGraph` (as does the capitalised header), so a substring mnemonic would
// misclassify a live frame.
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "  gitGraph:" || line == "gitGraph:") return false;
        if (line == "    parallelCommits:" || line == "  parallelCommits:") return false;
        if (line == "gitGraph" || line.rfind("gitGraph ", 0) == 0) return false;
    }
    return got.back() == "G45_SENTINEL";
}
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
// The synthetic twin: delete the `config:` line and every following line
// indented at least two spaces, up to the next column-0 key (the closing fence
// is column 0). The RFC 0039 helper cannot be reused: it only skips flat
// look/theme leaves and would leave the namespace in place.
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
// The twin keeps the folded description, which legitimately mentions
// `parallelCommits`; assert structurally that the config block and the
// namespace are gone (a substring scan of the whole source would misjudge the
// prose).
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
        if (line.starts_with("config:") || stripped.starts_with("gitGraph:") ||
            stripped.starts_with("parallelCommits:"))
            return false;
    }
    return true;
}
std::string parallel_line(std::string_view value) {
    return "config.gitGraph.parallelCommits=" + std::string(value) +
           " -> parallel lane compaction; not applied";
}
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  git parallelCommits boundary probe\n" + std::string(config) +
           "---\n" + std::string(kBody);
}
std::string config_probe(std::string_view config) {
    return probe(std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
// The footer block sits immediately above the fenced block's trailing blank
// row as one blank separator plus the ledger lines (header included).
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    std::vector<std::string> expected{"", std::string(kHeader)};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "G45_SENTINEL") << label;
    ASSERT_TRUE(lines[lines.size() - 2u].empty()) << label << " (trailing blank row)";
    const auto begin = lines.size() - 2u - expected.size();
    for (std::size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(lines[begin + i], expected[i]) << label << " footer line " << i;
}
std::vector<std::string> string_list(const JsonVal& value) {
    std::vector<std::string> out;
    value.iter([&](JsonVal item) { out.emplace_back(item.as_str()); });
    return out;
}
}  // namespace

TEST(MermaidGit0045Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_0045_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 45);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("originals"), 135);
    EXPECT_EQ(counts.get_int("drawn"), 70);
    EXPECT_EQ(counts.get_int("admitted"), 12);
    EXPECT_EQ(counts.get_int("remaining"), 53);
    const auto review = root.get("review");
    EXPECT_EQ(review.get_string("verdict"), "approved");
    EXPECT_FALSE(review.get_string("reviewer_agent").empty());

    // The admission table is the literal the battery below hardcodes.
    const auto admission = root.get("admission");
    EXPECT_EQ(string_list(admission.get("table").get("config.gitGraph.parallelCommits")),
              (std::vector<std::string>{"true", "false"}));
    EXPECT_EQ(admission.get("footer").get_int("line_limit_bytes"), 96);

    const auto selection = root.get("selection");
    EXPECT_EQ(selection.get("census").get_int("rfc0039"), 12);
    EXPECT_EQ(selection.get("census").get_int("base"), 58);
    EXPECT_EQ(selection.get("remaining_by_category").get_int("family_residual:git"), 34);
    EXPECT_EQ(selection.get("remaining_by_category").get_int("git_cherry_pick"), 12);
    EXPECT_EQ(selection.get("remaining_by_category").get_int("look_neo"), 6);
    EXPECT_EQ(selection.get("remaining_by_category").get_int("multiline_label"), 1);

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    // Upstream pin: the ledger must name the manifest it was measured against.
    const auto upstream = root.get("upstream");
    EXPECT_EQ(upstream.get_string("tag"), manifest.root().get("upstream").get_string("tag"));
    EXPECT_EQ(upstream.get_string("commit"),
              manifest.root().get("upstream").get_string("commit"));
    EXPECT_EQ(upstream.get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));

    std::vector<std::pair<std::string, std::string>> digest_input;
    std::set<std::string> admitted;
    int trues = 0, falses = 0;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        admitted.insert(rel);
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        auto footer = string_list(row.get("expected_footer_lines"));
        ASSERT_EQ(footer.size(), 2u) << rel;
        EXPECT_EQ(footer.front(), kHeader) << rel;
        EXPECT_TRUE(footer.back() == parallel_line("true") ||
                    footer.back() == parallel_line("false"))
            << rel;
        (footer.back() == parallel_line("true") ? trues : falses) += 1;
        // Synthetic twin: recompute it from the source and pin its sha. It must
        // draw — "both sides fall back equal" is not inertness.
        const auto twin = row.get("twin");
        ASSERT_EQ(twin.get_string("kind"), "synthetic") << rel;
        EXPECT_FALSE(twin.get_string("rule").empty()) << rel;
        const auto twin_source = config_free_twin(read_file(kCases / rel));
        EXPECT_EQ(loom::utils::crypto::sha256(twin_source), twin.get_string("sha256")) << rel;
        EXPECT_EQ(frontmatter_stripped(twin_source),
                  frontmatter_stripped(read_file(kCases / rel)))
            << rel << " body preserved";
        EXPECT_TRUE(twin_is_config_free(twin_source)) << rel << " twin still carries config";
        EXPECT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    std::set<std::string> pinned;
    selection.get("path_pin").iter(
        [&](JsonVal value) { pinned.insert(std::string(value.as_str())); });
    EXPECT_EQ(admitted, pinned);
    EXPECT_EQ(admitted.size(), 12u);
    EXPECT_EQ(trues, 10);
    EXPECT_EQ(falses, 2);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));

    // Sibling <-> RFC 0039 ledger: the frozen census still classes all 44
    // namespaced rows as exclusions (its per-row reasons predate this phase, so
    // `selection.census_note` declares the list frozen), the 12 admitted rows
    // are inside that class, and they are disjoint from the drawn rows.
    const auto ledger0039 = parse_doc(kLedger0039);
    EXPECT_EQ(ledger0039.root().get("counts").get_int("namespaced"), 44);
    EXPECT_FALSE(
        ledger0039.root().get("selection").get_string("census_note").empty());
    std::set<std::string> rows0039, excluded_ns;
    ledger0039.root().get("rows").iter(
        [&](JsonVal row) { rows0039.insert(row.get_string("path")); });
    ledger0039.root().get("excluded").iter([&](JsonVal entry) {
        if (entry.get_string("class") == "namespaced config.gitGraph.*")
            excluded_ns.insert(entry.get_string("path"));
    });
    for (const auto& rel : admitted) {
        EXPECT_TRUE(excluded_ns.contains(rel)) << rel;
        EXPECT_FALSE(rows0039.contains(rel)) << rel;
    }
}

TEST(MermaidGit0045Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin_source = config_free_twin(source);
        const auto states = load_states(row.get("expected_states"));

        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

        // Compose: twin frame (footer-free), plus the blank separator and the
        // source footer block. The twin's trailing blank rows are preserved.
        auto expected = trimmed_lines(capture(twin_source, "mermaid", kWide), kWide);
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "G45_SENTINEL") << rel;
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
        expected.emplace_back("G45_SENTINEL");
        const auto got = trimmed_lines(first, kWide);
        ASSERT_EQ(got.size(), expected.size()) << rel;
        for (std::size_t line = 0; line < got.size(); ++line) {
            ASSERT_EQ(got[line], expected[line])
                << rel << " line " << line << "\nGOT: " << got[line]
                << "\nEXP: " << expected[line];
        }
        ++checked;
    });
    EXPECT_EQ(checked, 12);
}

TEST(MermaidGit0045Cohort, NamespaceBattery) {
    for (const auto* value : {"true", "false"}) {
        const auto source =
            config_probe(std::string("  gitGraph:\n    parallelCommits: ") + value + "\n");
        ASSERT_TRUE(draws(source, kWide)) << value;
        ASSERT_TRUE(native_frame(source, kWide)) << value;
        expect_footer(source, {parallel_line(value)}, std::string("parallelCommits ") + value);
    }
    // git_inert_scalar normalization: a trailing comment and a quoted value are
    // the same boolean.
    for (const auto* form : {"  gitGraph:\n    parallelCommits: true # note\n",
                             "  gitGraph:\n    parallelCommits: \"true\"\n"}) {
        const auto source = config_probe(form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        EXPECT_TRUE(has_text(source, parallel_line("true"))) << form;
    }
    // Composition order: flat look/theme leaves first, the namespace last.
    {
        const auto source = config_probe(
            "  look: neo\n  theme: redux\n  gitGraph:\n    parallelCommits: true\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto text = capture(source, "mermaid", kWide).text;
        const auto look_at = text.find("config.look=neo -> source node style; not applied");
        const auto theme_at = text.find("config.theme=redux -> colour palette; not applied");
        const auto parallel_at = text.find(parallel_line("true"));
        ASSERT_NE(look_at, std::string::npos);
        ASSERT_NE(theme_at, std::string::npos);
        ASSERT_NE(parallel_at, std::string::npos);
        EXPECT_LT(look_at, theme_at);
        EXPECT_LT(theme_at, parallel_at);
    }
    // Closed shape and closed values. Both duplicate forms are rejected by the
    // envelope parser's `seen_paths` before the gate sees them; the gate's own
    // namespace/leaf bits remain a defensive second guard.
    for (const auto* form : {
             "  gitGraph:\n    parallelCommits: yes\n",
             "  gitGraph:\n    parallelCommits: 1\n",
             "  gitGraph:\n    parallelCommits: True\n",
             "  gitGraph:\n    parallelCommits:\n",
             "  gitGraph:\n    parallelCommits: true\n    parallelCommits: false\n",
             "  gitGraph:\n  gitGraph:\n    parallelCommits: true\n",
             "  gitGraph:\n",
             "  gitGraph: foo\n",
             "  gitGraph:\n    mainBranchName: trunk\n",
             "  gitGraph:\n    mainBranchOrder: 2\n",
             // D-424 admits the four closed booleans, so the unknown-leaf
             // negative here must use a leaf outside that table; the D-424
             // gate owns the showBranches positive/negative pair.
             "  gitGraph:\n    parallelCommits: true\n    showTags: false\n",
             "  gitGraph:\n    nested:\n      parallelCommits: true\n",
             "  mainBranchName: trunk\n",
             "  gitGraph.parallelCommits: true\n",
             "  flowchart:\n    htmlLabels: false\n",
         }) {
        EXPECT_FALSE(draws(config_probe(form), kWide)) << form;
    }
    EXPECT_FALSE(draws(probe("config:\n"), kWide)) << "empty config";
    // D-435 admits a frontmatter title (drawn first, see the D-435 gate); a
    // title after the config block still keeps the whole source.
    EXPECT_FALSE(draws("---\ndescription: >-\n  titled probe\nconfig:\n  look: neo\ntitle: "
                       "late\n---\n" +
                           std::string(kBody),
                       kWide))
        << "title after config stays unadmitted";
    EXPECT_FALSE(draws("---\ndescription: >-\n  x\nconfig:\n  gitGraph:\n    parallelCommits: "
                       "true\nconfig:\n  look: neo\n---\n" +
                           std::string(kBody),
                       kWide))
        << "second config";
}

TEST(MermaidGit0045Cohort, ExistingLeafBatteriesUnchanged) {
    // The RFC 0039 flat leaves keep their verdicts and line order.
    const auto look = config_probe("  look: neo\n");
    ASSERT_TRUE(draws(look, kWide));
    expect_footer(look, {"config.look=neo -> source node style; not applied"}, "look");
    const auto theme = config_probe("  theme: redux-dark-color\n");
    ASSERT_TRUE(draws(theme, kWide));
    expect_footer(theme, {"config.theme=redux-dark-color -> colour palette; not applied"},
                  "theme");
    for (const auto* form : {"  look: classic\n", "  theme: default\n",
                             "  fontFamily: courier\n", "  htmlLabels: false\n",
                             "  layout: elk\n",
                             "  gitGraph:\n    mainBranchName: trunk\n"}) {
        EXPECT_FALSE(draws(config_probe(form), kWide)) << form;
    }
    // A description-only envelope still draws without a footer.
    const auto plain = probe("");
    ASSERT_TRUE(draws(plain, kWide));
    EXPECT_FALSE(has_text(plain, std::string(kHeader)));
}

TEST(MermaidGit0045Cohort, LiteralPartition) {
    // Literal arithmetic against the filesystem, not a recomputation from the
    // ledger's own lists. The remaining-by-category split is a measured
    // constant: the committed fallback roster is a derived artifact refreshed
    // by `fallback_attribution.py --emit` at phase closure, so this gate does
    // not force roster sync (a forgotten `--emit` is caught by the roster
    // gates, not here).
    int files = 0;
    for (const auto& entry : fs::recursive_directory_iterator(kCases / "git"))
        if (entry.is_regular_file() && entry.path().extension() == ".mmd") ++files;
    EXPECT_EQ(files, 135);

    const auto ledger = parse_doc(kLedger);
    const auto counts = ledger.root().get("counts");
    EXPECT_EQ(counts.get_int("drawn") + counts.get_int("admitted") +
                  counts.get_int("remaining"),
              135);
    const auto selection = ledger.root().get("selection");
    const auto by_category = selection.get("remaining_by_category");
    int remaining = 0;
    for (const auto* key : {"family_residual:git", "git_cherry_pick", "look_neo",
                            "multiline_label"})
        remaining += static_cast<int>(by_category.get_int(key));
    EXPECT_EQ(remaining, 53);

    std::set<std::string> admitted;
    ledger.root().get("rows").iter(
        [&](JsonVal row) { admitted.insert(row.get_string("path")); });
    EXPECT_EQ(admitted.size(), 12u);
    const auto ledger0039 = parse_doc(kLedger0039);
    std::set<std::string> drawn;
    ledger0039.root().get("rows").iter(
        [&](JsonVal row) { drawn.insert(row.get_string("path")); });
    EXPECT_EQ(drawn.size(), 12u);
    for (const auto& rel : admitted) EXPECT_FALSE(drawn.contains(rel)) << rel;
    // 70 drawn = the 12 RFC 0039 cohort rows + 58 rows no admission ledger
    // enumerates.
    EXPECT_EQ(static_cast<int>(drawn.size()) + 58, counts.get_int("drawn"));
}
