/// @file design_tokens.cppm
/// @brief Design tokens: semantic palette, spacing, typography, animation,
/// role-based color resolution.
module;

#include <cstdint>
#include <cmath>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

export module loom.ui.foundation.design_tokens;

import std;

export namespace loom::ui::design::tokens {

// ─── Spacing (cells in the TTY grid) ─────────────────────────────────────────
// Uses a 4/8/16/24/32/48 px grid.  Since the terminal
// is cell-based we simply expose integer counts of cells.
namespace spacing {
inline constexpr std::int32_t xs   = 1;
inline constexpr std::int32_t sm   = 2;
inline constexpr std::int32_t md   = 3;
inline constexpr std::int32_t lg   = 4;
inline constexpr std::int32_t xl   = 6;
inline constexpr std::int32_t xxl  = 8;
} // namespace spacing

// ─── Radius (FTXUI border decorator approximation) ───────────────────────────
// FTXUI only exposes border/borderRounded/borderDouble/borderHeavy/borderLight
// so we expose an enum mapping directly to those decorator names.
enum class Radius { None, Light, Rounded, Double, Heavy };

// ─── Typography scale ────────────────────────────────────────────────────────
// On a TTY we can only change the *apparent* size via bold/dim/underlined and
// separator lines.  The enum values are therefore "hints" consumed by
// ui::design::primitives::heading() rather than true font-sizes.
enum class TextSize : std::uint8_t {
    Caption,   // dim, reduced weight
    Body,      // default
    Subtitle,  // bold
    Title,     // bold + underlined separator
    Display,   // bold + heavy separator + blockquote bar
};

// ─── Animation easing constants ──────────────────────────────────────────────
// PHASE_5 NOTE: animation is driven by explicit per-frame t∈[0,1] lerps.
// We expose the easing function numbers directly as cubic-bezier constants
// so callers can plug them into a chrono-driven Renderer without pulling
// in a full tweening engine.
namespace easing {
inline constexpr double linear            = 1.0;  // linear easing coefficient
inline constexpr double ease_out_cubic    = 0.33; // deceleration
inline constexpr double ease_in_out_quad  = 0.5;  // symmetric
inline constexpr double standard          = 0.2;  // Material-ish
// HSL sweep timings
inline constexpr std::int32_t asterisk_sweep_ms      = 1500;
inline constexpr std::int32_t asterisk_sweep_count   = 2;
inline constexpr std::int32_t spinner_frame_interval = 80;
inline constexpr std::int32_t logo_refresh_ms        = 120;
} // namespace easing

// ─── Semantic roles ──────────────────────────────────────────────────────────
// Each Theme exposes a single color per role.  New call sites should use
// token_by_role() rather than reaching into the palette struct by name.
enum class Role : std::uint8_t {
    Primary,      // Loom accent (loom_body)
    Info,         // Info / auxiliary blue accents
    Success,      // Green
    Warning,      // Yellow / amber
    Danger,       // Error / rejection
    Muted,        // Inactive / dim
    Subtle,       // Subtle foreground (chrome, borders)
    Suggestion,   // Typeahead suggestion foreground
    Permission,   // Dialog frame color
    RateLimit,    // Rate-limit pill fill
    Chrome,       // UI chrome / background-adjacent lines
    Brief,        // Brief-mode labels
    Ultra,        // Ultra-thinking rainbow head
    UserMessageBackground,      // user-bubble fill
    UserMessageBackgroundHover, // user-bubble hover fill
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
    // ── Semantic derived roles (GAP: clr-missing-42-tokens-struct) ────────
    TextMuted,             // secondary text
    TextLink,              // link text
    TextAccent,            // accent-colored text
    BorderSubtle,          // subtle divider borders
    BorderDefault,         // default frame borders
    BorderAccent,          // accent-colored border
    BorderError,           // error-state border
    SurfaceHover,          // hover-state surface
    SurfaceSelected,       // selected/active surface
    SurfaceBash,           // bash message surface
    SurfaceMemory,         // memory surface
    IconDefault,           // default icon color
    IconMuted,             // muted icon color
    IconAccent,            // accent icon color
};

// ─── Palette ─────────────────────────────────────────────────────────────────
// Comprehensive palette with all theme fields for all 5 variants
// (dark/light/darkDaltonized/lightDaltonized/monochrome).
// Field naming convention: snake_case.
struct Palette {
    ftxui::Color primary;
    ftxui::Color primary_shimmer;
    ftxui::Color info;
    ftxui::Color success;
    ftxui::Color warning;
    ftxui::Color danger;
    ftxui::Color muted;      // "inactive"
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
    //              dark:   rgb(253,  93, 177)  bright pink
    //              light:  rgb(255,   0, 135)  vibrant pink (more saturated)
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

    // this stays at the canonical clawd orange rgb(215,119,87) while
    // `primary`/`loom` shifts to a CVD-safe orange rgb(255,153,51).
    // Logo/mascot rendering MUST use loom_body, not primary, to avoid
    // color shifts in daltonized variants.
    ftxui::Color loom_body;

    // ─── Shimmer tokens (GAP: clr-shimmer-tokens-missing) ──────────────────
    // Only primary_shimmer + rainbow_shimmer existed; these 12 complete the set.
    // Shimmer colors are brighter/saturated versions of base colors used in
    // animated sweeps and attention-grabbing UI elements.
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
    ftxui::Color clawd_background;
    ftxui::Color selection_bg;
    ftxui::Color bash_message_background;
    ftxui::Color memory_background;
    ftxui::Color brief_label_you;
    ftxui::Color brief_label_loom;

    // ─── Individual rainbow fields (GAP: clr-missing-42-tokens-struct) ──────
    // Previously only accessible via rainbow[] / rainbow_shimmer_stops[] arrays;
    // now exposed as individually-named members for direct field access.
    ftxui::Color rainbow_red;
    ftxui::Color rainbow_orange;
    ftxui::Color rainbow_yellow;
    ftxui::Color rainbow_green;
    ftxui::Color rainbow_blue;
    ftxui::Color rainbow_indigo;
    ftxui::Color rainbow_violet;
    ftxui::Color rainbow_red_shimmer;
    ftxui::Color rainbow_orange_shimmer;
    ftxui::Color rainbow_yellow_shimmer;
    ftxui::Color rainbow_green_shimmer;
    ftxui::Color rainbow_blue_shimmer;
    ftxui::Color rainbow_indigo_shimmer;
    ftxui::Color rainbow_violet_shimmer;

    // ─── Semantic derived tokens (GAP: clr-missing-42-tokens-struct) ────────
    // Higher-level UI semantic roles not present as explicit theme fields
    // but commonly needed by components.  Resolved from base palette at
    // construction time so each theme variant can override independently.
    ftxui::Color text_muted;              // secondary text — alias for muted
    ftxui::Color text_link;               // link text — alias for suggestion
    ftxui::Color text_accent;             // accent-colored text — alias for primary
    ftxui::Color border_subtle;           // subtle divider borders — derived from subtle
    ftxui::Color border_default;          // default frame borders — alias for prompt_border
    ftxui::Color border_accent;           // accent-colored border — alias for permission
    ftxui::Color border_error;            // error-state border — alias for danger
    ftxui::Color surface_hover;           // hover-state surface — alias for user_message_background_hover
    ftxui::Color surface_selected;        // selected/active surface — alias for message_actions_background
    ftxui::Color surface_bash;            // bash message surface — alias for bash_message_background
    ftxui::Color surface_memory;          // memory surface — alias for memory_background
    ftxui::Color icon_default;            // default icon color — alias for text
    ftxui::Color icon_muted;              // muted icon color — alias for muted
    ftxui::Color icon_accent;             // accent icon color — alias for primary
    // ─── Welcome screen tokens ────────────────────────────────────────────
    ftxui::Color status_bar_background;   // welcome screen status bar fill
    ftxui::Color spinner_gold;            // welcome spinner / logo glyph gold

    // ─── Message list role tokens (GAP: clr-missing-role-tokens) ──────────
    // Background tints for the role badge in the message list header.
    ftxui::Color role_bg_user;            // user role badge background
    ftxui::Color role_bg_assistant;       // assistant role badge background
    ftxui::Color role_bg_system;          // system role badge background
    ftxui::Color role_bg_tool;            // tool role badge background
    ftxui::Color role_bg_thinking;        // thinking role badge background
    // Pill (accent bar) colors for the role badge.
    ftxui::Color role_pill_user;          // user role accent pill
    ftxui::Color role_pill_assistant;     // assistant role accent pill
    ftxui::Color role_pill_system;        // system role accent pill
    ftxui::Color role_pill_tool;          // tool role accent pill
    ftxui::Color role_pill_thinking;      // thinking role accent pill
    // Message list accents.
    ftxui::Color message_error_accent;    // error message accent
    ftxui::Color message_redacted_accent; // redacted message accent
    ftxui::Color message_list_selected_bg; // selected row background
    ftxui::Color message_list_muted_fg;   // muted foreground text
    ftxui::Color message_list_empty_state_fg; // empty-state hint text
    ftxui::Color message_list_streaming_fg;   // streaming indicator text

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
    ftxui::Color accent_pink;    // pink accent (mode indicator)

    // ─── Stats heat-gradient tokens (GAP: clr-hardcoded-stats-gradient) ──
    // 5-step green heat gradient for the stats widget.
    ftxui::Color stats_green_1;  // coolest
    ftxui::Color stats_green_2;
    ftxui::Color stats_green_3;
    ftxui::Color stats_green_4;
    ftxui::Color stats_green_5;  // hottest

