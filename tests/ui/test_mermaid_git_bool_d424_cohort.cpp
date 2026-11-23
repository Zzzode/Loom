// D-424 gitGraph closed-boolean namespace gate.
//
// Selection rationale (TL directive: state it in the gate, not a design doc):
// of the gitGraph fallbacks, 18 originals carry `config.gitGraph` and *only*
// the four closed boolean leaves {showBranches, showCommitLabel,
// rotateCommitLabel, parallelCommits}, each with an exact true/false value —
// a semantic source-state marker, not a render difference. Expected drop: 18
// rows (git 53 -> 35, global 583 -> 565, formal 1376 -> 1394).
//
// Criterion. `showBranches: false` and `showCommitLabel: false` are honoured:
// the hidden element is genuinely absent from the drawn frame. `rotateCommitLabel`
// has no terminal analogue and `parallelCommits` is inert (RFC 0045), so both
// are disclosed only and their true/false pairs must render identical canvases.
// The absence oracle is structural, not a substring mnemonic: a commit label is
// text drawn adjacent to a commit marker (`●` reachable leftward across hatch
// glyphs and spaces). A branch reference such as `*@main → 5` carries a commit
// id that is *not* adjacent to a marker, which a naive digit scan would
// misjudge. Paired with the disclosed `true` twin, which must carry every
// explicit id adjacent to its marker.
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/git-bool-d424-cohort.json";
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
        "```" + std::string(language) + "\n" + source + "\n```\n\nG49_SENTINEL";
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
// `gitGraph`, so a substring mnemonic would misclassify a live frame.
bool native_frame(const std::string& source, int width) {
    const auto got = trimmed_lines(capture(source, "mermaid", width), width);
    if (got.empty()) return false;
    for (const auto& line : got) {
        if (line == "```mermaid") return false;
        if (line == "description:" || line == "config:") return false;
        if (line == "  gitGraph:" || line == "gitGraph:") return false;
        if (line == "    showBranches:" || line == "  showBranches:") return false;
        if (line == "gitGraph" || line.rfind("gitGraph ", 0) == 0) return false;
    }
    return got.back() == "G49_SENTINEL";
}
// The art above the disclosure footer (trailing blanks dropped).
std::vector<std::string> canvas_lines(const std::string& source) {
    auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    std::size_t header = lines.size();
    for (std::size_t i = lines.size(); i-- > 0;)
        if (lines[i] == kHeader) {
            header = i;
            break;
        }
    if (header == lines.size()) return lines;
    std::vector<std::string> out(lines.begin(), lines.begin() + static_cast<std::ptrdiff_t>(header));
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}
std::string joined(const std::vector<std::string>& lines) {
    std::string text;
    for (const auto& line : lines) text += line;
    return text;
}
std::string body_of(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    return close == std::string::npos ? source : source.substr(close + 5);
}
// The synthetic twin: delete the `config:` line and every following line
// indented at least two spaces, up to the next column-0 key.
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
        if (line.starts_with("config:")) return false;
    }
    return true;
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}
// The footer block sits immediately above the fenced block's trailing blank
// row as one blank separator plus the ledger lines (header included).
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    // `footer` starts with the header line (it is the ledger's disclosure
    // block); the frame prepends one blank separator row.
    std::vector<std::string> expected{""};
    expected.insert(expected.end(), footer.begin(), footer.end());
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), expected.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "G49_SENTINEL") << label;
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
// A glyph row of the whole 240x900 screen. `chars` stores one full glyph per
// cell, so multi-byte box art stays a single cell here.
std::vector<std::vector<std::string>> glyph_rows(const CellCapture& capture) {
    std::vector<std::vector<std::string>> rows;
    rows.reserve(static_cast<std::size_t>(kHeight));
    for (int y = 0; y < kHeight; ++y) {
        std::vector<std::string> row;
        row.reserve(static_cast<std::size_t>(kWide));
        for (int x = 0; x < kWide; ++x)
            row.push_back(capture.chars[static_cast<std::size_t>(y) * static_cast<std::size_t>(kWide) +
                                        static_cast<std::size_t>(x)]);
        rows.push_back(std::move(row));
    }
    return rows;
}
bool is_hatch(const std::string& glyph) {
    static const std::string kHatch = "─│┘┐└┌┬┴├┤";
    return glyph.size() > 1 && kHatch.find(glyph) != std::string::npos;
}
bool is_digit(const std::string& glyph) {
    return glyph.size() == 1 && glyph[0] >= '0' && glyph[0] <= '9';
}
// True when the cell at `col` is a commit caption: walking left across hatch
// glyphs and spaces reaches a commit marker. `*@main → 5` stops at `→` and is
// therefore not a caption.
bool caption_adjacent(const std::vector<std::string>& row, std::size_t col) {
    std::size_t i = col;
    while (i > 0) {
        const auto& glyph = row[i - 1];
        if (glyph == " " || is_hatch(glyph)) {
            --i;
            continue;
        }
        return glyph == "●";
    }
    return false;
}
bool any_digit_caption(const std::vector<std::vector<std::string>>& rows) {
    for (const auto& row : rows)
        for (std::size_t x = 0; x < row.size(); ++x)
            if (is_digit(row[x]) && caption_adjacent(row, x)) return true;
    return false;
}
bool id_is_caption(const std::vector<std::vector<std::string>>& rows, const std::string& id) {
    for (const auto& row : rows) {
        for (std::size_t x = 0; x + 1 <= row.size() && x < row.size(); ++x) {
            bool match = true;
            for (std::size_t k = 0; k < id.size(); ++k) {
                if (x + k >= row.size() || row[x + k].size() != 1 || row[x + k][0] != id[k]) {
                    match = false;
                    break;
                }
            }
            if (match && caption_adjacent(row, x)) return true;
        }
    }
    return false;
}
std::string probe(std::string_view config) {
    return "---\ndescription: >-\n  git closed-boolean boundary probe\n" + std::string(config) +
           "---\n" + std::string(kBody);
}
std::string config_probe(std::string_view leaves) {
    return probe(std::string("config:\n  gitGraph:\n") + std::string(leaves));
}
std::string footer_line(std::string_view key, std::string_view value) {
    if (key == "showBranches")
        return "config.gitGraph.showBranches=" + std::string(value) +
               " -> branch reference labels";
    if (key == "showCommitLabel")
        return "config.gitGraph.showCommitLabel=" + std::string(value) + " -> commit id labels";
    if (key == "rotateCommitLabel")
        return "config.gitGraph.rotateCommitLabel=" + std::string(value) +
               " -> label rotation; not applied";
    return "config.gitGraph.parallelCommits=" + std::string(value) +
           " -> parallel lane compaction; not applied";
}
}  // namespace

