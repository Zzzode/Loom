// app_text_selection.cpp — impl unit for drag-to-select text selection.
// Extracted from app_autocomplete.cpp (RFC: P2-1e split) to keep the
// SelectionHighlightNode class, text extraction, and mouse drag state
// machine out of the autocomplete/render/event impl units.
//
// Contains: SelectionHighlightNode (custom FTXUI Node),
//           ExtractSelectedText, NotifyCopyComplete,
//           AppAdapter::HandleTextSelectionMouse,
//           AppAdapter::ApplySelectionHighlight.
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

/// Enqueue a "Copied N bytes" footer notification.
static void NotifyCopyComplete(
    loom::ui::prompt::footer::NotificationQueue& queue,
    std::size_t bytes) {
    namespace pif = loom::ui::prompt::footer;
    pif::NotificationItem item;
    item.key = "text-selection-copied";
    item.text = std::format("Copied {} bytes", bytes);
    item.color = "";
    item.priority = pif::NotificationPriority::Immediate;
    item.timeout_ms = 2000;
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
            if (dx * dx + dy * dy > 9) {  // 3px threshold
                text_selection_.active = true;
                PostRenderEvent();
            }
            if (text_selection_.active) return true;  // consume motion
        }
        return false;
    }

    if (mouse.button == Mouse::Left &&
        mouse.motion == Mouse::Released) {
        if (text_selection_.active) {
            // Extract text from screen and copy to clipboard.
            if (auto* screen = screen_.load(std::memory_order_acquire)) {
                std::string text = ExtractSelectedText(
                    screen,
                    text_selection_.start_x, text_selection_.start_y,
                    text_selection_.end_x, text_selection_.end_y);
                if (!text.empty()) {
                    (void)loom::commands::copy_to_clipboard_osc52(text);
                    NotifyCopyComplete(
                        screen_state_->footer_notification_queue,
                        text.size());
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

}  // namespace loom::ui
