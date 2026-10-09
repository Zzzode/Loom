/// @file test_markdown_render_styles.cpp
/// @brief Style and layout tests for the FTXUI terminal markdown renderer.
///
/// Layer 2 (style): verify the renderer applies correct FTXUI style flags
/// (bold, dim, hyperlink, strikethrough, colors) for each markdown construct.
/// Uses PixelAt() for direct pixel-level style inspection — no ANSI parsing.
///
/// Layer 3 (layout): verify layout behavior (wrapping, indentation, nesting,
/// blockquote borders, code block non-wrapping, table alignment).  Uses
/// Screen::ToString() with ANSI stripped for line-level assertions.
///
/// The conformance suite (test_markdown_render_conformance.cpp) verifies
/// text content; this file verifies presentation.

#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

import std;

import loom.ui.visual.markdown;

namespace {

namespace ui = loom::ui;

// ── Render helpers ──────────────────────────────────────────────────

// Render markdown to a Screen at the given dimensions.
[[nodiscard]] ftxui::Screen render_to_screen(
        std::string_view markdown, int width = 200, int height = 50) {
    auto el = ui::render_markdown(markdown);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width),
        ftxui::Dimension::Fixed(height));
    ftxui::Render(screen, el);
    return screen;
}

// ── Pixel lookup ────────────────────────────────────────────────────

struct PixelRef {
    int x = 0;
    int y = 0;
    const ftxui::Pixel* pixel = nullptr;
};

// Find the first pixel whose character content contains the given text.
// Searches row by row, handling multi-byte UTF-8 characters and automerged
// pixels (one pixel may hold several characters).  Returns the pixel at the
// start of the match.
[[nodiscard]] std::optional<PixelRef> find_pixel_with_text(
        const ftxui::Screen& screen, std::string_view text) {
    for (int y = 0; y < screen.dimy(); ++y) {
        std::string row_text;
        std::vector<int> byte_to_x;
        byte_to_x.reserve(static_cast<std::size_t>(screen.dimx()) * 4);
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& ch = screen.PixelAt(x, y).character;
            for (std::size_t i = 0; i < ch.size(); ++i) {
                byte_to_x.push_back(x);
            }
            row_text += ch;
        }
        auto pos = row_text.find(text);
        if (pos != std::string::npos) {
            int x = byte_to_x[pos];
            return PixelRef{x, y, &screen.PixelAt(x, y)};
        }
    }
    return std::nullopt;
}

// Check whether any pixel in the screen has a non-default foreground color.
[[nodiscard]] bool has_colored_pixel(const ftxui::Screen& screen) {
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            if (screen.PixelAt(x, y).foreground_color !=
                ftxui::Color::Default) {
                return true;
            }
        }
    }
    return false;
}

// ── ANSI stripping ──────────────────────────────────────────────────

