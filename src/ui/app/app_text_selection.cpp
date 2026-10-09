// app_text_selection.cpp — impl unit for drag-to-select text selection.
// Extracted from app_autocomplete.cpp (RFC: P2-1e split) to keep the
// SelectionHighlightNode class, text extraction, and mouse drag state
// machine out of the autocomplete/render/event impl units.
//
// Contains: SelectionHighlightNode (custom FTXUI Node),
//           ExtractSelectedText, NotifyCopyComplete,
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

import loom.commands.copy_cmd;           // copy_to_clipboard_osc52
import loom.diagnostics.debug;            // debug() (filtered by namespace)
import loom.diagnostics.log;              // log::add_to_in_memory_errors
import loom.ui.foundation.theme_provider; // current_theme().palette
import loom.ui.prompt.prompt_input_footer; // NotificationQueue / NotificationItem

namespace loom::ui {

// ── Custom FTXUI Node ──────────────────────────────────────────────────
// Paints the selection rectangle's background after rendering children.
// Pattern follows FTXUI's own `inverted` decorator: render children first,
// then modify pixels in-place.
class SelectionHighlightNode : public ftxui::Node {
public:
    SelectionHighlightNode(ftxui::Element child,
                           int sel_x0, int sel_y0, int sel_x1, int sel_y1,
                           ftxui::Color bg_color)
        : ftxui::Node(ftxui::Elements{std::move(child)}),
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
        // Render children first, then paint selection background.
        ftxui::Node::Render(screen);
        const int sx = std::max(0, std::min(x0_, x1_));
        const int sy = std::max(0, std::min(y0_, y1_));
        const int ex = std::min(screen.dimx() - 1, std::max(x0_, x1_));
        const int ey = std::min(screen.dimy() - 1, std::max(y0_, y1_));
        for (int y = sy; y <= ey; ++y) {
            for (int x = sx; x <= ex; ++x) {
                screen.PixelAt(x, y).background_color = bg_color_;
            }
        }
    }

private:
    int x0_, y0_, x1_, y1_;
    ftxui::Color bg_color_;
};

// ── Helpers ────────────────────────────────────────────────────────────

