// OSC 7501 wiring. State callbacks may run while Render drains UI actions;
// only startup, event completion and terminal shutdown deliver bytes.
module;

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>

module loom.ui.app.app;

import std;
import :impl;
import loom.ui.app.program_status;
import loom.ui.screens.repl_state; // arch-check: keep-import (complete ReplScreenState for screen_state_ access)
import loom.ui.dialogs.system;
import loom.terminal.osc;
import loom.terminal.output;

namespace loom::ui {
namespace {
using Clock = std::chrono::steady_clock;
namespace osc = loom::terminal::osc;
}  // namespace

void AppAdapter::StartProgramStatusWithWriter(bool interactive,
                                              std::function<bool(std::string_view)> writer) {
    if (impl_->program_status) return;
    impl_->program_status = std::make_unique<ProgramStatusReporter>(std::move(writer));
    UpdateProgramStatusSnapshot();
    impl_->program_status->Start(interactive, Clock::now());
}

void AppAdapter::StartProgramStatus(bool interactive) noexcept {
    try {
        StartProgramStatusWithWriter(interactive, [](std::string_view bytes) {
            return loom::terminal::output::write_control_sequence(bytes).has_value();
        });
    } catch (...) {
        // Optional terminal integration must not change the operation outcome.
    }
}

std::uint64_t AppAdapter::BeginProgramStatusOperation() {
    const auto generation = ++impl_->program_status_generation;
    if (impl_->program_status) impl_->program_status->Begin();
    return generation;
}

void AppAdapter::CancelProgramStatusOperation() {
    if (impl_->program_status) impl_->program_status->Cancel();
}

void AppAdapter::CompleteProgramStatusOperation(std::uint64_t generation, bool error, bool paused) {
    if (generation != impl_->program_status_generation) return;
    if (impl_->program_status) impl_->program_status->Complete(error, paused);
}

bool AppAdapter::HandleProgramStatusControl(const Event& event) {
    return impl_->program_status &&
           impl_->program_status->HandleControl(event.input(), Clock::now());
}

std::pair<bool, bool> AppAdapter::ProgramStatusBlocks() {
    using Type = loom::ui::dialogs::system::DialogType;
    const auto& queue = screen_state_->dialog_store.dialog_queue;
    bool permission = queue.contains_type(Type::ToolPermission) ||
                      queue.contains_type(Type::SandboxPermission) ||
                      queue.contains_type(Type::WorkerSandboxPermission) ||
                      queue.contains_type(Type::Elicitation);
    {
        std::lock_guard lock(teammate_.permission_mutex());
        permission = permission || !teammate_.pending_permissions().empty();
    }
    return {permission, queue.contains_type(Type::PromptDialog)};
}

void AppAdapter::PrepareProgramStatusInput(Event& event) {
    if (!impl_->program_status) return;
    const auto [permission, question] = ProgramStatusBlocks();
    // A decision answers the actual request, not the retained foreground result.
    if (permission || question) return;
    if (event == Event::Custom || event == Event::Character('\x03') ||
        event == Event::Character('\x04'))
        return;
    if (event == Event::Return &&
        (screen_state_->input_text == "/exit" || screen_state_->input_text == "/quit"))
        return;

    const bool key =
        event.is_character() || event == Event::Return || event == Event::Escape ||
        event == Event::Backspace || event == Event::Delete || event == Event::Tab ||
        event == Event::TabReverse || event == Event::ArrowLeft || event == Event::ArrowRight ||
        event == Event::ArrowUp || event == Event::ArrowDown || event == Event::Home ||
        event == Event::End || event == Event::PageUp || event == Event::PageDown ||
        event == Event::F1 || event == Event::F2 || event == Event::F3 || event == Event::F4 ||
        event == Event::F5 || event == Event::F6 || event == Event::F7 || event == Event::F8 ||
        event == Event::F9 || event == Event::F10 || event == Event::F11 || event == Event::F12;
    bool mouse_motion = false;
    if (event.is_mouse()) {
        auto raw = std::string_view(event.input());
        if (raw.starts_with("\x1b[")) {
            raw.remove_prefix(2);
            if (raw.starts_with('<')) raw.remove_prefix(1);
            unsigned int buttons = 0;
            const auto parsed = std::from_chars(raw.data(), raw.data() + raw.size(), buttons);
            mouse_motion = parsed.ec == std::errc{} && (buttons & 32U) != 0;
        }
    }
    // FTXUI also represents button-held motion as Pressed; inspect the wire bit.
    const bool click = event.is_mouse() && !mouse_motion &&
                       event.mouse().motion == Mouse::Pressed &&
                       event.mouse().button != Mouse::None;
    if (key || click) impl_->program_status->Acknowledge();
}

void AppAdapter::UpdateProgramStatusSnapshot() {
    if (!impl_->program_status) return;
    const auto [permission, question] = ProgramStatusBlocks();
    impl_->program_status->Refresh(query_running_.load() || bash_running_.load(),
                                   permission ? osc::ProgramStatusKind::Permission
                                   : question ? osc::ProgramStatusKind::Question
                                              : osc::ProgramStatusKind::None);
}

void AppAdapter::RefreshProgramStatus() noexcept {
    try {
        UpdateProgramStatusSnapshot();
        if (impl_->program_status) impl_->program_status->Flush(Clock::now());
    } catch (...) {
        if (impl_->program_status) impl_->program_status->Finish(false, Clock::now());
    }
}

void AppAdapter::FinishProgramStatus(bool normal) noexcept {
    try {
        if (!impl_->program_status) return;
        // Flush no queued work here: finish must not start another operation.
        UpdateProgramStatusSnapshot();
        impl_->program_status->Finish(normal, Clock::now());
    } catch (...) {
        if (impl_->program_status) impl_->program_status->Finish(false, Clock::now());
    }
}

void AppTestingSeams::start_program_status_for_testing(
    std::function<bool(std::string_view)> writer) {
    app->StartProgramStatusWithWriter(true, std::move(writer));
}

void AppTestingSeams::enqueue_program_status_completion_for_testing(bool error, bool paused) {
    const auto generation = app->impl_->program_status_generation;
    app->ui_dispatcher()([target = app, generation, error, paused] {
        target->CompleteProgramStatusOperation(generation, error, paused);
    });
}

}  // namespace loom::ui
