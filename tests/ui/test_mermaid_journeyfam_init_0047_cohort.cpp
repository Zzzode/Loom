// RFC 0047 journey-family in-body %%{init}%% directive admission cohort gate.
//
// Public API only, mirroring tools/mermaid/journeyfam-init-0047-cohort.json
// against the live renderers. Two duties beyond the ledger pins:
//
//  * per admitted row the projected frame must equal the directive-free twin's
//    frame plus exactly the literal disclosure footer the ledger pins;
//  * the single-source boundary artifact tools/mermaid/journeyfam-init-0047-probes.json
//    drives this gate and the Python mirror on the same cases (efficiency
//    rule 37): expected_kind -> user-visible verdict here, and the same
//    expected_kind/expected_footer on the generator side.
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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journeyfam-init-0047-cohort.json";
const fs::path kProbes =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journeyfam-init-0047-probes.json";
const fs::path kJourneyLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journey-cohort.json";
const fs::path kTimelineLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/timeline-cohort.json";
const fs::path kGanttLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/gantt-cohort.json";
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
        if (line == "  look:" || line == "  theme:") return false;
        if (line == "timeline" || line == "timeline TD" || line == "gantt" || line == "journey")
            return false;
    }
    return got.back() == "EM_SENTINEL";
}
// The renderers' body split: the source after the closing fence.
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}
// The ledger's synthetic twin: the frontmatter and every leading directive
// line are deleted; blank and comment lines between directives are kept.
std::string directive_free_twin(const std::string& source) {
    const std::string body = frontmatter_stripped(source);
    std::vector<std::string> kept;
    std::string line;
    bool leading = true;
    bool dropping = false;
    const auto rtrimmed = [](std::string_view view) {
        while (!view.empty() && (view.back() == ' ' || view.back() == '\t'))
            view.remove_suffix(1);
        return view;
    };
    auto flush = [&]() {
        std::string_view view{line};
        if (!view.empty() && view.back() == '\r') view.remove_suffix(1);
        const auto first = view.find_first_not_of(' ');
        const auto trimmed = first == std::string_view::npos ? std::string_view{} : view.substr(first);
        if (dropping) {
            // D-454: delete the whole spanning directive, not just its opener.
            if (rtrimmed(trimmed).ends_with("}%%")) dropping = false;
            return;
        }
        if (leading) {
            if (trimmed.starts_with("%%{")) {
                // delete a leading directive (single-line or spanning)
                if (!rtrimmed(trimmed).ends_with("}%%")) dropping = true;
                return;
            }
            if (trimmed.empty() || trimmed.starts_with("%%"))
                ;  // keep blanks/comments, stay in the leading block
            else
                leading = false;
        }
        kept.push_back(line);
    };
    for (const char c : body) {
        if (c == '\n') {
            flush();
            line.clear();
        } else {
            line.push_back(c);
        }
    }
    flush();
    std::string out;
    for (const auto& kept_line : kept) {
        out += kept_line;
        out.push_back('\n');
    }
    return out;
}
void expect_footer(const std::string& source, const std::vector<std::string>& footer,
                   const std::string& label) {
    const auto lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
    ASSERT_GE(lines.size(), footer.size() + 2u) << label;
    ASSERT_EQ(lines.back(), "EM_SENTINEL") << label;
    std::size_t end = lines.size() - 1u;
    ASSERT_TRUE(lines[end - 1u].empty()) << label << " (expected a trailing blank row)";
    --end;
    const auto begin = end - footer.size();
    for (std::size_t i = 0; i < footer.size(); ++i)
        EXPECT_EQ(lines[begin + i], footer[i]) << label << " footer line " << i;
}
std::string theme_line(std::string_view value) {
    return "config.theme=" + std::string(value) + " -> colour palette; not applied";
}
std::string log_level_line(std::string_view value) {
    return "config.logLevel=" + std::string(value) + " -> logger only; not applied";
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}

constexpr std::string_view kTimelineBody =
    "timeline\n  title T\n  section S\n  2024 : Event one\n  2025 : Event two\n";
constexpr std::string_view kGanttBody =
    "gantt\n  title T\n  dateFormat YYYY-MM-DD\n  section S\n  Task one :a1, 2024-01-01, 2d\n";
constexpr std::string_view kJourneyBody =
    "journey\n  title T\n  section S\n    Task one: 3: Me\n";

