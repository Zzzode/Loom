// app_testing_seams.cpp — plain impl unit for loom.ui.app.app. Owns the
// AppAdapter test-seam bodies (RFC 0002 F3 Finalize): the 28
// *_for_testing accessors that were inline in app.cppm, plus the
// AppTestingSeams proxy bodies and test_seams() factories (P5). Declarations
// stay in app.cppm; the bodies move here so the interface ratchet
// (inline_def_check.py) re-freezes at the single composition body
// (set_screen). Private access holds within member functions regardless of
// TU, and the AppAdapter friend declaration grants AppTestingSeams the same
// access out-of-line.
module;

#include <cstddef>
#include <cstdint>
#include <ftxui/component/component.hpp>

module loom.ui.app.app;

import std;

import loom.types.types;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.screens.messages_store;
import loom.ui.screens.dialog_store;
import loom.ui.screens.chrome_store;

namespace loom::ui {

namespace repl = loom::ui::repl_screen;

bool AppAdapter::is_query_running_for_testing() const noexcept {
    return query_running_.load();
}

void AppAdapter::submit_for_testing(const std::string& text) {
    this->HandleSubmit(text);
}

void AppAdapter::inject_stream_event_for_testing(const core::StreamEvent& ev) {
    this->handle_stream_event(ev);
}

void AppAdapter::set_query_running_for_testing(bool running) {
    query_running_.store(running);
    screen_state_->query_running = running;
    if (running) {
        repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::Requesting);
        screen_state_->task_view_store.spinner_verb = "Thinking";
    }
}

void AppAdapter::clear_streaming_thinking_for_testing() {
    std::lock_guard lk(result_mutex_);
    streaming_thinking_.clear();
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
    return paste_.pasted_contents().size();
}

bool AppAdapter::has_pasted_content_for_testing(int id) const noexcept {
    return paste_.pasted_contents().contains(id);
}

void AppAdapter::inject_pasted_image_for_testing(int id, ImageBlock ib) {
    paste_.pasted_contents()[id] = std::move(ib);
}

void AppAdapter::set_no_real_paste_worker_for_testing(bool v) {
    no_real_paste_worker_for_testing_ = v;
}

void AppAdapter::set_input_text_for_testing(std::string text) {
    screen_state_->input_text = std::move(text);
    screen_state_->input_cursor = screen_state_->input_text.size();
}

