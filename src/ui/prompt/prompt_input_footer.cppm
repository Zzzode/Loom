/// =========================================================================
/// @file prompt_input_footer.cppm
/// @brief Full-width status row and left/right prompt controls below input.
///
/// MODULE:   loom.ui.prompt.prompt_input_footer
/// LICENCE:  Exported.  Imported by repl_screen.cppm (bottom slot assembly).
///
/// Layout structure:
///   StatusLine                              // optional, full-width row
///   PromptInputFooterLeftSide | Notifications / BridgeStatusIndicator
///   Narrow layouts stack the control columns below the status row.
///
/// Left side contents:
///   - Exit message (dim: "Press <key> again to exit")
///   - Pasting indicator (dim: "Pasting text…")
///   - HistorySearchInput (when isSearching)
///   - Vim INSERT badge (dim: "-- INSERT --")
///   - ModeIndicator (permission mode, tasks, teams, PR, hints)
///
/// STABLE HEIGHT: In fullscreen the bottom slot is fixed-height, so every
///   row here is a row stolen from the ScrollBox.  The footer must have a
///   STABLE height so it never grows/shrinks and shifts scroll content.
/// =========================================================================

module;

#include <cstring>
#include <cmath>
#include <cstdio>

#include <ftxui/dom/elements.hpp>

export module loom.ui.prompt.prompt_input_footer;

import std;

// P0-1: palette tokens (bash_border / prompt_border color resolution).
import loom.ui.foundation.design_tokens;
// P0-1: active theme provider for bash-border consistency (BUG-3 fix).
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
// Unified canonical PromptInputMode enum (replaces local 5-value definition).
import loom.ui.foundation.ui_types;

export namespace loom::ui::prompt::footer {

using namespace ftxui;

// ============================================================
// Enums
// ============================================================

/// Prompt input mode — unified canonical enum from loom::ui::common.
/// Previously this file defined a local 5-value PromptInputMode:
///   {Prompt, Bash, SlashCommand, HistorySearch, PlanMode}
/// Old→new mapping:
///   PromptInputMode::Prompt → PromptInputMode::Normal
/// All other values (Bash, SlashCommand, HistorySearch, PlanMode)
/// are identical in the unified definition.
using loom::ui::common::PromptInputMode;

/// Vim mode — canonical enum from loom::ui::common (ui_types.cppm).
/// Previously this file defined a local 4-value enum { Normal, Insert, Visual, None }.
/// "None" (vim disabled) is now expressed as std::optional<VimMode>{nullopt}.
using loom::ui::common::VimMode;

/// Permission mode.  Canonical definition lives in loom::ui::common (ui_types.cppm).
using loom::ui::common::PermissionMode;

// ============================================================
// Permission mode helpers
// ============================================================

/// Whether the mode is the default (no special badge shown).
[[nodiscard]] inline bool IsDefaultMode(PermissionMode m) {
    return m == PermissionMode::Default;
}

/// Compact text symbols supplement the permission mode label.
[[nodiscard]] inline std::string_view PermissionModeSymbol(PermissionMode m) {
    namespace figs = loom::ui::design::figures;
    switch (m) {
        case PermissionMode::Default:    return "";
        case PermissionMode::AcceptEdits: return figs::kPointerSmall;
        case PermissionMode::AcceptAll:   return figs::kCircleFilled;
        case PermissionMode::Plan:        return figs::kEllipsis;
    }
    return "";
}

/// Human-readable title for the permission mode.
[[nodiscard]] inline std::string_view PermissionModeTitle(PermissionMode m) {
    switch (m) {
        case PermissionMode::Default:     return "default";
        case PermissionMode::AcceptEdits: return "Edit";
        case PermissionMode::AcceptAll:   return "All";
        case PermissionMode::Plan:        return "Plan";
    }
    return "default";
}

/// Color for the permission mode badge text.
[[nodiscard]] inline Color GetModeColor(PermissionMode m) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (m) {
        case PermissionMode::Default:     return pal.muted;
        case PermissionMode::AcceptEdits: return pal.auto_accept;
        case PermissionMode::AcceptAll:   return pal.danger;
        case PermissionMode::Plan:        return pal.plan_mode;
    }
    return pal.muted;
}

/// Cycle to the next permission mode.
/// Order for non-ant users: Default → AcceptEdits → Plan → Default.
/// AcceptAll is reachable only from Plan when bypass is available (not yet
/// wired in CPP — included for forward compatibility).
[[nodiscard]] inline PermissionMode GetNextPermissionMode(PermissionMode current) {
    switch (current) {
        case PermissionMode::Default:     return PermissionMode::AcceptEdits;
        case PermissionMode::AcceptEdits: return PermissionMode::Plan;
        case PermissionMode::Plan:        return PermissionMode::Default;
        case PermissionMode::AcceptAll:   return PermissionMode::Default;
    }
    return PermissionMode::Default;
}

// ============================================================
// ModeIndicator
// ============================================================
// Shows permission mode badge, background tasks pill, teams, PR badge,
// and keyboard hints.

struct ModeIndicatorOptions {
    PromptInputMode mode = PromptInputMode::Normal;
    PermissionMode permission_mode = PermissionMode::Default;
    bool show_hint = true;
    bool is_loading = false;
    bool tasks_selected = false;
    bool teams_selected = false;
    bool tmux_selected = false;
    std::optional<int> teammate_footer_index;

    // Task / team counts
    int background_task_count = 0;
    int teammate_count = 0;

    // Hints
    bool show_esc_hint = false;   // "esc to interrupt" when loading
    std::string esc_shortcut = "esc";
    std::string todos_shortcut = "ctrl+t";

    // Remote / session
    bool is_remote_mode = false;

    // Transcript / brief view modes.
    // Shown as pills in the mode indicator row so the user knows which filter
    // is currently active on the message list.
    bool is_transcript_mode = false;
    bool is_brief_mode = false;
};

