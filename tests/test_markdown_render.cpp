// test_markdown_render.cpp — FTXUI terminal renderer tests for the markdown
// module.  The conformance suite (test_markdown_conformance.cpp) tests the
// HTML serializer; this file tests the production FTXUI renderer that the
// terminal UI uses.

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <sstream>
#include <string>

import std;
import loom.ui.visual.markdown;

using namespace loom::ui;

namespace {

// Render an element to a string at the given width (single row).
std::string render_row(ftxui::Element el, int width = 200) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(width),
        ftxui::Dimension::Fixed(3));
    ftxui::Render(screen, el);
    return screen.ToString();
}

// Describe the inline tokens for debugging.
std::string describe_tokens(const std::vector<InlineToken>& tokens) {
    std::ostringstream os;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        os << "  [" << i << "] kind=" << static_cast<int>(t.kind);
        if (!t.text.empty()) os << " text=\"" << t.text << "\"";
        if (!t.url.empty()) os << " url=\"" << t.url << "\"";
        if (!t.children.empty()) {
            os << " children=[";
            for (std::size_t j = 0; j < t.children.size(); ++j) {
                if (j) os << ", ";
                os << "\"" << t.children[j].text << "\"";
            }
            os << "]";
        }
        os << "\n";
    }
    return os.str();
}

// Strip CSI (ESC[...final) and OSC (ESC]...BEL or ESC]...ESC-backslash)
// escape sequences from Screen::ToString() output.
std::string strip_ansi(std::string_view text) {
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

// Normalize text for comparison: strip ANSI, collapse whitespace runs to
// single spaces, trim leading/trailing whitespace.
std::string normalize_for_comparison(std::string_view text) {
    std::string clean = strip_ansi(text);
    std::string result;
    bool in_space = false;
    for (char c : clean) {
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            if (!in_space) {
                result += ' ';
                in_space = true;
            }
        } else {
            result += c;
            in_space = false;
        }
    }
    auto start = result.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    auto end = result.find_last_not_of(' ');
    return result.substr(start, end - start + 1);
}

} // namespace

// ── Character swallowing tests ──────────────────────────────────────────

TEST(MarkdownRender, MixedChineseEnglishBold) {
    // The user reported that "modules**" is swallowed when rendering
    // mixed Chinese/English text with bold.
    std::string md = "比较特别的一点：**整棵 C++23 named modules**（`import std;`），没有任何依赖";
    auto el = render_markdown(md);
    std::string out = render_row(el);
    std::cerr << "TOKENS:\n" << describe_tokens(detail::lex_blocks(md).empty()
        ? std::vector<InlineToken>{}
        : detail::lex_blocks(md).at(0).inlines);
    std::cerr << "RENDERED:\n" << out << "\n";
    // "modules" must appear in the output
    EXPECT_NE(out.find("modules"), std::string::npos)
        << "bold content 'modules' was swallowed";
}

TEST(MarkdownRender, InlineCodeWithMixedText) {
    // The user reported that `import std;` renders as `import s`.
    std::string md = "named modules**（`import std;`），没有任何";
    auto el = render_markdown(md);
    std::string out = render_row(el);
    std::cerr << "RENDERED:\n" << out << "\n";
    EXPECT_NE(out.find("import std;"), std::string::npos)
        << "code content 'import std;' was truncated to 'import s'";
}

TEST(MarkdownRender, LongChineseText) {
    // The user reported that "代码库移植的；设计意图记录在" is swallowed.
    std::string md = "它是从一个已删除的 TypeScript 代码库移植的；设计意图记录在 docs/decisions/design-decisions.md。";
    auto el = render_markdown(md);
    std::string out = render_row(el);
    std::cerr << "RENDERED:\n" << out << "\n";
    EXPECT_NE(out.find("代码库移植的"), std::string::npos)
        << "Chinese text '代码库移植的' was swallowed";
    EXPECT_NE(out.find("design-decisions.md"), std::string::npos)
        << "URL 'design-decisions.md' was truncated to 'design-'";
}

TEST(MarkdownRender, SimpleEnglishBold) {
    // Control: pure English bold should work.
    std::string md = "Hello **world** end";
    auto el = render_markdown(md);
    std::string out = render_row(el);
    std::cerr << "RENDERED:\n" << out << "\n";
    EXPECT_NE(out.find("world"), std::string::npos);
}

