module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/string.hpp>

module loom.ui.features.agents.instance_browser;

import std;
import loom.ui.foundation.theme_provider;

namespace loom::ui::agents::instances {
namespace {
using namespace ftxui;

// Never let stored output inject terminal protocols into the rendered surface.
std::string clean_text(std::string_view input) {
    std::string result;
    for (std::size_t i = 0; i < input.size();) {
        const auto ch = static_cast<unsigned char>(input[i]);
        if (ch == 0x1b) {
            ++i;
            if (i == input.size()) break;
            if (input[i] == '[') {
                ++i;
                while (i < input.size()) {
                    const auto c = static_cast<unsigned char>(input[i++]);
                    if (c >= 0x40 && c <= 0x7e) break;
                }
            } else if (input[i] == ']' || input[i] == 'P' || input[i] == 'X' || input[i] == '^' ||
                       input[i] == '_') {
                const bool osc = input[i] == ']';
                ++i;
                while (i < input.size()) {
                    if (osc && input[i] == '\a') {
                        ++i;
                        break;
                    }
                    if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '\\') {
                        i += 2;
                        break;
                    }
                    ++i;
                }
            } else
                ++i;
            continue;
        }
        if (ch < 0x20 || ch == 0x7f) {
            if (ch == '\n')
                result += '\n';
            else if (ch == '\t')
                result += "    ";
            ++i;
            continue;
        }
        // Preserve complete valid UTF-8 sequences and discard malformed bytes.
        std::size_t bytes = ch < 0x80                  ? 1
                            : ch >= 0xc2 && ch <= 0xdf ? 2
                            : ch >= 0xe0 && ch <= 0xef ? 3
                            : ch >= 0xf0 && ch <= 0xf4 ? 4
                                                       : 0;
        bool valid = bytes != 0 && i + bytes <= input.size();
        for (std::size_t j = 1; valid && j < bytes; ++j)
            valid = (static_cast<unsigned char>(input[i + j]) & 0xc0) == 0x80;
        if (valid && bytes > 1) {
            const auto second = static_cast<unsigned char>(input[i + 1]);
            valid = !(ch == 0xe0 && second < 0xa0) && !(ch == 0xed && second >= 0xa0) &&
                    !(ch == 0xf0 && second < 0x90) && !(ch == 0xf4 && second >= 0x90);
            // UTF-8 encoded C1 controls are terminal controls too.
            if (ch == 0xc2 && second >= 0x80 && second <= 0x9f) valid = false;
        }
        if (valid) {
            result.append(input.substr(i, bytes));
            i += bytes;
        } else
            ++i;
    }
    return result;
}

std::string bounded_text(std::string_view raw, bool transcript) {
    constexpr std::size_t limit = 16 * 1024;
    bool truncated = raw.size() > limit;
    if (truncated) {
        raw.remove_prefix(raw.size() - limit);
        while (!raw.empty() && (static_cast<unsigned char>(raw.front()) & 0xc0) == 0x80)
            raw.remove_prefix(1);
    }
    auto clean = clean_text(raw);
    constexpr std::string_view marker = "[truncated; showing tail]\n";
    if (clean.size() > limit - marker.size()) {
        clean.erase(0, clean.size() - (limit - marker.size()));
        while (!clean.empty() && (static_cast<unsigned char>(clean.front()) & 0xc0) == 0x80)
            clean.erase(0, 1);
        truncated = true;
    }
    if (transcript) {
        std::size_t lines = 0;
        for (std::size_t i = clean.size(); i > 0; --i) {
            if (clean[i - 1] == '\n' && ++lines >= 80) {
                clean.erase(0, i);
                truncated = true;
                break;
            }
        }
    }
    if (truncated) clean = std::string(marker) + clean;
    return clean;
}

std::vector<std::string> wrapped(std::string_view raw, int width) {
    std::vector<std::string> rows;
    std::string line;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(std::string(raw))) {
        if (glyph == "\n") {
            rows.push_back(std::move(line));
            line.clear();
            used = 0;
            continue;
        }
        const int cells = string_width(glyph);
        if (used + cells > width && !line.empty()) {
            rows.push_back(std::move(line));
            line.clear();
            used = 0;
        }
        if (cells <= width) {
            line += glyph;
            used += cells;
        }
    }
    rows.push_back(std::move(line));
    return rows;
}