TEST(MermaidGitBoolD424Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_git_bool_d424_cohort/v1");
    EXPECT_EQ(root.get_string("decision"), "D-424");
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 18);
    EXPECT_EQ(counts.get_int("hidden_branches"), 4);
    EXPECT_EQ(counts.get_int("hidden_labels"), 4);
    const auto review = root.get("review");
    EXPECT_EQ(review.get_string("verdict"), "approved");
    EXPECT_FALSE(review.get_string("reviewer_agent").empty());

    const auto selection = root.get("selection");
    EXPECT_EQ(selection.get_string("cohort_digest"),
              "87d27aeb777b2ccf86a99bd90caaeeabe3d61265dbea9521fd036d5309c1fce2");
    for (const auto* key :
         {"config.gitGraph.showBranches", "config.gitGraph.showCommitLabel",
          "config.gitGraph.rotateCommitLabel", "config.gitGraph.parallelCommits"})
        EXPECT_EQ(string_list(selection.get("table").get(key)),
                  (std::vector<std::string>{"true", "false"}))
            << key;

    const auto manifest = parse_doc(kManifest);
    std::map<std::string, JsonVal> manifest_rows;
    manifest.root().get("fixtures").iter(
        [&](JsonVal fixture) { manifest_rows.emplace(fixture.get_string("path"), fixture); });
    const auto upstream = root.get("upstream");
    EXPECT_EQ(upstream.get_string("tag"), manifest.root().get("upstream").get_string("tag"));
    EXPECT_EQ(upstream.get_string("commit"), manifest.root().get("upstream").get_string("commit"));
    EXPECT_EQ(upstream.get_string("manifest_sha256"),
              loom::utils::crypto::sha256(read_file(kManifest)));

    std::vector<std::pair<std::string, std::string>> digest_input;
    int hidden_branches = 0, hidden_labels = 0;
    root.get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        auto it = manifest_rows.find(rel);
        ASSERT_NE(it, manifest_rows.end()) << rel;
        EXPECT_EQ(loom::utils::crypto::sha256(read_file(kCases / rel)),
                  it->second.get_string("sha256"))
            << rel;
        EXPECT_EQ(row.get_string("sha256"), it->second.get_string("sha256")) << rel;
        // The disclosure block is the header plus one line per declared leaf,
        // in the ledger's canonical order, and matches the declared values.
        const auto booleans = row.get("booleans");
        auto footer = string_list(row.get("expected_footer_lines"));
        ASSERT_EQ(footer.size(), 5u) << rel;
        EXPECT_EQ(footer.front(), kHeader) << rel;
        std::size_t index = 1;
        for (const auto* key : {"showBranches", "showCommitLabel", "rotateCommitLabel",
                                "parallelCommits"}) {
            EXPECT_EQ(footer[index], footer_line(key, booleans.get_string(key))) << rel << key;
            ++index;
        }
        hidden_branches += booleans.get_string("showBranches") == "false";
        hidden_labels += booleans.get_string("showCommitLabel") == "false";
        // The twin is recomputed from the source and must draw: "both sides
        // fall back equal" is not inertness.
        const auto twin_source = config_free_twin(read_file(kCases / rel));
        EXPECT_EQ(body_of(twin_source), body_of(read_file(kCases / rel))) << rel << " body";
        EXPECT_TRUE(twin_is_config_free(twin_source)) << rel << " twin still carries config";
        EXPECT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(digest_input.size(), 18u);
    EXPECT_EQ(hidden_branches, 4);
    EXPECT_EQ(hidden_labels, 4);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));
}

