/// @file design_tokens.cppm
/// @brief Semantic palettes and role-based color resolution.
module;

#include <ftxui/screen/color.hpp>

#include <cstdint>

export module loom.ui.foundation.design_tokens;

import std;

export namespace loom::ui::design::tokens {

// ─── Semantic roles ──────────────────────────────────────────────────────────
// Each Theme exposes a single color per role.  New call sites should use
// token_by_role() rather than reaching into the palette struct by name.
enum class Role : std::uint8_t {
    Primary,                     // Readable Loom accent; logo identity uses loom_body
    Info,                        // Info / auxiliary blue accents
    Success,                     // Green
    Warning,                     // Yellow / amber
    Danger,                      // Error / rejection
    Muted,                       // Readable secondary text
    Subtle,                      // Decorative chrome and borders
    Suggestion,                  // Typeahead suggestion foreground
    Permission,                  // Dialog frame color
    RateLimit,                   // Rate-limit pill fill
    Chrome,                      // UI chrome / background-adjacent lines
    Brief,                       // Brief-mode labels
    Ultra,                       // Ultra-thinking rainbow head
    UserMessageBackground,       // user-bubble fill
    UserMessageBackgroundHover,  // user-bubble hover fill
    // ── New roles (GAP: clr-missing-42-tokens-struct) ─────────────────────
    PlanMode,
    FastMode,
    Remember,
    SelectionBg,
    Ide,
    BashMessageBackground,
    MemoryBackground,
    BriefLabelYou,
    BriefLabelLoom,
    AutoAccept,

};

// ─── Palette ─────────────────────────────────────────────────────────────────
// Canonical fields shared by all seven concrete theme variants.
// Field naming convention: snake_case.
struct Palette {
    ftxui::Color primary;
    ftxui::Color primary_shimmer;
    ftxui::Color info;
    ftxui::Color success;
    ftxui::Color warning;
    ftxui::Color danger;
    ftxui::Color muted;  // readable secondary text
    ftxui::Color subtle;
    ftxui::Color suggestion;
    ftxui::Color text;
    ftxui::Color inverse_text;
    ftxui::Color background;
    ftxui::Color chrome;
    ftxui::Color rate_limit_fill;
    ftxui::Color rate_limit_empty;
    ftxui::Color brief_label;
    // User/assistant message bubble & selection chrome:
    //   userMessageBackground      — dark-mode user bubble tint
    //   messageActionsBackground   — cool-gray bubble when selected/focused
    //   userMessageBackgroundHover — bubble tint on hover / interactive header
    ftxui::Color user_message_background;
    ftxui::Color user_message_background_hover;
    ftxui::Color message_actions_background;
    // rainbow cycle used by ultra-thinking (7 steps)
    std::array<ftxui::Color, 7> rainbow;
    ftxui::Color rainbow_shimmer;
    // diff tokens
    ftxui::Color diff_added;
    ftxui::Color diff_removed;
    ftxui::Color diff_added_word;
    ftxui::Color diff_removed_word;
    ftxui::Color merged;
    // ─── Prompt / input chrome ─────────────────────────
    // bash_border — accent color for '!' bash-mode prefix glyph, prompt border,
    //              transcript bash-input message frames.  Named for semantic
    //              role even though the REPL prompt no longer uses a full
    //              rectangular "border" around the input area.
    //              follows the readable warm primary accent in each theme.
    // prompt_border — outer frame of the brief prompt (separator top/bottom
    //                lines rendered around input).  Variants:
    //                dark/light: ansi:whiteBright ≈ rgb(136,136,136) mid-gray
    ftxui::Color bash_border;
    ftxui::Color prompt_border;
    // In the dark palette: permission == suggestion == rgb(177, 185, 249).
    // Kept as a separate struct field so future palette reworks (ANSI,
    // daltonized) can diverge the two values without breaking call sites.
    ftxui::Color permission;

    // Semantically distinct from `merged` even though values coincide in
    // every theme variant today (both = electric violet).
    ftxui::Color auto_accept;

    // Stable logo/mascot identity. UI primary adapts to the theme for contrast;
    // artwork uses loom_body so its canonical terracotta remains unchanged.
    ftxui::Color loom_body;

