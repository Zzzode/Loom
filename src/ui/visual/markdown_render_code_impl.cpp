// markdown_render_code_impl.cpp - impl unit for loom.ui.visual.markdown
// (RFC 0001 Phase C batch 6). Holds render_code_block(), the ONLY body that
// uses loom.ui.visual.code_highlight: keeping that import here (instead of in
// the cppm) removes code_highlight (+ its loom.types.types closure) from the
// markdown BMI and from every importer of loom.ui.visual.markdown.
module;

#include <ftxui/dom/elements.hpp>

module loom.ui.visual.markdown;

import std;

import loom.ui.visual.code_highlight;

namespace loom::ui {
namespace detail {

[[nodiscard]] Element render_code_block(const BlockToken& tok,
                                        const MarkdownOptions& opts) {
    // Split into lines (preserve trailing empty line behavior like EOL).
    auto split_lines = [](std::string_view code) {
        std::vector<std::string> lines;
        std::size_t start = 0;
        while (start <= code.size()) {
            auto nl = code.find('\n', start);
            if (nl == std::string_view::npos) {
                lines.emplace_back(code.substr(start));
                break;
            }
            lines.emplace_back(code.substr(start, nl - start));
            start = nl + 1;
        }
        return lines;
    };

    auto lines = split_lines(tok.code_content);

    if (opts.syntax_highlighting && !tok.code_content.empty()) {
        auto highlighted = code_highlight::highlight_source(
            tok.code_content, tok.code_lang);
        auto theme_result = code_highlight::get_syntax_theme(opts.theme_name);
        auto theme = theme_result ? *theme_result
                                  : code_highlight::get_theme(
                                        code_highlight::ThemeName::Dark);

        Elements out;
        for (const auto& hl : highlighted.lines) {
            // Chrome-free: no line-number gutter, no diff background, no
            // highlight background. Just the colored tokens of each line.
            //
            // colored_text_line (not hbox of text()): hbox routes overwidth
            // lines through box_helper::ComputeShrinkHard, which shrinks
            // every token proportionally and swallows characters from the
            // middle. colored_text_line keeps each token full-width and
            // truncates only at the right edge.
            std::vector<code_highlight::ColoredSegment> segments;
            if (hl.tokens.empty()) {
                segments.push_back({.text = ""});
            } else {
                for (const auto& t : hl.tokens) {
                    segments.push_back({
                        .text = t.text,
                        .foreground = code_highlight::token_color(
                            t.type, theme),
                    });
                }
            }
            Element line_el = code_highlight::colored_text_line(
                std::move(segments));
            if (opts.dim_color) line_el = line_el | dim;
            out.push_back(std::move(line_el));
        }
        // highlight_source emits one line per source line; if the tokenizer
        // produced fewer (shouldn't), fall back to plain lines.
        if (out.size() == lines.size()) {
            return vbox(std::move(out));
        }
    }

    // Plain code block (no highlighting or tokenizer mismatch).
    Elements out;
    for (const auto& line : lines) {
        Element el = text(line);
        if (opts.dim_color) el = el | dim;
        out.push_back(std::move(el));
    }
    return vbox(std::move(out));
}
} // namespace detail
} // namespace loom::ui
