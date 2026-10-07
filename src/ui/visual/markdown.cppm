/// @file markdown.cppm
/// @brief Production-grade Markdown to FTXUI element renderer.
///
/// Features:
/// - Full block-level parsing: headings, paragraphs, code fences, lists,
///   blockquotes, tables (GFM), horizontal rules
/// - Inline formatting: bold, italic, inline code, links,
///   escaped characters (strikethrough intentionally disabled, mirroring)
/// - Syntax-highlighted code blocks via loom.ui.visual.code_highlight
/// - LRU token cache for fast re-renders (virtual scrolling)
/// - Fast-path: skip lexing for plain text with no markdown markers
/// - Theme-aware coloring with dim option
/// - Streaming Markdown: stable prefix + unstable suffix for live output
/// - Interactive MarkdownComponent with scroll support
module;

#include <cstdint>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.visual.markdown;

import std;

export namespace loom::ui {

using namespace ftxui;

// ============================================================
// Options
// ============================================================

/// Rendering options for the Markdown renderer
struct MarkdownOptions {
    /// When true, render all text content as dim
    bool dim_color = false;
    /// Enable syntax highlighting in code blocks
    bool syntax_highlighting = true;
    /// Theme name for code highlighting ("dark", "light", "monokai", etc.)
    std::string theme_name = "dark";
    /// Maximum number of cached tokenized documents (LRU)
    std::size_t cache_max_size = 256;
};

// ============================================================
// Token Types (AST)
// ============================================================

/// Inline token types
enum class InlineTokenKind : std::uint8_t {
    Text,
    Bold,
    Italic,
    Code,
    Link,
    Escape,
    Math,       ///< Inline math: $...$ (LaTeX math expression)
    Image,      ///< Image: ![alt](url "title")
    Strikethrough,  ///< GFM strikethrough: ~~text~~ or ~text~
    HtmlRaw,    ///< Raw HTML tag (CommonMark §6.6) — passed through to HTML
    FootnoteRef,  ///< GFM footnote ref: [^label] (text field = label without '^')
};

/// An inline formatting token
struct InlineToken {
    InlineTokenKind kind;
    std::string text;
    std::string url;    // for links and images
    std::string title;  // for links and images (link title / image title)
    // Nested inline tokens for emphasis (Bold/Italic) that contain other
    // emphasis (e.g. _**text**_ → Italic with a Bold child).  Empty when
    // the token has no nested emphasis (text holds the raw content).
    std::vector<InlineToken> children;
};

// ============================================================
// Link reference definitions (CommonMark §4.7)
// ============================================================

/// A parsed link reference definition: [label]: /url "title"
struct LinkRefDef {
    std::string url;
    std::string title;  // empty if no title
};

/// Case-insensitive map from normalized label → definition.
/// Labels are normalized per CommonMark: case-folded, whitespace
/// collapsed, backslash escapes processed.
using LinkRefMap = std::map<std::string, LinkRefDef>;

/// Block token types
enum class BlockTokenKind : std::uint8_t {
    Paragraph,
    Heading,
    CodeBlock,
    UnorderedList,
    OrderedList,
    Blockquote,
    Table,
    HorizontalRule,
    MathBlock,   ///< Block math: $$...$$ (LaTeX display math)
    HtmlBlock,   ///< HTML block (CommonMark §4.6) — raw HTML passed through
    FootnoteDef,  ///< GFM footnote definition (hoisted to footnotes section)
};

/// A block-level markdown token
struct BlockToken {
    BlockTokenKind kind;
    // Heading
    int heading_level = 0;
    // Code block
    std::string code_lang;
    std::string code_content;
    // Table
    std::vector<std::string> table_headers;
    std::vector<std::vector<std::string>> table_rows;
    // GFM table column alignment: 0=none(default ---), 1=left(:---),
    // 2=center(:---:), 3=right(---:).  Parsed from the separator row's
    // ':' markers.
    std::vector<std::int8_t> table_align;
    // Table: link reference definitions from the enclosing document,
    // so the HTML serializer can resolve reference links in cells.
    // Only populated for Table blocks.
    LinkRefMap table_link_refs;
    // Lists: each item is a vector of sub-blocks (paragraphs, code blocks,
    // nested lists, blockquotes, etc.) — CommonMark §5.2.
    std::vector<std::vector<BlockToken>> list_items;
    // Per-item nesting depth (0 = top-level, 1 = first indent, ...).
    // Used by render_olist() for depth-based numbering (Arabic / letters /
    // Roman) and render_ulist() for indentation.
    std::vector<int> list_depths;
    // GFM task list items: per-item state.  -1 = not a task item,
    // 0 = unchecked ([ ]), 1 = checked ([x]).  Empty for non-task lists.
    std::vector<std::int8_t> task_state;
    // Ordered list: the starting number (first item's number).
    // CommonMark §5.2: <ol start="N"> when N != 1.
    int list_start = 1;
    // Whether the list is "loose" (has blank lines between or within items).
    // Loose lists wrap item content in <p> tags; tight lists don't.
    bool list_loose = false;
    // Paragraph / Heading / Blockquote: inline tokens
    std::vector<InlineToken> inlines;
    // Blockquote: nested block-level content (recursively parsed).
    // When non-empty, the renderer emits each sub-block inside the
    // blockquote element instead of wrapping inlines in a single <p>.
    std::vector<BlockToken> sub_blocks;
    // Blockquote depth
    int quote_depth = 0;
    // Math block content (LaTeX source for $$...$$ display math)
    std::string math_content;
    // HTML block content (raw HTML, CommonMark §4.6)
    std::string html_content;
    // FootnoteDef: raw label as written in [^label]: (used for href/id).
    // Definition content is in sub_blocks (parsed via recursive lex_blocks).
    std::string footnote_label;
};

// ============================================================
// Fast-path: Detect markdown syntax
// ============================================================

namespace detail {

/// Characters that indicate markdown syntax. If none are present, skip
/// the full lexer and render as a single plain paragraph.
[[nodiscard]] constexpr bool has_markdown_syntax(std::string_view s) {
    // Leading tabs or 4+ leading spaces may be indented code blocks.
    if (!s.empty() && s[0] == '\t') return true;
    if (s.size() >= 4 && s[0] == ' ' && s[1] == ' ' &&
        s[2] == ' ' && s[3] == ' ') {
        return true;
    }
    // Sample first 500 chars — if markdown exists it's usually early.
    const std::size_t sample_len = std::min(s.size(), std::size_t{500});
    for (std::size_t i = 0; i < sample_len; ++i) {
        char c = s[i];
        if (c == '#' || c == '*' || c == '`' || c == '|' ||
            c == '[' || c == ']' || c == '<' || c == '>' || c == '-' ||
            c == '_' || c == '~' || c == '$' || c == '\\' || c == '\t' ||
            c == '@') {
            return true;
        }
        // Ordered list marker: digit(s) followed by . or ) and space
        if (c >= '0' && c <= '9') {
            std::size_t j = i;
            while (j < sample_len && s[j] >= '0' && s[j] <= '9') ++j;
            if (j < sample_len && (s[j] == '.' || s[j] == ')') &&
                j + 1 < sample_len && (s[j + 1] == ' ' || s[j + 1] == '\t')) {
                return true;
            }
        }
        // GFM extended autolinks: bare URLs (www., ://)
        if (c == 'w' && i + 4 <= sample_len &&
            s[i + 1] == 'w' && s[i + 2] == 'w' && s[i + 3] == '.') {
            return true;
        }
        if (c == ':' && i + 3 <= sample_len &&
            s[i + 1] == '/' && s[i + 2] == '/') {
            return true;
        }
        // Potential entity reference: &Name; or &#NNN; / &#xHH;
        if (c == '&' && i + 1 < sample_len) {
            char n = s[i + 1];
            if ((n >= '0' && n <= '9') || (n >= 'a' && n <= 'z') ||
                (n >= 'A' && n <= 'Z') || n == '#') {
                return true;
            }
        }
        if (c == '\n') {
            // Hard line break: 2+ trailing spaces or backslash before \n
            if (i >= 2 && s[i - 1] == ' ' && s[i - 2] == ' ') return true;
            if (i >= 1 && s[i - 1] == '\\') return true;
            // Check for ordered list start on next line
            if (i + 1 < sample_len) {
                char n = s[i + 1];
                if (n >= '0' && n <= '9') return true;
                // Check for heading / list / blockquote after newline
                if (n == '#' || n == '-' || n == '*' || n == '>') return true;
            }
        }
    }
    // Check for double-newline (paragraph break)
    if (s.find("\n\n") != std::string_view::npos) return true;
    return false;
}

} // namespace detail

// ============================================================
// Inline Tokenizer
// ============================================================

namespace detail {

/// Tokenize inline markdown formatting.
/// Handles: bold (**), italic (*), code (`), links [text](url),
/// and escaped characters (\*). Strikethrough (~~) is intentionally NOT
/// handled — see note above matching the marked configuration.
/// When link_refs is non-null, reference links ([text], [text][label],
/// [text][]) are resolved against it (CommonMark §4.7 + §6.6).
/// When gfm_extensions is false, GFM-only features (extended autolinks)
/// are disabled (CommonMark mode).
[[nodiscard]] std::vector<InlineToken> tokenize_inline(
    std::string_view text, const LinkRefMap* link_refs = nullptr,
    bool gfm_extensions = true);

/// Normalize a label per CommonMark: Unicode case-fold, collapse whitespace.
/// Used for footnote label lookup (case-insensitive) and link ref labels.
[[nodiscard]] std::string normalize_label(std::string_view label);

} // namespace detail

// ============================================================
// Table Parsing Helpers
// ============================================================

namespace detail {

[[nodiscard]] std::string trim_cell(std::string_view value);

[[nodiscard]] std::vector<std::string> split_table_row(std::string_view line);

[[nodiscard]] bool is_table_separator(std::string_view line);

} // namespace detail

// ============================================================
// Ordered List Detection
// ============================================================

namespace detail {

/// Count leading whitespace characters (spaces + tabs) in a line.
/// Each 2 spaces or 1 tab counts as one indentation level, matching
/// the marked tokenizer's `indent` calculation.
[[nodiscard]] int count_list_indent(std::string_view line);

[[nodiscard]] std::optional<std::pair<int, std::string>>
parse_ordered_list(std::string_view line);

} // namespace detail

// ============================================================
// Block-Level Lexer
// ============================================================

namespace detail {

/// Split source string into lines.
[[nodiscard]] std::vector<std::string> split_lines(std::string_view source);

/// Lex a markdown document into block tokens.
/// When gfm_extensions is false, GFM-only features (extended autolinks)
/// are disabled (CommonMark mode).
/// When in_blockquote is true, setext heading detection is disabled —
/// a setext underline on a lazy continuation line is paragraph text,
/// not a heading (CommonMark §4.3 example 93).
[[nodiscard]] std::vector<BlockToken> lex_blocks(std::string_view source,
                                                   bool gfm_extensions = true,
                                                   bool in_blockquote = false);

} // namespace detail

// ============================================================
// LRU Token Cache
// ============================================================

namespace detail {

/// LRU cache for tokenized markdown documents.
/// Keyed by content hash to avoid retaining full strings.
class TokenCache {
public:
    explicit TokenCache(std::size_t max_size = 256);

