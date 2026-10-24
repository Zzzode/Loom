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
import loom.serdes.json;
import loom.crypto.crypto;

#include "mermaid_flow400_authority.hpp"

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

// RFC 0031 E6b: StreamingMarkdown owns its per-instance Mermaid layout-rows
// cache. Streaming frames with a closed fence must render byte-identically to
// uncached render_markdown, and reset() must not corrupt later documents.
TEST(MarkdownRender, StreamingMarkdownMermaidRowsAcrossFramesAndReset) {
    auto capture = [](ftxui::Element element) {
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(200),
                                            ftxui::Dimension::Fixed(24));
        ftxui::Render(screen, std::move(element));
        std::string result;
        for (int y = 0; y < screen.dimy(); ++y) {
            for (int x = 0; x < screen.dimx(); ++x)
                result += screen.PixelAt(x, y).character;
            result += '\n';
        }
        return result;
    };
    const std::string doc1 =
        "intro\n```mermaid\nflowchart TD\nA[X] --> B[Y]\n```\n";
    const std::string doc2 =
        "other\n```mermaid\nflowchart TD\nC[One] --> D[Two]\n```\n";

    StreamingMarkdown sm;
    // Growing-delta frames keep the closed fence stable; cached rows must
    // produce exactly the uncached surface on every frame.
    for (const std::string suffix : {"", "m", "mo", "mor", "more text"}) {
        const std::string grown = doc1 + suffix;
        EXPECT_EQ(capture(sm.update(grown)), capture(render_markdown(grown)))
            << "frame: " << grown;
    }
    // A brand-new document after reset renders correctly and independently.
    sm.reset();
    EXPECT_EQ(capture(sm.update(doc2)), capture(render_markdown(doc2)));
    EXPECT_EQ(capture(sm.update(doc1)), capture(render_markdown(doc1)));
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

TEST(MarkdownRender, StreamingPrefixesPreserveFullDocumentLayout) {
    const std::vector<std::vector<std::string>> documents{
        {"# Heading\n\nBody", "# Heading\n\nBody\n\nTail"},
        {"1. first\n\n1. second", "1. first\n\n1. second\n\nAfter"},
        {"- first\n\n  continuation", "- first\n\n  continuation\n\nAfter"},
        {"~~~\na\n\nb", "~~~\na\n\nb\n~~~\n"},
        {"  ```\na\n\nb", "  ```\na\n\nb\n  ```\n"},
        {"````\na\n```\n\nb", "````\na\n```\n\nb\n````\n"},
        {"[reference][id]", "[reference][id]\n\n[id]: https://example.com"},
        {"Body[^n]", "Body[^n]\n\n[^n]: Footnote"},
        {"# Heading\n\n| a | b |\n|---|---|\n| first | second |",
         "# Heading\n\n| a | b |\n|---|---|\n| first | second |\n| longer | value |"},
        {"> quote\n> continued", "> quote\n> continued\n\nAfter"}};
    for (int width : {24, 80}) {
        for (const auto& prefixes : documents) {
            StreamingMarkdown streaming;
            for (const auto& prefix : prefixes) {
                SCOPED_TRACE(std::to_string(width) + ": " + prefix);
                auto live = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                                 ftxui::Dimension::Fixed(80));
                auto final = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                                  ftxui::Dimension::Fixed(80));
                ftxui::Render(live, streaming.update(prefix));
                ftxui::Render(final, render_markdown(prefix));
                // Compare exact rows and styles, not whitespace-normalized text.
                EXPECT_EQ(live.ToString(), final.ToString());
            }
        }
    }
}