TEST(MarkdownRender, SimpleChineseBold) {
    // Control: pure Chinese bold should work.
    std::string md = "你好**世界**结束";
    auto el = render_markdown(md);
    std::string out = render_row(el);
    std::cerr << "RENDERED:\n" << out << "\n";
    EXPECT_NE(out.find("世界"), std::string::npos);
}

// ── StreamingMarkdown tests ─────────────────────────────────────────────

TEST(MarkdownRender, StreamingMarkdownFullContent) {
    // Simulate StreamingMarkdown::update with the full final content.
    // This is the path used for streaming assistant messages.
    std::string md =
        "比较特别的一点：**整棵 C++23 named modules**（`import std;`），"
        "没有任何 TypeScript/Bun/npm 依赖——它是从一个已删除的 TypeScript "
        "代码库移植的；设计意图记录在 docs/decisions/design-decisions.md。"
        "目前处于活跃开发阶段，设";
    StreamingMarkdown sm;
    auto el = sm.update(md);
    std::string out = render_row(el);
    std::cerr << "STREAMING RENDERED:\n" << out << "\n";
    EXPECT_NE(out.find("modules"), std::string::npos)
        << "bold content 'modules' was swallowed";
    EXPECT_NE(out.find("import std;"), std::string::npos)
        << "code content 'import std;' was truncated";
    EXPECT_NE(out.find("代码库移植的"), std::string::npos)
        << "Chinese text '代码库移植的' was swallowed";
    EXPECT_NE(out.find("design-decisions.md"), std::string::npos)
        << "URL 'design-decisions.md' was truncated";
}

TEST(MarkdownRender, StreamingMarkdownIncremental) {
    // Simulate streaming: content arrives in chunks.
    StreamingMarkdown sm;
    std::string full =
        "比较特别的一点：**整棵 C++23 named modules**（`import std;`），"
        "没有任何依赖。";
    // Feed in chunks
    std::vector<std::string> chunks = {
        "比较特别的一点：**整棵 C++23 named ",
        "modules**（`import std;`），",
        "没有任何依赖。",
    };
    for (const auto& chunk : chunks) {
        static std::string accumulated;
        accumulated += chunk;
        auto el = sm.update(accumulated);
        std::string out = render_row(el);
        std::cerr << "CHUNK: " << accumulated << "\n";
        std::cerr << "RENDERED:\n" << out << "\n";
    }
    // Final state should have all content
    auto el = sm.update(full);
    std::string out = render_row(el);
    EXPECT_NE(out.find("modules"), std::string::npos);
    EXPECT_NE(out.find("import std;"), std::string::npos);
}

// ── Line wrapping tests ─────────────────────────────────────────────────

// Count non-empty lines in rendered output.
static int count_nonempty_lines(const std::string& s) {
    int n = 0;
    std::istringstream is(s);
    std::string line;
    while (std::getline(is, line)) {
        // Strip trailing whitespace — a line with only spaces is empty.
        if (line.find_first_not_of(' ') != std::string::npos) ++n;
    }
    return n;
}

TEST(MarkdownRender, LongTextWrapsToMultipleLines) {
    // A long paragraph must wrap to multiple lines at a narrow width.
    std::string md =
        "This is a long English paragraph that should wrap to multiple "
        "lines when rendered at a narrow terminal width.";
    auto el = render_markdown(md);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(40), ftxui::Dimension::Fixed(10));
    ftxui::Render(screen, el);
    std::string out = screen.ToString();
    int lines = count_nonempty_lines(out);
    std::cerr << "WRAPPED (" << lines << " lines):\n" << out << "\n";
    EXPECT_GT(lines, 1) << "long text did not wrap to multiple lines";
    // All content must be present — no character swallowing.
    EXPECT_NE(out.find("paragraph"), std::string::npos);
    EXPECT_NE(out.find("multiple"), std::string::npos);
    EXPECT_NE(out.find("width."), std::string::npos);
}

TEST(MarkdownRender, LongChineseTextWraps) {
    // CJK text has no spaces — each character is a potential break point.
    std::string md =
        "这是一段很长的中文文本用来测试折行功能是否正常工作"
        "当终端宽度不够时应该能够在任意中文字符之间换行";
    auto el = render_markdown(md);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(30), ftxui::Dimension::Fixed(10));
    ftxui::Render(screen, el);
    std::string out = screen.ToString();
    int lines = count_nonempty_lines(out);
    std::cerr << "CJK WRAPPED (" << lines << " lines):\n" << out << "\n";
    EXPECT_GT(lines, 1) << "CJK text did not wrap";
    // All content must be present.
    EXPECT_NE(out.find("中文文本"), std::string::npos);
    EXPECT_NE(out.find("换行"), std::string::npos);
}

