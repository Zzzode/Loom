// repl_screen_welcome.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). status bar, spinner, truncate_columns, the preserved legacy welcome
// helpers and the LogoV2 welcome header.
//
// Phase A (#184957): textual FTXUI GMF headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.screens.chrome_store;
import loom.constants.spinner_verbs;
import loom.ui.foundation.logo;
import loom.ui.foundation.theme_provider;

namespace loom::ui::repl_screen {
using namespace ftxui;

// UI1: status bar — delegates to loom.ui.components.status_line.
// For now we provide a semantic assembler that status_line will style.
[[nodiscard]] Element RenderStatusBar(const StatusBarData& d) {
    const auto& palette = *loom::ui::design::theme::current_theme().palette;
    Elements L = { text(" ") };
    L.push_back(text(d.model_name) | bold | color(palette.info));
    if (d.is_fast_mode)  L.push_back(text(" fast") | color(palette.fast_mode) | dim);
    if (d.is_auto_mode)  L.push_back(text(" [auto]") | color(palette.auto_accept));
    if (d.agent_name)   { L.push_back(text(" @") | dim);
        L.push_back(text(*d.agent_name) | color(palette.info)); }
    if (d.effort_level) L.push_back(text(" [" + *d.effort_level + "]") | dim);
    if (d.is_brief_mode)L.push_back(text(" [brief]") | dim | color(palette.brief_label));
    Elements R;
    if (d.context_token_count > 0) {
        R.push_back(text(std::format("ctx:{}", d.context_token_count)) | dim);
        R.push_back(text(" | ") | dim); }
    if (d.input_tokens || d.output_tokens) R.push_back(
        text(std::format("{}v {}^", d.input_tokens, d.output_tokens)) | dim);
    if (d.cost_usd) { R.push_back(text(" ") | dim); R.push_back(
        text(std::format("${:.4f}", *d.cost_usd)) | dim | color(palette.success)); }
    if (d.swarm_session_count > 0) {
        R.push_back(text(" | ") | dim);
        R.push_back(text(std::format("{} swarm", d.swarm_session_count))
                        | dim | color(palette.auto_accept)); }
    if (d.session_name) { R.push_back(text(" | ") | dim);
        R.push_back(text(*d.session_name) | dim); }
    if (d.bridge_connected) {
        R.push_back(text(" | ") | dim);
        R.push_back(text("<-> bridge") | color(palette.info) | dim); }
    R.push_back(text(" "));
    return hbox({ hbox(L), filler(), hbox(R) })
         | bgcolor(palette.status_bar_background);
}

// UI19: spinner line shell.
// Single loom-gold theme, cycling TEARDROP_ASTERISK glyph, random playful
// verb sampled from spinner_verbs list, 3-dot blink cadence.
[[nodiscard]] Element RenderSpinner(
    SpinnerMode m, const std::optional<std::string>& verb,
    const std::optional<std::string>& tip,
    int frame) {
    if (m == SpinnerMode::Hidden) return text("");
    // Single theme token regardless of mode — no per-mode color switch.
    const Color kLoomGold =
        loom::ui::design::theme::current_theme().palette->spinner_gold;

    // Pick a random playful verb once "per mount".  We don't track mount
    // state here, so hash the mode + pid and sample the verbs list.
    using loom::constants::spinner_verbs::SPINNER_VERBS;
    static std::mt19937 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<std::size_t> dist(
        0, SPINNER_VERBS.size() - 1);
    static thread_local std::string_view cached_verb = SPINNER_VERBS[dist(rng)];
    // Re-sample when mode transitions from Hidden -> something (frame==0
    // from caller signals first visible frame of a new request).
    if (frame == 1) cached_verb = SPINNER_VERBS[dist(rng)];

    // 3-dot blink cycle: floor(time/300)%3.
    // Frame is time-based (100 ms/frame); frame/3 = 300 ms per state.
    const int dot_idx = std::max(0, frame / 3) % 3;
    const std::string dots = std::string(static_cast<std::size_t>(dot_idx + 1), '.') +
                             std::string(static_cast<std::size_t>(3 - dot_idx - 1), ' ');

    // SpinnerGlyph cycle (frames of TEARDROP_ASTERISK animation); 8 frames.
    // Frame is time-based (100 ms/frame); each frame advances one glyph.
    constexpr std::array<std::string_view, 8> kGlyphs = {
        "✻", "❋", "✦", "✧", "✶", "✷", "✸", "✹"
    };
    const auto glyph = kGlyphs[static_cast<std::size_t>(std::max(0, frame)) % kGlyphs.size()];

    std::string_view selected_verb = verb ? std::string_view(*verb) : cached_verb;
    std::string label = std::string(selected_verb) + "\xE2\x80\xA6";  // …
    Elements p = {
        text("  ") | size(WIDTH, EQUAL, 2),  // left padding = 2
        text(std::string(glyph)) | color(kLoomGold),
        text(" "),
        text(label) | color(kLoomGold),
        text(dots)  | color(kLoomGold) | dim,
    };
    if (tip) p.push_back(text("  -- " + *tip) | dim);
    return hbox(p);
}

[[nodiscard]] std::string truncate_columns(std::string text, int max_cols) {
    if (max_cols <= 0) return {};
    if (string_width(text) <= max_cols) return text;
    while (!text.empty() && string_width(text + "…") > max_cols) {
        text.pop_back();
    }
    return text + "…";
}

[[nodiscard]] std::string repeat_welcome_segment(std::string_view text, int count) {
    std::string out;
    for (int i = 0; i < count; ++i) out += text;
    return out;
}

[[nodiscard]] Element RenderWelcomeFeed(std::string title,
                                               std::string message,
                                               int width,
                                               Color accent,
                                               Color muted) {
    width = std::max(width, 10);
    Elements rows;
    rows.push_back(text(std::move(title)) | bold | color(accent));
    rows.push_back(text(truncate_columns(std::move(message), width)) | color(muted) | dim);
    return vbox(std::move(rows)) | size(WIDTH, EQUAL, width);
}

[[nodiscard]] Element RenderWelcomeFeedColumn(int width,
                                                     Color accent,
                                                     Color muted) {
    width = std::max(width, 30);
    return vbox({
        RenderWelcomeFeed(
            "Recent activity",
            "No recent activity",
            width,
            accent,
            muted),
        separator() | color(accent),
        RenderWelcomeFeed(
            "What's new",
            "Check the Loom changelog for updates",
            width,
            accent,
            muted),
    }) | size(WIDTH, EQUAL, width);
}

[[nodiscard]] Element RenderWelcomeWindow(Element title,
                                                 Element body,
                                                 int width,
                                                 int title_width,
                                                 Color accent) {
    width = std::max(width, 4);
    const int title_offset = std::min(3, std::max(0, width - 2));
    const int right_rule_width =
        std::max(0, width - 2 - title_offset - title_width);

    Element top = hbox({
        text("╭") | color(accent),
        text(repeat_welcome_segment("─", title_offset)) | color(accent),
        std::move(title),
        text(repeat_welcome_segment("─", right_rule_width)) | color(accent),
        text("╮") | color(accent),
    });
    Element middle = hbox({
        separator() | color(accent),
        std::move(body) | size(WIDTH, EQUAL, width - 2),
        separator() | color(accent),
    });
    Element bottom = hbox({
        text("╰") | color(accent),
        text(repeat_welcome_segment("─", width - 2)) | color(accent),
        text("╯") | color(accent),
    });
    return vbox({
        std::move(top),
        std::move(middle),
        std::move(bottom),
    }) | size(WIDTH, EQUAL, width);
}

// UI0: welcome header.
//   Row 1: orange weave icon + "Loom" bold + "vX.X.X" dim
//   Row 2: model · billing_type dim
//   Row 3: [@agent · ] cwd dim
// Shown only on a fresh idle session (messages empty + spinner hidden).
// The old ASCII-art bordered card / feed-column helpers are preserved in
// this file but no longer called by this renderer.
//
// P0-4 Faithful dispatch:
//   is_condensed_mode (default)  → CondensedLogo + 10 notices flat stack
//   force_full_logo + cols<70    → Compact round card + flat notice stack
//   force_full_logo + cols>=70   → Horizontal left|divider|feed card + stack
// The caller may set s.debug_* / s.tmux_* / s.sandboxing_enabled fields to
// drive notice activation; they default to off so the header renders the
// same minimal 3-row look as the default-condensed branch.
[[nodiscard]] Element RenderWelcomeHeader(const ReplScreenState& s,
                                                 int /*spinner_frame*/,
                                                 int term_cols,
                                                 bool force_full_logo) {
    namespace lv2 = loom::ui::logo;

    const std::string model_line = !s.chrome_store.model_display_name.empty()
        ? s.chrome_store.model_display_name
        : s.settings_model;

    // Build the LogoV2Options. Defaults mirror the component's initial
    // props (no onboarding, no release-notes → condensed branch).
    lv2::LogoV2Options opts;
    opts.cwd                  = s.cwd;
    opts.billing_type         = s.chrome_store.billing_type;
    // Agent name comes from initial settings.
    opts.agent_name           = s.settings_agent_name.empty()
                              ? std::nullopt
                              : std::make_optional(s.settings_agent_name);
    opts.model_display_name   = model_line;
    opts.username             = s.chrome_store.user_display_name.empty()
        ? std::nullopt
        : std::make_optional(s.chrome_store.user_display_name);
    opts.org_name             = std::nullopt;
    opts.is_condensed_mode    = !force_full_logo && !s.chrome_store.show_onboarding;
                                                                          // Early-return gate: condensed when
                                                                          // no release notes, no onboarding,
                                                                          // no full logo.
    opts.show_onboarding     = s.chrome_store.show_onboarding;
    opts.show_sandbox_status  = false;                    // TODO(engine-wire)
    opts.show_guest_passes    = s.chrome_store.show_guest_passes_upsell;
    opts.show_overage_credit  = s.chrome_store.show_overage_credit_upsell;
    opts.is_debug_mode        = false;               // TODO(engine-wire)
    opts.tmux_session         = std::nullopt;        // TODO(engine-wire)
    opts.company_announcement = std::nullopt;        // TODO(engine-wire)
    opts.emergency_tip        = std::nullopt;        // TODO(engine-wire)
    // StatusNotices: 6 definitions (memory/agent/subscriber/apikey/both/
    // jetbrains). All stubs inactive until engine wiring provides data.
    opts.status_notices       = {};

    // When force_full_logo is set and term_cols >= 70 (horizontal threshold),
    // build feeds for the right column.  The 4-branch feed priority chain
    // is resolved inside the logo module when feeds
    // is empty.  We only build explicit feeds for the DEFAULT branch (no
    // onboarding / no guest / no overage) so we can inject real data from
    // s.chrome_store.recent_activity_lines and s.chrome_store.changelog_lines.  For the other branches,
    // the module builds placeholder feeds until engine wiring provides real
    // onboarding steps / guest pass counts / overage data.
    std::vector<lv2::FeedConfig> feeds;
    const bool priority_branch_active = s.chrome_store.show_onboarding
        || s.chrome_store.show_guest_passes_upsell || s.chrome_store.show_overage_credit_upsell;
    if (force_full_logo && !priority_branch_active) {
      {
        lv2::FeedConfig recent;
        recent.title = "Recent activity";
        if (s.chrome_store.recent_activity_lines.empty()) {
          recent.empty_message =
              "No recent conversations — start a new chat above";
        } else {
          recent.lines.reserve(s.chrome_store.recent_activity_lines.size());
          for (const auto& a : s.chrome_store.recent_activity_lines) {
            recent.lines.push_back(lv2::FeedLine{
                /*text=*/std::string(a),
                /*timestamp=*/std::nullopt});
          }
        }
        feeds.push_back(std::move(recent));
      }
      {
        lv2::FeedConfig whats_new;
        whats_new.title = "What's new";
        if (s.chrome_store.changelog_lines.empty()) {
          whats_new.lines = {
            lv2::FeedLine{/*text=*/"Paste images into the prompt with Ctrl+V",
                          /*timestamp=*/std::nullopt},
            lv2::FeedLine{/*text=*/"3-mode logo: condensed / compact / horizontal",
                          /*timestamp=*/std::nullopt},
            lv2::FeedLine{/*text=*/"Bash sandboxing via /sandbox toggle",
                          /*timestamp=*/std::nullopt},
          };
        } else {
          whats_new.lines.reserve(s.chrome_store.changelog_lines.size());
          for (const auto& c : s.chrome_store.changelog_lines) {
            whats_new.lines.push_back(lv2::FeedLine{
                /*text=*/std::string(c), /*timestamp=*/std::nullopt});
          }
        }
        whats_new.footer = "Full changelog at /changelog";
        feeds.push_back(std::move(whats_new));
      }
    }

    return lv2::render_logo_v2(opts, term_cols, std::move(feeds));
  }

}  // namespace loom::ui::repl_screen
