// markdown_render_impl.cpp - impl unit for loom.ui.visual.markdown (RFC 0001
// Phase C batch 6). Holds the FTXUI block/inline renderers: render_inlines,
// render_heading, the depth numbering helpers (number_to_letter,
// number_to_roman, get_list_number - in the pre-existing nested
// loom::ui::detail::detail namespace), render_ulist/olist/blockquote/table/hr.
// render_code_block lives in markdown_render_code_impl.cpp (it pulls in
// loom.ui.visual.code_highlight).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.visual.markdown;

import std;

namespace loom::ui {
namespace detail {

// Decode one UTF-8 codepoint at |i| in |text|.  Returns the codepoint and
// advances |i| past the character.  Returns 0 and advances by 1 on invalid
// encoding (so the loop always makes progress).
static uint32_t decode_utf8(std::string_view text, std::size_t& i) {
    unsigned char c = text[i];
    uint32_t cp = 0;
    std::size_t len = 0;

    if (c < 0x80) {
        cp = c;
        len = 1;
    } else if (c < 0xC0) {
        ++i;
        return 0;  // continuation byte without lead
    } else if (c < 0xE0) {
        cp = c & 0x1F;
        len = 2;
    } else if (c < 0xF0) {
        cp = c & 0x0F;
        len = 3;
    } else {
        cp = c & 0x07;
        len = 4;
    }

    if (i + len > text.size()) {
        ++i;
        return 0;  // truncated
    }

    for (std::size_t j = 1; j < len; ++j) {
        if ((text[i + j] & 0xC0) != 0x80) {
            ++i;
            return 0;  // invalid continuation
        }
        cp = (cp << 6) | (text[i + j] & 0x3F);
    }

    i += len;
    return cp;
}

namespace {

// A zero-size marker element inserted at hard line breaks (CommonMark
// §6.1: two+ trailing spaces or a trailing backslash before \n).
// render_inlines() splits the element list at these markers and stacks
// each group in a separate flexbox row inside a vbox.
class HardBreakNode : public ftxui::Node {
public:
    HardBreakNode() = default;

    void ComputeRequirement() override {
        requirement_.min_x = 0;
        requirement_.min_y = 0;
    }

    void Render(ftxui::Screen&) override {
        // Layout marker only — nothing to draw.
    }
};

// A zero-size node for empty blocks (e.g. HTML blocks that strip to
// empty).  VBox gives it zero height so it doesn't occupy a row.
class EmptyNode : public ftxui::Node {
public:
    EmptyNode() = default;

    void ComputeRequirement() override {
        requirement_.min_x = 0;
        requirement_.min_y = 0;
    }

    void Render(ftxui::Screen&) override {
        // Nothing to draw.
    }
};

// A node that prefixes every row of its child with a blockquote bar
// (▎ U+258E + space).  A plain hbox({text("▎ "), content}) only shows
// the bar on the first line because Text::Render draws at box_.y_min
// only.  This node draws the bar on every row in its box so
// multi-line blockquotes keep the bar on continuation lines.
class BlockquoteBar : public ftxui::Node {
public:
    BlockquoteBar(ftxui::Element child, std::string bar)
        : ftxui::Node(ftxui::Elements{std::move(child)}),
          bar_(std::move(bar)),
          bar_width_(static_cast<int>(ftxui::string_width(bar_))) {}

    void ComputeRequirement() override {
        children_[0]->ComputeRequirement();
        requirement_ = children_[0]->requirement();
        requirement_.min_x += bar_width_;
    }

    void SetBox(ftxui::Box box) override {
        ftxui::Node::SetBox(box);
        ftxui::Box child_box = box;
        child_box.x_min = box.x_min + bar_width_;
        children_[0]->SetBox(child_box);
    }