/// Render the ModeIndicator — permission mode + tasks pill + teams + hints.
///
/// Structure:
///   mode part (fixed-width) · tasks part (fixed-width) · parts (truncate)
///
/// STABLE HEIGHT: always renders exactly 1 row (may show a space when empty
///   in fullscreen mode).
[[nodiscard]] inline Element RenderModeIndicator(const ModeIndicatorOptions& opts) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    // Early return for bash mode ("! for bash mode", colored with
    // bash_border token for consistency with prompt prefix + transcript
    // user-bash-input bubble — fixes BUG-3: 3-sites bash-border divergence).
    if (opts.mode == PromptInputMode::Bash) {
        using namespace loom::ui::design;
        const auto& pal = *theme::current_theme().palette;
        return hbox({ text("! for bash mode") | color(pal.bash_border) })
             | size(HEIGHT, EQUAL, 1);
    }

    Elements left_parts;   // fixed-width items (mode + tasks pill + teams)
    Elements hint_parts;   // truncatable hints

    bool has_active_mode = !IsDefaultMode(opts.permission_mode) && !opts.is_remote_mode;
    bool has_background_tasks = opts.background_task_count > 0;
    bool has_teams = opts.teammate_count > 0;

    // Count primary items for hint visibility logic
    int primary_item_count = (has_active_mode ? 1 : 0)
                           + (has_background_tasks ? 1 : 0)
                           + (has_teams ? 1 : 0);

    // ── Mode part (permission mode badge) ───────────────────────────────
    if (has_active_mode) {
        Elements mode_el = {
            text(std::string{PermissionModeSymbol(opts.permission_mode)})
                | color(GetModeColor(opts.permission_mode)),
            text(" "),
            text(std::string{PermissionModeTitle(opts.permission_mode)})
                | color(GetModeColor(opts.permission_mode)),
        };
        // Show cycle hint only when few primary items
        if (primary_item_count < 2 && opts.show_hint) {
            mode_el.push_back(text(" on") | dim);
            mode_el.push_back(text(" (") | dim);
            mode_el.push_back(text("shift+tab") | dim | bold);
            mode_el.push_back(text(" to cycle)") | dim);
        }
        left_parts.push_back(hbox(std::move(mode_el)));
    }

    // ── Tasks pill ──────────────────────────────────────────────────────
    if (has_background_tasks) {
        std::string label = std::format("{} task{}",
            opts.background_task_count,
            opts.background_task_count == 1 ? "" : "s");
        Color c = pal.info;
        Element pill = hbox({
            text(label) | color(c) | (opts.tasks_selected ? inverted : nothing),
        });
        if (opts.show_hint && !has_teams) {
            // "↓ to manage" hint when tasks present and no teams
            pill = hbox({
                std::move(pill),
                text(" ↓") | dim,
                text(" manage") | dim,
            });
        }
        left_parts.push_back(std::move(pill));
    }

    // ── Teams pill ──────────────────────────────────────────────────────
    if (has_teams) {
        std::string label = std::format("{} Team Agent{}",
            opts.teammate_count,
            opts.teammate_count == 1 ? "" : "s");
        Color c = pal.primary;
        Element pill = hbox({
            text(label) | color(c) | (opts.teams_selected ? inverted : nothing),
        });
        left_parts.push_back(std::move(pill));
    }

    // ── Transcript mode pill ─────────────────────────────────────────────
    //    Shown when user pressed Ctrl+O to enter detailed transcript view.
    if (opts.is_transcript_mode) {
        using namespace loom::ui::design;
        const auto& pal = *theme::current_theme().palette;
        Element pill = hbox({
            text("TRANSCRIPT") | color(pal.info) | bold,
        });
        left_parts.push_back(std::move(pill));
    }

    // ── Brief mode pill ──────────────────────────────────────────────────
    //    Shown when user enabled brief-only filter (e.g. via /brief).
    if (opts.is_brief_mode) {
        using namespace loom::ui::design;
        const auto& pal = *theme::current_theme().palette;
        Element pill = hbox({
            text("BRIEF") | color(pal.brief_label) | bold,
        });
        left_parts.push_back(std::move(pill));
    }

    // ── Hint parts ──────────────────────────────────────────────────────
    if (opts.show_hint) {
        if (opts.is_loading) {
            hint_parts.push_back(
                hbox({ text(opts.esc_shortcut) | dim | bold,
                       text(" to interrupt") | dim }));
        } else if (has_background_tasks || has_teams) {
            hint_parts.push_back(
                hbox({ text(opts.todos_shortcut) | dim | bold,
                       text(" toggle tasks") | dim }));
        } else if (left_parts.empty()) {
            // Default hint when nothing else is shown
            hint_parts.push_back(text("? for shortcuts") | dim);
        }
    }

    // ── Compose ─────────────────────────────────────────────────────────
    // Join left_parts with " · " separators (Byline style)
    Elements row_parts;
    for (std::size_t i = 0; i < left_parts.size(); ++i) {
        if (i > 0) row_parts.push_back(text(" · ") | dim);
        row_parts.push_back(std::move(left_parts[i]));
    }

    // Append hint parts (truncated at end)
    if (!hint_parts.empty()) {
        if (!row_parts.empty()) row_parts.push_back(text(" · ") | dim);
        for (auto& hp : hint_parts) {
            row_parts.push_back(std::move(hp));
        }
    }

    // Stable 1-row height.  When empty, render a space so FTXUI reserves
    // the row.
    if (row_parts.empty()) {
        return text(" ") | size(HEIGHT, EQUAL, 1);
    }

    return hbox(std::move(row_parts)) | size(HEIGHT, EQUAL, 1);
}

// ============================================================
// PromptInputFooterLeftSide
// ============================================================

struct LeftSideOptions {
    // Exit message
    bool exit_message_show = false;
    std::string exit_message_key;

    // Pasting
    bool is_pasting = false;

    // Vim — nullopt = vim disabled (replaces vim_mode=None + vim_enabled=false)
    std::optional<VimMode> vim_mode;

    // History search
    bool is_searching = false;
    std::string history_query;
    bool history_failed_match = false;

    // Mode indicator
    ModeIndicatorOptions mode_indicator;
};

/// Render the left side of the prompt input footer.
///
/// Priority order (only ONE of the top items shows at a time):
///   1. Exit message (highest priority, replaces everything)
///   2. Pasting indicator
///   3. History search input (when searching)
///   4. Vim INSERT badge (when insert mode and not searching)
///   5. ModeIndicator (always shown as baseline)
///
/// STABLE HEIGHT: returns exactly 1 row.  The ModeIndicator always
///   reserves a row even when empty.
[[nodiscard]] inline Element RenderLeftSide(const LeftSideOptions& opts) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    // 1. Exit message — highest priority, replaces everything
    if (opts.exit_message_show) {
        std::string msg = "Press " + opts.exit_message_key + " again to exit";
        return hbox({ text(msg) | dim }) | size(HEIGHT, EQUAL, 1);
    }

    // 2. Pasting indicator
    if (opts.is_pasting) {
        return hbox({ text("Pasting text…") | dim }) | size(HEIGHT, EQUAL, 1);
    }

    // 3. History search input
    if (opts.is_searching) {
        // HistorySearchInput: "(reverse-i-search)`query':"
        // failed match → red color
        std::string label = "(reverse-i-search)`" + opts.history_query + "':";
        Color c = opts.history_failed_match ? pal.danger : pal.primary;
        return hbox({ text(label) | color(c) }) | size(HEIGHT, EQUAL, 1);
    }

    // 4. Vim INSERT badge + mode indicator
    //    Shows "-- INSERT --" only in insert mode (normal/visual show
    //    nothing extra in the footer).
    bool show_vim = opts.vim_mode == VimMode::Insert;
    if (show_vim) {
        return hbox({
            text("-- INSERT --") | dim,
            filler(),
            RenderModeIndicator(opts.mode_indicator),
        }) | size(HEIGHT, EQUAL, 1);
    }

    // 5. ModeIndicator only (baseline)
    return RenderModeIndicator(opts.mode_indicator);
}

