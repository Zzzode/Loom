// markdown_api_impl.cpp - impl unit for loom.ui.visual.markdown (RFC 0001
// Phase C batch 6). Holds the public entry points render_markdown /
// render_markdown_dim, the cache-management accessors, and the
// StreamingMarkdown out-of-line members (update, reset, private static
// find_block_boundary). Default arguments stay on the cppm declarations.
module;

#include <ftxui/dom/elements.hpp>

module loom.ui.visual.markdown;

import std;

namespace loom::ui {

namespace {

// Normalize a footnote label per CommonMark §4.3: case-fold, collapse
// whitespace runs to a single space, trim.  Used to match [^ref] tokens
// against [^label]: definitions.
[[nodiscard]] std::string normalize_label(std::string_view label) {
    std::string result;
    bool last_was_space = false;
    for (char c : label) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!last_was_space && !result.empty()) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));
            last_was_space = false;
        }
    }
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

// Recursively collect all footnote reference labels from inline tokens
// (including nested emphasis children).  Labels are appended to `order`
// on first reference only (dedup via `seen`), preserving the GFM-mandated
// reference order for the footnotes section.
void collect_footnote_refs(const std::vector<InlineToken>& tokens,
                           std::vector<std::string>& order,
                           std::set<std::string>& seen) {
    for (const auto& tok : tokens) {
        if (tok.kind == InlineTokenKind::FootnoteRef) {
            std::string label = normalize_label(tok.text);
            if (seen.insert(label).second) {
                order.push_back(std::move(label));
            }
        }
        if (!tok.children.empty()) {
            collect_footnote_refs(tok.children, order, seen);
        }
    }
}

// Recursively collect all footnote reference labels from a block tree
// (including blockquote sub_blocks and list items).
void collect_footnote_refs(const std::vector<BlockToken>& blocks,
                           std::vector<std::string>& order,
                           std::set<std::string>& seen) {
    for (const auto& block : blocks) {
        collect_footnote_refs(block.inlines, order, seen);
        if (!block.sub_blocks.empty()) {
            collect_footnote_refs(block.sub_blocks, order, seen);
        }
        for (const auto& item : block.list_items) {
            collect_footnote_refs(item, order, seen);
        }
    }
}

// Render one sub-block of a footnote definition.
//
// detail::render_block_element() (the shared sub-block dispatcher in
// markdown_render_impl.cpp) is not reachable from this impl unit — it is
// not declared in the module interface — so dispatch here to the
// interface-declared block renderers.  Keep this switch in sync with
// render_block_element() if block kinds are added.
[[nodiscard]] Element render_footnote_sub_block(const BlockToken& sub,
                                                const MarkdownOptions& opts) {
    switch (sub.kind) {
        case BlockTokenKind::Paragraph:
            return detail::render_inlines(sub.inlines, opts);
        case BlockTokenKind::Heading:
            return detail::render_heading(sub, opts);
        case BlockTokenKind::CodeBlock:
            return detail::render_code_block(sub, opts);
        case BlockTokenKind::UnorderedList:
            return detail::render_ulist(sub, opts);
        case BlockTokenKind::OrderedList:
            return detail::render_olist(sub, opts);
        case BlockTokenKind::Blockquote:
            return detail::render_blockquote(sub, opts);
        case BlockTokenKind::Table:
            return detail::render_table(sub, opts);
        case BlockTokenKind::HorizontalRule:
            return detail::render_hr(opts);
        case BlockTokenKind::MathBlock:
            // Display math: show the raw LaTeX in a distinct color,
            // matching render_block_element().
            return text(sub.math_content) | color(Color::CyanLight);
        case BlockTokenKind::HtmlBlock:
            return detail::render_html_block(sub, opts);
        case BlockTokenKind::FootnoteDef:
            // Nested footnote defs are hoisted, not rendered inline.
            return text("");
    }
    return text("");
}

} // namespace

