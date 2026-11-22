// RFC 0048 phase P1 cohort gate: State concurrent regions.
//
// Public API only, mirroring tools/mermaid/state-region-0048-cohort.json
// against the live renderer. Three duties beyond the ledger pins:
//
//  * per admitted row the projected frame must equal the config-free twin's
//    frame plus exactly the literal disclosure footer the ledger pins;
//  * the region grammar's paired negatives must stay whole-source fallback:
//    a top-level divider, `A -- B`, an empty region, a cross-region edge, a
//    region-local direction, and more than kStateRegionLimit regions;
//  * a region-bearing composite must draw the divider column and both
//    junction glyphs on the frame the ledger declares.
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
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
const fs::path kLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-region-0048-cohort.json";
constexpr int kWide = 240;
constexpr int kNarrow = 40;
constexpr int kHeight = 400;

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
        if (line == "stateDiagram" || line == "stateDiagram-v2") return false;
    }
    return got.back() == "EM_SENTINEL";
}

// The ledger's synthetic twin. Phase P1 deletes the whole `config:` block from
// the frontmatter; phase P2 deletes the leading in-body directive line. Both
// keep the description and both fences.
std::string twin_source(const JsonVal& row) {
    const std::string frontmatter = row.get("twin").get_string("frontmatter");
    std::string body = [&] {
        const auto source = read_file(kCases / row.get_string("path"));
        const auto close = source.find("\n---\n", 4);
        return close == std::string::npos ? source : source.substr(close + 5);
    }();
    if (row.get_string("phase") == "P2-init-accessibility") {
        // Leading blank/comment lines stay; the directive lines go.
        std::string kept;
        std::size_t at = 0;
        bool leading = true;
        while (at <= body.size()) {
            const auto newline = body.find('\n', at);
            const std::string_view line =
                std::string_view(body).substr(at, newline == std::string::npos
                                                      ? std::string::npos
                                                      : newline - at);
            auto trimmed = line;
            while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
                trimmed.remove_prefix(1);
            if (leading) {
                if (trimmed.starts_with("%%{")) {
                    // delete the directive line
                } else if (!trimmed.empty() && !trimmed.starts_with("%%")) {
                    leading = false;
                    kept.append(line);
                    kept.push_back('\n');
                } else {
                    kept.append(line);
                    kept.push_back('\n');
                }
            } else {
                kept.append(line);
                kept.push_back('\n');
            }
            if (newline == std::string::npos) break;
            at = newline + 1;
        }
        body = std::move(kept);
    }
    return "---\n" + frontmatter + "\n---\n" + body;
}

std::vector<std::string> strings(const JsonVal& array) {
    std::vector<std::string> out;
    array.iter([&](const JsonVal value) { out.emplace_back(value.as_str()); });
    return out;
}

std::string state_body(const std::string& body) { return "stateDiagram-v2\n" + body; }
}  // namespace

