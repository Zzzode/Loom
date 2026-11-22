// RFC 0040 journey-family (timeline + gantt + journey) look/theme admission
// cohort gate.
//
// Public API only, mirroring tools/mermaid/journeyfam-0040-cohort.json against
// the live renderers. Per admitted row the projected frame must equal the
// config-free twin's frame plus exactly the literal disclosure footer the
// ledger pins. Per admitted family the batteries below are an independent
// oracle: their expected verdicts and footer strings are hardcoded literals,
// never derived from the ledger or from the renderers' own tables, so a wrong
// table, a wrong reading position or a wrong footer in any one TU fails.
// The last test is the drawn-row frame-identity oracle over the three
// existing cohorts.
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
const fs::path kLedger =
    fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journeyfam-0040-cohort.json";
const fs::path kTimelineLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/timeline-cohort.json";
const fs::path kGanttLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/gantt-cohort.json";
const fs::path kJourneyLedger = fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journey-cohort.json";
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
// A native journey-family projection never echoes the fenced source envelope.
// Line equality only; bare family-name substrings are not valid (drawn bodies
// literally contain "Gantt"/"Journey" in their titles, and the new footers
// carry the family names).
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
// The synthetic config-free twin: drop every admitted visual leaf and an
// emptied `config:` block; the description payload is preserved verbatim.
std::string config_free_twin(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---", 3);
    if (close == std::string::npos) return source;
    std::vector<std::string> lines;
    std::string cur;
    for (char c : source.substr(0, close + 1)) {
        if (c == '\n') {
            lines.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    std::vector<std::string> kept;
    bool in_block = false, in_config = false, config_leaf = false;
    long config_line = -1;
    for (const auto& line : lines) {
        if (line.find_first_not_of(" \t") == std::string::npos) {
            kept.push_back(line);
            continue;
        }
        const auto start = line.find_first_not_of(' ');
        const bool top = start == 0u;
        const auto colon = line.find(':');
        const auto at = start == std::string::npos ? 0u : start;
        const auto key = colon == std::string::npos ? std::string{}
                                                    : line.substr(at, colon - at);
        const auto value = colon == std::string::npos ? std::string{} : line.substr(colon + 1);
        if (top) {
            in_block = false;
            in_config = false;
            if (key == "config") {
                in_config = true;
                config_line = static_cast<long>(kept.size());
                kept.push_back(line);
                continue;
            }
            const auto trimmed = value.find_first_not_of(" \t");
            const bool block_indicator =
                trimmed != std::string::npos && (value[trimmed] == '>' || value[trimmed] == '|');
            if (key == "description" && (trimmed == std::string::npos || block_indicator))
                in_block = true;
            kept.push_back(line);
            continue;
        }
        if (in_block) {
            kept.push_back(line);
            continue;
        }
        if (in_config && (key == "look" || key == "theme")) continue;
        if (in_config) config_leaf = true;
        kept.push_back(line);
    }
    if (!config_leaf && config_line >= 0) kept.erase(kept.begin() + config_line);
    std::string rebuilt;
    for (const auto& line : kept) {
        rebuilt += line;
        rebuilt.push_back('\n');
    }
    return rebuilt + source.substr(close + 1);
}
// What the renderers' body helpers return: the source after the closing fence.
std::string frontmatter_stripped(const std::string& source) {
    if (!source.starts_with("---\n")) return source;
    const auto close = source.find("\n---\n", 4);
    if (close == std::string::npos) return source;
    return source.substr(close + 5);
}

constexpr std::string_view kTimelineBody =
    "timeline\n  title T\n  section S\n  2024 : Event one\n  2025 : Event two\n";
constexpr std::string_view kGanttBody =
    "gantt\n  title T\n  dateFormat YYYY-MM-DD\n  section S\n  Task one :a1, 2024-01-01, 2d\n";
constexpr std::string_view kJourneyBody =
    "journey\n  title T\n  section S\n    Task one: 3: Me\n";

std::string probe(std::string_view body, std::string_view description,
                  std::string_view frontmatter_tail) {
    return "---\ndescription: >-\n  " + std::string(description) + "\n" + std::string(frontmatter_tail) +
           "---\n" + std::string(body);
}
std::string config_probe(std::string_view body, std::string_view config) {
    return probe(body, "journey-family boundary probe", std::string("config:\n") + std::string(config));
}
bool has_text(const std::string& source, const std::string& needle) {
    return capture(source, "mermaid", kWide).text.find(needle) != std::string::npos;
}

// RFC 0040 section 3: one hardcoded battery run against each family. Every
// expected verdict and footer string below is a literal, never loaded from the
// ledger and never derived from the renderer's own table, so each TU's grammar
// is pinned independently.
struct FamilyBattery {
    std::string_view name;
    std::string_view body;
    std::string_view header;
    std::string_view theme;                      // one admitted theme value
    std::vector<std::string_view> themes;        // every admitted theme value
    std::vector<std::string_view> looks;         // every admitted look value
    std::vector<std::string_view> sibling_themes;  // admitted by a sibling only
    std::vector<std::string_view> rejected_looks;  // out of this family's look table
};

std::string theme_line(std::string_view value) {
    return "config.theme=" + std::string(value) + " -> colour palette; not applied";
}
std::string look_line(std::string_view value) {
    return "config.look=" + std::string(value) + " -> source node style; not applied";
}
// Compare the footer block literally: blank separator, header, leaf lines in
// that order, immediately below the art and above the fenced block's trailing
// blank row (the sentinel follows it).
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
void run_battery(const FamilyBattery& family) {
    const std::string label(family.name);
    const auto themed = [&](std::string_view value) {
        return config_probe(family.body, "  theme: " + std::string(value) + "\n");
    };
    // Every admitted value draws with the exact theme-only footer.
    for (auto value : family.themes) {
        const auto source = themed(value);
        ASSERT_TRUE(draws(source, kWide)) << label << " theme: " << value;
        ASSERT_TRUE(native_frame(source, kWide)) << label << " theme: " << value;
        expect_footer(source, {"", std::string(family.header), theme_line(value)},
                      label + " theme: " + std::string(value));
    }
    // Every admitted look value, and look-before-theme footer order.
    for (auto value : family.looks) {
        const auto source =
            config_probe(family.body, "  look: " + std::string(value) + "\n");
        ASSERT_TRUE(draws(source, kWide)) << label << " look: " << value;
        expect_footer(source, {"", std::string(family.header), look_line(value)},
                      label + " look: " + std::string(value));
    }
    if (!family.looks.empty()) {
        const auto both = config_probe(family.body, "  look: " +
                                                         std::string(family.looks.front()) +
                                                         "\n  theme: " +
                                                         std::string(family.theme) + "\n");
        ASSERT_TRUE(draws(both, kWide)) << label << " look+theme";
        expect_footer(both, {"", std::string(family.header), look_line(family.looks.front()),
                             theme_line(family.theme)},
                      label + " look+theme order");
    }
    // Sibling-only values, unknown values, a case variant and an empty value.
    for (auto value : family.sibling_themes)
        EXPECT_FALSE(draws(themed(value), kWide)) << label << " sibling-only " << value;
    EXPECT_FALSE(draws(themed("chartreuse"), kWide)) << label << " unknown value";
    std::string case_variant(family.theme);
    case_variant[0] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(case_variant[0])));
    EXPECT_FALSE(draws(themed(case_variant), kWide)) << label << " case variant";
    EXPECT_FALSE(draws(config_probe(family.body, "  theme:\n"), kWide)) << label << " empty";
    // No family admits a look outside its own table.
    for (auto value : family.rejected_looks)
        EXPECT_FALSE(draws(config_probe(family.body, "  look: " + std::string(value) + "\n"),
                           kWide))
            << label << " look " << value;
    // A duplicate leaf, an unmatched quote, and the indent-0 / indent-4
    // spellings stay whole-source.
    EXPECT_FALSE(draws(config_probe(family.body, "  theme: " + std::string(family.theme) +
                                                  "\n  theme: " +
                                                  std::string(family.theme) + "\n"),
                       kWide))
        << label << " duplicate leaf";
    EXPECT_FALSE(
        draws(config_probe(family.body, "  theme: \"" + std::string(family.theme) + "\n"), kWide))
        << label << " unmatched quote";
    EXPECT_FALSE(
        draws(probe(family.body, "indent 0 probe", "theme: " + std::string(family.theme) + "\n"),
              kWide))
        << label << " indent 0";
    EXPECT_FALSE(draws(config_probe(family.body, "    theme: " + std::string(family.theme) + "\n"),
                       kWide))
        << label << " indent 4";
    // A quoted scalar and a trailing comment normalize to the same value.
    const auto quoted =
        config_probe(family.body, "  theme: \"" + std::string(family.theme) + "\"\n");
    ASSERT_TRUE(draws(quoted, kWide)) << label << " quoted";
    expect_footer(quoted, {"", std::string(family.header), theme_line(family.theme)},
                  label + " quoted footer");
    const auto commented = config_probe(family.body, "  theme: " + std::string(family.theme) +
                                                         " # note\n");
    ASSERT_TRUE(draws(commented, kWide)) << label << " trailing comment";
    expect_footer(commented, {"", std::string(family.header), theme_line(family.theme)},
                  label + " trailing comment footer");
    // A description payload line that looks like a visual key is a gate, and so
    // is an admitted leaf combined with an in-body initializer.
    EXPECT_FALSE(draws("---\ndescription: >-\n  theme: " + std::string(family.theme) + "\n---\n" +
                           std::string(family.body),
                       kWide))
        << label << " payload";
    EXPECT_FALSE(draws("---\ndescription: >-\n  body init probe\nconfig:\n  theme: " +
                           std::string(family.theme) +
                           "\n---\n%%{init: {\"theme\": \"dark\"}}%%\n" +
                           std::string(family.body),
                       kWide))
        << label << " admitted + body init";
    // `themeVariables:` is pass-through: alone it draws with no footer, and it
    // does not suppress an admitted sibling leaf (or add a line of its own).
    const auto passthrough =
        config_probe(family.body, "  themeVariables:\n    primaryColor: red\n");
    ASSERT_TRUE(draws(passthrough, kWide)) << label << " themeVariables";
    EXPECT_FALSE(has_text(passthrough, "native projection v1"))
        << label << " themeVariables footer";
    const auto mixed = config_probe(family.body, "  themeVariables:\n    primaryColor: red\n"
                                                 "  theme: " +
                                                     std::string(family.theme) + "\n");
    ASSERT_TRUE(draws(mixed, kWide)) << label << " themeVariables + theme";
    expect_footer(mixed, {"", std::string(family.header), theme_line(family.theme)},
                  label + " themeVariables + theme footer");
}
}  // namespace