[[nodiscard]] Element render_markdown(std::string_view source,
                                      const MarkdownOptions& opts) {
    std::vector<BlockToken> tokens;

    // Fast path: plain text with no markdown syntax
    if (!detail::has_markdown_syntax(source)) {
        BlockToken tok;
        tok.kind = BlockTokenKind::Paragraph;
        tok.inlines = {{InlineTokenKind::Text, std::string(source), "", "", {}}};
        tokens.push_back(std::move(tok));
    } else {
        // Check cache
        auto& cache = detail::global_token_cache();
        const auto* cached = cache.find(source);
        if (cached) {
            tokens = *cached;
        } else {
            tokens = detail::lex_blocks(source);
            cache.put(std::string(source), tokens);
        }
    }

    // Render tokens.  Consecutive blocks are separated by a blank line.
    // Headings already carry a trailing blank line (render_heading returns
    // vbox({el, text("")})), so the separator is skipped after a heading
    // to avoid double-spacing.  Footnote definitions are hoisted to a
    // footnotes section rendered after the body (GFM semantics) and do
    // not participate in the block flow.
    Elements elements;
    std::vector<const BlockToken*> footnotes;
    bool prev_was_heading = false;

    for (const auto& tok : tokens) {
        if (tok.kind == BlockTokenKind::FootnoteDef) {
            footnotes.push_back(&tok);
            continue;
        }

        // Render the block element first so we can check for empty blocks
        // (e.g. HTML comments that strip to nothing) and skip them entirely,
        // avoiding extra blank lines from separators.
        Element el;
        switch (tok.kind) {
            case BlockTokenKind::Paragraph:
                el = detail::render_inlines(tok.inlines, opts);
                break;
            case BlockTokenKind::Heading:
                el = detail::render_heading(tok, opts);
                break;
            case BlockTokenKind::CodeBlock:
                el = detail::render_code_block(tok, opts);
                break;
            case BlockTokenKind::UnorderedList:
                el = detail::render_ulist(tok, opts);
                break;
            case BlockTokenKind::OrderedList:
                el = detail::render_olist(tok, opts);
                break;
            case BlockTokenKind::Blockquote:
                el = detail::render_blockquote(tok, opts);
                break;
            case BlockTokenKind::Table:
                el = detail::render_table(tok, opts);
                break;
            case BlockTokenKind::HorizontalRule:
                el = detail::render_hr(opts);
                break;
            case BlockTokenKind::MathBlock: {
                // Display math ($...$).  renders via KaTeX → HTML; in a
                // terminal we show the raw LaTeX centered and in a distinct
                // cyan color so it's visually recognized as a formula block.
                Element math_el = text(tok.math_content) |
                                  color(Color::CyanLight);
                el = hbox({filler(), std::move(math_el), filler()});
                break;
            }
            case BlockTokenKind::HtmlBlock:
                // HTML blocks render as plain text in the terminal (no HTML
                // rendering capability).  Strip tags for a cleaner display.
                el = detail::render_html_block(tok, opts);
                break;
            case BlockTokenKind::FootnoteDef:
                // Collected above; unreachable here.
                break;
        }

        // Skip empty blocks (e.g. HTML comments that strip to nothing)
        if (detail::is_empty_block(el)) {
            continue;
        }

        if (!elements.empty() && !prev_was_heading) {
            elements.push_back(text(""));
        }
        elements.push_back(std::move(el));
        prev_was_heading = (tok.kind == BlockTokenKind::Heading);
    }

    // Footnotes section: a rule followed by the collected definitions,
    // mirroring the HTML serializer's hoisted footnotes section.  Only
    // definitions that have a matching [^label] reference are rendered
    // (GFM semantics — orphan definitions are not displayed), and they
    // are ordered by first reference (not source order), matching the
    // HTML serializer's <ol> in the footnotes section.
    if (!footnotes.empty()) {
        std::vector<std::string> ref_order;
        std::set<std::string> seen;
        collect_footnote_refs(tokens, ref_order, seen);

        // Build a lookup from normalized label to definition.
        std::map<std::string, const BlockToken*> defs_by_label;
        for (const auto* fn : footnotes) {
            defs_by_label.emplace(normalize_label(fn->footnote_label), fn);
        }

        // Filter to referenced definitions, in reference order.
        std::vector<const BlockToken*> visible;
        for (const auto& label : ref_order) {
            auto it = defs_by_label.find(label);
            if (it != defs_by_label.end()) {
                visible.push_back(it->second);
            }
        }

        if (!visible.empty()) {
            if (!elements.empty() && !prev_was_heading) {
                elements.push_back(text(""));
            }
            elements.push_back(detail::render_hr(opts));

            for (std::size_t i = 0; i < visible.size(); ++i) {
                const auto* fn = visible[i];
                if (i > 0) {
                    elements.push_back(text(""));
                }
                Elements entry_parts;
                if (fn->sub_blocks.empty()) {
                    // Definition with no content: the label alone.
                    entry_parts.push_back(
                        text(std::string("[^") + fn->footnote_label + "]: "));
                } else {
                    // Label prefix on the same line as the first sub-block,
                    // matching the list-item prefix pattern (hbox of prefix +
                    // rendered content); remaining sub-blocks stack below.
                    entry_parts.push_back(hbox({
                        text(std::string("[^") + fn->footnote_label + "]: "),
                        render_footnote_sub_block(fn->sub_blocks[0], opts)}));
                    for (std::size_t j = 1; j < fn->sub_blocks.size(); ++j) {
                        entry_parts.push_back(
                            render_footnote_sub_block(fn->sub_blocks[j], opts));
                    }
                }
                if (entry_parts.size() == 1) {
                    elements.push_back(std::move(entry_parts[0]));
                } else {
                    elements.push_back(vbox(std::move(entry_parts)));
                }
            }
        }
    }

    if (elements.empty()) {
        return text("");
    }
    return vbox(std::move(elements));
}

