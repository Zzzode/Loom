// repl_screen_dialog_queue.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). the whole dialog_queue_render namespace: slot context + standalone /
// modal / overlay / bottom renderers, priority event dispatch and layer
// composition.
//
// Phase A (#184957): textual FTXUI GMF headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.dialog_store;
import loom.ui.dialogs.system;
import loom.ui.chrome.fullscreen_layout;

namespace loom::ui::repl_screen {
using namespace ftxui;

// =========================================================
// M7: Queue-based dialog rendering
// =========================================================
//
// The legacy dialog routing system (ReplMode → RouteDialog → dialog_stubs
// SimpleDialog builders) was REMOVED in M7 Task #124.
//
// ALL dialogs now flow through the DialogQueue:
//   - Engine side calls PushXxx() triggers in app.cppm / query_engine.cppm
//     (there are ZERO writes to a "DialogContext" bridge struct anywhere).
//   - Engine sets ReplMode for some legacy mode-aware UI chrome (mode opts,
//     vim indicator, etc.) but no longer uses it to determine dialog content.
//   - SyncReplModeToQueue was also REMOVED — audit confirmed there are ZERO
//     ReplMode→PushXxx() gaps in the engine call path; every engine-side
//     ReplMode assignment is paired with a matching DialogQueue push.
//
// Render path for queue:
//   RenderStandaloneDialog() (highest — fullscreen takeover)
//   RenderModalDialog()       (panel overlay — /settings, /tasks...)
//   RenderOverlayDialog()     (floating ToolPermission in scroll area)
//   RenderBottomDialog()      (prompt-affixed focused input dialogs)
//
// Event path:
//   HandleDialogQueueEvent() dispatches in priority order:
//     standalone > modal > overlay > bottom
//   with appropriate typing/animation suppression checks.
//
// Suppression rules:
//   - is_prompt_input_active = typing in progress → Bands 2..6 hidden
//   - is_tool_animation_active = JSX tool animation running → Band3 hidden

// M7: Queue-based dialog rendering
// =========================================================

// =========================================================
// M7 Dialog Framework: dialog_queue renderer dispatchers
// =========================================================
//
// Four slots (DialogSlot enum) in priority order:
//   Standalone > Modal > Overlay > Bottom
//
// The Render* functions return Element (wrapped in optional or
// Element{} to signal "nothing to render").  Handle* events return
// true if they consumed the event.
namespace dialog_queue_render {

namespace dsys = dsys_fw;

/// Build the render-time width/height context for the registry.
/// When `is_modal` is true, the context's `modal_available_cols/rows`
/// are populated using the same formula so modal renderers can size
/// content to the actual available pane area.
[[nodiscard]] dsys::DialogRenderContext MakeContext(
    int term_w, int term_h, bool is_modal,
    const void* repl_state)
{
    dsys::DialogRenderContext c;
    c.term_cols  = term_w;
    c.term_rows  = term_h;
    c.repl_state = repl_state;
    if (is_modal) {
        // MODAL_TRANSCRIPT_PEEK = 2 (fullscreen_layout.cppm kModalTranscriptPeek)
        // The -1 accounts for the ▔ divider row.
        constexpr int kModalTranscriptPeek =
            loom::ui::layout::fullscreen::kModalTranscriptPeek;
        c.modal_available_cols = std::max(10, term_w - 4);
        c.modal_available_rows = std::max(4, term_h - kModalTranscriptPeek - 1);
    }
    return c;
}

/// Standalone dialog: full-takeover render (no chrome).
/// Passes full terminal dimensions (no modal adjustments) since standalone
/// dialogs own the entire screen.
[[nodiscard]] Element RenderStandaloneDialog(ReplScreenState& s,
                                                    int w, int h) {
    auto peek = s.dialog_store.dialog_queue.peek_standalone_mut();
    if (!peek) return Element{};
    dsys::DialogPayloadVariant& payload = peek->get();
    if (std::holds_alternative<std::monostate>(payload)) return Element{};
    return s.dialog_store.dialog_renderers.render(payload, MakeContext(w, h));
}

/// Modal dialog (stack top): rendered full-width dbox above the rest.
/// Computes modal-available dimensions (cols-4, rows-PEEK-1) and passes
/// them via DialogRenderContext.modal_available_cols/rows so renderers
/// can use actual pane geometry instead of hardcoded fallbacks.
[[nodiscard]] Element RenderModalDialog(ReplScreenState& s,
                                               int w, int h) {
    auto peek = s.dialog_store.dialog_queue.peek_modal_mut();
    if (!peek) return Element{};
    dsys::DialogPayloadVariant& payload = peek->get();
    if (std::holds_alternative<std::monostate>(payload)) return Element{};
    // is_modal=true → populate modal_available_cols/rows.
    auto ctx = MakeContext(w, h, /*is_modal=*/true, &s);
    auto el = s.dialog_store.dialog_renderers.render(payload, ctx);
    if (!el) return Element{};
    // Clamp modal content to its available height.
    if (ctx.modal_available_rows > 0) {
        el = std::move(el) | size(HEIGHT, LESS_THAN, ctx.modal_available_rows);
    }
    return dbox({
        vbox({ filler(),
               hbox({ filler(), el, filler() }) | flex_shrink,
               filler() }) | flex,
    });
}

/// Overlay dialog (ToolPermission, Band3): centered floating dbox
/// clamped to 3/5 of terminal height so it never blocks the prompt.
/// Suppressed when prompt has input active or when allow_animation
/// dialogs are disabled (mid-tool-animation).
[[nodiscard]] Element RenderOverlayDialog(
    ReplScreenState& s,
    bool is_prompt_input_active,
    bool allow_dialogs_with_animation,
    int w, int h)
{
    auto peek = s.dialog_store.dialog_queue.peek_overlay_mut();
    if (!peek) return Element{};
    dsys::DialogPayloadVariant& payload = peek->get();
    if (std::holds_alternative<std::monostate>(payload)) return Element{};
    if (!dsys::should_show_dialog(payload, is_prompt_input_active,
                                   allow_dialogs_with_animation)) {
        return Element{};
    }
    auto el = s.dialog_store.dialog_renderers.render(payload, MakeContext(w, h));
    if (!el) return Element{};
    // Size clamp: 60% of rows max.
    const int max_h = std::max(12, h * 3 / 5);
    el = std::move(el) | size(HEIGHT, LESS_THAN, max_h);
    // Inject inside a centered dbox above the messages area.
    return dbox({
        vbox({ filler(),
               hbox({ filler(), std::move(el) | flex_shrink, filler() })
                   | flex_shrink,
               filler() }) | flex,
    });
}

/// Bottom slot: banner-style dialogs pushed to the bottom of the screen.
/// Wraps content in max-height = 50% (half the terminal rows).  Pass
/// actual dimensions.
[[nodiscard]] Element RenderBottomDialog(
    ReplScreenState& s,
    bool is_prompt_input_active,
    bool allow_dialogs_with_animation,
    int w, int h)
{
    auto peek = s.dialog_store.dialog_queue.peek_bottom_mut(is_prompt_input_active,
                                               allow_dialogs_with_animation);
    if (!peek) return Element{};
    dsys::DialogPayloadVariant& payload = peek->get();
    if (std::holds_alternative<std::monostate>(payload)) return Element{};
    if (!dsys::should_show_dialog(payload, is_prompt_input_active,
                                   allow_dialogs_with_animation)) {
        return Element{};
    }
    auto el = s.dialog_store.dialog_renderers.render(payload, MakeContext(w, h));
    if (!el) return Element{};
    return std::move(el) | size(WIDTH, EQUAL, w);
}

// ── Event dispatch: priority Standalone > Modal > Overlay > Bottom ──
bool DispatchDialogQueueEvents(ReplScreenState& s,
                                      const ftxui::Event& ev,
                                      bool is_prompt_input_active,
                                      bool allow_dialogs_with_animation) {
    namespace dsys = dsys_fw;

    // Standalone always takes every event.
    {
        auto peek = s.dialog_store.dialog_queue.peek_standalone_mut();
        if (peek) {
            dsys::DialogPayloadVariant& payload = peek->get();
            if (!std::holds_alternative<std::monostate>(payload)) {
                if (s.dialog_store.dialog_renderers.handle_event(payload, ev)) return true;
                // Standalone Escape fallback — closes as Abort.
                if (ev == ftxui::Event::Escape) {
                    s.dialog_store.dialog_queue.pop_standalone();
                    return true;
                }
            }
        }
    }

    // Modal stack top.
    {
        auto peek = s.dialog_store.dialog_queue.peek_modal_mut();
        if (peek) {
            dsys::DialogPayloadVariant& payload = peek->get();
            if (!std::holds_alternative<std::monostate>(payload)) {
                if (s.dialog_store.dialog_renderers.handle_event(payload, ev)) return true;
                if (ev == ftxui::Event::Escape) {
                    s.dialog_store.dialog_queue.pop_modal();
                    return true;
                }
            }
        }
    }
    // Overlay (Band3).  Skip when suppressed so typing can continue.
    {
        auto peek = s.dialog_store.dialog_queue.peek_overlay_mut();
        if (peek) {
            dsys::DialogPayloadVariant& payload = peek->get();
            if (!std::holds_alternative<std::monostate>(payload)) {
                if (dsys::should_show_dialog(payload, is_prompt_input_active,
                                             allow_dialogs_with_animation)) {
                    if (s.dialog_store.dialog_renderers.handle_event(payload, ev)) return true;
                }
            }
        }
    }

    // Bottom.
    {
        auto peek = s.dialog_store.dialog_queue.peek_bottom_mut(is_prompt_input_active,
                                                   allow_dialogs_with_animation);
        if (peek) {
            dsys::DialogPayloadVariant& payload = peek->get();
            if (!std::holds_alternative<std::monostate>(payload)) {
                if (dsys::should_show_dialog(payload, is_prompt_input_active,
                                             allow_dialogs_with_animation)) {
                    if (s.dialog_store.dialog_renderers.handle_event(payload, ev)) return true;
                }
            }
        }
    }
    return false;
}

namespace {
bool permission_payload(const dsys::DialogPayloadVariant& payload) {
    return std::holds_alternative<dsys::ToolPermissionPayload>(payload)
        || std::holds_alternative<dsys::SandboxPermissionPayload>(payload)
        || std::holds_alternative<dsys::WorkerSandboxPermissionPayload>(payload);
}

bool actionable_permission(ReplScreenState& s, dsys::DialogPayloadVariant& payload) {
    if (!s.dialog_store.dialog_renderers.has_renderer_and_handler(dsys::type_of(payload)))
        return false;
    return std::visit([](const auto& request) {
        using T = std::decay_t<decltype(request)>;
        if constexpr (std::is_same_v<T, dsys::ToolPermissionPayload> ||
                      std::is_same_v<T, dsys::SandboxPermissionPayload> ||
                      std::is_same_v<T, dsys::WorkerSandboxPermissionPayload>)
            return static_cast<bool>(request.on_response);
        return false;
    }, payload);
}
} // namespace

PreparedDialogLayers PrepareDialogLayers(
    ReplScreenState& s, bool typing, bool allow_animation, int w, int h) {
    PreparedDialogLayers prepared;
    // Preserve renderer execution order, including lazy payload initialization.
    prepared.bottom = RenderBottomDialog(s, typing, allow_animation, w, h);
    prepared.overlay = RenderOverlayDialog(s, typing, allow_animation, w, h);
    prepared.modal = RenderModalDialog(s, w, h);
    if (prepared.modal) {
        prepared.takes_focus = true;
        return prepared;
    }
    if (prepared.overlay) {
        auto top = s.dialog_store.dialog_queue.peek_overlay_mut();
        prepared.awaiting_permission = top && actionable_permission(s, top->get());
        prepared.takes_focus = prepared.awaiting_permission
            || (top && !permission_payload(top->get()));
    } else if (prepared.bottom) {
        auto top = s.dialog_store.dialog_queue.peek_bottom_mut(typing, allow_animation);
        prepared.awaiting_permission = top && actionable_permission(s, top->get());
        prepared.takes_focus = prepared.awaiting_permission
            || (top && !permission_payload(top->get()));
    }
    return prepared;
}

Element LayerAllDialogs(Element base_chrome, PreparedDialogLayers prepared) {
    Elements layers{std::move(base_chrome)};
    if (prepared.bottom)
        layers.push_back(vbox({filler(), std::move(prepared.bottom)}) | flex);
    if (prepared.overlay) layers.push_back(std::move(prepared.overlay));
    if (prepared.modal) layers.push_back(std::move(prepared.modal));
    if (layers.size() == 1) return layers.front();
    return dbox(std::move(layers));
}

} // namespace dialog_queue_render

} // namespace loom::ui::repl_screen
