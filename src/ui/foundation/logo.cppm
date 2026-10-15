// ==========================================================================
// Loom logo module — condensed / compact / horizontal welcome logo.
// ==========================================================================

module;
#include <cstddef>
#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>

export module loom.ui.foundation.logo;

import std;
import loom.ui.foundation.design_figures;

import loom.ui.foundation.design_tokens;
import loom.ui.foundation.theme_provider;

export namespace loom::ui::logo {

using ftxui::bold;
using ftxui::bgcolor;
using ftxui::color;
using ftxui::dim;
using ftxui::Element;
using ftxui::Elements;
using ftxui::hbox;
using ftxui::size;
using ftxui::text;
using ftxui::vbox;
using ftxui::HEIGHT;
using ftxui::WIDTH;
using ftxui::EQUAL;
using ftxui::GREATER_THAN;
using ftxui::Color;

// --- Welcome greeting ---
// Rule: empty/null username OR username longer than MAX_USERNAME_LENGTH(20)
// → "Welcome back!".  Otherwise → "Welcome back {user}!".
// The first-run banner caption ("Welcome to Loom") is handled
// separately by the banner renderer and only applies on a first-run session.
inline constexpr std::size_t k_max_username_length = 20;
[[nodiscard]] inline auto format_welcome_message(std::string_view username)
    -> std::string {
    if (username.empty() || username.size() > k_max_username_length) {
        return "Welcome back!";
    }
    return "Welcome back " + std::string(username) + "!";
}

// --- Logo display data ---
struct LogoDisplayData {
    std::string cwd;
    std::string billing_type;
    std::optional<std::string> agent_name;
    std::string model_display_name;
};

// ============================================================
// Condensed logo
// Layout: hbox of [28×6 Loom wordmark, 19×4 below 70 cols] + [gap=2 cols]
// + [vertically centered 3-line text column].
// Text column rows:
//   1: Loom (bold, text)                                        — SINGLE line
//   2: {model} [· {billing}]                                    — all dim
//   3: [@{agent} · ] {truncate-path(cwd)}                       — all dim
// ============================================================
namespace detail {
[[nodiscard]] auto make_wordmark(bool large = false) -> Element;
}

[[nodiscard]] auto RenderCondensedLogoElement(
    const LogoDisplayData& data, int term_cols) -> Element;

// --------------------------------------------------------------------
// §1  Layout-mode enum + column thresholds
// --------------------------------------------------------------------
// Layout mode from column count:
//   columns < 70 → "compact"   (single-column rounded border-card, no feed)
//   columns >= 70 → "horizontal"  (rounded border-card, LEFT=meta+weave,
//                                   RIGHT=feed column, vertical divider)
// "condensed" mode is NOT a layout mode; it is an early return
// path gated by the condensed-mode flag (no changelog/onboarding/force).
// We still expose it in the enum for callers that want to short-circuit.
enum class LogoLayoutMode {
  Condensed,    // early return: CondensedLogo + notices, no outer card
  Compact,      // columns < 70, rounded card, centered, no feed column
  Horizontal,   // columns >= 70, rounded card, left panel + divider + feed
};

// Column thresholds for layout mode selection.
inline constexpr int kHorizontalMinCols = 70;
inline constexpr int kWelcomeFixedWidth = 58;

// Decide layout mode purely from terminal width.
[[nodiscard]] constexpr auto layout_mode_from_cols(int cols) noexcept
    -> LogoLayoutMode {
  return cols >= kHorizontalMinCols ? LogoLayoutMode::Horizontal
                                    : LogoLayoutMode::Compact;
}

// --------------------------------------------------------------------
// §2  Extended LogoDisplayData (superset of LogoDisplayData)
// --------------------------------------------------------------------
// Reads: cwd / billing_type / agent_name
//   / effort_suffix / model_display_name / username / org_name / company_announcement
//   / has_release_notes / show_onboarding / show_sandbox_status
//   / debug_mode / tmux session / sandboxing / show_guest_passes / show_overage_credit
//   / num_startups / global_config fields
//
// We do NOT reach out to globalConfig from this module (separation of
// concerns — no IO inside UI rendering). The ReplScreen / caller is
// expected to populate these fields from their config source; default
// values produce the safe "minimal condensed" look with no side-effects.
struct LogoOptions {
  // --- Identity / chrome (same as LogoDisplayData) ---
  std::string cwd;
  std::string billing_type;
  std::optional<std::string> agent_name;
  std::string model_display_name;

  // --- Extra fields used by Compact/Horizontal & notice stack ---
  std::optional<std::string> username;    // Welcome greeting: "Welcome back X!"
  std::optional<std::string> org_name;    // shown in Horizontal model-line

  // --- Feature gating ---
  bool is_condensed_mode = true;          // no onboarding & no release notes
  bool show_onboarding = false;           // project-onboarding gate
  bool show_sandbox_status = false;       // sandbox-status flag
  bool show_guest_passes = false;         // guest-passes upsell visible
  bool show_overage_credit = false;       // overage-credit upsell visible
  bool is_debug_mode = false;             // debug mode enabled warning
  bool debug_log_to_stderr = false;       // true → stderr, false → path
  std::string debug_log_path;             // path only meaningful if !stderr

  // --- Environment notices ---
  std::optional<std::string> tmux_session;   // LOOM_TMUX_SESSION
  std::optional<std::string> tmux_prefix;    // LOOM_TMUX_PREFIX
  bool tmux_prefix_conflicts = false;        // show "press twice" variant
  std::optional<std::string> company_announcement;  // org-wide banner
  std::optional<std::string> emergency_tip;         // EmergencyTip override

