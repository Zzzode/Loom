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

const Palette& dark() noexcept;

const Palette& light() noexcept;

// Daltonized variants (deuteranopia-safe) use EXPLICIT rgb() literals,
// NOT a matrix approximation — ported verbatim here (clr-daltonized-27-p1).
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