TEST(MarkdownRender, MixedContentWraps) {
    // Mixed Chinese/English with bold and code — the original bug report.
    std::string md =
        "比较特别的一点：**整棵 C++23 named modules**（`import std;`），"
        "没有任何 TypeScript/Bun/npm 依赖——它是从一个已删除的 TypeScript "
        "代码库移植的；设计意图记录在 docs/decisions/design-decisions.md。"
        "目前处于活跃开发阶段，设";
    auto el = render_markdown(md);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(60), ftxui::Dimension::Fixed(10));
    ftxui::Render(screen, el);
    std::string out = screen.ToString();
    int lines = count_nonempty_lines(out);
    std::cerr << "MIXED WRAPPED (" << lines << " lines):\n" << out << "\n";
    EXPECT_GT(lines, 1) << "mixed content did not wrap";
    // The characters that were previously swallowed must be present.
    EXPECT_NE(out.find("modules"), std::string::npos);
    EXPECT_NE(out.find("import std;"), std::string::npos);
    EXPECT_NE(out.find("代码库移植的"), std::string::npos);
    EXPECT_NE(out.find("design-decisions.md"), std::string::npos);
    EXPECT_NE(out.find("阶段，设"), std::string::npos);
}

// ── Layer 4: Edge case + streaming tests ──────────────────────────────

TEST(MarkdownRender, EmptyContent) {
    // Empty markdown must not crash and should produce whitespace-only output.
    auto el = render_markdown("");
    std::string out = strip_ansi(render_row(el));
    EXPECT_EQ(out.find_first_not_of(" \n\r\t"), std::string::npos)
        << "empty markdown produced non-whitespace output: " << out;
}

TEST(MarkdownRender, VeryLongSingleWord) {
    // A very long word must not crash the renderer.  The flexbox may
    // truncate it at the screen boundary, but the renderer must not
    // segfault or assert.
    std::string word(500, 'a');
    auto el = render_markdown(word);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(40), ftxui::Dimension::Fixed(20));
    ftxui::Render(screen, el);
    std::string out = strip_ansi(screen.ToString());
    // The beginning of the word must be present (not swallowed entirely).
    EXPECT_NE(out.find("aaaaaaaaaa"), std::string::npos)
        << "long word was entirely swallowed";
    // The output should contain at least 40 'a' characters (one screen width).
    std::size_t a_count = 0;
    for (char c : out) {
        if (c == 'a') ++a_count;
    }
    EXPECT_GE(a_count, 40u)
        << "long word was truncated too aggressively: only " << a_count
        << " chars visible";
}

TEST(MarkdownRender, LongUrl) {
    // A long URL must be present in the output, not truncated.
    std::string url =
        "https://example.com/very/long/path/with/many/segments/"
        "that/exceeds/normal/width?param=value&other=123";
    std::string md = "See " + url + " for details";
    auto el = render_markdown(md);
    std::string out = strip_ansi(render_row(el));
    EXPECT_NE(out.find(url), std::string::npos)
        << "long URL was truncated or swallowed";
}

// ── Code block character swallowing tests ──────────────────────────────

TEST(MarkdownRender, CodeBlockLongLineTruncatesAtEdge) {
    // A code block line longer than the terminal width must truncate at
    // the right edge, not shrink every syntax token proportionally.
    // The hbox path routed overwidth lines through
    // box_helper::ComputeShrinkHard, which scaled each token down and
    // swallowed characters from the middle of the line.
    std::string code_line =
        "auto result = some_function(arg1, arg2, arg3, arg4, arg5, "
        "arg6, arg7, arg8, arg9, arg10);";
    std::string md = "```cpp\n" + code_line + "\n```";
    auto el = render_markdown(md);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(40), ftxui::Dimension::Fixed(5));
    ftxui::Render(screen, el);
    std::string out = strip_ansi(screen.ToString());
    std::cerr << "CODE BLOCK (40 cols):\n" << out << "\n";

    // The first 40 characters of the source must appear contiguously on
    // one line. With the hbox bug, tokens were shrunk proportionally,
    // dropping characters from throughout the line and breaking this
    // prefix (and wrapping the remainder onto the next row).
    std::string prefix = code_line.substr(0, 40);
    EXPECT_NE(out.find(prefix), std::string::npos)
        << "code block prefix was not contiguous — characters were "
           "swallowed from the middle of the line";
}