/// Convenience: render with dim_color = true
[[nodiscard]] Element render_markdown_dim(std::string_view source) {
    MarkdownOptions opts;
    opts.dim_color = true;
    return render_markdown(source, opts);
}

Element StreamingMarkdown::update(std::string_view content) {
        // Reset if content was replaced (not a prefix extension)
        if (!content.starts_with(stable_prefix_)) {
            stable_prefix_.clear();
        }

        // Find a safe block boundary in the new content
        std::size_t boundary = find_block_boundary(content);

        // Advance stable prefix
        if (boundary > stable_prefix_.size()) {
            stable_prefix_ = std::string(content.substr(0, boundary));
        }

        std::string_view unstable = content.substr(stable_prefix_.size());

        Elements parts;
        if (!stable_prefix_.empty()) {
            parts.push_back(render_markdown(stable_prefix_));
        }
        if (!unstable.empty()) {
            // Separate the stable prefix from the in-progress suffix with
            // a blank line, matching the inter-block separator in
            // render_markdown.
            if (!parts.empty()) {
                parts.push_back(text(""));
            }
            parts.push_back(render_markdown(unstable));
        }
        return vbox(std::move(parts));
    }

void StreamingMarkdown::reset() { stable_prefix_.clear(); }

std::size_t StreamingMarkdown::find_block_boundary(
    std::string_view content) {

        std::size_t last_good = 0;
        bool in_code_fence = false;

        std::size_t pos = 0;
        while (pos < content.size()) {
            auto nl = content.find('\n', pos);
            if (nl == std::string_view::npos) break;

            std::string_view line;
            if (nl > pos) {
                line = content.substr(pos, nl - pos);
            }

            if (line.starts_with("```")) {
                if (in_code_fence) {
                    // End of code fence — boundary after this line
                    last_good = nl + 1;
                    in_code_fence = false;
                } else {
                    in_code_fence = true;
                }
            }

            // Double newline = paragraph boundary
            if (nl + 1 < content.size() && content[nl + 1] == '\n') {
                if (!in_code_fence) {
                    last_good = nl + 1;
                }
            }

            // Horizontal rule = block boundary
            if (line == "---" || line == "***" || line == "___") {
                if (!in_code_fence) {
                    last_good = nl + 1;
                }
            }

            pos = nl + 1;
        }

        return last_good;
    }

/// Clear the global markdown token cache.
/// Call this when memory pressure is high or on settings change.
void clear_markdown_cache() {
    detail::global_token_cache().clear();
}

/// Get current cache size (for diagnostics)
[[nodiscard]] std::size_t markdown_cache_size() {
    return detail::global_token_cache().size();
}

/// Get maximum cache size
[[nodiscard]] std::size_t markdown_cache_max_size() {
    return detail::global_token_cache().max_size();
}

} // namespace loom::ui