  // --- StatusNotices (exactly 6 definitions) ---
  // Each field is a vector of pre-rendered notice rows; empty means
  // "not active".  Caller is responsible for computing active notices
  // (we deliberately keep the render side IO-free).
  struct NoticeRow {
    std::string glyph;          // U+26A0 ⚠ for warning, U+2191 ↑ for info
    std::string body;           // main text (may include "· sub-clause")
    bool is_warning = true;     // warning→warning color; info→ide/default
  };
  std::vector<NoticeRow> status_notices;
};

// --------------------------------------------------------------------
// §3  Individual notice renderers
// --------------------------------------------------------------------
// All renderers return text("") when not applicable (so a flat vbox over
// all of them produces the correct sparse layout with no extra
// padding for inactive notices).  Uses a 2-column leading space with
// a vertical column; we approximate with a 2-column leading space.

namespace detail {

// Theme-backed brand colors.  Accessors (not constants) so they track the
// active theme; the underlying palettes are function-local statics, immune
// to the static-init-order fiasco that previously forced hardcoded values.
inline const Color& kWarningColor()       { return loom::ui::design::theme::current_theme().palette->warning; }
inline const Color& kIdeColor()           { return loom::ui::design::theme::current_theme().palette->ide; }
inline const Color& kLoomAccent()         { return loom::ui::design::theme::current_theme().palette->loom_body; }
inline const Color& kMuted()              { return loom::ui::design::theme::current_theme().palette->muted; }

// Padding-left 2 columns.
[[nodiscard]] inline auto pad2(Element e) -> Element {
  return hbox({ text("  "), std::move(e) });
}

} // namespace detail

// --- 3c. ChannelsNotice — feature-gated (KAIROS || KAIROS_CHANNELS).
//     C++ counterpart is a constexpr bool; when false, returns empty Element.
template <bool KAIROS = false, bool KAIROS_CHANNELS = false>
[[nodiscard]] inline auto RenderChannelsNotice() -> Element {
  if constexpr (!(KAIROS || KAIROS_CHANNELS)) {
    return Element();  // 0-height: FTXUI default-constructed Element has no renderer
  } else {
    using namespace detail;
    return pad2(hbox({
      text(std::string{loom::ui::design::figures::kLightningBolt} + " ") | color(kLoomAccent()) | bold,   // ⚡ U+26A1
      text("Channels beta: join live conversations") | color(kMuted()) | dim,
    }));
  }
}

// --- 3d. Debug mode notice.
//     Vertical column, left padding = 2:
//       Warning: "Debug mode enabled"
//       Dim: "Logging to: {stderr | path}"
[[nodiscard]] inline auto RenderDebugNotice(const LogoOptions& o) -> Element {
  if (!o.is_debug_mode) return Element();
  using namespace detail;
  Elements lines;
  lines.push_back(hbox({
    text("Debug mode enabled") | color(kWarningColor()),
  }));
  lines.push_back(hbox({
    text("Logging to: "),
    text(o.debug_log_to_stderr ? "stderr" : o.debug_log_path),
  }) | dim | color(kMuted()));
  return pad2(vbox(std::move(lines)));
}

// --- 3e. EmergencyTip.
//     Usually a single line about "Use /settings to
//     change your model provider" or similar. Structure:
//       Horizontal row, left padding = 2:
//         Warning: "⚠"
//         Dim: emergency-tip text
[[nodiscard]] inline auto RenderEmergencyTip(const LogoOptions& o) -> Element {
  if (!o.emergency_tip.has_value()) return Element();
  using namespace detail;
  return pad2(hbox({
    text("\xE2\x9A\xA0 ") | color(kWarningColor()),                 // ⚠
    text(*o.emergency_tip) | dim | color(kMuted()),
  }));
}

// --- 3f. Tmux session notice.
[[nodiscard]] inline auto RenderTmuxNotice(const LogoOptions& o) -> Element {
  if (!o.tmux_session.has_value()) return Element();
  using namespace detail;
  Elements lines;
  lines.push_back(hbox({
    text("tmux session: "),
    text(*o.tmux_session),
  }) | dim | color(kMuted()));
  if (o.tmux_prefix.has_value()) {
    const std::string prefix = *o.tmux_prefix;
    std::string detach_msg =
        o.tmux_prefix_conflicts
            ? "Detach: " + prefix + " " + prefix
                  + " d (press prefix twice - Loom uses " + prefix + ")"
            : "Detach: " + prefix + " d";
    lines.push_back(text(std::move(detach_msg)) | dim | color(kMuted()));
  }
  return pad2(vbox(std::move(lines)));
}

// --- 3g. Organisation announcement.
//     Vertical column, left padding = 2:
//       [optional dim: "Message from {org_name}:"]
//       announcement text
[[nodiscard]] inline auto RenderOrgAnnouncement(const LogoOptions& o) -> Element {
  if (!o.company_announcement.has_value()) return Element();
  using namespace detail;
  Elements lines;
  if (o.org_name.has_value() && !o.org_name->empty()) {
    lines.push_back(hbox({
      text("Message from "),
      text(*o.org_name),
      text(":"),
    }) | dim | color(kMuted()));
  }
  lines.push_back(text(*o.company_announcement));
  return pad2(vbox(std::move(lines)));
}

// --- 3h. Sandbox warning (compact/horizontal only).
//     <Text color="warning">Your bash commands will be sandboxed.
//                            Disable with /sandbox.</Text>
[[nodiscard]] inline auto RenderSandboxNotice(const LogoOptions& o) -> Element {
  if (!o.show_sandbox_status) return Element();
  using namespace detail;
  return pad2(hbox({
    text("\xE2\x9A\xA0 ") | color(kWarningColor()),
    text("Your bash commands will be sandboxed. Disable with /sandbox.")
      | color(kWarningColor()),
  }));
}

// --- 3i. GuestPassesUpsell (also shown in feed).
//     Structure: three [✻] glyphs + "N guest passes at /passes" dimText.
[[nodiscard]] inline auto RenderGuestPassesUpsell(const LogoOptions& o,
                                                 int count = 3) -> Element {
  if (!o.show_guest_passes) return Element();
  using namespace detail;
  Elements brackets;
  brackets.reserve(static_cast<std::size_t>(count * 3));
  for (int i = 0; i < count; ++i) {
    if (i > 0) brackets.push_back(text(" "));
    brackets.push_back(text("[") | color(kMuted()) | dim);
    brackets.push_back(text("\xE2\x9C\xBB") | color(kLoomAccent()) | bold); // ✻
    brackets.push_back(text("]") | color(kMuted()) | dim);
  }
  return pad2(hbox({
    hbox(std::move(brackets)),
    text("  "),
    text(std::to_string(count) + " guest passes at /passes")
      | dim | color(kMuted()),
  }));
}

// --- 3j. OverageCreditUpsell.
//     Structure varies; faithful fallback: orange ⚠ + dimText "You have used 90% of your
//     credit this period · see /billing for details".
[[nodiscard]] inline auto RenderOverageCreditUpsell(const LogoOptions& o) -> Element {
  if (!o.show_overage_credit) return Element();
  using namespace detail;
  return pad2(vbox({
    hbox({
      text("\xE2\x9A\xA0 ") | color(kWarningColor()),
      text("Nearing monthly credit limit") | color(kWarningColor()),
    }),
    hbox({
      text(" "),
      text("Visit /billing to see your usage · upgrades available")
        | dim | color(kMuted()),
    }),
  }));
}

// --- 3k. StatusNotices (exactly 6 definitions).
//     We accept pre-rendered rows so callers
//     decide which are active (keeping this file IO-free).
[[nodiscard]] inline auto RenderStatusNotices(const LogoOptions& o) -> Elements {
  using namespace detail;
  Elements out;
  out.reserve(o.status_notices.size());
  for (const auto& n : o.status_notices) {
    const auto& col = n.is_warning ? kWarningColor() : kIdeColor();
    out.push_back(pad2(hbox({
      text(n.glyph.empty() ? std::string("\xE2\x9A\xA0 ") : (n.glyph + " "))
          | color(col),
      text(n.body) | color(col),
    })));
  }
  return out;
}

// --------------------------------------------------------------------
// §4  Notice aggregator — flat vbox over all active notices.
// --------------------------------------------------------------------
// Condensed stack and compact/horizontal post-card stack. Order matters:
// Channels → Debug → Emergency → Tmux →
// OrgAnnounce → Sandbox → StatusNotices → Guest/Overage (upsells).
//
// (Note: GuestPasses/Overage appear inside CondensedLogo's right column
// but the same components are ALSO rendered in the feed column. We
// render them last in the aggregated stack so both paths can consume
// this helper; CondensedLogo callers may hide them by setting bools off
// and rendering them via a dedicated call if stricter fidelity is needed.)
[[nodiscard]] inline auto RenderNoticeStackAggregated(const LogoOptions& o)
    -> Elements {
  Elements out;
  out.reserve(6 + o.status_notices.size() + 3);
  auto push = [&](Element&& e) {
    // Only push if the element is non-null (has a renderer installed).
    // Inactive notice renderers return Element() (default-constructed,
    // null shared_ptr<Node>) — these MUST be filtered out because vbox
    // dereferences child pointers and null → UB / black screen.
    if (e) out.push_back(std::move(e));
  };
  push(RenderChannelsNotice<>());
  push(RenderDebugNotice(o));
  push(RenderEmergencyTip(o));
  push(RenderTmuxNotice(o));
  push(RenderOrgAnnouncement(o));
  push(RenderSandboxNotice(o));
  for (auto& n : RenderStatusNotices(o)) push(std::move(n));
  push(RenderGuestPassesUpsell(o));
  push(RenderOverageCreditUpsell(o));
  return out;
}

// --------------------------------------------------------------------
// §4b  Welcome theme selector
// --------------------------------------------------------------------
// Retain the terminal/theme selector for callers. Artwork is the same Loom
// weave in all variants; foreground colors come from the active palette.
enum class WelcomeTheme {
  Dark,               // Normal terminal + dark/non-light theme
  Light,              // Normal terminal + light theme
  AppleTerminalDark,  // Apple Terminal + dark theme
  AppleTerminalLight, // Apple Terminal + light theme
};

// --------------------------------------------------------------------
// §5  Fixed-width Loom welcome banner
// --------------------------------------------------------------------
// Reuse the same Loom weave and wordmark as the condensed and card layouts.
[[nodiscard]] auto RenderWelcome(
    WelcomeTheme theme = WelcomeTheme::Dark) -> Element;

// --------------------------------------------------------------------
// Convenience: resolve WelcomeTheme from theme string + terminal type.
// Dispatch:
//   env.terminal === "Apple_Terminal" → AppleTerminal variant
//   ["light", "light-daltonized", "light-ansi"].includes(theme) → Light
//   else → Dark
[[nodiscard]] inline auto ResolveWelcomeTheme(
    std::string_view theme_name,
    bool is_apple_terminal) -> WelcomeTheme {
  const bool is_light =
      (theme_name == "light" || theme_name == "light-daltonized" ||
       theme_name == "light-ansi");
  if (is_apple_terminal) {
    return is_light ? WelcomeTheme::AppleTerminalLight
                    : WelcomeTheme::AppleTerminalDark;
  }
  return is_light ? WelcomeTheme::Light : WelcomeTheme::Dark;
}

// Convenience: render with a theme string + Apple Terminal flag.
[[nodiscard]] inline auto RenderWelcome(
    std::string_view theme_name,
    bool is_apple_terminal) -> Element {
  return RenderWelcome(
                         ResolveWelcomeTheme(theme_name, is_apple_terminal));
}

// --------------------------------------------------------------------
// §5b  Feed / feed-column system
// --------------------------------------------------------------------
// Feed types:
//   FeedLine  = { text: string; timestamp?: string }
//   FeedConfig = {
//     title: string; lines: FeedLine[];
//     footer?: string; empty_message?: string;
//   };
//
// calculate_feed_width:
//   max_width = string_width(title)
//   if custom_content: max_width = max(max_width, custom_content.width)
//   else if lines.empty && empty_message: max_width = max(max_width, string_width(empty_message))
//   else: max_timestamp_width = max(0, ...lines.map(l => l.timestamp?.width ?? 0))
//         for each line: w = text_width + (timestamp>0 ? timestamp_width+2 : 0)
//                        max_width = max(max_width, w)
//   if footer: max_width = max(max_width, string_width(footer))
struct FeedLine {
  std::string text;
  std::optional<std::string> timestamp;
};

struct FeedConfig {
  std::string title;
  std::vector<FeedLine> lines;
  std::optional<std::string> footer;
  std::optional<std::string> empty_message;
};

namespace detail {

// truncate(s, max_width) — suffix-truncate with
// trailing U+2026 HORIZONTAL ELLIPSIS when the string is too long.
// Simple byte-based truncation (sufficient for ASCII paths/models).
// Defined early (before RenderFeedColumn) so it is visible to all helpers.
[[nodiscard]] inline auto truncate_str(std::string s, int max_width)
    -> std::string {
  if (max_width <= 0) return "";
  if (static_cast<int>(s.size()) <= max_width) return s;
  if (max_width == 1) return "\xE2\x80\xA6";
  s.resize(static_cast<std::size_t>(max_width - 3));
  s += "\xE2\x80\xA6";  // … U+2026
  return s;
}

// calculateFeedWidth — byte-width approx.
[[nodiscard]] inline auto calculate_feed_width(const FeedConfig& cfg) -> int {
  auto sw = [](const std::string& s) { return static_cast<int>(s.size()); };

  int max_w = sw(cfg.title);
  if (cfg.lines.empty() && cfg.empty_message.has_value()) {
    max_w = std::max(max_w, sw(*cfg.empty_message));
  } else {
    int max_ts = 0;
    for (const auto& l : cfg.lines) {
      if (l.timestamp.has_value()) {
        max_ts = std::max(max_ts, sw(*l.timestamp));
      }
    }
    for (const auto& l : cfg.lines) {
      int w = sw(l.text);
      if (max_ts > 0) w += max_ts + 2;  // "  " gap between timestamp + text
      max_w = std::max(max_w, w);
    }
  }
  if (cfg.footer.has_value()) {
    max_w = std::max(max_w, sw(*cfg.footer));
  }
  return max_w;
}

// Feed component — renders a single feed:
//   <Text bold color=loom>{title}</Text>
//   custom_content.content  OR  (empty_message  OR  lines+padding+footer)
// Each row: [timestamp padEnd(max_ts) + "  " + truncate(text, text_width)]
[[nodiscard]] inline auto RenderFeed(const FeedConfig& cfg, int actual_width)
    -> Element {
  using namespace ftxui;
  actual_width = std::max(actual_width, 10);

  // Title row: bold + loom color.
  Element title_el = text(truncate_str(cfg.title, actual_width))
                   | bold | color(kLoomAccent());

  // Body rows.
  int max_ts = 0;
  for (const auto& l : cfg.lines) {
    if (l.timestamp.has_value()) {
      max_ts = std::max(max_ts, static_cast<int>(l.timestamp->size()));
    }
  }

  Elements body;
  if (cfg.lines.empty() && cfg.empty_message.has_value()) {
    body.push_back(text(truncate_str(*cfg.empty_message, actual_width))
                   | dim | color(kMuted()));
  } else {
    body.reserve(cfg.lines.size() + (cfg.footer ? 1 : 0));
    const int text_width = std::max(10,
        actual_width - (max_ts > 0 ? max_ts + 2 : 0));
    for (const auto& l : cfg.lines) {
      Elements row_parts;
      if (max_ts > 0) {
        std::string ts = l.timestamp.value_or("");
        // padEnd: right-pad to max_ts with spaces.
        if (static_cast<int>(ts.size()) < max_ts) {
          ts += std::string(static_cast<std::size_t>(
              max_ts - static_cast<int>(ts.size())), ' ');
        }
        row_parts.push_back(text(ts) | dim | color(kMuted()));
        row_parts.push_back(text("  "));  // gap
      }
      row_parts.push_back(text(truncate_str(l.text, text_width)));
      body.push_back(hbox(std::move(row_parts)));
    }
    if (cfg.footer.has_value()) {
      body.push_back(text(truncate_str(*cfg.footer, actual_width))
                     | dim);
    }
  }

  Elements all;
  all.reserve(1 + body.size());
  all.push_back(std::move(title_el));
  for (auto& r : body) all.push_back(std::move(r));
  return vbox(std::move(all))
       | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, actual_width);
}

// Feed column:
//   widths = feeds.map(calculate_feed_width)
//   actual_width = min(max(...widths), max_width)
//   feeds.map((f, i) =>
//     Feed(config=f, actual_width=actual_width)
//     {i < feeds.length-1 && Divider(color=loom, width=actual_width)}
//   )
// Returns the right_width used for geometry hints (caller's benefit).
struct RenderedFeedColumn {
  Element element;
  int actual_width;
};

[[nodiscard]] inline auto RenderFeedColumn(std::vector<FeedConfig> feeds,
                                            int max_width)
    -> RenderedFeedColumn {
  if (feeds.empty()) {
    return { text(""), 0 };
  }
  int max_of_all = 0;
  for (const auto& f : feeds) {
    max_of_all = std::max(max_of_all, calculate_feed_width(f));
  }
  int actual_width = std::min(max_of_all, std::max(max_width, 10));

  Elements rows;
  rows.reserve(feeds.size() * 2);
  for (std::size_t i = 0; i < feeds.size(); ++i) {
    rows.push_back(RenderFeed(feeds[i], actual_width));
    if (i + 1 < feeds.size()) {
      // Divider in loom colour, width = actual_width — 1-row horizontal
      // divider of actual_width chars in loom accent colour.
      // Using FTXUI separator() styled with the loom colour.
      rows.push_back(ftxui::separator() | color(kLoomAccent())
                   | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, actual_width));
    }
  }
  return { vbox(std::move(rows))
               | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, actual_width),
           actual_width };
}

} // namespace detail