// Strip CSI (ESC[...final) and OSC (ESC]...BEL or ESC]...ESC-backslash)
// escape sequences from Screen::ToString() output.  SGR codes and OSC 8
// hyperlinks would otherwise break text comparison.
[[nodiscard]] std::string strip_ansi(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\x1b' && i + 1 < text.size()) {
            char next = text[i + 1];
            if (next == '[') {
                // CSI: ESC [ ... final byte (0x40–0x7E)
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

// ── Line helpers ────────────────────────────────────────────────────

[[nodiscard]] int count_nonempty_lines(const std::string& s) {
    int n = 0;
    std::istringstream is(s);
    std::string line;
    while (std::getline(is, line)) {
        if (line.find_first_not_of(' ') != std::string::npos) ++n;
    }
    return n;
}

[[nodiscard]] std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream is(s);
    std::string line;
    while (std::getline(is, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Find byte offsets of a UTF-8 character sequence in a line.
[[nodiscard]] std::vector<std::size_t> find_utf8_positions(
        std::string_view line, std::string_view utf8_char) {
    std::vector<std::size_t> positions;
    std::size_t pos = 0;
    while ((pos = line.find(utf8_char, pos)) != std::string_view::npos) {
        positions.push_back(pos);
        pos += utf8_char.size();
    }
    return positions;
}

// Count leading spaces in a string.
[[nodiscard]] std::size_t leading_spaces(std::string_view s) {
    std::size_t n = 0;
    while (n < s.size() && s[n] == ' ') ++n;
    return n;
}

} // namespace

// ════════════════════════════════════════════════════════════════════
// Layer 2: Style tests
// ════════════════════════════════════════════════════════════════════

TEST(MarkdownRenderStyles, BoldStyle) {
    auto screen = render_to_screen("This is **bold** text");
    auto ref = find_pixel_with_text(screen, "bold");
    ASSERT_TRUE(ref.has_value()) << "bold text not found in rendered output";
    EXPECT_TRUE(ref->pixel->bold)
        << "bold markdown did not produce bold pixel style";
}

TEST(MarkdownRenderStyles, ItalicStyle) {
    // FTXUI has no italic Screen style; the renderer uses dim as the
    // closest visual proxy.
    auto screen = render_to_screen("This is *italic* text");
    auto ref = find_pixel_with_text(screen, "italic");
    ASSERT_TRUE(ref.has_value()) << "italic text not found in rendered output";
    EXPECT_TRUE(ref->pixel->dim)
        << "italic markdown did not produce dim pixel style";
}

TEST(MarkdownRenderStyles, InlineCodeColor) {
    auto screen = render_to_screen("Use `std::cout` for output");
    auto ref = find_pixel_with_text(screen, "std::cout");
    ASSERT_TRUE(ref.has_value()) << "inline code not found in rendered output";
    EXPECT_NE(ref->pixel->foreground_color, ftxui::Color::Default)
        << "inline code did not produce a colored pixel";
}

TEST(MarkdownRenderStyles, LinkHyperlink) {
    auto screen = render_to_screen(
        "Visit [Example](https://example.com) for details");
    auto ref = find_pixel_with_text(screen, "Example");
    ASSERT_TRUE(ref.has_value()) << "link text not found in rendered output";
    EXPECT_NE(ref->pixel->hyperlink, 0)
        << "link text did not produce a hyperlink pixel";
    if (ref->pixel->hyperlink != 0) {
        EXPECT_EQ(screen.Hyperlink(ref->pixel->hyperlink),
                  "https://example.com")
            << "hyperlink URL mismatch";
    }
    EXPECT_TRUE(ref->pixel->underlined)
        << "link text did not produce underlined pixel style";
}

TEST(MarkdownRenderStyles, StrikethroughStyle) {
    auto screen = render_to_screen("This is ~~deleted~~ text");
    auto ref = find_pixel_with_text(screen, "deleted");
    ASSERT_TRUE(ref.has_value())
        << "strikethrough text not found in rendered output";
    EXPECT_TRUE(ref->pixel->strikethrough)
        << "strikethrough markdown did not produce strikethrough pixel style";
}

TEST(MarkdownRenderStyles, DimOption) {
    auto el = ui::render_markdown_dim("some dimmed text");
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(200), ftxui::Dimension::Fixed(3));
    ftxui::Render(screen, el);
    auto ref = find_pixel_with_text(screen, "dimmed");
    ASSERT_TRUE(ref.has_value()) << "dimmed text not found in rendered output";
    EXPECT_TRUE(ref->pixel->dim)
        << "render_markdown_dim did not apply dim style";
}

TEST(MarkdownRenderStyles, HeadingBold) {
    auto screen = render_to_screen("# Heading One");
    auto ref = find_pixel_with_text(screen, "Heading");
    ASSERT_TRUE(ref.has_value()) << "heading text not found in rendered output";
    EXPECT_TRUE(ref->pixel->bold)
        << "heading did not produce bold pixel style";
}

TEST(MarkdownRenderStyles, CodeBlockSyntaxHighlight) {
    auto screen = render_to_screen(
        "```cpp\nint main() { return 0; }\n```");
    EXPECT_TRUE(has_colored_pixel(screen))
        << "code block with syntax highlighting produced no colored pixels";
}

TEST(MarkdownRenderStyles, FootnoteRefDimmed) {
    // Footnote definitions are hoisted to the footnotes section (HTML
    // serializer only); the terminal renderer shows the literal label dimmed.
    auto screen = render_to_screen(
        "See note[^1] here.\n\n[^1]: The footnote content.");
    auto ref = find_pixel_with_text(screen, "[^1]");
    ASSERT_TRUE(ref.has_value())
        << "footnote ref not found in rendered output";
    EXPECT_TRUE(ref->pixel->dim)
        << "footnote ref did not produce dim pixel style";
}

TEST(MarkdownRenderStyles, MathCyanColor) {
    auto screen = render_to_screen("The formula $x^2 + y^2 = z^2$ holds");
    auto ref = find_pixel_with_text(screen, "x^2");
    ASSERT_TRUE(ref.has_value()) << "math text not found in rendered output";
    EXPECT_EQ(ref->pixel->foreground_color, ftxui::Color::CyanLight)
        << "inline math did not produce CyanLight pixel color";
}

TEST(MarkdownRenderStyles, EscapeNotBold) {
    // Escaped asterisks must render as literal asterisks, not bold markers.
    auto screen = render_to_screen(R"(\*not bold\*)");
    auto ref = find_pixel_with_text(screen, "not bold");
    ASSERT_TRUE(ref.has_value())
        << "escaped text not found in rendered output";
    EXPECT_FALSE(ref->pixel->bold)
        << "escaped asterisks incorrectly produced bold style";
    // The literal asterisks must be present in the output.
    std::string out = strip_ansi(screen.ToString());
    EXPECT_NE(out.find("*not bold*"), std::string::npos)
        << "escaped asterisks were not rendered as literal characters";
}

TEST(MarkdownRenderStyles, NestedBoldItalic) {
    // ***both*** is bold+italic — the terminal renderer applies bold+dim.
    auto screen = render_to_screen("This is ***both*** styles");
    auto ref = find_pixel_with_text(screen, "both");
    ASSERT_TRUE(ref.has_value())
        << "nested bold+italic text not found in rendered output";
    EXPECT_TRUE(ref->pixel->bold)
        << "nested bold+italic did not produce bold pixel style";
    EXPECT_TRUE(ref->pixel->dim)
        << "nested bold+italic did not produce dim pixel style";
}

// ════════════════════════════════════════════════════════════════════
// Layer 3: Layout tests
// ════════════════════════════════════════════════════════════════════

TEST(MarkdownRenderStyles, EnglishWrapsAtWordBoundary) {
    std::string md =
        "This is a long English paragraph that should wrap to multiple "
        "lines when rendered at a narrow terminal width.";
    auto screen = render_to_screen(md, 20, 10);
    std::string out = strip_ansi(screen.ToString());
    int lines = count_nonempty_lines(out);
    EXPECT_GT(lines, 1) << "long English text did not wrap at width 20";
    // Words must not be split — every word from the source must appear
    // intact in the output.
    for (const char* word : {"paragraph", "multiple", "width."}) {
        EXPECT_NE(out.find(word), std::string::npos)
            << "word '" << word << "' was split or swallowed during wrapping";
    }
}

TEST(MarkdownRenderStyles, CjkWrapsAtCharBoundary) {
    // CJK text has no spaces — each character is a potential break point.
    // At width 10 (5 CJK chars per line at 2 cols each), a 4-char sequence
    // can span a line boundary, so assert on 2-char substrings that stay
    // on one line.
    std::string md =
        "这是一段很长的中文文本用来测试折行功能是否正常工作"
        "当终端宽度不够时应该能够在任意中文字符之间换行";
    auto screen = render_to_screen(md, 10, 15);
    std::string out = strip_ansi(screen.ToString());
    int lines = count_nonempty_lines(out);
    EXPECT_GT(lines, 1) << "CJK text did not wrap at width 10";
    // All content must be present — no character swallowing.
    EXPECT_NE(out.find("中文"), std::string::npos)
        << "CJK content was swallowed during wrapping";
    EXPECT_NE(out.find("换行"), std::string::npos)
        << "CJK content was swallowed during wrapping";
}

TEST(MarkdownRenderStyles, MixedContentWraps) {
    std::string md =
        "比较特别的一点：**整棵 C++23 named modules**（`import std;`），"
        "没有任何依赖。";
    auto screen = render_to_screen(md, 30, 10);
    std::string out = strip_ansi(screen.ToString());
    int lines = count_nonempty_lines(out);
    EXPECT_GT(lines, 1) << "mixed content did not wrap at width 30";
    // The characters that were previously swallowed must be present.
    EXPECT_NE(out.find("modules"), std::string::npos);
    EXPECT_NE(out.find("import std;"), std::string::npos);
    EXPECT_NE(out.find("依赖"), std::string::npos);
}

TEST(MarkdownRenderStyles, ListItemIndentation) {
    auto screen = render_to_screen(
        "- first item\n- second item\n- third item", 80, 10);
    std::string out = strip_ansi(screen.ToString());
    auto lines = split_lines(out);
    int found = 0;
    for (const auto& line : lines) {
        if (line.find("item") != std::string::npos) {
            // Top-level items start with "- " (no leading indent).
            EXPECT_EQ(leading_spaces(line), 0u)
                << "top-level list item has unexpected indentation: " << line;
            EXPECT_NE(line.find("- "), std::string::npos)
                << "list item missing '- ' bullet: " << line;
            ++found;
        }
    }
    EXPECT_EQ(found, 3) << "expected 3 list items, found " << found;
}

TEST(MarkdownRenderStyles, NestedListProgressiveIndent) {
    auto screen = render_to_screen(
        "- top level\n  - nested level", 80, 10);
    std::string out = strip_ansi(screen.ToString());
    auto lines = split_lines(out);
    std::size_t top_indent = 0;
    std::size_t nested_indent = 0;
    bool found_top = false;
    bool found_nested = false;
    for (const auto& line : lines) {
        if (line.find("top level") != std::string::npos) {
            top_indent = leading_spaces(line);
            found_top = true;
        }
        if (line.find("nested level") != std::string::npos) {
            nested_indent = leading_spaces(line);
            found_nested = true;
        }
    }
    ASSERT_TRUE(found_top) << "top-level list item not found";
    ASSERT_TRUE(found_nested) << "nested list item not found";
    EXPECT_GT(nested_indent, top_indent)
        << "nested list item does not have more indentation than top-level";
}

TEST(MarkdownRenderStyles, BlockquoteLeftBorder) {
    auto screen = render_to_screen("> quoted text here", 80, 5);
    std::string out = strip_ansi(screen.ToString());
    auto lines = split_lines(out);
    bool found_border = false;
    for (const auto& line : lines) {
        // The blockquote bar is ▎ (U+258E, UTF-8: \xE2\x96\x8E).
        if (line.find("\xE2\x96\x8E") != std::string::npos) {
            found_border = true;
            // The bar should be at the start of the line (column 0).
            EXPECT_EQ(line.find("\xE2\x96\x8E"), 0u)
                << "blockquote bar is not at the start of the line: " << line;
            break;
        }
    }
    EXPECT_TRUE(found_border)
        << "blockquote did not produce a left border (▎) in the output";
}

TEST(MarkdownRenderStyles, CodeBlockNoWrap) {
    // A long code line must stay on one line (truncated by the stencil,
    // not wrapped to the next line).
    auto screen = render_to_screen(
        "```\nabcdefghijklmnopqrstuvwxyz0123456789\n```", 20, 10);
    std::string out = strip_ansi(screen.ToString());
    // The first 20 characters of the code must be present.
    EXPECT_NE(out.find("abcdefghijklmnopqrst"), std::string::npos)
        << "code content was truncated or wrapped";
    // Count lines containing code content — must be exactly 1.
    int code_lines = 0;
    auto lines = split_lines(out);
    for (const auto& line : lines) {
        if (line.find("abcdefghij") != std::string::npos) ++code_lines;
    }
    EXPECT_EQ(code_lines, 1)
        << "code block wrapped to " << code_lines << " lines (expected 1)";
}

TEST(MarkdownRenderStyles, TableAlignsColumns) {
    auto screen = render_to_screen(
        "| Name | Value |\n"
        "|------|-------|\n"
        "| alpha | 1 |\n"
        "| beta | 22 |",
        80, 10);
    std::string out = strip_ansi(screen.ToString());
    // The table must contain vertical and horizontal box-drawing chars.
    constexpr std::string_view kVBar = "\xE2\x94\x82";  // │ U+2502
    constexpr std::string_view kHBar = "\xE2\x94\x80";  // ─ U+2500
    EXPECT_NE(out.find(kVBar), std::string::npos)
        << "table did not produce vertical separators (│)";
    EXPECT_NE(out.find(kHBar), std::string::npos)
        << "table did not produce horizontal separators (─)";
    // Header and data text must be present.
    EXPECT_NE(out.find("Name"), std::string::npos);
    EXPECT_NE(out.find("alpha"), std::string::npos);
    EXPECT_NE(out.find("beta"), std::string::npos);
    // The │ positions must be aligned across header and data rows.
    auto lines = split_lines(out);
    std::vector<std::size_t> header_positions;
    std::vector<std::size_t> data_positions;
    for (const auto& line : lines) {
        if (line.find("Name") != std::string::npos) {
            header_positions = find_utf8_positions(line, kVBar);
        }
        if (line.find("alpha") != std::string::npos) {
            data_positions = find_utf8_positions(line, kVBar);
        }
    }
    ASSERT_FALSE(header_positions.empty())
        << "table header row with │ not found";
    ASSERT_FALSE(data_positions.empty())
        << "table data row with │ not found";
    EXPECT_EQ(header_positions, data_positions)
        << "table │ separators are not aligned between header and data rows";
}