TEST(MarkdownRender, StreamingLongDocumentKeepsFullLayout) {
    std::string document;
    for (int section = 0; section < 120; ++section) {
        document += "## Section " + std::to_string(section) +
            "\n\nA completed paragraph with **emphasis**, `inline code`, and words "
            "that wrap across the terminal viewport. Keep every word readable.\n\n";
    }
    StreamingMarkdown streaming;
    std::int64_t total_us = 0;
    std::int64_t max_us = 0;
    int frames = 0;
    for (std::size_t size = 128; size < document.size(); size += 128) {
        const auto started = std::chrono::steady_clock::now();
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                           ftxui::Dimension::Fixed(40));
        ftxui::Render(screen, streaming.update(std::string_view(document).substr(0, size)));
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        total_us += elapsed;
        max_us = std::max(max_us, elapsed);
        ++frames;
    }
    auto live = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                     ftxui::Dimension::Fixed(600));
    auto final = ftxui::Screen::Create(ftxui::Dimension::Fixed(80),
                                      ftxui::Dimension::Fixed(600));
    ftxui::Render(live, streaming.update(document));
    ftxui::Render(final, render_markdown(document));
    EXPECT_EQ(live.ToString(), final.ToString());
    RecordProperty("streaming_document_bytes", static_cast<int>(document.size()));
    RecordProperty("streaming_mean_frame_us", static_cast<int>(total_us / frames));
    RecordProperty("streaming_max_frame_us", static_cast<int>(max_us));
    std::cout << "Streaming document: " << document.size() << " bytes, " << frames
              << " frames, mean " << total_us / frames << " us, max " << max_us << " us\n";
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

TEST(MarkdownRender, StreamingRepeatedFramesResizeReplaceAndReset) {
    StreamingMarkdown streaming;
    const std::vector<std::string> documents{
        "# Heading\n\nA long paragraph with **bold text** that wraps repeatedly.",
        "# Heading\n\nReplacement", ""};
    for (const auto& document : documents) {
        for (int width : {24, 80, 24}) {
            auto live = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                             ftxui::Dimension::Fixed(40));
            auto final = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                              ftxui::Dimension::Fixed(40));
            ftxui::Render(live, streaming.update(document));
            ftxui::Render(final, render_markdown(document));
            EXPECT_EQ(live.ToString(), final.ToString());
        }
    }
    streaming.reset();
    EXPECT_EQ(render_row(streaming.update(documents.front())),
              render_row(render_markdown(documents.front())));
}

// RFC 0030: exercise the production Markdown entry, including its width-aware
// diagram/source choice, rather than a separate parser or HTML serializer.
namespace {
std::string mermaid_fence(std::string_view source) {
    return "```mermaid\n" + std::string(source) + "\n```";
}

std::string render_document(ftxui::Element element, int width = 120) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width),
                                       ftxui::Dimension::Fixed(256));
    ftxui::Render(screen, element);
    return strip_ansi(screen.ToString());
}

std::string mermaid_document(std::string_view source, int width = 120) {
    return render_document(render_markdown(mermaid_fence(source)), width);
}

void expect_diagram(const std::string& rendered, std::string_view header) {
    EXPECT_EQ(rendered.find(header), std::string::npos) << rendered;
    EXPECT_TRUE(rendered.find("┌") != std::string::npos ||
                rendered.find("╭") != std::string::npos) << rendered;
}

std::pair<int, int> text_position(const std::string& rendered, std::string_view label) {
    const auto offset = rendered.find(label);
    if (offset == std::string::npos) return {-1, -1};
    const auto row = static_cast<int>(std::count(rendered.begin(), rendered.begin() + offset, '\n'));
    const auto last_newline = rendered.rfind('\n', offset);
    return {row, static_cast<int>(last_newline == std::string::npos ? offset : offset - last_newline - 1)};
}

int diagram_arrows(const std::string& rendered) {
    int count = 0;
    for (const auto* glyph : {"▶", "◀", "▼", "▲"}) {
        std::size_t at = 0;
        while ((at = rendered.find(glyph, at)) != std::string::npos) {
            ++count;
            at += std::string_view(glyph).size();
        }
    }
    return count;
}
}

TEST(MarkdownMermaid, FlowchartBranchesKeepNodesEdgesAndLabels) {
    const auto rendered = mermaid_document(
        "flowchart TD\nStart[Begin] --> Gate{Ready?}\n"
        "Gate -->|yes| Success(Complete)\nGate -->|no| Retry[Try again]");
    expect_diagram(rendered, "flowchart TD");
    for (const auto* label : {"Begin", "Ready?", "Complete", "Try again", "yes", "no"})
        EXPECT_NE(rendered.find(label), std::string::npos) << rendered;
    EXPECT_EQ(rendered.find("-->"), std::string::npos);
    EXPECT_EQ(diagram_arrows(rendered), 3) << rendered;
    EXPECT_LT(text_position(rendered, "Begin").first, text_position(rendered, "Ready?").first);
    EXPECT_LT(text_position(rendered, "Ready?").first, text_position(rendered, "Complete").first);
}