// Output geometry for horizontal-layout render — mirrors what the caller
// needs to know for slot sizing / layout debugging.
struct HorizontalLayoutOutput {
  Element layout;
  int left_width  = 0;
  int right_width = 0;
};

// --------------------------------------------------------------------
// §6  Compact & Horizontal card layouts
// --------------------------------------------------------------------
namespace detail {

inline constexpr int kMaxUsernameLength = 20;
// Layout constants.
inline constexpr int kMaxLeftWidth    = 50;
inline constexpr int kBorderPadding   = 4;
inline constexpr int kDividerWidth    = 1;
inline constexpr int kContentPadding  = 2;

// formatWelcomeMessage(username):
//   username empty/null OR longer than MAX_USERNAME_LENGTH(20) → "Welcome back!"
//   Otherwise → "Welcome back {username}!"
// NOTE: The first-run "Welcome to Loom" variant is NOT produced by
// this function — it is handled separately by the Welcome card renderer
// (see RenderWelcome).
[[nodiscard]] inline auto format_welcome_message(
    const std::optional<std::string>& username) -> std::string {
  if (!username.has_value() || username->empty()
      || static_cast<int>(username->size()) > kMaxUsernameLength) {
    return "Welcome back!";
  }
  return "Welcome back " + *username + "!";
}

} // namespace detail