    /// Look up tokens for a content string. Returns nullptr if not found.
    [[nodiscard]] const std::vector<BlockToken>* find(std::string_view key) const;

    /// Insert a new entry, evicting LRU if at capacity.
    void put(std::string key, std::vector<BlockToken> tokens);

    void clear() { map_.clear(); lru_.clear(); }
    [[nodiscard]] std::size_t size() const { return lru_.size(); }
    [[nodiscard]] std::size_t max_size() const { return max_size_; }

private:
    struct CacheEntry {
        std::string key;
        std::vector<BlockToken> tokens;
    };

    std::size_t max_size_;
    mutable std::list<CacheEntry> lru_;
    std::unordered_map<std::string, typename std::list<CacheEntry>::iterator> map_;
};

/// Global token cache instance
TokenCache& global_token_cache();

} // namespace detail

// ============================================================
// GitHub Issue Reference Linkification
// ============================================================

namespace detail {

/// A text segment produced by linkify_issue_references(): plain text with an
/// optional URL (empty for plain text, non-empty for hyperlinked refs).
struct TextSegment {
    std::string text;
    std::string url;  // empty for plain text
};

/// Issue reference pattern and linkifyIssueReferences()
///
/// Pattern: owner/repo#123  →  clickable OSC 8 link to GitHub issues page.
///
/// Regex equivalent:  /(^|[^\w.\/-])([A-Za-z0-9][\w-]*\/[A-Za-z0-9][\w.-]*)#(\d+)\b/g
///
/// We use manual scanning instead of std::regex for performance (markdown
/// rendering is on the hot path for streaming updates).
///
/// @param text  Plain text to scan for issue references.
/// @return      Text segments: plain text interspersed with hyperlinked
///              issue references (url non-empty).
[[nodiscard]] std::vector<TextSegment> linkify_issue_references(
    std::string_view text);

/// Split text into wrappable segments for flexbox layout.
///
/// FTXUI's hbox does NOT wrap — when the total child width exceeds the
/// available width, box_helper::Compute proportionally shrinks every child,
/// and Text::Render() truncates at the shrunk boundary, silently swallowing
/// characters from the middle of inline elements.
///
/// This function splits text into segments that flexbox can wrap at:
///   - ASCII words (runs of half-width, non-space characters) — atomic
///   - Spaces — breakable points
///   - Full-width characters (CJK, etc.) — each is its own breakable segment
///
/// The caller creates one styled text() element per segment and passes them
/// to flexbox(), which wraps at segment boundaries.
///
/// at_paragraph_start is true only before the first non-space character of
/// the entire paragraph (shared across all per-token calls).  It
/// distinguishes a paragraph-leading space (CommonMark trims these — safe
/// to skip) from an inter-token space (e.g. Text(" end") after
/// Bold("world")) which must be preserved.
///
/// trim_leading_space controls whether paragraph-leading spaces are dropped.
/// Code spans and other content tokens where spaces are significant pass
/// false so their leading spaces are preserved.
[[nodiscard]] std::vector<std::string> split_for_wrapping(
    std::string_view text, bool& at_paragraph_start,
    bool trim_leading_space = true);

} // namespace detail

// ============================================================
// Inline Rendering
// ============================================================

namespace detail {

/// Render inline tokens to FTXUI element with theme colors.
///
/// Inline token formatting:
///   - strong (bold)  -> bold
///   - em (italic)    -> italic
///   - codespan       -> color('permission', theme)  (rgb(87,105,247) on dark)
///   - link           -> OSC 8 hyperlink (rendered as underlined text here)
///   - text           -> linkifyIssueReferences(text)
/// Strikethrough (del) is intentionally disabled (the `del` tokenizer is
/// disabled in the marked configuration).
[[nodiscard]] Element render_inlines(
    const std::vector<InlineToken>& tokens,
    const MarkdownOptions& opts);

} // namespace detail

// ============================================================
// Block Rendering
// ============================================================

namespace detail {

/// Render a heading block.
///
/// Heading formatting:
///   depth 1 (h1) -> bold + italic + underline(content) + EOL + EOL
///   depth 2 (h2) -> bold(content) + EOL + EOL
///   default (h3+) -> bold(content) + EOL + EOL
/// No coloring — only bold/italic/underline attributes. The
/// prior divergent renderer added cyan/white coloring, which is not
/// part of the spec.
/// The trailing EOL+EOL becomes a blank line under the heading.
[[nodiscard]] Element render_heading(const BlockToken& tok,
                                     const MarkdownOptions& opts);

/// Render a code block.
///
/// Code block formatting:
///   if (!highlight) return token.text + EOL;
///   return highlight.highlight(token.text, {language}) + EOL;
/// The code block is the highlighted text ONLY — no border, no line
/// numbers, no scroll status bar, no [Copy] tag. cli-highlight emits ANSI
/// per-line; those lines are rendered stacked. The prior divergent renderer
/// called RenderCodeHighlight, which injected a copy corner tag, a status
/// bar (`[j/k] scroll [q] close`), and line-number gutters — none of which
/// appear in the terminal-native output. That chrome polluted every fenced
/// code block in assistant messages.
///
/// We render the existing code_highlight tokenizer's per-line tokens with
/// show_line_numbers=false and no surrounding frame, preserving the prior
/// fix (no border) and achieving shiki/cli-highlight visual parity.
[[nodiscard]] Element render_code_block(const BlockToken& tok,
                                        const MarkdownOptions& opts);

// ============================================================
// List Numbering Helpers
// ============================================================

namespace detail {

/// Convert a positive integer to its lowercase alphabetic representation.
/// numberToLetter(): 1→"a", 2→"b", ..., 26→"z",
/// 27→"aa", 28→"ab", etc.
[[nodiscard]] std::string number_to_letter(int n);

/// Convert a positive integer to its lowercase Roman numeral representation.
/// numberToRoman(): 1→"i", 4→"iv", 9→"ix", etc.
/// Returns the Arabic string for values outside the classical range (1–3999).
[[nodiscard]] std::string number_to_roman(int n);

/// Select the ordered-list number label based on nesting depth.
///
/// getListNumber(listDepth, orderedListNumber):
///   depth 0,1 → Arabic numeral
///   depth 2   → lowercase letter
///   depth 3   → lowercase Roman numeral
///   depth ≥4  → Arabic (wraps back)
///
/// @param depth  List nesting level (0 = top-level).
/// @param n      1-based item number within its depth group.
/// @return       Label string without the trailing ". " (caller appends it).
[[nodiscard]] std::string get_list_number(int depth, int n);

} // namespace detail

/// Render an unordered list.
///
/// A list_item's text token renders as
///   `${'  '.repeat(listDepth)}- ${content}${EOL}`
/// Top-level (listDepth 0) items use `- ` with no leading indent. Nested
/// items get `'  '.repeat(depth)` indentation prefix. The prior divergent
/// renderer used a cyan `•` bullet and 2-space indent — neither is
/// part of the spec.
[[nodiscard]] Element render_ulist(const BlockToken& tok,
                                   const MarkdownOptions& opts,
                                   int depth = 0);

/// Render an ordered list.
///
/// getListNumber(listDepth, n):
///   depth 0,1 → Arabic numeral  ("1.", "2.")
///   depth 2   → lowercase letter ("a.", "b.")
///   depth 3   → lowercase Roman  ("i.", "ii.")
///   depth 4+  → Arabic (wraps)
///
/// Numbering resets per depth level: each time a deeper depth appears,
/// numbering starts from 1 for that depth. This mirrors how marked
/// produces nested `list` tokens each with their own item counter.
///
/// Each item also gets `'  '.repeat(depth)` indentation prefix.
[[nodiscard]] Element render_olist(const BlockToken& tok,
                                   const MarkdownOptions& opts,
                                   int depth = 0);

/// Render a blockquote.
///
/// Blockquote formatting:
///   const bar = dim(BLOCKQUOTE_BAR)   // ▎ U+258E
///   inner.split(EOL).map(line =>
///     stripAnsi(line).trim()
///       ? `${bar} ${italic(line)}`
///       : line)
/// Each non-blank line is prefixed with a DIM ▎ and the content is italic at
/// normal brightness (dim on the bar only, not the text — dim is nearly
/// invisible on dark themes). The prior divergent
/// renderer used a blue bar, a non-dim content, and a background color —
/// all absent from the spec.
[[nodiscard]] Element render_blockquote(const BlockToken& tok,
                                        const MarkdownOptions& opts);

/// Render an HTML block as plain text in the terminal (tags stripped).
[[nodiscard]] Element render_html_block(const BlockToken& tok,
                                        const MarkdownOptions& opts);

/// Check whether an element is an empty block (e.g. an HTML block that
/// stripped to nothing).  Used by the block flow to skip empty blocks
/// entirely, avoiding extra blank lines from separators.
[[nodiscard]] bool is_empty_block(const Element& el);

/// Render a GFM table using Unicode box-drawing characters, matching the
/// MarkdownTable renderer's non-wrapped
/// path when columns fit the terminal at ideal width.
///
/// The output is a fully bordered table:
///   ┌──────┬──────┐
///   │ hdr1 │ hdr2 │
///   ├──────┼──────┤
///   │ c1   │ c2   │
///   └──────┴──────┘
///
/// Borders use: │ U+2502 (vertical), ─ U+2500 (horizontal),
///              corners (┌┐└┘ U+250C/10/14/18),
///              cross pieces (┬┼┴ U+252C/3C/34, ├┤ U+251C/24).
///
/// Column width = max(string_width(header), max(string_width(cell))) with a
/// minimum of 3. Header cells are bold; data cells are plain.
///
/// The previous divergent renderer emitted a plaintext ASCII pipe table
/// (`|---|---|`) which leaked literal `---` dashes into the rendered text
/// (P0 bug: GFM separator line leaked as paragraph content). Using
/// box-drawing ─ chars instead of ASCII hyphens prevents any spurious
/// "---" substring match and matches the terminal-native output.
[[nodiscard]] Element render_table(const BlockToken& tok,
                                   const MarkdownOptions& opts);

/// Render a horizontal rule.
///
/// The 'hr' token returns the literal
/// string "---" (not a separator/box-drawing line). The prior divergent
/// renderer used ftxui::separator(), which draws a full-width ── rule —
/// absent from the spec.
[[nodiscard]] Element render_hr(const MarkdownOptions& opts);

} // namespace detail

// ============================================================
// Main Markdown Renderer
// ============================================================

/// Parse and render a markdown document string to FTXUI elements.
/// Uses token cache for repeated renders.
[[nodiscard]] Element render_markdown(std::string_view source,
                                      const MarkdownOptions& opts = {});

/// Convenience: render with dim_color = true
[[nodiscard]] Element render_markdown_dim(std::string_view source);

// ============================================================
// HTML Serialization (for conformance testing)
// ============================================================

/// Serialize a markdown document to an HTML string.
/// Uses the same block lexer + inline tokenizer as the FTXUI renderer,
/// then walks the AST and emits standard HTML.  Intended for running
/// CommonMark/GFM conformance test suites — not used in production
/// rendering (the terminal renderer is the primary output path).
/// Set gfm_extensions to false for CommonMark-only mode (no extended
/// autolinks, no GFM-specific features).
[[nodiscard]] std::string render_markdown_to_html(std::string_view source,
                                                   bool gfm_extensions = true);

// ============================================================
// Streaming Markdown
// ============================================================

/// Streaming markdown renderer optimized for live output.
/// Maintains a "stable prefix" that has been fully parsed and cached,
/// and re-parses only the growing "unstable suffix" on each update.
///
/// Algorithm: find the last top-level block boundary. Everything before
/// that boundary is stable (memoized via the token cache) and only the
/// final block is re-parsed per delta. Code fences are correctly handled
/// as single tokens even when unclosed.
class StreamingMarkdown {
public:
    /// Update the content and return rendered element.
    Element update(std::string_view content);