TEST(MarkdownMermaid, FlowDirectionsPreserveGeometricMeaning) {
    for (const auto* direction : {"TD", "TB", "BT", "LR", "RL"}) {
        SCOPED_TRACE(direction);
        const auto rendered = mermaid_document(std::string("graph ") + direction +
            "\nA[Origin] --> B[Destination]");
        expect_diagram(rendered, "graph ");
        const auto origin = text_position(rendered, "Origin");
        const auto target = text_position(rendered, "Destination");
        ASSERT_GE(origin.first, 0);
        ASSERT_GE(target.first, 0);
        const std::string_view value(direction);
        const auto* arrow = value == "LR" ? "▶" : value == "RL" ? "◀" : value == "BT" ? "▲" : "▼";
        EXPECT_NE(rendered.find(arrow), std::string::npos) << rendered;
        EXPECT_EQ(diagram_arrows(rendered), 1) << rendered;
        if (value == "LR") EXPECT_LT(origin.second, target.second);
        else if (value == "RL") EXPECT_GT(origin.second, target.second);
        else if (value == "BT") EXPECT_GT(origin.first, target.first);
        else EXPECT_LT(origin.first, target.first);
    }
}

TEST(MarkdownMermaid, CyclesAndSelfEdgesRemainConnectedDiagrams) {
    for (const auto* source : {
            "flowchart TD\nA[First] --> B[Second]\nB -->|back| A",
            "flowchart LR\nA[Loop] -->|again| A"}) {
        const auto rendered = mermaid_document(source);
        expect_diagram(rendered, "flowchart ");
        EXPECT_EQ(rendered.find("-->"), std::string::npos);
        EXPECT_EQ(diagram_arrows(rendered), std::string_view(source).find("back") != std::string_view::npos ? 2 : 1)
            << rendered;
        EXPECT_TRUE(rendered.find("back") != std::string::npos ||
                    rendered.find("again") != std::string::npos) << rendered;
    }
}

TEST(MarkdownMermaid, QuotedChineseAndCombiningLabelsRemainWhole) {
    const auto rendered = mermaid_document(
        "flowchart LR\nA[\"中文客户端\"] -->|请求| B[\"Café 服务\"]");
    expect_diagram(rendered, "flowchart LR");
    EXPECT_NE(rendered.find("中文客户端"), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("Café 服务"), std::string::npos) << rendered;
    EXPECT_NE(rendered.find("请求"), std::string::npos) << rendered;
}

TEST(MarkdownMermaid, ReferencesAllowLaterConsistentDeclarations) {
    const auto rendered = mermaid_document(
        "graph TD; A --> B; A[Explicit origin]; B[Explicit target]; A[Explicit origin]");
    expect_diagram(rendered, "graph TD");
    EXPECT_NE(rendered.find("Explicit origin"), std::string::npos);
    EXPECT_NE(rendered.find("Explicit target"), std::string::npos);
}

TEST(MarkdownMermaid, SequenceParticipantsAliasesAndMessagesRemainVisible) {
    const auto rendered = mermaid_document(
        "sequenceDiagram\nparticipant A as Client\nparticipant B as Service\n"
        "A->>B: Request\nB-->>A: Response\nB->>B: Local step");
    expect_diagram(rendered, "sequenceDiagram");
    for (const auto* label : {"Client", "Service", "Request", "Response", "Local step"})
        EXPECT_NE(rendered.find(label), std::string::npos) << rendered;
    EXPECT_LT(text_position(rendered, "Client").second, text_position(rendered, "Service").second);
    EXPECT_LT(text_position(rendered, "Request").first, text_position(rendered, "Response").first);
    EXPECT_LT(text_position(rendered, "Response").first, text_position(rendered, "Local step").first);
    EXPECT_EQ(diagram_arrows(rendered), 3) << rendered;
    EXPECT_NE(rendered.find("▶"), std::string::npos);
    const auto first_left = rendered.find("◀");
    ASSERT_NE(first_left, std::string::npos);
    EXPECT_NE(rendered.find("◀", first_left + 1), std::string::npos);
}

