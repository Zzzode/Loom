/// @file test_markdown_render_conformance.cpp
/// @brief Text-content property tests for the FTXUI terminal renderer,
///        driven by the CommonMark + GFM conformance fixtures.
///
/// The HTML conformance suite (test_markdown_conformance.cpp) tests the
/// HTML serializer; this file tests the production FTXUI renderer.  For
/// each fixture example we:
///   1. Render the markdown to an FTXUI element → Screen at 200 cols
///   2. Extract text via Screen::ToString()
///   3. Convert the expected HTML to plain text (strip tags, decode entities)
///   4. Assert every word in the expected text appears in the FTXUI output,
///      in the same order (word-level containment, not equality)
///
/// Containment (not equality) because the terminal renderer adds formatting
/// the HTML output lacks — list bullets (•), blockquote borders (│), HR
/// characters.  Word-level containment is robust against those while still
/// catching content-dropping bugs (the character-swallowing class).
///
/// Sections skipped where HTML and terminal rendering fundamentally differ:
/// Images, Raw HTML, HTML blocks, Disallowed Raw HTML, Link reference
/// definitions, <IGNORE> crash-test examples.

#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

import std;

import loom.ui.visual.markdown;
import loom.serdes.json;

namespace {

// ── Fixture path resolution ─────────────────────────────────────────

[[nodiscard]] std::string fixture_path(const char* name) {
    std::filesystem::path here(__FILE__);
    return (here.parent_path() / "fixtures" / name).string();
}

// ── HTML → plain text ───────────────────────────────────────────────
// Strip HTML tags, decode entities, convert block tags to newlines.
// Skips <img> entirely.  Preserves <pre> content whitespace verbatim.
// This is a simplified parser sufficient for the well-formed HTML in the
// conformance fixtures — not a general-purpose HTML parser.

[[nodiscard]] std::string decode_numeric_entity(std::string_view digits,
                                                  bool hex) {
    unsigned long codepoint = 0;
    try {
        codepoint = std::stoul(std::string(digits), nullptr, hex ? 16 : 10);
    } catch (...) {
        return "";
    }
    std::string out;
    if (codepoint < 0x80) {
        out += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        out += static_cast<char>(0xC0 | (codepoint >> 6));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        out += static_cast<char>(0xE0 | (codepoint >> 12));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codepoint >> 18));
        out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return out;
}