TEST(MermaidStateRegion0048Cohort, LedgerPinsAndDigest) {
    const auto ledger = parse_doc(kLedger);
    EXPECT_EQ(ledger.root().get_string("schema"), "loom_mermaid_state_region_0048_cohort/v1");
    EXPECT_EQ(ledger.root().get_int("rfc"), 48);
    const auto selection = ledger.root().get("selection");
    EXPECT_EQ(selection.get("families").get_int("state-diagram"), 1);
    EXPECT_EQ(selection.get("families").get_int("state-diagram-v2"), 16);
    EXPECT_NE(ledger.root().get("counts").get_string("note").find("phase-P1"),
              std::string::npos);
    EXPECT_EQ(ledger.root().get("counts").get_int("total"), 25);
    EXPECT_EQ(ledger.root().get("counts").get_int("phase_p1"), 17);
    EXPECT_EQ(ledger.root().get("counts").get_int("phase_p2"), 8);
    EXPECT_EQ(ledger.root().get("review").get_string("verdict"), "approved");
    // The upstream pins match the immutable matrix manifest.
    const auto manifest_sha = std::string(loom::utils::crypto::sha256(
        read_file(kCases / "../manifest.json")));
    EXPECT_EQ(ledger.root().get("upstream").get_string("manifest_sha256"), manifest_sha);
    // The cohort digest recomputes over path+sha, so a hand edit fails.
    std::string payload;
    std::size_t rows = 0;
    std::set<std::string> paths;
    ledger.root().get("rows").iter([&](const JsonVal row) {
        ++rows;
        payload += row.get_string("path") + "\t" + row.get_string("sha256") + "\n";
        paths.insert(row.get_string("path"));
        EXPECT_TRUE(paths.size() == rows) << "duplicate cohort row";
        const auto prior = row.get("prior_status");
        EXPECT_EQ(prior.get_string("expected"), "source") << row.get_string("path");
        EXPECT_EQ(prior.get_string("reason"), "frontmatter") << row.get_string("path");
        EXPECT_EQ(row.get("twin").get_string("kind"), "synthetic");
        EXPECT_EQ(row.get_string("prediction"), "predicted-draw");
        const auto phase = row.get_string("phase");
        EXPECT_TRUE(phase == "P1-concurrent-regions" || phase == "P2-init-accessibility")
            << row.get_string("path");
        const auto footer = strings(row.get("expected_footer_lines"));
        if (footer.empty()) {
            // Only a config-free row may omit the disclosure footer.
            EXPECT_EQ(phase, "P1-concurrent-regions") << row.get_string("path");
            EXPECT_TRUE(row.get("config").is_obj()) << row.get_string("path");
            EXPECT_EQ(row.get("config").size(), 0u) << row.get_string("path");
        } else {
            EXPECT_EQ(footer.front(), "") << row.get_string("path");
            ASSERT_GE(footer.size(), 2u) << row.get_string("path");
            EXPECT_NE(footer[1].find("State native projection v1"), std::string::npos)
                << row.get_string("path");
        }
        // The ledger row's source digest must match the corpus byte-for-byte.
        EXPECT_EQ(row.get_string("sha256"),
                  std::string(loom::utils::crypto::sha256(read_file(
                      kCases / row.get_string("path")))))
            << row.get_string("path");
    });
    EXPECT_EQ(rows, 25u);
    EXPECT_EQ(selection.get_string("cohort_digest"), loom::utils::crypto::sha256(payload));
    // The directive grammar's review is bound to the single-source artifact.
    const auto probes_sha = std::string(loom::utils::crypto::sha256(
        read_file(fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-init-0048-probes.json")));
    EXPECT_EQ(selection.get_string("probes_sha256"), probes_sha);
}

TEST(MermaidStateRegion0048Cohort, RowsDrawTwinFramePlusFooter) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = twin_source(row);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (the twin must itself draw)";
        EXPECT_EQ(capture(twin, "mermaid", kWide).text.find("native projection v1"),
                  std::string::npos)
            << rel << " twin footer";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_TRUE(native_frame(source, kWide)) << rel << " native frame";
        // Render twice: the same source must produce the same cells (the
        // restored-width determinism check).
        EXPECT_TRUE(same_surface(capture(source, "mermaid", kWide),
                                 capture(source, "mermaid", kWide)))
            << rel << " render determinism";
        // The stronger oracle: the projected art must equal the twin's art
        // line for line, then the literal footer and the trailing blank row.
        // For the one config-free row the twin is the source itself and the
        // footer is empty, so this degenerates to a draw + determinism check;
        // its region evidence is the junction assertions below.
        const auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        ASSERT_FALSE(twin_lines.empty()) << rel;
        ASSERT_EQ(twin_lines.back(), "EM_SENTINEL") << rel;
        std::vector<std::string> expected(twin_lines.begin(), twin_lines.end() - 1);
        const auto footer = strings(row.get("expected_footer_lines"));
        if (!footer.empty()) {
            // The footer replaces the fenced block's trailing blank row with
            // its own leading separator, then adds the closing blank row.
            ASSERT_FALSE(expected.empty()) << rel;
            ASSERT_EQ(expected.back(), "") << rel;
            expected.pop_back();
            expected.insert(expected.end(), footer.begin(), footer.end());
            expected.emplace_back();
        }
        const auto row_lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_FALSE(row_lines.empty()) << rel;
        ASSERT_EQ(row_lines.back(), "EM_SENTINEL") << rel;
        const std::vector<std::string> actual(row_lines.begin(), row_lines.end() - 1);
        EXPECT_EQ(actual, expected) << rel << " frame = twin + footer";
        // The divider column and both junctions are user-visible.
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("┬"), std::string::npos)
            << rel << " top junction";
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("┴"), std::string::npos)
            << rel << " bottom junction";
        // `│` also appears on every rounded state border, so the junction
        // glyphs above are the specific evidence; this one only proves the
        // column is drawn.
        EXPECT_NE(capture(source, "mermaid", kWide).text.find("│"), std::string::npos)
            << rel << " divider column";
        const auto states = row.get("expected_states");
        EXPECT_EQ(states.get_string("wide_240"), "drawn") << rel;
        EXPECT_EQ(states.get_string("restored_240"), "drawn") << rel;
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow ledger";
    });
}

