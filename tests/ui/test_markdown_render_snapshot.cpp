/// @file test_markdown_render_snapshot.cpp
/// @brief Golden file snapshot tests for the FTXUI terminal markdown renderer.
///
/// Two complementary suites:
///   1. TerminalSpecific — 4 hand-crafted cases covering behaviors the
///      conformance fixtures can't: CJK rendering, paragraph wrapping at
///      80 columns, long URL wrapping, and long code-line non-wrapping.
///   2. Conformance — 1353 examples from the CommonMark + GFM spec fixtures,
///      batch-generated (LOOM_UPDATE_SNAPSHOTS=1) and reviewed once.
///
/// Each case is rendered to a Screen at 80 columns, ANSI-stripped, trailing
/// whitespace trimmed, and compared byte-by-byte against a golden file under
/// tests/fixtures/render_snapshots/.
///
/// To regenerate golden files after an intentional rendering change:
///   LOOM_UPDATE_SNAPSHOTS=1 ctest --preset debug -R MarkdownRenderSnapshot
///
/// Golden files store ANSI-stripped plain text (readable in PRs).  Style
/// correctness is covered by Layer 2; this layer focuses on text content
/// and layout stability.

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

import std;

import loom.ui.visual.markdown;
import loom.serdes.json;

namespace {

namespace ui = loom::ui;
namespace fs = std::filesystem;

// ── Configuration ───────────────────────────────────────────────────

constexpr int kSnapshotWidth = 80;
constexpr int kSnapshotHeight = 500;

// ── ANSI stripping ──────────────────────────────────────────────────

// Strip CSI (ESC[...final) and OSC (ESC]...BEL or ESC]...ESC-backslash)
// escape sequences from Screen::ToString() output.
[[nodiscard]] std::string strip_ansi(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\x1b' && i + 1 < text.size()) {
            char next = text[i + 1];
            if (next == '[') {
                // CSI: ESC [ ... final byte (0x40-0x7E)
                i += 2;
                while (i < text.size() &&
                       !(text[i] >= 0x40 && text[i] <= 0x7e)) ++i;
                if (i < text.size()) ++i;  // skip final byte
                continue;
            }
            if (next == ']') {
                // OSC: ESC ] ... BEL or ESC-backslash
                i += 2;
                while (i < text.size()) {
                    if (text[i] == '\x07') { ++i; break; }
                    if (text[i] == '\x1b' && i + 1 < text.size() &&
                        text[i + 1] == '\\') { i += 2; break; }
                    ++i;
                }
                continue;
            }
        }
        out += text[i];
        ++i;
    }
    return out;
}

// ── Snapshot normalization ──────────────────────────────────────────

// Strip ANSI, trim trailing whitespace per line, remove empty trailing
// lines.  Produces clean, diff-friendly golden file content.
// Handles CRLF line endings (Screen::ToString uses \r\n on some platforms).
[[nodiscard]] std::string normalize_snapshot(std::string_view raw) {
    std::string clean = strip_ansi(raw);
    std::string result;
    std::string line;
    for (std::size_t i = 0; i < clean.size(); ++i) {
        if (clean[i] == '\n') {
            while (!line.empty() &&
                   (line.back() == ' ' || line.back() == '\t' ||
                    line.back() == '\r'))
                line.pop_back();
            result += line;
            result += '\n';
            line.clear();
        } else {
            line += clean[i];
        }
    }
    // Last line (Screen::ToString may not end with \n)
    while (!line.empty() &&
           (line.back() == ' ' || line.back() == '\t' ||
            line.back() == '\r'))
        line.pop_back();
    result += line;

    // Remove empty trailing lines
    while (!result.empty() && result.back() == '\n')
        result.pop_back();

    return result;
}

// ── Golden file path ────────────────────────────────────────────────

[[nodiscard]] fs::path golden_path(std::string_view name) {
    return fs::path(LOOM_TESTS_DIR) / "fixtures" /
           "render_snapshots" / (std::string(name) + ".txt");
}

// ── Snapshot comparison ─────────────────────────────────────────────