void AppAdapter::set_next_action_suggestion_for_testing(std::string suggestion) {
    screen_state_->next_action_suggestion = std::move(suggestion);
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

// ============================================================
// AppTestingSeams proxy bodies (P5) — declarations in app.cppm.
// Each forwards to the matching AppAdapter private *_for_testing method;
// the friend declaration on AppAdapter grants access out-of-line.
// ============================================================

void AppTestingSeams::configure_teammate_for_testing(std::string agent_name, std::string team) {
    app->configure_teammate_for_testing(std::move(agent_name), std::move(team));
}

void AppTestingSeams::poll_teammate_inbox_once_for_testing() {
    app->poll_teammate_inbox_once_for_testing();
}

std::size_t AppTestingSeams::teammate_pending_count_for_testing() {
    return app->teammate_pending_count_for_testing();
}

std::string AppTestingSeams::pop_teammate_prompt_for_testing() {
    return app->pop_teammate_prompt_for_testing();
}

bool AppTestingSeams::is_query_running_for_testing() const noexcept {
    return app->is_query_running_for_testing();
}

void AppTestingSeams::submit_for_testing(const std::string& text) {
    app->submit_for_testing(text);
}

bool AppTestingSeams::is_local_bash_running_for_testing() const noexcept {
    return app->is_local_bash_running_for_testing();
}

void AppTestingSeams::wait_for_local_bash_for_testing() {
    app->wait_for_local_bash_for_testing();
}

bool AppTestingSeams::is_loading_for_testing() const noexcept {
    return app->is_loading_for_testing();
}

void AppTestingSeams::inject_stream_event_for_testing(const loom::core::StreamEvent& ev) {
    app->inject_stream_event_for_testing(ev);
}

void AppTestingSeams::set_query_running_for_testing(bool running) {
    app->set_query_running_for_testing(running);
}

void AppTestingSeams::clear_streaming_thinking_for_testing() {
    app->clear_streaming_thinking_for_testing();
}

std::uint64_t AppTestingSeams::ui_animation_tick_count_for_testing() const noexcept {
    return app->ui_animation_tick_count_for_testing();
}

std::string AppTestingSeams::status_message_for_testing() const {
    return app->status_message_for_testing();
}

bool AppTestingSeams::status_line_enabled_for_testing() const noexcept {
    return app->status_line_enabled_for_testing();
}

std::string AppTestingSeams::status_line_command_for_testing() const {
    return app->status_line_command_for_testing();
}

int AppTestingSeams::status_line_padding_for_testing() const noexcept {
    return app->status_line_padding_for_testing();
}

std::string AppTestingSeams::status_bar_model_for_testing() const {
    return app->status_bar_model_for_testing();
}

std::size_t AppTestingSeams::autocomplete_suggestion_count_for_testing() const noexcept {
    return app->autocomplete_suggestion_count_for_testing();
}

std::vector<std::string> AppTestingSeams::autocomplete_suggestions_for_testing() const {
    return app->autocomplete_suggestions_for_testing();
}

int AppTestingSeams::autocomplete_index_for_testing() const noexcept {
    return app->autocomplete_index_for_testing();
}

std::vector<std::string> AppTestingSeams::messages_for_testing() const {
    return app->messages_for_testing();
}

std::string AppTestingSeams::input_text_for_testing() const {
    return app->input_text_for_testing();
}

void AppTestingSeams::set_disable_chain_compression_for_testing(bool v) {
    app->screen_state_->disable_chain_compression = v;
}

std::size_t AppTestingSeams::pasted_contents_size_for_testing() const noexcept {
    return app->pasted_contents_size_for_testing();
}

bool AppTestingSeams::has_pasted_content_for_testing(int id) const noexcept {
    return app->has_pasted_content_for_testing(id);
}

void AppTestingSeams::inject_pasted_image_for_testing(int id, loom::core::ImageBlock ib) {
    app->inject_pasted_image_for_testing(id, std::move(ib));
}

void AppTestingSeams::set_no_real_paste_worker_for_testing(bool v) {
    app->set_no_real_paste_worker_for_testing(v);
}

void AppTestingSeams::set_input_text_for_testing(std::string text) {
    app->set_input_text_for_testing(std::move(text));
}

void AppTestingSeams::set_next_action_suggestion_for_testing(std::string suggestion) {
    app->set_next_action_suggestion_for_testing(std::move(suggestion));
}

void AppTestingSeams::handle_submit_for_testing(std::string text) {
    app->handle_submit_for_testing(std::move(text));
}

void AppTestingSeams::trigger_orphan_cleanup_for_testing() {
    app->trigger_orphan_cleanup_for_testing();
}

bool AppTestingSeams::is_agents_view_for_testing() const noexcept {
    return app->is_agents_view_for_testing();
}

bool AppTestingSeams::is_local_jsx_command_for_testing(
    std::string_view command_name) const noexcept {
    return app->is_local_jsx_command_for_testing(command_name);
}

int AppTestingSeams::active_agents_selection_position_for_testing() const noexcept {
    return app->active_agents_selection_position_for_testing();
}

std::size_t AppTestingSeams::agent_card_count_for_testing() const noexcept {
    return app->agent_card_count_for_testing();
}

bool AppTestingSeams::has_pending_dialog_for_testing() const noexcept {
    return app->has_pending_dialog_for_testing();
}

void AppTestingSeams::set_live_teammates_for_testing(void* v) {
    app->set_live_teammates_for_testing(v);
}

bool AppTestingSeams::teams_overview_open_for_testing() const {
    return app->teams_overview_open_for_testing();
}

int AppTestingSeams::teams_overview_count_for_testing() const noexcept {
    return app->teams_overview_count_for_testing();
}

void AppTestingSeams::enqueue_teammate_permission_for_testing(void* request, std::string team) {
    app->enqueue_teammate_permission_for_testing(request, std::move(team));
}

std::size_t AppTestingSeams::pending_teammate_permission_count_for_testing() {
    return app->pending_teammate_permission_count_for_testing();
}

AppTestingSeams test_seams(const ftxui::Component& app) {
    return AppTestingSeams{dynamic_cast<AppAdapter*>(app.get())};
}

AppTestingSeams test_seams(AppAdapter* app) {
    return AppTestingSeams{app};
}

} // namespace loom::ui