TEST(MermaidGitBoolD424Cohort, RowsDrawDiscloseAndMatchTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0, twins_exact = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto booleans = row.get("booleans");
        const bool hides = booleans.get_string("showBranches") == "false" ||
                           booleans.get_string("showCommitLabel") == "false";
        const auto twin_source = config_free_twin(source);
        const auto states = load_states(row.get("expected_states"));
        const auto footer = string_list(row.get("expected_footer_lines"));

        ASSERT_TRUE(draws(twin_source, kWide)) << rel << " (twin must draw)";
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        ASSERT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";
        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;
        expect_footer(source, footer, rel);

        if (!hides) {
            // The visible booleans are the defaults, so the drawn art equals
            // the config-free twin's art; only the disclosure block is added.
            auto expected = trimmed_lines(capture(twin_source, "mermaid", kWide), kWide);
            ASSERT_FALSE(expected.empty()) << rel;
            ASSERT_EQ(expected.back(), "G49_SENTINEL") << rel;
            expected.pop_back();
            std::size_t tail_blanks = 0;
            while (!expected.empty() && expected.back().empty()) {
                expected.pop_back();
                ++tail_blanks;
            }
            expected.emplace_back();
            for (const auto& line : footer) expected.emplace_back(line);
            for (std::size_t i = 0; i < tail_blanks; ++i) expected.emplace_back();
            expected.emplace_back("G49_SENTINEL");
            const auto got = trimmed_lines(first, kWide);
            ASSERT_EQ(got.size(), expected.size()) << rel;
            for (std::size_t line = 0; line < got.size(); ++line)
                ASSERT_EQ(got[line], expected[line])
                    << rel << " line " << line << "\nGOT: " << got[line]
                    << "\nEXP: " << expected[line];
            ++twins_exact;
        }
        ++checked;
    });
    EXPECT_EQ(checked, 18);
    // 18 rows: 7 hide something (four branch rows + four label rows, one row
    // overlapping), 11 render exactly as their config-free twin.
    EXPECT_EQ(twins_exact, 11);
}