/// Extract text from the screen between (x0,y0) and (x1,y1).
/// Reads rendered pixels directly via Screen::PixelAt, trims trailing
/// whitespace per line, and joins lines with newlines.
[[nodiscard]] static std::string ExtractSelectedText(
    ftxui::Screen* screen, int x0, int y0, int x1, int y1) {
    const int sx = std::max(0, std::min(x0, x1));
    const int sy = std::max(0, std::min(y0, y1));
    const int ex = std::min(screen->dimx() - 1, std::max(x0, x1));
    const int ey = std::min(screen->dimy() - 1, std::max(y0, y1));
    std::string result;
    for (int y = sy; y <= ey; ++y) {
        std::string line;
        for (int x = sx; x <= ex; ++x) {
            line += screen->PixelAt(x, y).character;
        }
        // Trim trailing whitespace (selection rectangles often extend
        // past the actual text into padding).
        while (!line.empty() &&
               (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (!result.empty()) result += '\n';
        result += line;
    }
    return result;
}

/// Enqueue a "Copied N bytes (backend)" footer notification.
static void NotifyCopyComplete(
    loom::ui::prompt::footer::NotificationQueue& queue,
    std::size_t bytes, std::string_view backend) {
    namespace pif = loom::ui::prompt::footer;
    pif::NotificationItem item;
    item.key = "text-selection-copied";
    item.text = std::format("Copied {} bytes ({})", bytes, backend);
    item.color = "";
    item.priority = pif::NotificationPriority::Immediate;
    item.timeout_ms = 2000;
    pif::QueueAddNotification(queue, item);
}

/// Enqueue a "Copying..." footer notification.
static void NotifyCopying(
    loom::ui::prompt::footer::NotificationQueue& queue) {
    namespace pif = loom::ui::prompt::footer;
    pif::NotificationItem item;
    item.key = "text-selection-copying";
    item.text = "Copying...";
    item.color = "";
    item.priority = pif::NotificationPriority::Immediate;
    item.timeout_ms = 2000;
    pif::QueueAddNotification(queue, item);
}

/// Enqueue a "Copy requested via terminal" footer notification (OSC 52).
static void NotifyOsc52Emitted(
    loom::ui::prompt::footer::NotificationQueue& queue) {
    namespace pif = loom::ui::prompt::footer;
    pif::NotificationItem item;
    item.key = "text-selection-osc52";
    item.text = "Copy requested via terminal (OSC 52)";
    item.color = "";
    item.priority = pif::NotificationPriority::Immediate;
    item.timeout_ms = 2000;
    pif::QueueAddNotification(queue, item);
}

/// Enqueue a "Copy failed" footer notification.
static void NotifyCopyFailed(
    loom::ui::prompt::footer::NotificationQueue& queue) {
    namespace pif = loom::ui::prompt::footer;
    pif::NotificationItem item;
    item.key = "text-selection-failed";
    item.text = "Copy failed — select text in terminal and use Ctrl+Shift+C";
    item.color = "";
    item.priority = pif::NotificationPriority::Immediate;
    item.timeout_ms = 4000;
    pif::QueueAddNotification(queue, item);
}

// ── AppAdapter member functions ────────────────────────────────────────

Element AppAdapter::ApplySelectionHighlight(Element el) {
    if (!text_selection_.active) return el;
    namespace theme = loom::ui::design::theme;
    const auto& pal = *theme::current_theme().palette;
    return std::make_shared<SelectionHighlightNode>(
        std::move(el),
        text_selection_.start_x, text_selection_.start_y,
        text_selection_.end_x, text_selection_.end_y,
        pal.selection_bg);
}

bool AppAdapter::HandleTextSelectionMouse(Event event) {
    if (!event.is_mouse()) return false;
    const auto& mouse = event.mouse();

    if (mouse.button == Mouse::Left &&
        mouse.motion == Mouse::Pressed) {
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
            // Extract text from screen and copy to clipboard.
            if (auto* screen = screen_.load(std::memory_order_acquire)) {
                std::string text = ExtractSelectedText(
                    screen,
                    text_selection_.start_x, text_selection_.start_y,
                    text_selection_.end_x, text_selection_.end_y);
                if (!text.empty()) {
                    // RFC 0007 Phase 1: async copy pipeline.
                    // Advance generation counter for stale-result detection.
                    auto gen = copy_generation_.fetch_add(
                        1, std::memory_order_acq_rel) + 1;

                    // Show "Copying..." immediately.
                    NotifyCopying(screen_state_->footer_notification_queue);

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
                }
            }
            text_selection_.potential = false;
            text_selection_.active = false;
            PostRenderEvent();
            return true;  // consume — don't trigger click handlers
        }
        // Click without drag — let it pass through to click handlers.
        text_selection_.potential = false;
    }

    return false;
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

    auto& queue = screen_state_->footer_notification_queue;
    const auto byte_count = pending->text ? pending->text->size() : 0;

    if (r.status ==
        loom::utils::clipboard::CopyResult::Status::NativeCopied) {
        // Native clipboard succeeded.
        NotifyCopyComplete(queue, byte_count, r.backend_name);
    } else {
        // Native failed or unsupported (SSH/no backend) — try OSC 52.
        // OSC 52 is written on the UI thread (terminal-output ownership
        // invariant: only the UI thread writes to stdout/stderr).
        if (pending->text) {
            auto osc52 = loom::commands::copy_to_clipboard_osc52(
                *pending->text);
            if (osc52) {
                NotifyOsc52Emitted(queue);
            } else {
                NotifyCopyFailed(queue);
            }
        } else {
            NotifyCopyFailed(queue);
        }
    }
}

}  // namespace loom::ui