// Compact mode (cols < 70):
//
// Structure (faithful, line-by-line):
//   Offscreen freeze wrapper:
//     Vertical column, rounded border in loom color,
//     border title = compact border text  ← " Loom " in loom color
//     horizontal padding = 1, vertical padding = 1,
//     centred, width = columns:
//       Bold welcome message
//       Loom weave (top margin 1)
//       Dim model display name
//       Dim billing type
//       Dim agent·cwd line
//     (end vertical column)
//   (end offscreen freeze)
//   notices…
//
// Border title: compact border text, positioned at top-start, offset 1
//   → the title text is inset 1 char from the left border corner.
//
// FTXUI border note: FTXUI's closest equivalent to a border-embedded title
// border stroke. FTXUI's closest equivalent is ftxui::window(title, body) which
// paints title inside a ╭─{title}─…─╮ box. The visual structure (inset title on
// the top border, rounded-style border) is preserved.
[[nodiscard]] inline auto RenderCompactLayout(const LogoOptions& o,
                                               int term_cols,
                                               int /*is_new_user_unused*/ = 0)
    -> Element {
  using namespace detail;
  using ftxui::window;

  const std::string welcome = format_welcome_message(o.username);
  // Compact card uses BORDER_PADDING (4 chars: 2 borders + 2 horizontal padding).
  // Card inner width = columns - kBorderPadding (subtract 2 borders + 2 padX).
  const int card_width   = std::max(term_cols, 20);
  const int inner_width  = std::max(card_width - kBorderPadding, 12);

  // If welcome message is wider than available space, fall back to the
  // username=null variant.
  const std::string welcome_effective =
      static_cast<int>(welcome.size()) > inner_width
          ? format_welcome_message(std::nullopt)
          : welcome;

  // Compact border title: loom-accent " Loom "
  // Title sits on the top border, inset by offset=1.
  Element border_title = hbox({
      // offset=1: 1 leading space, then the title text.
      text(" ") | color(kLoomAccent()),
      text("Loom") | color(kLoomAccent()) | bold,
      text(" ") | color(kLoomAccent()),
  });

  // --- Compact Loom wordmark (19 cols × 4 rows), centered. ---

  // --- Model line + billing + agent·cwd, all dim ---
  // Width budget = inner_width (inside card, after borders + padding).
  // Horizontal padding of 1 → content starts 2 cols in.
  const int content_budget = inner_width;

  // model_display_name = truncate(
  //   full model name + effort suffix, LEFT_PANEL_MAX_WIDTH - 20)
  // Pre-truncate to kMaxLeftWidth - 20 = 30 before layout-specific clipping.
  const std::string model_raw = !o.model_display_name.empty()
      ? o.model_display_name : std::string("Loom");
  const int model_trunc_width =
      std::min(kMaxLeftWidth - 20, content_budget);
  const std::string model_display = truncate_str(model_raw, model_trunc_width);
  const std::string billing_display =
      truncate_str(o.billing_type, content_budget);

  const std::string cwd_line = [&]() -> std::string {
    // cwd available width = agent_name ? columns - 4 - 1 - string_width(agent_name) - 3
    //                                   : columns - 4
    // Note: 4 = layout_width (border+padding).
    int budget = content_budget;
    if (o.agent_name.has_value() && !o.agent_name->empty()) {
      // "@" + name + " · " = 1 + name.size() + 3
      budget = budget - 1 - static_cast<int>(o.agent_name->size()) - 3;
    }
    budget = std::max(budget, 10);
    std::string cwd = o.cwd;
    if (static_cast<int>(cwd.size()) > budget) {
      cwd = "\xE2\x80\xA6" + cwd.substr(
          cwd.size() - static_cast<std::size_t>(budget - 3));
    }
    if (o.agent_name.has_value() && !o.agent_name->empty()) {
      return "@" + *o.agent_name + " \xC2\xB7 " + cwd;
    }
    return cwd;
  }();

  // Centre each line horizontally.
  Element inner = vbox({
    text(welcome_effective) | bold | ftxui::center,
    text(""),                        // top margin = 1 (weave icon wrapped in it)
    inner_width >= 19 ? make_wordmark()
        : text("loom") | bold | color(loom::ui::design::theme::current_theme().palette->text)
                       | ftxui::center,
    text(model_display) | dim | color(kMuted()) | ftxui::center,
    text(billing_display) | dim | color(kMuted()) | ftxui::center,
    text(cwd_line)      | dim | color(kMuted()) | ftxui::center,
  }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, inner_width);

  // ftxui::window(title, body) — border-embedded title with
  // top-start alignment. Border colour = loom (kLoomAccent()).
  Element card = window(std::move(border_title), std::move(inner))
               | color(kLoomAccent());

  return card | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, card_width);
}

