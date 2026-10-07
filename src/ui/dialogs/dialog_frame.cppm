/// @file dialog_frame.cppm
/// @brief Reusable DialogFrame component — standard permission-style frame.
///
/// MODULE:   loom.ui.dialogs.frame
/// LICENCE:  Exported.  Imported by all dialog renderers that use the
///           standard permission-style frame.
///
/// LAYOUT:
///   ╭───────────────────────────────────────────────────╮
///   │ Title                            [worker badge]  │
///   │ subtitle                                         │
///   ├───────────────────────────────────────────────────┤
///   │                                                   │
///   │  content                                          │
///   │                                                   │
///   ╰───────────────────────────────────────────────────╯
///
/// FEATURES:
///   - Rounded top border (╭╮ style — no bottom border in overlay mode)
///   - Title row with bold title + optional subtitle
///   - Optional worker badge (right-aligned in title row)
///   - Optional title-right slot (e.g. close button, status)
///   - Configurable inner padding
///   - Themed border color (by risk level or custom color)
///   - Bottom-anchored "floating" look when used in overlay/bottom slots
module;

#include <cstdint>

#include <ftxui/dom/elements.hpp>

export module loom.ui.dialogs.frame;

import std;

import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.component_primitives;
import loom.ui.permissions.components;

export namespace loom::ui::dialogs::frame {

using namespace ftxui;
using Theme = loom::ui::design::theme::Theme;
using Role = loom::ui::design::tokens::Role;
namespace primitives = loom::ui::design::primitives;
namespace pc = loom::ui::permissions::components;

// ============================================================
// DialogFrameStyle — visual variant
// ============================================================

/// Style variant for the frame border/background.
/// Dialog color semantics: the overwhelming default is
/// `Permission` (a slate-blue/lavender hue, shared with the
/// suggestion token).  Info is reserved for auxiliary blue accents;
/// Danger/Error map to the error color (used only by the Sandbox
/// bypass-permissions dialog).
enum class FrameStyle : std::uint8_t {
    Permission,     ///< Default dialog frame
    Info,          ///< Auxiliary info-blue
    Success,       ///< Green
    Warning,       ///< Yellow/orange
    Danger,        ///< Red (high risk, alias of Error)
    Error,         ///< Sandbox bypass dialogs
    Critical,      ///< Bold red (critical risk)
    Muted,         ///< Dim border
};

/// Get the border color for a frame style.
/// Color semantics: Permission/Suggestion share a value.
[[nodiscard]] inline Color frame_border_color(FrameStyle style,
                                              const Theme& theme) {
    switch (style) {
        case FrameStyle::Permission: return theme.color_for(Role::Permission);
        case FrameStyle::Info:       return theme.color_for(Role::Info);
        case FrameStyle::Success:    return theme.color_for(Role::Success);
        case FrameStyle::Warning:    return theme.color_for(Role::Warning);
        case FrameStyle::Danger:
        case FrameStyle::Error:
        case FrameStyle::Critical:   return theme.color_for(Role::Danger);
        case FrameStyle::Muted:      return theme.color_for(Role::Muted);
    }
    return theme.color_for(Role::Permission);
}

// ============================================================
// PaneVariant — layout mode
// ============================================================

/// Controls how the frame is laid out — matches the two branches of
/// the Pane component.
///
///   * `PanelPadded` — non-modal panels (Standalone / Bottom / Overlay
///     slots, or dialogs rendered outside a modal centering wrapper).
///     Rendering:
///       `[empty row (top padding = 1)]` → `[Divider(color)]` →
///       `[content padded with horizontal padding = 2]`
///     NO rounded corners, NO 4-sided frame.
///
///   * `ModalMinimal` — content wrapped inside an outer modal (Modal slot
///     centred by dialog_queue_render::RenderModalDialog).
///     Renders the inside-modal branch → no divider, only horizontal padding = 1,
///     no border chrome.
enum class PaneVariant : std::uint8_t {
    PanelPadded = 0,
    ModalMinimal,
};

// ============================================================
// DialogFrameProps — properties for the frame
// ============================================================

/// Properties for building a DialogFrame.
struct DialogFrameProps {
    std::string title;
    std::optional<std::string> subtitle;
    /// Default = Permission — matches the default color="permission".
    FrameStyle style = FrameStyle::Permission;