TEST(MermaidGitBoolD424Cohort, HideBooleansHideRealElements) {
    // Group the cohort by body: each group is a true/false pair of the same
    // graph under the same namespace.
    std::map<std::string, std::vector<std::pair<std::string, JsonVal>>> groups;
    std::vector<std::pair<std::string, JsonVal>> rows;
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        groups[body_of(read_file(kCases / rel))].emplace_back(rel, row);
        rows.emplace_back(rel, row);
    });
    // Eight distinct bodies: six true/false pairs plus the 77/78/79/80/82
    // family (all five spell the same graph) and the lone 81 body.
    ASSERT_EQ(groups.size(), 8u);
    std::size_t paired = 0;
    for (const auto& [body, members] : groups) paired += members.size();
    EXPECT_EQ(paired, 18u);
    ASSERT_EQ(groups.at(body_of(read_file(
                  kCases /
                  "git/nightly-check/77-should-show-branch-lines-when-showbranches-is-true-default.mmd")))
                  .size(),
              5u);

    int branch_checks = 0, label_checks = 0, inert_pairs = 0;
    for (const auto& [rel, row] : rows) {
        const auto source = read_file(kCases / rel);
        const auto booleans = row.get("booleans");
        const auto semantics = row.get("semantics");
        const auto branch_names = string_list(semantics.get("branch_names"));
        const auto ids = string_list(semantics.get("commit_ids"));
        const auto canvas = canvas_lines(source);
        const auto canvas_text = joined(canvas);
        const auto glyphs = glyph_rows(capture(source, "mermaid", kWide));

        // Find the visible twin of the same body.
        const JsonVal* visible_twin = nullptr;
        for (const auto& [other_rel, other] : groups.at(body_of(source))) {
            if (other_rel == rel) continue;
            const auto other_booleans = other.get("booleans");
            if (other_booleans.get_string("showBranches") == "true" &&
                other_booleans.get_string("showCommitLabel") == "true")
                visible_twin = &other;
        }
        // A row that hides something must have a visible twin of the same
        // body to prove the hidden element was really there.
        const bool hides = booleans.get_string("showBranches") == "false" ||
                           booleans.get_string("showCommitLabel") == "false";
        if (hides) ASSERT_NE(visible_twin, nullptr) << rel;
        if (visible_twin == nullptr) {
            for (const auto& id : ids)
                EXPECT_TRUE(id_is_caption(glyphs, id)) << rel << " dropped id " << id;
            for (const auto& name : branch_names)
                EXPECT_NE(canvas_text.find(name + " → "), std::string::npos)
                    << rel << " dropped branch reference " << name;
            continue;
        }

        if (booleans.get_string("showBranches") == "false") {
            // Hidden: no branch reference text survives anywhere in the art.
            for (const auto& name : branch_names)
                EXPECT_EQ(canvas_text.find(name), std::string::npos)
                    << rel << " leaked branch reference " << name;
            // The visible twin really does render those references.
            const auto twin_text = joined(canvas_lines(read_file(kCases / (*visible_twin).get_string("path"))));
            for (const auto& name : branch_names)
                EXPECT_NE(twin_text.find(name + " → "), std::string::npos) << name;
            ++branch_checks;
        } else {
            for (const auto& name : branch_names)
                EXPECT_NE(canvas_text.find(name + " → "), std::string::npos)
                    << rel << " dropped branch reference " << name;
        }

        if (booleans.get_string("showCommitLabel") == "false") {
            // Hidden: no commit id text sits next to a commit marker. Digits
            // are scanned too, which also covers the implicit merge id (the
            // corpus renders a `0` for `merge ... tag:` even though the source
            // declares no explicit id for it).
            EXPECT_FALSE(any_digit_caption(glyphs)) << rel << " drew a commit id label";
            for (const auto& id : ids)
                EXPECT_FALSE(id_is_caption(glyphs, id)) << rel << " drew id " << id;
            const auto twin_glyphs =
                glyph_rows(capture(read_file(kCases / (*visible_twin).get_string("path")),
                                   "mermaid", kWide));
            for (const auto& id : ids)
                EXPECT_TRUE(id_is_caption(twin_glyphs, id)) << rel << " twin lost id " << id;
            ++label_checks;
        } else {
            for (const auto& id : ids)
                EXPECT_TRUE(id_is_caption(glyphs, id)) << rel << " dropped id " << id;
        }
    }
    EXPECT_EQ(branch_checks, 4);
    EXPECT_EQ(label_checks, 4);

    // Inert pairs: rotateCommitLabel and parallelCommits have no terminal
    // analogue, so their canvases are identical to the false twin's.
    for (const auto& pair : {std::pair{"91-should-render-with-rotatecommitlabel-set-to-true.mmd",
                                       "92-should-render-with-rotatecommitlabel-set-to-false.mmd"},
                             std::pair{"93-should-render-with-parallelcommits-set-to-true.mmd",
                                       "94-should-render-with-parallelcommits-set-to-false.mmd"}}) {
        const auto base = fs::path("git/nightly-check");
        const auto on = canvas_lines(read_file(kCases / base / pair.first));
        const auto off = canvas_lines(read_file(kCases / base / pair.second));
        ASSERT_FALSE(on.empty()) << pair.first;
        ASSERT_FALSE(off.empty()) << pair.second;
        EXPECT_EQ(on, off) << pair.first << " must not change the frame";
        ++inert_pairs;
    }
    EXPECT_EQ(inert_pairs, 2);
}