std::string clipped(std::string_view raw, int width) {
    auto clean = clean_text(raw);
    std::ranges::replace(clean, '\n', ' ');
    if (string_width(clean) <= width) return clean;
    std::string result;
    int used = 0;
    for (const auto& glyph : Utf8ToGlyphs(clean)) {
        const int cells = string_width(glyph);
        if (used + cells > std::max(0, width - 1)) break;
        result += glyph;
        used += cells;
    }
    if (width > 0) result += "…";
    return result;
}

class Browser final : public ComponentBase {
public:
    explicit Browser(Options options)
        : options_(std::move(options)) {
        if (!options_.state) options_.state = std::make_shared<BrowserState>();
        detail_open_ = options_.open_detail && !state().selected_id.empty();
        InputOption input_options;
        input_options.multiline = false;
        input_ = Input(&continuation_, "Continuation instruction", input_options);
        Add(input_);
    }

    Element Render() override {
        const auto& palette = *design::theme::current_theme().palette;
        width_ = std::max(1, state().columns);
        const int height = std::max(6, state().rows);
        row_boxes_.clear();
        row_ids_.clear();
        continue_box_ = {-1, -2, -1, -2};
        stop_box_ = {-1, -2, -1, -2};
        Elements rows;
        rows.push_back(text(clipped(detail_open_ ? "Agent instance" : "Agent instances", width_)) |
                       bold | color(palette.text));
        if (!detail_open_) {
            rows.push_back(
                text(clipped(std::format("{} · {}", state().team ? "Team Agents" : "Subagents",
                                         state().history ? "History" : "Active"),
                             width_)) |
                color(palette.primary));
            auto entries = visible_entries();
            synchronize_selection(entries);
            const int capacity = std::max(1, height - 5);
            const auto selected = selected_index(entries);
            const auto first = selected < static_cast<std::size_t>(capacity)
                                   ? 0
                                   : selected - static_cast<std::size_t>(capacity) + 1;
            row_boxes_.assign(entries.size(), Box{-1, -2, -1, -2});
            for (const auto& entry : entries)
                row_ids_.push_back(entry.id);
            if (entries.empty())
                rows.push_back(
                    text(state().history ? "  No ended instances." : "  No active instances.") |
                    color(palette.muted));
            for (std::size_t i = first;
                 i < entries.size() && i < first + static_cast<std::size_t>(capacity); ++i) {
                const auto& entry = entries[i];
                const auto status =
                    entry.cancelling ? "stopping" : runtime::status_name(entry.status);
                const int name_width = std::max(0, width_ - static_cast<int>(status.size()) - 4);
                auto row = text(clipped(
                    std::format("{}{} {}", entry.id == state().selected_id ? "> " : "  ",
                                clipped(entry.name.empty() ? entry.id : entry.name, name_width),
                                status),
                    width_));
                if (entry.id == state().selected_id)
                    row = row | bgcolor(palette.surface_tint_3) | color(palette.text) | bold;
                rows.push_back(row | reflect(row_boxes_[i]));
            }
            rows.push_back(filler());
            rows.push_back(text(clipped("↑↓ select · Enter session · i details", width_)) |
                           color(palette.muted));
            rows.push_back(text(clipped("←→ category · Tab history · Esc close", width_)) |
                           color(palette.muted));
        } else {
            const auto detail = get_detail();
            if (!detail) {
                rows.push_back(text("Instance unavailable in this session.") |
                               color(palette.warning));
                rows.push_back(filler());
            } else {
                const auto& entry = detail->entry;
                rows.push_back(
                    text(clipped(std::format("{} · {}", entry.team ? "Team Agent" : "Subagent",
                                             entry.cancelling ? "stopping"
                                                              : runtime::status_name(entry.status)),
                                 width_)) |
                    color(palette.primary));
                std::vector<std::string> lines;
                const auto append = [&](std::string_view label, std::string_view value,
                                        bool transcript = false) {
                    if (value.empty()) return;
                    lines.push_back(std::string(label));
                    auto section =
                        wrapped(bounded_text(value, transcript), std::max(1, width_ - 2));
                    for (auto& line : section)
                        lines.push_back("  " + line);
                };
                append("ID", entry.id);
                append("Name", entry.name);
                append("Type", entry.type);
                append("Team", entry.team_name);
                append("Description", entry.description);
                append("Failure reason", detail->error);
                append("Output", detail->output);
                append("Transcript", detail->transcript, true);
                if (!detail->can_continue) append("Continue unavailable", detail->continue_reason);
                if (!detail->can_stop) append("Stop unavailable", detail->stop_reason);
                const int capacity = std::max(1, height - (editing_ ? 8 : 6));
                max_scroll_ = std::max(0, static_cast<int>(lines.size()) - capacity);
                scroll_ = std::clamp(scroll_, 0, max_scroll_);
                for (int i = scroll_; i < static_cast<int>(lines.size()) && i < scroll_ + capacity;
                     ++i)
                    rows.push_back(text(clipped(lines[static_cast<std::size_t>(i)], width_)) |
                                   color(palette.text));
                rows.push_back(filler());
                if (editing_) {
                    rows.push_back(text("Continuation instruction") | color(palette.muted));
                    rows.push_back(input_->Render() | size(WIDTH, LESS_THAN, width_));
                    rows.push_back(text(clipped("Enter launch · Esc cancel editing", width_)) |
                                   color(palette.muted));
                } else {
                    auto continue_button =
                        text(detail->can_continue ? "[c] Continue" : "Continue unavailable") |
                        color(detail->can_continue ? palette.primary : palette.muted) |
                        reflect(continue_box_);
                    auto stop_button = text(detail->can_stop ? "[s] Stop" : "Stop unavailable") |
                                       color(detail->can_stop ? palette.warning : palette.muted) |
                                       reflect(stop_box_);
                    if (width_ >= 48)
                        rows.push_back(hbox({continue_button, text("  "), stop_button}));
                    else {
                        rows.push_back(continue_button);
                        rows.push_back(stop_button);
                    }
                }
            }
            rows.push_back(text(clipped("↑↓ / PgUp PgDn scroll · Esc back", width_)) |
                           color(palette.muted));
        }
        // The modal owns every cell, including filler and short text rows.
        // Clear before painting the theme background so dbox cannot expose the
        // transcript or editor through otherwise untouched cells.
        return vbox(std::move(rows)) | size(WIDTH, EQUAL, width_) | size(HEIGHT, EQUAL, height) |
               bgcolor(palette.background) | clear_under;
    }

