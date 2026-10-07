/// =========================================================================
/// @file fullscreen_layout.cppm
/// @brief FullscreenLayout — the slot-based REPL shell architecture.
///
/// MODULE:   loom.ui.chrome.fullscreen_layout
/// LICENCE:  Exported.  Imported by repl_screen.cppm (RenderReplScreen).
///
/// REGION MODEL:
///   ┌───────────────────────────────────────────────────────────┐
///   │ scrollwrap = vbox:                                        │
///   │   • StickyPromptHeader (1 row, optional)                  │
///   │   • ScrollBox (flex-grow) = {scrollable, overlay}            │
///   │   • NewMessagesPill (absolute bottom-centre, optional)      │
///   │   • bottom-float (absolute bottom-right, optional)          │
///   │    ↳ the scrollwrap is the flex-grow region (fills height)  │
///   ├───────────────────────────────────────────────────────────┤
///   │ bottom (fixed-height, max-height ≈ 50%) =                   │
///   │   SuggestionsOverlay, DialogOverlay, {bottom}              │
///   ├───────────────────────────────────────────────────────────┤
///   │ modal (absolute bottom-anchored, max-height = rows-PEEK):    │
///   │   ▔ top divider, {modal} body — paints over scrollwrap+   │
///   │   bottom; MODAL_TRANSCRIPT_PEEK rows of transcript visible│
///   │   above it                                                │
///   └───────────────────────────────────────────────────────────┘
///
/// These three are stacked via absolute positioning → in FTXUI we express
/// that stacking with `dbox` (overlay).  `scrollable` fills remaining
/// height via `flex`; `bottom` is pinned (no flex); `overlay` paints
/// inside the scroll region; `modal` floats anchored-bottom via dbox.
///
/// DISCIPLINE: this is a COMPOSITION module.  It accepts already-rendered
///   Element slots from the caller (repl_screen) and assembles them.  It
///   does NOT own message/prompt/dialog rendering — those stay in their
///   existing modules.  This mirrors how FullscreenLayout wraps a
///   `<ScrollBox>{scrollable}{overlay}</ScrollBox>` without knowing what
///   those props are.
///
/// CHROME (built here):
///   • StickyPromptHeader — a 1-row context breadcrumb shown above the
///     scroll region while scrolled up (single fixed row so the ScrollBox
///     anchor never shifts).
///     Interactive: click collapses it to a 0-row pad so row 0 of the
///     scroll region becomes the prompt that generated the visible
///     assistant responses (3-state sticky prompt: nullopt / {text,
///     scroll_target_row} / 'clicked' sentinel).
///   • NewMessagesPill — "N new messages ↓" / "Jump to bottom ↓" centred
///     at the bottom of the scroll region when new content arrived below
///     the fold while unpinned.
///   • Modal divider — the ▔ row atop an open modal pane.
/// =========================================================================

module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
export module loom.ui.chrome.fullscreen_layout;

import std;

import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.design_figures;

