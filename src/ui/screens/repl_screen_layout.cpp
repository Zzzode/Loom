// repl_screen_layout.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). RouteDialog and RenderReplScreen - terminal probe, unseen divider, the
// FullscreenLayout slot composition (scrollable/header/bottom) with both
// sticky/pill lambdas, and final dialog-queue layering.
//
// Phase A (#184957): textual FTXUI GMF headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.chrome.ink_utils;
import loom.platform.terminal_helpers;
import loom.ui.chrome.fullscreen_layout;
import loom.ui.foundation.logo;
import loom.ui.foundation.theme_provider;
import loom.ui.features.teams.live_teammates;
import loom.ui.features.todos.todo_panel;
import loom.ui.prompt.prompt_input_footer;
import loom.ui.messages.message_components;
import loom.ui.messages.virtual_list;
import loom.ui.visual.markdown;

namespace loom::ui::repl_screen {
using namespace ftxui;

/// Top-level dialog router: ReplMode -> overlay Element.
/// Panel modes (Normal / Tasks / Teams / Help / QuickOpen) return nullopt.
/// Priority matches getFocusedInputDialog():
///   Exit > message-selector > sandbox > permissions/hook/elicit >
///   cost/idle/ultraplan > onboarding > recs > panels.
[[nodiscard]] std::optional<Element> RouteDialog(
    ReplMode m, const ReplScreenState& s) {
    (void)s;
    // M7: All legacy ReplMode dialog routing is retired.  Dialogs now flow
    // exclusively via the 4-slot DialogQueue with priority
    // Standalone > Modal > Overlay > Bottom.  Any remaining ReplMode-driven
    // chrome (IdleReturn return-from-idle prompt, onboarding, etc.) is
    // rendered by the owning slot in dialog_queue_render.  This stub
    // preserves the file's historical RouteDialog() API so downstream
    // callers (RenderReplScreen) don't need another rewrite.
    switch (m) {
      default: break; }
    return std::nullopt;
}

// =========================================================
// Full layout composition
// =========================================================

/// M1: Composed via the FullscreenLayout slot-system.  The previously-flat
/// top-to-bottom vbox is now
/// slotted:
///   scrollable slot (flex-grow region) =
///     [WelcomeHeader (fresh session)] | Messages | Spinner | Tasks/Teams
///   bottom slot (pinned, fixed-height) =
    ///     PromptInput | Footer
///   modal slot (dbox overlay, bottom-anchored) =
///     RouteDialog() when non-null (with MODAL_TRANSCRIPT_PEEK peek)
///   overlay slot =
///     (reserved — engine wires PermissionRequest here in a future milestone;
///      currently permission flows through RouteDialog as the modal slot,
///      matching how it rendered before.)
///
    /// All existing render functions are PRESERVED (RenderWelcomeHeader,
    /// RenderMessages, RenderSpinner, RenderPromptInput,
/// RouteDialog) — only HOW they are composed changed.  Visual order is
/// preserved: messages scroll above, status/prompt pinned below.
///
/// Fix #1: welcome header atop the list on a fresh session.
    /// Fix #7: StatusLine lives inside the prompt footer.
/// Fix #11: terminal size probed once per frame for adaptive clamping.
[[nodiscard]] Element RenderReplScreen(ReplScreenState& s,
    // Recovery for the current failed model request, owned by screen shortcuts.
    std::function<void()> on_retry,
    // P2 gap api-error-retry: clear-session callback threaded through to
    // RenderMessages for session-expired error cards.
    std::function<void()> on_clear_session,
    // Shared StreamingMarkdown instance for the streaming-text tail row.
    ::loom::ui::StreamingMarkdown* streaming_md,
    // Dismiss callback for API error cards (ESC / q).
    std::function<void()> on_dismiss_error,
    std::function<void()> on_continue) {
    s.messages_store.continue_action_box = {.x_min = 0, .x_max = -1, .y_min = 0, .y_max = -1};
    // Probe terminal size once per frame for adaptive layout (fix #11).
    auto [term_cols, term_rows] = loom::ui::ink_utils::query_terminal_size();
    if (term_cols <= 0) term_cols = 80;
    if (term_rows <= 0) term_rows = 24;
    // Use the actual scrollable area height from the previous frame's
    // render (captured via reflect in ComposeFullscreen).  This accounts
    // for variable chrome (spinner, suggestions, todo panel, teammates
    // strip, status line) that the old term_rows−5 estimate missed.
    // Fall back to term_rows−5 on the first frame (before any render).
    const auto& sb = s.messages_store.scrollable_box;
    const bool have_measured = sb.y_max >= sb.y_min && sb.y_min >= 0;
    // Static layout records the message yframe itself, excluding recovery
    // action rows in the surrounding scrollable slot. Keep that measurement
    // for both scroll bounds and focus positioning on the next frame.
    if (!s.messages_store.rendered_content_height_lines ||
        s.messages_store.virtual_list_active) {
        s.messages_store.viewport_height_lines = have_measured
            ? std::max(1, sb.y_max - sb.y_min + 1)
            : std::max(1, term_rows - 5);
    }
    s.messages_store.viewport_width_cols = term_cols;

    if (s.dialog_store.dialog_queue.has_standalone())
        return dialog_queue_render::RenderStandaloneDialog(s, term_cols, term_rows);
    auto prepared_dialogs = dialog_queue_render::PrepareDialogLayers(
        s, s.is_prompt_input_active, !s.is_tool_animation_active, term_cols, term_rows);
    const bool current_failure = !s.query_running && !s.messages_store.messages.empty()
        && s.messages_store.messages.back().is_model_error
        && s.messages_store.messages.back().is_error;
    const bool emphasize_work = !HasOuterFocusOwner(s.mode)
        && !prepared_dialogs.takes_focus && !current_failure;
    const bool show_work_spinner = emphasize_work && IsToolAnimating(s.task_view_store);

    // Time-based spinner frame: 100 ms per frame (10 fps), matching the
    // reference.  Previously this was a per-render counter, which made the
    // spinner flash fast during stream events (many renders per second).
    // The start time is reset when the spinner transitions Hidden → visible.
    static auto spinner_start_time = std::chrono::steady_clock::now();
    static SpinnerMode last_spinner_mode = SpinnerMode::Hidden;
    if (IsToolAnimating(s.task_view_store)) {
        if (last_spinner_mode == SpinnerMode::Hidden)
            spinner_start_time = std::chrono::steady_clock::now();
        last_spinner_mode = s.task_view_store.spinner_mode;
    } else {
        last_spinner_mode = SpinnerMode::Hidden;
    }
    const int spinner_frame = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - spinner_start_time).count() / 100);

    // ── Unseen-divider computation ────────────────────────────────────
    // When the user has scrolled away from bottom, compute the in-transcript
    // "N new messages" divider anchor + count.  Cleared on repin by
    // ScrollTranscript / on_pill_click (divider_index.reset()).
    if (s.messages_store.divider_index.has_value()) {
        if (s.messages_store.scroll_pinned_to_bottom) {
            // View is at the bottom — the divider is stale (e.g. set before
            // a pin-to-bottom re-engage that didn't go through ScrollTranscript).
            // Clear it so the pill doesn't show for already-visible messages.
            s.messages_store.divider_index.reset();
            s.messages_store.unseen_divider.reset();
            s.messages_store.unseen_message_count = 0;
            s.messages_store.pill_visible = false;
        } else {
            s.messages_store.unseen_divider = ComputeUnseenDivider(s);
            if (s.messages_store.unseen_divider.has_value()) {
                s.messages_store.unseen_message_count = static_cast<int>(s.messages_store.unseen_divider->count);
                s.messages_store.pill_visible = true;
            }
        }
    } else {
        s.messages_store.unseen_divider.reset();
    }

    namespace fl = loom::ui::layout::fullscreen;
    fl::FullscreenLayoutSlots slots;
    slots.term_cols = term_cols;
    slots.term_rows = term_rows;

    // ── scrollable slot (flex-grow region) ───────────────────────────────
    // Builds the same top→bottom order the old flat vbox had for the
    // message transcript area: welcome header (fresh session), messages.
    // Spinner is NOT a scroll row — it lives in the pinned chrome between
    // the messages list and the prompt input (BriefSpinner top margin = 1).
    //
    // Logo/welcome lives INSIDE the scrollable
    // area, not in a pinned header.  The LogoHeader is a
    // thin 1-row bar; the full welcome card (Logo condensed / compact) is
    // rendered inside the VirtualMessageList scrollback.  Putting it here
    // means: (a) blank space appears BELOW messages, not between logo and
    // messages; (b) as messages arrive and pin-to-bottom engages, the logo
    // scrolls out of the visible viewport naturally.
    Elements L;
    Elements scroll_rows; scroll_rows.reserve(2);
    const auto& visible_messages = BuildVisibleMessages(s);

    // ── Welcome / logo card (passed as leading element inside yframe) ───
    // Logo welcome card is the first element
    // inside the VirtualMessageList scrollback.  It is ALWAYS present in
    // the scroll content — when messages overflow the viewport and
    // pin-to-bottom engages, the logo scrolls above the visible window but
    // is reachable by scrolling up.
    //
    // EXCEPTION: when a local command overlay (/skills, /help, etc.) is
    // active with no real conversation messages (s.messages_store.messages empty), we
    // skip the logo so the command output has full viewport space.  This
    // matches the behavior where command overlays are not "real" transcript entries.
    Elements logo_leading;
    const bool has_real_messages = !s.messages_store.messages.empty();
    const bool has_command_overlay = s.active_local_jsx_command;
    if (has_real_messages || !has_command_overlay) {
        logo_leading.push_back(
            RenderWelcomeHeader(s, spinner_frame, term_cols)
            | size(WIDTH, EQUAL, term_cols));
    }

    // ── Messages yframe (logo prepended INSIDE, fills scrollable) ───────
    // render_messages_list_view returns yframe | vscroll_indicator | flex
    // with the logo card as its first scroll child.  Pin-to-bottom logic
    // in messages_list.cppm keeps short content top-aligned (no blank
    // space above messages), and scrolls to bottom only when content
    // exceeds viewport.
    scroll_rows.push_back(RenderMessages(
        visible_messages, s.messages_store.selected_message_idx,
        s.messages_store.viewport_height_lines, s.messages_store.scroll_offset,
        s.messages_store.scroll_pinned_to_bottom, spinner_frame,
        s.messages_store.unseen_divider,
        std::move(logo_leading),
        s.is_brief_mode,
        s.expanded_keys,
        s.is_transcript_mode,
        s.show_all_in_transcript,
        s.disable_chain_compression,
        s.query_running,
        on_retry,
        // P2 gap api-error-retry: thread on_clear_session for session-expired
        // error cards.
        on_clear_session,
        // Thread the shared StreamingMarkdown instance to the messages list.
        streaming_md,
        // Mouse hit-testing tracker for click-to-expand thinking blocks.
        &s.messages_store.row_click_tracker,
        // Store for virtual-list scroll bounds (exact geometry).
        &s.messages_store,
        // Real terminal width for the virtual-path height estimator.
        term_cols,
        // Dismiss callback for API error cards (ESC / q).
        on_dismiss_error, emphasize_work));
    // These error shortcuts belong to the screen, not historical error cards.
    if (!s.query_running && !s.messages_store.messages.empty() &&
        s.messages_store.messages.back().is_model_error &&
        s.messages_store.messages.back().is_error) {
        Elements actions;
        if (on_retry && s.input_text.empty() &&
            s.messages_store.messages.back().can_retry &&
            !s.messages_store.messages.back().session_expired)
            actions.push_back(messages::message_key_hint("R", "Retry request"));
        actions.push_back(messages::message_key_hint("Esc", "Dismiss error"));
        scroll_rows.push_back(hbox(std::move(actions)));
    }
    if (on_continue && s.mode == ReplMode::Normal && s.dialog_store.dialog_queue.empty() &&
        !s.query_running && s.input_text.empty() &&
        !s.messages_store.messages.empty() &&
        s.messages_store.messages.back().can_continue) {
        scroll_rows.push_back(hbox({
            messages::message_action("Continue") | reflect(s.messages_store.continue_action_box),
            text(" "), messages::message_key_hint("C", "Continue from saved progress")}));
    }
    // Spinner lives in the chrome BETWEEN messages list and prompt input
    // (BriefSpinner top margin = 1, NOT a message row inside scroll content).
    Element spinner_chrome = text("");
    if (show_work_spinner)
        spinner_chrome = RenderSpinner(s.task_view_store.spinner_mode, s.task_view_store.spinner_verb,
                                       s.task_view_store.spinner_tip, spinner_frame,
                                       s.task_view_store.estimated_generation_tps);
    // M7.5: Panel views (Tasks/Teams/Help/Settings/About/QuickOpen) are
    // now rendered as modal dialogs via DialogQueue — no longer inlined
    // in the scrollable slot.
    (void)L;
    slots.scrollable = vbox(std::move(scroll_rows));

    // ── Pinned header (non-scroll) ─────────────────────────────────────
    // A thin LogoHeader bar above VirtualMessageList
    // that stays visible even when the welcome card scrolls off.  Without
    // this, pin-to-bottom scrolls the full welcome card out of view and
    // the user sees "logo 也没了" (user report 2026-07-04).
    //
    // We render a compact 1-line logo bar here so the app identity is
    // always visible at the top of the terminal.  The full welcome card
    // still lives inside the scrollable area (first child of yframe).
    {
        namespace logo = loom::ui::logo;
        const std::string model_line = !s.chrome_store.model_display_name.empty()
            ? s.chrome_store.model_display_name
            : s.settings_model;
        slots.header = logo::render_logo_header_bar(model_line, term_cols);
    }

    if (!s.active_local_jsx_command) {
        // ── bottom slot (pinned, fixed-height) ──────────────────────────────
        // Chrome order: [spinner (top margin = 1)] → [suggestions overlay?] →
        //               [prompt input] → [footer]
        //
        // PromptInputFooter structure:
        //   suggestions overlay?  →  prompt input  →  footer (left/right columns)
        //
        // The footer contains StatusLine (optional, user-configurable) +
        // PromptInputFooterLeftSide (mode indicator, tasks, teams, hints) on
        // the left, and bridge/notifications on the right.
        //
        // STABLE HEIGHT: The LeftSide row is always exactly 1 row so scroll
        // content never shifts when hints change.  StatusLine adds a row when
        // present but is conditionally shown only in prompt mode + not short.
        namespace pif = loom::ui::prompt::footer;

        // Build StatusLine options (user-configurable command-driven status).
        //
        // StatusLine:
        //   - Configured by settings.statusLine, rendered only in prompt mode
        //     and hidden in short fullscreen layouts
        //   - content comes from executing the user's shell command
        //   - In fullscreen, reserves a row even while loading (stable height)
        //   - Text may contain ANSI escape codes for coloring
        const bool is_fullscreen = loom::utils::is_fullscreen_enabled();
        const bool is_short = is_fullscreen && term_rows < 24;
        const bool status_line_configured =
            s.status_line_enabled && !s.status_line_command.empty();

        // P0-6 builtin statusline: populate fallback data from screen state.
        // When the user's command returns empty (or no command configured),
        // RenderStatusLine() uses this to show folder/git/model/token info.
        pif::BuiltinStatusLineData builtin_data;
        builtin_data.cwd = s.cwd;
        builtin_data.git_branch = s.chrome_store.git_branch;
        // Prefer model_display_name (human-friendly), fall back to model_name.
        builtin_data.model_name = !s.chrome_store.model_display_name.empty()
            ? s.chrome_store.model_display_name
            : s.chrome_store.status_bar.model_name;
        builtin_data.input_tokens = s.chrome_store.status_bar.input_tokens;
        builtin_data.output_tokens = s.chrome_store.status_bar.output_tokens;
        builtin_data.context_token_count = s.chrome_store.status_bar.context_token_count;
        builtin_data.cost_usd = s.chrome_store.status_bar.cost_usd;
        builtin_data.context_window_size = s.chrome_store.status_bar.context_window_size;
        builtin_data.task_count = s.task_view_store.teammate_count;
        builtin_data.agent_name = s.chrome_store.status_bar.agent_name.value_or("");
        builtin_data.segments = s.status_bar_segments;
        builtin_data.status_bar_enabled = s.status_bar_enabled;
        builtin_data.mouse_mode_label = s.chrome_store.status_bar.mouse_mode_label;

        pif::StatusLineOptions status_line_opts;
        status_line_opts.columns = std::max(1, term_cols - 4);
        status_line_opts.content = s.status_line_text;
        status_line_opts.builtin = std::move(builtin_data);
        // Show statusline when:
        //   (a) user has a configured statusLine command, OR
        //   (b) builtin data is available (always, since cwd is set)
        // Same mode guards apply: prompt mode, not bash, not short terminal.
        const bool in_prompt_mode =
            s.prompt_store.input_mode == InputMode::Normal &&
            !effective_is_bash(s) &&
            !is_short;
        status_line_opts.should_display = in_prompt_mode &&
            (status_line_configured || !status_line_opts.content.empty() ||
             status_line_opts.builtin.has_value());
        status_line_opts.is_fullscreen = is_fullscreen;
        status_line_opts.padding_x = s.status_line_padding;

        // Map InputMode to footer PromptInputMode.  Text-derived mode takes
        // precedence over state-toggle (text-derived semantics — see
        // effective_is_bash()).
        // NOTE: Both InputMode and pif::PromptInputMode are now the same
        // unified type (loom::ui::common::PromptInputMode), so this is a
        // direct assignment with bash-detection override.
        pif::PromptInputMode footer_mode =
            static_cast<pif::PromptInputMode>(s.prompt_store.input_mode);
        if (effective_is_bash(s)) {
            footer_mode = pif::PromptInputMode::Bash;
        } else if (footer_mode == pif::PromptInputMode::Bash) {
            // State says bash but text doesn't start with '!' — normalize
            // to Normal (consistent with old switch default behavior).
            footer_mode = pif::PromptInputMode::Normal;
        }
        // ── Assemble the bottom slot ──
        // Chrome order: [top-margin gap] → [spinner (top margin = 1)] →
        //               [suggestions overlay?] → [prompt input] → [footer]
        //
        // Top margin = (brief_owns_gap ? 0 : 1) on the
        // outermost container.  In non-brief mode this is a 1-row gap between the
        // scrollback area and the top border of the input box.  We emulate with a
        // leading text("") row.
        L.reserve(5);
        L.push_back(text(""));   // top margin = 1
        if (show_work_spinner) {
            L.push_back(hbox({spinner_chrome, filler()}) | flex_shrink);
        }
        // Activity is inserted here after fixed chrome has been measured.
        const auto activity_position = L.size();
        // PromptInputFooter: LeftSide carries mode/tasks/teams via
        // ModeIndicatorOptions; StatusLine is its own nested struct.
        pif::LeftSideOptions left_opts;
        // Pasting hint is visible for 100ms after the last paste batch
        // (PASTE_COMPLETION_TIMEOUT_MS = 100).
        if (s.pasting_since) {
            const auto age = std::chrono::steady_clock::now() - *s.pasting_since;
            if (age <= std::chrono::milliseconds(100)) {
                left_opts.is_pasting = true;
            } else {
                s.pasting_since.reset();
            }
        }
        // Idle Ctrl+C double-press footer ("Press <key> again to exit"),
        // projected from the app-layer ExitHandler. Expiry is event-driven
        // exactly like the pasting hint (no ticker).
        // `Press {exitMessage.key} again to exit` — with key "Ctrl-C"
        // RenderLeftSide composes the exact string.
        if (s.exit_message_until) {
            if (std::chrono::steady_clock::now() <= *s.exit_message_until) {
                left_opts.exit_message_show = true;
                left_opts.exit_message_key = s.exit_message_key;
            } else {
                s.exit_message_until.reset();
            }
        }
        left_opts.mode_indicator.mode                 = footer_mode;
        left_opts.mode_indicator.permission_mode      = s.permission_mode;
        left_opts.mode_indicator.background_task_count = s.task_view_store.background_task_count;
        left_opts.mode_indicator.teammate_count        = s.task_view_store.teammate_count;
        left_opts.mode_indicator.teams_selected        = s.task_view_store.teams_footer_selected;
        // Show "esc to interrupt" in the footer while a query is running.
        left_opts.mode_indicator.is_loading            = IsToolAnimating(s.task_view_store);
        // Transcript/brief mode pills (is_transcript_mode + is_brief_mode).
        left_opts.mode_indicator.is_transcript_mode    = s.is_transcript_mode;
        left_opts.mode_indicator.is_brief_mode         = s.is_brief_mode;
        if (status_line_opts.should_display) {
            left_opts.mode_indicator.show_hint = false;
        }
        pif::FooterOptions footer_opts;
        footer_opts.status_line = std::move(status_line_opts);
        footer_opts.left_side   = std::move(left_opts);
        footer_opts.is_fullscreen = is_fullscreen;
        footer_opts.is_narrow = term_cols < 80;

        // Bridge status pill (BridgeStatusIndicator + getBridgeStatus).
        if (s.bridge_enabled) {
            namespace bs = loom::ui::prompt::footer;
            bs::BridgeOptions bopt;
            // Priority: reconnecting > connected(session) > connected,
            // mirroring getBridgeStatus (failed is surfaced via notification).
            if (s.bridge_reconnecting) {
                bopt.status = bs::BridgeStatus::Reconnecting;
            } else if (s.bridge_connected || s.bridge_session_active) {
                bopt.status = bs::BridgeStatus::Connected;
            } else {
                bopt.status = bs::BridgeStatus::Disconnected;
            }
            bopt.explicit_remote = s.bridge_explicit_remote;
            bopt.selected = s.bridge_selected;
            footer_opts.bridge = std::move(bopt);
        }

        // P1 Footer notifications — populate from ReplScreenState
        {
            auto& nd = footer_opts.notification;
            nd.api_key_status = s.api_key_status;
            nd.is_remote = s.is_remote_session;
            nd.debug_mode = s.debug_mode;
            nd.verbose = s.verbose;
            nd.token_usage = s.chrome_store.status_bar.context_token_count;
            nd.is_overage_mode = s.chrome_store.show_overage_credit_upsell;
            nd.ide.connected = s.ide_connected;
            nd.ide.file_path = s.ide_file_path;
            nd.ide.selected_lines = s.ide_selected_lines;
            nd.dynamic_text = s.footer_dynamic_text;
            nd.dynamic_color = s.footer_dynamic_color;

            // P1: Advance the notification queue carousel — this is the
            // timer-based rotation through queued items.  Called here
            // (event-driven, on each render) rather than a constant ticker.
            // processQueue()
            namespace pif = loom::ui::prompt::footer;
            (void)pif::QueueAdvance(s.footer_notification_queue);
            nd.queue = s.footer_notification_queue;
        }

        // M4 faithful: outer chrome.
        //   * Clipboard image hint — stays nullopt until the engine wires up
        //     platform clipboard-image detection; no visual regression while
        //     empty.
        // NOTE: No brand pill is rendered in the footer.
        // Branding is rendered by CondensedLogo only
        // in the top header.
        auto footer = pif::RenderPromptInputFooter(footer_opts);
        auto measure = [](const Element& element) {
            element->ComputeRequirement();
            return element->requirement().min_y;
        };
        int fixed_rows = measure(footer);
        for (const auto& element : L) fixed_rows += measure(element);
        const int header_rows = slots.header ? measure(slots.header) : 0;
        // Reserve the maximum one row used by the scroll spacer/sticky
        // header. A clicked prompt may use none, but overlays can restore it.
        const int scroll_chrome_rows = 1;
        const int bottom_limit = std::max(4, term_rows / 2);
        // Suggestions share the pinned slot with the editor. Reserve its
        // minimum one-row viewport before choosing the popup window size;
        // otherwise five overlay rows can clip the caret/footer at 24 rows.
        bool have_popup = false;
        if (!s.autocomplete_suggestions.empty()) {
            const auto minimum_prompt = RenderPromptInput(s, term_cols, 1);
            const int popup_overhead = is_fullscreen ? 2 : 0;
            const int item_capacity = std::max(0,
                bottom_limit - fixed_rows - measure(minimum_prompt) - popup_overhead);
            auto popup = RenderPromptSuggestions(s, term_cols,
                /*is_overlay=*/is_fullscreen, /*term_rows=*/term_rows,
                /*max_visible_items=*/item_capacity);
            if (popup) {
                fixed_rows += measure(popup);
                L.push_back(std::move(popup));
                have_popup = true;
            }
        }
        int prompt_rows = std::clamp((term_rows - 6) / 3, 1, 12);
        auto prompt = RenderPromptInput(s, term_cols, prompt_rows);
        int prompt_height = measure(prompt);
        const bool multiline = prompt_height > 3;
        const int transcript_floor = std::min(multiline || have_popup ? 8 : 12, term_rows / 2);
        const int fixed_limit = std::min(bottom_limit,
            std::max(0, term_rows - header_rows - scroll_chrome_rows - transcript_floor));
        // Reduce the held editor's viewport before activity allocation; input
        // bytes and caret/selection ownership remain with the same component.
        while (prompt_rows > 1 && fixed_rows + prompt_height > fixed_limit) {
            --prompt_rows;
            prompt = RenderPromptInput(s, term_cols, prompt_rows);
            prompt_height = measure(prompt);
        }
        fixed_rows += prompt_height;
        const int activity_budget = std::max(0, std::min({
            std::clamp(term_rows / 6, 1, 5),
            bottom_limit - fixed_rows,
            term_rows - header_rows - scroll_chrome_rows - transcript_floor - fixed_rows}));
        const int todo_need = s.todo_store.items.empty() ? 0
            : static_cast<int>(s.todo_store.items.size()) + 1;
        const int team_need = static_cast<int>(s.task_view_store.live_teammates.size());
        Elements activity;
        if (activity_budget == 1 && todo_need > 0 && team_need > 0) {
            const auto done = std::ranges::count_if(s.todo_store.items, [](const auto& item) {
                return item.status == todos::TodoDisplayItem::Status::Completed;
            });
            const auto summary = term_cols < 70
                ? std::format("Tasks {}/{} +{} · Teams {} +{}", done,
                    s.todo_store.items.size(), s.todo_store.items.size(), team_need, team_need)
                : std::format("Tasks {}/{} done (+{} hidden) · Teams {} (+{} hidden)",
                    done, s.todo_store.items.size(), s.todo_store.items.size(), team_need, team_need);
            activity.push_back(text(teams::live::one_line_tail(summary,
                static_cast<std::size_t>(term_cols)))
                | color(loom::ui::design::theme::current_theme().palette->muted)
                | size(WIDTH, EQUAL, term_cols));
        } else if (activity_budget > 0) {
            int todo_rows = todo_need > 0 ? activity_budget : 0;
            int team_rows = team_need > 0 ? activity_budget : 0;
            if (todo_need > 0 && team_need > 0) {
                todo_rows = (activity_budget + 1) / 2;
                team_rows = activity_budget - todo_rows;
                if (todo_need < todo_rows) {
                    team_rows += todo_rows - todo_need;
                    todo_rows = todo_need;
                }
                if (team_need < team_rows) {
                    todo_rows = std::min(todo_need, todo_rows + team_rows - team_need);
                    team_rows = team_need;
                }
            }
            if (todo_rows > 0)
                activity.push_back(todos::RenderTodoPanel(s.todo_store.items, term_cols, todo_rows));
            if (team_rows > 0)
                activity.push_back(teams::live::RenderLiveTeammateStrip(
                    s.task_view_store.live_teammates, term_cols, team_rows));
        }
        L.insert(L.begin() + static_cast<std::ptrdiff_t>(activity_position),
                 activity.begin(), activity.end());
        L.push_back(std::move(prompt));
        L.push_back(std::move(footer));

        slots.bottom = vbox(std::move(L)) | flex_shrink;
    }

    // Legacy RouteDialog path — only used when dialog_queue has no
    // overlay/bottom/modal slots.  Eventually this will be phased out
    // in favour of the queue for all dialogs.

    // ── M1 FullscreenLayout: 3-state sticky prompt chrome ─────────────
    // 3-state discriminant,
    // collapsed-padding resolution, header-prompt guard.
    slots.sticky_prompt         = s.messages_store.sticky_prompt;
    slots.sticky_clicked        = s.messages_store.sticky_prompt_clicked;
    slots.hide_sticky           = false;
    slots.pill_visible          = s.messages_store.pill_visible;
    slots.hide_pill             = false;
    slots.new_message_count     = s.messages_store.unseen_message_count;
    slots.pill_hit_box          = &s.messages_store.new_messages_pill_box;
    slots.scrollable_box        = &s.messages_store.scrollable_box;

    // Inline "copied N chars to clipboard" toast (bottom_float slot).
    // Set by AppAdapter::Render from TextSelection state.
    if (s.copy_toast_text) {
        slots.bottom_float = hbox({
            text(*s.copy_toast_text) | dim,
            text("  ")  // right margin
        });
    }

    // on_sticky_click: the click handler sets the 'clicked' sentinel
    // (a stable state update that reacts before the scroll side-effects
    // fire).  We match that order in C++: (1) flip sticky_prompt_clicked
    // to hide the header + keep collapsed-padding=true; (2) compute the
    // delta between the prompt's visual line and current scroll_top and
    // ask ScrollTranscript to jump there.
    //
    // Captures: `&s` is a ReplScreenState& whose lifetime is bound to the
    // outer `std::shared_ptr<ReplScreenState>` in MakeReplScreen; it is
    // stable across renders.  The callback is only invoked from within
    // FTXUI event dispatch (same thread), so no data races.
    slots.on_sticky_click = [&s](const fl::StickyPrompt& sp) {
        s.messages_store.sticky_prompt_clicked = true;
        // Jump so the target visual line is at the TOP of the viewport.
        // scroll_target_row is measured from scroll_top=0 (content
        // coordinates); ScrollTranscript(delta) is relative — so delta =
        // target - current.  Clamp against viewport_rows to avoid
        // overshooting below min-scroll.
        int viewport = std::max(1, s.messages_store.viewport_height_lines);
        int total;
        if (s.messages_store.virtual_list_active) {
            namespace vl = loom::ui::messages::virtual_list;
            total = s.messages_store.virtual_jh.total();
        } else {
            total = s.messages_store.rendered_content_height_lines
                ? *s.messages_store.rendered_content_height_lines
                : EstimateTranscriptRows(BuildVisibleMessages(s),
                    s.messages_store.viewport_width_cols);
        }
        int max_top = std::max(0, total - viewport);
        // When pinned to bottom, scroll_offset is 0 but the view is at
        // max_top.  Use the effective position so the delta is correct.
        const bool pinned = s.messages_store.scroll_pinned_to_bottom;
        int current = pinned
            ? max_top
            : std::max(0, s.messages_store.scroll_offset);
        int delta   = static_cast<int>(sp.scroll_target_row) - current;
        if (delta != 0) {
            // We need to call ScrollTranscript which takes
            // shared_ptr<ReplScreenState>.  Here we only have a bare ref;
            // but this callback is dispatched from FTXUI's event loop from
            // within the outer MakeReplScreen Component's OnEvent chain
            // which owns the shared_ptr.  For a purely visual change
            // (clicking the header is a scroll, not engine-state mutation),
            // a direct offset mutation achieves the same effect without
            // requiring the shared_ptr here.
            int old_top = std::clamp(current, 0, max_top);
            int target  = std::clamp(old_top + delta, 0, max_top);
            if (target != old_top) {
                const bool was_pinned = s.messages_store.scroll_pinned_to_bottom;
                s.messages_store.scroll_offset = target;
                s.messages_store.scroll_pinned_to_bottom = (target >= max_top);
                // RFC 0008: release the persistent bottom-anchor when the
                // user scrolls away from bottom.
                if (was_pinned && !s.messages_store.scroll_pinned_to_bottom) {
                    s.messages_store.force_bottom_after_mode_switch = false;
                }
                if (s.messages_store.virtual_list_state) {
                    namespace vl = loom::ui::messages::virtual_list;
                    s.messages_store.virtual_list_state->scroll_top = target;
                    vl::update_sticky_after_scroll(*s.messages_store.virtual_list_state,
                                                    old_top);
                }
            }
        }
        // Note: after the click, the sticky prompt stays in the 'clicked'
        // state until the NEXT scroll event re-emits a fresh {text, scroll_target_row}
        // from the sticky tracker.  Any movement (wheel, PageUp,
        // click-to-select)
        // that moves the viewport will write a new sticky_prompt and
        // clear sticky_prompt_clicked.  We therefore do NOT clear the
        // flag ourselves here.
    };

    Element base = fl::ComposeFullscreen(std::move(slots));
    auto dlg = RouteDialog(s.mode, s);
    if (dlg) base = dbox({
        std::move(base) | dim,
        vbox({ filler(),
               hbox({ filler(), std::move(*dlg) | flex_shrink, filler() })
               | flex_shrink, filler() }) | flex });

    // M7: Layer Bottom + Overlay + Modal dialogs from the dialog_queue.
    // ModalContext provides actual
    // terminal dimensions (cols-4, rows-PEEK-1) to modal dialogs.  Pass real
    // term_cols/term_rows here instead of the old hardcoded 120x40 so dialog
    // renderers get accurate viewport geometry.
    return dialog_queue_render::LayerAllDialogs(std::move(base), std::move(prepared_dialogs));
}

}  // namespace loom::ui::repl_screen