// ============================================================
// StatusLine (simplified — user-configurable command output)
// ============================================================
// StatusLine executes a user-defined shell command and renders ANSI output
// in a dedicated full-width row. We provide a placeholder that can
// be populated by the engine with pre-formatted text.
//
// P0-6: When the user's command returns empty (or no statusLine is
// configured), a built-in statusline shows folder/git/model/token info.
// See BuiltinStatusLineData below.

namespace fs = std::filesystem;

// P0-6 builtin statusline data.  Defined early so StatusLineOptions can
// hold a std::optional<BuiltinStatusLineData>.

/// Per-segment enable mask for the built-in status bar. Each bool controls
/// one segment; defaults match the "default" preset.
struct StatusBarSegments {
    bool cwd = true;       // last 2 path components
    bool git = true;       // git branch name
    bool model = true;     // model display name
    bool tokens = true;    // context usage percentage + token counts
    bool cost = true;      // $ session cost
    bool tasks = false;    // N Team Agents
    bool agent = false;    // current agent name
};

struct BuiltinStatusLineData {
    std::string cwd;                ///< Current working directory (full path)
    std::string git_branch;         ///< Git branch name (empty if not a repo)
    std::string model_name;         ///< Model display name
    int input_tokens = 0;           ///< Input tokens used this session
    int output_tokens = 0;          ///< Output tokens used this session
    int context_token_count = 0;    ///< Total context tokens (in+out)
    std::optional<double> cost_usd; ///< Session cost in USD
    std::int64_t context_window_size = 200000; ///< Context window size (default 200k)
    int task_count = 0;             ///< Background task/teammate count
    std::string agent_name;         ///< Current agent display name
    StatusBarSegments segments;     ///< Per-segment enable mask
    bool status_bar_enabled = true; ///< Master switch (false = hide entirely)
    std::string mouse_mode_label;   ///< Mouse-mode hint (empty = hidden)
    int columns = 0;                ///< Usable terminal cells; zero is unconstrained
};

/// Extract the last N path components for display.
/// e.g. "/a/b/c/d" with n=2 → "c/d"
[[nodiscard]] std::string GetLastPathComponents(
    const std::string& cwd, int n = 2);

/// Format token count as "K" string (e.g. 28000 → "28.0K")
[[nodiscard]] std::string FormatTokensK(std::int64_t tokens);

/// Render the built-in statusline (defined after RenderStatusLine).
[[nodiscard]] Element RenderBuiltinStatusLine(
    const BuiltinStatusLineData& data);

struct StatusLineOptions {
    std::string content;     // Status line output text (may contain ANSI codes)
    bool should_display = false;  // True when statusLine is configured in settings
    bool is_fullscreen = true;    // In fullscreen, reserve row even while loading
    int padding_x = 0;            // Horizontal padding
    // P0-6 builtin statusline: fallback data when content is empty (e.g. user's
    // command returns nothing, or no statusLine configured).  When set and
    // content is empty, RenderBuiltinStatusLine() is used instead of blank
    // placeholder.  Provides folder/git/model/token info for standalone mode.
    std::optional<BuiltinStatusLineData> builtin;
    int columns = 0;         // Row width before status padding; zero is unconstrained
};

/// Render the user-configurable StatusLine.
///
///   - Text may contain ANSI escape codes (SGR) for colored output
///   - ANSI output keeps its own brightness; dim is applied to the local
///     Text component
///   - Single line, truncated if too long
///   - In fullscreen mode, reserves a row even while loading (stable height)
///   - Horizontal padding from settings.statusLine.padding
[[nodiscard]] Element RenderStatusLine(const StatusLineOptions& opts);

// ============================================================
// BridgeStatusIndicator
// ============================================================

enum class BridgeStatus {
    Disabled,
    Connected,
    Disconnected,
    Reconnecting,
};

struct BridgeOptions {
    BridgeStatus status = BridgeStatus::Disabled;
    bool selected = false;
    bool explicit_remote = false;   // explicit (--remote) vs implicit config
};

/// Render the bridge / remote control status indicator.
/// Returns empty element when bridge is disabled or status isn't shown.
[[nodiscard]] inline Element RenderBridgeStatus(const BridgeOptions& opts) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    if (opts.status == BridgeStatus::Disabled) return text("");

    // For implicit (config-driven) remote, only show reconnecting state
    if (!opts.explicit_remote && opts.status != BridgeStatus::Reconnecting) {
        return text("");
    }

    std::string label;
    Color c = pal.muted;
    switch (opts.status) {
        case BridgeStatus::Connected:
            label = "Remote Control"; c = pal.success; break;
        case BridgeStatus::Disconnected:
            label = "Remote disconnected"; c = pal.danger; break;
        case BridgeStatus::Reconnecting:
            label = "Remote Control reconnecting"; c = pal.warning; break;
        default:
            return text("");
    }

    Element el = text(label) | color(c);
    if (opts.selected) {
        el = el | inverted;
        // "· Enter to view" when selected
        el = hbox({ std::move(el), text(" · Enter to view") | dim });
    }
    return el;
}

// ============================================================
// Notifications (right column, non-fullscreen only)
// ============================================================
//
// The Notifications component renders ONE notification at a time, chosen by
// priority.  The full priority chain is:
//   IdeStatusIndicator (highest)
//   > notifications.current (dynamic: env-hook, external-editor-hint, etc.)
//   > overage mode
//   > apiKeyHelper slow
//   > apiKeyStatus invalid/missing
//   > debug mode
//   > verbose token count
//   > TokenWarning (context limit approaching)
//   > AutoUpdater
//   > MemoryUsageIndicator
//   > SandboxPromptFooterHint (lowest)
//
// We implement the top user-visible items that have data available.  Items
// requiring engine wiring (autoUpdater, memory, sandbox) are stubs that
// render nothing until data is provided.

/// API key verification status.
enum class ApiKeyStatus {
    Valid,       // verified and working
    Invalid,     // rejected by API
    Missing,     // no key configured
    Unknown,     // not yet checked
};

/// IDE selection info.
struct IdeSelectionInfo {
    bool connected = false;
    std::optional<std::string> file_path;     // basename shown in indicator
    std::optional<int> selected_lines;        // when text is selected in IDE
};