// Horizontal mode (cols >= 70):
//
// Outer structure (faithful):
//   Offscreen freeze wrapper:
//     Vertical column, rounded border in loom color,
//     border title = border title text  ← " Loom "
//     Horizontal row, horizontal padding = 1, gap = 1:
//       LEFT_PANEL
//       VERTICAL_DIVIDER (when horizontal layout)
//       Feed column (when horizontal layout)
//   (end offscreen freeze)
//   notices…
//
// Border title offset=3 → the title text starts 3 chars in from the left corner.
//
// Left panel:
//   Vertical column, width = left_width,
//   space-between layout, centred, min-height = 9:
//     Top margin 1, bold welcome message
//     Loom weave
//     Vertical column, centred:
//       Dim model line
//       Dim cwd line
//
// Layout math (faithful):
//   optimal_left_width = min(max( welcome.width, cwd_line.width, model_line.width, 20 ) + 4, 50)
//   if horizontal:
//     left_width  = optimal_left_width
//     used_space  = BORDER_PADDING(4) + CONTENT_PADDING(2) + DIVIDER(1) + left_width
//     right_width = max(30, columns - used_space)
//     total_width = min(left_width + right_width + DIVIDER + CONTENT_PADDING,
//                      columns - BORDER_PADDING)
//     if total clamped: right_width = total_width - left_width - DIVIDER - CONTENT_PADDING
//   (compact branch not reached here — horizontal mode selection is the caller's)
//
// Feed selection (4-armed ternary):
//   show_onboarding        → [ProjectOnboarding, RecentActivity]
//   show_guest_passes      → [RecentActivity, GuestPasses]
//   show_overage_credit    → [RecentActivity, OverageCredit]
//   (default)              → [RecentActivity, What'sNew]
//
// We accept feeds as a vector<FeedConfig> via LogoOptions; if empty we fall
// back to the default two-feed layout [RecentActivity, What'sNew] using the
// placeholders provided by the repl_screen caller data.