void expect_snapshot(std::string_view name, std::string_view markdown) {
    auto el = ui::render_markdown(markdown);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(kSnapshotWidth),
        ftxui::Dimension::Fixed(kSnapshotHeight));
    ftxui::Render(screen, el);

    std::string actual = normalize_snapshot(screen.ToString());
    fs::path path = golden_path(name);

    // Update mode: write golden file and pass
    if (std::getenv("LOOM_UPDATE_SNAPSHOTS") != nullptr) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path);
        out << actual << '\n';
        SUCCEED() << "Updated snapshot: " << name;
        return;
    }

    // Golden file must exist
    if (!fs::exists(path)) {
        FAIL() << "Golden file not found: " << path.filename().string()
               << "\nRun with LOOM_UPDATE_SNAPSHOTS=1 to generate it.";
    }

    // Read expected
    std::ifstream in(path);
    std::string expected((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    // Writer adds a trailing newline; strip it for comparison
    if (!expected.empty() && expected.back() == '\n')
        expected.pop_back();

    EXPECT_EQ(expected, actual)
        << "Snapshot mismatch for: " << name
        << "\nIf this is intentional, regenerate with:\n"
        << "  LOOM_UPDATE_SNAPSHOTS=1 ctest -R MarkdownRenderSnapshot";
}

// ── Test cases ──────────────────────────────────────────────────────

struct SnapshotCase {
    std::string_view name;
    std::string_view markdown;
};

// Terminal-specific cases that have no counterpart in the CommonMark/GFM
// conformance fixtures.  The fixtures cover every construct (headings,
// emphasis, lists, tables, ...) but contain no CJK text, no lines long
// enough to wrap at 80 columns, and no long URLs or code lines.  These
// four cases fill that gap.
constexpr std::array<SnapshotCase, 4> kCases = {{
    {"cjk",
     "这是一段中文文本，用于测试终端渲染器对 CJK 字符的处理。\n"
     "Chinese text should render correctly in the terminal.\n"},

    {"paragraph_wrap",
     "This is a longer paragraph that should wrap at the 80-column boundary. "
     "The quick brown fox jumps over the lazy dog. Pack my box with five "
     "dozen liquor jugs. How vexingly quick daft zebras jump!\n"},

    {"long_url",
     "Check out https://example.com/very/long/path/that/might/wrap/or/not "
     "for details.\n"},

    {"code_block_long",
     "```\n"
     "this is a very long code line that should not wrap at the 80-column boundary because code blocks do not wrap\n"
     "```\n"},
}};

// ── Conformance fixture snapshot runner ─────────────────────────────
// Iterates over every example in a conformance fixture JSON, renders each
// through the FTXUI renderer, and compares against a golden file named
// "<prefix>_<example>.txt".  These cover the full spec example set
// (1353 examples) — the golden files are batch-generated, not hand-written.
//
// Skip <IGNORE> crash-test examples (any output is acceptable per spec).

void run_conformance_snapshot(const char* fixture_name,
                               std::string_view prefix) {
    const auto path =
        fs::path(LOOM_TESTS_DIR) / "fixtures" / fixture_name;
    auto doc = loom::utils::json::parse_file_string(path.string());
    if (!doc) {
        FAIL() << "Failed to load fixture " << fixture_name;
        return;
    }
    const auto root = doc->root();
    if (!root.is_arr()) {
        FAIL() << "Fixture root is not an array: " << fixture_name;
        return;
    }

    for (std::size_t i = 0; i < root.size(); ++i) {
        const auto tc = root.at(i);
        const std::string markdown(tc.get("markdown").as_str());
        const std::string expected_html(tc.get("html").as_str());
        const int example = static_cast<int>(tc.get("example").as_int());

        // GFM spec uses <IGNORE> for crash-test examples — any output is
        // acceptable, so no golden file to compare against.
        if (expected_html == "<IGNORE>\n" || expected_html == "<IGNORE>")
            continue;

        const std::string name = std::format("{}_{}", prefix, example);
        SCOPED_TRACE(std::string(prefix) + " example " +
                     std::to_string(example));
        expect_snapshot(name, markdown);
    }
}

} // namespace

// ── Parameterized test ─────────────────────────────────────────────

class MarkdownRenderSnapshot : public ::testing::TestWithParam<SnapshotCase> {};

TEST_P(MarkdownRenderSnapshot, MatchesGolden) {
    const auto& c = GetParam();
    expect_snapshot(c.name, c.markdown);
}

INSTANTIATE_TEST_SUITE_P(
    TerminalSpecific, MarkdownRenderSnapshot,
    ::testing::ValuesIn(kCases),
    [](const ::testing::TestParamInfo<SnapshotCase>& info) {
        return std::string(info.param.name);
    });

// ── Conformance fixture snapshots ───────────────────────────────────
// Golden file snapshots for every example in the CommonMark + GFM spec
// fixtures.  These are batch-generated (LOOM_UPDATE_SNAPSHOTS=1) and
// reviewed once; they only change when the renderer intentionally changes.

TEST(MarkdownRenderSnapshotConformance, CommonMark) {
    run_conformance_snapshot("commonmark_spec.json", "cm");
}

TEST(MarkdownRenderSnapshotConformance, Gfm) {
    run_conformance_snapshot("gfm_spec.json", "gfm");
}

TEST(MarkdownRenderSnapshotConformance, GfmExtensions) {
    run_conformance_snapshot("gfm_extensions.json", "ext");
}