TEST(MarkdownMermaid, SequenceImplicitParticipantsKeepFirstAppearanceOrder) {
    const auto rendered = mermaid_document(
        "sequenceDiagram\nZ->>A: First\nparticipant A as Alpha\nA-->>Z: Reply");
    expect_diagram(rendered, "sequenceDiagram");
    EXPECT_LT(text_position(rendered, "Z").second, text_position(rendered, "Alpha").second);
    EXPECT_NE(rendered.find("Reply"), std::string::npos);
}

TEST(MarkdownMermaid, SequenceSolidAndDashedMessagesAreVisuallyDistinct) {
    const auto solid = mermaid_document("sequenceDiagram\nA->>B: Transfer");
    const auto dashed = mermaid_document("sequenceDiagram\nA-->>B: Transfer");
    expect_diagram(solid, "sequenceDiagram");
    expect_diagram(dashed, "sequenceDiagram");
    EXPECT_NE(solid, dashed);
}

TEST(MarkdownMermaid, SequenceUnclosedActivationRetainsItsOwnedTail) {
    const auto rendered = mermaid_document("sequenceDiagram\nA->>B: Request\nactivate B");
    expect_diagram(rendered, "sequenceDiagram");
    for (const auto* label : {"A", "B", "Request"})
        EXPECT_NE(rendered.find(label), std::string::npos);
    EXPECT_EQ(diagram_arrows(rendered), 1);
    EXPECT_NE(rendered.find("╺╋╸"), std::string::npos);
    EXPECT_NE(rendered.find("┃"), std::string::npos);
}

TEST(MarkdownMermaid, UnsupportedConstructsKeepEntireOriginalSource) {
    for (const std::string source : {
            "classDiagram\nclass PreservedClass\nclick PreservedClass \"https://example.com\"",
            "flowchart TD\nA --> B\nclick A \"https://example.com\"",
            "flowchart TD\nA[before &amp; after] --> B",
            "flowchart TD\nA[`rich`] --> B",
            "flowchart TD\nA[\"#quot;\"] --> B",
            "flowchart TD\nA[\"a\\\\b\"] --> B",
            "flowchart TD\nA --> end",
            "flowchart TD\nA --> B\nend",
            "flowchart TD\nA[\"a\u061cb\"] --> B",
            "flowchart TD\nA[\"a\u200eb\"] --> B",
            "flowchart TD\nA[\"a\u200fb\"] --> B",
            "flowchart TD\nA --> B\nA[one]\nA[two]",
            "flowchart TD\n%%{init: {}}%%\nA --> B",
            "flowchart TD\nA --> B residual",
            "sequenceDiagram\nloop->>A: Ping",
            "sequenceDiagram\nEND->>A: Ping",
            "sequenceDiagram\nparticipant Loop\nLoop->>A: Ping",
            "sequenceDiagram\nA->>B: Request\nnote left of B: retained\nunknown directive",
            "sequenceDiagram\nparticipant A as \"#quot;\"\nA->>B: Request"}) {
        SCOPED_TRACE(source);
        const auto expected = render_document(render_markdown("```text\n" + source + "\n```"));
        EXPECT_EQ(mermaid_document(source), expected);
    }
    expect_diagram(mermaid_document("flowchart TD\nA --> B\nsubgraph PreservedGroup\nC\nend"), "flowchart TD");
    expect_diagram(mermaid_document("flowchart TD\nA[before<br/>after] --> B"), "flowchart TD");
    expect_diagram(mermaid_document("flowchart TD\nA[\"end\"] --> B[Finish]"), "flowchart TD");
    expect_diagram(mermaid_document("sequenceDiagram\nparticipant A as loop\nA->>B: end"), "sequenceDiagram");
    // The source-identical semicolon case is now supported; the decoded
    // delimiter remains one message label rather than a second statement.
    const auto semicolon = mermaid_document("sequenceDiagram\nA->>B: before #59; A->>B: after");
    expect_diagram(semicolon,"sequenceDiagram");
    EXPECT_NE(semicolon.find("before ; A->>B: after"),std::string::npos);
    EXPECT_EQ(semicolon.find("#59;"),std::string::npos);
    EXPECT_EQ(diagram_arrows(semicolon),1);
}