    bool OnEvent(Event event) override {
        if (event == Event::Escape) {
            if (editing_)
                editing_ = false;
            else if (detail_open_) {
                detail_open_ = false;
                scroll_ = 0;
            } else if (options_.on_close)
                options_.on_close();
            return true;
        }
        if (editing_) {
            if (event == Event::Return) {
                const auto detail = get_detail();
                if (detail && detail->can_continue && options_.on_continue &&
                    !continuation_.empty())
                    options_.on_continue(state().selected_id, continuation_);
                return true;
            }
            return input_->OnEvent(event);
        }
        if (detail_open_) {
            if (event == Event::ArrowUp || event == Event::PageUp) {
                scroll_ = std::max(0, scroll_ - (event == Event::PageUp ? 10 : 1));
                return true;
            }
            if (event == Event::ArrowDown || event == Event::PageDown) {
                scroll_ = std::min(max_scroll_, scroll_ + (event == Event::PageDown ? 10 : 1));
                return true;
            }
            if (event == Event::Character('c')) {
                begin_continue();
                return true;
            }
            if (event == Event::Character('s')) {
                stop();
                return true;
            }
            if (event.is_mouse()) {
                const auto& mouse = event.mouse();
                if (mouse.button == Mouse::WheelUp) {
                    scroll_ = std::max(0, scroll_ - 3);
                    return true;
                }
                if (mouse.button == Mouse::WheelDown) {
                    scroll_ = std::min(max_scroll_, scroll_ + 3);
                    return true;
                }
                if (mouse.button == Mouse::Left && mouse.motion == Mouse::Released) {
                    if (continue_box_.Contain(mouse.x, mouse.y)) {
                        begin_continue();
                        return true;
                    }
                    if (stop_box_.Contain(mouse.x, mouse.y)) {
                        stop();
                        return true;
                    }
                }
            }
            return false;
        }
        auto entries = visible_entries();
        synchronize_selection(entries);
        if (event == Event::Tab || event == Event::TabReverse) {
            state().history = !state().history;
            return true;
        }
        if (event == Event::ArrowLeft || event == Event::ArrowRight) {
            state().team = !state().team;
            return true;
        }
        if (event == Event::ArrowUp || event == Event::ArrowDown) {
            if (!entries.empty()) {
                auto index = selected_index(entries);
                if (event == Event::ArrowUp && index > 0) --index;
                if (event == Event::ArrowDown && index + 1 < entries.size()) ++index;
                state().selected_id = entries[index].id;
            }
            return true;
        }
        if (event == Event::Character('i') && !entries.empty()) {
            detail_open_ = true;
            scroll_ = 0;
            return true;
        }
        if (event == Event::Return && !entries.empty()) {
            if (options_.on_open)
                options_.on_open(state().selected_id);
            else {
                detail_open_ = true;
                scroll_ = 0;
            }
            return true;
        }
        if (event.is_mouse() && event.mouse().button == Mouse::Left &&
            event.mouse().motion == Mouse::Released) {
            for (std::size_t i = 0; i < row_boxes_.size(); ++i)
                if (row_boxes_[i].Contain(event.mouse().x, event.mouse().y) &&
                    std::ranges::find(entries, row_ids_[i], &Entry::id) != entries.end()) {
                    state().selected_id = row_ids_[i];
                    if (options_.on_open) {
                        options_.on_open(state().selected_id);
                        return true;
                    }
                    detail_open_ = true;
                    scroll_ = 0;
                    return true;
                }
        }
        return false;
    }