// ============================================================
// Typed notification pill variants (P1: footer-notifications-stub)
// ============================================================
//
// These typed pill variants provide structured data for the most common
// footer notifications.  They are rendered as styled pills (icon + text
// with colored border/background) via RenderNotificationPill().
//
// To add a typed notification to the queue, use the convenience helpers:
//   AddApiKeyNotification(), AddAutoUpdaterNotification(),
//   AddProRenewalNotification(), AddNewReleaseNotification().

/// Auto-updater install status.
enum class AutoUpdaterStatus {
    Available,      ///< New version found, not yet installed
    Downloading,    ///< Currently downloading/installing
    Installed,      ///< Successfully installed, restart needed
    Error,          ///< Install failed (install_failed or no_permissions)
};

/// Auto-updater notification data.
struct AutoUpdaterData {
    AutoUpdaterStatus status = AutoUpdaterStatus::Available;
    std::string version;          ///< Target version string (e.g. "2.1.57")
    std::string error_detail;     ///< Optional error message for Error status
};

/// Pro/Team subscription renewal reminder.
/// Shows a renewal reminder pill when days_remaining < threshold (default 7).
struct ProRenewalData {
    int days_remaining = 0;       ///< Days until subscription expires
    std::string plan_name;        ///< Plan display name (e.g. "Pro")
};

/// New release announcement pill.
/// Shows "New: vX.Y.Z" gift pill when a new version is announced.
struct NewReleaseData {
    std::string version;          ///< New version string (e.g. "2.2.0")
};

/// Tagged union of typed pill variants for NotificationItem.
/// When set, RenderNotificationPill() uses this to produce a styled pill
/// element instead of the plain text+color path.
enum class PillVariant {
    None,             ///< Plain text notification (default)
    ApiKey,           ///< API key status pill (key icon + status)
    AutoUpdater,      ///< Auto-updater status pill (download icon)
    ProRenewal,       ///< Pro renewal reminder pill (clock icon)
    NewRelease,       ///< New release announcement pill (gift icon)
};

/// Typed pill payload stored in NotificationItem.
struct PillPayload {
    PillVariant variant = PillVariant::None;
    // Only one of these is valid depending on variant:
    ApiKeyStatus       api_key_status = ApiKeyStatus::Unknown;
    AutoUpdaterData    auto_updater;
    ProRenewalData     pro_renewal;
    NewReleaseData     new_release;
};

// ============================================================
// Notification Queue — priority-based rotating carousel
// ============================================================
//
// Implements a priority queue of up to 12 notification items that rotate
// through the "current" slot on a timeout basis.
//   - Priority ordering: immediate > high > medium > low
//   - Each item has a timeout (default 8000ms)
//   - When current expires, highest-priority item from the queue becomes current
//   - Queue capped at 12 items; oldest lowest-priority evicted when full
//   - Items can invalidate others (via invalidates key list)
//   - Duplicate keys are prevented (dedup)
//   - "immediate" priority items replace current right away
//
// The queue is advanced by QueueAdvance() which should be called from the
// engine's event-driven update phase (not a constant ticker — ground rule 5).
// RenderNotifications() reads queue.current if set and shows it as the
// highest-priority display item.

/// Notification priority levels.
enum class NotificationPriority {
    Immediate = 0,  ///< Shown immediately, replaces current
    High      = 1,  ///< Next-highest after immediate
    Medium    = 2,  ///< Default for most notifications
    Low       = 3,  ///< Lowest priority
};

/// A single queued notification item.
struct NotificationItem {
    std::string key;                              ///< Unique key for dedup/invalidation
    std::string text;                             ///< Display text (plain string, no JSX)
    std::string color;                            ///< "error", "warning", "success", "info", or empty=dim
    NotificationPriority priority = NotificationPriority::Low;  ///< Display priority
    int timeout_ms = 8000;                        ///< Display timeout
    std::vector<std::string> invalidates;         ///< Keys this notification invalidates
    PillPayload pill;                             ///< P1: typed pill variant (for styled rendering)
};

/// The notification queue state.
/// Holds the currently-displayed item + up to 12 pending items.
struct NotificationQueue {
    /// Currently displayed item (nullopt if nothing shown).
    std::optional<NotificationItem> current;

    /// Pending items waiting to be shown (capped at kMaxItems).
    std::vector<NotificationItem> queue;

    /// Maximum items in the queue (carousel size = "up to 12").
    static constexpr std::size_t kMaxItems = 12;

    /// Wall-clock time when current was activated (for timeout check).
    /// Stored as seconds since steady_clock epoch for simple comparison.
    double current_activated_at_sec = 0.0;
};

// ============================================================
// Notification data fed into the footer
// ============================================================

/// Notification data fed into the footer.
///
/// Each field corresponds to one notification type.
/// The RenderNotifications() function picks the highest-priority active item.
struct NotificationData {
    // Auth
    ApiKeyStatus api_key_status = ApiKeyStatus::Unknown;
    bool is_remote = false;   // LOOM_REMOTE — changes error text

    // Mode indicators
    bool debug_mode = false;
    bool verbose = false;
    int token_usage = 0;      // used when verbose + apiKey valid

    // Overage
    bool is_overage_mode = false;

    // IDE
    IdeSelectionInfo ide;

    // P1: typed notification pills (footer-notifications-stub)
    // When populated, these render as styled pills in the right column.
    std::optional<AutoUpdaterData> auto_updater;
    std::optional<ProRenewalData> pro_renewal;
    std::optional<NewReleaseData> new_release;

    // Dynamic notification (e.g. env-hook feedback, external-editor hint)
    // When set, takes priority over most static notifications.
    // DEPRECATED: prefer using `queue` instead (NotificationQueue carousel).
    std::optional<std::string> dynamic_text;
    std::optional<std::string> dynamic_color;  // "error", "warning", or empty=dim

    // Notification queue — priority-based rotating carousel of up to 12 items.
    // When queue.current is set, it takes highest priority in RenderNotifications.
    NotificationQueue queue;
};

