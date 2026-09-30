// app_local_command.cpp — plain impl unit for cc.ui.app.app. Owns the
// local-command / local-JSX row bodies (RFC 0001 Phase C batch 2):
//   AppAdapter::AppendLocalMessagesToScreenState
//   AppAdapter::AppendLocalCommandInputMessage
//   AppAdapter::AppendLocalCommandMessage
//   AppAdapter::ClearActiveLocalJsxCommand
//   AppAdapter::DismissLocalJsxCommand
//
// Declarations stay in app.cppm. AppendLocalCommandMessage's default
// argument (is_error = false) stays on the in-class declaration; this
// out-of-line definition omits it — a default argument cannot be redefined
// by a later declaration in the same scope. The function-local static
// s_local_seq moves with AppendLocalMessagesToScreenState and stays
// function-local (one strong definition in exactly one TU).
//
// LLVM #184957: like app_extra_methods.cpp / app_handle_submit.cpp /
// app_prompt_suggestion_wiring.cpp / app_team.cpp / app_run.cpp, this unit
// must NOT `import std;` — under the reduced-BMI writer a cold module cache
// mis-merges the global aligned operator new when an app impl unit imports
// std while the primary's GMF pulls libc++ textually via FTXUI. Keep textual
// std headers in the global module fragment (see CMakeLists.txt:283-292).
module;

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

module cc.ui.app.app;

import cc.ui.screens.repl_state;
import cc.ui.screens.messages_store;

namespace cc::ui {

namespace repl = cc::ui::repl_screen;

void AppAdapter::AppendLocalMessagesToScreenState() {
    // Ensure local-command entries have a synthetic 24-char uuids so the
    // UnseenDivider anchor match still lands consistently.  Each local
    // command row is self-contained (not part of any source Message) so
    // each gets its own unique prefix.  A monotonically counter ensures
    // no collisions.
    static std::uint64_t s_local_seq = 0;
    for (auto it = local_command_messages_.begin();
         it != local_command_messages_.end(); ++it) {
        if (it->id.empty()) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "loc_%016llx",
                          (unsigned long long)s_local_seq++);
            it->id = std::string(buf, 24);
        }
    }
    screen_state_->messages_store.messages.insert(
        screen_state_->messages_store.messages.end(),
        local_command_messages_.begin(),
        local_command_messages_.end());
}

void AppAdapter::AppendLocalCommandInputMessage(std::string command) {
    if (command.empty()) return;
    repl::MessageDisplayEntry entry;
    entry.role = "user";
    entry.content_preview = std::move(command);
    entry.is_local_command_input = true;
    entry.timestamp = std::chrono::system_clock::now();
    local_command_messages_.push_back(std::move(entry));
}

void AppAdapter::AppendLocalCommandMessage(std::string message, bool is_error) {
    if (message.empty()) return;
    repl::MessageDisplayEntry entry;
    entry.role = "system";
    entry.content_preview = std::move(message);
    entry.is_local_command_output = true;
    entry.is_error = is_error;
    entry.timestamp = std::chrono::system_clock::now();
    local_command_messages_.push_back(std::move(entry));
    this->SyncState();
    PostRenderEvent();
}

void AppAdapter::ClearActiveLocalJsxCommand() {
    screen_state_->active_local_jsx_command = false;
    screen_state_->active_local_jsx_command_name.clear();
    screen_state_->active_local_jsx_command_args.clear();
    screen_state_->active_local_jsx_content.clear();
    screen_state_->active_agents_selection_position = 0;
}

void AppAdapter::DismissLocalJsxCommand(std::string result_message) {
    if (!screen_state_->active_local_jsx_command) return;
    std::string command = "/" + screen_state_->active_local_jsx_command_name;
    if (!screen_state_->active_local_jsx_command_args.empty()) {
        command += " " + screen_state_->active_local_jsx_command_args;
    }

    ClearActiveLocalJsxCommand();
    AppendLocalCommandInputMessage(std::move(command));
    AppendLocalCommandMessage(std::move(result_message), false);
}

} // namespace cc::ui