    // ─── Permission rule list tokens (GAP: clr-hardcoded-perm-rule-list) ──
    // Background tints for the permission rule list entries.
    ftxui::Color perm_rule_bg;         // base rule background
    ftxui::Color perm_rule_bg_alt;     // alternate / highlighted background
    ftxui::Color perm_rule_bg_hover;   // hover background
    ftxui::Color perm_rule_bg_selected; // selected background
    ftxui::Color perm_rule_bg_info;    // info-tinted background
    ftxui::Color perm_rule_bg_dark;    // dark background
    ftxui::Color perm_rule_bg_darker;  // darker background
    ftxui::Color perm_rule_bg_active;  // active background
    ftxui::Color perm_rule_bg_accent;  // accent-tinted background
};

// ─── Concrete palettes ───────────────────────────────────────────────────────
// Light/daltonized variants included; monochrome reduces everything to
// grayscale for accessibility testing and reduced-color terminals.
namespace palette {

// rgb(215,119,87) — Loom's canonical "clawd" orange
inline const auto CLAWDED      = ftxui::Color::RGB(215, 119,  87);
inline const auto CLAWDED_SHIM = ftxui::Color::RGB(235, 159, 127);

inline const Palette& dark() noexcept {
    static const Palette p = {
    .primary             = CLAWDED,
    .primary_shimmer     = CLAWDED_SHIM,
    .info                = ftxui::Color::RGB(177, 185, 249),
    .success             = ftxui::Color::RGB( 78, 186, 101),
    .warning             = ftxui::Color::RGB(255, 193,   7),
    .danger              = ftxui::Color::RGB(255, 107, 128),
    .muted               = ftxui::Color::RGB(153, 153, 153),
    //   subtle                   = rgb(80, 80, 80)
    //   suggestion               = rgb(177, 185, 249)  (lavender, NOT sky blue)
    //   text                     = rgb(255, 255, 255)  (pure white)
    // NOTE (clr-dark-palette-11-p1): `info` carries the info/permission
    // accent — corrected sky-blue rgb(110,151,255) → lavender
    // rgb(177,185,249) (== permission/suggestion) so dialog/info accents match.
    .subtle              = ftxui::Color::RGB( 80,  80,  80),
    .suggestion          = ftxui::Color::RGB(177, 185, 249),
    .text                = ftxui::Color::RGB(255, 255, 255),
    // accent chips.  (Distinct from `background`, which stays app-chrome dark.)
    .inverse_text        = ftxui::Color::RGB(  0,   0,   0),
    // The background stays app-chrome dark rgb(32,33,36) rather than a
    // bright cyan that was never rendered — avoids a destructive visual
    // change.
    .background          = ftxui::Color::RGB( 32,  33,  36),
    .chrome              = ftxui::Color::RGB( 55,  57,  61),
    .rate_limit_fill     = ftxui::Color::RGB(177, 185, 249),
    .rate_limit_empty    = ftxui::Color::RGB( 80,  83, 112),
    .brief_label         = CLAWDED,
    //   userMessageBackground      = rgb(55, 55, 55)      (dark slate bubble)
    //   messageActionsBackground   = rgb(44, 50, 62)      (cool-gray selection)
    //   userMessageBackgroundHover = rgb(70, 70, 70)      (slightly lighter slate)
    .user_message_background        = ftxui::Color::RGB( 55,  55,  55),
    .user_message_background_hover  = ftxui::Color::RGB( 70,  70,  70),
    .message_actions_background     = ftxui::Color::RGB( 44,  50,  62),
    // themes).  Corrected from the Chart.js default palette to the
    // warm→cool 7-stop gradient (clr-rainbow-28-p1).
    .rainbow = {{
        ftxui::Color::RGB(235,  95,  87),   // rainbow_red
        ftxui::Color::RGB(245, 139,  87),   // rainbow_orange
        ftxui::Color::RGB(250, 195,  95),   // rainbow_yellow
        ftxui::Color::RGB(145, 200, 130),   // rainbow_green
        ftxui::Color::RGB(130, 170, 220),   // rainbow_blue
        ftxui::Color::RGB(155, 130, 200),   // rainbow_indigo
        ftxui::Color::RGB(200, 130, 180),   // rainbow_violet
    }},
    .rainbow_shimmer     = ftxui::Color::RGB(230, 230, 250),
    // GitHub-bright greens/reds to the original's saturated dark shades (clr-dark-11):
    //   diffAdded=rgb(34,92,43)  diffRemoved=rgb(122,41,54)
    //   diffAddedWord=rgb(56,166,96)  diffRemovedWord=rgb(179,89,107)
    .diff_added          = ftxui::Color::RGB( 34,  92,  43),
    .diff_removed        = ftxui::Color::RGB(122,  41,  54),
    .diff_added_word     = ftxui::Color::RGB( 56, 166,  96),
    .diff_removed_word   = ftxui::Color::RGB(179,  89, 107),
    .merged              = ftxui::Color::RGB(175, 135, 255),
    // Prompt chrome:
    //   dark bash_border            = rgb(253, 93, 177)   bright pink
    //   dark prompt_border          = rgb(136,136, 136)  ansi:whiteBright
    .bash_border         = ftxui::Color::RGB(253,  93, 177),
    .prompt_border       = ftxui::Color::RGB(136, 136, 136),
    .permission          = ftxui::Color::RGB(177, 185, 249),
    .auto_accept         = ftxui::Color::RGB(175, 135, 255),
    .loom_body          = CLAWDED,
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color::RGB(207, 215, 255),
    .inactive_shimmer       = ftxui::Color::RGB(193, 193, 193),
    .warning_shimmer        = ftxui::Color::RGB(255, 223,  57),
    .prompt_border_shimmer  = ftxui::Color::RGB(166, 166, 166),
    .fast_mode_shimmer      = ftxui::Color::RGB(255, 165,  70),
    .loom_blue_shimmer    = ftxui::Color::RGB(177, 195, 255),
    .rainbow_shimmer_stops = {{
        ftxui::Color::RGB(250, 155, 147),
        ftxui::Color::RGB(255, 185, 137),
        ftxui::Color::RGB(255, 225, 155),
        ftxui::Color::RGB(185, 230, 180),
        ftxui::Color::RGB(180, 205, 240),
        ftxui::Color::RGB(195, 180, 230),
        ftxui::Color::RGB(230, 180, 210),
    }},
    // ── Loom blue system spinner ──
    .loom_blue           = ftxui::Color::RGB(147, 165, 255),
    // ── Mode colors ──
    .plan_mode             = ftxui::Color::RGB( 72, 150, 140),
    .ide                   = ftxui::Color::RGB( 71, 130, 200),
    .remember              = ftxui::Color::RGB(177, 185, 249),
    .fast_mode             = ftxui::Color::RGB(255, 120,  20),
    // ── Diff dimmed ──
    .diff_added_dimmed     = ftxui::Color::RGB( 71,  88,  74),
    .diff_removed_dimmed   = ftxui::Color::RGB(105,  72,  77),
    // ── Subagent colors (*_FOR_SUBAGENTS_ONLY, dark theme 473-480) ──
    .subagent_red          = ftxui::Color::RGB(220,  38,  38),
    .subagent_blue         = ftxui::Color::RGB( 37,  99, 235),
    .subagent_green        = ftxui::Color::RGB( 22, 163,  74),
    .subagent_yellow       = ftxui::Color::RGB(202, 138,   4),
    .subagent_purple       = ftxui::Color::RGB(147,  51, 234),
    .subagent_orange       = ftxui::Color::RGB(234,  88,  12),
    .subagent_pink         = ftxui::Color::RGB(219,  39, 119),
    .subagent_cyan         = ftxui::Color::RGB(  8, 145, 178),
    // ── Misc ──
    .professional_blue     = ftxui::Color::RGB(106, 155, 204),
    .chrome_yellow         = ftxui::Color::RGB(251, 188,   4),
    .clawd_background      = ftxui::Color::RGB(  0,   0,   0),
    .selection_bg          = ftxui::Color::RGB( 38,  79, 120),
    .bash_message_background = ftxui::Color::RGB( 65,  60,  65),
    .memory_background     = ftxui::Color::RGB( 55,  65,  70),
    .brief_label_you       = ftxui::Color::RGB(122, 180, 232),
    .brief_label_loom    = CLAWDED,
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color::RGB(235,  95,  87),
    .rainbow_orange        = ftxui::Color::RGB(245, 139,  87),
    .rainbow_yellow        = ftxui::Color::RGB(250, 195,  95),
    .rainbow_green         = ftxui::Color::RGB(145, 200, 130),
    .rainbow_blue          = ftxui::Color::RGB(130, 170, 220),
    .rainbow_indigo        = ftxui::Color::RGB(155, 130, 200),
    .rainbow_violet        = ftxui::Color::RGB(200, 130, 180),
    .rainbow_red_shimmer   = ftxui::Color::RGB(250, 155, 147),
    .rainbow_orange_shimmer= ftxui::Color::RGB(255, 185, 137),
    .rainbow_yellow_shimmer= ftxui::Color::RGB(255, 225, 155),
    .rainbow_green_shimmer = ftxui::Color::RGB(185, 230, 180),
    .rainbow_blue_shimmer  = ftxui::Color::RGB(180, 205, 240),
    .rainbow_indigo_shimmer= ftxui::Color::RGB(195, 180, 230),
    .rainbow_violet_shimmer= ftxui::Color::RGB(230, 180, 210),
    // ── Semantic derived tokens (dark theme) ──
    .text_muted            = ftxui::Color::RGB(153, 153, 153),  // = muted
    .text_link             = ftxui::Color::RGB(177, 185, 249),  // = suggestion
    .text_accent           = CLAWDED,                            // = primary
    .border_subtle         = ftxui::Color::RGB( 55,  57,  61),  // = chrome
    .border_default        = ftxui::Color::RGB(136, 136, 136),  // = prompt_border
    .border_accent         = ftxui::Color::RGB(177, 185, 249),  // = permission
    .border_error          = ftxui::Color::RGB(255, 107, 128),  // = danger
    .surface_hover         = ftxui::Color::RGB( 70,  70,  70),  // = user_message_background_hover
    .surface_selected      = ftxui::Color::RGB( 44,  50,  62),  // = message_actions_background
    .surface_bash          = ftxui::Color::RGB( 65,  60,  65),  // = bash_message_background
    .surface_memory        = ftxui::Color::RGB( 55,  65,  70),  // = memory_background
    .icon_default          = ftxui::Color::RGB(255, 255, 255),  // = text (white on dark)
    .icon_muted            = ftxui::Color::RGB(153, 153, 153),  // = muted
    .icon_accent           = CLAWDED,                            // = primary
    .status_bar_background = ftxui::Color::RGB( 20,  20,  22),
    .spinner_gold          = ftxui::Color::RGB(217, 154,  56),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_assistant       = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_system          = ftxui::Color::RGB( 30,  30,  36),
    .role_bg_tool            = ftxui::Color::RGB( 24,  40,  40),
    .role_bg_thinking        = ftxui::Color::RGB( 34,  30,  48),
    .role_pill_user          = ftxui::Color::RGB( 59, 130, 246),
    .role_pill_assistant     = ftxui::Color::RGB(168,  85, 247),
    .role_pill_system        = ftxui::Color::RGB(234, 179,   8),
    .role_pill_tool          = ftxui::Color::RGB( 20, 184, 166),
    .role_pill_thinking      = ftxui::Color::RGB(139,  92, 246),
    .message_error_accent    = ftxui::Color::RGB(239,  68,  68),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB( 30,  64, 175),
    .message_list_muted_fg      = ftxui::Color::RGB(156, 163, 175),
    .message_list_empty_state_fg = ftxui::Color::RGB(107, 114, 128),
    .message_list_streaming_fg  = ftxui::Color::RGB( 34, 211, 238),
    .surface_tint_1           = ftxui::Color::RGB( 25,  30,  45),
    .surface_tint_2           = ftxui::Color::RGB( 30,  40,  55),
    .surface_tint_3           = ftxui::Color::RGB( 20,  30,  50),
    .surface_tint_4           = ftxui::Color::RGB( 25,  35,  50),
    .surface_tint_5           = ftxui::Color::RGB( 20,  30,  55),
    .success_tint_bg          = ftxui::Color::RGB( 16,  36,  24),
    .danger_tint_bg           = ftxui::Color::RGB( 40,  16,  20),
    .warning_tint_bg          = ftxui::Color::RGB( 40,  30,   0),
    .info_tint_bg             = ftxui::Color::RGB( 30,  40,  60),
    .accent_green             = ftxui::Color::RGB( 80, 200, 120),
    .accent_purple            = ftxui::Color::RGB(120,  80, 120),
    .accent_pink              = ftxui::Color::RGB(255,   0, 135),
    .stats_green_1            = ftxui::Color::RGB( 60, 100,  60),
    .stats_green_2            = ftxui::Color::RGB( 80, 140,  80),
    .stats_green_3            = ftxui::Color::RGB(100, 180, 100),
    .stats_green_4            = ftxui::Color::RGB(120, 220, 120),
    .stats_green_5            = ftxui::Color::RGB(140, 255, 140),
    .perm_rule_bg             = ftxui::Color::RGB( 18,  18,  22),
    .perm_rule_bg_alt         = ftxui::Color::RGB( 40,  40,  46),
    .perm_rule_bg_hover       = ftxui::Color::RGB( 28,  34,  46),
    .perm_rule_bg_selected    = ftxui::Color::RGB( 30,  32,  36),
    .perm_rule_bg_info        = ftxui::Color::RGB( 28,  48,  62),
    .perm_rule_bg_dark        = ftxui::Color::RGB( 26,  28,  32),
    .perm_rule_bg_darker      = ftxui::Color::RGB( 20,  20,  24),
    .perm_rule_bg_active      = ftxui::Color::RGB( 30,  32,  42),
    .perm_rule_bg_accent      = ftxui::Color::RGB( 20,  28,  48),
};
    return p;
}

inline const Palette& light() noexcept {
    static const Palette p = {
    .primary             = CLAWDED,
    .primary_shimmer     = ftxui::Color::RGB(245, 149, 117),
    // (clr-light-palette-17-p1).  `info` carries the permission/suggestion
    // accent = rgb(87,105,247) medium blue.
    .info                = ftxui::Color::RGB( 87, 105, 247),
    .success             = ftxui::Color::RGB( 44, 122,  57),
    .warning             = ftxui::Color::RGB(150, 108,  30),
    .danger              = ftxui::Color::RGB(171,  43,  63),
    .muted               = ftxui::Color::RGB(102, 102, 102),
    .subtle              = ftxui::Color::RGB(175, 175, 175),
    .suggestion          = ftxui::Color::RGB( 87, 105, 247),
    .text                = ftxui::Color::RGB(  0,   0,   0),
    .inverse_text        = ftxui::Color::RGB(255, 255, 255),
    // as dark; keep CPP's real light app-chrome rather than paint the UI cyan.
    .background          = ftxui::Color::RGB(245, 245, 245),
    .chrome              = ftxui::Color::RGB(220, 220, 220),
    .rate_limit_fill     = ftxui::Color::RGB( 87, 105, 247),
    .rate_limit_empty    = ftxui::Color::RGB( 39,  47, 111),
    .brief_label         = CLAWDED,
    // Light-mode equivalents of the message chrome tokens:
    //   userMessageBackground      = rgb(240, 240, 240)  near-white bubble
    //   messageActionsBackground   = rgb(232, 236, 244)  cool gray
    //   userMessageBackgroundHover = rgb(252, 252, 252)  near-white (barely visible on light bg)
    .user_message_background        = ftxui::Color::RGB(240, 240, 240),
    .user_message_background_hover  = ftxui::Color::RGB(252, 252, 252),
    .message_actions_background     = ftxui::Color::RGB(232, 236, 244),
    .rainbow              = dark().rainbow,
    .rainbow_shimmer     = ftxui::Color::RGB(120,  80, 200),
    //   diffAdded=rgb(105,219,124)  diffRemoved=rgb(255,168,180)
    //   diffAddedWord=rgb(47,157,68)  diffRemovedWord=rgb(209,69,75)
    .diff_added          = ftxui::Color::RGB(105, 219, 124),
    .diff_removed        = ftxui::Color::RGB(255, 168, 180),
    .diff_added_word     = ftxui::Color::RGB( 47, 157,  68),
    .diff_removed_word   = ftxui::Color::RGB(209,  69,  75),
    .merged              = ftxui::Color::RGB(135,   0, 255),
    // Prompt chrome:
    //   light bash_border            = rgb(255,  0, 135)   same saturated pink
    //   light prompt_border        = rgb(153,153, 153)   ansi:whiteBright
    .bash_border         = ftxui::Color::RGB(255,   0, 135),
    .prompt_border       = ftxui::Color::RGB(153, 153, 153),
    .permission          = ftxui::Color::RGB( 87, 105, 247),
    .auto_accept         = ftxui::Color::RGB(135,   0, 255),
    .loom_body          = CLAWDED,
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color::RGB(137, 155, 255),
    .inactive_shimmer       = ftxui::Color::RGB(142, 142, 142),
    .warning_shimmer        = ftxui::Color::RGB(200, 158,  80),
    .prompt_border_shimmer  = ftxui::Color::RGB(183, 183, 183),
    .fast_mode_shimmer      = ftxui::Color::RGB(255, 150,  50),
    .loom_blue_shimmer    = ftxui::Color::RGB(117, 135, 255),
    .rainbow_shimmer_stops  = dark().rainbow_shimmer_stops,         // identical across all themes
    // ── Loom blue system spinner ──
    .loom_blue           = ftxui::Color::RGB( 87, 105, 247),
    // ── Mode colors ──
    .plan_mode             = ftxui::Color::RGB(  0, 102, 102),
    .ide                   = ftxui::Color::RGB( 71, 130, 200),
    .remember              = ftxui::Color::RGB(  0,   0, 255),
    .fast_mode             = ftxui::Color::RGB(255, 106,   0),
    // ── Diff dimmed ──
    .diff_added_dimmed     = ftxui::Color::RGB(199, 225, 203),
    .diff_removed_dimmed   = ftxui::Color::RGB(253, 210, 216),
    // ── Subagent colors (light theme 148-155 — same Tailwind 600 values) ──
    .subagent_red          = ftxui::Color::RGB(220,  38,  38),
    .subagent_blue         = ftxui::Color::RGB( 37,  99, 235),
    .subagent_green        = ftxui::Color::RGB( 22, 163,  74),
    .subagent_yellow       = ftxui::Color::RGB(202, 138,   4),
    .subagent_purple       = ftxui::Color::RGB(147,  51, 234),
    .subagent_orange       = ftxui::Color::RGB(234,  88,  12),
    .subagent_pink         = ftxui::Color::RGB(219,  39, 119),
    .subagent_cyan         = ftxui::Color::RGB(  8, 145, 178),
    // ── Misc ──
    .professional_blue     = ftxui::Color::RGB(106, 155, 204),
    .chrome_yellow         = ftxui::Color::RGB(251, 188,   4),
    .clawd_background      = ftxui::Color::RGB(  0,   0,   0),
    .selection_bg          = ftxui::Color::RGB(180, 213, 255),
    .bash_message_background = ftxui::Color::RGB(250, 245, 250),
    .memory_background     = ftxui::Color::RGB(230, 245, 250),
    .brief_label_you       = ftxui::Color::RGB( 37,  99, 235),
    .brief_label_loom    = CLAWDED,
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color::RGB(235,  95,  87),
    .rainbow_orange        = ftxui::Color::RGB(245, 139,  87),
    .rainbow_yellow        = ftxui::Color::RGB(250, 195,  95),
    .rainbow_green         = ftxui::Color::RGB(145, 200, 130),
    .rainbow_blue          = ftxui::Color::RGB(130, 170, 220),
    .rainbow_indigo        = ftxui::Color::RGB(155, 130, 200),
    .rainbow_violet        = ftxui::Color::RGB(200, 130, 180),
    .rainbow_red_shimmer   = ftxui::Color::RGB(250, 155, 147),
    .rainbow_orange_shimmer= ftxui::Color::RGB(255, 185, 137),
    .rainbow_yellow_shimmer= ftxui::Color::RGB(255, 225, 155),
    .rainbow_green_shimmer = ftxui::Color::RGB(185, 230, 180),
    .rainbow_blue_shimmer  = ftxui::Color::RGB(180, 205, 240),
    .rainbow_indigo_shimmer= ftxui::Color::RGB(195, 180, 230),
    .rainbow_violet_shimmer= ftxui::Color::RGB(230, 180, 210),
    // ── Semantic derived tokens (light theme) ──
    .text_muted            = ftxui::Color::RGB(102, 102, 102),  // = muted
    .text_link             = ftxui::Color::RGB( 87, 105, 247),  // = suggestion
    .text_accent           = CLAWDED,                            // = primary
    .border_subtle         = ftxui::Color::RGB(175, 175, 175),  // = subtle
    .border_default        = ftxui::Color::RGB(153, 153, 153),  // = prompt_border
    .border_accent         = ftxui::Color::RGB( 87, 105, 247),  // = permission
    .border_error          = ftxui::Color::RGB(171,  43,  63),  // = danger
    .surface_hover         = ftxui::Color::RGB(252, 252, 252),  // = user_message_background_hover
    .surface_selected      = ftxui::Color::RGB(232, 236, 244),  // = message_actions_background
    .surface_bash          = ftxui::Color::RGB(250, 245, 250),  // = bash_message_background
    .surface_memory        = ftxui::Color::RGB(230, 245, 250),  // = memory_background
    .icon_default          = ftxui::Color::RGB(  0,   0,   0),  // = text (black on light)
    .icon_muted            = ftxui::Color::RGB(102, 102, 102),  // = muted
    .icon_accent           = CLAWDED,                            // = primary
    .status_bar_background = ftxui::Color::RGB(240, 240, 240),
    .spinner_gold          = ftxui::Color::RGB(180, 120,  40),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB(219, 234, 254),
    .role_bg_assistant       = ftxui::Color::RGB(237, 233, 254),
    .role_bg_system          = ftxui::Color::RGB(243, 244, 246),
    .role_bg_tool            = ftxui::Color::RGB(204, 251, 241),
    .role_bg_thinking        = ftxui::Color::RGB(237, 233, 254),
    .role_pill_user          = ftxui::Color::RGB( 37,  99, 235),
    .role_pill_assistant     = ftxui::Color::RGB(147,  51, 234),
    .role_pill_system        = ftxui::Color::RGB(202, 138,   4),
    .role_pill_tool          = ftxui::Color::RGB( 13, 148, 136),
    .role_pill_thinking      = ftxui::Color::RGB(124,  58, 237),
    .message_error_accent    = ftxui::Color::RGB(220,  38,  38),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB(191, 219, 254),
    .message_list_muted_fg      = ftxui::Color::RGB(107, 114, 128),
    .message_list_empty_state_fg = ftxui::Color::RGB(156, 163, 175),
    .message_list_streaming_fg  = ftxui::Color::RGB(  8, 145, 178),
    .surface_tint_1           = ftxui::Color::RGB(230, 235, 245),
    .surface_tint_2           = ftxui::Color::RGB(225, 232, 242),
    .surface_tint_3           = ftxui::Color::RGB(235, 238, 248),
    .surface_tint_4           = ftxui::Color::RGB(232, 236, 245),
    .surface_tint_5           = ftxui::Color::RGB(235, 238, 248),
    .success_tint_bg          = ftxui::Color::RGB(220, 245, 225),
    .danger_tint_bg           = ftxui::Color::RGB(250, 225, 228),
    .warning_tint_bg          = ftxui::Color::RGB(250, 240, 210),
    .info_tint_bg             = ftxui::Color::RGB(225, 235, 250),
    .accent_green             = ftxui::Color::RGB( 80, 200, 120),
    .accent_purple            = ftxui::Color::RGB(120,  80, 120),
    .accent_pink              = ftxui::Color::RGB(255,   0, 135),
    .stats_green_1            = ftxui::Color::RGB( 60, 100,  60),
    .stats_green_2            = ftxui::Color::RGB( 80, 140,  80),
    .stats_green_3            = ftxui::Color::RGB(100, 180, 100),
    .stats_green_4            = ftxui::Color::RGB(120, 220, 120),
    .stats_green_5            = ftxui::Color::RGB(140, 255, 140),
    .perm_rule_bg             = ftxui::Color::RGB(240, 240, 242),
    .perm_rule_bg_alt         = ftxui::Color::RGB(220, 220, 225),
    .perm_rule_bg_hover       = ftxui::Color::RGB(230, 235, 245),
    .perm_rule_bg_selected    = ftxui::Color::RGB(228, 230, 234),
    .perm_rule_bg_info        = ftxui::Color::RGB(225, 240, 250),
    .perm_rule_bg_dark        = ftxui::Color::RGB(232, 234, 238),
    .perm_rule_bg_darker      = ftxui::Color::RGB(238, 238, 242),
    .perm_rule_bg_active      = ftxui::Color::RGB(228, 230, 240),
    .perm_rule_bg_accent      = ftxui::Color::RGB(235, 238, 248),
};
    return p;
}

// Daltonized variants (deuteranopia-safe) use EXPLICIT rgb() literals,
// NOT a matrix approximation — ported verbatim here (clr-daltonized-27-p1).
inline const Palette& dark_daltonized() noexcept {
    static const Palette p = {
    .primary             = ftxui::Color::RGB(255, 153,  51),
    .primary_shimmer     = ftxui::Color::RGB(255, 183, 101),  // loomShimmer
    // info/suggestion/permission all = rgb(153,204,255) light blue.
    .info                = ftxui::Color::RGB(153, 204, 255),
    .success             = ftxui::Color::RGB( 51, 153, 255),  // blue, not green
    .warning             = ftxui::Color::RGB(255, 204,   0),  // yellow-orange
    .danger              = ftxui::Color::RGB(255, 102, 102),  // error
    .muted               = ftxui::Color::RGB(153, 153, 153),  // inactive
    .subtle              = ftxui::Color::RGB( 80,  80,  80),
    .suggestion          = ftxui::Color::RGB(153, 204, 255),
    .text                = ftxui::Color::RGB(255, 255, 255),
    .inverse_text        = ftxui::Color::RGB(  0,   0,   0),
    .background          = dark().background,
    .chrome              = dark().chrome,
    .rate_limit_fill     = ftxui::Color::RGB(153, 204, 255),
    .rate_limit_empty    = ftxui::Color::RGB( 69,  92, 115),
    .brief_label         = ftxui::Color::RGB(255, 153,  51),
    // Daltonized: reuse dark-daltonized base bubble chromes (same
    // userMessageBackground/messageActionsBackground as dark).
    .user_message_background        = dark().user_message_background,
    .user_message_background_hover  = dark().user_message_background_hover,
    .message_actions_background     = dark().message_actions_background,
    // daltonized — alias the corrected dark gradient (clr-rainbow-28-p1).
    .rainbow = dark().rainbow,
    .rainbow_shimmer     = dark().rainbow_shimmer,
    //   diffAdded=rgb(0,68,102)  diffRemoved=rgb(102,0,0)
    //   diffAddedWord=rgb(0,119,179)  diffRemovedWord=rgb(179,0,0)
    .diff_added          = ftxui::Color::RGB(  0,  68, 102),
    .diff_removed        = ftxui::Color::RGB(102,   0,   0),
    .diff_added_word     = ftxui::Color::RGB(  0, 119, 179),
    .diff_removed_word   = ftxui::Color::RGB(179,   0,   0),
    .merged              = ftxui::Color::RGB(175, 135, 255),  // == autoAccept
    // Prompt chrome — dark daltonized: bash_border
    //   = rgb(51,153,255) bright blue (not pink — CVD-safe). prompt_border = dark gray.
    .bash_border         = ftxui::Color::RGB( 51, 153, 255),
    .prompt_border       = ftxui::Color::RGB(136, 136, 136),
    .permission          = ftxui::Color::RGB(153, 204, 255),
    .auto_accept         = ftxui::Color::RGB(175, 135, 255),
    // orange (unlike `primary`/`loom` which shifts to rgb(255,153,51) for CVD).
    .loom_body          = CLAWDED,
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color::RGB(183, 224, 255),
    .inactive_shimmer       = ftxui::Color::RGB(193, 193, 193),
    .warning_shimmer        = ftxui::Color::RGB(255, 234,  50),
    .prompt_border_shimmer  = ftxui::Color::RGB(166, 166, 166),
    .fast_mode_shimmer      = ftxui::Color::RGB(255, 165,  70),
    .loom_blue_shimmer    = ftxui::Color::RGB(183, 224, 255),
    .rainbow_shimmer_stops  = dark().rainbow_shimmer_stops,         // identical across all themes
    // ── Loom blue system spinner ──
    .loom_blue           = ftxui::Color::RGB(153, 204, 255),
    // ── Mode colors ──
    .plan_mode             = ftxui::Color::RGB(102, 153, 153),
    .ide                   = ftxui::Color::RGB( 71, 130, 200),
    .remember              = ftxui::Color::RGB(153, 204, 255),
    .fast_mode             = ftxui::Color::RGB(255, 120,  20),
    // ── Diff dimmed ──
    .diff_added_dimmed     = ftxui::Color::RGB( 62,  81,  91),
    .diff_removed_dimmed   = ftxui::Color::RGB( 62,  44,  44),
    // ── Subagent colors (darkDaltonizedTheme 554-561 — bright CVD-safe) ──
    .subagent_red          = ftxui::Color::RGB(255, 102, 102),  // bright red
    .subagent_blue         = ftxui::Color::RGB(102, 178, 255),  // bright blue
    .subagent_green        = ftxui::Color::RGB(102, 255, 102),  // bright green
    .subagent_yellow       = ftxui::Color::RGB(255, 255, 102),  // bright yellow
    .subagent_purple       = ftxui::Color::RGB(178, 102, 255),  // bright purple
    .subagent_orange       = ftxui::Color::RGB(255, 178, 102),  // bright orange
    .subagent_pink         = ftxui::Color::RGB(255, 153, 204),  // bright pink
    .subagent_cyan         = ftxui::Color::RGB(102, 204, 204),  // bright cyan
    // ── Misc ──
    .professional_blue     = ftxui::Color::RGB(106, 155, 204),
    .chrome_yellow         = ftxui::Color::RGB(251, 188,   4),
    .clawd_background      = ftxui::Color::RGB(  0,   0,   0),
    .selection_bg          = ftxui::Color::RGB( 38,  79, 120),
    .bash_message_background = ftxui::Color::RGB( 65,  60,  65),
    .memory_background     = ftxui::Color::RGB( 55,  65,  70),
    .brief_label_you       = ftxui::Color::RGB(122, 180, 232),
    .brief_label_loom    = ftxui::Color::RGB(255, 153,  51),
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color::RGB(235,  95,  87),
    .rainbow_orange        = ftxui::Color::RGB(245, 139,  87),
    .rainbow_yellow        = ftxui::Color::RGB(250, 195,  95),
    .rainbow_green         = ftxui::Color::RGB(145, 200, 130),
    .rainbow_blue          = ftxui::Color::RGB(130, 170, 220),
    .rainbow_indigo        = ftxui::Color::RGB(155, 130, 200),
    .rainbow_violet        = ftxui::Color::RGB(200, 130, 180),
    .rainbow_red_shimmer   = ftxui::Color::RGB(250, 155, 147),
    .rainbow_orange_shimmer= ftxui::Color::RGB(255, 185, 137),
    .rainbow_yellow_shimmer= ftxui::Color::RGB(255, 225, 155),
    .rainbow_green_shimmer = ftxui::Color::RGB(185, 230, 180),
    .rainbow_blue_shimmer  = ftxui::Color::RGB(180, 205, 240),
    .rainbow_indigo_shimmer= ftxui::Color::RGB(195, 180, 230),
    .rainbow_violet_shimmer= ftxui::Color::RGB(230, 180, 210),
    // ── Semantic derived tokens (dark daltonized) ──
    .text_muted            = ftxui::Color::RGB(153, 153, 153),  // = muted
    .text_link             = ftxui::Color::RGB(153, 204, 255),  // = suggestion
    .text_accent           = ftxui::Color::RGB(255, 153,  51),  // = primary
    .border_subtle         = ftxui::Color::RGB( 55,  57,  61),  // = chrome
    .border_default        = ftxui::Color::RGB(136, 136, 136),  // = prompt_border
    .border_accent         = ftxui::Color::RGB(153, 204, 255),  // = permission
    .border_error          = ftxui::Color::RGB(255, 102, 102),  // = danger
    .surface_hover         = ftxui::Color::RGB( 70,  70,  70),  // = user_message_background_hover
    .surface_selected      = ftxui::Color::RGB( 44,  50,  62),  // = message_actions_background
    .surface_bash          = ftxui::Color::RGB( 65,  60,  65),  // = bash_message_background
    .surface_memory        = ftxui::Color::RGB( 55,  65,  70),  // = memory_background
    .icon_default          = ftxui::Color::RGB(255, 255, 255),  // = text (white on dark)
    .icon_muted            = ftxui::Color::RGB(153, 153, 153),  // = muted
    .icon_accent           = ftxui::Color::RGB(255, 153,  51),  // = primary (loom daltonized)
    .status_bar_background = ftxui::Color::RGB( 20,  20,  22),
    .spinner_gold          = ftxui::Color::RGB(217, 154,  56),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_assistant       = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_system          = ftxui::Color::RGB( 30,  30,  36),
    .role_bg_tool            = ftxui::Color::RGB( 24,  40,  40),
    .role_bg_thinking        = ftxui::Color::RGB( 34,  30,  48),
    .role_pill_user          = ftxui::Color::RGB( 59, 130, 246),
    .role_pill_assistant     = ftxui::Color::RGB(168,  85, 247),
    .role_pill_system        = ftxui::Color::RGB(234, 179,   8),
    .role_pill_tool          = ftxui::Color::RGB( 20, 184, 166),
    .role_pill_thinking      = ftxui::Color::RGB(139,  92, 246),
    .message_error_accent    = ftxui::Color::RGB(255, 159,  67),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB( 30,  64, 175),
    .message_list_muted_fg      = ftxui::Color::RGB(156, 163, 175),
    .message_list_empty_state_fg = ftxui::Color::RGB(107, 114, 128),
    .message_list_streaming_fg  = ftxui::Color::RGB( 34, 211, 238),
    .surface_tint_1           = ftxui::Color::RGB( 25,  30,  45),
    .surface_tint_2           = ftxui::Color::RGB( 30,  40,  55),
    .surface_tint_3           = ftxui::Color::RGB( 20,  30,  50),
    .surface_tint_4           = ftxui::Color::RGB( 25,  35,  50),
    .surface_tint_5           = ftxui::Color::RGB( 20,  30,  55),
    .success_tint_bg          = ftxui::Color::RGB( 16,  36,  24),
    .danger_tint_bg           = ftxui::Color::RGB( 40,  16,  20),
    .warning_tint_bg          = ftxui::Color::RGB( 40,  30,   0),
    .info_tint_bg             = ftxui::Color::RGB( 30,  40,  60),
    .accent_green             = ftxui::Color::RGB( 80, 200, 120),
    .accent_purple            = ftxui::Color::RGB(120,  80, 120),
    .accent_pink              = ftxui::Color::RGB(255,   0, 135),
    .stats_green_1            = ftxui::Color::RGB( 60, 100,  60),
    .stats_green_2            = ftxui::Color::RGB( 80, 140,  80),
    .stats_green_3            = ftxui::Color::RGB(100, 180, 100),
    .stats_green_4            = ftxui::Color::RGB(120, 220, 120),
    .stats_green_5            = ftxui::Color::RGB(140, 255, 140),
    .perm_rule_bg             = ftxui::Color::RGB( 18,  18,  22),
    .perm_rule_bg_alt         = ftxui::Color::RGB( 40,  40,  46),
    .perm_rule_bg_hover       = ftxui::Color::RGB( 28,  34,  46),
    .perm_rule_bg_selected    = ftxui::Color::RGB( 30,  32,  36),
    .perm_rule_bg_info        = ftxui::Color::RGB( 28,  48,  62),
    .perm_rule_bg_dark        = ftxui::Color::RGB( 26,  28,  32),
    .perm_rule_bg_darker      = ftxui::Color::RGB( 20,  20,  24),
    .perm_rule_bg_active      = ftxui::Color::RGB( 30,  32,  42),
    .perm_rule_bg_accent      = ftxui::Color::RGB( 20,  28,  48),
};
    return p;
}

inline const Palette& light_daltonized() noexcept {
    static const Palette p = {
    .primary             = ftxui::Color::RGB(255, 153,  51),  // loom (deuteranopia)
    .primary_shimmer     = ftxui::Color::RGB(255, 183, 101),  // loomShimmer
    .info                = ftxui::Color::RGB( 51, 102, 255),  // permission/suggestion
    .success             = ftxui::Color::RGB(  0, 102, 153),  // blue, not green
    .warning             = ftxui::Color::RGB(255, 153,   0),  // orange
    .danger              = ftxui::Color::RGB(204,   0,   0),  // pure red (error)
    .muted               = ftxui::Color::RGB(102, 102, 102),  // inactive
    .subtle              = ftxui::Color::RGB(175, 175, 175),
    .suggestion          = ftxui::Color::RGB( 51, 102, 255),
    .text                = ftxui::Color::RGB(  0,   0,   0),
    .inverse_text        = ftxui::Color::RGB(255, 255, 255),
    .background          = light().background,
    .chrome              = light().chrome,
    .rate_limit_fill     = ftxui::Color::RGB( 51, 102, 255),
    .rate_limit_empty    = ftxui::Color::RGB( 23,  46, 114),
    .brief_label         = ftxui::Color::RGB(255, 153,  51),
    .user_message_background        = light().user_message_background,
    .user_message_background_hover  = light().user_message_background_hover,
    .message_actions_background     = light().message_actions_background,
    .rainbow             = dark().rainbow,   // identical across all themes
    .rainbow_shimmer     = light().rainbow_shimmer,
    // Light-daltonized diffs: light blue / light red (deuteranopia-safe).
    //   diffAdded=rgb(153,204,255)  diffRemoved=rgb(255,204,204)
    //   diffAddedWord=rgb(51,102,204)  diffRemovedWord=rgb(153,51,51)
    .diff_added          = ftxui::Color::RGB(153, 204, 255),
    .diff_removed        = ftxui::Color::RGB(255, 204, 204),
    .diff_added_word     = ftxui::Color::RGB( 51, 102, 204),
    .diff_removed_word   = ftxui::Color::RGB(153,  51,  51),
    .merged              = ftxui::Color::RGB(135,   0, 255),  // == autoAccept
    // Prompt chrome — light daltonized: bash_border
    //   rgb(0,102,204) medium blue. prompt_border = light mid-gray.
    .bash_border         = ftxui::Color::RGB(  0, 102, 204),
    .prompt_border       = ftxui::Color::RGB(153, 153, 153),
    .permission          = ftxui::Color::RGB( 51, 102, 255),
    .auto_accept         = ftxui::Color::RGB(135,   0, 255),
    // orange (unlike `primary`/`loom` which shifts to rgb(255,153,51) for CVD).
    .loom_body          = CLAWDED,
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color::RGB(101, 152, 255),
    .inactive_shimmer       = ftxui::Color::RGB(142, 142, 142),
    .warning_shimmer        = ftxui::Color::RGB(255, 183,  50),
    .prompt_border_shimmer  = ftxui::Color::RGB(183, 183, 183),
    .fast_mode_shimmer      = ftxui::Color::RGB(255, 150,  50),
    .loom_blue_shimmer    = ftxui::Color::RGB(101, 152, 255),
    .rainbow_shimmer_stops  = dark().rainbow_shimmer_stops,         // identical across all themes
    // ── Loom blue system spinner ──
    .loom_blue           = ftxui::Color::RGB( 51, 102, 255),
    // ── Mode colors ──
    .plan_mode             = ftxui::Color::RGB( 51, 102, 102),
    .ide                   = ftxui::Color::RGB( 71, 130, 200),
    .remember              = ftxui::Color::RGB( 51, 102, 255),
    .fast_mode             = ftxui::Color::RGB(255, 106,   0),
    // ── Diff dimmed ──
    .diff_added_dimmed     = ftxui::Color::RGB(209, 231, 253),
    .diff_removed_dimmed   = ftxui::Color::RGB(255, 233, 233),
    // ── Subagent colors (lightDaltonizedTheme 392-399 — pure CVD-safe) ──
    .subagent_red          = ftxui::Color::RGB(204,   0,   0),  // pure red
    .subagent_blue         = ftxui::Color::RGB(  0, 102, 204),  // pure blue
    .subagent_green        = ftxui::Color::RGB(  0, 204,   0),  // pure green
    .subagent_yellow       = ftxui::Color::RGB(255, 204,   0),  // golden yellow
    .subagent_purple       = ftxui::Color::RGB(128,   0, 128),  // true purple
    .subagent_orange       = ftxui::Color::RGB(255, 128,   0),  // true orange
    .subagent_pink         = ftxui::Color::RGB(255, 102, 178),  // adjusted pink
    .subagent_cyan         = ftxui::Color::RGB(  0, 178, 178),  // adjusted cyan
    // ── Misc ──
    .professional_blue     = ftxui::Color::RGB(106, 155, 204),
    .chrome_yellow         = ftxui::Color::RGB(251, 188,   4),
    .clawd_background      = ftxui::Color::RGB(  0,   0,   0),
    .selection_bg          = ftxui::Color::RGB(180, 213, 255),
    .bash_message_background = ftxui::Color::RGB(250, 245, 250),
    .memory_background     = ftxui::Color::RGB(230, 245, 250),
    .brief_label_you       = ftxui::Color::RGB( 37,  99, 235),
    .brief_label_loom    = ftxui::Color::RGB(255, 153,  51),
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color::RGB(235,  95,  87),
    .rainbow_orange        = ftxui::Color::RGB(245, 139,  87),
    .rainbow_yellow        = ftxui::Color::RGB(250, 195,  95),
    .rainbow_green         = ftxui::Color::RGB(145, 200, 130),
    .rainbow_blue          = ftxui::Color::RGB(130, 170, 220),
    .rainbow_indigo        = ftxui::Color::RGB(155, 130, 200),
    .rainbow_violet        = ftxui::Color::RGB(200, 130, 180),
    .rainbow_red_shimmer   = ftxui::Color::RGB(250, 155, 147),
    .rainbow_orange_shimmer= ftxui::Color::RGB(255, 185, 137),
    .rainbow_yellow_shimmer= ftxui::Color::RGB(255, 225, 155),
    .rainbow_green_shimmer = ftxui::Color::RGB(185, 230, 180),
    .rainbow_blue_shimmer  = ftxui::Color::RGB(180, 205, 240),
    .rainbow_indigo_shimmer= ftxui::Color::RGB(195, 180, 230),
    .rainbow_violet_shimmer= ftxui::Color::RGB(230, 180, 210),
    // ── Semantic derived tokens (light daltonized) ──
    .text_muted            = ftxui::Color::RGB(102, 102, 102),  // = muted
    .text_link             = ftxui::Color::RGB( 51, 102, 255),  // = suggestion
    .text_accent           = ftxui::Color::RGB(255, 153,  51),  // = primary
    .border_subtle         = ftxui::Color::RGB(175, 175, 175),  // = subtle
    .border_default        = ftxui::Color::RGB(153, 153, 153),  // = prompt_border
    .border_accent         = ftxui::Color::RGB( 51, 102, 255),  // = permission
    .border_error          = ftxui::Color::RGB(204,   0,   0),  // = danger
    .surface_hover         = light().surface_hover,               // = user_message_background_hover
    .surface_selected      = light().surface_selected,            // = message_actions_background
    .surface_bash          = ftxui::Color::RGB(250, 245, 250),  // = bash_message_background
    .surface_memory        = ftxui::Color::RGB(230, 245, 250),  // = memory_background
    .icon_default          = ftxui::Color::RGB(  0,   0,   0),  // = text (black on light)
    .icon_muted            = ftxui::Color::RGB(102, 102, 102),  // = muted
    .icon_accent           = ftxui::Color::RGB(255, 153,  51),  // = primary (loom daltonized)
    .status_bar_background = ftxui::Color::RGB(240, 240, 240),
    .spinner_gold          = ftxui::Color::RGB(180, 120,  40),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB(219, 234, 254),
    .role_bg_assistant       = ftxui::Color::RGB(237, 233, 254),
    .role_bg_system          = ftxui::Color::RGB(243, 244, 246),
    .role_bg_tool            = ftxui::Color::RGB(204, 251, 241),
    .role_bg_thinking        = ftxui::Color::RGB(237, 233, 254),
    .role_pill_user          = ftxui::Color::RGB( 37,  99, 235),
    .role_pill_assistant     = ftxui::Color::RGB(147,  51, 234),
    .role_pill_system        = ftxui::Color::RGB(202, 138,   4),
    .role_pill_tool          = ftxui::Color::RGB( 13, 148, 136),
    .role_pill_thinking      = ftxui::Color::RGB(124,  58, 237),
    .message_error_accent    = ftxui::Color::RGB(234,  88,  12),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB(191, 219, 254),
    .message_list_muted_fg      = ftxui::Color::RGB(107, 114, 128),
    .message_list_empty_state_fg = ftxui::Color::RGB(156, 163, 175),
    .message_list_streaming_fg  = ftxui::Color::RGB(  8, 145, 178),
    .surface_tint_1           = ftxui::Color::RGB(230, 235, 245),
    .surface_tint_2           = ftxui::Color::RGB(225, 232, 242),
    .surface_tint_3           = ftxui::Color::RGB(235, 238, 248),
    .surface_tint_4           = ftxui::Color::RGB(232, 236, 245),
    .surface_tint_5           = ftxui::Color::RGB(235, 238, 248),
    .success_tint_bg          = ftxui::Color::RGB(220, 245, 225),
    .danger_tint_bg           = ftxui::Color::RGB(250, 225, 228),
    .warning_tint_bg          = ftxui::Color::RGB(250, 240, 210),
    .info_tint_bg             = ftxui::Color::RGB(225, 235, 250),
    .accent_green             = ftxui::Color::RGB( 80, 200, 120),
    .accent_purple            = ftxui::Color::RGB(120,  80, 120),
    .accent_pink              = ftxui::Color::RGB(255,   0, 135),
    .stats_green_1            = ftxui::Color::RGB( 60, 100,  60),
    .stats_green_2            = ftxui::Color::RGB( 80, 140,  80),
    .stats_green_3            = ftxui::Color::RGB(100, 180, 100),
    .stats_green_4            = ftxui::Color::RGB(120, 220, 120),
    .stats_green_5            = ftxui::Color::RGB(140, 255, 140),
    .perm_rule_bg             = ftxui::Color::RGB(240, 240, 242),
    .perm_rule_bg_alt         = ftxui::Color::RGB(220, 220, 225),
    .perm_rule_bg_hover       = ftxui::Color::RGB(230, 235, 245),
    .perm_rule_bg_selected    = ftxui::Color::RGB(228, 230, 234),
    .perm_rule_bg_info        = ftxui::Color::RGB(225, 240, 250),
    .perm_rule_bg_dark        = ftxui::Color::RGB(232, 234, 238),
    .perm_rule_bg_darker      = ftxui::Color::RGB(238, 238, 242),
    .perm_rule_bg_active      = ftxui::Color::RGB(228, 230, 240),
    .perm_rule_bg_accent      = ftxui::Color::RGB(235, 238, 248),
};
    return p;
}

// ── Light ANSI palette ──
// Uses only the 16 standard ANSI palette16 colors for terminals without
// true-color support.  ansi:* names mapped to ftxui::Color::Palette16.
inline const Palette& light_ansi() noexcept {
    static const Palette p = {
    .primary             = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .primary_shimmer     = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .info                = ftxui::Color{ftxui::Color::Palette16::Blue},
    .success             = ftxui::Color{ftxui::Color::Palette16::Green},
    .warning             = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .danger              = ftxui::Color{ftxui::Color::Palette16::Red},
    .muted               = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .subtle              = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .suggestion          = ftxui::Color{ftxui::Color::Palette16::Blue},
    .text                = ftxui::Color{ftxui::Color::Palette16::Black},
    .inverse_text        = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .background          = ftxui::Color{ftxui::Color::Palette16::Cyan},
    .chrome              = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .rate_limit_fill     = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rate_limit_empty    = ftxui::Color{ftxui::Color::Palette16::Black},
    .brief_label         = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .user_message_background        = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .user_message_background_hover  = ftxui::Color{ftxui::Color::Palette16::White},
    .message_actions_background     = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .rainbow = {{
        ftxui::Color{ftxui::Color::Palette16::Red},
        ftxui::Color{ftxui::Color::Palette16::RedLight},
        ftxui::Color{ftxui::Color::Palette16::Yellow},
        ftxui::Color{ftxui::Color::Palette16::Green},
        ftxui::Color{ftxui::Color::Palette16::Cyan},
        ftxui::Color{ftxui::Color::Palette16::Blue},
        ftxui::Color{ftxui::Color::Palette16::Magenta},
    }},
    .rainbow_shimmer     = ftxui::Color{ftxui::Color::Palette16::White},
    .diff_added          = ftxui::Color{ftxui::Color::Palette16::Green},
    .diff_removed        = ftxui::Color{ftxui::Color::Palette16::Red},
    .diff_added_word     = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .diff_removed_word   = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .merged              = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .bash_border         = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .prompt_border       = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .permission          = ftxui::Color{ftxui::Color::Palette16::Blue},
    .auto_accept         = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .loom_body          = ftxui::Color{ftxui::Color::Palette16::RedLight},
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .inactive_shimmer       = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .warning_shimmer        = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .prompt_border_shimmer  = ftxui::Color{ftxui::Color::Palette16::White},
    .fast_mode_shimmer      = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .loom_blue_shimmer    = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .rainbow_shimmer_stops = {{
        ftxui::Color{ftxui::Color::Palette16::RedLight},
        ftxui::Color{ftxui::Color::Palette16::Yellow},
        ftxui::Color{ftxui::Color::Palette16::YellowLight},
        ftxui::Color{ftxui::Color::Palette16::GreenLight},
        ftxui::Color{ftxui::Color::Palette16::CyanLight},
        ftxui::Color{ftxui::Color::Palette16::BlueLight},
        ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    }},
    .loom_blue           = ftxui::Color{ftxui::Color::Palette16::Blue},
    .plan_mode             = ftxui::Color{ftxui::Color::Palette16::Cyan},
    .ide                   = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .remember              = ftxui::Color{ftxui::Color::Palette16::Blue},
    .fast_mode             = ftxui::Color{ftxui::Color::Palette16::Red},
    .diff_added_dimmed     = ftxui::Color{ftxui::Color::Palette16::Green},
    .diff_removed_dimmed   = ftxui::Color{ftxui::Color::Palette16::Red},
    // ── Subagent colors ──
    .subagent_red          = ftxui::Color{ftxui::Color::Palette16::Red},
    .subagent_blue         = ftxui::Color{ftxui::Color::Palette16::Blue},
    .subagent_green        = ftxui::Color{ftxui::Color::Palette16::Green},
    .subagent_yellow       = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .subagent_purple       = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .subagent_orange       = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .subagent_pink         = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .subagent_cyan         = ftxui::Color{ftxui::Color::Palette16::Cyan},
    // ── Misc ──
    .professional_blue     = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .chrome_yellow         = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .clawd_background      = ftxui::Color{ftxui::Color::Palette16::Black},
    .selection_bg          = ftxui::Color{ftxui::Color::Palette16::Cyan},
    .bash_message_background = ftxui::Color{ftxui::Color::Palette16::White},
    .memory_background     = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .brief_label_you       = ftxui::Color{ftxui::Color::Palette16::Blue},
    .brief_label_loom    = ftxui::Color{ftxui::Color::Palette16::RedLight},
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color{ftxui::Color::Palette16::Red},
    .rainbow_orange        = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .rainbow_yellow        = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rainbow_green         = ftxui::Color{ftxui::Color::Palette16::Green},
    .rainbow_blue          = ftxui::Color{ftxui::Color::Palette16::Cyan},
    .rainbow_indigo        = ftxui::Color{ftxui::Color::Palette16::Blue},
    .rainbow_violet        = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .rainbow_red_shimmer   = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .rainbow_orange_shimmer= ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rainbow_yellow_shimmer= ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .rainbow_green_shimmer = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .rainbow_blue_shimmer  = ftxui::Color{ftxui::Color::Palette16::CyanLight},
    .rainbow_indigo_shimmer= ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .rainbow_violet_shimmer= ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    // ── Semantic derived tokens (light ANSI) ──
    .text_muted            = ftxui::Color{ftxui::Color::Palette16::GrayDark},     // = muted
    .text_link             = ftxui::Color{ftxui::Color::Palette16::Blue},         // = suggestion
    .text_accent           = ftxui::Color{ftxui::Color::Palette16::RedLight},    // = primary
    .border_subtle         = ftxui::Color{ftxui::Color::Palette16::GrayDark},     // = subtle
    .border_default        = ftxui::Color{ftxui::Color::Palette16::GrayLight},        // = prompt_border
    .border_accent         = ftxui::Color{ftxui::Color::Palette16::Blue},         // = permission
    .border_error          = ftxui::Color{ftxui::Color::Palette16::Red},          // = danger
    .surface_hover         = ftxui::Color{ftxui::Color::Palette16::White},    // = user_message_background_hover
    .surface_selected      = ftxui::Color{ftxui::Color::Palette16::GrayLight},        // = message_actions_background
    .surface_bash          = ftxui::Color{ftxui::Color::Palette16::White},    // = bash_message_background
    .surface_memory        = ftxui::Color{ftxui::Color::Palette16::GrayLight},        // = memory_background
    .icon_default          = ftxui::Color{ftxui::Color::Palette16::Black},        // = text (black on light)
    .icon_muted            = ftxui::Color{ftxui::Color::Palette16::GrayDark},     // = muted
    .icon_accent           = ftxui::Color{ftxui::Color::Palette16::RedLight},    // = primary
    .status_bar_background = ftxui::Color::RGB(240, 240, 240),
    .spinner_gold          = ftxui::Color::RGB(153, 153, 153),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB(219, 234, 254),
    .role_bg_assistant       = ftxui::Color::RGB(237, 233, 254),
    .role_bg_system          = ftxui::Color::RGB(243, 244, 246),
    .role_bg_tool            = ftxui::Color::RGB(204, 251, 241),
    .role_bg_thinking        = ftxui::Color::RGB(237, 233, 254),
    .role_pill_user          = ftxui::Color::RGB( 37,  99, 235),
    .role_pill_assistant     = ftxui::Color::RGB(147,  51, 234),
    .role_pill_system        = ftxui::Color::RGB(202, 138,   4),
    .role_pill_tool          = ftxui::Color::RGB( 13, 148, 136),
    .role_pill_thinking      = ftxui::Color::RGB(124,  58, 237),
    .message_error_accent    = ftxui::Color::RGB(220,  38,  38),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB(191, 219, 254),
    .message_list_muted_fg      = ftxui::Color::RGB(107, 114, 128),
    .message_list_empty_state_fg = ftxui::Color::RGB(156, 163, 175),
    .message_list_streaming_fg  = ftxui::Color::RGB(  8, 145, 178),
    .surface_tint_1           = ftxui::Color::GrayDark,
    .surface_tint_2           = ftxui::Color::GrayDark,
    .surface_tint_3           = ftxui::Color::GrayDark,
    .surface_tint_4           = ftxui::Color::GrayDark,
    .surface_tint_5           = ftxui::Color::GrayDark,
    .success_tint_bg          = ftxui::Color::GrayDark,
    .danger_tint_bg           = ftxui::Color::GrayDark,
    .warning_tint_bg          = ftxui::Color::GrayDark,
    .info_tint_bg             = ftxui::Color::GrayDark,
    .accent_green             = ftxui::Color::White,
    .accent_purple            = ftxui::Color::White,
    .accent_pink              = ftxui::Color::White,
    .stats_green_1            = ftxui::Color::GrayDark,
    .stats_green_2            = ftxui::Color::GrayDark,
    .stats_green_3            = ftxui::Color::GrayLight,
    .stats_green_4            = ftxui::Color::GrayLight,
    .stats_green_5            = ftxui::Color::White,
    .perm_rule_bg             = ftxui::Color::GrayDark,
    .perm_rule_bg_alt         = ftxui::Color::GrayDark,
    .perm_rule_bg_hover       = ftxui::Color::GrayLight,
    .perm_rule_bg_selected    = ftxui::Color::GrayLight,
    .perm_rule_bg_info        = ftxui::Color::GrayDark,
    .perm_rule_bg_dark        = ftxui::Color::GrayDark,
    .perm_rule_bg_darker      = ftxui::Color::Black,
    .perm_rule_bg_active      = ftxui::Color::GrayLight,
    .perm_rule_bg_accent      = ftxui::Color::GrayDark,
};
    return p;
}

// ── Dark ANSI palette ──
// Uses only the 16 standard ANSI palette16 colors.
inline const Palette& dark_ansi() noexcept {
    static const Palette p = {
    .primary             = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .primary_shimmer     = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .info                = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .success             = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .warning             = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .danger              = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .muted               = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .subtle              = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .suggestion          = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .text                = ftxui::Color{ftxui::Color::Palette16::White},
    .inverse_text        = ftxui::Color{ftxui::Color::Palette16::Black},
    .background          = ftxui::Color{ftxui::Color::Palette16::CyanLight},
    .chrome              = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .rate_limit_fill     = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rate_limit_empty    = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .brief_label         = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .user_message_background        = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .user_message_background_hover  = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .message_actions_background     = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .rainbow = {{
        ftxui::Color{ftxui::Color::Palette16::Red},
        ftxui::Color{ftxui::Color::Palette16::RedLight},
        ftxui::Color{ftxui::Color::Palette16::Yellow},
        ftxui::Color{ftxui::Color::Palette16::Green},
        ftxui::Color{ftxui::Color::Palette16::Cyan},
        ftxui::Color{ftxui::Color::Palette16::Blue},
        ftxui::Color{ftxui::Color::Palette16::Magenta},
    }},
    .rainbow_shimmer     = ftxui::Color{ftxui::Color::Palette16::White},
    .diff_added          = ftxui::Color{ftxui::Color::Palette16::Green},
    .diff_removed        = ftxui::Color{ftxui::Color::Palette16::Red},
    .diff_added_word     = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .diff_removed_word   = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .merged              = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .bash_border         = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .prompt_border       = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .permission          = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .auto_accept         = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .loom_body          = ftxui::Color{ftxui::Color::Palette16::RedLight},
    // ── Shimmer tokens ──
    .permission_shimmer     = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .inactive_shimmer       = ftxui::Color{ftxui::Color::Palette16::White},
    .warning_shimmer        = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .prompt_border_shimmer  = ftxui::Color{ftxui::Color::Palette16::White},
    .fast_mode_shimmer      = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .loom_blue_shimmer    = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .rainbow_shimmer_stops = {{
        ftxui::Color{ftxui::Color::Palette16::RedLight},
        ftxui::Color{ftxui::Color::Palette16::Yellow},
        ftxui::Color{ftxui::Color::Palette16::YellowLight},
        ftxui::Color{ftxui::Color::Palette16::GreenLight},
        ftxui::Color{ftxui::Color::Palette16::CyanLight},
        ftxui::Color{ftxui::Color::Palette16::BlueLight},
        ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    }},
    .loom_blue           = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .plan_mode             = ftxui::Color{ftxui::Color::Palette16::CyanLight},
    .ide                   = ftxui::Color{ftxui::Color::Palette16::Blue},
    .remember              = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .fast_mode             = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .diff_added_dimmed     = ftxui::Color{ftxui::Color::Palette16::Green},
    .diff_removed_dimmed   = ftxui::Color{ftxui::Color::Palette16::Red},
    // ── Subagent colors ──
    .subagent_red          = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .subagent_blue         = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .subagent_green        = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .subagent_yellow       = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .subagent_purple       = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .subagent_orange       = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .subagent_pink         = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .subagent_cyan         = ftxui::Color{ftxui::Color::Palette16::CyanLight},
    // ── Misc ──
    .professional_blue     = ftxui::Color::RGB(106, 155, 204),
    .chrome_yellow         = ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .clawd_background      = ftxui::Color{ftxui::Color::Palette16::Black},
    .selection_bg          = ftxui::Color{ftxui::Color::Palette16::Blue},
    .bash_message_background = ftxui::Color{ftxui::Color::Palette16::Black},
    .memory_background     = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .brief_label_you       = ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .brief_label_loom    = ftxui::Color{ftxui::Color::Palette16::RedLight},
    // ── Individual rainbow fields ──
    .rainbow_red           = ftxui::Color{ftxui::Color::Palette16::Red},
    .rainbow_orange        = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .rainbow_yellow        = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rainbow_green         = ftxui::Color{ftxui::Color::Palette16::Green},
    .rainbow_blue          = ftxui::Color{ftxui::Color::Palette16::Cyan},
    .rainbow_indigo        = ftxui::Color{ftxui::Color::Palette16::Blue},
    .rainbow_violet        = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .rainbow_red_shimmer   = ftxui::Color{ftxui::Color::Palette16::RedLight},
    .rainbow_orange_shimmer= ftxui::Color{ftxui::Color::Palette16::Yellow},
    .rainbow_yellow_shimmer= ftxui::Color{ftxui::Color::Palette16::YellowLight},
    .rainbow_green_shimmer = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .rainbow_blue_shimmer  = ftxui::Color{ftxui::Color::Palette16::CyanLight},
    .rainbow_indigo_shimmer= ftxui::Color{ftxui::Color::Palette16::BlueLight},
    .rainbow_violet_shimmer= ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    // ── Semantic derived tokens (dark ANSI) ──
    .text_muted            = ftxui::Color{ftxui::Color::Palette16::GrayLight},         // = muted
    .text_link             = ftxui::Color{ftxui::Color::Palette16::BlueLight},    // = suggestion
    .text_accent           = ftxui::Color{ftxui::Color::Palette16::RedLight},    // = primary
    .border_subtle         = ftxui::Color{ftxui::Color::Palette16::GrayLight},         // = subtle
    .border_default        = ftxui::Color{ftxui::Color::Palette16::GrayLight},         // = prompt_border
    .border_accent         = ftxui::Color{ftxui::Color::Palette16::BlueLight},    // = permission
    .border_error          = ftxui::Color{ftxui::Color::Palette16::RedLight},    // = danger
    .surface_hover         = ftxui::Color{ftxui::Color::Palette16::GrayLight},       // = user_message_background_hover
    .surface_selected      = ftxui::Color{ftxui::Color::Palette16::GrayDark},    // = message_actions_background
    .surface_bash          = ftxui::Color{ftxui::Color::Palette16::Black},      // = bash_message_background
    .surface_memory        = ftxui::Color{ftxui::Color::Palette16::GrayDark},     // = memory_background
    .icon_default          = ftxui::Color{ftxui::Color::Palette16::White},     // = text (white on dark)
    .icon_muted            = ftxui::Color{ftxui::Color::Palette16::GrayLight},         // = muted
    .icon_accent           = ftxui::Color{ftxui::Color::Palette16::RedLight},    // = primary
    .status_bar_background = ftxui::Color::RGB( 20,  20,  22),
    .spinner_gold          = ftxui::Color::RGB(217, 154,  56),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_assistant       = ftxui::Color::RGB( 30,  41,  59),
    .role_bg_system          = ftxui::Color::RGB( 30,  30,  36),
    .role_bg_tool            = ftxui::Color::RGB( 24,  40,  40),
    .role_bg_thinking        = ftxui::Color::RGB( 34,  30,  48),
    .role_pill_user          = ftxui::Color::RGB( 59, 130, 246),
    .role_pill_assistant     = ftxui::Color::RGB(168,  85, 247),
    .role_pill_system        = ftxui::Color::RGB(234, 179,   8),
    .role_pill_tool          = ftxui::Color::RGB( 20, 184, 166),
    .role_pill_thinking      = ftxui::Color::RGB(139,  92, 246),
    .message_error_accent    = ftxui::Color::RGB(239,  68,  68),
    .message_redacted_accent = ftxui::Color::RGB(107, 114, 128),
    .message_list_selected_bg   = ftxui::Color::RGB( 30,  64, 175),
    .message_list_muted_fg      = ftxui::Color::RGB(156, 163, 175),
    .message_list_empty_state_fg = ftxui::Color::RGB(107, 114, 128),
    .message_list_streaming_fg  = ftxui::Color::RGB( 34, 211, 238),
    .surface_tint_1           = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .surface_tint_2           = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .surface_tint_3           = ftxui::Color{ftxui::Color::Palette16::Black},
    .surface_tint_4           = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .surface_tint_5           = ftxui::Color{ftxui::Color::Palette16::Black},
    .success_tint_bg          = ftxui::Color{ftxui::Color::Palette16::Green},
    .danger_tint_bg           = ftxui::Color{ftxui::Color::Palette16::Red},
    .warning_tint_bg          = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .info_tint_bg             = ftxui::Color{ftxui::Color::Palette16::Blue},
    .accent_green             = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .accent_purple            = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .accent_pink              = ftxui::Color{ftxui::Color::Palette16::MagentaLight},
    .stats_green_1            = ftxui::Color{ftxui::Color::Palette16::Green},
    .stats_green_2            = ftxui::Color{ftxui::Color::Palette16::Green},
    .stats_green_3            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .stats_green_4            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .stats_green_5            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .perm_rule_bg             = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .perm_rule_bg_alt         = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_hover       = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_selected    = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_info        = ftxui::Color{ftxui::Color::Palette16::Blue},
    .perm_rule_bg_dark        = ftxui::Color{ftxui::Color::Palette16::GrayDark},
    .perm_rule_bg_darker      = ftxui::Color{ftxui::Color::Palette16::Black},
    .perm_rule_bg_active      = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_accent      = ftxui::Color{ftxui::Color::Palette16::Blue},
};
    return p;
}

// Monochrome palette: for reduced-color / braille-only terminals.
inline const Palette& monochrome() noexcept {
    static const Palette p = {
    .primary             = ftxui::Color::White,
    .primary_shimmer     = ftxui::Color::GrayLight,
    .info                = ftxui::Color::GrayLight,
    .success             = ftxui::Color::White,
    .warning             = ftxui::Color::White,
    .danger              = ftxui::Color::White,
    .muted               = ftxui::Color::GrayDark,
    .subtle              = ftxui::Color::GrayDark,
    .suggestion          = ftxui::Color::GrayLight,
    .text                = ftxui::Color::White,
    .inverse_text        = ftxui::Color::Black,
    .background          = ftxui::Color::Black,
    .chrome              = ftxui::Color::GrayDark,
    .rate_limit_fill     = ftxui::Color::White,
    .rate_limit_empty    = ftxui::Color::GrayDark,
    .brief_label         = ftxui::Color::White,
    .user_message_background        = ftxui::Color::GrayDark,
    .user_message_background_hover  = ftxui::Color::GrayLight,
    .message_actions_background     = ftxui::Color::GrayDark,
    .rainbow = {{ ftxui::Color::White, ftxui::Color::White, ftxui::Color::White,
                  ftxui::Color::White, ftxui::Color::White, ftxui::Color::White,
                  ftxui::Color::White }},
    .rainbow_shimmer     = ftxui::Color::GrayLight,
    .diff_added          = ftxui::Color::White,
    .diff_removed        = ftxui::Color::White,
    .diff_added_word     = ftxui::Color::White,
    .diff_removed_word   = ftxui::Color::White,
    .merged              = ftxui::Color::White,
    // Prompt chrome (monochrome): pure White for accent, GrayDark for frame.
    .bash_border         = ftxui::Color::White,
    .prompt_border       = ftxui::Color::GrayDark,
    .permission          = ftxui::Color::White,
    // Monochrome: auto_accept = White (electric violet → white in reduced color)
    .auto_accept         = ftxui::Color::White,
    // Monochrome: loom_body = White (logo body → white in monochrome)
    .loom_body          = ftxui::Color::White,
    // ── Shimmer tokens (monochrome: all GrayLight) ──
    .permission_shimmer     = ftxui::Color::GrayLight,
    .inactive_shimmer       = ftxui::Color::GrayLight,
    .warning_shimmer        = ftxui::Color::GrayLight,
    .prompt_border_shimmer  = ftxui::Color::GrayLight,
    .fast_mode_shimmer      = ftxui::Color::GrayLight,
    .loom_blue_shimmer    = ftxui::Color::GrayLight,
    .rainbow_shimmer_stops = {{ ftxui::Color::GrayLight, ftxui::Color::GrayLight,
                                 ftxui::Color::GrayLight, ftxui::Color::GrayLight,
                                 ftxui::Color::GrayLight, ftxui::Color::GrayLight,
                                 ftxui::Color::GrayLight }},
    // ── Loom blue system spinner (monochrome: White) ──
    .loom_blue           = ftxui::Color::White,
    // ── Mode colors (monochrome: all White for accents) ──
    .plan_mode             = ftxui::Color::White,
    .ide                   = ftxui::Color::White,
    .remember              = ftxui::Color::White,
    .fast_mode             = ftxui::Color::White,
    // ── Diff dimmed (monochrome: GrayDark) ──
    .diff_added_dimmed     = ftxui::Color::GrayDark,
    .diff_removed_dimmed   = ftxui::Color::GrayDark,
    // ── Subagent colors (monochrome: all White, distinguished by label only) ──
    .subagent_red          = ftxui::Color::White,
    .subagent_blue         = ftxui::Color::White,
    .subagent_green        = ftxui::Color::White,
    .subagent_yellow       = ftxui::Color::White,
    .subagent_purple       = ftxui::Color::White,
    .subagent_orange       = ftxui::Color::White,
    .subagent_pink         = ftxui::Color::White,
    .subagent_cyan         = ftxui::Color::White,
    // ── Misc (monochrome) ──
    .professional_blue     = ftxui::Color::White,
    .chrome_yellow         = ftxui::Color::White,
    .clawd_background      = ftxui::Color::Black,
    .selection_bg          = ftxui::Color::GrayDark,
    .bash_message_background = ftxui::Color::GrayDark,
    .memory_background     = ftxui::Color::GrayDark,
    .brief_label_you       = ftxui::Color::White,
    .brief_label_loom    = ftxui::Color::White,
    // ── Individual rainbow fields (monochrome: all White) ──
    .rainbow_red           = ftxui::Color::White,
    .rainbow_orange        = ftxui::Color::White,
    .rainbow_yellow        = ftxui::Color::White,
    .rainbow_green         = ftxui::Color::White,
    .rainbow_blue          = ftxui::Color::White,
    .rainbow_indigo        = ftxui::Color::White,
    .rainbow_violet        = ftxui::Color::White,
    .rainbow_red_shimmer   = ftxui::Color::GrayLight,
    .rainbow_orange_shimmer= ftxui::Color::GrayLight,
    .rainbow_yellow_shimmer= ftxui::Color::GrayLight,
    .rainbow_green_shimmer = ftxui::Color::GrayLight,
    .rainbow_blue_shimmer  = ftxui::Color::GrayLight,
    .rainbow_indigo_shimmer= ftxui::Color::GrayLight,
    .rainbow_violet_shimmer= ftxui::Color::GrayLight,
    // ── Semantic derived tokens (monochrome) ──
    .text_muted            = ftxui::Color::GrayDark,
    .text_link             = ftxui::Color::GrayLight,
    .text_accent           = ftxui::Color::White,
    .border_subtle         = ftxui::Color::GrayDark,
    .border_default        = ftxui::Color::GrayDark,
    .border_accent         = ftxui::Color::White,
    .border_error          = ftxui::Color::White,
    .surface_hover         = ftxui::Color::GrayLight,
    .surface_selected      = ftxui::Color::GrayDark,
    .surface_bash          = ftxui::Color::GrayDark,
    .surface_memory        = ftxui::Color::GrayDark,
    .icon_default          = ftxui::Color::White,
    .icon_muted            = ftxui::Color::GrayDark,
    .icon_accent           = ftxui::Color::White,
    .status_bar_background = ftxui::Color::RGB( 20,  20,  22),
    .spinner_gold          = ftxui::Color::RGB(180, 120,  40),
    // ── Message list role tokens ──
    .role_bg_user            = ftxui::Color::RGB( 50,  50,  50),
    .role_bg_assistant       = ftxui::Color::RGB( 50,  50,  50),
    .role_bg_system          = ftxui::Color::RGB( 40,  40,  40),
    .role_bg_tool            = ftxui::Color::RGB( 45,  45,  45),
    .role_bg_thinking        = ftxui::Color::RGB( 55,  55,  55),
    .role_pill_user          = ftxui::Color::RGB(200, 200, 200),
    .role_pill_assistant     = ftxui::Color::RGB(200, 200, 200),
    .role_pill_system        = ftxui::Color::RGB(180, 180, 180),
    .role_pill_tool          = ftxui::Color::RGB(180, 180, 180),
    .role_pill_thinking      = ftxui::Color::RGB(200, 200, 200),
    .message_error_accent    = ftxui::Color::RGB(220, 220, 220),
    .message_redacted_accent = ftxui::Color::RGB(140, 140, 140),
    .message_list_selected_bg   = ftxui::Color::RGB( 70,  70,  70),
    .message_list_muted_fg      = ftxui::Color::RGB(150, 150, 150),
    .message_list_empty_state_fg = ftxui::Color::RGB(130, 130, 130),
    .message_list_streaming_fg  = ftxui::Color::RGB(200, 200, 200),
    .surface_tint_1           = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .surface_tint_2           = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .surface_tint_3           = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .surface_tint_4           = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .surface_tint_5           = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .success_tint_bg          = ftxui::Color{ftxui::Color::Palette16::Green},
    .danger_tint_bg           = ftxui::Color{ftxui::Color::Palette16::Red},
    .warning_tint_bg          = ftxui::Color{ftxui::Color::Palette16::Yellow},
    .info_tint_bg             = ftxui::Color{ftxui::Color::Palette16::Blue},
    .accent_green             = ftxui::Color{ftxui::Color::Palette16::Green},
    .accent_purple            = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .accent_pink              = ftxui::Color{ftxui::Color::Palette16::Magenta},
    .stats_green_1            = ftxui::Color{ftxui::Color::Palette16::Green},
    .stats_green_2            = ftxui::Color{ftxui::Color::Palette16::Green},
    .stats_green_3            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .stats_green_4            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .stats_green_5            = ftxui::Color{ftxui::Color::Palette16::GreenLight},
    .perm_rule_bg             = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_alt         = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_hover       = ftxui::Color{ftxui::Color::Palette16::White},
    .perm_rule_bg_selected    = ftxui::Color{ftxui::Color::Palette16::White},
    .perm_rule_bg_info        = ftxui::Color{ftxui::Color::Palette16::Blue},
    .perm_rule_bg_dark        = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_darker      = ftxui::Color{ftxui::Color::Palette16::GrayLight},
    .perm_rule_bg_active      = ftxui::Color{ftxui::Color::Palette16::White},
    .perm_rule_bg_accent      = ftxui::Color{ftxui::Color::Palette16::Blue},
};
    return p;
}

} // namespace palette

// ─── Role → Color resolution ─────────────────────────────────────────────────
/// Resolve a semantic Role to its concrete ftxui::Color inside *palette*.
[[nodiscard]] inline ftxui::Color token_by_role(const Palette& pal, Role role) noexcept {
    switch (role) {
        case Role::Primary:    return pal.primary;
        case Role::Info:       return pal.info;
        case Role::Success:    return pal.success;
        case Role::Warning:    return pal.warning;
        case Role::Danger:     return pal.danger;
        case Role::Muted:      return pal.muted;
        case Role::Subtle:     return pal.subtle;
        case Role::Suggestion: return pal.suggestion;
        case Role::Permission: return pal.permission;
        case Role::RateLimit:  return pal.rate_limit_fill;
        case Role::Chrome:     return pal.chrome;
        case Role::Brief:      return pal.brief_label;
        case Role::Ultra:      return pal.rainbow_shimmer;
        case Role::UserMessageBackground:     return pal.user_message_background;
        case Role::UserMessageBackgroundHover:return pal.user_message_background_hover;
        // New roles (clr-missing-42-tokens-struct)
        case Role::PlanMode:       return pal.plan_mode;
        case Role::FastMode:       return pal.fast_mode;
        case Role::Remember:       return pal.remember;
        case Role::SelectionBg:    return pal.selection_bg;
        case Role::Ide:            return pal.ide;
        case Role::BashMessageBackground: return pal.bash_message_background;
        case Role::MemoryBackground:      return pal.memory_background;
        case Role::BriefLabelYou:         return pal.brief_label_you;
        case Role::BriefLabelLoom:      return pal.brief_label_loom;
        case Role::AutoAccept:            return pal.auto_accept;
        // Semantic derived roles (clr-missing-42-tokens-struct)
        case Role::TextMuted:             return pal.text_muted;
        case Role::TextLink:              return pal.text_link;
        case Role::TextAccent:            return pal.text_accent;
        case Role::BorderSubtle:          return pal.border_subtle;
        case Role::BorderDefault:         return pal.border_default;
        case Role::BorderAccent:          return pal.border_accent;
        case Role::BorderError:           return pal.border_error;
        case Role::SurfaceHover:          return pal.surface_hover;
        case Role::SurfaceSelected:       return pal.surface_selected;
        case Role::SurfaceBash:           return pal.surface_bash;
        case Role::SurfaceMemory:         return pal.surface_memory;
        case Role::IconDefault:           return pal.icon_default;
        case Role::IconMuted:             return pal.icon_muted;
        case Role::IconAccent:            return pal.icon_accent;
    }
    return pal.text;
}

// ─── Shading helpers ─────────────────────────────────────────────────────────
/// Lighten or darken an RGB color.  amount ∈ [-1, 1]; positive lightens.
/// If the input color is a palette index (not true RGB) it is returned
/// unchanged to avoid breaking ANSI-mode themes.
[[nodiscard]] inline ftxui::Color shade(ftxui::Color c, double amount) noexcept {
    // FTXUI's Color stores TrueColor as RGB inside .red()/.green()/.blue()
    // when Color::Print is not set.  The simplest portable approach is to
    // reconstruct via RGB() ourselves if the color is 24-bit capable.
    if (amount == 0.0) return c;

    // We only shade 24-bit colors.  Named / palette colors pass through.
    auto rgb = c.Print(false);
    // Heuristic: if the string is an "rgb(R,G,B)"-style output we can parse
    // it.  For simplicity do the shading via the FTXUI RGB constructor and
    // a manual clamp.
    // PHASE_5 NOTE: FTXUI v5 does not expose Color::red() accessors on the
    // public ABI; we therefore implement shading via re-parsing Print().
    // If the format is not "rgb(r,g,b)" just return c unmodified.
    if (rgb.size() < 6 || rgb.substr(0, 4) != "rgb(") return c;
    std::uint32_t r = 0, g = 0, b = 0;
    std::size_t i = 4;
    auto read_u8 = [&](std::uint32_t& out) {
        out = 0;
        while (i < rgb.size() && rgb[i] >= '0' && rgb[i] <= '9') {
            out = out * 10 + static_cast<std::uint32_t>(rgb[i] - '0');
            ++i;
        }
        if (i < rgb.size() && (rgb[i] == ',' || rgb[i] == ')')) ++i;
    };
    read_u8(r); read_u8(g); read_u8(b);

    auto apply = [amount](std::uint32_t v) -> std::uint8_t {
        double d = static_cast<double>(v);
        if (amount > 0) d = d + (255.0 - d) * amount;
        else            d = d * (1.0 + amount);
        if (d < 0)   d = 0;
        if (d > 255) d = 255;
        return static_cast<std::uint8_t>(std::lround(d));
    };
    return ftxui::Color::RGB(apply(r), apply(g), apply(b));
}

/// Shorthand: shade() with positive amount.
[[nodiscard]] inline ftxui::Color tint(ftxui::Color c, double amount) noexcept {
    return shade(c, std::clamp(amount, 0.0, 1.0));
}
[[nodiscard]] inline ftxui::Color tone(ftxui::Color c, double amount) noexcept {
    return shade(c, -std::clamp(amount, 0.0, 1.0));
}

// ─── Interpolate between two RGB colors (used by shimmer) ───────────────────
/// Linearly interpolate between a and b.  t ∈ [0,1].  Falls back to `a` for
/// non-RGB inputs.
[[nodiscard]] inline ftxui::Color interpolate(ftxui::Color a, ftxui::Color b, double t) noexcept {
    t = std::clamp(t, 0.0, 1.0);
    if (t == 0.0) return a;
    if (t == 1.0) return b;

    auto parse = [](ftxui::Color c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) -> bool {
        auto s = c.Print(false);
        if (s.size() < 6 || s.substr(0, 4) != "rgb(") return false;
        std::uint32_t rr = 0, gg = 0, bb = 0;
        std::size_t i = 4;
        auto rd = [&](std::uint32_t& out) {
            out = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                out = out * 10 + static_cast<std::uint32_t>(s[i] - '0');
                ++i;
            }
            if (i < s.size() && (s[i] == ',' || s[i] == ')')) ++i;
        };
        rd(rr); rd(gg); rd(bb);
        r = static_cast<std::uint8_t>(std::clamp<std::uint32_t>(rr, 0, 255));
        g = static_cast<std::uint8_t>(std::clamp<std::uint32_t>(gg, 0, 255));
        b = static_cast<std::uint8_t>(std::clamp<std::uint32_t>(bb, 0, 255));
        return true;
    };