TEST(MarkdownMermaid, MalformedOrIncompleteSyntaxKeepsSource) {
    for (const std::string source : {"", "flowchart TD", "flowchart UNKNOWN\nA --> B",
            "flowchart TD\nA[unclosed --> B", "flowchart TD\nA -->",
            "sequenceDiagram\nA->>B", "sequenceDiagram\nparticipant",
            "flowchart TD\nA[\"unterminated] --> B"}) {
        const auto expected = render_document(render_markdown("```text\n" + source + "\n```"));
        EXPECT_EQ(mermaid_document(source), expected) << source;
    }
}

TEST(MarkdownMermaid, OnlyClosedExactLanguageFencesRenderDiagrams) {
    const std::string body = "flowchart TD\nA[First] --> B[Second]";
    for (const auto* language : {"text", "MERMAID", "mermaid-extra"}) {
        EXPECT_NE(render_document(render_markdown(std::string("```") + language + "\n" + body + "\n```"))
            .find("flowchart TD"), std::string::npos);
    }
    EXPECT_NE(render_document(render_markdown("```mermaid\n" + body)).find("flowchart TD"), std::string::npos);
    expect_diagram(render_document(render_markdown(mermaid_fence(body))), "flowchart TD");
}

TEST(MarkdownMermaid, StreamingPrefixesReplacementAndResetPreserveLayout) {
    const std::string complete = mermaid_fence("flowchart LR\nA[Client] --> B[Server]");
    StreamingMarkdown streaming;
    for (std::size_t end = complete.find("flowchart") + std::string("flowchart LR").size();
            end < complete.size(); ++end) {
        const auto prefix = complete.substr(0, end);
        const auto element = streaming.update(prefix);
        EXPECT_EQ(render_document(element), render_document(render_markdown(prefix))) << end;
        EXPECT_NE(render_document(element).find("flowchart LR"), std::string::npos) << end;
    }
    const auto diagram = render_document(streaming.update(complete));
    expect_diagram(diagram, "flowchart LR");
    EXPECT_EQ(render_document(streaming.update(complete)), diagram);
    EXPECT_NE(render_document(streaming.update("replacement paragraph")).find("replacement paragraph"), std::string::npos);
    streaming.reset();
    EXPECT_EQ(render_document(streaming.update(complete)), diagram);
}

TEST(MarkdownMermaid, SameNodeResizesToSourceAndBackWithoutLosingFollowingText) {
    const auto markdown = mermaid_fence(
        "flowchart LR\nA[Long client label] -->|request| B[Long server label]") +
        "\n\nFOLLOWING_SENTINEL";
    const auto node = render_markdown(markdown);
    const auto wide = render_document(node, 160);
    expect_diagram(wide, "flowchart LR");
    EXPECT_NE(wide.find("FOLLOWING_SENTINEL"), std::string::npos);
    const auto narrow = render_document(node, 24);
    EXPECT_NE(narrow.find("flowchart LR"), std::string::npos) << narrow;
    EXPECT_NE(narrow.find("FOLLOWING_SENTINEL"), std::string::npos) << narrow;
    const auto plain_source = std::string("```text\nflowchart LR\nA[Long client label] -->|request| B[Long server label]\n```") +
        "\n\nFOLLOWING_SENTINEL";
    EXPECT_EQ(narrow, render_document(render_markdown(plain_source), 24));
    EXPECT_EQ(render_document(node, 160), wide);
}

TEST(MarkdownMermaid, NestedListAndQuoteFencesRetainClosureState) {
    for (const auto* prefix : {"> ", "  "}) {
        std::string markdown = std::string(prefix) == "  " ? "- item\n\n" : "";
        for (const auto* line : {"```mermaid", "flowchart TD", "A[First] --> B[Second]", "```"})
            markdown += std::string(prefix) + line + "\n";
        const auto rendered = render_document(render_markdown(markdown));
        expect_diagram(rendered, "flowchart TD");
        EXPECT_NE(rendered.find("First"), std::string::npos);
        EXPECT_NE(rendered.find("Second"), std::string::npos);
    }
}

