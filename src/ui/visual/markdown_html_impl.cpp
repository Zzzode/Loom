// markdown_html_impl.cpp — impl unit for loom.ui.visual.markdown
//
// HTML serializer for CommonMark/GFM conformance testing.  Walks the same
// BlockToken/InlineToken AST produced by lex_blocks + tokenize_inline and
// emits standard HTML.  This is NOT the production renderer (the FTXUI
// terminal renderer is); it exists so the CommonMark/GFM conformance
// suites can validate parser correctness.
module;

#include <cstddef>

module loom.ui.visual.markdown;

import std;

namespace loom::ui {

// ── Footnote render state ──────────────────────────────────────────
// Footnote definitions are collected from the token tree (recursively,
// including blockquote/list sub_blocks).  As refs are encountered in
// document order during rendering, display numbers are assigned and
// ref counts tracked.  After all body blocks are rendered, the footnotes
// section is emitted with defs in first-reference order.

struct FootnoteDefInfo {
    std::string raw_label;        // def's label as written (for href/id)
    const BlockToken* def_block;  // owns sub_blocks (from lex_blocks output)
};

struct FootnoteRenderState {
    const std::map<std::string, FootnoteDefInfo>* defs = nullptr;
    std::map<std::string, int> number;         // normalized label -> display number
    std::map<std::string, int> ref_count;      // normalized label -> refs seen
    std::vector<std::string> reference_order;  // normalized labels, first-ref order
    int next_number = 1;
};

// Forward declaration — render_list_html calls it for non-paragraph sub-blocks.
namespace detail {
[[nodiscard]] std::string render_block_to_html(const BlockToken& tok,
                                               bool gfm_extensions,
                                               FootnoteRenderState* fn = nullptr);
}

namespace {

// ── HTML escaping ──────────────────────────────────────────────────

[[nodiscard]] std::string escape_html(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default:  out += c; break;
        }
    }
    return out;
}

// Escape a string for use in an href/id attribute, matching cmark-gfm's
// houdini_escape_href.  Safe chars (not escaped): alphanumeric +
// !#$%()*+,-./:;=?@_~  & -> &amp;  ' -> &#x27;  else -> %XX (uppercase hex).
[[nodiscard]] std::string escape_href(std::string_view s) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || c == '!' || c == '#' || c == '$' ||
            c == '%' || c == '(' || c == ')' || c == '*' || c == '+' ||
            c == ',' || c == '-' || c == '.' || c == '/' || c == ':' ||
            c == ';' || c == '=' || c == '?' || c == '@' || c == '_' ||
            c == '~') {
            out += static_cast<char>(c);
        } else if (c == '&') {
            out += "&amp;";
        } else if (c == '\'') {
            out += "&#x27;";
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 0x0F];
        }
    }
    return out;
}

// ── GFM tagfilter for HTML block content ───────────────────────────