/// Render the IdeStatusIndicator.
/// Shows "⧉ In <basename>" or "⧉ N lines selected" when IDE is connected
/// and has a selection.  Returns empty element when nothing to show.
[[nodiscard]] inline Element RenderIdeStatusIndicator(const IdeSelectionInfo& ide) {
    using ftxui::text;
    if (!ide.connected) return text("");

    // shouldShowIdeSelection = ideStatus === "connected" &&
    //   (ideSelection?.filePath || (ideSelection?.text && ideSelection.lineCount > 0))
    const bool has_file = ide.file_path.has_value() && !ide.file_path->empty();
    const bool has_text_sel = ide.selected_lines.has_value() && *ide.selected_lines > 0;

    if (!has_file && !has_text_sel) return text("");
    const auto ide_color = loom::ui::design::theme::current_theme().color_for(
        loom::ui::design::tokens::Role::Ide);

    if (has_text_sel) {
        const int n = *ide.selected_lines;
        const std::string unit = (n == 1) ? "line" : "lines";
        // "{lineCount} {unit} selected" — color="ide"
        return hbox({
            text("\xE2\xA7\x89 ") | color(ide_color),   // ⧉
            text(std::to_string(n) + " " + unit + " selected") | color(ide_color),
        });
    }

    if (has_file) {
        // basename(ideSelection.filePath)
        const std::string& path = *ide.file_path;
        auto pos = path.find_last_of("/\\");
        std::string basename = (pos != std::string::npos)
            ? path.substr(pos + 1) : path;
        // "⧉ In {basename}" — color="ide"
        return hbox({
            text("\xE2\xA7\x89 In ") | color(ide_color),   // ⧉ In
            text(basename) | color(ide_color),
        });
    }

    return text("");
}

// ============================================================
// Notification Pill rendering (P1: footer-notifications-stub)
// ============================================================
//
// Each pill: small icon + text, with a colored border or background tint.
// Uses the same semantic color tokens (success/warning/error) and emoji
// icons that render well in modern terminals.

namespace detail {

/// Resolve a semantic color name ("error", "warning", "success", "info")
/// to an FTXUI Color, using the active theme palette where possible.
[[nodiscard]] inline Color ResolveSemanticColor(std::string_view color_name) {
    using namespace loom::ui::design;
    const auto& pal = *theme::current_theme().palette;
    if (color_name == "error")   return pal.danger;
    if (color_name == "warning") return pal.warning;
    if (color_name == "success") return pal.success;
    if (color_name == "info")    return pal.info;
    return pal.muted;   // default dim
}

} // namespace detail

/// Render a styled notification pill from a PillPayload.
///
/// Each pill variant produces a compact 1-row element:
///   ApiKey:    check/cross/warning + explicit key status
///   AutoUpdater: availability arrow, updating indicator, installed check, failed cross
///   ProRenewal: explicit plan renewal reminder (only when < 7 days)
///   NewRelease: [info New: vX.Y.Z] informational role
///
/// Returns empty text("") for PillVariant::None or empty data.
[[nodiscard]] inline Element RenderNotificationPill(const NotificationItem& item) {
    using ftxui::text;
    using ftxui::color;
    using ftxui::bgcolor;
    using ftxui::bold;
    using ftxui::dim;
    using ftxui::hbox;

    using namespace loom::ui::design;
    const auto& pal = *theme::current_theme().palette;

    const auto& pill = item.pill;

    switch (pill.variant) {
        case PillVariant::None:
            // Fallback: plain text+color (backward compatible)
            if (item.text.empty()) return text("");
            {
                Color c = detail::ResolveSemanticColor(item.color);
                return hbox({ text(item.text) | color(c) })
                     | size(HEIGHT, EQUAL, 1);
            }

        case PillVariant::ApiKey: {
            switch (pill.api_key_status) {
                case ApiKeyStatus::Valid: {
                    // ✓ green — key is working
                    const std::string label = std::string(loom::ui::design::figures::kTick) + " API key OK";
                    return hbox({ text(label) | color(pal.success) | dim })
                         | size(HEIGHT, EQUAL, 1);
                }
                case ApiKeyStatus::Invalid: {
                    // ✗ red — key rejected
                    const std::string label = std::string(loom::ui::design::figures::kCross) + " Invalid key";
                    return hbox({ text(label) | color(pal.danger) })
                         | size(HEIGHT, EQUAL, 1);
                }
                case ApiKeyStatus::Missing: {
                    // ! yellow — no key configured
                    const std::string label = std::string(loom::ui::design::figures::kWarning) + " Not logged in";
                    return hbox({ text(label) | color(pal.warning) })
                         | size(HEIGHT, EQUAL, 1);
                }
                case ApiKeyStatus::Unknown:
                    return text("");
            }
            return text("");
        }

        case PillVariant::AutoUpdater: {
            //   Downloading: "Auto-updating…" (dim text)
            //   Installed:   "✓ Update installed · Restart to apply" (success)
            //   Error:       "✗ Auto-update failed" (error)
            //   Available:   "New version vX.Y.Z available" (info)
            const auto& au = pill.auto_updater;
            switch (au.status) {
                case AutoUpdaterStatus::Downloading: {
                    // ⟳ U+27F3 — "updating"
                    const std::string label =
                        std::string("\xE2\x9F\xB3 ") + "Auto-updating…";
                    return hbox({ text(label) | color(pal.muted) | dim })
                         | size(HEIGHT, EQUAL, 1);
                }
                case AutoUpdaterStatus::Installed: {
                    // ✓ U+2713 — "installed"
                    const std::string label =
                        std::string("\xE2\x9C\x93 Update installed \xC2\xB7 Restart to apply");
                    return hbox({ text(label) | color(pal.success) })
                         | size(HEIGHT, EQUAL, 1);
                }
                case AutoUpdaterStatus::Error: {
                    // ✗ U+2717 — "failed"
                    std::string label = "\xE2\x9C\x97 Auto-update failed";
                    if (!au.error_detail.empty()) {
                        label += " \xC2\xB7 " + au.error_detail;
                    }
                    return hbox({ text(label) | color(pal.danger) })
                         | size(HEIGHT, EQUAL, 1);
                }
                case AutoUpdaterStatus::Available: {
                    // The existing downward arrow marks update availability.
                    std::string label = std::string(loom::ui::design::figures::kArrowDown) + " New: v" + au.version;
                    return hbox({ text(label) | color(pal.info) })
                         | size(HEIGHT, EQUAL, 1);
                }
            }
            return text("");
        }

        case PillVariant::ProRenewal: {
            // Shows renewal reminder only when days_remaining < 7.
            const auto& pr = pill.pro_renewal;
            if (pr.days_remaining >= 7) return text("");
            const std::string plan = pr.plan_name.empty() ? "Pro" : pr.plan_name;
            std::string label = plan + " renews in "
                              + std::to_string(pr.days_remaining)
                              + (pr.days_remaining == 1 ? " day" : " days");
            // Color: orange/amber for urgency (use warning for < 3 days, info otherwise)
            Color c = (pr.days_remaining <= 3) ? pal.warning : pal.info;
            return hbox({ text(label) | color(c) })
                 | size(HEIGHT, EQUAL, 1);
        }

        case PillVariant::NewRelease: {
            // The informational figure marks release availability.
            const auto& nr = pill.new_release;
            if (nr.version.empty()) return text("");
            const std::string label =
                (std::string(loom::ui::design::figures::kInfo) + " New: v") + nr.version;
            // Release availability uses the theme's informational accent.
            return hbox({ text(label) | color(pal.info) })
                 | size(HEIGHT, EQUAL, 1);
        }
    }
    return text("");
}

