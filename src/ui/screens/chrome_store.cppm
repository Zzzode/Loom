// chrome_store.cppm — RFC 0002 F3 store: chrome / welcome-header /
// status-bar projection state, sharded out of ReplScreenState.
//
// Threading: UI-thread-affined plain data. No locks, no worker threads —
// the one staged-queue mutex that lived in repl_state.cppm
// (pending_at_mention_mutex) moves to the AppImpl composition layer, not
// into any store. Cross-store reads go through selectors wired by the
// composition root, never a direct field reach-up.
//
// Import discipline (enforced by `graph_check.py --store-lint`): a store
// module's cc.ui.* imports target only areas ranked below screens
// (UI9_RANK < 10). Every field here is a std-only type (strings, vectors,
// bools and the std-only StatusBarData projection), so this store imports
// no cc.ui.* area at all.
module;

export module loom.ui.screens.chrome_store;

import std;

export namespace cc::ui::repl_screen {

/// Status bar projection.  Mirrors TS REPL top status line.
/// Moved here from repl_state.cppm in RFC 0002 F3 (ChromeStore shard).
struct StatusBarData {
    std::string model_name;
    std::optional<std::string> session_name, agent_name;
    std::optional<double> cost_usd;
    int input_tokens = 0, output_tokens = 0, context_token_count = 0;
    bool is_fast_mode = false, is_auto_mode = false, is_brief_mode = false;
    std::optional<std::string> effort_level;
    int swarm_session_count = 0;
    bool bridge_connected = false;
    std::optional<std::string> current_path;
};

/// RFC 0002 F3 store — chrome / welcome-header / status-bar projection
/// state, sharded out of ReplScreenState. Homed in cc.ui.screens (rank 10):
/// every field is a std-only type, so no cross-area edge is created at all.
/// The StickyPrompt chrome-layout state named in the shard inventory was
/// already classified into MessagesStore (scroll/chrome state) when that
/// store landed, so it is not duplicated here. UI-thread-affined plain
/// data — see the file header for the threading and import rules.
struct ChromeStore {
    /// Status bar projection (TS REPL top status line).
    StatusBarData status_bar;

    // ── Welcome-header data (shown when messages is empty) ──────────────
    std::string app_version = "0.0.0";
    std::string model_display_name;
    // TS LogoV2/CondensedLogo row 2 separator billing_type token (e.g.
    // "API Usage Billing" / "Team Seat" / "Rate Limited").  Empty = row 2
    // shows only the model name without " · <billing>" suffix.
    std::string billing_type;
    // P0-6 builtin statusline: detected git branch for cwd (empty = not a git
    // repo or detection failed).  Populated by AppAdapter from
    // cc::utils::git::get_branch(), cached per-cwd-change to avoid spawning
    // `git` on every render tick.
    std::string git_branch;
    // M2: oauthAccount.displayName analogue — drives formatWelcomeMessage
    // ("Welcome back {user}!" vs "Welcome back!").  Empty for new users.
    std::string user_display_name;

    // ── Feed content (full-logo horizontal mode right column) ───────────
    // P0 Gap logov2-render-modes-missing: pre-rendered feed content for the
    // full-logo horizontal mode's right column. Populated by the engine layer
    // from session storage (recent activity) and changelog parser (what's
    // new). Empty vectors fall back to placeholders defined in RenderWelcomeHeader.
    std::vector<std::string> recent_activity_lines;
    std::vector<std::string> changelog_lines;
    // TS REF: LogoV2.tsx L56 shouldShowProjectOnboarding() — when true, the
    // horizontal-layout feed column shows [ProjectOnboarding, RecentActivity]
    // instead of [RecentActivity, What'sNew].  Also prevents condensed mode
    // (TS isCondensedMode = !hasReleaseNotes && !showOnboarding && !forceFull).
    bool show_onboarding = false;
    // TS REF: LogoV2.tsx L70 useShowGuestPassesUpsell() — when true and no
    // onboarding, feed shows [RecentActivity, GuestPasses] instead of default.
    bool show_guest_passes_upsell = false;
    // TS REF: LogoV2.tsx L71 useShowOverageCreditUpsell() — when true and no
    // onboarding/guest-passes, feed shows [RecentActivity, OverageCredit].
    bool show_overage_credit_upsell = false;
};

}  // namespace cc::ui