[[nodiscard]] inline auto RenderHorizontalLayout(
    const LogoOptions& o,
    int term_cols,
    int /*is_new_user_unused*/ = 0,
    std::vector<FeedConfig> feeds = std::vector<FeedConfig>(),
    Element feed_column_override = Element())
    -> HorizontalLayoutOutput {
  using namespace detail;

  const int columns = std::max(term_cols, kHorizontalMinCols);

  // --- Compute welcome / model_line / cwd_line. ---
  const std::string welcome = format_welcome_message(o.username);

  // model_line: org ? "{model} · {billing} · {org}" : "{model} · {billing}"
  // (single composed line — NOT the compact 3-separate-lines variant).
  //
  // model_display_name is pre-truncated to
  //   LEFT_PANEL_MAX_WIDTH - 20 (= 30) before being composed into modelLine.
  //   "-20 to account for the max length of subscription name
  //    '· Loom Enterprise'."
  const std::string model_raw = o.model_display_name.empty()
      ? std::string("Loom") : o.model_display_name;
  const std::string model_for_line =
      truncate_str(model_raw, kMaxLeftWidth - 20);

  std::string model_line_full = [&]() {
    std::string m = model_for_line;
    if (!o.billing_type.empty()) {
      m += " \xC2\xB7 " + o.billing_type;
    }
    if (o.org_name.has_value() && !o.org_name->empty()) {
      m += " \xC2\xB7 " + *o.org_name;
    }
    return m;
  }();

  // cwd_line: agent_name ? "@{agent} · {truncated_cwd}" : truncated_cwd
  // Left-panel budget = LEFT_PANEL_MAX_WIDTH (50).
  // cwd available width = agent_name ? MAX(50) - 1 - name.size() - 3 : 50
  // (Note: agent name + "@" + " · " is the fixed prefix.)
  const std::string cwd_line_full = [&]() {
    const int max_w = kMaxLeftWidth;
    int agent_cost = 0;
    if (o.agent_name.has_value() && !o.agent_name->empty()) {
      agent_cost = 1 + static_cast<int>(o.agent_name->size()) + 3;
    }
    int budget = std::max(max_w - agent_cost, 10);
    std::string cwd = o.cwd;
    if (static_cast<int>(cwd.size()) > budget) {
      cwd = "\xE2\x80\xA6" + cwd.substr(
          cwd.size() - static_cast<std::size_t>(budget - 3));
    }
    if (o.agent_name.has_value() && !o.agent_name->empty()) {
      return "@" + *o.agent_name + " \xC2\xB7 " + cwd;
    }
    return cwd;
  }();

  // --- calculateOptimalLeftWidth ---
  auto sw = [](const std::string& s){ return static_cast<int>(s.size()); };
  const int content_width = std::max({
      sw(welcome), sw(cwd_line_full), sw(model_line_full), 20 /* minimum metadata width */ });
  const int optimal_left_width = std::min(content_width + 4, kMaxLeftWidth);

  // --- calculateLayoutDimensions, horizontal branch ---
  const int left_width  = optimal_left_width;
  const int used_space  = kBorderPadding + kContentPadding + kDividerWidth + left_width;
  int right_width       = std::max(30, columns - used_space);
  int total_width = std::min(
      left_width + right_width + kDividerWidth + kContentPadding,
      columns - kBorderPadding);
  if (total_width < left_width + right_width + kDividerWidth + kContentPadding) {
    right_width = total_width - left_width - kDividerWidth - kContentPadding;
  }
  right_width = std::max(right_width, 20);  // safety minimum

  // --- Border title:
  //   " {loom-accent Loom} "
  //   offset=3 → title starts at column 3 from the left border corner.
  Element border_title = hbox({
      text("   ") | color(kLoomAccent()),                // offset=3 leading spaces
      text("Loom") | color(kLoomAccent()) | bold,
  });

  // --- Loom wordmark sized to the left panel, centered. ---

  // --- Left panel: space-between layout approximated. ---
  // FTXUI can't do true space-between; we push the welcome up, icon in middle,
  // meta at bottom using flexbox with filler rows.
  Element left_panel = ftxui::vbox({
      // top margin = 1 → one blank row then welcome.
      text(""),
      hbox({ text(welcome) | bold }) | ftxui::center,
      text(""),
      make_wordmark(left_width >= 28),
      ftxui::vbox({
          text(truncate_str(model_line_full, left_width - 2))
              | dim | color(kMuted()),
          text(truncate_str(cwd_line_full, left_width - 2))
              | dim | color(kMuted()),
      }) | ftxui::center,
  }) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, left_width)
     | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 9);

  // --- Vertical divider:
  //   Full-height, single-line border in loom accent colour,
  //   dimmed, with top/bottom/left borders disabled.
  // This is a single full-height vertical rule in the accent colour (or a
  // dimmed variant). We approximate with 9 rows of "│" in kLoomAccent(), sized to
  // the minimum of the left panel (GREATER_THAN 9).
  Element divider =
      vbox(Elements(9, text("\xE2\x94\x82") | color(kLoomAccent())))
      | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, kDividerWidth)
      | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 9);

  // --- Feed column. ---
  Element feed;
  if (feed_column_override) {
    feed = std::move(feed_column_override);
  } else if (!feeds.empty()) {
    auto [fc, _aw] = detail::RenderFeedColumn(std::move(feeds), right_width);
    feed = std::move(fc);
  } else {
    // 4-branch feed priority chain:
    //   show_onboarding        → [ProjectOnboarding, RecentActivity]
    //   show_guest_passes      → [RecentActivity, GuestPasses]
    //   show_overage_credit    → [RecentActivity, OverageCredit]
    //   (default)              → [RecentActivity, What'sNew]
    //
    // Priority order: onboarding > guest passes > overage credit > default.
    // When the caller provides explicit `feeds`, those are used as-is (the
    // !feeds.empty() branch above). This else-branch fires only when no
    // feeds were supplied, and we resolve the priority chain from the
    // option flags.
    std::vector<FeedConfig> resolved_feeds;

    if (o.show_onboarding) {
      // Branch 1: Project onboarding takes highest priority.
      // createProjectOnboardingFeed(getSteps()) + createRecentActivityFeed().
      FeedConfig onboarding;
      onboarding.title = "Getting started";
      onboarding.lines = {
        FeedLine{ .text = "Ask Loom a question to start a conversation",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "Use /help to see available commands",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "Paste images into the prompt with Ctrl+V",
                  .timestamp = std::nullopt },
      };
      onboarding.footer = "Dismissed automatically after first message";
      resolved_feeds.push_back(std::move(onboarding));

      FeedConfig recent;
      recent.title = "Recent activity";
      recent.empty_message = "(no recent activity yet — engine wiring pending)";
      resolved_feeds.push_back(std::move(recent));
    } else if (o.show_guest_passes) {
      // Branch 2: Guest passes upsell.
      // createRecentActivityFeed(activities) + createGuestPassesFeed().
      FeedConfig recent;
      recent.title = "Recent activity";
      recent.empty_message = "(no recent activity yet — engine wiring pending)";
      resolved_feeds.push_back(std::move(recent));

      FeedConfig guest;
      guest.title = "Guest passes";
      guest.lines = {
        FeedLine{ .text = "You have 3 guest passes available",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "Share Loom with teammates at /passes",
                  .timestamp = std::nullopt },
      };
      guest.footer = "Use /passes to manage invitations";
      resolved_feeds.push_back(std::move(guest));
    } else if (o.show_overage_credit) {
      // Branch 3: Overage credit upsell.
      // createRecentActivityFeed(activities) + createOverageCreditFeed().
      FeedConfig recent;
      recent.title = "Recent activity";
      recent.empty_message = "(no recent activity yet — engine wiring pending)";
      resolved_feeds.push_back(std::move(recent));

      FeedConfig overage;
      overage.title = "Usage alert";
      overage.lines = {
        FeedLine{ .text = "Nearing monthly credit limit",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "Visit /billing to see usage details",
                  .timestamp = std::nullopt },
      };
      overage.footer = "Upgrades available at /billing";
      resolved_feeds.push_back(std::move(overage));
    } else {
      // Branch 4 (default): Recent activity + What's new.
      // createRecentActivityFeed(activities) + createWhatsNewFeed(changelog).
      FeedConfig recent;
      recent.title = "Recent activity";
      recent.empty_message = "(no recent activity yet — engine wiring pending)";
      resolved_feeds.push_back(std::move(recent));

      FeedConfig whats_new;
      whats_new.title = "What's new";
      whats_new.lines = {
        // Changelog placeholder lines — no timestamps on default feed
        FeedLine{ .text = "Paste images with Ctrl+V directly into the prompt",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "New logo modes: compact, condensed, horizontal",
                  .timestamp = std::nullopt },
        FeedLine{ .text = "Sandboxing bash commands with /sandbox toggle",
                  .timestamp = std::nullopt },
      };
      whats_new.footer = "See full changelog at /changelog";
      resolved_feeds.push_back(std::move(whats_new));
    }

    auto [fc, _aw] = detail::RenderFeedColumn(std::move(resolved_feeds), right_width);
    feed = std::move(fc);
  }
  feed = std::move(feed)
       | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, right_width);

  // --- Inner row: horizontal padding = 1, gap = 1 approximated. ---
  // gap=1 → one space between panels.
  Element inner_row = hbox({
      text(" "),  // left padding
      std::move(left_panel),
      text(" "),  // gap
      std::move(divider),
      text(" "),  // gap
      std::move(feed),
      text(" "),  // right padding
  });

  // Wrap inner_row in window() with the border title —
  // rounded border + loom accent colour + border title text.
  Element outer = ftxui::window(std::move(border_title), std::move(inner_row))
                | color(kLoomAccent());

  return { std::move(outer), left_width, right_width };
}