    /// Optional worker badge element (rendered right of title).
    std::optional<Element> worker_badge;

    /// Optional right-aligned slot in title row (status, close button, etc.).
    std::optional<Element> title_right;

    /// The main content body.
    Element content = text("");

    /// Optional explicit border color override.
    /// When set, takes precedence over the `style`-derived border color.
    /// Free-form theme color key that gets resolved to an ANSI color at
    /// render time.
    std::optional<Color> color_override;

    /// Optional explicit title color override.
    /// When set, the title text color is replaced with this value (the default
    /// is theme-inherited / no color wrap, i.e. the terminal foreground).
    std::optional<Color> title_color_override;

    /// Horizontal padding inside the content area (cells).
    int inner_padding_x = 1;
    /// Vertical padding inside the content area (cells).
    int inner_padding_y = 0;

    /// Whether to show a full border or just the top "cap".
    /// Full border = modal panels, top-only = floating overlay dialogs.
    bool full_border = true;

    /// Whether to use rounded corners.
    /// @deprecated Kept as a no-op for ABI-compat.  The frame never draws
    ///             a 4-sided border, so the concept of "rounded" does not
    ///             apply.  New callers should leave this at its default.
    bool rounded = true;

    /// Pane layout mode — see PaneVariant docs.
    /// Callers must set this based on which DialogSlot the content will
    /// be composed into:
    ///   * PanelPadded  → Standalone, Bottom, Overlay slots, or dialogs
    ///                    rendered outside the modal centering wrapper.
    ///   * ModalMinimal → Modal slot (the outer centering dbox already
    ///                    provides "modal" visual separation).
    PaneVariant pane_variant = PaneVariant::PanelPadded;
};

// ============================================================
// DialogFrame — the reusable frame component
// ============================================================

/// Build a standard dialog frame with title, subtitle, and content.
[[nodiscard]] inline Element DialogFrame(const DialogFrameProps& props,
                                          const Theme& theme) {
    // Resolve border color: explicit override wins, otherwise derive from FrameStyle.
    auto border_col = props.color_override.value_or(
        frame_border_color(props.style, theme));

    // ---- Title row ----
    auto title_el = [&]() -> Element {
        auto t = text(props.title) | bold;
        if (props.title_color_override) {
            t = t | color(*props.title_color_override);
        }
        return t;
    }();

    Elements title_row_els;
    title_row_els.push_back(title_el);

    // Optional worker badge (to the right of title, separated by space)
    if (props.worker_badge) {
        title_row_els.push_back(text("  "));
        title_row_els.push_back(*props.worker_badge);
    }

    // filler() pushes title_right to the right edge
    title_row_els.push_back(filler());

    if (props.title_right) {
        title_row_els.push_back(*props.title_right);
    }

    auto title_row = hbox(title_row_els);

    // ---- Subtitle row ----
    auto subtitle_el = [&]() -> Element {
        if (!props.subtitle) return text("");
        return text(*props.subtitle) | dim | color(theme.color_for(Role::Muted));
    }();

    // ---- Header block (title + subtitle) ----
    Elements header_els;
    header_els.push_back(title_row);
    if (props.subtitle) {
        header_els.push_back(text(""));
        header_els.push_back(subtitle_el);
    }
    auto header = vbox(header_els);

    // ---- Padding for content ----
    // Layout padding values:
    //   Modal   → horizontal padding = 1 (no divider, no top padding)
    //   Panel   → horizontal padding = 2 (content column)
    //
    // `inner_padding_x` adds INSIDE the pane padding so callers that need
    // extra breathing room (e.g. settings tab content) still work.
    const int pane_padding_x =
        props.pane_variant == PaneVariant::ModalMinimal ? 1 : 2;
    const int total_pad_x = pane_padding_x + props.inner_padding_x;

    auto padded_content = [&]() -> Element {
        auto c = props.content;
        if (total_pad_x > 0) {
            auto pad = std::string(total_pad_x, ' ');
            c = hbox({ text(pad), c | xflex, text(pad) });
        }
        if (props.inner_padding_y > 0) {
            Elements ys;
            for (int i = 0; i < props.inner_padding_y; ++i) ys.push_back(text(""));
            ys.push_back(c);
            for (int i = 0; i < props.inner_padding_y; ++i) ys.push_back(text(""));
            c = vbox(ys);
        }
        return c;
    }();

    // ---- Assemble header row (h-padded same as content pane outer) ----
    // Header always uses PANE_PAD_X (not inner) — the title/subtitle
    // align visually flush with the left edge of the padded content text.
    Elements outer_header_els;
    {
        auto pad = std::string(pane_padding_x, ' ');
        auto hp_row = hbox({
            text(pad),
            header | xflex,
            text(pad),
        });
        outer_header_els.push_back(hp_row);
    }

    // ---- Divider between header and content ----
    // Full width, theme-colored divider.
    // In ModalMinimal mode the divider is omitted (modal content is
    // already visually separated by the outer dbox centering).
    Element divider_row = text("");
    if (props.pane_variant == PaneVariant::PanelPadded) {
        divider_row = pc::ThinDivider(border_col);
    }

    // ---- Assemble body (vertical column layout) ----
    //
    // PanelPadded:
    //   top padding = 1  →  empty row at top
    //   Divider(color)  →  full width color line
    //   Header          →  title + subtitle h-padded
    //   Divider         →  between header and content (ThinDivider already exists)
    //   Padded content  →  inner body
    //
    // ModalMinimal:
    //   Header          →  title + subtitle (no outer padding row)
    //   [no divider]
    //   Padded content  →  horizontal padding = 1 only
    Elements body_els;
    body_els.reserve(8);

    if (props.pane_variant == PaneVariant::PanelPadded) {
        // top padding row
        body_els.push_back(text(""));
        // Divider(color) — the top colored stripe.
        body_els.push_back(divider_row);
    }
    // Title + subtitle block.
    for (auto& el : outer_header_els) body_els.push_back(std::move(el));

    // ThinDivider between header and content (present in BOTH modes — it
    // demarcates the title area from the body, matching the legacy layout
    // callers already depend on.)
    body_els.push_back(pc::ThinDivider());

    // Main body.
    body_els.push_back(padded_content);

    auto body = vbox(std::move(body_els)) | xflex;

    // ---- Apply minimum width, NO 4-sided border, NO rounded corners ----
    //
    // The frame never applies a 4-sided border or rounded corners.  The
    // coloured stripe at the top (PanelPadded) + the inner ThinDivider
    // demarcate the structure.  Minimum width clamp preserves layout for
    // tiny terminals.
    return body | size(WIDTH, GREATER_THAN, 30);
}

// ============================================================
// Convenience builders
// ============================================================

/// Build a simple info dialog frame with title and message.
[[nodiscard]] inline Element SimpleInfoFrame(std::string_view title,
                                              std::string_view message,
                                              const Theme& theme) {
    DialogFrameProps props;
    props.title = std::string{title};
    props.style = FrameStyle::Info;
    props.content = paragraph(std::string{message});
    return DialogFrame(props, theme);
}

/// Build a warning dialog frame with title and message.
[[nodiscard]] inline Element SimpleWarningFrame(std::string_view title,
                                                 std::string_view message,
                                                 const Theme& theme) {
    DialogFrameProps props;
    props.title = std::string{title};
    props.style = FrameStyle::Warning;
    props.content = paragraph(std::string{message});
    return DialogFrame(props, theme);
}

/// Build a danger dialog frame with title and message.
[[nodiscard]] inline Element SimpleDangerFrame(std::string_view title,
                                                std::string_view message,
                                                const Theme& theme) {
    DialogFrameProps props;
    props.title = std::string{title};
    props.style = FrameStyle::Danger;
    props.content = paragraph(std::string{message});
    return DialogFrame(props, theme);
}

// ============================================================
// WorkerBadge — small "worker" indicator badge
// ============================================================

/// Render a worker badge (shown in dialog title for worker-initiated requests).
[[nodiscard]] inline Element WorkerBadge(std::string_view worker_id,
                                          const Theme& theme) {
    return hbox({
        text("👷 ") | dim,
        text(std::string{worker_id}) | bold | color(theme.color_for(Role::Info)) | dim,
    });
}

/// Render a compact worker dot badge (for smaller dialogs).
[[nodiscard]] inline Element WorkerBadgeCompact(std::string_view worker_id,
                                                 const Theme& theme) {
    return hbox({
        text("●") | color(theme.color_for(Role::Info)),
        text(" "),
        text(std::string{worker_id}) | dim,
    });
}

} // namespace loom::ui::dialogs::frame
