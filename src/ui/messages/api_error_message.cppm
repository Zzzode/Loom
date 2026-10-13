/// @file api_error_message.cppm
/// @brief Model-request failure with a clear origin, diagnostic details,
/// and request-scoped recovery actions.
module;

#include <cstdint>
#include <cmath>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.messages.api_error_message;

import loom.ui.foundation.theme_provider;

import std;
import loom.ui.messages.message_components;

import loom.ui.foundation.design_figures;

export namespace loom::ui::messages::api_error_message {
using namespace ftxui;

// ============================================================
// Types
// ============================================================

/// Severity drives border + accent color. Integer HTTP codes also influence
/// the final color (see resolve_border_color()).
enum class ErrorSeverity : std::uint8_t {
    Warning,   // e.g. 401 / 403 — orange family
    Error,     // e.g. 4xx other — red
    Critical,  // e.g. 5xx, transport — deep red
};

/// The canonical API error payload for rendering.
struct APIErrorData {
    std::optional<int> http_status;           // e.g. 401 / 500
    std::string provider;                     // "Messages", "OpenAI", etc.
    std::string error_code;                   // e.g. "auth_error", "rate_limit"
    std::string message;                      // Human-readable message
    std::string trace_id;                     // x-request-id / x-amzn-trace-id
    std::string request_id;                   // Secondary id (may be empty)

    ErrorSeverity severity{ErrorSeverity::Error};

    // Retry info
    std::optional<int> current_attempt;       // N of M
    std::optional<int> max_attempts;
    std::optional<double> retry_after_ms;     // Total backoff duration (ms)
    /// When true, the authentication session has expired and the user must
    /// clear / re-authenticate rather than just retry.
    bool session_expired{false};

    // Oversized-body warning
    std::optional<std::size_t> response_bytes;
    bool truncated{false};

    // Expand / collapse long message
    bool collapsed{true};
    std::size_t max_chars_collapsed{1000};    // Max chars in collapsed body
};

/// Component options
struct APIErrorOptions {
    APIErrorData error;
    bool show_trace_ids{true};
    bool show_buttons{true};
    bool dismissible{true};
    bool copyable_trace{true};