// --------------------------------------------------------------------
// §7  Top-level render_logo dispatch
// --------------------------------------------------------------------
// Complete render return paths.
//
// Dispatch:
//   1. is_condensed_mode → CondensedLogo + aggregatedNoticeStack
//   2. else layout_mode_from_cols(term_cols) → Compact or Horizontal
//        card + aggregatedNoticeStack
struct LogoResult {
  Element root;                      // combined vbox(card, notices)
  Element header;                    // just the card / condensed header
  Elements notices;                 // just the aggregated notice rows
  LogoLayoutMode mode;
  // Geometry hints (populated only for Horizontal mode; 0 otherwise).
  int left_width  = 0;
  int right_width = 0;
};

[[nodiscard]] inline auto RenderLogo(const LogoOptions& o,
                                       int term_cols,
                                       std::vector<FeedConfig> feeds = std::vector<FeedConfig>(),
                                       Element feed_column_override = Element())
    -> LogoResult {
  if (o.is_condensed_mode) {
    // --- CONDENSED path ---
    LogoDisplayData inner;
    inner.cwd                = o.cwd;
    inner.billing_type       = o.billing_type;
    inner.agent_name         = o.agent_name;
    inner.model_display_name = o.model_display_name;

    Element header = RenderCondensedLogoElement(inner, term_cols);
    Elements notices = RenderNoticeStackAggregated(o);

    Elements combined;
    combined.reserve(1 + notices.size());
    combined.push_back(std::move(header));
    for (auto& n : notices) combined.push_back(std::move(n));
    return { vbox(std::move(combined)), /*header=*/{}, std::move(notices),
             LogoLayoutMode::Condensed };
  }

  // --- CARD path ---
  const LogoLayoutMode mode = layout_mode_from_cols(term_cols);
  Element card;
  struct W { int l = 0, r = 0; };
  W widths;
  if (mode == LogoLayoutMode::Compact) {
    // Compact mode: 3rd param is (unused) is_new_user compatibility flag.
    card = RenderCompactLayout(o, term_cols, 0);
  } else {
    auto [h, lw, rw] = RenderHorizontalLayout(o, term_cols, 0,
                                              std::move(feeds),
                                              std::move(feed_column_override));
    card = std::move(h);
    widths = {lw, rw};
  }
  Elements notices = RenderNoticeStackAggregated(o);
  Elements combined;
  combined.reserve(1 + notices.size());
  combined.push_back(std::move(card));
  for (auto& n : notices) combined.push_back(std::move(n));
  return { vbox(std::move(combined)), std::move(card), std::move(notices), mode,
           widths.l, widths.r };
}