export namespace loom::ui::layout::fullscreen {

using namespace ftxui;
using Theme   = loom::ui::design::theme::Theme;
using Palette = loom::ui::design::tokens::Palette;
using Role    = loom::ui::design::tokens::Role;
namespace figures_ns = loom::ui::design::figures;

// ─── Constants ──────────────────────────────────────────────────────────────

/// Rows of transcript context kept visible above the modal pane's ▔ divider.
inline constexpr int kModalTranscriptPeek = 2;

// ─── Component-lifetime guard (copied verbatim from permission_rule_list.cppm
//     CompEl pattern — commit bd507bc).  An Element that holds a Component
//     shared_ptr alive so the Component's internal Box (used by mouse
//     reflect(), OnEvent) does not become a dangling reference.
// ──────────────────────────────────────────────────────────────────────────
namespace compel_detail {

class ComponentHolderNode : public Node {
 public:
    Component held_;
    ComponentHolderNode(Component c, Element el)
        : Node(Elements{std::move(el)}), held_(std::move(c)) {}
    void ComputeRequirement() override {
        Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }
    void SetBox(Box box) override {
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }
};

}  // namespace compel_detail

/// Wrap a Component's rendered Element in a Node that owns the Component
/// shared_ptr.  This is required whenever a Component (e.g. Button, or any
/// subclass with an internal `Box box_` / `OnEvent`) is rendered into a
/// stateless Element tree: dropping the Component immediately after
/// `Render()` produces a heap-use-after-free because the returned Element's
/// Nodes reference `&box_` / other fields on the destroyed Component.
[[nodiscard]] inline Element CompEl(Component c) {
    if (!c) return emptyElement();
    Element el = c->Render();
    return std::make_shared<compel_detail::ComponentHolderNode>(
        std::move(c), std::move(el));
}

// ─── Sticky prompt 3-state model ──────────────────────────────────────────
//
// The sticky prompt is a 3-way discriminant:
//   nullopt                             → at bottom, no header
//   { text, scroll_target_row }         → scrolled up; header visible
//   'clicked' (literal sentinel)        → user just clicked header → hide
//                                        it but keep collapsed padding so
//                                        the prompt occupies scroll row 0
//
// Collapsed padding (sticky active, no overlay) drops the top padding from
// 1 → 0 as soon as sticky is non-null, EVEN IN 'clicked' — this is how
// clicking the header leaves the prompt that generated the visible replies
// anchored at the absolute top of the visible scroll area.  On the next
// scroll event, the tracker re-emits {text, scroll_target_row} (new scroll
// window → new sticky prompt) and the header re-appears.
//
// We model this as:
//   sticky_active         → sticky_prompt.has_value() (drives collapsed padding)
//   sticky_header_visible → sticky is an OBJECT (not nullopt, not 'clicked')
//   sticky_prompt_text    → sticky_prompt.text
//   on_sticky_click       → sets the 'clicked' sentinel before invoking
//                           the real scroll-to-target

struct StickyPrompt {
    /// Header prompt text.  Empty is equivalent to no sticky prompt.
    /// The string shown in the StickyPromptHeader row when scrolled up.
    std::string text;
    /// Row index the header click should scroll to (content-coordinate, as
    /// produced by the virtual list's sticky tracker).  The
    /// scroll-target parameter; the header click handler invokes it.
    std::size_t scroll_target_row = 0;
};

// ─── Slot bundle ────────────────────────────────────────────────────────────
//
// Mirrors the layout props shape (scrollable, bottom, overlay, modal,
// bottom-float) plus the chrome-driver flags
// (hide_pill / hide_sticky / new_message_count / pill_visible).  Every field has
// a default so a caller can populate incrementally without breaking.

struct FullscreenLayoutSlots {
    /// Pinned header drawn ABOVE the scroll region (never scrolls out of
    /// view).  The logo header is placed OUTSIDE the virtual message list,
    /// so scrolling messages never clip it.
    /// Optional; render-time null check skips it.
    Element header{};

    /// Content that scrolls (messages, tool output).  The `scrollable` slot.
    /// Required — drives the flex-grow region.
    Element scrollable{};

    /// Content pinned to the bottom (status bar, prompt, footer).  The
    /// `bottom` slot.  Fixed height so it never collapses under scroll content.
    Element bottom{};

    /// Content painted inside the ScrollBox region over the transcript
    /// (PermissionRequest).  The `overlay` slot.  Optional.
    std::optional<Element> overlay{};

    /// Slash-command / centered dialog content.  The `modal` slot.
    /// Rendered in a bottom-anchored absolute pane with a ▔ top divider.
    std::optional<Element> modal{};

    /// Absolute bottom-right companion bubble (buddy speech).  The
    /// bottom-float slot.  Optional; can be a stub.
    std::optional<Element> bottom_float{};

    // ── Chrome-driver inputs ────────────────────────────────────────────────

    /// Sticky prompt 3-state model.
    ///   std::nullopt                        → null (at bottom, no chrome)
    ///   {text, scroll_target_row}           → mid scroll, header visible
    ///   {text, scroll_target_row} + `sticky_clicked=true` → 'clicked' sentinel
    ///                                         (header hidden, pad collapsed)
    std::optional<StickyPrompt> sticky_prompt{};

    /// True when the sticky prompt is in the 'clicked' sentinel state.
    /// When true: header is hidden but collapsed padding still applies.
    bool sticky_clicked{false};

    /// Callback fired by the StickyPromptHeader click handler.  The closure
    /// should (1) flip `sticky_clicked=true` and (2) perform the actual
    /// scroll-to-target.  We intentionally
    /// decouple "state change" from "scroll mutation" here: callers can
    /// implement re-render loops without this module owning scroll state.
    std::function<void(const StickyPrompt&)> on_sticky_click{};

