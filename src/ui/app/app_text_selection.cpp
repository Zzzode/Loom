// app_text_selection.cpp — impl unit for drag-to-select text selection.
// Extracted from app_autocomplete.cpp (RFC: P2-1e split) to keep the
// SelectionHighlightNode class, text extraction, and mouse drag state
// machine out of the autocomplete/render/event impl units.
//
// Contains: SelectionHighlightNode (custom FTXUI Node),
//           ExtractSelectedText,
//           AppAdapter::HandleTextSelectionMouse,
//           AppAdapter::ApplySelectionHighlight,
//           AppAdapter::DrainCopyResult.
//
// RFC 0007 Phase 1: copy pipeline restructured to use ClipboardCoordinator
// (async, SSH-aware, honest four-state feedback). Threshold fixed (D-4).
// Release endpoint updated when selection is active (D-8).
module;

#include <algorithm>
#include <string>
#include <string_view>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

module loom.ui.app.app;

import std;

import loom.platform.clipboard;           // emit_osc52
import loom.diagnostics.debug;            // debug() (filtered by namespace)
import loom.diagnostics.log;              // log::add_to_in_memory_errors
import loom.ui.foundation.theme_provider; // current_theme().palette

namespace loom::ui {

// Normalize in row-major order, keeping both cells of any selected wide glyph.
// FTXUI uses an empty character for a wide glyph's continuation; ordinary
// blank cells contain a space. Highlighting and extraction share this range.
struct SelectionRange {
    int x0, y0, x1, y1;

    std::pair<int, int> columns(int y, int width) const {
        return {y == y0 ? x0 : 0, y == y1 ? x1 : width - 1};
    }
};

static SelectionRange NormalizeSelection(
    const ftxui::Screen& screen, int x0, int y0, int x1, int y1) {
    if (screen.dimx() == 0 || screen.dimy() == 0) return {0, 0, -1, -1};
    x0 = std::clamp(x0, 0, screen.dimx() - 1);
    x1 = std::clamp(x1, 0, screen.dimx() - 1);
    y0 = std::clamp(y0, 0, screen.dimy() - 1);
    y1 = std::clamp(y1, 0, screen.dimy() - 1);
    if (std::tie(y0, x0) > std::tie(y1, x1)) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    if (x0 > 0 && screen.PixelAt(x0, y0).character.empty()) --x0;
    if (x1 + 1 < screen.dimx() && screen.PixelAt(x1 + 1, y1).character.empty()) ++x1;
    return {x0, y0, x1, y1};
}

// ── Custom FTXUI Node ──────────────────────────────────────────────────
// Paints the linear selection's background after rendering children.
// Pattern follows FTXUI's own `inverted` decorator: render children first,
// then modify pixels in-place.
class SelectionHighlightNode : public ftxui::Node {
public:
    SelectionHighlightNode(ftxui::Element child,
                           std::shared_ptr<ftxui::Screen> frame,
                           bool active,
                           int sel_x0, int sel_y0, int sel_x1, int sel_y1,
                           ftxui::Color bg_color)
        : ftxui::Node(ftxui::Elements{std::move(child)}),
          frame_(std::move(frame)), active_(active),
          x0_(sel_x0), y0_(sel_y0), x1_(sel_x1), y1_(sel_y1),
          bg_color_(bg_color) {}

    void ComputeRequirement() override {
        ftxui::Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }

    void SetBox(ftxui::Box box) override {
        ftxui::Node::SetBox(box);
        children_[0]->SetBox(box);
    }