TEST(MermaidJourneyFam0040Cohort, LedgerSchemaReviewsAndPins) {
    const auto ledger = parse_doc(kLedger);
    const auto& root = ledger.root();
    EXPECT_EQ(root.get_string("schema"), "loom_mermaid_journeyfam_0040_cohort/v1");
    EXPECT_EQ(root.get("rfc").as_int(), 40);
    const auto counts = root.get("counts");
    EXPECT_EQ(counts.get_int("total"), 31);
    EXPECT_EQ(counts.get_int("timeline"), 16);
    EXPECT_EQ(counts.get_int("gantt"), 11);
    EXPECT_EQ(counts.get_int("journey"), 4);
    const auto families = root.get("selection").get("families");
    EXPECT_EQ(families.get("timeline").get_int("admitted"), 16);
    EXPECT_EQ(families.get("gantt").get_int("admitted"), 11);
    EXPECT_EQ(families.get("journey").get_int("admitted"), 4);

    // The review verdict is approved only when the phase carries an approved
    // record; while pending the matrix branch must refuse the ledger.
    const auto reviews = root.get("review").get("phases");
    const std::string verdict = root.get("review").get_string("verdict");
    bool all_approved = true;
    for (const auto& phase : {"P1-look-theme"}) {
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

    // The three existing ledgers' `admitted` partitions must equal this
    // ledger's per-family row sets exactly (disjoint from their drawn rows).
    std::map<std::string, std::set<std::string>> admitted_by_family;
    std::map<std::string, std::set<std::string>> drawn_by_family;
    root.get("rows").iter([&](JsonVal row) {
        admitted_by_family[row.get_string("family")].insert(row.get_string("path"));
    });
    for (const auto& [ledger_path, family] :
         {std::pair{kTimelineLedger, "timeline"}, std::pair{kGanttLedger, "gantt"},
          std::pair{kJourneyLedger, "journey"}}) {
        const auto existing = parse_doc(ledger_path);
        std::set<std::string> admitted, drawn;
        existing.root().get("selection").get("admitted").iter(
            [&](JsonVal value) { admitted.insert(std::string(value.as_str())); });
        existing.root().get("rows").iter(
            [&](JsonVal row) { drawn.insert(row.get_string("path")); });
        EXPECT_EQ(admitted, admitted_by_family[family]) << family;
        for (const auto& rel : admitted) EXPECT_FALSE(drawn.contains(rel)) << family << " " << rel;
        EXPECT_EQ(existing.root().get("selection").get_string("admitted_digest"),
                  root.get("selection").get("families").get(family).get_string("digest"))
            << family;
    }

    int rows = 0;
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
        EXPECT_EQ(row.get("prior_status").get_string("expected"), "source") << rel;
        EXPECT_EQ(row.get("prior_status").get_string("reason"), "frontmatter") << rel;
        const auto states = load_states(row.get("expected_states"));
        EXPECT_EQ(states[0], "drawn") << rel;
        EXPECT_EQ(states[2], "drawn") << rel;
        EXPECT_EQ(row.get("twin").get_string("kind"), "synthetic") << rel;
        digest_input.emplace_back(rel, row.get_string("sha256"));
    });
    EXPECT_EQ(rows, 31);
    std::ranges::sort(digest_input);
    std::string payload;
    for (const auto& [rel, sha] : digest_input) payload += rel + "\t" + sha + "\n";
    EXPECT_EQ(root.get("selection").get_string("cohort_digest"),
              loom::utils::crypto::sha256(payload));
}