/// Render the highest-priority active notification.
///
/// Returns an element (possibly empty text("") if nothing active).
/// The element is always exactly 1 row high for stable footer height.
[[nodiscard]] inline Element RenderNotifications(const NotificationData& data) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    using ftxui::text;
    using ftxui::dim;
    using ftxui::color;
    using ftxui::hbox;

    // Priority chain (highest first):

    // 1. Notification queue — current item (rotating carousel)
    //    The queue's current item has the highest display priority because
    //    it represents time-sensitive dynamic feedback (env-hook, etc.).
    //    P1: When item.pill.variant != None, render as a styled pill via
    //    RenderNotificationPill() — icon + text with themed colors.
    if (data.queue.current) {
        const auto& item = *data.queue.current;
        const bool has_pill = item.pill.variant != PillVariant::None;
        const bool has_text = !item.text.empty();
        if (has_pill || has_text) {
            if (has_pill) {
                // Styled pill rendering (P1: footer-notifications-stub)
                Element pill_el = RenderNotificationPill(item);
                if (pill_el) {
                    return hbox({ std::move(pill_el) })
                         | size(HEIGHT, EQUAL, 1);
                }
            }
            // Fallback: plain text+color
            if (has_text) {
                Color c = detail::ResolveSemanticColor(item.color);
                return hbox({ text(item.text) | color(c) })
                     | size(HEIGHT, EQUAL, 1);
            }
        }
    }

    // 2. Dynamic notification (env-hook, external-editor hint, etc.)
    //    Kept for backward compatibility; prefer using the queue API.
    if (data.dynamic_text && !data.dynamic_text->empty()) {
        Color c = detail::ResolveSemanticColor(data.dynamic_color
            ? std::string_view{*data.dynamic_color} : std::string_view{});
        return hbox({ text(*data.dynamic_text) | color(c) })
             | size(HEIGHT, EQUAL, 1);
    }

    // 3. IDE status indicator
    Element ide_el = RenderIdeStatusIndicator(data.ide);
    if (ide_el) {
        return hbox({ std::move(ide_el) }) | size(HEIGHT, EQUAL, 1);
    }

    // 4. Overage mode — "Now using extra usage" (dim)
    if (data.is_overage_mode) {
        return hbox({ text("Now using extra usage") | dim })
             | size(HEIGHT, EQUAL, 1);
    }

    // 5. API key invalid/missing — credential problem with the configured
    //    endpoint (error). There is no account login to point the user at, so
    //    the affordance names the configuration instead.
    if (data.api_key_status == ApiKeyStatus::Invalid
        || data.api_key_status == ApiKeyStatus::Missing)
    {
        std::string msg = data.is_remote
            ? "Authentication error \xC2\xB7 Try again"   // ·
            : "No API key \xC2\xB7 Set one in /config";  // ·
        return hbox({ text(msg) | color(pal.danger) })
             | size(HEIGHT, EQUAL, 1);
    }

    // 6. Debug mode — "Debug mode" (warning)
    if (data.debug_mode) {
        return hbox({ text("Debug mode") | color(pal.warning) })
             | size(HEIGHT, EQUAL, 1);
    }

    // 7. Verbose token count — "{tokenUsage} tokens" (dim, only when apiKey valid)
    if (data.verbose && data.api_key_status == ApiKeyStatus::Valid
        && data.token_usage > 0)
    {
        return hbox({ text(std::to_string(data.token_usage) + " tokens") | dim })
             | size(HEIGHT, EQUAL, 1);
    }

    // 8. Auto-updater — styled pill (P1: footer-notifications-stub)
    //    Shows download/install status.  Rendered via NotificationItem with
    //    PillVariant::AutoUpdater so it gets the proper icon + color.
    if (data.auto_updater) {
        NotificationItem au_item;
        au_item.pill.variant = PillVariant::AutoUpdater;
        au_item.pill.auto_updater = *data.auto_updater;
        // Also populate text for accessibility / fallback
        switch (data.auto_updater->status) {
            case AutoUpdaterStatus::Available:
                au_item.text = "New: v" + data.auto_updater->version;
                au_item.color = "info"; break;
            case AutoUpdaterStatus::Downloading:
                au_item.text = "Auto-updating…"; break;
            case AutoUpdaterStatus::Installed:
                au_item.text = "✓ Update installed · Restart to apply";
                au_item.color = "success"; break;
            case AutoUpdaterStatus::Error:
                au_item.text = "✗ Auto-update failed";
                au_item.color = "error"; break;
        }
        Element pill_el = RenderNotificationPill(au_item);
        if (pill_el) {
            return hbox({ std::move(pill_el) }) | size(HEIGHT, EQUAL, 1);
        }
    }

    // 9. New release announcement — release gift pill (P1: footer-notifications-stub)
    if (data.new_release && !data.new_release->version.empty()) {
        NotificationItem nr_item;
        nr_item.pill.variant = PillVariant::NewRelease;
        nr_item.pill.new_release = *data.new_release;
        nr_item.text = "New: v" + data.new_release->version;
        Element pill_el = RenderNotificationPill(nr_item);
        if (pill_el) {
            return hbox({ std::move(pill_el) }) | size(HEIGHT, EQUAL, 1);
        }
    }

    // 10. Pro renewal reminder — orange clock pill (P1: footer-notifications-stub)
    //    CPP enhancement — no direct equivalent.  Shows only when
    //    days_remaining < 7.
    if (data.pro_renewal && data.pro_renewal->days_remaining > 0
        && data.pro_renewal->days_remaining < 7)
    {
        NotificationItem pr_item;
        pr_item.pill.variant = PillVariant::ProRenewal;
        pr_item.pill.pro_renewal = *data.pro_renewal;
        pr_item.text = data.pro_renewal->plan_name + " renews in "
                     + std::to_string(data.pro_renewal->days_remaining) + "d";
        Element pill_el = RenderNotificationPill(pr_item);
        if (pill_el) {
            return hbox({ std::move(pill_el) }) | size(HEIGHT, EQUAL, 1);
        }
    }

    // Nothing active — return empty placeholder row for stable height
    return text(" ") | size(HEIGHT, EQUAL, 1);
}

// ============================================================
// Notification Queue operations
// ============================================================
// These free functions implement the carousel rotation logic.
// They should be called from the engine's event-driven update phase,
// not from a constant-rate ticker (ground rule 5).