    bool Focusable() const override { return true; }

private:
    BrowserState& state() { return *options_.state; }

    std::optional<Detail> get_detail() {
        return options_.detail ? options_.detail(state().selected_id) : std::nullopt;
    }

    std::vector<Entry> visible_entries() {
        auto entries = options_.list ? options_.list() : std::vector<Entry>{};
        std::erase_if(entries, [&](const Entry& entry) {
            return entry.team != state().team || entry.active == state().history;
        });
        return entries;
    }

    void synchronize_selection(const std::vector<Entry>& entries) {
        if (std::ranges::none_of(
                entries, [&](const auto& entry) { return entry.id == state().selected_id; }))
            state().selected_id = entries.empty() ? std::string{} : entries.front().id;
    }

    std::size_t selected_index(const std::vector<Entry>& entries) {
        const auto it = std::ranges::find(entries, state().selected_id, &Entry::id);
        return static_cast<std::size_t>(std::distance(entries.begin(), it));
    }

    void begin_continue() {
        const auto detail = get_detail();
        if (!detail || !detail->can_continue) return;
        editing_ = true;
        input_->TakeFocus();
    }

    void stop() {
        const auto detail = get_detail();
        if (detail && detail->can_stop && options_.on_stop) options_.on_stop(state().selected_id);
    }

    Options options_;
    Component input_;
    std::string continuation_ =
        "Continue the previous work without unnecessarily repeating completed operations.";
    bool detail_open_ = false;
    bool editing_ = false;
    int width_ = 80;
    int scroll_ = 0;
    int max_scroll_ = 0;
    std::vector<Box> row_boxes_;
    std::vector<std::string> row_ids_;
    Box continue_box_;
    Box stop_box_;
};

}  // namespace

Component CreateBrowser(Options options) {
    return Make<Browser>(std::move(options));
}

}  // namespace loom::ui::agents::instances
