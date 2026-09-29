// prompt_store.cppm — RFC 0002 F3 store: prompt-input state (input mode,
// stashed prompt, placeholder-cascade inputs, teammate prefix color),
// sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's cc.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). foundation (2) is downward-legal, so the
// PromptInputMode field type recreates no up-edge.
module;

#include <ftxui/screen/color.hpp>

export module cc.ui.screens.prompt_store;

import std;

// Genuinely used (ImageBlock fields in StashedPrompt below), but only via
// leading-`::` qualified names `::cc::core::ImageBlock`, which the
// dead-import detector's prefix-chain cannot resolve — same blind spot as
// messages_store.cppm's identical import, hence the keep-import marker.
import cc.types.types;                  // arch-check: keep-import
import cc.ui.foundation.ui_types;       // cc::ui::common::PromptInputMode

export namespace cc::ui::repl_screen {

// GAP 2: stashed-prompt-restore-logic-missing
// TS REF: src/screens/REPL.tsx L1373-1377 — stashedPrompt state:
//   {text, cursorOffset, pastedContents}.  When the user sends a message
//   while a background agent is running (or when permission interrupts),
//   the current input is stashed and can be restored after the request
//   completes.  Restore at:
//     - TS L3251-3255 (after local-jsx result returns)
//     - TS L3344-3348 (on submit when not slash-command)
//     - TS L3527-3531 (after handlePromptSubmit for slash/loading)
//   The stash notice (PromptInputStashNotice.tsx) renders
//   "{figures.pointerSmall} Stashed (auto-restores after submit)" when
//   hasStash is true.
// TS REF: src/screens/REPL.tsx L1373-1377 — stashedPrompt state:
//   {text, cursorOffset, pastedContents}.  pastedContents carries the
//   image/text paste records so that [Image #N] / [...Truncated text #N]
//   refs in the stashed text resolve correctly after restore.
// Moved here from repl_state.cppm in RFC 0002 F3 (PromptStore shard).
struct StashedPrompt {
    std::string text;
    std::size_t cursor_offset = std::string::npos;
    // TS REF: pastedContents: Record<number, PastedContent> — image pastes.
    std::unordered_map<int, ::cc::core::ImageBlock> pasted_images;
    // TS REF: pastedContents also holds text-type entries for truncated
    // text pastes (inputPaste.ts maybeTruncateInput → type:'text').
    std::unordered_map<int, std::string> pasted_texts;
};

/// RFC 0002 F3 store — prompt-input state (input mode, stashed prompt,
/// placeholder-cascade inputs, teammate prefix color), sharded out of
/// ReplScreenState. Homed in cc.ui.screens (rank 10): the concrete
/// cross-area field type (PromptInputMode) lives in foundation (rank 2),
/// so a by-value field recreates no up-edge. UI-thread-affined plain data
/// — see the file header for the threading and import rules.
struct PromptStore {
    /// Unified canonical input mode (TS PromptInputMode). The vim modes
    /// (VimInsert/VimNormal/VimVisual) are values of this unified enum, so
    /// the old per-mode vim state folded into this single field.
    cc::ui::common::PromptInputMode input_mode =
        cc::ui::common::PromptInputMode::Normal;

    // GAP 2: stashed-prompt-restore-logic-missing — see StashedPrompt above.
    std::optional<StashedPrompt> stashed_prompt;

    // TS-style contextual placeholder rather than a generic default.
    // NOTE: This is the FALLBACK only.  The actual displayed placeholder is
    // computed dynamically by ComputePlaceholder() at render time from the
    // cascade (teammate hint > queue hint > onboarding example > AI suggestion
    // override).  This string is used only when all cascade conditions fail
    // AND the caller explicitly wants a static default (e.g. standalone
    // TextInputImpl usage outside the REPL).
    std::string input_placeholder = "Try \"write a test\", \"/help\", or ask anything...";

    // ── TS usePromptInputPlaceholder cascade state ──────────────────────
    // Viewing agent/teammate name.  When set and input is empty, the
    // placeholder becomes "Message @{name}..." (TS REF: usePromptInputPlaceholder.ts
    // viewingAgentName branch).  Truncated to 20 chars in ComputePlaceholder().
    std::optional<std::string> viewing_agent_name;
    // Number of user submissions (messages sent).  Drives the onboarding
    // example placeholder: shown only when submit_count < 1 (TS REF:
    // usePromptInputPlaceholder.ts submitCount < 1 guard).
    int submit_count = 0;
    // How many times the "Press up to edit queued messages" hint has been
    // shown.  Capped at 3 (NUM_TIMES_QUEUE_HINT_SHOWN in TS).  Incremented
    // by the renderer each time the hint is displayed.
    int queued_command_hint_shown_count = 0;
    // Whether prompt suggestions (AI next-action hints) are enabled.
    // Maps to TS AppState.promptSuggestionEnabled.
    bool prompt_suggestion_enabled = true;
    // True when the command queue holds user-editable pending commands
    // (TS REF: isQueuedCommandEditable check).  Populated by the engine
    // from the command queue state.
    bool has_editable_queued_commands = false;
    // TS AGENT_COLOR_TO_THEME_COLOR teammate prefix color.  Empty = use
    // palette.text (the default prompt prefix color).  Populated from the
    // engine's active-teammate lookup.  Rendered as the prefix glyph's
    // foreground in RenderPromptInput (replaces the old plain white).
    std::optional<ftxui::Color> teammate_prefix_color;
};

}  // namespace cc::ui