namespace detail {

/// Get current steady_clock time as seconds (floating point).
[[nodiscard]] inline double NowSeconds() {
    using clock = std::chrono::steady_clock;
    auto now = clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}

/// Priority value for sorting (lower = higher priority).
[[nodiscard]] inline int PriorityValue(NotificationPriority p) {
    switch (p) {
        case NotificationPriority::Immediate: return 0;
        case NotificationPriority::High:      return 1;
        case NotificationPriority::Medium:    return 2;
        case NotificationPriority::Low:       return 3;
    }
    return 3;
}

/// Find the highest-priority item in the queue (lowest PriorityValue).
[[nodiscard]] inline std::size_t FindHighestPriorityIndex(
    const std::vector<NotificationItem>& queue)
{
    if (queue.empty()) return 0;
    std::size_t best = 0;
    for (std::size_t i = 1; i < queue.size(); ++i) {
        if (PriorityValue(queue[i].priority) < PriorityValue(queue[best].priority)) {
            best = i;
        }
    }
    return best;
}

} // namespace detail

/// Add a notification to the queue.
///
///   - "immediate" priority → replaces current right away, re-queues previous
///   - Other priorities → added to queue (deduped by key)
///   - Queue is capped at kMaxItems (12); oldest lowest-priority evicted
///   - If item.invalidates is set, matching keys are removed from queue/current
inline void QueueAddNotification(NotificationQueue& nq,
                                  const NotificationItem& item)
{
    // Prevent duplicates — dedup by key
    const bool already_in_queue = [&]() {
        for (const auto& q : nq.queue) {
            if (q.key == item.key) return true;
        }
        return false;
    }();
    const bool already_current = nq.current && nq.current->key == item.key;
    if (already_in_queue || already_current) return;

    // Handle invalidation — remove matching keys from queue and current
    if (!item.invalidates.empty()) {
        // Check if current is invalidated
        if (nq.current) {
            for (const auto& inv_key : item.invalidates) {
                if (nq.current->key == inv_key) {
                    nq.current.reset();
                    break;
                }
            }
        }
        // Remove invalidated items from queue
        nq.queue.erase(
            std::remove_if(nq.queue.begin(), nq.queue.end(),
                [&](const NotificationItem& q) {
                    for (const auto& inv_key : item.invalidates) {
                        if (q.key == inv_key) return true;
                    }
                    return false;
                }),
            nq.queue.end());
    }

    // "immediate" priority → show right now
    if (item.priority == NotificationPriority::Immediate) {
        // Re-queue the current item if it's not immediate
        if (nq.current && nq.current->priority != NotificationPriority::Immediate) {
            // Cap queue: if full, evict oldest lowest-priority
            if (nq.queue.size() >= NotificationQueue::kMaxItems) {
                // Find lowest-priority item, prefer older ones
                std::size_t worst = 0;
                for (std::size_t i = 1; i < nq.queue.size(); ++i) {
                    if (detail::PriorityValue(nq.queue[i].priority) >
                        detail::PriorityValue(nq.queue[worst].priority)) {
                        worst = i;
                    }
                }
                nq.queue.erase(nq.queue.begin() + static_cast<std::ptrdiff_t>(worst));
            }
            nq.queue.push_back(*nq.current);
        }
        nq.current = item;
        nq.current_activated_at_sec = detail::NowSeconds();
        return;
    }

    // Non-immediate → add to queue (capped at kMaxItems)
    if (nq.queue.size() >= NotificationQueue::kMaxItems) {
        // Evict the oldest lowest-priority item to make room
        std::size_t worst = 0;
        for (std::size_t i = 1; i < nq.queue.size(); ++i) {
            if (detail::PriorityValue(nq.queue[i].priority) >
                detail::PriorityValue(nq.queue[worst].priority)) {
                worst = i;
            }
        }
        nq.queue.erase(nq.queue.begin() + static_cast<std::ptrdiff_t>(worst));
    }
    nq.queue.push_back(item);
}

/// Remove a notification by key (from both current and queue).
inline void QueueRemoveNotification(NotificationQueue& nq,
                                     const std::string& key)
{
    // Remove from current
    if (nq.current && nq.current->key == key) {
        nq.current.reset();
    }
    // Remove from queue
    nq.queue.erase(
        std::remove_if(nq.queue.begin(), nq.queue.end(),
            [&](const NotificationItem& q) { return q.key == key; }),
        nq.queue.end());
}

/// Advance the queue: if current has expired, clear it and pull the next
/// highest-priority item from the queue.
///
/// Returns true if the display changed (current was advanced or cleared).
///
/// This should be called from the engine's event-driven update loop — NOT
/// from a constant-rate ticker (ground rule 5).  The engine drives repaints
/// on user input, API responses, etc., which is frequent enough that
/// timeout expiry will be detected within reasonable accuracy.
[[nodiscard]] inline bool QueueAdvance(NotificationQueue& nq, double now_sec)
{
    // If nothing is current, try to pull from queue
    if (!nq.current) {
        if (nq.queue.empty()) return false;
        // Get highest-priority item from queue
        std::size_t idx = detail::FindHighestPriorityIndex(nq.queue);
        nq.current = nq.queue[idx];
        nq.queue.erase(nq.queue.begin() + static_cast<std::ptrdiff_t>(idx));
        nq.current_activated_at_sec = now_sec;
        return true;
    }

    // Check if current has expired
    const double elapsed_sec = now_sec - nq.current_activated_at_sec;
    const double timeout_sec = nq.current->timeout_ms / 1000.0;
    if (elapsed_sec < timeout_sec) {
        return false;  // Not yet expired
    }

    // Current has expired — clear it
    nq.current.reset();

    // Try to pull next from queue
    if (!nq.queue.empty()) {
        std::size_t idx = detail::FindHighestPriorityIndex(nq.queue);
        nq.current = nq.queue[idx];
        nq.queue.erase(nq.queue.begin() + static_cast<std::ptrdiff_t>(idx));
        nq.current_activated_at_sec = now_sec;
    }
    return true;
}

/// Convenience overload that uses the current steady_clock time.
[[nodiscard]] inline bool QueueAdvance(NotificationQueue& nq) {
    return QueueAdvance(nq, detail::NowSeconds());
}

/// Get the text and color of the currently-displayed notification.
/// Returns nullopt if nothing is current (queue is idle).
/// This is a const read — it does NOT advance the queue.
[[nodiscard]] inline std::optional<std::pair<std::string, std::string>>
QueueGetCurrentDisplay(const NotificationQueue& nq)
{
    if (!nq.current) return std::nullopt;
    return std::make_pair(nq.current->text, nq.current->color);
}

/// Check if the queue has any items (either current or pending).
[[nodiscard]] inline bool QueueHasItems(const NotificationQueue& nq) {
    return nq.current.has_value() || !nq.queue.empty();
}

// ============================================================
// Typed notification convenience helpers (P1: footer-notifications-stub)
// ============================================================
// These helpers construct properly-configured NotificationItems for the
// most common notification types and add them to the queue.
//
// Each helper:
//   - Sets a unique key for dedup
//   - Configures the pill variant with typed data
//   - Sets appropriate priority and timeout
//   - Calls QueueAddNotification to enqueue