TEST(MarkdownMermaid, ResourceLimitsPreserveSourceWithoutPartialGraph) {
    std::vector<std::string> sources;
    std::string many_nodes = "flowchart TD\n";
    for (int i = 0; i < 65; ++i) many_nodes += "N" + std::to_string(i) + "\n";
    sources.push_back(many_nodes);
    sources.push_back("flowchart TD\nA[" + std::string(257, 'x') + "] --> B");
    std::string many_messages = "sequenceDiagram\n";
    for (int i = 0; i < 129; ++i) many_messages += "A->>B: Message\n";
    sources.push_back(many_messages);
    sources.push_back("flowchart TD\n%%" + std::string(65536, 'x') + "\nA --> B");
    std::string many_edges = "flowchart TD\n";
    for (int i = 0; i < 129; ++i) many_edges += "A --> B\n";
    sources.push_back(many_edges);
    std::string many_participants = "sequenceDiagram\n";
    for (int i = 0; i < 33; ++i) many_participants += "participant P" + std::to_string(i) + "\n";
    sources.push_back(many_participants);
    sources.push_back("flowchart LR\nA[" + std::string(220, 'a') + "] --> B[" + std::string(220, 'b') + "]");
    for (const auto& source : sources) {
        SCOPED_TRACE(source.substr(0, 60));
        EXPECT_EQ(mermaid_document(source), render_document(render_markdown("```text\n" + source + "\n```")));
    }
}

TEST(MarkdownMermaid, HtmlSerializationKeepsSourceAndLanguage) {
    const auto markdown = mermaid_fence("graph TD\nA --> B");
    EXPECT_EQ(render_markdown_to_html(markdown),
              "<pre><code class=\"language-mermaid\">graph TD\nA --&gt; B\n</code></pre>\n");
}

TEST(MarkdownMermaid, CumulativeCanvasWorkBudgetFallsBackForOtherwiseAdmittedChain) {
    const auto chain = [](int count, int label_width) {
        std::string source = "graph TD\n";
        for (int i = 0; i < count; ++i)
            source += "N" + std::to_string(i) + "[" + std::string(label_width, 'x') + "]\n";
        for (int i = 0; i + 1 < count; ++i)
            source += "N" + std::to_string(i) + " --> N" + std::to_string(i + 1) + "\n";
        return source;
    };
    expect_diagram(mermaid_document(chain(8, 20)), "graph TD");
    // All parser limits and the natural 104 x 828 node layout fit. The
    // bounded routing-plane initialization plus drawing exhaust the one
    // shared per-block work budget, rather than resetting it for each edge.
    const auto bounded = chain(64, 100);
    ASSERT_LT(bounded.size(), 65536U);
    EXPECT_EQ(mermaid_document(bounded), render_document(render_markdown("```text\n" + bounded + "\n```")));
}

TEST(MarkdownMermaid, DiagramAfterLongPlainPrefaceStillUsesMarkdownParser) {
    const auto document = std::string(600, 'x') + "\n" +
        mermaid_fence("flowchart LR\nA[Client] --> B[Server]");
    const auto rendered = render_document(render_markdown(document));
    expect_diagram(rendered, "flowchart LR");
    EXPECT_NE(rendered.find("Client"), std::string::npos);
    EXPECT_NE(rendered.find("Server"), std::string::npos);
    EXPECT_EQ(diagram_arrows(rendered), 1);
}

TEST(MarkdownMermaid, FullSyntaxScanHandlesLongNumericRunsAndNumericPrefaces) {
    const std::string digits(65536, '7');
    EXPECT_FALSE(detail::has_markdown_syntax(digits));
    const auto fence = mermaid_fence("graph LR\nA[Client] --> B[Server]");
    EXPECT_TRUE(detail::has_markdown_syntax(digits + "\n" + fence));
    const auto rendered = render_document(render_markdown(digits.substr(0, 600) + "\n" + fence));
    expect_diagram(rendered, "graph LR");
    EXPECT_EQ(diagram_arrows(rendered), 1);
    EXPECT_TRUE(detail::has_markdown_syntax(digits + ". item"));
    EXPECT_TRUE(detail::has_markdown_syntax(digits + "\\"));
}