// Convenience overload: returns just the combined-root Element (the
// common case in repl_screen::RenderWelcomeHeader).
[[nodiscard]] inline auto render_logo(const LogoOptions& o,
                                          int term_cols = 120,
                                          std::vector<FeedConfig> feeds = {})
    -> Element {
  return RenderLogo(o, term_cols, std::move(feeds)).root;
}

/// Thin sticky logo header bar (1 line).  Sits ABOVE the message list
/// and stays visible even when the welcome card scrolls off due to
/// pin-to-bottom.
///
/// Visual: "◆ Loom  ·  ModelName"  (left-aligned, dim)
[[nodiscard]] inline Element render_logo_header_bar(
    std::string_view model_display_name,
    int /*term_cols*/ = 120)
{
    using namespace ftxui;
    Elements parts;
    parts.push_back(text(std::string{loom::ui::design::figures::kDiamond} + " ") | color(loom::ui::design::theme::current_theme().palette->primary));  // ◆ diamond
    parts.push_back(text("Loom") | bold);
    if (!model_display_name.empty()) {
        parts.push_back(text("  \xc2\xb7 ") | dim);  // · separator
        parts.push_back(text(std::string(model_display_name)) | dim);
    }
    return hbox(std::move(parts)) | size(HEIGHT, EQUAL, 1);
}

} // namespace loom::ui::logo