std::vector<std::string> strings(const JsonVal& array) {
    std::vector<std::string> out;
    array.iter([&](const JsonVal value) { out.emplace_back(value.as_str()); });
    return out;
}
std::string assemble(const JsonVal& entry) {
    return entry.get_string("frontmatter") +
           [&] {
               std::string joined;
               bool first = true;
               entry.get("body").iter([&](const JsonVal line) {
                   if (!first) joined.push_back('\n');
                   joined += line.as_str();
                   first = false;
               });
               return joined;
           }() +
           "\n";
}
}  // namespace

TEST(MermaidJourneyFamInit0047Cohort, LedgerPinsAndProbeDigest) {
    const auto ledger = parse_doc(kLedger);
    EXPECT_EQ(ledger.root().get_string("schema"), "loom_mermaid_journeyfam_init_0047_cohort/v1");
    EXPECT_EQ(ledger.root().get_int("rfc"), 47);
    const auto selection = ledger.root().get("selection");
    EXPECT_EQ(selection.get("families").get_int("journey"), 4);
    EXPECT_EQ(selection.get("families").get_int("timeline"), 7);
    EXPECT_EQ(selection.get("families").get_int("gantt"), 0);
    EXPECT_EQ(ledger.root().get("counts").get_int("total"), 11);
    EXPECT_EQ(ledger.root().get("review").get_string("verdict"), "approved");
    // The review is bound to the authored probe artifact.
    const auto probes_sha = std::string(loom::utils::crypto::sha256(read_file(kProbes)));
    EXPECT_EQ(selection.get_string("probes_sha256"), probes_sha);
    EXPECT_EQ(ledger.root().get("review").get_string("probes_sha256"), probes_sha);
    // The cohort digest recomputes over path+sha, so a hand edit fails.
    std::string payload;
    std::size_t rows = 0;
    ledger.root().get("rows").iter([&](const JsonVal row) {
        ++rows;
        payload += row.get_string("path") + "\t" + row.get_string("sha256") + "\n";
        const auto prior = row.get("prior_status");
        EXPECT_EQ(prior.get_string("expected"), "source") << row.get_string("path");
        EXPECT_EQ(prior.get_string("reason"), "frontmatter") << row.get_string("path");
        EXPECT_EQ(row.get("twin").get_string("kind"), "synthetic");
        const auto footer = strings(row.get("expected_footer_lines"));
        ASSERT_FALSE(footer.empty()) << row.get_string("path");
        EXPECT_EQ(footer.front(), "") << row.get_string("path");
        ASSERT_GE(footer.size(), 2u) << row.get_string("path");
        EXPECT_NE(footer[1].find("native projection v1"), std::string::npos)
            << row.get_string("path");
    });
    EXPECT_EQ(rows, 11u);
    EXPECT_EQ(selection.get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidJourneyFamInit0047Cohort, RowsDrawTwinFramePlusFooter) {
    const auto ledger = parse_doc(kLedger);
    ledger.root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        const auto twin = directive_free_twin(source);
        ASSERT_TRUE(draws(twin, kWide)) << rel << " (the twin must itself draw)";
        EXPECT_EQ(has_text(twin, "native projection v1"), false) << rel << " twin footer";
        ASSERT_TRUE(draws(source, kWide)) << rel;
        EXPECT_TRUE(native_frame(source, kWide)) << rel << " native frame";
        EXPECT_TRUE(same_surface(capture(source, "mermaid", kWide),
                                 capture(source, "mermaid", kWide)))
            << rel << " restored determinism";
        const auto footer = strings(row.get("expected_footer_lines"));
        expect_footer(source, footer, rel);
        // The stronger oracle: the projected art must equal the twin's art
        // line for line, then the literal footer and the trailing blank row.
        const auto twin_lines = trimmed_lines(capture(twin, "mermaid", kWide), kWide);
        ASSERT_FALSE(twin_lines.empty()) << rel;
        ASSERT_EQ(twin_lines.back(), "EM_SENTINEL") << rel;
        std::vector<std::string> expected(twin_lines.begin(), twin_lines.end() - 1);
        // The fenced block leaves one blank row between the art and the next
        // block; the footer replaces it with its own leading blank separator.
        ASSERT_FALSE(expected.empty()) << rel;
        ASSERT_EQ(expected.back(), "") << rel;
        expected.pop_back();
        expected.insert(expected.end(), footer.begin(), footer.end());
        expected.emplace_back();
        const auto row_lines = trimmed_lines(capture(source, "mermaid", kWide), kWide);
        ASSERT_FALSE(row_lines.empty()) << rel;
        ASSERT_EQ(row_lines.back(), "EM_SENTINEL") << rel;
        const std::vector<std::string> actual(row_lines.begin(), row_lines.end() - 1);
        EXPECT_EQ(actual, expected) << rel << " frame = twin + footer";
        const auto states = row.get("expected_states");
        EXPECT_EQ(draws(source, kNarrow), states.get_string("narrow_40") == "drawn")
            << rel << " narrow ledger";
        EXPECT_EQ(states.get_string("wide_240"), "drawn") << rel;
        EXPECT_EQ(states.get_string("restored_240"), "drawn") << rel;
    });
}