[[nodiscard]] std::string html_to_text(std::string_view html) {
    std::string out;
    bool in_pre = false;
    std::size_t i = 0;
    while (i < html.size()) {
        if (html[i] == '<') {
            const auto end = html.find('>', i);
            if (end == std::string_view::npos) break;
            const std::string_view tag(html.data() + i + 1, end - i - 1);

            // Extract tag name (lowercase, stop at space/slash/end)
            std::string tag_name;
            for (char c : tag) {
                if (c == ' ' || c == '\t' || c == '/' || c == '>') break;
                tag_name += static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));
            }

            // Block-level tags → newline
            if (tag_name == "p" || tag_name == "h1" || tag_name == "h2" ||
                tag_name == "h3" || tag_name == "h4" || tag_name == "h5" ||
                tag_name == "h6" || tag_name == "blockquote" ||
                tag_name == "ul" || tag_name == "ol" || tag_name == "li" ||
                tag_name == "table" || tag_name == "tr" || tag_name == "td" ||
                tag_name == "th" || tag_name == "hr" || tag_name == "div" ||
                tag_name == "br" || tag_name == "pre" || tag_name == "/pre" ||
                tag_name == "/p" || tag_name == "/h1" || tag_name == "/h2" ||
                tag_name == "/h3" || tag_name == "/h4" || tag_name == "/h5" ||
                tag_name == "/h6" || tag_name == "/blockquote" ||
                tag_name == "/ul" || tag_name == "/ol" || tag_name == "/li" ||
                tag_name == "/table" || tag_name == "/tr" ||
                tag_name == "/td" || tag_name == "/th" ||
                tag_name == "/div") {
                out += '\n';
            }

            // Skip images entirely
            if (tag_name == "img") {
                i = end + 1;
                continue;
            }

            // Track <pre> mode
            if (tag_name == "pre") in_pre = true;
            if (tag_name == "/pre") in_pre = false;

            i = end + 1;
        } else if (html[i] == '&') {
            // Decode entity
            const auto semi = html.find(';', i);
            if (semi != std::string_view::npos && semi - i < 12) {
                const std::string_view entity(html.data() + i + 1,
                                              semi - i - 1);
                std::string decoded;
                if (entity == "amp") decoded = "&";
                else if (entity == "lt") decoded = "<";
                else if (entity == "gt") decoded = ">";
                else if (entity == "quot") decoded = "\"";
                else if (entity == "apos" || entity == "#39") decoded = "'";
                else if (entity == "nbsp") decoded = " ";
                else if (entity.size() > 1 && entity[0] == '#') {
                    decoded = decode_numeric_entity(
                        entity.substr(1),
                        entity.size() > 1 &&
                            (entity[1] == 'x' || entity[1] == 'X'));
                } else {
                    // Unknown entity — keep as-is
                    decoded = std::string(html.substr(i, semi - i + 1));
                }
                out += decoded;
                i = semi + 1;
            } else {
                out += html[i];
                ++i;
            }
        } else {
            // Text content: preserve whitespace in <pre>, collapse elsewhere
            if (in_pre) {
                out += html[i];
            } else if (html[i] == ' ' || html[i] == '\t' ||
                       html[i] == '\n' || html[i] == '\r' ||
                       html[i] == '\f' || html[i] == '\v') {
                if (!out.empty() && out.back() != ' ' &&
                    out.back() != '\n') {
                    out += ' ';
                }
            } else {
                out += html[i];
            }
            ++i;
        }
    }
    return out;
}

// ── Strip ANSI escape codes ─────────────────────────────────────────
// Screen::ToString() includes SGR (ESC[...m) and OSC 8 hyperlink
// (ESC]8;;...ESC\) sequences.  Strip them so word matching sees only
// the visible text.

[[nodiscard]] std::string strip_ansi(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\x1b') {  // ESC
            if (i + 1 >= text.size()) {
                ++i;
                continue;
            }
            if (text[i + 1] == '[') {
                // CSI: ESC [ ... final byte (0x40–0x7E)
                i += 2;
                while (i < text.size()) {
                    const unsigned char c =
                        static_cast<unsigned char>(text[i]);
                    ++i;
                    if (c >= 0x40 && c <= 0x7E) break;
                }
            } else if (text[i + 1] == ']') {
                // OSC: ESC ] ... BEL or ESC-backslash
                i += 2;
                while (i < text.size()) {
                    if (text[i] == '\x07') {  // BEL
                        ++i;
                        break;
                    }
                    if (text[i] == '\x1b' && i + 1 < text.size() &&
                        text[i + 1] == '\\') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
            } else {
                i += 2;
            }
        } else {
            out += text[i];
            ++i;
        }
    }
    return out;
}

// ── Text normalization ──────────────────────────────────────────────
// Collapse whitespace runs to single spaces, trim leading/trailing.

[[nodiscard]] std::string normalize_text(std::string_view text) {
    std::string out;
    bool last_was_space = true;  // trim leading
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '\f' || c == '\v') {
            if (!last_was_space) {
                out += ' ';
                last_was_space = true;
            }
        } else {
            out += c;
            last_was_space = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// ── Word-level containment check ────────────────────────────────────
// Every word in `expected` appears in `actual`, in the same order.
// Robust against list bullets, blockquote borders, HR characters.

[[nodiscard]] std::vector<std::string> split_words(std::string_view text) {
    std::vector<std::string> words;
    std::size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() &&
               std::isspace(static_cast<unsigned char>(text[start]))) {
            ++start;
        }
        if (start >= text.size()) break;
        std::size_t end = start;
        while (end < text.size() &&
               !std::isspace(static_cast<unsigned char>(text[end]))) {
            ++end;
        }
        words.emplace_back(text.substr(start, end - start));
        start = end;
    }
    return words;
}

