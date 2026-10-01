// app_testing_seams.cpp — plain impl unit for cc.ui.app.app. Owns the
// AppAdapter test-seam bodies (RFC 0002 F3 Finalize): the 28
// *_for_testing accessors that were inline in app.cppm. Declarations stay
// in app.cppm; the bodies move here so the interface ratchet
// (inline_def_check.py) re-freezes at the single composition body
// (set_screen). Private access holds within member functions regardless of
// TU, so no friend or access change is needed.
//
// LLVM #184957: like app_extra_methods.cpp / app_handle_submit.cpp /
// app_prompt_suggestion_wiring.cpp / app_team.cpp / app_run.cpp, this unit
// must NOT `import std;` — under the reduced-BMI writer a cold module cache
// mis-merges the global aligned operator new when an app impl unit imports
// std while the primary's GMF pulls libc++ textually via FTXUI. Keep textual
// std headers in the global module fragment (see CMakeLists.txt:283-292).
module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

module loom.ui.app.app;

import loom.types.types;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.screens.messages_store;
import loom.ui.screens.dialog_store;
import loom.ui.screens.chrome_store;

namespace cc::ui {

namespace repl = cc::ui::repl_screen;

bool AppAdapter::is_query_running_for_testing() const noexcept {
    return query_running_.load();
}

void AppAdapter::submit_for_testing(const std::string& text) {
    this->HandleSubmit(text);
}

bool AppAdapter::is_local_bash_running_for_testing() const noexcept {
    return bash_running_.load();
}

void AppAdapter::wait_for_local_bash_for_testing() {
    if (bash_thread_.joinable()) bash_thread_.join();
    this->ConsumePendingResult();
}

bool AppAdapter::is_loading_for_testing() const noexcept {
    return screen_state_->task_view_store.spinner_mode != repl::SpinnerMode::Hidden;
}

std::uint64_t AppAdapter::ui_animation_tick_count_for_testing() const noexcept {
    return ui_animation_tick_count_.load(std::memory_order_relaxed);
}

std::string AppAdapter::status_message_for_testing() const {
    return screen_state_->task_view_store.spinner_tip.value_or(std::string{});
}

bool AppAdapter::status_line_enabled_for_testing() const noexcept {
    return screen_state_->status_line_enabled;
}

std::string AppAdapter::status_line_command_for_testing() const {
    return screen_state_->status_line_command;
}

int AppAdapter::status_line_padding_for_testing() const noexcept {
    return screen_state_->status_line_padding;
}

std::string AppAdapter::status_bar_model_for_testing() const {
    return screen_state_->chrome_store.status_bar.model_name;
}

std::size_t AppAdapter::autocomplete_suggestion_count_for_testing() const noexcept {
    return screen_state_->autocomplete_suggestions.size();
}

std::vector<std::string> AppAdapter::autocomplete_suggestions_for_testing() const {
    std::vector<std::string> out;
    out.reserve(screen_state_->autocomplete_suggestions.size());
    for (const auto& suggestion : screen_state_->autocomplete_suggestions) {
        out.push_back(suggestion.display_text);
    }
    return out;
}

int AppAdapter::autocomplete_index_for_testing() const noexcept {
    return screen_state_->autocomplete_index;
}

std::vector<std::string> AppAdapter::messages_for_testing() const {
    std::vector<std::string> out;
    out.reserve(screen_state_->messages_store.messages.size());
    for (const auto& m : screen_state_->messages_store.messages) {
        std::string label = m.role;
        if (m.is_local_command_input) label = "lc-input";
        else if (m.is_local_command_output) label = "lc-output";
        else if (m.is_thinking) label = "thinking";
        std::string pv = m.content_preview.substr(
            0, std::min<std::size_t>(30, m.content_preview.size()));
        out.push_back(label + ":" + pv);
    }
    return out;
}

std::string AppAdapter::input_text_for_testing() const {
    return screen_state_->input_text;
}

std::size_t AppAdapter::pasted_contents_size_for_testing() const noexcept {
    return pasted_contents_.size();
}

bool AppAdapter::has_pasted_content_for_testing(int id) const noexcept {
    return pasted_contents_.contains(id);
}

void AppAdapter::inject_pasted_image_for_testing(int id, ImageBlock ib) {
    pasted_contents_[id] = std::move(ib);
}

void AppAdapter::set_no_real_paste_worker_for_testing(bool v) {
    no_real_paste_worker_for_testing_ = v;
}

void AppAdapter::set_input_text_for_testing(std::string text) {
    screen_state_->input_text = std::move(text);
    screen_state_->input_cursor = screen_state_->input_text.size();
}

void AppAdapter::handle_submit_for_testing(std::string text) {
    this->HandleSubmit(text);
}

bool AppAdapter::is_agents_view_for_testing() const noexcept {
    return screen_state_->mode == repl::ReplMode::AgentsView;
}

bool AppAdapter::is_local_jsx_command_for_testing(
    std::string_view command_name) const noexcept {
    return screen_state_->active_local_jsx_command &&
           screen_state_->active_local_jsx_command_name == command_name;
}

int AppAdapter::active_agents_selection_position_for_testing() const noexcept {
    return screen_state_->active_agents_selection_position;
}

std::size_t AppAdapter::agent_card_count_for_testing() const noexcept {
    return screen_state_->task_view_store.agent_cards.size();
}

bool AppAdapter::has_pending_dialog_for_testing() const noexcept {
    return screen_state_->dialog_store.dialog_queue.has_overlay() ||
           screen_state_->dialog_store.dialog_queue.has_any_bottom() ||
           screen_state_->dialog_store.dialog_queue.has_modal() ||
           screen_state_->dialog_store.dialog_queue.has_standalone();
}

int AppAdapter::teams_overview_count_for_testing() const noexcept {
    return static_cast<int>(screen_state_->task_view_store.live_teammates.size());
}

} // namespace cc::ui