TEST(MermaidGitBoolD424Cohort, ClosedNamespaceNegativesStayWholeSource) {
    // Positives: each of the four leaves alone, both values, is admitted and
    // disclosed in canonical order.
    for (const auto* key : {"showBranches", "showCommitLabel", "rotateCommitLabel",
                            "parallelCommits"}) {
        for (const auto* value : {"true", "false"}) {
            const auto source =
                config_probe(std::string("    ") + key + ": " + value + "\n");
            ASSERT_TRUE(draws(source, kWide)) << key << "=" << value;
            ASSERT_TRUE(native_frame(source, kWide)) << key << "=" << value;
            expect_footer(source, {std::string(kHeader), footer_line(key, value)},
                          std::string(key) + "=" + value);
        }
    }
    // git_inert_scalar normalization: a trailing comment and a quoted value are
    // the same boolean.
    for (const auto* form : {"    showBranches: false # note\n",
                             "    showCommitLabel: \"false\"\n"}) {
        const auto source = config_probe(form);
        ASSERT_TRUE(draws(source, kWide)) << form;
        EXPECT_TRUE(has_text(source, footer_line(
                                        std::string(form).find("showBranches") != std::string::npos
                                            ? "showBranches"
                                            : "showCommitLabel",
                                        "false")))
            << form;
    }
    // Canonical disclosure order regardless of source order, and composition
    // with the flat look/theme leaves ahead of the namespace.
    {
        const auto source = probe(
            "config:\n  look: neo\n  gitGraph:\n    parallelCommits: true\n"
            "    rotateCommitLabel: true\n    showCommitLabel: false\n    showBranches: false\n");
        ASSERT_TRUE(draws(source, kWide));
        const auto text = capture(source, "mermaid", kWide).text;
        const auto look_at = text.find("config.look=neo -> source node style; not applied");
        const auto branches_at = text.find(footer_line("showBranches", "false"));
        const auto labels_at = text.find(footer_line("showCommitLabel", "false"));
        const auto rotate_at = text.find(footer_line("rotateCommitLabel", "true"));
        const auto parallel_at = text.find(footer_line("parallelCommits", "true"));
        ASSERT_NE(look_at, std::string::npos);
        ASSERT_NE(branches_at, std::string::npos);
        ASSERT_NE(labels_at, std::string::npos);
        ASSERT_NE(rotate_at, std::string::npos);
        ASSERT_NE(parallel_at, std::string::npos);
        EXPECT_LT(look_at, branches_at);
        EXPECT_LT(branches_at, labels_at);
        EXPECT_LT(labels_at, rotate_at);
        EXPECT_LT(rotate_at, parallel_at);
    }
    // Negatives: anything outside the closed boolean namespace stays
    // whole-source. Each is paired with a drawing control above.
    for (const auto* form : {
             "    colour: red\n",                     // unknown leaf (D-436 owns mainBranchName)
             "    showBranches: yes\n",              // not a boolean literal
             "    showBranches: TRUE\n",             // case-sensitive
             "    showBranches: 1\n",                // numeric
             "    showBranches:\n",                  // empty value
             "    showBranches: false\n    showBranches: true\n",  // duplicate
             "    nested:\n      showBranches: false\n",           // wrong indent
             "    showBranches: false\n     showBranches: true\n",  // 5-space indent
         }) {
        const auto source = config_probe(form);
        EXPECT_FALSE(draws(source, kWide)) << form;
        EXPECT_FALSE(native_frame(source, kWide)) << form;
    }
    // A namespace with no leaf, and a leaf outside the namespace.
    for (const auto* form : {"config:\n  gitGraph:\n", "config:\n  showBranches: false\n"}) {
        const auto source = probe(form);
        EXPECT_FALSE(draws(source, kWide)) << form;
    }
}