    /// Force-hide the sticky header (e.g. viewing a teammate task).
    bool hide_sticky{false};

    /// Should the "N new messages" pill render this frame?  Caller computes
    /// it from scroll state (scroll_pinned_to_bottom + divider snapshot).
    bool pill_visible{false};

    /// Force-hide the pill.
    bool hide_pill{false};

    /// Count for the pill label.  0 => "Jump to bottom", >0 => "N new msgs".
    int new_message_count{0};

    /// Terminal geometry for modal height clamping.  Probed once per frame
    /// by the caller (repl_screen) via ink_utils::query_terminal_size().
    int term_cols{80};
    int term_rows{24};

    /// Callback fired when the user "clicks" the pill — scroll-to-bottom.
    /// When set, the pill is rendered as an interactive Button (FTXUI
    /// Component) instead of a static Element, so mouse clicks dispatch.
    std::function<void()> on_pill_click{};
};

// ─── Chrome helpers ─────────────────────────────────────────────────────────

/// Resolve the active palette via ThemeProvider current_theme() fallback.
[[nodiscard]] inline const Palette& active_palette() noexcept {
    return *loom::ui::design::theme::current_theme().palette;
}

/// Sticky prompt header — interactive Component:
///
///   • Fixed-height 1 row (truncate-end for long prompts)
///   • Background = userMessageBackground, hover = userMessageBackgroundHover
///   • Text = subtle color, prefix = figures.pointer (▶)
///   • onClick() invokes the passed callback (which scrolls + enters
///     'clicked' sentinel state)
///   • onMouseEnter/Leave toggles hover styling
///
/// FTXUI LIFETIME: callers MUST wrap the returned Component in `CompEl()`
/// before inserting it into an Element tree, otherwise the Component's
/// `Box box_` (used by reflect/OnEvent) is a UAF after the Component is
/// dropped (see CompEl note above / commit bd507bc).
class StickyPromptHeaderComponent : public ComponentBase {
 public:
    using OnClickFn = std::function<void()>;

    StickyPromptHeaderComponent(std::string text, OnClickFn on_click)
        : text_(std::move(text)), on_click_(std::move(on_click)) {}

    Element Render() override {
        const auto& pal = active_palette();
        const Color bg = hovered_
            ? loom::ui::design::tokens::token_by_role(pal, Role::UserMessageBackgroundHover)
            : loom::ui::design::tokens::token_by_role(pal, Role::UserMessageBackground);
        const Color fg = loom::ui::design::tokens::token_by_role(pal, Role::Subtle);
        // The `figures.pointer` prefix (▶ U+25B6).
        // NOTE: figures_ns::kPointer is constexpr string_view; std::string
        // ctor requires explicit materialization.
        const std::string pointer(figures_ns::kPointer);
        // NOTE: This FTXUI build does not expose a `truncation()` decorator.
        // We approximate end-truncation with xflex_shrink, which collapses
        // the text to available space and appends a default ellipsis in the
        // screen rasteriser when it overflows.  The "…" character is still
        // prepended to the trailing space as a visual sentinel.
        return hbox({
            text(" "),
            text(pointer) | color(fg),
            text(" "),
            text(text_) | color(fg) | xflex_shrink,
            text(" "),
        }) | bgcolor(bg)
           | reflect(box_)
           | size(HEIGHT, EQUAL, 1);
    }

    bool OnEvent(Event event) override {
        // onClick + onMouseEnter/Leave.
        if (event.is_mouse()) {
            const auto& m = event.mouse();
            const bool inside = box_.Contain(m.x, m.y);
            // hover follows the cursor entering/leaving the row.  This FTXUI
            // version does not surface a separate Mouse::Moved motion enum —
            // every mouse event carries the current cursor position, so we
            // update the hover state from any event whose cursor is inside
            // the reflected box.
            if (inside != hovered_) {
                hovered_ = inside;
                return true;
            }
            if (!inside) return false;
            if (m.button == Mouse::Left && m.motion == Mouse::Released) {
                hovered_ = false;
                if (on_click_) on_click_();
                return true;
            }
        }
        // Also accept Enter/Space as click (keyboard-accessible fallback).
        if (event == Event::Return || event == Event::Character(' ')) {
            if (on_click_) on_click_();
            return true;
        }
        return ComponentBase::OnEvent(std::move(event));
    }

    bool Focusable() const override { return static_cast<bool>(on_click_); }