    std::function<void()> on_retry;         // Retry the failed model request
    std::function<void()> on_diagnose;      // Diagnose button
    std::function<void()> on_dismiss;       // Dismiss button
    std::function<void(const std::string&)> on_copy_trace;   // Copy trace id
    /// Called when the user clicks "Clear session" on a session-expired
    /// error card.
    std::function<void()> on_clear_session;
    /// When the error was first displayed (for live countdown).  Set by the
    /// interactive component at construction time; static renders may leave
    /// it unset (shows raw retry_after_ms value instead).
    std::optional<std::chrono::steady_clock::time_point> error_start_time;
};

// ============================================================
// Color helpers
// ============================================================

/// Resolve effective severity from explicit value + HTTP status hints.
/// 5xx -> Critical, 401/403 -> Warning, other 4xx -> Error.
[[nodiscard]] inline ErrorSeverity resolve_severity(const APIErrorData& e) {
    if (e.http_status) {
        int s = *e.http_status;
        if (s >= 500) return ErrorSeverity::Critical;
        if (s == 401 || s == 403) return ErrorSeverity::Warning;
        if (s >= 400) return ErrorSeverity::Error;
    }
    return e.severity;
}

struct ErrorPalette {
    Color border;
    Color accent;
    Color title;
    Color body_bg;
    const char* badge;
};

[[nodiscard]] inline ErrorPalette palette(const APIErrorData& e) {
    switch (resolve_severity(e)) {
        case ErrorSeverity::Warning:
            return {loom::ui::design::theme::current_theme().palette->warning, loom::ui::design::theme::current_theme().palette->warning, loom::ui::design::theme::current_theme().palette->warning,
                    loom::ui::design::theme::current_theme().palette->warning_tint_bg, loom::ui::design::figures::kWarning.data()};
        case ErrorSeverity::Critical:
            return {loom::ui::design::theme::current_theme().palette->danger, loom::ui::design::theme::current_theme().palette->danger, loom::ui::design::theme::current_theme().palette->danger,
                    loom::ui::design::theme::current_theme().palette->danger_tint_bg, loom::ui::design::figures::kCross.data()};
        case ErrorSeverity::Error:
        default:
            return {loom::ui::design::theme::current_theme().palette->danger, loom::ui::design::theme::current_theme().palette->danger, loom::ui::design::theme::current_theme().palette->danger,
                    loom::ui::design::theme::current_theme().palette->danger_tint_bg, loom::ui::design::figures::kCross.data()};
    }
}

// ============================================================
// Rendering
// ============================================================

/// Render a single "pill" button: [<key> Label]
[[nodiscard]] inline Element pill_button(const std::string& key_hint,
                                          const std::string& label,
                                          Color c = loom::ui::design::theme::current_theme().palette->text) {
    return message_key_hint(key_hint, label) | color(c);
}

/// Render the full error card (static element view)
[[nodiscard]] inline Element RenderAPIError(const APIErrorOptions& opts, bool keyboard_interactive = true) {
    const auto& e = opts.error;
    auto p = palette(e);

    Elements rows;

    // Identify the failing operation before its provider diagnostics.
    {
        Elements parts;
        parts.push_back(text(std::string(" ") + p.badge + " ") | bold | color(p.accent));
        parts.push_back(text("Model request failed") | bold | color(p.title));
        if (e.http_status)
            parts.push_back(text(std::format(" · HTTP {}", *e.http_status)) | color(p.title));
        if (!e.provider.empty())
            parts.push_back(text(" · " + e.provider) | color(loom::ui::design::theme::current_theme().palette->muted));
        if (!e.error_code.empty())
            parts.push_back(text(" · " + e.error_code) | color(p.accent));
        parts.push_back(filler());
        if (e.current_attempt && e.max_attempts) {
            parts.push_back(text(std::format(" attempt {}/{}",
                                             *e.current_attempt, *e.max_attempts))
                            | dim | color(p.title));
        }
        rows.push_back(hbox(parts));
    }

    // --- Row 3: error message (collapsed / expanded body) ---
    {
        std::string body = e.message.empty()
            ? std::string("(no error message from provider)")
            : e.message;
        const auto hidden_chars = keyboard_interactive && e.collapsed && body.size() > e.max_chars_collapsed
            ? body.size() - e.max_chars_collapsed : 0;
        if (hidden_chars > 0) body.resize(e.max_chars_collapsed);
        rows.push_back(paragraph(" " + body) | color(loom::ui::design::theme::current_theme().palette->text));
        if (hidden_chars > 0) {
            rows.push_back(hbox({text(std::format(" … {} chars hidden", hidden_chars)),
                                 message_key_hint("Enter", "Show more")}));
        }

        if (e.truncated || e.response_bytes) {
            Elements warn;
            warn.push_back(text(" "));
            if (e.response_bytes) {
                warn.push_back(text(std::format("response body: {} bytes",
                                                *e.response_bytes)) | dim);
            }
            if (e.truncated) {
                if (e.response_bytes) warn.push_back(text(" · ") | dim);
                warn.push_back(text("truncated preview") | color(p.accent) | dim);
            }
            rows.push_back(hbox(warn));
        }
    }

    // --- Row 3b: retry countdown text ---
    // Shows "Retrying in X seconds… (attempt N/M)" when retry_after_ms is set.
    // The live countdown is computed from error_start_time + retry_after_ms.
    if (e.retry_after_ms && e.current_attempt && e.max_attempts) {
        // Compute remaining seconds from the start time stored in error data.
        // When start_time is not set (static render path), show raw retry_after_ms.
        double remaining_sec = 0.0;
        if (opts.error_start_time) {
            auto now = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(
                now - *opts.error_start_time).count();
            remaining_sec = std::max(0.0,
                (*e.retry_after_ms - elapsed_ms) / 1000.0);
        } else {
            remaining_sec = *e.retry_after_ms / 1000.0;
        }
        int secs_int = static_cast<int>(std::round(remaining_sec));
        std::string unit = (secs_int == 1) ? "second" : "seconds";
        std::string countdown_text = std::format(
            " Retrying in {} {}… (attempt {}/{})",
            secs_int, unit, *e.current_attempt, *e.max_attempts);
        rows.push_back(text(countdown_text) | dim | color(loom::ui::design::theme::current_theme().palette->muted));
    }
    if (opts.show_trace_ids) {
        Elements ids;
        auto add_id = [&](const char* label, const std::string& val) {
            if (val.empty()) return;
            ids.push_back(text(std::format(" {}: ", label)) | dim);
            ids.push_back(text(val) | color(loom::ui::design::theme::current_theme().palette->suggestion) | bold);
        };
        add_id("trace-id",   e.trace_id);
        add_id("request-id", e.request_id);
        if (!ids.empty()) {
            ids.insert(ids.begin(), text(" ") | dim);
            if (keyboard_interactive && opts.copyable_trace && opts.on_copy_trace) {
                ids.push_back(filler());
                ids.push_back(message_key_hint("T", "Copy trace"));
            }
            rows.push_back(hbox(ids));
        }
    }

    // --- Row 5: Retry / Diagnose / Dismiss / Clear session action buttons ---
    if (keyboard_interactive && opts.show_buttons) {
        Elements bts;
        bts.push_back(text(" "));
        if (opts.on_retry && !e.session_expired && e.http_status != 401 && e.http_status != 403) {
            bts.push_back(pill_button("R", "Retry request", loom::ui::design::theme::current_theme().palette->success));
            bts.push_back(text("  "));
        }
        if (opts.on_clear_session && e.session_expired) {
            bts.push_back(pill_button("c", "Clear session", loom::ui::design::theme::current_theme().palette->warning));
            bts.push_back(text("  "));
        }
        if (opts.on_diagnose) {
            bts.push_back(pill_button("d", "Diagnose", loom::ui::design::theme::current_theme().palette->info));
            bts.push_back(text("  "));
        }
        if (opts.dismissible && opts.on_dismiss) {
            bts.push_back(pill_button("q", "Dismiss", loom::ui::design::theme::current_theme().palette->muted));
        }
        if (e.message.size() > e.max_chars_collapsed) {
            bts.push_back(filler());
            bts.push_back(message_key_hint("Enter/Space", e.collapsed ? "Expand" : "Collapse"));
        }
        if (bts.size() > 1) rows.push_back(hbox(bts));
    }

    return vbox(rows);
}

// ============================================================
// Interactive Component
// ============================================================

[[nodiscard]] inline Component APIErrorMessage(APIErrorOptions options) {
    struct State {
        APIErrorOptions opts;
    };
    auto s = std::make_shared<State>();
    s->opts = std::move(options);
    // Capture the start time so the renderer can compute live remaining
    // seconds (remaining = retry_after_ms - (now - error_start_time)).
    if (!s->opts.error_start_time) {
        s->opts.error_start_time = std::chrono::steady_clock::now();
    }

    return Renderer([s] { return RenderAPIError(s->opts); })
        | CatchEvent([s](Event event) -> bool {
              auto& o = s->opts;
              auto& e = o.error;

              if ((event == Event::Return || event == Event::Character(' ')) &&
                  e.message.size() > e.max_chars_collapsed) {
                  e.collapsed = !e.collapsed;
                  return true;
              }
              if (event == Event::Character('r') || event == Event::Character('R')) {
                  if (o.on_retry && !e.session_expired && e.http_status != 401 && e.http_status != 403) {
                      o.on_retry();
                      return true;
                  }
              }
              if (event == Event::Character('c') || event == Event::Character('C')) {
                  if (o.on_clear_session && e.session_expired) {
                      o.on_clear_session();
                      return true;
                  }
              }
              if (event == Event::Character('d') || event == Event::Character('D')) {
                  if (o.on_diagnose) { o.on_diagnose(); return true; }
              }
              if (event == Event::Character('q') || event == Event::Character('Q') ||
                  event == Event::Escape) {
                  if (o.dismissible && o.on_dismiss) {
                      o.on_dismiss();
                      return true;
                  }
              }
              if (event == Event::Character('t') || event == Event::Character('T')) {
                  if (o.copyable_trace && o.on_copy_trace) {
                      std::string joined;
                      if (!e.trace_id.empty())   joined += "trace-id=" + e.trace_id;
                      if (!e.request_id.empty()) {
                          if (!joined.empty()) joined += ", ";
                          joined += "request-id=" + e.request_id;
                      }
                      o.on_copy_trace(joined);
                      return true;
                  }
              }
              return false;
          });
}

} // namespace loom::ui::messages::api_error_message