TEST(MermaidJourneyFam0040Cohort, RowsAreInertAgainstTwins) {
    const auto ledger = parse_doc(kLedger);
    int checked = 0;
    ledger.root().get("rows").iter([&](JsonVal row) {
        const auto rel = row.get_string("path");
        const auto source = read_file(kCases / rel);
        ASSERT_EQ(row.get("twin").get_string("kind"), "synthetic") << rel;
        const auto twin = config_free_twin(source);
        EXPECT_LT(twin.size(), source.size()) << rel;
        const auto states = load_states(row.get("expected_states"));

        ASSERT_TRUE(draws(twin, kWide)) << rel << " (twin must draw)";
        ASSERT_TRUE(native_frame(twin, kWide)) << rel;
        EXPECT_EQ(draws(source, kWide), states[0] == "drawn") << rel << " wide ledger state";
        EXPECT_TRUE(native_frame(source, kWide)) << rel;
        EXPECT_EQ(draws(source, kNarrow), states[1] == "drawn") << rel << " narrow ledger state";
        EXPECT_EQ(draws(source, kWide), states[2] == "drawn") << rel << " restored ledger state";

        const CellCapture first = capture(source, "mermaid", kWide);
        EXPECT_TRUE(same_surface(first, capture(source, "mermaid", kWide))) << rel;

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
    EXPECT_EQ(checked, 31);
}

TEST(MermaidJourneyFam0040Cohort, TimelineTablesAreClosed) {
    run_battery({"timeline", kTimelineBody, "Timeline native projection v1", "dark",
                 {"dark", "default", "forest", "neo", "neo-dark", "neutral", "redux",
                  "redux-dark", "redux-color", "redux-dark-color"},
                 {"classic", "neo"}, {}, {"handDrawn"}});
}

TEST(MermaidJourneyFam0040Cohort, GanttTablesAreClosed) {
    run_battery({"gantt", kGanttBody, "Gantt native projection v1", "dark",
                 {"dark", "default", "forest", "neutral", "redux", "redux-dark", "redux-color",
                  "redux-dark-color"},
                 {}, {"neo", "neo-dark"}, {"classic", "neo", "handDrawn"}});
    // The gate reads sanitized bytes: `dark<br/>` normalizes to `dark`.
    const auto sanitized = config_probe(kGanttBody, "  theme: dark<br/>\n");
    ASSERT_TRUE(draws(sanitized, kWide));
    expect_footer(sanitized, {"", "Gantt native projection v1", theme_line("dark")},
                  "gantt theme: dark<br/>");
    // The two malformed-date fixtures stay whole source (carried by the
    // existing cohort ledger; asserted here through its fail_closed list).
    const auto existing = parse_doc(kGanttLedger);
    existing.root().get("selection").get("fail_closed").iter([&](JsonVal value) {
        EXPECT_FALSE(draws(read_file(kCases / std::string(value.as_str())), kWide))
            << value.as_str();
    });
}

TEST(MermaidJourneyFam0040Cohort, JourneyTablesAreClosed) {
    run_battery({"journey", kJourneyBody, "Journey native projection v1", "redux",
                 {"redux", "redux-dark", "redux-color", "redux-dark-color"}, {},
                 {"dark", "default", "forest", "neutral", "neo", "neo-dark"},
                 {"classic", "neo", "handDrawn"}});
    // RFC 0047 admitted the four remaining journey fallbacks (in-body init
    // directives); they are now positive rows of the sibling cohort, which is
    // also the frame oracle for them. The family bucket must be empty.
    const auto existing = parse_doc(kJourneyLedger);
    EXPECT_EQ(existing.root().get("selection").get_int("themed_excluded_count"), 0);
    EXPECT_EQ(existing.root().get("selection").get_int("init_admitted_count"), 4);
    const auto admitted_0047 = parse_doc(
        fs::path(LOOM_TESTS_DIR) / "../tools/mermaid/journeyfam-init-0047-cohort.json");
    std::size_t journey_rows = 0;
    admitted_0047.root().get("rows").iter([&](const JsonVal row) {
        const auto rel = row.get_string("path");
        if (!rel.starts_with("journey/")) return;
        ++journey_rows;
        ASSERT_TRUE(draws(read_file(kCases / rel), kWide)) << rel;
        EXPECT_TRUE(native_frame(read_file(kCases / rel), kWide)) << rel;
    });
    EXPECT_EQ(journey_rows, 4u);
}

// The three renderers must not drift: one probe battery, three hardcoded
// expected verdicts, all literal.
TEST(MermaidJourneyFam0040Cohort, CrossFamilyVerdictsAgree) {
    struct Case {
        const char* family;
        std::string_view body;
        const char* config;
        bool draws;
    };
    const Case cases[] = {
        {"timeline", kTimelineBody, "  theme: neo\n", true},
        {"timeline", kTimelineBody, "  look: handDrawn\n", false},
        {"timeline", kTimelineBody, "  theme: base\n", false},
        {"gantt", kGanttBody, "  theme: neo\n", false},
        {"gantt", kGanttBody, "  theme: neo-dark\n", false},
        {"gantt", kGanttBody, "  theme: dark\n", true},
        {"journey", kJourneyBody, "  theme: dark\n", false},
        {"journey", kJourneyBody, "  theme: default\n", false},
        {"journey", kJourneyBody, "  theme: forest\n", false},
        {"journey", kJourneyBody, "  theme: neutral\n", false},
        {"journey", kJourneyBody, "  theme: neo\n", false},
        {"journey", kJourneyBody, "  theme: neo-dark\n", false},
        {"journey", kJourneyBody, "  theme: redux\n", true},
    };
    for (const auto& probe_case : cases)
        EXPECT_EQ(draws(config_probe(probe_case.body, probe_case.config), kWide),
                  probe_case.draws)
            << probe_case.family << " " << probe_case.config;
    // The three footer headers stay family-specific literals.
    EXPECT_TRUE(has_text(config_probe(kTimelineBody, "  theme: dark\n"),
                         "Timeline native projection v1"));
    EXPECT_TRUE(has_text(config_probe(kGanttBody, "  theme: dark\n"),
                         "Gantt native projection v1"));
    EXPECT_TRUE(has_text(config_probe(kJourneyBody, "  theme: redux\n"),
                         "Journey native projection v1"));
}

// Regression oracle: every row the three existing ledgers already draw must
// render identically with and without its frontmatter. A footer or blank
// separator emitted merely because a frontmatter is present breaks the full
// untrimmed surface equality (the sentinel shifts), and no footer marker may
// appear.
TEST(MermaidJourneyFam0040Cohort, DrawnRowsKeepFrameIdentity) {
    int total = 0;
    for (const auto& [ledger_path, expected_rows] :
         {std::pair{kTimelineLedger, 7}, std::pair{kGanttLedger, 39}, std::pair{kJourneyLedger, 10}}) {
        const auto ledger = parse_doc(ledger_path);
        int checked = 0;
        ledger.root().get("rows").iter([&](JsonVal row) {
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
            for (const auto* marker : {"Timeline native projection v1", "Gantt native projection v1",
                                       "Journey native projection v1", "config.look=",
                                       "config.theme="})
                EXPECT_EQ(with.text.find(marker), std::string::npos) << rel << " " << marker;
            ++checked;
        });
        EXPECT_EQ(checked, expected_rows) << ledger_path;
        total += checked;
    }
    EXPECT_EQ(total, 56);
}