// RFC 0030 Phase B: these minimized regressions are derived from the pinned
// official corpus/grammar. They are not original upstream fixture passes.
TEST(MarkdownMermaid, OfficialFontAwesomeLabelsKeepSourceRatherThanLiteralIcons) {
    for (const auto* family : {"fa", "fab", "fak", "fal", "far", "fas"}) {
        for (const auto& source : {
                std::string("flowchart LR\nA[") + family + ":fa-car Car] --> B[Done]",
                std::string("flowchart LR\nA[\"") + family + ":fa-car Car\"] --> B[Done]",
                std::string("flowchart LR\nA -->|prefix ") + family + ":fa-car Car| B"}) {
            SCOPED_TRACE(source);
            EXPECT_EQ(mermaid_document(source),
                      render_document(render_markdown("```text\n" + source + "\n```")));
        }
    }
    expect_diagram(mermaid_document("flowchart LR\nA[fa:description] --> B[fa:fa-]"), "flowchart LR");
}

// Preserve the exact four former fallback controls as mandatory drawing
// controls when their classic shapes become supported. Models are authored
// directly from each full source; ordinary text never earns drawing credit.
TEST(MarkdownMermaid, FormerClassicShapeFallbacksRequireCompleteGeometry) {
    namespace flow = loom::test::mermaid_flow;
    struct Example { std::string_view source, label; flow::Shape shape; };
    constexpr std::array<Example,4> cases{{
        {"flowchart LR\nA[/Lean right/] --> B","Lean right",flow::Shape::Parallelogram},
        {"flowchart LR\nA[\\Lean left\\] --> B","Lean left",flow::Shape::ParallelogramAlt},
        {"flowchart LR\nA[/Trapezoid\\] --> B","Trapezoid",flow::Shape::Trapezoid},
        {"flowchart LR\nA[\\Inverse trapezoid/] --> B","Inverse trapezoid",flow::Shape::TrapezoidAlt},
    }};
    for (const auto& item:cases) {
        SCOPED_TRACE(item.source);
        flow::OracleWork work{flow::kFlow400SceneLimit};
        work.spend(65536u+32u*sizeof(flow::Grid)+32u*sizeof(flow::Model)+256u*sizeof(std::string));
        const auto fenced=[&](std::string_view language) {
            return "```"+std::string(language)+"\n"+std::string(item.source)+"\n```\n"+std::string(flow::kSentinel);
        };
        work.product(item.source.size(),32u);
        auto element=render_markdown(fenced("mermaid"));
        auto ordinary=render_markdown(fenced("text"));
        std::array<flow::Grid,5> frames;
        constexpr std::array<int,5> widths{{120,40,120,120,40}};
        for (std::size_t n=0;n<frames.size();++n)
            frames[n]=flow::capture(n<3u?element:ordinary,widths[n],32,&work);
        for (const auto& frame:frames) static_cast<void>(flow::flow400_following(frame,work));
        ASSERT_FALSE(flow::flow400_same(frames[0],frames[3],work));
        ASSERT_TRUE(flow::flow400_same(frames[0],frames[2],work));
        const flow::Model model{"LR",{{"A",std::string(item.label),item.shape},{"B","B",flow::Shape::Rectangle}},{{0,1,"","solid",false,true}},{}};
        for (std::size_t n=0;n<3u;++n) {
            if (n==1u&&flow::flow400_same(frames[1],frames[4],work)) continue;
            const auto errors=flow::flow400_geometry(frames[n],model,work);
            ASSERT_TRUE(errors.empty()) << flow::errors_text(errors);
        }
        flow::flow_typed_expect_model_rejected(frames[0],model,work,[](flow::Model& bad){bad.nodes[0].shape=flow::Shape::Rectangle;},"former fallback shape");
        flow::flow_typed_expect_model_rejected(frames[0],model,work,[](flow::Model& bad){bad.nodes[0].label+="x";},"former fallback caption");
        flow::flow_typed_expect_model_rejected(frames[0],model,work,[](flow::Model& bad){bad.edges[0].to=0;},"former fallback endpoint");
        ASSERT_EQ(frames[0].at({119,0})," ");
        flow::flow_typed_expect_rejected(frames[0],model,work,[](flow::Grid& bad){bad.put({119,0},"X");},"former fallback unowned physical ink");
    }
    RecordProperty("retained_complete_sources",4);
    RecordProperty("semantic_mutations",12);
    RecordProperty("physical_mutations",4);
}

