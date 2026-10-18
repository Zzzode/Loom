// Settings-local cell wrapping, row packing and allocated-box layout.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.dialogs.settings_dialog;

import std;

namespace loom::ui::dialogs::settings_dialog {

[[nodiscard]] Element SettingsText(const std::string& value, int columns) {
    if (columns <= 0) return text(value);
    Elements lines;
    std::size_t start = 0;
    do {
        const auto end = value.find('\n', start);
        const auto logical = value.substr(start, end == std::string::npos ? end : end - start);
        std::string line;
        int cells = 0;
        for (const auto& glyph : Utf8ToGlyphs(logical)) {
            if (glyph.empty()) continue;
            const int width = string_width(glyph);
            if (cells + width > columns && !line.empty()) {
                lines.push_back(text(line));
                line.clear();
                cells = 0;
            }
            line += glyph;
            cells += width;
        }
        lines.push_back(text(line));
        if (end == std::string::npos) break;
        start = end + 1;
    } while (start <= value.size());
    return vbox(std::move(lines));
}

[[nodiscard]] Element FocusedSetting(Element element) {
    return element | inverted | focus;
}

[[nodiscard]] int ValueColumns(int columns) {
    return columns <= 0 ? 0 : std::max(1, columns - (columns < 52 ? 2 : 30));
}

[[nodiscard]] Element SettingsFlow(Elements items, int columns, int gap) {
    Elements rows;
    Elements row;
    int used = 0;
    for (auto& item : items) {
        item->ComputeRequirement();
        const int width = item->requirement().min_x;
        if (columns > 0 && !row.empty() && used + gap + width > columns) {
            rows.push_back(hbox(std::move(row)));
            row.clear();
            used = 0;
        }
        if (!row.empty()) {
            row.push_back(text(std::string(gap, ' ')));
            used += gap;
        }
        row.push_back(std::move(item));
        used += width;
    }
    if (!row.empty()) rows.push_back(hbox(std::move(row)));
    return vbox(std::move(rows));
}

SettingsLayoutNode::SettingsLayoutNode(std::function<Element(int, int)> render)
    : render_(std::move(render)) {}

void SettingsLayoutNode::ComputeRequirement() {
    if (children_.empty() || columns_ != computed_columns_ || rows_ != computed_rows_)
        children_ = {render_(columns_, rows_)};
    children_[0]->ComputeRequirement();
    requirement_ = children_[0]->requirement();
    requirement_.min_x = 80;
    requirement_.min_y = 26;
    requirement_.flex_shrink_x = 1;
    requirement_.flex_shrink_y = 1;
    computed_columns_ = columns_;
    computed_rows_ = rows_;
}

void SettingsLayoutNode::SetBox(Box box) {
    Node::SetBox(box);
    columns_ = std::max(1, box.x_max - box.x_min + 1);
    rows_ = std::max(1, box.y_max - box.y_min + 1);
    children_[0]->SetBox(box);
}

void SettingsLayoutNode::Check(Status* status) {
    Node::Check(status);
    status->need_iteration |= columns_ != computed_columns_ || rows_ != computed_rows_;
}

} // namespace loom::ui::dialogs::settings_dialog