// The single-source boundary artifact: expected_kind -> verdict, literally.
TEST(MermaidJourneyFamInit0047Cohort, ProbeArtifactDrivesTheGate) {
    const auto probes = parse_doc(kProbes);
    EXPECT_EQ(probes.root().get_string("schema"), "loom_journeyfam_init_0047_probes/v1");
    std::set<std::string> kinds;
    std::size_t entries = 0;
    probes.root().get("entries").iter([&](const JsonVal entry) {
        ++entries;
        const auto kind = entry.get_string("expected_kind");
        kinds.insert(kind);
        const auto family = entry.get_string("family");
        const auto source = assemble(entry);
        const auto label = family + ": " + entry.get_string("note");
        const auto footer = strings(entry.get("expected_footer"));
        if (kind == "themed") {
            EXPECT_FALSE(draws(source, kWide)) << label;
            return;
        }
        ASSERT_TRUE(draws(source, kWide)) << label;
        if (kind == "drawn") {
            EXPECT_FALSE(has_text(source, "native projection v1")) << label << " no footer";
        } else {
            ASSERT_FALSE(footer.empty()) << label;
            expect_footer(source, footer, label);
        }
    });
    EXPECT_EQ(entries, 73u);
    EXPECT_EQ(kinds.size(), 4u);
}

TEST(MermaidJourneyFamInit0047Cohort, DirectiveBatteryIsClosedPerFamily) {
    // Hardcoded literals, independent of the probe file and the ledger.
    const auto journey = [](std::string_view directive) {
        return std::string(directive) + "\n" + std::string(kJourneyBody);
    };
    const auto timeline = [](std::string_view directive) {
        return std::string(directive) + "\n" + std::string(kTimelineBody);
    };
    EXPECT_TRUE(draws(journey("%%{init}%%"), kWide));
    EXPECT_TRUE(draws(journey("%%{init: {}}%%"), kWide));
    ASSERT_TRUE(draws(journey("%%{init: {'logLevel': 0, 'theme': 'dark'}}%%"), kWide));
    expect_footer(journey("%%{init: {'logLevel': 0, 'theme': 'dark'}}%%"),
                  {"Journey native projection v1", log_level_line("0"),
                   theme_line("dark")},
                  "journey both keys");
    ASSERT_TRUE(draws(timeline("%%{init: {'logLevel': 'debug', 'theme': 'base'}}%%"), kWide));
    expect_footer(timeline("%%{init: {'logLevel': 'debug', 'theme': 'base'}}%%"),
                  {"Timeline native projection v1", log_level_line("debug"),
                   theme_line("base")},
                  "timeline both keys");
    ASSERT_TRUE(draws(journey("%%{init: {theme: 'dark'}}%%"), kWide));
    expect_footer(journey("%%{init: {theme: 'dark'}}%%"),
                  {"Journey native projection v1", theme_line("dark")}, "unquoted key");
    // A directives-only body passes the gate but the renderer has no header.
    EXPECT_FALSE(draws("%%{init}%%\n", kWide));
    EXPECT_FALSE(draws("%%{init: {'theme': 'dark'}}%%\n", kWide));
    for (const char* directive : {"%%{init: {'theme': 'neo'}}%%",
                                  "%%{init: {'theme': 'da,rk'}}%%",
                                  "%%{init: {'logLevel': 6}}%%",
                                  "%%{init: {'theme': 'dark', 'theme': 'dark'}}%%",
                                  "%%{init: {'flowchart': {'htmlLabels': true}}}%%",
                                  "%%{init: {'theme': 'dark'}}%% trailing",
                                  "%%{init : {'theme': 'dark'}}%%",
                                  "%%{wrap}%%",
                                  "%%{init: {'theme': 'dark}%%"})
        EXPECT_FALSE(draws(journey(directive), kWide)) << directive;
    // A non-leading theme directive still gates; a non-leading no-theme one
    // keeps the pass-through.
    EXPECT_FALSE(draws(std::string(kJourneyBody) + "%%{init: {'theme': 'dark'}}%%\n", kWide));
    EXPECT_TRUE(draws(std::string(kJourneyBody) + "%%{init}%%\n", kWide));
    // G5: a frontmatter leaf and a keyed directive are mutually exclusive; a
    // bare init alongside a frontmatter leaf stays admitted.
    const std::string redux = "---\nconfig:\n  theme: redux\n---\n";
    EXPECT_FALSE(draws(redux + "%%{init: {'theme': 'dark'}}%%\n" + std::string(kJourneyBody),
                       kWide));
    ASSERT_TRUE(draws(redux + "%%{init}%%\n" + std::string(kJourneyBody), kWide));
    expect_footer(redux + "%%{init}%%\n" + std::string(kJourneyBody),
                  {"Journey native projection v1", theme_line("redux")},
                  "journey frontmatter + bare init");
    // Gantt keeps the legacy switch: its theme directive gates but logLevel
    // alone stays an undisclosed pass-through.
    EXPECT_FALSE(draws("%%{init: {'theme': 'dark'}}%%\n" + std::string(kGanttBody), kWide));
    ASSERT_TRUE(draws("%%{init: {'logLevel': 0}}%%\n" + std::string(kGanttBody), kWide));
    EXPECT_FALSE(has_text("%%{init: {'logLevel': 0}}%%\n" + std::string(kGanttBody),
                          "native projection v1"));
}