struct ContainmentResult {
    bool passed = true;
    std::string missing_word;
};

[[nodiscard]] ContainmentResult check_text_contains_words(
    std::string_view actual, std::string_view expected) {
    const auto words = split_words(expected);
    std::size_t pos = 0;
    for (const auto& word : words) {
        const auto found = actual.find(word, pos);
        if (found == std::string_view::npos) {
            return {false, word};
        }
        pos = found + word.size();
    }
    return {true, ""};
}

// ── Render to Screen ────────────────────────────────────────────────

[[nodiscard]] ftxui::Screen render_to_screen(
    std::string_view markdown, int width = 200, int height = 200) {
    auto element = loom::ui::render_markdown(markdown);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width),
        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, element);
    return screen;
}

// ── Escape for one-line printing ────────────────────────────────────

[[nodiscard]] std::string escape_for_print(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:   out += c; break;
        }
    }
    // Truncate very long strings
    if (out.size() > 200) {
        out.resize(200);
        out += "...";
    }
    return out;
}

// ── Skip sections ───────────────────────────────────────────────────

[[nodiscard]] bool is_skippable_section(std::string_view section) {
    // Sections where HTML and terminal rendering fundamentally differ
    if (section == "Images" ||
        section == "Raw HTML" ||
        section == "HTML blocks" ||
        section == "Disallowed Raw HTML (extension)" ||
        section == "Link reference definitions" ||
        section == "HTML tag filter" ||
        section == "Embedded HTML") {
        return true;
    }
    // Table sections: the FTXUI table renderer adds border characters
    // (┌─┬─┐│├─┼─┤) that break word matching.  Table content correctness
    // is covered by the layout tests in test_markdown_render_styles.cpp.
    if (section.starts_with("Table") ||
        section == "Embedded pipes" ||
        section == "Sequential cells" ||
        section == "a table can be recognised when separated from a "
                     "paragraph of text without an empty line") {
        return true;
    }
    // Footnote sections: the terminal renderer intentionally skips
    // footnote definitions (they are HTML-only back-reference targets).
    if (section.starts_with("Footnote") ||
        section.starts_with("When a footnote")) {
        return true;
    }
    return false;
}

// ── Suite runner ────────────────────────────────────────────────────

struct RenderFailure {
    int example = 0;
    std::string section;
    std::string markdown;
    std::string expected_text;
    std::string actual_text;
    std::string missing_word;
};

struct RenderSuiteReport {
    std::string name;
    int total = 0;
    int passed = 0;
    int skipped = 0;
    int floor = 0;
    std::vector<RenderFailure> failures;
    std::map<std::string, std::pair<int, int>> by_section;
};

[[nodiscard]] RenderSuiteReport run_render_suite(
    const std::string& path, std::string_view name, int floor) {
    RenderSuiteReport report;
    report.name = std::string(name);
    report.floor = floor;

    auto doc = loom::utils::json::parse_file_string(path);
    if (!doc) {
        ADD_FAILURE() << "Failed to load fixture " << path;
        return report;
    }
    const auto root = doc->root();
    if (!root.is_arr()) {
        ADD_FAILURE() << "Fixture root is not an array: " << path;
        return report;
    }

    for (std::size_t i = 0; i < root.size(); ++i) {
        const auto tc = root.at(i);
        const std::string markdown(tc.get("markdown").as_str());
        const std::string expected_html(tc.get("html").as_str());
        const int example = static_cast<int>(tc.get("example").as_int());
        const std::string section(tc.get("section").as_str());

        // Skip <IGNORE> crash-test examples
        if (expected_html == "<IGNORE>\n" || expected_html == "<IGNORE>") {
            ++report.skipped;
            continue;
        }

        // Skip sections where HTML and terminal rendering differ
        if (is_skippable_section(section)) {
            ++report.skipped;
            continue;
        }

        ++report.total;
        auto& section_stats = report.by_section[section];
        ++section_stats.second;  // total

        // Render to FTXUI and extract text (strip ANSI style codes)
        auto screen = render_to_screen(markdown);
        const std::string actual = normalize_text(strip_ansi(screen.ToString()));

        // Extract expected text from HTML
        const std::string expected = normalize_text(html_to_text(expected_html));

        // Word-level containment check
        const auto result = check_text_contains_words(actual, expected);
        if (result.passed) {
            ++report.passed;
            ++section_stats.first;  // passed
        } else {
            report.failures.push_back(
                {example, section, markdown, expected, actual,
                 result.missing_word});
        }
    }
    return report;
}