    /// Reset the streaming state
    void reset();

    /// Get current stable prefix length (for debugging)
    [[nodiscard]] std::size_t stable_length() const {
        return stable_prefix_.size();
    }

private:
    std::string stable_prefix_;

    /// Find the last safe block boundary in content.
    /// A safe boundary is a double-newline or a closing code fence.
    [[nodiscard]] static std::size_t find_block_boundary(
        std::string_view content);
};

// ============================================================
// Interactive Markdown Component
// ============================================================

struct MarkdownComponentOptions {
    std::string content;
    MarkdownOptions render_options;
    int visible_lines = 20;
    bool show_scrollbar = true;
};

/// An interactive markdown viewer component with scroll support.
/// Useful for long markdown documents in dialogs.
class MarkdownComponentBase : public ComponentBase {
public:
    explicit MarkdownComponentBase(MarkdownComponentOptions opts);

    Element Render() override;

    bool OnEvent(Event event) override;

    void set_content(std::string content) {
        opts_.content = std::move(content);
    }

    void set_options(MarkdownOptions render_opts) {
        opts_.render_options = std::move(render_opts);
    }

private:
    MarkdownComponentOptions opts_;
    int scroll_offset_;
};

/// Create an interactive Markdown viewer component
[[nodiscard]] Component MarkdownComponent(MarkdownComponentOptions opts);

// ============================================================
// Cache Management
// ============================================================

/// Clear the global markdown token cache.
/// Call this when memory pressure is high or on settings change.
void clear_markdown_cache();

/// Get current cache size (for diagnostics)
[[nodiscard]] std::size_t markdown_cache_size();

/// Get maximum cache size
[[nodiscard]] std::size_t markdown_cache_max_size();

} // namespace loom::ui