/// Add an API key status notification pill to the queue.
///
///   - Valid:   green check pill (low priority, informational)
///   - Invalid: red X pill (immediate priority — user needs to act)
///   - Missing: yellow ! pill (immediate priority)
///   - Unknown: no-op (nothing to show)
///
/// Invalidates any existing "api-key" notification.
inline void AddApiKeyNotification(NotificationQueue& nq, ApiKeyStatus status) {
    if (status == ApiKeyStatus::Unknown) return;

    NotificationItem item;
    item.key = "api-key-status";
    item.pill.variant = PillVariant::ApiKey;
    item.pill.api_key_status = status;
    item.invalidates = { "api-key-status" };   // replace existing

    switch (status) {
        case ApiKeyStatus::Valid:
            item.text = std::string(loom::ui::design::figures::kTick) + " API key OK";
            item.color = "success";
            item.priority = NotificationPriority::Low;
            item.timeout_ms = 5000;   // brief confirmation
            break;
        case ApiKeyStatus::Invalid:
            item.text = std::string(loom::ui::design::figures::kCross) + " Invalid key";
            item.color = "error";
            item.priority = NotificationPriority::Immediate;
            item.timeout_ms = 15000;  // stays visible longer
            break;
        case ApiKeyStatus::Missing:
            item.text = std::string(loom::ui::design::figures::kWarning) + " Not logged in";
            item.color = "warning";
            item.priority = NotificationPriority::Immediate;
            item.timeout_ms = 15000;
            break;
        case ApiKeyStatus::Unknown:
            return;
    }

    QueueAddNotification(nq, item);
}

/// Add an auto-updater status notification pill to the queue.
///
///   - Available:   blue "⬇ New: vX.Y.Z" (medium priority)
///   - Downloading: dim "⟳ Auto-updating…" (immediate — user is waiting)
///   - Installed:   green "✓ Update installed · Restart to apply" (immediate)
///   - Error:       red "✗ Auto-update failed" (immediate)
///
/// Invalidates any existing "auto-updater" notification.
inline void AddAutoUpdaterNotification(NotificationQueue& nq,
                                        const AutoUpdaterData& data)
{
    NotificationItem item;
    item.key = "auto-updater";
    item.pill.variant = PillVariant::AutoUpdater;
    item.pill.auto_updater = data;
    item.invalidates = { "auto-updater" };   // replace existing

    switch (data.status) {
        case AutoUpdaterStatus::Available:
            item.text = std::string(loom::ui::design::figures::kArrowDown) + " New: v" + data.version;
            item.color = "info";
            item.priority = NotificationPriority::Medium;
            item.timeout_ms = 10000;
            break;
        case AutoUpdaterStatus::Downloading:
            item.text = "\xE2\x9F\xB3 Auto-updating…";   // ⟳
            item.color = "";   // dim
            item.priority = NotificationPriority::Immediate;
            item.timeout_ms = 30000;  // downloading takes a while
            break;
        case AutoUpdaterStatus::Installed:
            item.text = "\xE2\x9C\x93 Update installed \xC2\xB7 Restart to apply";   // ✓ ·
            item.color = "success";
            item.priority = NotificationPriority::Immediate;
            item.timeout_ms = 20000;
            break;
        case AutoUpdaterStatus::Error:
            item.text = "\xE2\x9C\x97 Auto-update failed";   // ✗
            if (!data.error_detail.empty()) {
                item.text += " \xC2\xB7 " + data.error_detail;
            }
            item.color = "error";
            item.priority = NotificationPriority::Immediate;
            item.timeout_ms = 20000;
            break;
    }

    QueueAddNotification(nq, item);
}

/// Add a Pro/Team subscription renewal reminder pill.
///
/// CPP enhancement — no direct equivalent.  Shows only when
/// days_remaining < 7 (configurable urgency threshold).
///
///   - days <= 3:  orange warning (high priority)
///   - days 4-6:   blue info (medium priority)
///   - days >= 7:  no-op (not urgent enough)
///
/// Invalidates any existing "pro-renewal" notification.
inline void AddProRenewalNotification(NotificationQueue& nq,
                                       const ProRenewalData& data,
                                       int urgency_threshold_days = 7)
{
    if (data.days_remaining >= urgency_threshold_days) return;
    if (data.days_remaining <= 0) return;   // already expired

    NotificationItem item;
    item.key = "pro-renewal";
    item.pill.variant = PillVariant::ProRenewal;
    item.pill.pro_renewal = data;
    item.invalidates = { "pro-renewal" };

    const std::string plan = data.plan_name.empty() ? "Pro" : data.plan_name;
    item.text = plan + " renews in "
              + std::to_string(data.days_remaining) + "d";

    if (data.days_remaining <= 3) {
        item.color = "warning";
        item.priority = NotificationPriority::High;
    } else {
        item.color = "info";
        item.priority = NotificationPriority::Medium;
    }
    item.timeout_ms = 12000;

    QueueAddNotification(nq, item);
}

/// Add a new release announcement pill.
///
/// Shows release availability with the informational figure and theme role.
///
/// Invalidates any existing "new-release" notification.
inline void AddNewReleaseNotification(NotificationQueue& nq,
                                       const NewReleaseData& data)
{
    if (data.version.empty()) return;

    NotificationItem item;
    item.key = "new-release";
    item.pill.variant = PillVariant::NewRelease;
    item.pill.new_release = data;
    item.text = std::string(loom::ui::design::figures::kInfo) + " New: v" + data.version;
    item.color = "info";   // release availability uses the informational role
    item.priority = NotificationPriority::Medium;
    item.timeout_ms = 12000;
    item.invalidates = { "new-release", "auto-updater" };   // new release supersedes available update

    QueueAddNotification(nq, item);
}

// ============================================================
// Full PromptInputFooter (left + right columns)
// ============================================================

struct FooterOptions {
    // Left column
    StatusLineOptions status_line;
    LeftSideOptions left_side;

    // Right column
    BridgeOptions bridge;
    NotificationData notification;        // P1: footer notifications
    bool show_notifications = false;      // non-fullscreen only
    bool is_undercover = false;           // ant-only

    // Layout
    bool is_fullscreen = true;
    bool is_narrow = false;            // columns < 80
    bool help_open = false;

    // Suggestions (when present, footer shows only suggestions)
    bool has_suggestions = false;
    Element suggestions_content;       // pre-rendered suggestions list
};

/// Render the status row above the mode and notification/bridge controls.
/// Status uses the full available width independently of right controls.
/// Fullscreen status and left controls each reserve a row while loading.
[[nodiscard]] Element RenderPromptInputFooter(const FooterOptions& opts);

} // namespace loom::ui::prompt::footer