    void Render(ftxui::Screen& screen) override {
        // Draw the bar at the start of every row.
        for (int y = box_.y_min; y <= box_.y_max; ++y) {
            int x = box_.x_min;
            for (const auto& glyph : ftxui::Utf8ToGlyphs(bar_)) {
                if (x > box_.x_max) break;
                auto& pixel = screen.PixelAt(x, y);
                pixel.character = glyph;
                pixel.dim = true;
                ++x;
            }
        }
        // Draw the child content (offset by bar_width_ via SetBox).
        ftxui::Node::Render(screen);
    }

private:
    std::string bar_;
    int bar_width_;
};

// A piece of inline text between hard line breaks.
struct HardBreakPiece {
    std::string text;
    bool hard_break_after = false;
};

// Split text on hard line breaks (CommonMark §6.1).  A '\n' preceded by
// two+ trailing spaces or a trailing backslash is a hard break (forced
// line break); the markers are stripped.  A regular '\n' is a soft
// break (replaced with a space).  Trailing spaces before a soft break
// and leading spaces on the continuation line are stripped (CommonMark
// §6.1: they are insignificant paragraph indentation).
[[nodiscard]] std::vector<HardBreakPiece> split_on_hard_breaks(
    std::string_view text) {
    std::vector<HardBreakPiece> pieces;
    std::string current;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        if (nl == std::string_view::npos) {
            current += text.substr(pos);
            break;
        }
        current += text.substr(pos, nl - pos);
        bool hard = false;
        if (current.size() >= 2 &&
            current[current.size() - 1] == ' ' &&
            current[current.size() - 2] == ' ') {
            hard = true;
            // Strip ALL trailing spaces (CommonMark §6.1: the hard break
            // marker is two+ trailing spaces; all are part of the marker).
            while (!current.empty() && current.back() == ' ') {
                current.pop_back();
            }
        } else if (!current.empty() && current.back() == '\\') {
            hard = true;
            current.pop_back();
        }
        if (hard) {
            pieces.push_back({std::move(current), true});
            current.clear();
        } else {
            // Soft break: strip insignificant trailing spaces, then append
            // a single space (CommonMark §6.1: soft breaks render as spaces).
            while (!current.empty() && current.back() == ' ') {
                current.pop_back();
            }
            current += ' ';
        }
        pos = nl + 1;
        // Skip leading spaces on the continuation line (CommonMark trims
        // paragraph indentation of up to 3 spaces; skipping all is safe
        // because the lexer has already validated the indentation).
        while (pos < text.size() && text[pos] == ' ') {
            ++pos;
        }
    }
    pieces.push_back({std::move(current), false});
    return pieces;
}

} // namespace

// at_paragraph_start is true only before the first non-space character of
// the entire paragraph (shared across all per-token calls via a reference
// in render_inlines_to_elements).  It distinguishes a paragraph-leading
// space (CommonMark trims these — safe to skip) from an inter-token space
// (e.g. Text(" end") after Bold("world")) which must be preserved.
//
// trim_leading_space controls whether paragraph-leading spaces are dropped.
// Code spans and other content tokens where spaces are significant pass
// false so their leading spaces are preserved.
std::vector<std::string> split_for_wrapping(std::string_view text,
                                            bool& at_paragraph_start,
                                            bool trim_leading_space) {
    std::vector<std::string> segments;
    std::string current;

    std::size_t i = 0;
    while (i < text.size()) {
        std::size_t start = i;
        uint32_t cp = decode_utf8(text, i);
        if (cp == 0 && i == start + 1) {
            // Invalid byte — treat as a single-char segment so it is
            // preserved rather than swallowed.
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
            segments.push_back(std::string(1, text[start]));
            at_paragraph_start = false;
            continue;
        }

        std::string char_str(text.substr(start, i - start));

        if (cp == ' ' || cp == '\t' || cp == '\n') {
            // Merge the space into the preceding word segment instead of
            // emitting a standalone " " segment.  Standalone spaces are
            // zero-min-width flexbox items that get pushed to the start
            // of the next wrapped line, leaving a visible leading space.
            if (!current.empty()) {
                current += ' ';
                segments.push_back(std::move(current));
                current.clear();
                at_paragraph_start = false;
            } else if (!segments.empty()) {
                // current is empty after a CJK char (pushed immediately);
                // append the space to the last segment so it is not lost.
                segments.back() += ' ';
                at_paragraph_start = false;
            } else if (at_paragraph_start && trim_leading_space) {
                // Paragraph-leading space — CommonMark trims these.
            } else {
                // Inter-token space (e.g. Text(" end") after Bold("world"))
                // or code-span leading space preserved by R2: emit as a
                // standalone segment so the space is not dropped.  Set
                // at_paragraph_start = false — we have emitted content, so
                // a subsequent soft-break space must not be trimmed as a
                // paragraph-leading space.
                segments.push_back(" ");
                at_paragraph_start = false;
            }
        } else if (ftxui::string_width(char_str) == 2) {
            // Full-width character (CJK, etc.) — each is its own breakable
            // segment so flexbox can wrap between any two CJK chars.
            if (!current.empty()) {
                segments.push_back(std::move(current));
                current.clear();
            }
            segments.push_back(std::move(char_str));
            at_paragraph_start = false;
        } else {
            current += char_str;
            at_paragraph_start = false;
        }
    }

    if (!current.empty()) {
        segments.push_back(std::move(current));
    }

    return segments;
}

// Render inline tokens to a flat list of styled text() elements, each small
// enough for flexbox to wrap at (a word, a space, or a single CJK char).
// This is the inner loop of render_inlines(); callers that need to merge
// child tokens (Bold/Italic with nested children) call this recursively.
// at_paragraph_start tracks whether any non-space character has been emitted
// yet, so split_for_wrapping can distinguish a paragraph-leading space
// (skipped) from an inter-token space (preserved).
[[nodiscard]] static Elements render_inlines_to_elements(
    const std::vector<InlineToken>& tokens,
    const MarkdownOptions& opts,
    bool& at_paragraph_start) {

    Elements elements;
    for (const auto& tok : tokens) {
        switch (tok.kind) {
            case InlineTokenKind::Text: {
                // linkifyIssueReferences — applied
                // to plain text tokens so owner/repo#NNN becomes clickable.
                // Paragraph lines are joined with '\n'; detect hard breaks
                // (two+ trailing spaces or trailing backslash) and insert
                // HardBreakNode markers so render_inlines() can split rows.
                auto pieces = split_on_hard_breaks(tok.text);
                for (std::size_t p = 0; p < pieces.size(); ++p) {
                    const auto& piece = pieces[p];
                    auto segments = linkify_issue_references(piece.text);
                    for (auto& seg : segments) {
                        auto words = split_for_wrapping(seg.text,
                                                        at_paragraph_start);
                        for (auto& word : words) {
                            Element el;
                            if (!seg.url.empty()) {
                                el = ftxui::text(word) | hyperlink(seg.url) |
                                     underlined;
                            } else {
                                el = ftxui::text(word);
                            }
                            if (opts.dim_color) el = std::move(el) | dim;
                            elements.push_back(std::move(el));
                        }
                    }
                    if (piece.hard_break_after) {
                        elements.push_back(
                            std::make_shared<HardBreakNode>());
                    }
                }
                break;
            }
            case InlineTokenKind::Bold: {
                if (!tok.children.empty()) {
                    // Nested emphasis (e.g. **_text_**) — render children
                    // with bold applied to every segment.  HardBreakNode
                    // markers must NOT be decorated (the decorator would
                    // wrap them and hide the dynamic_cast in render_inlines).
                    for (const auto& child : tok.children) {
                        auto child_elements =
                            render_inlines_to_elements({child}, opts,
                                                       at_paragraph_start);
                        for (auto& el : child_elements) {
                            if (!dynamic_cast<const HardBreakNode*>(
                                    el.get())) {
                                el = std::move(el) | bold;
                                if (opts.dim_color)
                                    el = std::move(el) | dim;
                            }
                            elements.push_back(std::move(el));
                        }
                    }
                } else {
                    auto pieces = split_on_hard_breaks(tok.text);
                    for (std::size_t p = 0; p < pieces.size(); ++p) {
                        const auto& piece = pieces[p];
                        auto words = split_for_wrapping(piece.text,
                                                        at_paragraph_start);
                        for (auto& word : words) {
                            Element el = ftxui::text(word) | bold;
                            if (opts.dim_color) el = el | dim;
                            elements.push_back(std::move(el));
                        }
                        if (piece.hard_break_after) {
                            elements.push_back(
                                std::make_shared<HardBreakNode>());
                        }
                    }
                }
                break;
            }
            case InlineTokenKind::Italic: {
                if (!tok.children.empty()) {
                    for (const auto& child : tok.children) {
                        auto child_elements =
                            render_inlines_to_elements({child}, opts,
                                                       at_paragraph_start);
                        for (auto& el : child_elements) {
                            if (!dynamic_cast<const HardBreakNode*>(
                                    el.get())) {
                                el = std::move(el) | dim;
                                if (opts.dim_color)
                                    el = std::move(el) | dim;
                            }
                            elements.push_back(std::move(el));
                        }
                    }
                } else {
                    // Italic. FTXUI has no italic Screen style, so we use
                    // dim as the closest visual proxy.
                    auto pieces = split_on_hard_breaks(tok.text);
                    for (std::size_t p = 0; p < pieces.size(); ++p) {
                        const auto& piece = pieces[p];
                        auto words = split_for_wrapping(piece.text,
                                                        at_paragraph_start);
                        for (auto& word : words) {
                            Element el = ftxui::text(word) | dim;
                            if (opts.dim_color) el = el | dim;
                            elements.push_back(std::move(el));
                        }
                        if (piece.hard_break_after) {
                            elements.push_back(
                                std::make_shared<HardBreakNode>());
                        }
                    }
                }
                break;
            }
            case InlineTokenKind::Code: {
                // codespan -> color('permission', theme). Dark theme
                // permission = rgb(87,105,247). No background inversion.
                // Code spans preserve leading spaces (they are content,
                // not paragraph indentation).
                auto words = split_for_wrapping(tok.text, at_paragraph_start,
                                                /*trim_leading_space=*/false);
                for (auto& word : words) {
                    Element el = ftxui::text(word) |
                                 color(Color::RGB(87, 105, 247));
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
            case InlineTokenKind::Link: {
                if (!tok.children.empty()) {
                    // Render parsed children (handles escapes, emphasis,
                    // code spans inside link text) with hyperlink +
                    // underline applied to every segment.  HardBreakNode
                    // markers must NOT be decorated (the decorator would
                    // wrap them and hide the dynamic_cast in render_inlines).
                    for (const auto& child : tok.children) {
                        // Linked images ([![alt](img)](url)): render the alt
                        // text as plain text so the outer link's URL takes
                        // precedence over the image's own URL.
                        if (child.kind == InlineTokenKind::Image) {
                            auto words = split_for_wrapping(
                                child.text, at_paragraph_start);
                            for (auto& word : words) {
                                Element el = ftxui::text(word) |
                                             hyperlink(tok.url) | underlined;
                                if (opts.dim_color) el = el | dim;
                                elements.push_back(std::move(el));
                            }
                            continue;
                        }
                        // Inline HTML inside link text: render as literal
                        // text (not stripped) so tags like <b> are visible.
                        if (child.kind == InlineTokenKind::HtmlRaw) {
                            auto words = split_for_wrapping(
                                child.text, at_paragraph_start);
                            for (auto& word : words) {
                                Element el = ftxui::text(word) |
                                             hyperlink(tok.url) | underlined;
                                if (opts.dim_color) el = el | dim;
                                elements.push_back(std::move(el));
                            }
                            continue;
                        }
                        auto child_elements =
                            render_inlines_to_elements({child}, opts,
                                                       at_paragraph_start);
                        for (auto& el : child_elements) {
                            if (!dynamic_cast<const HardBreakNode*>(
                                    el.get())) {
                                el = std::move(el) | hyperlink(tok.url) |
                                     underlined;
                                if (opts.dim_color)
                                    el = std::move(el) | dim;
                            }
                            elements.push_back(std::move(el));
                        }
                    }
                } else {
                    // Fallback: render raw text when no parsed children.
                    auto words = split_for_wrapping(tok.text,
                                                    at_paragraph_start);
                    for (auto& word : words) {
                        Element el = ftxui::text(word) | hyperlink(tok.url) |
                                     underlined;
                        if (opts.dim_color) el = el | dim;
                        elements.push_back(std::move(el));
                    }
                }
                break;
            }
            case InlineTokenKind::Escape: {
                auto words = split_for_wrapping(tok.text, at_paragraph_start);
                for (auto& word : words) {
                    Element el = ftxui::text(word);
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
            case InlineTokenKind::Image: {
                // Terminal can't render images; show the alt text as a
                // hyperlink to the source URL.
                auto words = split_for_wrapping(tok.text, at_paragraph_start);
                for (auto& word : words) {
                    Element el = ftxui::text(word) | hyperlink(tok.url) |
                                 color(Color::RGB(87, 105, 247));
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
            case InlineTokenKind::Strikethrough: {
                auto words = split_for_wrapping(tok.text, at_paragraph_start);
                for (auto& word : words) {
                    Element el = ftxui::text(word) | strikethrough;
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
            case InlineTokenKind::Math: {
                // Inline LaTeX math ($...$).  Rendered via KaTeX → HTML in
                // the web UI; in a terminal we approximate with a distinct cyan color.
                auto words = split_for_wrapping(tok.text, at_paragraph_start);
                for (auto& word : words) {
                    Element el = ftxui::text(word) | color(Color::CyanLight);
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
            case InlineTokenKind::HtmlRaw:
                // Raw HTML tags are markup, not content — strip them in
                // the terminal renderer.
                break;
            case InlineTokenKind::FootnoteRef: {
                // GFM footnote ref: [^label].  The terminal renderer has
                // no footnotes section; show the literal label.
                std::string label = "[^" + tok.text + "]";
                auto words = split_for_wrapping(label, at_paragraph_start);
                for (auto& word : words) {
                    Element el = ftxui::text(word) | dim;
                    if (opts.dim_color) el = el | dim;
                    elements.push_back(std::move(el));
                }
                break;
            }
        }
    }
    return elements;
}

[[nodiscard]] Element render_inlines(
    const std::vector<InlineToken>& tokens,
    const MarkdownOptions& opts) {

    bool at_paragraph_start = true;
    auto elements = render_inlines_to_elements(tokens, opts,
                                               at_paragraph_start);
    if (elements.empty()) {
        return text("");
    }

    // Split the element list at HardBreakNode markers.  Each group becomes
    // one flexbox row; rows are stacked in a vbox so hard breaks produce
    // visible line breaks.
    Elements rows;
    Elements current_row;
    for (auto& el : elements) {
        if (dynamic_cast<const HardBreakNode*>(el.get())) {
            if (!current_row.empty()) {
                rows.push_back(
                    flexbox(std::move(current_row), FlexboxConfig()));
                current_row.clear();
            } else {
                // Hard break at the start or consecutive hard breaks —
                // push an empty row to preserve the break.
                rows.push_back(text(""));
            }
        } else {
            current_row.push_back(std::move(el));
        }
    }
    if (!current_row.empty()) {
        rows.push_back(flexbox(std::move(current_row), FlexboxConfig()));
    }

    if (rows.size() == 1) {
        return std::move(rows[0]);
    }
    return vbox(std::move(rows));
}

[[nodiscard]] Element render_heading(const BlockToken& tok,
                                     const MarkdownOptions& opts) {
    auto el = render_inlines(tok.inlines, opts) | bold;
    if (tok.heading_level == 1) {
        // Bold + italic + underline. FTXUI lacks italic, so bold+underline
        // is the closest visual proxy (the bold already differentiates h1
        // from body text; underline mirrors the italic attribute).
        el = el | underlined;
    }
    // h2 and h3+ are plain bold (no extra attributes).
    return vbox({el, text("")});
}

namespace detail {

[[nodiscard]] std::string number_to_letter(int n) {
    if (n <= 0) return "a";
    std::string result;
    while (n > 0) {
        --n;  // 0-indexed: 0→'a', 25→'z'
        result = static_cast<char>('a' + (n % 26)) + result;
        n /= 26;
    }
    return result;
}

[[nodiscard]] std::string number_to_roman(int n) {
    if (n <= 0 || n > 3999) return std::to_string(n);

    struct RomanPair { int value; const char* symbol; };
    static constexpr RomanPair roman_map[] = {
        {1000, "m"}, {900, "cm"}, {500, "d"}, {400, "cd"},
        {100, "c"},  {90, "xc"},  {50, "l"},  {40, "xl"},
        {10, "x"},   {9, "ix"},   {5, "v"},   {4, "iv"},
        {1, "i"}
    };

    std::string result;
    for (const auto& [value, symbol] : roman_map) {
        while (n >= value) {
            result += symbol;
            n -= value;
        }
    }
    return result;
}

[[nodiscard]] std::string get_list_number(int depth, int n) {
    switch (depth) {
        case 0:
        case 1:
            return std::to_string(n);
        case 2:
            return number_to_letter(n);
        case 3:
            return number_to_roman(n);
        default:
            // Depth 4+ wraps back to Arabic, matching the default case.
            return std::to_string(n);
    }
}

} // namespace detail

// Render a sub-block as an Element (shared by blockquote and list items).
[[nodiscard]] Element render_block_element(const BlockToken& sub,
                                           const MarkdownOptions& opts,
                                           int depth = 0) {
    switch (sub.kind) {
        case BlockTokenKind::Paragraph:
            return render_inlines(sub.inlines, opts);
        case BlockTokenKind::Heading:
            return render_heading(sub, opts);
        case BlockTokenKind::CodeBlock:
            return render_code_block(sub, opts);
        case BlockTokenKind::UnorderedList:
            return render_ulist(sub, opts, depth);
        case BlockTokenKind::OrderedList:
            return render_olist(sub, opts, depth);
        case BlockTokenKind::Blockquote:
            return render_blockquote(sub, opts);
        case BlockTokenKind::Table:
            return render_table(sub, opts);
        case BlockTokenKind::HorizontalRule:
            return render_hr(opts);
        case BlockTokenKind::HtmlBlock:
            return render_html_block(sub, opts);
        case BlockTokenKind::MathBlock:
            return text(sub.math_content) | color(Color::CyanLight);
        case BlockTokenKind::FootnoteDef:
            // Footnote definitions are hoisted to the footnotes section
            // (HTML serializer only); no terminal output here.
            return text("");
    }
    return text("");
}

[[nodiscard]] Element render_ulist(const BlockToken& tok,
                                   const MarkdownOptions& opts,
                                   int depth) {
    Elements items;
    for (std::size_t i = 0; i < tok.list_items.size(); ++i) {
        // Indent prefix: '  ' × depth
        std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
        Element indent_el = text(indent);
        if (opts.dim_color) indent_el = indent_el | dim;

        Element bullet = text("- ");
        if (opts.dim_color) bullet = bullet | dim;

        // GFM task list checkbox
        const auto& tasks = tok.task_state;
        Element checkbox = text("");
        if (i < tasks.size() && tasks[i] >= 0) {
            checkbox = text(tasks[i] == 1 ? "[x] " : "[ ] ");
            if (opts.dim_color) checkbox = checkbox | dim;
        }

        const auto& sub_blocks = tok.list_items[i];
        Elements item_parts;
        for (std::size_t j = 0; j < sub_blocks.size(); ++j) {
            const auto& sub = sub_blocks[j];
            if (j == 0 && sub.kind == BlockTokenKind::Paragraph) {
                // First paragraph: render with bullet prefix
                item_parts.push_back(hbox({
                    indent_el, bullet, checkbox,
                    render_inlines(sub.inlines, opts)
                }));
            } else if (j == 0) {
                // First block is not a paragraph (code block, blockquote,
                // etc.): emit the marker on its own line so the list item
                // is visible, then render the sub-block with continuation
                // indent.
                item_parts.push_back(hbox({indent_el, bullet, checkbox}));
                if (sub.kind == BlockTokenKind::UnorderedList ||
                    sub.kind == BlockTokenKind::OrderedList) {
                    item_parts.push_back(
                        render_block_element(sub, opts, depth + 1));
                } else {
                    std::string cont_indent(static_cast<std::size_t>(depth) * 2 + 2, ' ');
                    Element cont_el = text(cont_indent);
                    if (opts.dim_color) cont_el = cont_el | dim;
                    item_parts.push_back(hbox({
                        cont_el,
                        render_block_element(sub, opts, depth)
                    }));
                }
            } else if (sub.kind == BlockTokenKind::UnorderedList ||
                       sub.kind == BlockTokenKind::OrderedList) {
                // Nested list: render recursively with depth + 1
                item_parts.push_back(
                    render_block_element(sub, opts, depth + 1));
            } else {
                // Other blocks: render with continuation indent
                std::string cont_indent(static_cast<std::size_t>(depth) * 2 + 2, ' ');
                Element cont_el = text(cont_indent);
                if (opts.dim_color) cont_el = cont_el | dim;
                item_parts.push_back(hbox({
                    cont_el,
                    render_block_element(sub, opts, depth)
                }));
            }
        }
        if (item_parts.empty()) {
            // Empty list item: emit bare marker so the item is visible.
            items.push_back(hbox({indent_el, bullet, checkbox}));
        } else if (item_parts.size() == 1) {
            items.push_back(std::move(item_parts[0]));
        } else {
            items.push_back(vbox(std::move(item_parts)));
        }
    }
    // Loose lists (blank lines between items in the source) render with
    // blank lines between items to match CommonMark §5.3, which wraps
    // loose-list item content in <p> tags.
    if (tok.list_loose && items.size() > 1) {
        Elements spaced;
        spaced.reserve(items.size() * 2 - 1);
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i > 0) spaced.push_back(text(""));
            spaced.push_back(std::move(items[i]));
        }
        return vbox(std::move(spaced));
    }
    return vbox(std::move(items));
}

[[nodiscard]] Element render_olist(const BlockToken& tok,
                                   const MarkdownOptions& opts,
                                   int depth) {
    Elements items;
    int counter = tok.list_start;

    for (std::size_t i = 0; i < tok.list_items.size(); ++i) {
        std::string label = detail::get_list_number(depth, counter) + ". ";
        ++counter;

        // Indent prefix: '  ' × depth
        std::string indent(static_cast<std::size_t>(depth) * 2, ' ');
        Element indent_el = text(indent);
        if (opts.dim_color) indent_el = indent_el | dim;

        Element num_el = text(label);
        if (opts.dim_color) num_el = num_el | dim;

        // GFM task list checkbox
        const auto& tasks = tok.task_state;
        Element checkbox = text("");
        if (i < tasks.size() && tasks[i] >= 0) {
            checkbox = text(tasks[i] == 1 ? "[x] " : "[ ] ");
            if (opts.dim_color) checkbox = checkbox | dim;
        }

        const auto& sub_blocks = tok.list_items[i];
        Elements item_parts;
        for (std::size_t j = 0; j < sub_blocks.size(); ++j) {
            const auto& sub = sub_blocks[j];
            if (j == 0 && sub.kind == BlockTokenKind::Paragraph) {
                // First paragraph: render with number prefix
                item_parts.push_back(hbox({
                    indent_el, num_el, checkbox,
                    render_inlines(sub.inlines, opts)
                }));
            } else if (j == 0) {
                // First block is not a paragraph (code block, blockquote,
                // etc.): emit the marker on its own line so the list item
                // is visible, then render the sub-block with continuation
                // indent.
                item_parts.push_back(hbox({indent_el, num_el, checkbox}));
                if (sub.kind == BlockTokenKind::UnorderedList ||
                    sub.kind == BlockTokenKind::OrderedList) {
                    item_parts.push_back(
                        render_block_element(sub, opts, depth + 1));
                } else {
                    std::string cont_indent(static_cast<std::size_t>(depth) * 2 + 2, ' ');
                    Element cont_el = text(cont_indent);
                    if (opts.dim_color) cont_el = cont_el | dim;
                    item_parts.push_back(hbox({
                        cont_el,
                        render_block_element(sub, opts, depth)
                    }));
                }
            } else if (sub.kind == BlockTokenKind::UnorderedList ||
                       sub.kind == BlockTokenKind::OrderedList) {
                // Nested list: render recursively with depth + 1
                item_parts.push_back(
                    render_block_element(sub, opts, depth + 1));
            } else {
                // Other blocks: render with continuation indent
                std::string cont_indent(static_cast<std::size_t>(depth) * 2 + 2, ' ');
                Element cont_el = text(cont_indent);
                if (opts.dim_color) cont_el = cont_el | dim;
                item_parts.push_back(hbox({
                    cont_el,
                    render_block_element(sub, opts, depth)
                }));
            }
        }
        if (item_parts.empty()) {
            // Empty list item: emit bare marker so the item is visible.
            items.push_back(hbox({indent_el, num_el, checkbox}));
        } else if (item_parts.size() == 1) {
            items.push_back(std::move(item_parts[0]));
        } else {
            items.push_back(vbox(std::move(item_parts)));
        }
    }
    // Loose lists (blank lines between items in the source) render with
    // blank lines between items to match CommonMark §5.3, which wraps
    // loose-list item content in <p> tags.
    if (tok.list_loose && items.size() > 1) {
        Elements spaced;
        spaced.reserve(items.size() * 2 - 1);
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i > 0) spaced.push_back(text(""));
            spaced.push_back(std::move(items[i]));
        }
        return vbox(std::move(spaced));
    }
    return vbox(std::move(items));
}

[[nodiscard]] Element render_blockquote(const BlockToken& tok,
                                        const MarkdownOptions& opts) {
    // When sub_blocks is non-empty, the blockquote contains block-level
    // content (HTML blocks, lists, nested quotes, etc.) — render each
    // sub-block and stack them with the bar prefix.
    if (!tok.sub_blocks.empty()) {
        Elements parts;
        for (const auto& sub : tok.sub_blocks) {
            switch (sub.kind) {
                case BlockTokenKind::Paragraph:
                    parts.push_back(render_inlines(sub.inlines, opts));
                    break;
                case BlockTokenKind::Heading:
                    parts.push_back(render_heading(sub, opts));
                    break;
                case BlockTokenKind::CodeBlock:
                    parts.push_back(render_code_block(sub, opts));
                    break;
                case BlockTokenKind::UnorderedList:
                    parts.push_back(render_ulist(sub, opts));
                    break;
                case BlockTokenKind::OrderedList:
                    parts.push_back(render_olist(sub, opts));
                    break;
                case BlockTokenKind::Blockquote:
                    parts.push_back(render_blockquote(sub, opts));
                    break;
                case BlockTokenKind::Table:
                    parts.push_back(render_table(sub, opts));
                    break;
                case BlockTokenKind::HorizontalRule:
                    parts.push_back(render_hr(opts));
                    break;
                case BlockTokenKind::HtmlBlock:
                    parts.push_back(render_html_block(sub, opts));
                    break;
                case BlockTokenKind::MathBlock:
                    parts.push_back(text(sub.math_content) | color(Color::CyanLight));
                    break;
                case BlockTokenKind::FootnoteDef:
                    // Hoisted to footnotes section (HTML serializer only).
                    break;
            }
        }
        if (parts.empty()) return text("");
        Element content_el = vbox(std::move(parts));
        return std::make_shared<BlockquoteBar>(
            std::move(content_el), "\xE2\x96\x8E ");
    }

    // Simple paragraph content — tok.inlines may contain '\n' embedded
    // when the quote spanned multiple source lines (the lexer joins them
    // with ' '); render each segment.  We re-split on any embedded
    // newlines to mirror the per-line bar prefix.
    if (tok.inlines.empty()) {
        // Empty blockquote (lone ">" or only link-ref definitions) —
        // no content to render, so no bar either.
        return text("");
    }
    auto content_el = render_inlines(tok.inlines, opts);
    // Italic on the content is unavailable in FTXUI; the dim bar alone
    // provides the blockquote visual cue (dim on text is
    // nearly invisible, so we keep content at normal brightness).
    return std::make_shared<BlockquoteBar>(
        std::move(content_el), "\xE2\x96\x8E ");
}

[[nodiscard]] Element render_html_block(const BlockToken& tok,
                                         const MarkdownOptions& opts) {
    // HTML blocks render as plain text in the terminal.  Strip tags for
    // a cleaner display — the terminal has no HTML rendering capability.
    //
    // The stripper is comment/PI/CDATA/declaration-aware: it skips entire
    // spans for <!-- ... -->, <? ... ?>, <![CDATA[ ... ]]>, and <! ... >
    // rather than treating the first '>' as the tag end (which would
    // mangle comment text containing '>').
    std::string text_content;
    text_content.reserve(tok.html_content.size());
    std::string_view sv = tok.html_content;
    std::size_t pos = 0;
    while (pos < sv.size()) {
        if (sv[pos] != '<') {
            text_content += sv[pos];
            ++pos;
            continue;
        }
        // Check for special constructs after '<'
        if (pos + 4 <= sv.size() && sv.substr(pos, 4) == "<!--") {
            // CommonMark: a comment is '<!--' + text + '-->', where text
            // does not start with '>' or '->'.  '<!-->' and '<!--->' are
            // NOT valid comments — treat '<' as literal text and let the
            // remaining characters fall through as regular text.
            if (pos + 5 <= sv.size() && sv[pos + 4] == '>') {
                text_content += '<';
                ++pos;
            } else if (pos + 6 <= sv.size() && sv[pos + 4] == '-' &&
                       sv[pos + 5] == '>') {
                text_content += '<';
                ++pos;
            } else {
                // Comment: find '-->'
                auto end = sv.find("-->", pos + 4);
                if (end == std::string_view::npos) {
                    // Unterminated comment — skip the rest
                    pos = sv.size();
                } else {
                    pos = end + 3;
                }
            }
        } else if (pos + 2 <= sv.size() && sv[pos + 1] == '?') {
            // Processing instruction: find '?>'
            auto end = sv.find("?>", pos + 2);
            if (end == std::string_view::npos) {
                pos = sv.size();
            } else {
                pos = end + 2;
            }
        } else if (pos + 9 <= sv.size() &&
                   sv.substr(pos, 9) == "<![CDATA[") {
            // CDATA section: find ']]>'
            auto end = sv.find("]]>", pos + 9);
            if (end == std::string_view::npos) {
                pos = sv.size();
            } else {
                pos = end + 3;
            }
        } else if (pos + 2 <= sv.size() && sv[pos + 1] == '!') {
            // Declaration (e.g. <!DOCTYPE>): find '>'
            auto end = sv.find('>', pos + 2);
            if (end == std::string_view::npos) {
                pos = sv.size();
            } else {
                pos = end + 1;
            }
        } else if (pos + 1 < sv.size() &&
                   (std::isalpha(static_cast<unsigned char>(
                                      sv[pos + 1])) ||
                    sv[pos + 1] == '/')) {
            // Regular tag: find '>'
            auto end = sv.find('>', pos + 1);
            if (end == std::string_view::npos) {
                // Unclosed tag — treat the '<' as literal text and
                // continue from the next character so the rest of the
                // content is preserved.
                text_content += '<';
                ++pos;
            } else {
                pos = end + 1;
            }
        } else {
            // Bare '<' (not followed by a letter or '/') — treat as
            // literal text, not a tag start.
            text_content += '<';
            ++pos;
        }
    }
    // Trim leading/trailing whitespace
    auto lb = text_content.find_first_not_of(" \t\n\r");
    if (lb == std::string::npos)
        return std::make_shared<EmptyNode>();
    auto rb = text_content.find_last_not_of(" \t\n\r");
    text_content = text_content.substr(lb, rb - lb + 1);

    // Split on newlines and render each line as a separate text() element
    // in a vbox.  FTXUI's text() silently drops '\n', so passing multi-line
    // content to a single text() would collapse all lines into one.
    Elements lines;
    std::size_t start = 0;
    while (start <= text_content.size()) {
        auto nl = text_content.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text(text_content.substr(start)));
            break;
        }
        lines.push_back(text(text_content.substr(start, nl - start)));
        start = nl + 1;
    }
    if (lines.size() == 1) {
        Element el = std::move(lines[0]);
        if (opts.dim_color) el = el | dim;
        return el;
    }
    if (opts.dim_color) {
        for (auto& l : lines) l = std::move(l) | dim;
    }
    return vbox(std::move(lines));
}

[[nodiscard]] bool is_empty_block(const Element& el) {
    return dynamic_cast<const EmptyNode*>(el.get()) != nullptr;
}

// Recursively append plain text from inline tokens to `out`.
// HTML tags are stripped (they are markup, not terminal content);
// formatting markers (**, _, `, etc.) are resolved to their text;
// links contribute their visible text, not the URL.
void append_inline_text(const std::vector<InlineToken>& tokens,
                        std::string& out) {
    for (const auto& tok : tokens) {
        switch (tok.kind) {
            case InlineTokenKind::Text:
            case InlineTokenKind::Code:
            case InlineTokenKind::Escape:
            case InlineTokenKind::Math:
                out += tok.text;
                break;
            case InlineTokenKind::Bold:
            case InlineTokenKind::Italic:
            case InlineTokenKind::Strikethrough:
            case InlineTokenKind::Link:
            case InlineTokenKind::Image:
                if (!tok.children.empty()) {
                    append_inline_text(tok.children, out);
                } else {
                    out += tok.text;
                }
                break;
            case InlineTokenKind::HtmlRaw:
                break;  // stripped — markup, not content
            case InlineTokenKind::FootnoteRef:
                out += "[^";
                out += tok.text;
                out += ']';
                break;
        }
    }
}

// Extract plain text from inline tokens (HTML stripped, formatting
// resolved to text, links show their text).
[[nodiscard]] std::string inline_to_text(
        const std::vector<InlineToken>& tokens) {
    std::string out;
    append_inline_text(tokens, out);
    return out;
}

// Process a raw table cell through the inline tokenizer and return
// the display text.  This resolves formatting markers, strips HTML
// tags, and extracts link text — matching what the HTML serializer
// does for table cells (markdown_html_impl.cpp render_table_html).
[[nodiscard]] std::string cell_to_text(std::string_view raw,
                                       const LinkRefMap* refs) {
    auto tokens = tokenize_inline(raw, refs, /*gfm_extensions=*/true);
    return inline_to_text(tokens);
}

[[nodiscard]] Element render_table(const BlockToken& tok,
                                   const MarkdownOptions& opts) {
    auto display_width = [](std::string_view s) -> std::size_t {
        return static_cast<std::size_t>(
            ftxui::string_width(std::string(s)));
    };

    // Unicode box-drawing glyphs (UTF-8 encoded).
    constexpr std::string_view kHBar      = "\xE2\x94\x80";  // ─ U+2500
    constexpr std::string_view kVBar      = "\xE2\x94\x82";  // │ U+2502
    constexpr std::string_view kCornerTL  = "\xE2\x94\x8C";  // ┌ U+250C
    constexpr std::string_view kCornerTR  = "\xE2\x94\x90";  // ┐ U+2510
    constexpr std::string_view kCornerBL  = "\xE2\x94\x94";  // └ U+2514
    constexpr std::string_view kCornerBR  = "\xE2\x94\x98";  // ┘ U+2518
    constexpr std::string_view kTeeDown   = "\xE2\x94\xAC";  // ┬ U+252C
    constexpr std::string_view kTeeUp     = "\xE2\x94\xB4";  // ┴ U+2534
    constexpr std::string_view kTeeRight  = "\xE2\x94\x9C";  // ├ U+251C
    constexpr std::string_view kTeeLeft   = "\xE2\x94\xA4";  // ┤ U+2524
    constexpr std::string_view kCross     = "\xE2\x94\xBC";  // ┼ U+253C

    std::size_t num_cols = tok.table_headers.size();
    if (num_cols == 0) return text("");

    // Process every cell through the inline tokenizer so that formatting
    // markers, HTML tags, and link syntax are resolved to display text
    // before computing column widths and building the ASCII grid.
    const LinkRefMap* refs =
        tok.table_link_refs.empty() ? nullptr : &tok.table_link_refs;
    std::vector<std::string> headers;
    headers.reserve(num_cols);
    for (const auto& h : tok.table_headers) {
        headers.push_back(cell_to_text(h, refs));
    }
    std::vector<std::vector<std::string>> body;
    body.reserve(tok.table_rows.size());
    for (const auto& row : tok.table_rows) {
        std::vector<std::string> processed;
        processed.reserve(row.size());
        for (const auto& cell : row) {
            processed.push_back(cell_to_text(cell, refs));
        }
        body.push_back(std::move(processed));
    }

    // Compute column widths (min 3).
    std::vector<std::size_t> col_widths(num_cols, 3);
    for (std::size_t c = 0; c < num_cols; ++c) {
        std::size_t w = display_width(headers[c]);
        for (const auto& row : body) {
            if (c < row.size()) {
                w = std::max(w, display_width(row[c]));
            }
        }
        col_widths[c] = std::max(w, std::size_t{3});
    }

    auto pad_right = [](std::string s, std::size_t width) {
        std::size_t w = ftxui::string_width(s);
        if (w < width) s.append(width - w, ' ');
        return s;
    };
    auto pad_center = [](std::string s, std::size_t width) {
        // Center headers: spaces split evenly on left / right.
        std::size_t w = ftxui::string_width(s);
        if (w >= width) return s;
        std::size_t diff = width - w;
        std::size_t left = diff / 2;
        std::size_t right = diff - left;
        std::string out;
        out.reserve(s.size() + diff);
        out.append(left, ' ');
        out += s;
        out.append(right, ' ');
        return out;
    };

    // Repeat a (possibly multi-byte) glyph N times into `out`.
    auto repeat_glyph = [](std::string& out, std::string_view g, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) out.append(g);
    };

    // Build a horizontal border: {corner_left} hbar*(width+2) {cross or end} ...
    auto make_border = [&](std::string_view l, std::string_view mid,
                           std::string_view cross, std::string_view r) {
        std::string line;
        line += l;
        for (std::size_t c = 0; c < num_cols; ++c) {
            repeat_glyph(line, kHBar, col_widths[c] + 2);  // +2 for inner spaces
            line += (c + 1 < num_cols) ? cross : r;
        }
        (void)mid;
        return line;
    };

    // Build a content row using │ cells. `cells` may be shorter than cols
    // (empty cells are padded).
    auto make_row = [&](const std::vector<std::string>& cells, bool is_header) {
        std::string line;
        line += kVBar;
        for (std::size_t c = 0; c < num_cols; ++c) {
            std::string cell = (c < cells.size()) ? cells[c] : std::string{};
            // Header is centered, data uses left-align.
            std::string padded = is_header
                ? pad_center(std::move(cell), col_widths[c])
                : pad_right(std::move(cell), col_widths[c]);
            line += " " + std::move(padded) + " ";
            line += kVBar;
        }
        Element el = text(line);
        if (is_header) el = el | bold;
        if (opts.dim_color) el = el | dim;
        return el;
    };

    Elements rows;
    // Top border.
    {
        Element el = text(make_border(kCornerTL, kHBar, kTeeDown, kCornerTR));
        if (opts.dim_color) el = el | dim;
        rows.push_back(std::move(el));
    }
    // Header row.
    rows.push_back(make_row(headers, /*is_header=*/true));
    // Header/data separator.
    {
        Element el = text(make_border(kTeeRight, kHBar, kCross, kTeeLeft));
        if (opts.dim_color) el = el | dim;
        rows.push_back(std::move(el));
    }
    // Data rows.
    for (const auto& row : body) {
        rows.push_back(make_row(row, /*is_header=*/false));
    }
    // Bottom border.
    {
        Element el = text(make_border(kCornerBL, kHBar, kTeeUp, kCornerBR));
        if (opts.dim_color) el = el | dim;
        rows.push_back(std::move(el));
    }

    return vbox(std::move(rows));
}

[[nodiscard]] Element render_hr(const MarkdownOptions& opts) {
    Element el = text("---");
    if (opts.dim_color) el = el | dim;
    return el;
}

} // namespace detail
} // namespace loom::ui