    void Render(ftxui::Screen& screen) override {
        // Capture pixels before ScreenInteractive::Draw() flushes and clears
        // them. Capture even before a drag begins, for press/release events
        // delivered without an intervening render or motion event.
        ftxui::Node::Render(screen);
        if (frame_) *frame_ = screen;
        if (!active_) return;
        const auto range = NormalizeSelection(screen, x0_, y0_, x1_, y1_);
        for (int y = range.y0; y <= range.y1; ++y) {
            const auto [sx, ex] = range.columns(y, screen.dimx());
            for (int x = sx; x <= ex; ++x) {
                screen.PixelAt(x, y).background_color = bg_color_;
            }
        }
    }

private:
    std::shared_ptr<ftxui::Screen> frame_;
    bool active_;
    int x0_, y0_, x1_, y1_;
    ftxui::Color bg_color_;
};

// ── Helpers ────────────────────────────────────────────────────────────

/// Extract text from the screen between (x0,y0) and (x1,y1).
/// Reads rendered pixels directly via Screen::PixelAt, trims trailing
/// whitespace per line, and joins lines with newlines.
[[nodiscard]] static std::string ExtractSelectedText(
    const ftxui::Screen* screen, int x0, int y0, int x1, int y1) {
    const auto range = NormalizeSelection(*screen, x0, y0, x1, y1);
    std::string result;
    for (int y = range.y0; y <= range.y1; ++y) {
        const auto [sx, ex] = range.columns(y, screen->dimx());
        std::string line;
        for (int x = sx; x <= ex; ++x) {
            line += screen->PixelAt(x, y).character;
        }
        // Trim trailing whitespace (selected rows often extend
        // past the actual text into padding).
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (y != range.y0) result += '\n';
        result += line;
    }
    return result;
}

/// Count UTF-8 characters (not bytes) in a string.
/// Non-continuation bytes (0b10xxxxxx) are skipped; every other byte
/// starts a new character.
[[nodiscard]] static std::size_t CountUtf8Chars(std::string_view s) {
    std::size_t count = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

// ── AppAdapter member functions ────────────────────────────────────────

Element AppAdapter::ApplySelectionHighlight(Element el) {
    if (screen_.load(std::memory_order_acquire) && !text_selection_frame_) {
        text_selection_frame_ = std::make_shared<ftxui::Screen>(0, 0);
    }
    if (!text_selection_.active && !text_selection_frame_) return el;
    namespace theme = loom::ui::design::theme;
    const auto& pal = *theme::current_theme().palette;
    return std::make_shared<SelectionHighlightNode>(
        std::move(el),
        text_selection_frame_, text_selection_.active,
        text_selection_.start_x, text_selection_.start_y,
        text_selection_.end_x, text_selection_.end_y,
        pal.selection_bg);
}

bool AppAdapter::HandleTextSelectionMouse(Event event) {
    if (!event.is_mouse() || !mouse_mode_ || !mouse_tracking_on_) return false;
    const auto& mouse = event.mouse();

    if (mouse.button == Mouse::Left &&
        mouse.motion == Mouse::Pressed) {
        // If a selection is active but the mouse button was released
        // (potential=false), a new press clears it and starts fresh.
        // This covers all cases: copy succeeded, copy failed, or the
        // selection was made without a screen (tests).
        if (text_selection_.active && !text_selection_.potential) {
            ClearTextSelection();
            PostRenderEvent();
        }
        if (!text_selection_.potential && !text_selection_.active) {
            // Initial press — record position, don't consume.
            text_selection_.potential = true;
            text_selection_.start_x = text_selection_.end_x = mouse.x;
            text_selection_.start_y = text_selection_.end_y = mouse.y;
            return false;
        }
        if (text_selection_.potential) {
            // Motion event (FTXUI encodes motion as Pressed).
            text_selection_.end_x = mouse.x;
            text_selection_.end_y = mouse.y;
            const int dx = mouse.x - text_selection_.start_x;
            const int dy = mouse.y - text_selection_.start_y;
            // RFC 0007 D-4: threshold is dx²+dy² > 0 (any displacement
            // beyond the starting cell), not > 9 (3 cells).
            if (dx * dx + dy * dy > 0) {
                text_selection_.active = true;
                PostRenderEvent();
            }
            if (text_selection_.active) return true;  // consume motion
        }
        return false;
    }

    if (mouse.button == Mouse::Left &&
        mouse.motion == Mouse::Released) {
        // No-motion fallback: terminals that don't deliver mode-1003 motion
        // events (e.g. macOS Terminal.app, tmux without `mouse on`) only
        // send press + release. Detect the drag by displacement on release.
        if (!text_selection_.active && text_selection_.potential) {
            const int dx = mouse.x - text_selection_.start_x;
            const int dy = mouse.y - text_selection_.start_y;
            // RFC 0007 D-4: threshold is dx²+dy² > 0.
            if (dx * dx + dy * dy > 0) {
                text_selection_.active = true;
            }
        }
        // RFC 0007 D-8: always update the release endpoint when selection
        // is active, even if the endpoint was already set during motion.
        // This ensures the final position is captured on terminals that
        // deliver release events with updated coordinates.
        if (text_selection_.active) {
            text_selection_.end_x = mouse.x;
            text_selection_.end_y = mouse.y;
        }
        if (text_selection_.active) {
            // Extract the displayed frame, not ScreenInteractive's cleared
            // working buffer, and copy to clipboard.
            if (auto* screen = text_selection_frame_.get()) {
                std::string text = ExtractSelectedText(
                    screen,
                    text_selection_.start_x, text_selection_.start_y,
                    text_selection_.end_x, text_selection_.end_y);
                if (!text.empty()) {
                    text_selection_.copied = false;
                    text_selection_.copy_feedback = "Copying…";
                    // RFC 0007 Phase 1: async copy pipeline.
                    // Advance generation counter for stale-result detection.
                    auto gen = copy_generation_.fetch_add(
                        1, std::memory_order_acq_rel) + 1;

                    // Post async native clipboard copy.
                    auto text_ptr = std::make_shared<const std::string>(
                        std::move(text));
                    [[maybe_unused]] auto posted_gen = clipboard_coordinator_.post({
                        .text = text_ptr,
                        .on_complete = [this, gen, text_ptr](
                            loom::utils::clipboard::CopyResult result) {
                            // Store result for UI thread to drain.
                            {
                                std::lock_guard lock(copy_result_mutex_);
                                pending_copy_result_ = PendingCopyResult{
                                    .generation = gen,
                                    .result = std::move(result),
                                    .text = text_ptr,
                                };
                            }
                            PostRenderEvent();
                        },
                    });
                    // Keep the selection visible. Success feedback is set
                    // only when the native backend confirms completion.
                }
            }
            text_selection_.potential = false;
            // Don't clear active — the selection stays highlighted after
            // copy, matching Claude Code's behavior.  Cleared on the next
            // mouse press or key press.
            PostRenderEvent();
            return true;  // consume — don't trigger click handlers
        }
        // Click without drag — let it pass through to click handlers.
        text_selection_.potential = false;
    }

    return false;
}

void AppAdapter::ClearTextSelection() {
    // A cleared/replaced selection must not emit a late OSC 52 fallback.
    copy_generation_.fetch_add(1, std::memory_order_acq_rel);
    text_selection_.active = false;
    text_selection_.potential = false;
    text_selection_.copied = false;
    text_selection_.copied_chars = 0;
    text_selection_.copy_feedback.reset();
    screen_state_->copy_toast_text.reset();
}

void AppAdapter::DrainCopyResult() {
    // Drain pending clipboard copy result from the worker thread.
    std::optional<PendingCopyResult> pending;
    {
        std::lock_guard lock(copy_result_mutex_);
        pending = std::move(pending_copy_result_);
        pending_copy_result_.reset();
    }
    if (!pending) return;

    // Stale result? Discard (a newer selection was posted).
    auto current_gen = copy_generation_.load(std::memory_order_acquire);
    if (pending->generation != current_gen) return;

    // RFC 0007 §11.0 item 4: diagnostics — log backend/status/duration/bytes
    // to the in-memory error buffer for failures, and to the debug channel
    // for all results. Never logs selected text content.
    const auto& r = pending->result;
    loom::utils::debug("clipboard",
        "copy result: backend={} status={} bytes={} duration={}ms detail={}",
        r.backend_name,
        r.status == loom::utils::clipboard::CopyResult::Status::NativeCopied
            ? "copied" : "failed",
        r.bytes_sent,
        r.duration.count(),
        r.error_detail);
    if (r.status == loom::utils::clipboard::CopyResult::Status::NativeFailed) {
        loom::utils::log::add_to_in_memory_errors(std::format(
            "clipboard copy failed: backend={} bytes={} duration={}ms detail={}",
            r.backend_name, r.bytes_sent, r.duration.count(),
            r.error_detail));
    }

    if (r.status ==
        loom::utils::clipboard::CopyResult::Status::NativeCopied) {
        if (text_selection_.active && !text_selection_.potential && pending->text) {
            text_selection_.copied = true;
            text_selection_.copied_chars = CountUtf8Chars(*pending->text);
            text_selection_.copy_feedback = std::format(
                "Copied {} chars to clipboard", text_selection_.copied_chars);
        }
    } else {
        // Only native success can confirm clipboard acceptance.
        text_selection_.copied = false;
        // Native failed or unsupported (SSH/no backend) — try OSC 52.
        // OSC 52 is written on the UI thread (terminal-output ownership
        // invariant: only the UI thread writes to stdout/stderr).
        if (pending->text) {
            auto osc52 = loom::utils::clipboard::emit_osc52(
                *pending->text);
            if (osc52) {
                text_selection_.copy_feedback = "Copy sent to terminal · paste to verify";
            } else {
                text_selection_.copy_feedback = "Copy failed · try again";
            }
        } else {
            text_selection_.copy_feedback = "Copy failed · try again";
        }
    }
}

std::string AppTestingSeams::selected_text_for_testing(
    const ftxui::Screen& screen, int x0, int y0, int x1, int y1) {
    return ExtractSelectedText(&screen, x0, y0, x1, y1);
}

ftxui::Element AppTestingSeams::selection_highlight_for_testing(
    ftxui::Element child, int x0, int y0, int x1, int y1) {
    return std::make_shared<SelectionHighlightNode>(
        std::move(child), nullptr, true, x0, y0, x1, y1,
        loom::ui::design::theme::current_theme().palette->selection_bg);
}

}  // namespace loom::ui