// The single-source probe artifact drives this gate and the Python mirror on
// the same cases (efficiency rule 37): expected_kind -> user-visible verdict,
// and the same expected_footer the ledger's generator recomputes.
TEST(MermaidStateRegion0048Cohort, ProbeArtifactDrivesTheGate) {
    const auto probes = parse_doc(fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/state-init-0048-probes.json");
    EXPECT_EQ(probes.root().get_string("schema"), "loom_state_init_0048_probes/v1");
    std::size_t entries = 0, admitted = 0, fallback = 0;
    probes.root().get("entries").iter([&](const JsonVal entry) {
        ++entries;
        std::string source = "---\n";
        entry.get("frontmatter").iter(
            [&](const JsonVal line) {
                source += std::string(line.as_str());
                source.push_back('\n');
            });
        source += "---\n";
        entry.get("body").iter([&](const JsonVal line) {
            source += std::string(line.as_str());
            source.push_back('\n');
        });
        const auto label = std::string(entry.get("label").as_str());
        const bool expect_admitted = entry.get_string("expected_kind") == "admitted";
        const bool drawn = draws(source, kWide);
        EXPECT_EQ(drawn, expect_admitted) << label;
        if (!expect_admitted) {
            ++fallback;
            EXPECT_TRUE(strings(entry.get("expected_footer")).empty()) << label;
            return;
        }
        ++admitted;
        const auto footer = strings(entry.get("expected_footer"));
        ASSERT_FALSE(footer.empty()) << label;
        const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_GE(lines.size(), footer.size() + 2u) << label;
        ASSERT_EQ(lines.back(), "EM_SENTINEL") << label;
        std::size_t end = lines.size() - 1u;
        ASSERT_TRUE(lines[end - 1u].empty()) << label << " trailing blank row";
        --end;
        const auto begin = end - footer.size();
        for (std::size_t i = 0; i < footer.size(); ++i)
            EXPECT_EQ(lines[begin + i], footer[i]) << label << " footer line " << i;
        // A directive must never surface as visible source text.
        EXPECT_EQ(capture(source, "mermaid", kWide).text.find("%%{"), std::string::npos)
            << label;
    });
    EXPECT_EQ(entries, 25u);
    // Eight directive-grammar positives plus one accessibility positive.
    EXPECT_EQ(admitted, 9u);
    EXPECT_EQ(fallback, 16u);
}

// Paired negatives: every shape the region grammar does not admit stays a
// whole-source fallback. Each case is a positive control plus the mutation.
TEST(MermaidStateRegion0048Cohort, GrammarNegativesStayWholeSource) {
    ASSERT_TRUE(draws(state_body("  state A {\n  s1\n  --\n  s2\n  }\n"), kWide))
        << "positive control must draw";
    for (const auto& [label, body] :
         std::vector<std::pair<std::string, std::string>>{
             {"top-level divider", "  --\n  [*] --> A\n"},
             {"inline divider operand", "  A -- B\n"},
             {"empty leading region", "  state A {\n  --\n  s2\n  }\n"},
             {"empty trailing region", "  state A {\n  s1\n  --\n  }\n"},
             {"two empty regions", "  state A {\n  s1\n  --\n  --\n  s2\n  }\n"},
             {"cross-region edge", "  state A {\n  s1\n  --\n  s2\n  s1 --> s2\n  }\n"},
             {"cross-region edge reversed", "  state A {\n  s1\n  --\n  s2\n  s2 --> s1\n  }\n"},
             {"cross-region through child", "  state A {\n  state B {\n  b1\n  }\n  --\n"
                                            "  s2\n  b1 --> s2\n  }\n"},
             {"direction after divider", "  state A {\n  s1\n  --\n  direction LR\n  s2\n  }\n"},
             {"nine regions",
              "  state A {\n  s1\n  --\n  s2\n  --\n  s3\n  --\n  s4\n  --\n  s5\n  --\n"
              "  s6\n  --\n  s7\n  --\n  s8\n  --\n  s9\n  }\n"},
         }) {
        EXPECT_FALSE(draws(state_body(body), kWide)) << label << " must stay whole-source";
    }
    // The bound itself is inclusive: exactly eight regions still draw.
    EXPECT_TRUE(draws(state_body("  state A {\n  s1\n  --\n  s2\n  --\n  s3\n  --\n  s4\n  --\n"
                                 "  s5\n  --\n  s6\n  --\n  s7\n  --\n  s8\n  }\n"),
                      kWide))
        << "eight regions must draw";
    // A direction declared before the first divider still belongs to the
    // whole composite and stays admitted.
    EXPECT_TRUE(draws(state_body("  state A {\n  direction LR\n  s1\n  --\n  s2\n  }\n"), kWide))
        << "pre-divider direction stays admitted";
    // A nested composite without dividers keeps its single-column body.
    EXPECT_TRUE(draws(state_body("  state A {\n  s1\n  state B {\n  b1\n  }\n  }\n"), kWide))
        << "plain nesting stays admitted";
}

// Disclosure negatives: the region mechanism must not widen the State config
// or directive surface. A row whose frontmatter carries an unadmitted leaf or
// an in-body directive still falls back even though its body has a divider.
TEST(MermaidStateRegion0048Cohort, ConfigSurfaceNegativesStayWholeSource) {
    const std::string body = "stateDiagram-v2\n  state A {\n  s1\n  --\n  s2\n  }\n";
    const std::string with_config =
        "---\ndescription: >-\n  region probe.\nconfig:\n  theme: neo\n---\n" + body;
    ASSERT_TRUE(draws(with_config, kWide)) << "admitted leaf positive control";
    const std::string unadmitted =
        "---\ndescription: >-\n  region probe.\nconfig:\n  theme: dark\n---\n" + body;
    EXPECT_FALSE(draws(unadmitted, kWide)) << "unadmitted theme must stay whole-source";
    const std::string leading_directive =
        "---\ndescription: >-\n  region probe.\n---\n%%{init: {'theme': 'dark'}}%%\n" + body;
    ASSERT_TRUE(draws(leading_directive, kWide)) << "leading directive positive control";
    const std::string directive =
        "---\ndescription: >-\n  region probe.\n---\n" + body + "%%{init: {'theme': 'dark'}}%%\n";
    EXPECT_FALSE(draws(directive, kWide)) << "non-leading directive stays whole-source";
    const std::string duplicate_divider =
        "stateDiagram-v2\n  state A {\n  s1\n  --\n  s2\n  }\n  state A {\n  s3\n  }\n";
    EXPECT_FALSE(draws(duplicate_divider, kWide)) << "redeclared composite stays whole-source";
}