    // ─── Accent shimmer tokens ────────────────────────────────────────────
    // Shimmer colors stay in the accent family with readable light/dark
    // counterparts; callers retain ownership of animation timing.
    ftxui::Color permission_shimmer;
    ftxui::Color inactive_shimmer;
    ftxui::Color warning_shimmer;
    ftxui::Color prompt_border_shimmer;
    ftxui::Color fast_mode_shimmer;
    ftxui::Color loom_blue_shimmer;
    // Rainbow per-stop shimmers (rainbow_*_shimmer, 7 stops)
    std::array<ftxui::Color, 7> rainbow_shimmer_stops;

    // ─── Loom blue system spinner (GAP: clr-missing-42-tokens-struct) ────

    ftxui::Color loom_blue;

    // ─── Mode colors ───────────────────────────────────────────────────────
    ftxui::Color plan_mode;
    ftxui::Color ide;
    ftxui::Color remember;
    ftxui::Color fast_mode;

    // ─── Diff dimmed ───────────────────────────────────────────────────────
    ftxui::Color diff_added_dimmed;
    ftxui::Color diff_removed_dimmed;

    // ─── Subagent colors (8 agents) ───────────────────────────────────────
    ftxui::Color subagent_red;
    ftxui::Color subagent_blue;
    ftxui::Color subagent_green;
    ftxui::Color subagent_yellow;
    ftxui::Color subagent_purple;
    ftxui::Color subagent_orange;
    ftxui::Color subagent_pink;
    ftxui::Color subagent_cyan;

    // ─── Miscellaneous tokens ──────────────────────────────────────────────
    ftxui::Color professional_blue;
    ftxui::Color chrome_yellow;
    ftxui::Color selection_bg;
    ftxui::Color bash_message_background;
    ftxui::Color memory_background;
    ftxui::Color brief_label_you;
    ftxui::Color brief_label_loom;

    // Subtle divider border is distinct from decorative subtle in dark themes.
    ftxui::Color border_subtle;
    // ─── Welcome screen tokens ────────────────────────────────────────────
    ftxui::Color status_bar_background;  // welcome screen status bar fill
    ftxui::Color spinner_gold;           // welcome spinner / logo glyph gold

    // ─── Message list role tokens (GAP: clr-missing-role-tokens) ──────────
    // Background tints for the role badge in the message list header.
    ftxui::Color role_bg_user;       // user role badge background
    ftxui::Color role_bg_assistant;  // assistant role badge background
    ftxui::Color role_bg_system;     // system role badge background
    ftxui::Color role_bg_tool;       // tool role badge background
    ftxui::Color role_bg_thinking;   // thinking role badge background
    // Pill (accent bar) colors for the role badge.
    ftxui::Color role_pill_user;       // user role accent pill
    ftxui::Color role_pill_assistant;  // assistant role accent pill
    ftxui::Color role_pill_system;     // system role accent pill
    ftxui::Color role_pill_tool;       // tool role accent pill
    ftxui::Color role_pill_thinking;   // thinking role accent pill
    // Message list accents.
    ftxui::Color message_error_accent;         // error message accent
    ftxui::Color message_redacted_accent;      // redacted message accent
    ftxui::Color message_list_selected_bg;     // selected row background
    ftxui::Color message_list_muted_fg;        // muted foreground text
    ftxui::Color message_list_empty_state_fg;  // empty-state hint text
    ftxui::Color message_list_streaming_fg;    // streaming indicator text

    // ─── Surface tint tokens (GAP: clr-hardcoded-surface-tints) ───────────
    // Dark blue-gray backgrounds for selected rows, cards, and panels.
    // Each is a step on the surface-tint scale from darkest to lightest.
    ftxui::Color surface_tint_1;  // card / selected-row background (darkest)
    ftxui::Color surface_tint_2;  // selected-row background (dialogs)
    ftxui::Color surface_tint_3;  // selected-row background (plugins)
    ftxui::Color surface_tint_4;  // selected-row background (agents)
    ftxui::Color surface_tint_5;  // surface background (lightest)

    // ─── Status tint background tokens (GAP: clr-hardcoded-status-tints) ──
    // Dark status-colored backgrounds for badges, pills, and tinted panels.
    ftxui::Color success_tint_bg;  // dark green background
    ftxui::Color danger_tint_bg;   // dark red background
    ftxui::Color warning_tint_bg;  // dark amber background
    ftxui::Color info_tint_bg;     // dark blue background