TEST(MarkdownMermaid, OfficialUnsupportedShapeOpenersKeepSource) {
    const auto* source="flowchart LR\nA(-Ellipse-) --> B";
    EXPECT_EQ(mermaid_document(source),
              render_document(render_markdown(std::string("```text\n")+source+"\n```")));
    expect_diagram(mermaid_document("flowchart LR\nA[\"/data/\"] --> B(\"-literal-\")"),"flowchart LR");
}

TEST(MarkdownMermaid, SequenceWrapPrefixesRespectSourceCase) {
    for (const auto* prefix : {"WRAP:", "NoWrap:", ":WRAP:", ":NoWrap:"}) {
        for (const bool actor : {false, true}) {
            const std::string label = std::string(prefix) + (actor ? " Client" : " Message");
            const std::string source = actor
                ? "sequenceDiagram\nparticipant A as " + label + "\nA->>B: Request"
                : "sequenceDiagram\nA->>B: " + label;
            SCOPED_TRACE(source);
            const auto element = render_markdown(mermaid_fence(source));
            const auto rendered = render_document(element);
            expect_diagram(rendered, "sequenceDiagram");
            EXPECT_EQ(render_document(element, 12),
                      render_document(render_markdown("```text\n" + source + "\n```"), 12));
            EXPECT_EQ(render_document(element), rendered);
            EXPECT_NE(rendered.find(label), std::string::npos);
            EXPECT_EQ(diagram_arrows(rendered), 1);
        }
    }
    expect_diagram(mermaid_document("sequenceDiagram\nA->>B: Ordinary WRAP word"), "sequenceDiagram");
}

TEST(MarkdownMermaid, ReviewedSequenceTextAndReplacementRulesDrawCompleteLabels) {
    struct Example { std::string source, text, absent; };
    const std::array<Example, 10> examples{{
        {"sequenceDiagram\nparticipant A as \"Client\"\nA->>B: Request", "\"Client\"", ""},
        {"sequenceDiagram\nA->>B: \"Hello\"", "\"Hello\"", ""},
        {"sequenceDiagram\nA->>B: wrap: Hello", "Hello", "wrap:"},
        {"sequenceDiagram\nA->>B: nowrap: Hello", "Hello", "nowrap:"},
        {"sequenceDiagram\nA->>B: :wrap: Hello", "Hello", ":wrap:"},
        {"sequenceDiagram\nA->>B: :nowrap: Hello", "Hello", ":nowrap:"},
        {"sequenceDiagram\nA->>B: \"#35;\"", "\"#\"", "#35;"},
        {"sequenceDiagram\nparticipant A as \"a\u200fb\"\nA->>B: Request", "\"a\\u{200F}b\"", "a\u200fb"},
        {"sequenceDiagram\nA->>B: \"a\u061cb\"", "\"a\\u{061C}b\"", "a\u061cb"},
        {"sequenceDiagram\nparticipant A as One\nparticipant A as Two\nA->>B: Request", "Two", "One"},
    }};
    for (const auto& example : examples) {
        SCOPED_TRACE(example.source);
        const auto element = render_markdown(mermaid_fence(example.source));
        const auto rendered = render_document(element);
        expect_diagram(rendered, "sequenceDiagram");
        EXPECT_EQ(render_document(element, 12),
                  render_document(render_markdown("```text\n" + example.source + "\n```"), 12));
        EXPECT_EQ(render_document(element), rendered);
        EXPECT_NE(rendered.find(example.text), std::string::npos);
        if (!example.absent.empty()) EXPECT_EQ(rendered.find(example.absent), std::string::npos);
        EXPECT_EQ(diagram_arrows(rendered), 1);
        if (example.source.find("Request") != std::string::npos)
            EXPECT_NE(rendered.find("Request"), std::string::npos);
    }
}