// GFM disallowed raw HTML tags (GFM §6.11).  Matched case-insensitively
// on the tag name; these are escaped as text instead of passed through.
[[nodiscard]] bool is_disallowed_block_tag(std::string_view name) {
    static constexpr std::array<std::string_view, 9> kDisallowed = {
        "title", "textarea", "style", "xmp", "iframe",
        "noembed", "noframes", "script", "plaintext"};
    for (const auto& d : kDisallowed) {
        if (name.size() == d.size()) {
            bool match = true;
            for (std::size_t j = 0; j < name.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(name[j])) !=
                    d[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Check if an HTML block is a Type 1 block (<script>, <pre>, <style>,
// <textarea>).  Type 1 blocks are "raw text" elements — their content is
// not parsed as HTML, so the GFM tagfilter does not apply to them
// (CommonMark examples 140–145 expect unescaped <script>/<style> in
// Type 1 blocks, while GFM example 652 expects <xmp> escaped inside a
// Type 6 <blockquote> block).
[[nodiscard]] bool is_type1_html_block(std::string_view content) {
    // Skip up to 3 spaces indent
    std::size_t pos = 0;
    while (pos < content.size() && content[pos] == ' ' && pos < 3) ++pos;
    if (pos >= content.size() || content[pos] != '<') return false;
    ++pos;
    // Extract tag name (must start with a letter)
    const std::size_t name_start = pos;
    while (pos < content.size() &&
           std::isalpha(static_cast<unsigned char>(content[pos]))) {
        ++pos;
    }
    if (pos == name_start) return false;
    // Tag name must be followed by whitespace, '>', '/', or end-of-line
    // (rejects e.g. <scriptx>)
    if (pos < content.size() && content[pos] != ' ' &&
        content[pos] != '\t' && content[pos] != '\n' &&
        content[pos] != '\r' && content[pos] != '>' &&
        content[pos] != '/') {
        return false;
    }
    const std::string_view name = content.substr(name_start, pos - name_start);
    static constexpr std::array<std::string_view, 4> kType1 = {
        "script", "pre", "style", "textarea"};
    for (const auto& t : kType1) {
        if (name.size() == t.size()) {
            bool match = true;
            for (std::size_t j = 0; j < name.size(); ++j) {
                if (std::tolower(static_cast<unsigned char>(name[j])) !=
                    t[j]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

// Apply GFM tagfilter to HTML block content: escape disallowed tags by
// replacing the leading '<' with '&lt;'.  Type 1 blocks (<script>, <pre>,
// <style>, <textarea>) are exempt — their content is raw text, not HTML.
// Comments, CDATA, processing instructions, and declarations are passed
// through unchanged (GFM §6.11, examples 652 + GFM ext ex 22).
[[nodiscard]] std::string filter_disallowed_html_in_block(
    std::string_view content) {
    if (is_type1_html_block(content)) {
        return std::string(content);
    }
    std::string result;
    result.reserve(content.size());
    std::size_t i = 0;
    while (i < content.size()) {
        if (content[i] != '<') {
            result += content[i];
            ++i;
            continue;
        }
        // HTML comment: <!-- ... -->
        if (content.compare(i, 4, "<!--") == 0) {
            const auto end = content.find("-->", i + 4);
            if (end == std::string_view::npos) {
                result += content.substr(i);
                break;
            }
            result += content.substr(i, end + 3 - i);
            i = end + 3;
            continue;
        }
        // CDATA section: <![CDATA[ ... ]]>
        if (content.compare(i, 9, "<![CDATA[") == 0) {
            const auto end = content.find("]]>", i + 9);
            if (end == std::string_view::npos) {
                result += content.substr(i);
                break;
            }
            result += content.substr(i, end + 3 - i);
            i = end + 3;
            continue;
        }
        // Processing instruction: <? ... ?>
        if (i + 1 < content.size() && content[i + 1] == '?') {
            const auto end = content.find("?>", i + 2);
            if (end == std::string_view::npos) {
                result += content.substr(i);
                break;
            }
            result += content.substr(i, end + 2 - i);
            i = end + 2;
            continue;
        }
        // Declaration: <! [A-Z]+ ... >
        if (i + 1 < content.size() && content[i + 1] == '!') {
            const auto end = content.find('>', i + 2);
            if (end == std::string_view::npos) {
                result += content.substr(i);
                break;
            }
            result += content.substr(i, end + 1 - i);
            i = end + 1;
            continue;
        }
        // Regular tag (opening or closing): extract tag name
        std::size_t tag_pos = i + 1;
        if (tag_pos < content.size() && content[tag_pos] == '/') {
            ++tag_pos;
        }
        const std::size_t name_start = tag_pos;
        while (tag_pos < content.size() &&
               (std::isalnum(static_cast<unsigned char>(content[tag_pos])) ||
                content[tag_pos] == '-')) {
            ++tag_pos;
        }
        if (tag_pos > name_start) {
            const std::string_view name =
                content.substr(name_start, tag_pos - name_start);
            if (is_disallowed_block_tag(name)) {
                result += "&lt;";
                ++i;
                continue;
            }
        }
        // Not a disallowed tag — pass through unchanged
        result += content[i];
        ++i;
    }
    return result;
}

// Escape HTML + handle hard line breaks (CommonMark §6.1).
// Two+ trailing spaces or a trailing backslash before a newline → <br />.
// Trailing-space stripping is done by the caller (render_inlines_to_html)
// on the full concatenated output, not per-token.
[[nodiscard]] std::string render_text_html(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\n') {
            // Count trailing spaces in the output so far
            std::size_t trailing = 0;
            while (trailing < out.size() &&
                   out[out.size() - 1 - trailing] == ' ') {
                ++trailing;
            }
            if (trailing >= 2) {
                out.resize(out.size() - trailing);
                out += "<br />\n";
            } else if (trailing == 0 && !out.empty() && out.back() == '\\') {
                out.pop_back();
                out += "<br />\n";
            } else {
                out += '\n';
            }
        } else {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default:  out += c; break;
            }
        }
    }
    return out;
}

// ── Footnote definition collection ─────────────────────────────────

// Recursively walk the token tree and collect FootnoteDef blocks.
// First-def-wins by normalized label (case-insensitive lookup).  The
// BlockToken pointers remain valid because they point into `blocks` (and
// its sub_blocks / list_items), which outlives the render.
void collect_footnote_defs(
    const std::vector<BlockToken>& blocks,
    std::map<std::string, FootnoteDefInfo>& defs) {
    for (const auto& block : blocks) {
        if (block.kind == BlockTokenKind::FootnoteDef) {
            auto key = detail::normalize_label(block.footnote_label);
            if (!defs.contains(key)) {
                defs.emplace(key,
                             FootnoteDefInfo{block.footnote_label, &block});
            }
        }
        if (!block.sub_blocks.empty()) {
            collect_footnote_defs(block.sub_blocks, defs);
        }
        for (const auto& item : block.list_items) {
            collect_footnote_defs(item, defs);
        }
    }
}

// ── Inline token → HTML ────────────────────────────────────────────

// GFM (based on CommonMark 0.29) does not allow same-type emphasis
// nesting: <strong><strong>foo</strong></strong> renders as
// <strong>foo</strong>.  cmark-gfm achieves this in the HTML renderer by
// suppressing <strong> tags when the node's direct parent is also
// <strong> (see src/html.c CMARK_NODE_STRONG case).  CommonMark 0.30+
// allows the nesting, so this flattening is GFM-only.  parent_is_strong
// tracks whether the tokens' direct parent in the AST is a Bold node.
[[nodiscard]] std::string render_inlines_to_html(
    const std::vector<InlineToken>& tokens,
    bool gfm_extensions = false,
    bool parent_is_strong = false,
    FootnoteRenderState* fn = nullptr) {
    std::string out;
    for (const auto& tok : tokens) {
        switch (tok.kind) {
            case InlineTokenKind::Text:
                out += render_text_html(tok.text);
                break;
            case InlineTokenKind::Bold:
                if (gfm_extensions && parent_is_strong) {
                    // Suppress <strong> tags — same-type nesting flattened.
                    if (!tok.children.empty()) {
                        out += render_inlines_to_html(
                            tok.children, gfm_extensions,
                            /*parent_is_strong=*/true, fn);
                    } else {
                        out += render_text_html(tok.text);
                    }
                } else {
                    out += "<strong>";
                    if (!tok.children.empty()) {
                        out += render_inlines_to_html(
                            tok.children, gfm_extensions,
                            /*parent_is_strong=*/true, fn);
                    } else {
                        out += render_text_html(tok.text);
                    }
                    out += "</strong>";
                }
                break;
            case InlineTokenKind::Italic:
                out += "<em>";
                if (!tok.children.empty()) {
                    out += render_inlines_to_html(
                        tok.children, gfm_extensions,
                        /*parent_is_strong=*/false, fn);
                } else {
                    out += render_text_html(tok.text);
                }
                out += "</em>";
                break;
            case InlineTokenKind::Code:
                out += "<code>" + escape_html(tok.text) + "</code>";
                break;
            case InlineTokenKind::Link:
                out += "<a href=\"" + escape_html(tok.url) + "\"";
                if (!tok.title.empty()) {
                    out += " title=\"" + escape_html(tok.title) + "\"";
                }
                out += ">";
                if (!tok.children.empty()) {
                    out += render_inlines_to_html(
                        tok.children, gfm_extensions,
                        /*parent_is_strong=*/false, fn);
                } else {
                    out += escape_html(tok.text);
                }
                out += "</a>";
                break;
            case InlineTokenKind::Image:
                out += "<img src=\"" + escape_html(tok.url) + "\" alt=\"" +
                       escape_html(tok.text) + "\"";
                if (!tok.title.empty()) {
                    out += " title=\"" + escape_html(tok.title) + "\"";
                }
                out += " />";
                break;
            case InlineTokenKind::Strikethrough:
                // GFM strikethrough content can contain other inline
                // constructs (autolinks, emphasis, code spans).  The
                // tokenizer stores the raw content; re-tokenize here so
                // nested constructs render correctly (GFM ex 26:
                // ~~www.google.com~~ → <del><a href="...">...</a></del>).
                out += "<del>";
                out += render_inlines_to_html(
                    detail::tokenize_inline(tok.text, nullptr, gfm_extensions),
                    gfm_extensions,
                    /*parent_is_strong=*/false, fn);
                out += "</del>";
                break;
            case InlineTokenKind::Escape:
                // The tokenizer already unescaped the character into `text`.
                out += escape_html(tok.text);
                break;
            case InlineTokenKind::Math:
                // CommonMark has no math; emit the raw content.
                out += escape_html(tok.text);
                break;
            case InlineTokenKind::HtmlRaw:
                // Raw HTML passes through unchanged (CommonMark §6.6).
                out += tok.text;
                break;
            case InlineTokenKind::FootnoteRef: {
                // GFM footnote reference: [^label].  If no definition
                // exists (or footnotes disabled), render literal text.
                if (!fn || !fn->defs) {
                    out += "[^" + escape_html(tok.text) + "]";
                    break;
                }
                auto key = detail::normalize_label(tok.text);
                auto it = fn->defs->find(key);
                if (it == fn->defs->end()) {
                    out += "[^" + escape_html(tok.text) + "]";
                    break;
                }
                // Assign display number on first reference.
                int num = 0;
                if (auto nit = fn->number.find(key); nit != fn->number.end()) {
                    num = nit->second;
                } else {
                    num = fn->next_number++;
                    fn->number[key] = num;
                    fn->reference_order.push_back(key);
                }
                int count = ++fn->ref_count[key];
                const auto& raw = it->second.raw_label;
                const auto esc = escape_href(raw);
                out += "<sup class=\"footnote-ref\"><a href=\"#fn-" + esc +
                       "\" id=\"fnref-" + esc;
                if (count > 1) out += "-" + std::to_string(count);
                out += "\" data-footnote-ref>" + std::to_string(num) +
                       "</a></sup>";
                break;
            }
        }
    }
    // Strip trailing spaces on the last line (CommonMark: trailing spaces
    // at end of paragraph are stripped, not a hard break — no following line).
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

// ── List rendering (sub-blocks per item) ────────────────────────────

void render_list_html(
    const BlockToken& tok,
    std::string& out,
    bool ordered,
    bool gfm_extensions,
    FootnoteRenderState* fn = nullptr) {
    const auto& items = tok.list_items;
    const auto& tasks = tok.task_state;
    if (items.empty()) return;

    // Open tag with optional start attribute
    if (ordered) {
        out += "<ol";
        if (tok.list_start != 1) {
            out += " start=\"" + std::to_string(tok.list_start) + "\"";
        }
        out += ">\n";
    } else {
        out += "<ul>\n";
    }

    for (std::size_t i = 0; i < items.size(); ++i) {
        out += "<li>";
        // GFM task list item: render checkbox before content.
        // Format matches the GFM spec: type first, then disabled, then
        // checked, self-closing (XHTML style).
        if (i < tasks.size() && tasks[i] >= 0) {
            out += "<input type=\"checkbox\"";
            if (tasks[i] == 1) out += " checked=\"\"";
            out += " disabled=\"\" /> ";
        }

        const auto& sub_blocks = items[i];
        if (sub_blocks.size() == 1 &&
            sub_blocks[0].kind == BlockTokenKind::Paragraph &&
            !tok.list_loose) {
            // Tight list, single paragraph: render inline content directly,
            // no trailing newline (</li> follows immediately).
            out += render_inlines_to_html(sub_blocks[0].inlines,
                                          gfm_extensions,
                                          /*parent_is_strong=*/false, fn);
        } else {
            // Loose list or multi-block item: render each sub-block
            for (const auto& sub : sub_blocks) {
                if (sub.kind == BlockTokenKind::Paragraph && !tok.list_loose) {
                    // Tight list: paragraph without <p> tags
                    out += render_inlines_to_html(sub.inlines, gfm_extensions,
                                                  /*parent_is_strong=*/false,
                                                  fn);
                    out += '\n';
                } else {
                    // Block element: ensure a newline separates it from
                    // <li> or the previous block (blocks already end with
                    // a trailing newline, so avoid doubling).
                    if (!out.empty() && out.back() != '\n') out += '\n';
                    out += detail::render_block_to_html(sub, gfm_extensions,
                                                        fn);
                }
            }
        }
        out += "</li>\n";
    }

    out += ordered ? "</ol>\n" : "</ul>\n";
}

// ── Table rendering ─────────────────────────────────────────────────

void render_table_html(const BlockToken& tok, std::string& out,
                       bool gfm_extensions,
                       FootnoteRenderState* fn = nullptr) {
    const auto& align = tok.table_align;
    auto align_attr = [&](std::size_t col) -> std::string {
        if (col >= align.size() || align[col] == 0) return "";
        switch (align[col]) {
            case 1:  return " align=\"left\"";
            case 2:  return " align=\"center\"";
            default: return " align=\"right\"";
        }
    };

    out += "<table>\n<thead>\n<tr>\n";
    for (std::size_t c = 0; c < tok.table_headers.size(); ++c) {
        out += "<th";
        out += align_attr(c);
        out += ">";
        out += render_inlines_to_html(
            detail::tokenize_inline(tok.table_headers[c],
                                    &tok.table_link_refs, gfm_extensions),
            gfm_extensions,
            /*parent_is_strong=*/false, fn);
        out += "</th>\n";
    }
    out += "</tr>\n</thead>\n";
    if (!tok.table_rows.empty()) {
        out += "<tbody>\n";
        for (const auto& row : tok.table_rows) {
            out += "<tr>\n";
            for (std::size_t c = 0; c < row.size(); ++c) {
                out += "<td";
                out += align_attr(c);
                out += ">";
                out += render_inlines_to_html(
                    detail::tokenize_inline(row[c], &tok.table_link_refs,
                                            gfm_extensions),
                    gfm_extensions,
                    /*parent_is_strong=*/false, fn);
                out += "</td>\n";
            }
            out += "</tr>\n";
        }
        out += "</tbody>\n";
    }
    out += "</table>\n";
}

// ── Footnote backref + section rendering ───────────────────────────

// Render the backref link(s) for a footnote definition.  `esc_label` is
// the href-escaped raw label.  `display_num` is the def's display number
// (assigned on first ref).  `ref_total` is the total number of refs to
// this def.  Multiple backrefs are space-separated.
[[nodiscard]] std::string render_footnote_backref(
    const std::string& esc_label, int display_num, int ref_total) {
    std::string out;
    for (int i = 1; i <= ref_total; ++i) {
        if (i > 1) out += ' ';
        out += "<a href=\"#fnref-" + esc_label;
        if (i > 1) out += "-" + std::to_string(i);
        out += "\" class=\"footnote-backref\" data-footnote-backref "
               "data-footnote-backref-idx=\"" +
               std::to_string(display_num);
        if (i > 1) out += "-" + std::to_string(i);
        out += "\" aria-label=\"Back to reference " +
               std::to_string(display_num);
        if (i > 1) out += "-" + std::to_string(i);
        out += "\">↩";
        if (i > 1) {
            out += "<sup class=\"footnote-ref\">" + std::to_string(i) +
                   "</sup>";
        }
        out += "</a>";
    }
    return out;
}

// Render the footnotes section: <section class="footnotes"
// data-footnotes> containing an <ol> of definitions in first-reference
// order.  The backref is placed inside the last paragraph (before </p>)
// or directly in <li> if the last block is not a paragraph.
[[nodiscard]] std::string render_footnotes_section(
    FootnoteRenderState& fn, bool gfm_extensions) {
    // Index-based loop (not a snapshot): rendering def content may
    // append new entries to reference_order (footnote refs inside
    // footnote def content).  Terminates because each def is appended
    // at most once (the number map guards re-assignment).
    std::string out =
        "<section class=\"footnotes\" data-footnotes>\n<ol>\n";
    for (std::size_t idx = 0; idx < fn.reference_order.size(); ++idx) {
        // Copy: the vector may grow during rendering, invalidating refs.
        const std::string key = fn.reference_order[idx];
        const auto& info = fn.defs->at(key);
        const auto esc = escape_href(info.raw_label);
        const int num = fn.number.at(key);
        out += "<li id=\"fn-" + esc + "\">\n";
        const auto& subs = info.def_block->sub_blocks;
        if (!subs.empty() &&
            subs.back().kind == BlockTokenKind::Paragraph) {
            // Backref inside the last paragraph (before </p>).
            for (std::size_t i = 0; i + 1 < subs.size(); ++i) {
                out += detail::render_block_to_html(subs[i], gfm_extensions,
                                                    &fn);
            }
            out += "<p>";
            out += render_inlines_to_html(subs.back().inlines,
                                          gfm_extensions,
                                          /*parent_is_strong=*/false, &fn);
            // Re-read count after rendering: self-refs in the def
            // content increment ref_count during rendering.
            const int count = fn.ref_count.at(key);
            out += " " + render_footnote_backref(esc, num, count) + "</p>\n";
        } else {
            // Backref directly in <li>.
            for (const auto& sub : subs) {
                out += detail::render_block_to_html(sub, gfm_extensions, &fn);
            }
            const int count = fn.ref_count.at(key);
            out += render_footnote_backref(esc, num, count) + "\n";
        }
        out += "</li>\n";
    }
    out += "</ol>\n</section>\n";
    return out;
}

}  // anonymous namespace

namespace detail {

// ── Block → HTML ────────────────────────────────────────────────────

[[nodiscard]] std::string render_block_to_html(const BlockToken& tok,
                                               bool gfm_extensions,
                                               FootnoteRenderState* fn) {
    std::string out;

    switch (tok.kind) {
        case BlockTokenKind::Paragraph:
            out += "<p>";
            out += render_inlines_to_html(tok.inlines, gfm_extensions,
                                          /*parent_is_strong=*/false, fn);
            out += "</p>\n";
            break;

        case BlockTokenKind::Heading:
            out += "<h" + std::to_string(tok.heading_level) + ">";
            out += render_inlines_to_html(tok.inlines, gfm_extensions,
                                          /*parent_is_strong=*/false, fn);
            out += "</h" + std::to_string(tok.heading_level) + ">\n";
            break;

        case BlockTokenKind::CodeBlock:
            out += "<pre><code";
            if (!tok.code_lang.empty()) {
                out += " class=\"language-" + escape_html(tok.code_lang) + "\"";
            }
            out += ">";
            out += escape_html(tok.code_content);
            if (!tok.code_content.empty()) out += '\n';
            out += "</code></pre>\n";
            break;

        case BlockTokenKind::UnorderedList:
            render_list_html(tok, out, /*ordered=*/false, gfm_extensions, fn);
            break;

        case BlockTokenKind::OrderedList:
            render_list_html(tok, out, /*ordered=*/true, gfm_extensions, fn);
            break;

        case BlockTokenKind::Blockquote:
            out += "<blockquote>\n";
            if (!tok.sub_blocks.empty()) {
                for (const auto& sub : tok.sub_blocks) {
                    out += render_block_to_html(sub, gfm_extensions, fn);
                }
            } else if (!tok.inlines.empty()) {
                out += "<p>";
                out += render_inlines_to_html(tok.inlines, gfm_extensions,
                                              /*parent_is_strong=*/false, fn);
                out += "</p>\n";
            }
            out += "</blockquote>\n";
            break;

        case BlockTokenKind::Table:
            render_table_html(tok, out, gfm_extensions, fn);
            break;

        case BlockTokenKind::HorizontalRule:
            out += "<hr />\n";
            break;

        case BlockTokenKind::MathBlock:
            // CommonMark has no math blocks; emit as paragraph.
            out += "<p>" + escape_html(tok.math_content) + "</p>\n";
            break;

        case BlockTokenKind::HtmlBlock:
            // Raw HTML passes through unchanged (CommonMark §4.6).
            // GFM's tagfilter extension is applied to inline HTML during
            // tokenization (see parse_inline_html) and to HTML block
            // content here — except for Type 1 blocks (<script>, <pre>,
            // <style>, <textarea>), whose content is raw text, not HTML
            // (CommonMark examples 140–145 vs GFM example 652).
            if (gfm_extensions) {
                out += filter_disallowed_html_in_block(tok.html_content);
            } else {
                out += tok.html_content;
            }
            out += '\n';
            break;

        case BlockTokenKind::FootnoteDef:
            // Footnote definitions are hoisted to the footnotes section
            // (rendered by render_footnotes_section); no output here.
            break;
    }

    return out;
}

}  // namespace detail

// ── Public API ──────────────────────────────────────────────────────

[[nodiscard]] std::string render_markdown_to_html(std::string_view source,
                                                   bool gfm_extensions) {
    // Fast-path: no markdown syntax → single paragraph.
    if (!detail::has_markdown_syntax(source)) {
        std::string text(source);
        // Trim trailing whitespace/newlines for the paragraph.
        while (!text.empty() && (text.back() == '\n' || text.back() == ' '))
            text.pop_back();
        if (text.empty()) return "";
        return "<p>" + escape_html(text) + "</p>\n";
    }

    const auto blocks = detail::lex_blocks(source, gfm_extensions);

    // GFM footnotes: collect definitions, then render with shared state
    // so refs are numbered in first-reference order and the footnotes
    // section is appended at the end.
    FootnoteRenderState fn_state;
    std::map<std::string, FootnoteDefInfo> fn_defs;
    if (gfm_extensions) {
        collect_footnote_defs(blocks, fn_defs);
        fn_state.defs = &fn_defs;
    }

    std::string out;
    for (const auto& block : blocks) {
        out += detail::render_block_to_html(block, gfm_extensions, &fn_state);
    }
    if (gfm_extensions && !fn_state.reference_order.empty()) {
        out += render_footnotes_section(fn_state, gfm_extensions);
    }
    return out;
}

}  // namespace loom::ui