    // ─── Accent tokens (GAP: clr-hardcoded-accents) ───────────────────────
    ftxui::Color accent_green;   // green dot / success accent
    ftxui::Color accent_purple;  // purple accent
    ftxui::Color accent_pink;    // historical key for the warm mode indicator

    // ─── Stats heat-gradient tokens (GAP: clr-hardcoded-stats-gradient) ──
    // 5-step green heat gradient for the stats widget.
    ftxui::Color stats_green_1;  // coolest
    ftxui::Color stats_green_2;
    ftxui::Color stats_green_3;
    ftxui::Color stats_green_4;
    ftxui::Color stats_green_5;  // hottest

    // ─── Permission rule list tokens (GAP: clr-hardcoded-perm-rule-list) ──
    // Background tints for the permission rule list entries.
    ftxui::Color perm_rule_bg;           // base rule background
    ftxui::Color perm_rule_bg_alt;       // alternate / highlighted background
    ftxui::Color perm_rule_bg_hover;     // hover background
    ftxui::Color perm_rule_bg_selected;  // selected background
    ftxui::Color perm_rule_bg_info;      // info-tinted background
    ftxui::Color perm_rule_bg_dark;      // dark background
    ftxui::Color perm_rule_bg_darker;    // darker background
    ftxui::Color perm_rule_bg_active;    // active background
    ftxui::Color perm_rule_bg_accent;    // accent-tinted background
};

// ─── Concrete palettes ───────────────────────────────────────────────────────
// Light/daltonized variants included; monochrome reduces everything to
// grayscale for accessibility testing and reduced-color terminals.
namespace palette {

// Construct concrete colors on first palette use, after terminal setup.
const Palette& dark() noexcept;

const Palette& light() noexcept;

// Color-vision-deficiency variants retain explicit differentiated status
// colors and readable agent hues. Names/glyphs remain available; these
// palettes are not a clinical color-distinguishability certification.
const Palette& dark_daltonized() noexcept;

const Palette& light_daltonized() noexcept;

// ── Light ANSI palette ──
// Uses only the 16 standard ANSI palette16 colors for terminals without
// true-color support.  ansi:* names mapped to ftxui::Color::Palette16.
const Palette& light_ansi() noexcept;

// ── Dark ANSI palette ──
// Uses only the 16 standard ANSI palette16 colors.
const Palette& dark_ansi() noexcept;

// Monochrome palette: for reduced-color / braille-only terminals.
const Palette& monochrome() noexcept;

}  // namespace palette

// ─── Role → Color resolution ─────────────────────────────────────────────────
/// Resolve a semantic Role to its concrete ftxui::Color inside *palette*.
[[nodiscard]] inline ftxui::Color token_by_role(const Palette& pal, Role role) noexcept {
    switch (role) {
    case Role::Primary:
        return pal.primary;
    case Role::Info:
        return pal.info;
    case Role::Success:
        return pal.success;
    case Role::Warning:
        return pal.warning;
    case Role::Danger:
        return pal.danger;
    case Role::Muted:
        return pal.muted;
    case Role::Subtle:
        return pal.subtle;
    case Role::Suggestion:
        return pal.suggestion;
    case Role::Permission:
        return pal.permission;
    case Role::RateLimit:
        return pal.rate_limit_fill;
    case Role::Chrome:
        return pal.chrome;
    case Role::Brief:
        return pal.brief_label;
    case Role::Ultra:
        return pal.rainbow_shimmer;
    case Role::UserMessageBackground:
        return pal.user_message_background;
    case Role::UserMessageBackgroundHover:
        return pal.user_message_background_hover;
    // New roles (clr-missing-42-tokens-struct)
    case Role::PlanMode:
        return pal.plan_mode;
    case Role::FastMode:
        return pal.fast_mode;
    case Role::Remember:
        return pal.remember;
    case Role::SelectionBg:
        return pal.selection_bg;
    case Role::Ide:
        return pal.ide;
    case Role::BashMessageBackground:
        return pal.bash_message_background;
    case Role::MemoryBackground:
        return pal.memory_background;
    case Role::BriefLabelYou:
        return pal.brief_label_you;
    case Role::BriefLabelLoom:
        return pal.brief_label_loom;
    case Role::AutoAccept:
        return pal.auto_accept;
    }
    return pal.text;
}

}  // namespace loom::ui::design::tokens