    std::uint8_t ar{}, ag{}, ab{}, br{}, bg{}, bb{};
    if (!parse(a, ar, ag, ab) || !parse(b, br, bg, bb)) return a;

    auto lerp = [t](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
        double v = static_cast<double>(x) * (1.0 - t) + static_cast<double>(y) * t;
        return static_cast<std::uint8_t>(std::lround(v));
    };
    return ftxui::Color::RGB(lerp(ar, br), lerp(ag, bg), lerp(ab, bb));
}

// ─── HSL → RGB (used by hue sweep) ──────────────────────────────────────────
/// Simple HSL→RGB with S=0.70, L=0.60 tuned for the animated asterisk
/// hue sweep.
[[nodiscard]] inline ftxui::Color hue_to_rgb(double hue_deg) noexcept {
    double h = std::fmod(hue_deg, 360.0) / 60.0;
    if (h < 0) h += 6.0;
    constexpr double s = 0.70;
    constexpr double l = 0.60;
    double c = (1.0 - std::fabs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - std::fabs(std::fmod(h, 2.0) - 1.0));
    double m = l - c / 2.0;
    double r=0, g=0, b=0;
    if      (h < 1) { r = c; g = x; }
    else if (h < 2) { r = x; g = c; }
    else if (h < 3) { g = c; b = x; }
    else if (h < 4) { g = x; b = c; }
    else if (h < 5) { r = x; b = c; }
    else            { r = c; b = x; }
    auto to_u8 = [m](double v) -> std::uint8_t {
        double w = (v + m) * 255.0;
        if (w < 0)   w = 0;
        if (w > 255) w = 255;
        return static_cast<std::uint8_t>(std::lround(w));
    };
    return ftxui::Color::RGB(to_u8(r), to_u8(g), to_u8(b));
}

} // namespace loom::ui::design::tokens