TEST(MarkdownRender, CodeBlockLongLineMiddleTokensIntact) {
    // Regression: tokens in the middle of an overwidth code line must
    // survive intact. The hbox shrink dropped characters from every token,
    // so a mid-line token like "configuration_value" was rendered as
    // "configuration_valu" or similar. With the fix, only the right edge
    // is cut.
    std::string code_line =
        "int configuration_value = compute(parameters, options, "
        "overrides, defaults, settings, preferences, modifiers);";
    std::string md = "```cpp\n" + code_line + "\n```";
    auto el = render_markdown(md);
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(50), ftxui::Dimension::Fixed(5));
    ftxui::Render(screen, el);
    std::string out = strip_ansi(screen.ToString());
    std::cerr << "CODE BLOCK (50 cols):\n" << out << "\n";

    // "configuration_value" (cols 4–22) and "compute(parameters"
    // (cols 29–45) are both within the 50-col width, so they must appear
    // intact — not shrunk by the proportional shrink.
    EXPECT_NE(out.find("configuration_value"), std::string::npos)
        << "mid-line token 'configuration_value' was shrunk";
    EXPECT_NE(out.find("compute(parameters"), std::string::npos)
        << "mid-line token 'compute(parameters' was swallowed";
}

TEST(MarkdownRender, StreamingMatchesFullRender) {
    // StreamingMarkdown::update(full) should produce the same text as
    // render_markdown(full) for a document without block boundaries.
    std::string md =
        "Hello **world** with `code` and [link](https://example.com)";
    auto full_el = render_markdown(md);
    std::string full_out = normalize_for_comparison(render_row(full_el));

    StreamingMarkdown sm;
    auto stream_el = sm.update(md);
    std::string stream_out = normalize_for_comparison(render_row(stream_el));

    EXPECT_EQ(stream_out, full_out)
        << "streaming render differs from full render\n"
        << "  stream: " << stream_out << "\n"
        << "  full:   " << full_out;
}

TEST(MarkdownRender, StreamingIncrementalNoLoss) {
    // Feeding content in chunks should produce the same final text as
    // feeding the full content at once.
    std::string full =
        "比较特别的一点：**整棵 C++23 named modules**（`import std;`），"
        "没有任何依赖。";
    std::vector<std::string> chunks = {
        "比较特别的一点：**整棵 C++23 named ",
        "modules**（`import std;`），",
        "没有任何依赖。",
    };

    // Feed chunks incrementally.
    StreamingMarkdown sm_chunked;
    std::string accumulated;
    for (const auto& chunk : chunks) {
        accumulated += chunk;
        sm_chunked.update(accumulated);
    }
    auto chunked_el = sm_chunked.update(full);
    std::string chunked_out = normalize_for_comparison(render_row(chunked_el));

    // Feed the full content at once.
    StreamingMarkdown sm_full;
    auto full_el = sm_full.update(full);
    std::string full_out = normalize_for_comparison(render_row(full_el));

    EXPECT_EQ(chunked_out, full_out)
        << "incremental streaming produced different output than full content\n"
        << "  chunked: " << chunked_out << "\n"
        << "  full:    " << full_out;
}

TEST(MarkdownRender, StreamingStablePrefix) {
    // After each chunk, the stable prefix length should be non-decreasing.
    // The stable prefix covers complete blocks (paragraphs separated by
    // double newlines); it grows as more block boundaries are seen.
    std::string full =
        "First paragraph.\n\nSecond paragraph with **bold** text.\n\nThird.";
    std::vector<std::string> chunks = {
        "First paragraph.\n\nSecond paragraph with **bold** text.",
        "\n\nThird.",
    };

    StreamingMarkdown sm;
    std::size_t prev_stable = 0;
    std::string accumulated;
    for (const auto& chunk : chunks) {
        accumulated += chunk;
        sm.update(accumulated);
        std::size_t stable = sm.stable_length();
        EXPECT_GE(stable, prev_stable)
            << "stable prefix length decreased from " << prev_stable
            << " to " << stable;
        prev_stable = stable;
    }
    // After the full content, the stable prefix should cover the first
    // two paragraphs (the double-newline boundaries).
    EXPECT_GT(sm.stable_length(), 0u)
        << "stable prefix is empty after full content";
    // The stable prefix should include the first paragraph boundary.
    EXPECT_GE(sm.stable_length(),
              std::string("First paragraph.\n\n").size())
        << "stable prefix did not advance past the first paragraph boundary";
}