TEST(MermaidJourneyFamInit0047Cohort, FamilyLedgersPartitionFiveWays) {
    const auto journey_doc = parse_doc(kJourneyLedger);
    const auto journey = journey_doc.root().get("selection");
    EXPECT_EQ(journey.get_int("clean_count"), 10);
    EXPECT_EQ(journey.get_int("admitted_count"), 4);
    EXPECT_EQ(journey.get_int("init_admitted_count"), 4);
    EXPECT_EQ(journey.get_int("themed_excluded_count"), 0);
    EXPECT_EQ(journey.get_int("clean_count") + journey.get_int("admitted_count") +
                  journey.get_int("init_admitted_count") +
                  journey.get_int("themed_excluded_count"),
              18);
    const auto timeline_doc = parse_doc(kTimelineLedger);
    const auto timeline = timeline_doc.root().get("selection");
    EXPECT_EQ(timeline.get_int("clean_count"), 7);
    EXPECT_EQ(timeline.get_int("admitted_count"), 16);
    // D-454: the two former themed rows join the init cohort.
    EXPECT_EQ(timeline.get_int("init_admitted_count"), 7);
    EXPECT_EQ(timeline.get_int("themed_excluded_count"), 0);
    EXPECT_EQ(timeline.get_int("clean_count") + timeline.get_int("admitted_count") +
                  timeline.get_int("init_admitted_count") +
                  timeline.get_int("themed_excluded_count"),
              30);
    const auto gantt_doc = parse_doc(kGanttLedger);
    const auto gantt = gantt_doc.root().get("selection");
    EXPECT_EQ(gantt.get_int("draw_count"), 39);
    EXPECT_EQ(gantt.get_int("admitted_count"), 11);
    EXPECT_EQ(gantt.get_int("themed_excluded_count"), 0);
    EXPECT_EQ(gantt.get_int("fail_closed_count"), 2);
    // The three init buckets are exactly the cohort rows, family by family.
    std::set<std::string> cohort;
    const auto ledger_doc = parse_doc(kLedger);
    ledger_doc.root().get("rows").iter(
        [&](const JsonVal row) { cohort.insert(row.get_string("path")); });
    std::set<std::string> buckets;
    for (const auto& [selection, family] : std::array<std::pair<JsonVal, std::string>, 2>{
             std::pair{journey, std::string("journey")},
             std::pair{timeline, std::string("timeline")}}) {
        for (const auto& path : strings(selection.get("init_admitted")))
            if (path.starts_with(family + "/")) buckets.insert(path);
    }
    EXPECT_EQ(buckets, cohort);
}