// ── Report printer ──────────────────────────────────────────────────

void print_render_report(const RenderSuiteReport& report,
                          std::size_t max_failures) {
    const double pct =
        report.total ? (100.0 * report.passed / report.total) : 0.0;
    std::cout << "\n── " << report.name << " ── " << report.passed << '/'
              << report.total << " passed (" << std::fixed
              << std::setprecision(1) << pct << "%), "
              << report.skipped << " skipped (floor: " << report.floor
              << ")\n";

    // Sections with failures, worst first
    std::vector<std::pair<std::string, std::pair<int, int>>> weak;
    for (const auto& [section, stats] : report.by_section) {
        if (stats.first < stats.second) {
            weak.push_back({section, stats});
        }
    }
    std::sort(weak.begin(), weak.end(),
              [](const auto& a, const auto& b) {
                  return (a.second.first - a.second.second) <
                         (b.second.first - b.second.second);
              });
    for (const auto& [section, stats] : weak) {
        std::cout << "  " << section << ": " << stats.first << '/'
                  << stats.second << '\n';
    }

    // Print failures
    const std::size_t shown = std::min(report.failures.size(), max_failures);
    for (std::size_t i = 0; i < shown; ++i) {
        const auto& f = report.failures[i];
        std::cout << "  FAIL example " << f.example << " (" << f.section
                  << "): missing word '" << f.missing_word << "'\n"
                  << "    markdown: " << escape_for_print(f.markdown) << '\n'
                  << "    expected: " << escape_for_print(f.expected_text)
                  << '\n'
                  << "    actual:   " << escape_for_print(f.actual_text)
                  << '\n';
    }
    if (report.failures.size() > max_failures) {
        std::cout << "  … and " << (report.failures.size() - max_failures)
                  << " more failures\n";
    }
}

// ── Floors ──────────────────────────────────────────────────────────
// Initial conservative floors.  Raise as renderer improves.

constexpr int kCommonMarkRenderFloor = 530;
constexpr int kGfmRenderFloor = 545;
constexpr int kGfmExtensionsRenderFloor = 12;

}  // namespace

TEST(MarkdownRenderConformance, CommonMarkTextContent) {
    auto report = run_render_suite(
        fixture_path("commonmark_spec.json"), "CommonMark 0.31.2 (render)",
        kCommonMarkRenderFloor);
    print_render_report(report, 25);
    EXPECT_GE(report.passed, kCommonMarkRenderFloor);
}

TEST(MarkdownRenderConformance, GfmTextContent) {
    auto report = run_render_suite(
        fixture_path("gfm_spec.json"), "GFM core (render)",
        kGfmRenderFloor);
    print_render_report(report, 25);
    EXPECT_GE(report.passed, kGfmRenderFloor);
}

TEST(MarkdownRenderConformance, GfmExtensionsTextContent) {
    auto report = run_render_suite(
        fixture_path("gfm_extensions.json"), "GFM extensions (render)",
        kGfmExtensionsRenderFloor);
    print_render_report(report, 30);
    EXPECT_GE(report.passed, kGfmExtensionsRenderFloor);
}
