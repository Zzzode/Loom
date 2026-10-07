// code_highlight_impl.cpp — impl unit for loom.ui.visual.code_highlight.
// Owns the ColoredTextLine Node (a single-line colored-segment renderer that
// truncates at the right edge instead of proportionally shrinking) and the
// colored_text_line() factory body.  The declarations stay in
// code_highlight.cppm so ColoredSegment and the factory remain part of the
// exported interface (markdown_render_code_impl.cpp constructs segments and
// calls the factory); the Node subclass and the body move here to keep the
// inline-def ratchet (inline_def_check.py) at its frozen count.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.visual.code_highlight;

import std;

namespace loom::ui::code_highlight {

namespace detail {

/// A single-line element that renders colored text segments left-to-right,
/// truncating at the box's right edge. hbox({text(...) | color(...), ...})
/// routes overwidth lines through box_helper::ComputeShrinkHard, which scales
/// every non-shrinkable child down proportionally — swallowing characters
/// from the middle of each token. This node keeps every segment at its full
/// width and stops rendering at box_.x_max, so characters are lost only at
/// the right edge, never from the middle.
class ColoredTextLine : public Node {
 public:
    explicit ColoredTextLine(std::vector<ColoredSegment> segments)
        : segments_(std::move(segments)) {}

    void ComputeRequirement() override {
        requirement_.min_x = 0;
        for (const auto& s : segments_) {
            requirement_.min_x += string_width(s.text);
        }
        requirement_.min_y = 1;
    }

    void Render(Screen& screen) override {
        int x = box_.x_min;
        const int y = box_.y_min;
        if (y > box_.y_max) {
            return;
        }
        for (const auto& seg : segments_) {
            for (const auto& cell : Utf8ToGlyphs(seg.text)) {
                if (x > box_.x_max) {
                    return;
                }
                if (cell == "\n") {
                    continue;
                }
                auto& pixel = screen.PixelAt(x, y);
                pixel.character = cell;
                if (seg.foreground != Color::Default) {
                    pixel.foreground_color = seg.foreground;
                }
                if (seg.background != Color::Default) {
                    pixel.background_color = seg.background;
                }
                ++x;
            }
        }
    }

 private:
    std::vector<ColoredSegment> segments_;
};

}  // namespace detail

Element colored_text_line(std::vector<ColoredSegment> segments) {
    return std::make_shared<detail::ColoredTextLine>(std::move(segments));
}

}  // namespace loom::ui::code_highlight