 private:
    std::string text_;
    OnClickFn on_click_;
    bool hovered_ = false;
    Box box_;
};

/// Thin helper: wrap a StickyPromptHeaderComponent in a Component and then
/// in CompEl so it is safe to drop into an Element tree.
/// `<StickyPromptHeader>` element rendered inside the scrollwrap vbox
/// above the ScrollBox.
[[nodiscard]] inline Element StickyPromptHeader(std::string text,
    std::function<void()> on_click) {
    auto c = Make<StickyPromptHeaderComponent>(std::move(text),
                                                std::move(on_click));
    return CompEl(std::move(c));
}

/// "N new messages ↓" pill — centred at the bottom of the scrollwrap.
/// Slack-style: floats over the last content row.  `NewMessagesPill` uses
/// absolute-positioned at the bottom, centred horizontally,
/// so it stays anchored to the bottom-centre of the visible scroll region
/// regardless of scroll position (NOT at the scroll tail).  In FTXUI the
/// caller wraps this element in vbox({filler(), pill}) inside a dbox to
/// achieve the same overlay anchoring (see ComposeFullscreen).
[[nodiscard]] inline Element NewMessagesPill(int count, bool actionable,
                                             std::function<void()> on_click) {
    // count > 0 → "N new message(s)"; 0 → "Jump to bottom".
    std::string label = count > 0
        ? std::to_string(count) + " new message" + (count == 1 ? "" : "s")
        : std::string("Jump to bottom");
    // kArrowDown is a string_view holding the arrow-down glyph (↓ U+2193),
    // so materialize it to std::string for concatenation with const char[].
    label += " " + std::string(figures_ns::kArrowDown);

    const auto& pal = active_palette();
    const Color bg_normal = loom::ui::design::tokens::token_by_role(
        pal, Role::UserMessageBackground);
    const Color bg_hover = loom::ui::design::tokens::token_by_role(
        pal, Role::UserMessageBackgroundHover);
    const Color fg = loom::ui::design::tokens::token_by_role(pal, Role::Subtle);

    // If a click callback is supplied, render as an interactive Component
    // (Button pattern via a small OnEvent wrapper) so mouse events fire.
    if (on_click) {
        class PillComponent : public ComponentBase {
         public:
            PillComponent(std::string label, Color bg_n, Color bg_h,
                          Color fg, std::function<void()> cb)
                : label_(std::move(label)), bg_n_(bg_n), bg_h_(bg_h),
                  fg_(fg), cb_(std::move(cb)) {}
            Element Render() override {
                const Color bg = hovered_ ? bg_h_ : bg_n_;
                return hbox({
                    filler(),
                    text(" " + label_ + " ") | color(fg_) | bgcolor(bg),
                    filler(),
                }) | reflect(box_) | size(HEIGHT, EQUAL, 1);
            }
            bool OnEvent(Event ev) override {
                if (ev.is_mouse()) {
                    const auto& m = ev.mouse();
                    const bool inside = box_.Contain(m.x, m.y);
                    // Mirror StickyPromptHeader hover logic (see note there).
                    if (inside != hovered_) {
                        hovered_ = inside;
                        return true;
                    }
                    if (!inside) return false;
                    if (m.button == Mouse::Left && m.motion == Mouse::Released) {
                        hovered_ = false;
                        if (cb_) cb_();
                        return true;
                    }
                }
                return ComponentBase::OnEvent(std::move(ev));
            }
         private:
            std::string label_;
            Color bg_n_, bg_h_, fg_;
            std::function<void()> cb_;
            bool hovered_ = false;
            Box box_;
        };
        (void)actionable;
        return CompEl(Make<PillComponent>(std::move(label), bg_normal,
                                          bg_hover, fg, std::move(on_click)));
    }

    // Stateless fallback: render as plain text with actionable background.
    const Color bg = actionable ? bg_hover : bg_normal;
    return hbox({
        filler(),
        text(" " + label + " ") | color(fg) | bgcolor(bg),
        filler(),
    }) | size(HEIGHT, EQUAL, 1);
}

/// Modal pane: a bottom-anchored box with a ▔ top divider and the modal
/// body, sized so `kModalTranscriptPeek` rows of transcript remain visible
/// above it.  The modal pane: absolute-positioned, bottom-anchored,
/// max-height = terminal_rows - MODAL_TRANSCRIPT_PEEK.
[[nodiscard]] inline Element ModalPane(Element body, int term_cols, int term_rows) {
    int max_h = std::max(4, term_rows - kModalTranscriptPeek);
    // ▔ (U+2594) divider spanning the full width.
    // Build as UTF-8 so the multibyte sequence stays intact.
    static constexpr char kPeekDiv[] = "\xE2\x96\x94";   // ▔
    std::string divider;
    int n = std::max(1, term_cols);
    divider.reserve(static_cast<std::size_t>(n) * 3);
    for (int i = 0; i < n; ++i) divider.append(kPeekDiv);
    // The divider uses the permission color.
    const auto& pal = active_palette();
    const Color perm = loom::ui::design::tokens::token_by_role(pal, Role::Permission);
    return vbox({
        text(divider) | color(perm),
        // Body kept at natural height — the outer size(LESS_THAN, max_h)
        // caps it and the bottom-anchor filler above absorbs slack.  Do
        // NOT flex here: a windowed dialog border compresses badly under
        // flex when the anchor region is taller than the dialog.
        std::move(body),
    }) | size(HEIGHT, LESS_THAN, max_h);
}

// ─── Main composer ──────────────────────────────────────────────────────────

/// Compose the 5-slot fullscreen shell from already-rendered slot Elements.
///
/// Layout (region model, expressed in FTXUI):
///   result = dbox(
///     vbox(                              // normal-flow base
///       scrollwrap,                      //   flex-grow (scroll region)
///       bottom_slot                      //   fixed-height (pinned)
///     ),
///     modal_overlay                      //   absolute bottom-anchored
///   )
///
/// where scrollwrap = dbox overlay stack:
///     base = vbox({
///       stickyHeader?,                   //   1 row (optional; 3-state)
///       top padding,                     //   0 or 1 row (collapsed-padding)
///       scrollable_flex,                 //   flex-grow region
///       overlay?                         //   painted inside scroll region
///     })
///     + bottom-float overlay?            //   vbox{filler, hbox{filler, float}}
///     + pill_overlay?                   //   vbox{filler, pill}
///                                      //   both anchored to bottom via dbox
///
/// Sticky 3-state logic:
///   sticky_active    = !hide_sticky && sticky_present
///   header_visible   = sticky_active && !clicked && !overlay_present
///   collapsed_padding = sticky_active && !overlay_present
///
/// Translated into C++ field semantics:
///   sticky_active        = sticky_prompt.has_value()
///   header_visible       = sticky_active && !sticky_clicked
///                          && !overlay.has_value() && !hide_sticky
///   pad_top              = (sticky_active && !overlay.has_value()) ? 0 : 1
///
/// @note All slot Elements are MOVED into the composition (FTXUI Elements
///       are shared_ptr-backed, cheap to move).  Callers should pass
///       `std::move(slot.scrollable)` etc.
[[nodiscard]] inline Element ComposeFullscreen(FullscreenLayoutSlots s) {
    // ── 3-state sticky resolution ──
    const bool overlay_present = s.overlay.has_value() && *s.overlay;
    const bool sticky_active =
        !s.hide_sticky && s.sticky_prompt.has_value();
    // header_visible = sticky_active && !clicked && !overlay_present
    const bool header_visible = sticky_active && !s.sticky_clicked
                             && !overlay_present
                             && !s.sticky_prompt->text.empty();
    // collapsed-padding = sticky_active && !overlay_present
    // padding_top = collapsed ? 0 : 1
    const int padding_top = (sticky_active && !overlay_present) ? 0 : 1;

    // ── scrollwrap (flex-grow region) ────────────────────────────────────
    Elements scroll_children;

    // 1. Sticky prompt header (fixed 1 row, interactive).
    if (header_visible) {
        auto prompt = *s.sticky_prompt;
        std::function<void()> cb = nullptr;
        if (s.on_sticky_click) {
            cb = [prompt, cb = std::move(s.on_sticky_click)] {
                cb(prompt);
            };
        }
        scroll_children.push_back(
            StickyPromptHeader(std::move(prompt.text), std::move(cb)));
    }

    // 2. ScrollBox top-padding row.  The ScrollBox wraps scrollable+overlay
    //    with top padding = collapsed ? 0 : 1.  FTXUI vbox has no direct
    //    padding, so we emulate with an explicit 1-row spacer.  No
    //    bgcolor — the ScrollBox has no explicit background, so
    //    the padding area shows the terminal/canvas default.  Applying
    //    palette.background here created a subtly different-tinted row
    //    (RGB 32,33,36 vs terminal black 0,0,0) that some users perceived
    //    as a faint "白条" below the logo.
    if (padding_top > 0) {
        scroll_children.push_back(text("")
            | size(HEIGHT, EQUAL, padding_top));
    }

    // 3. The scrollable transcript — fills remaining height.
    if (s.scrollable) {
        scroll_children.push_back(std::move(s.scrollable) | flex);
    } else {
        scroll_children.push_back(filler());
    }

    // 4. Overlay painted inside the scroll region (PermissionRequest).
    //    This lives INSIDE <ScrollBox>{scrollable}{overlay}</ScrollBox>
    //    so the user can scroll up to see context.
    if (overlay_present) {
        scroll_children.push_back(std::move(*s.overlay));
    }

    // Build the base scroll content (normal-flow children only).
    Element scroll_content = vbox(std::move(scroll_children));

    // ── Floating overlays (absolute-positioned) ──────────────────────────
    // The parent Box wraps {header, ScrollBox, pill (absolute),
    //   bottom-float (absolute)}
    // Both pill and bottom-float use absolute positioning at the bottom, so they
    // float OVER the scroll content anchored to the bottom of the scrollwrap
    // region, NOT at the scroll tail.  In FTXUI we express this with dbox:
    // the base scroll_content shares its box with overlay layers that use
    // vbox({filler(), element}) to push the element to the bottom.
    //
    // Overlay z-order: later in the vector paints on top.  The DOM order is
    // pill before bottom-float; since both are absolute and non-overlapping
    // the order is cosmetic, but we keep the ordering (bottom-float on top).

    const bool pill_should_show =
        !s.hide_pill && s.pill_visible && !overlay_present;
    const bool float_should_show =
        s.bottom_float.has_value() && *s.bottom_float;

    if (pill_should_show || float_should_show) {
        Elements overlay_layers;
        overlay_layers.reserve(3);
        overlay_layers.push_back(std::move(scroll_content));

        // bottom-float: absolute, anchored bottom-right.
        if (float_should_show) {
            overlay_layers.push_back(vbox({
                filler(),
                hbox({ filler(), std::move(*s.bottom_float) }),
            }));
        }

        // NewMessagesPill: absolute, anchored bottom-centre.
        //   The pill Element itself is already centred (hbox{filler, text,
        //   filler}) and 1-row tall; vbox{filler, pill} anchors it to the
        //   bottom of the shared dbox allocation.
        if (pill_should_show) {
            overlay_layers.push_back(vbox({
                filler(),
                NewMessagesPill(
                    s.new_message_count,
                    /*actionable=*/static_cast<bool>(s.on_pill_click),
                    std::move(s.on_pill_click)),
            }));
        }

        scroll_content = dbox(std::move(overlay_layers));
    }

    Element scrollwrap = std::move(scroll_content) | flex;

    // ── bottom slot (fixed-height, max-height ≈ 50%) ─────────────────
    //    Wraps {SuggestionsOverlay, DialogOverlay, bottom} in a column
    //    with fixed height and max-height 50%.
    int bottom_max_h = std::max(4, s.term_rows / 2);
    Element bottom_slot = s.bottom
        ? (std::move(s.bottom) | size(HEIGHT, LESS_THAN, bottom_max_h))
        : (filler() | size(HEIGHT, EQUAL, 0));

    // ── normal-flow base: [pinned header (non-scroll)] + scrollwrap + bottom ──
    Elements normal_flow;
    normal_flow.reserve(3);
    if (s.header) normal_flow.push_back(std::move(s.header) | flex_shrink);
    normal_flow.push_back(std::move(scrollwrap));
    normal_flow.push_back(std::move(bottom_slot));
    Element base = vbox(std::move(normal_flow)) | flex;

    // ── modal overlay (absolute bottom-anchored, via dbox) ─────────────
    if (s.modal && *s.modal) {
        Element pane = ModalPane(std::move(*s.modal), s.term_cols, s.term_rows);
        Element modal_overlay = vbox({
            filler(),
            std::move(pane),
        }) | size(HEIGHT, EQUAL, s.term_rows);
        return dbox({ std::move(base), std::move(modal_overlay) });
    }
    return base;
}

} // namespace loom::ui::layout::fullscreen
