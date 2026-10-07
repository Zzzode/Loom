// app_extra_methods.cpp — impl unit for AppAdapter methods and free functions
// kept OUT of app.cppm AND out of app_autocomplete.cpp to stay under clang's
// 2GB source-location budget.
//
// Contains: project_agent_definition_card (free function),
//           RunLocalBashCommand, ProjectRuntimeMetadataToScreenState,
//           ApplyMessageCollapsePipeline, SpawnPasteWorker,
//           ProcessCompletedPastes.
//
// These were originally moved from app.cppm to app_autocomplete.cpp, but the
// combined import closure of app_autocomplete.cpp + these methods exceeded
// the source-location budget (fatal error: "ran out of source locations").
//
// Phase A (import std): this impl unit deliberately keeps std TEXTUAL and
// does NOT `import std;`. clang 22.1.8's reduced-BMI writer (LLVM #184957,
// fixed in clang 23 / PR #179178) leaks a duplicate aligned operator new into
// impl units of a primary whose GMF pulls libc++ textually (app.cppm via FTXUI),
// causing "operator new is ambiguous". Re-evaluate after the toolchain upgrade.
module;

#include <cstdio>
#include <cstdlib>
#include <ctime>

module loom.ui.app.app;

import std;

import loom.query.query_engine;

// ── Imports needed by the 5 methods (not available via the interface) ────
import loom.process.bash.bash_execution;
import loom.model.model;
import loom.constants.constants;
import loom.scm.git.git;
import loom.text.parse_references;
import loom.ui.messages.collapse_background_bash;
import loom.ui.features.agents.agent_shared_widgets;
import loom.ui.features.todos.todo_panel;
import loom.tools.agent_display;
import loom.tools.todo_write;

// ── Imports available via the interface but needed for namespace aliases ─
import loom.tools.agent_runtime;
import loom.ui.features.agents.agent_cards;

namespace loom::ui {

namespace agent_runtime = loom::tools::agent_runtime;
namespace agent_cards = loom::ui::agents::cards;
namespace agent_shared = loom::ui::agents::shared;
namespace agent_display = loom::tools::agent_display;

// ── project_agent_definition_card (moved out of app.cppm) ───────────────
// Agent defs with color + truncated when_to_use.
agent_cards::AgentCardData project_agent_definition_card(
    const agent_runtime::AgentDefinition& agent) {
    agent_cards::AgentCardData card;
    card.id = agent.agent_type;
    card.name = agent.agent_type;
    card.agent_type = agent.agent_type;
    card.source = agent.source;
    card.description = summarize_agent_description(agent.when_to_use);
    card.description_long = agent.when_to_use;
    card.role_tags.push_back(std::string(agent_display::source_display_name(agent.source)));
    card.tools = agent.tools;
    card.status = agent_shared::AgentStatus::Idle;
    if (!agent.model.empty()) {
        card.model_override = agent.model;
    }
    card.permission_mode = agent.permission_mode;
    card.is_subagent = true;
    return card;
}

// ── RunLocalBashCommand (moved out of app.cppm) ─────────────────────────
// Run a user-initiated `!` command LOCALLY (never an LLM turn).  It runs
// the command with the sandbox disabled, renders a <bash-input> user row
// plus a <bash-stdout>/<bash-stderr> output row, and NEVER sends the
// command to the model.
//
// We mirror that: append the input row immediately (like the initial
// setToolJSX(<BashModeProgress>)), then run `/bin/sh -c` on a worker thread
// (combined stdout+stderr via popen_spawn, run in the session cwd) and post
// the output back to the render thread via pending_bash_result_.  The
// engine / query path is never touched, so no Bash *tool-use* card and no
// assistant summary are produced — matching the transcript exactly.
void AppAdapter::RunLocalBashCommand(std::string command) {
    command = trim_ascii_copy(command);
    if (command.empty()) return;

    // Show the command row immediately (render thread — HandleSubmit runs
    // here).  Mirrors createUserMessage(`<bash-input>…`).
    AppendLocalCommandInputMessage(command);
    this->SyncState();
    PostRenderEvent();

    // If a previous bash command is still running, join it first so we
    // don't overlap workers (local `!` commands are strictly sequential in
    // too — the prompt is blocked on the single BashTool.call).
    if (bash_thread_.joinable()) bash_thread_.join();

    bash_running_.store(true);
    const std::string cwd = static_cast<loom::core::QueryEngine*>(engine_raw()) ? static_cast<loom::core::QueryEngine*>(engine_raw())->working_directory() : std::string{};
    bash_thread_ = std::jthread(
        [this, command, cwd](std::stop_token st) {
            // POSIX single-quote helper for paths with spaces/quotes.
            auto sq = [](std::string_view s) {
                std::string out = "'";
                for (char c : s) {
                    if (c == '\'') out += "'\\''";
                    else out += c;
                }
                out += "'";
                return out;
            };

            // Write final cwd to a tempfile (not stdout) so command output
            // can't collide with the cwd data.  Use && so pwd -P only runs
            // on success (failed commands shouldn't change cwd).
            const auto cwd_file = std::filesystem::temp_directory_path() /
                ("loom-cwd-" + current_session_id_);
            const std::string cwd_file_str = cwd_file.string();

            std::string full;
            if (!cwd.empty()) {
                full = "cd " + sq(cwd) + " && " + command +
                       " 2>&1 && pwd -P > " + sq(cwd_file_str);
            } else {
                full = command + " 2>&1 && pwd -P > " + sq(cwd_file_str);
            }

            std::string output;
            bool is_error = false;
            if (FILE* pipe = loom::utils::bash::popen_spawn(full)) {
                std::array<char, 4096> buf{};
                while (!st.stop_requested() &&
                       std::fgets(buf.data(), static_cast<int>(buf.size()), pipe)
                               != nullptr) {
                    output += buf.data();
                }
                const int status = loom::utils::bash::pclose_spawn(pipe);
                is_error = status != 0;
            } else {
                output = "Command failed: could not spawn /bin/sh";
                is_error = true;
            }

            // Trim trailing newlines from the visible output.
            while (!output.empty() &&
                   (output.back() == '\n' || output.back() == '\r')) {
                output.pop_back();
            }

            // Read the cwd file (only exists if command succeeded).
            std::string new_cwd;
            {
                std::ifstream ifs(cwd_file_str);
                if (ifs.is_open()) {
                    std::getline(ifs, new_cwd);
                    // Trim trailing whitespace
                    while (!new_cwd.empty() &&
                           (new_cwd.back() == '\n' || new_cwd.back() == '\r' ||
                            new_cwd.back() == ' '))
                        new_cwd.pop_back();
                }
                // Clean up tempfile
                std::error_code ec;
                std::filesystem::remove(cwd_file, ec);
            }

            // Update process cwd if the command changed it.
            if (!new_cwd.empty() && new_cwd != cwd) {
                std::error_code ec;
                std::filesystem::current_path(new_cwd, ec);
                if (!ec) {
                    screen_state_->cwd = new_cwd;
                    if (static_cast<loom::core::QueryEngine*>(engine_raw())) static_cast<loom::core::QueryEngine*>(engine_raw())->set_working_directory(new_cwd);
                }
            }
            {
                std::lock_guard lk(bash_result_mutex_);
                pending_bash_result_ = PendingBashResult{
                    .output = std::move(output), .is_error = is_error};
            }
            bash_running_.store(false);
            PostRenderEvent();
        });
}

// ── ProjectRuntimeMetadataToScreenState (moved out of app.cppm) ─────────
void AppAdapter::ProjectRuntimeMetadataToScreenState() {
    const auto& model_id = static_cast<loom::core::QueryEngine*>(engine_raw())->model_params().model;
    screen_state_->chrome_store.status_bar.model_name = model_id;
    screen_state_->chrome_store.model_display_name =
        loom::utils::get_model_display_name(model_id);

    auto usage = static_cast<loom::core::QueryEngine*>(engine_raw())->get_usage();
    screen_state_->chrome_store.status_bar.input_tokens =
        static_cast<int>(usage.input_tokens);
    screen_state_->chrome_store.status_bar.output_tokens =
        static_cast<int>(usage.output_tokens);
    screen_state_->chrome_store.status_bar.cost_usd =
        static_cast<loom::core::QueryEngine*>(engine_raw())->budget_tracker().current_spend_usd;
    screen_state_->chrome_store.status_bar.context_token_count =
        static_cast<int>(usage.input_tokens + usage.output_tokens);

    screen_state_->cwd = static_cast<loom::core::QueryEngine*>(engine_raw())->working_directory();
    screen_state_->chrome_store.status_bar.current_path = screen_state_->cwd;

    // P0-6 builtin statusline: detect git branch for the current cwd.
    // Cached per-cwd-change to avoid spawning `git` on every render tick
    // (ProjectRuntimeMetadataToScreenState is called from SyncState on
    // every render event).
    if (screen_state_->cwd != last_branch_cwd_) {
        last_branch_cwd_ = screen_state_->cwd;
        cached_git_branch_ = loom::utils::git::get_branch(
            last_branch_cwd_.empty() ? "." : last_branch_cwd_);
    }
    screen_state_->chrome_store.git_branch = cached_git_branch_;
}

// ── ProjectTodosToScreenState ───────────────────────────────────────────
// Project the AI-managed todo list (TodoWriteTool singleton) into the
// screen-state TodoStore. The tool owns its own mutex; get_sorted_items()
// returns a pre-sorted copy (in_progress > pending > completed, then by
// priority), so the store needs no re-sort. Max 10 items — the per-call copy
// is trivial. Called from SyncState and Render, matching the
// ProjectRuntimeMetadataToScreenState wiring.
void AppAdapter::ProjectTodosToScreenState() {
    namespace tw = loom::tools;
    using Status = todos::TodoDisplayItem::Status;
    using Priority = todos::TodoDisplayItem::Priority;

    auto map_status = [](tw::TodoStatus s) -> Status {
        switch (s) {
            case tw::TodoStatus::InProgress: return Status::InProgress;
            case tw::TodoStatus::Completed:  return Status::Completed;
            case tw::TodoStatus::Pending:    return Status::Pending;
        }
        return Status::Pending;
    };
    auto map_priority = [](tw::Priority p) -> Priority {
        switch (p) {
            case tw::Priority::High:   return Priority::High;
            case tw::Priority::Low:    return Priority::Low;
            case tw::Priority::Medium: return Priority::Medium;
        }
        return Priority::Medium;
    };

    auto items = tw::todo_write_store().get_sorted_items();
    auto& out = screen_state_->todo_store.items;
    out.clear();
    out.reserve(items.size());
    for (const auto& it : items) {
        out.push_back(todos::TodoDisplayItem{
            .content = it.content,
            .status = map_status(it.status),
            .priority = map_priority(it.priority),
        });
    }
}

// ── ApplyMessageCollapsePipeline (moved out of app.cppm) ────────────────
// Apply a chain of collapse passes to the message list before projecting
// rows:
//   collapseBackgroundBashNotifications(collapseHookSummaries(
//     collapseTeammateShutdowns(collapseReadSearchGroups(grouped, tools))))
//
// We run the same chain here, on the raw conversation, before the
// per-message projection loop in SyncState()/Render().  Only the passes
// that have a faithful port are wired so far:
//   * collapseBackgroundBashNotifications — DONE (this call).
//   * collapseHookSummaries / collapseTeammateShutdowns / collapseReadSearch
//     — pending (need richer SystemMessage / AttachmentMessage types).
// As each pass lands it slots in here, preserving the ordering.
//
// `fullscreen=true`: the CPP transcript is always the fullscreen-equivalent
// view (collapse gated on isFullscreenEnvEnabled()).  `verbose=false`:
// there is no ctrl+O verbose transcript toggle at this layer yet, so we use
// the default collapsed presentation (each item shown only in verbose).
std::vector<Message> AppAdapter::ApplyMessageCollapsePipeline(
    std::vector<Message> messages) const {
    namespace collapse = loom::ui::messages::collapse;
    messages = collapse::collapse_background_bash_notifications(
        messages, /*fullscreen=*/true, /*verbose=*/false);
    return messages;
}

// ── SpawnPasteWorker (moved out of app.cppm) ────────────────────────────
// Spawns a detached thread that reads the clipboard image.  On success
// the ImageBlock is posted to pending_paste_results_; on failure the id
// is posted to pending_paste_failures_.  ProcessCompletedPastes() drains
// both queues on the render/event thread.
//
// Why async?  std::system() + osascript fork + PNG-to-file + base64
// encode takes 100-500ms on macOS.  Doing that synchronously in OnEvent
// blocks the FTXUI render loop, causing visible UI freeze and (worse)
// terminal raw-mode state corruption that can take seconds to recover
// from.  The placeholder "[Image #N]" is inserted synchronously so the
// user gets instant feedback; the image data fills in shortly after.
void AppAdapter::SpawnPasteWorker(int id) {
    // P4-1b: delegates to PasteCoordinator.  The coordinator owns the
    // in-flight set, the pending-result maps, and the background thread.
    // PostRenderEvent is injected as the wake callback.
    paste_.SpawnPasteWorker(id, [this] { this->PostRenderEvent(); },
                            no_real_paste_worker_for_testing_);
}

// ── ProcessCompletedPastes (moved out of app.cppm) ──────────────────────
/// Drain background paste results onto pasted_contents_ (render thread).
/// Called at the top of every OnEvent so results are picked up as soon as
/// possible without blocking.  Failed pastes have their "[Image #N]"
/// placeholder removed from input_text.  Text pastes replace "[Image #N]"
/// with the actual text (truncating if >10K chars).
void AppAdapter::ProcessCompletedPastes() {
    std::unordered_map<int, ImageBlock> results;
    std::unordered_set<int> failures;
    std::unordered_map<int, std::string> text_results;
    paste_.drain_pending(results, failures, text_results);
    // Successful image pastes: store in pasted_contents_.
    for (auto& [id, ib] : results) {
        paste_.pasted_contents()[id] = std::move(ib);
        paste_.mark_completed(id);  // main thread
        // Surface the footer "Pasting text…" hint for the async path too.
        screen_state_->pasting_since = std::chrono::steady_clock::now();
    }
    // Text pastes (clipboard had text, not an image): replace the
    // "[Image #N]" placeholder with the actual text.  If >10K chars,
    // apply truncation (head + [...Truncated text #N] + tail) and store
    // the middle content in pasted_text_contents_ for submit-time
    // expansion.
    for (auto& [id, raw_text] : text_results) {
        const std::string placeholder = loom::utils::format_image_ref(id);
        auto& input = screen_state_->input_text;
        auto pos = input.find(placeholder);
        if (pos == std::string::npos) {
            paste_.mark_completed(id);
            continue;
        }

        // Determine replacement text and optional truncated middle content.
        std::string replacement;
        std::string placeholder_content;
        constexpr std::size_t kTruncationThreshold = 10000;
        if (raw_text.size() > kTruncationThreshold) {
            const auto trunc_result = loom::utils::maybe_truncate_paste(raw_text, id);
            replacement = trunc_result.truncated_text;
            placeholder_content = trunc_result.placeholder_content;
        } else {
            replacement = std::move(raw_text);
        }

        // Replace "[Image #N]" with the text (possibly truncated).
        // Also eat a leading space if present.
        std::size_t replace_start = pos;
        std::size_t replace_len = placeholder.size();
        if (pos > 0 && input[pos - 1] == ' ') {
            replace_start = pos - 1;
            replace_len += 1;
        }
        input.replace(replace_start, replace_len, replacement);

        // Adjust cursor if it was past the replaced region.
        auto cursor = screen_state_->input_cursor;
        if (cursor != std::string::npos && cursor > replace_start) {
            const std::size_t old_end = replace_start + replace_len;
            const std::size_t new_end = replace_start + replacement.size();
            if (cursor >= old_end) {
                cursor = cursor - old_end + new_end;
            } else {
                cursor = replace_start + replacement.size();
            }
            screen_state_->input_cursor = cursor;
        }

        // Store truncated middle content for later expansion.
        if (!placeholder_content.empty()) {
            paste_.pasted_text_contents()[id] = std::move(placeholder_content);
        }

        paste_.mark_completed(id);
    }
    // Failed pastes: remove the "[Image #N]" placeholder from input_text
    // so the user doesn't submit a dangling ref.  We search for the exact
    // placeholder string and erase it (plus any leading space that was
    // added by the insert logic).
    if (!failures.empty()) {
        for (int id : failures) {
            const std::string placeholder = loom::utils::format_image_ref(id);
            auto& input = screen_state_->input_text;
            auto pos = input.find(placeholder);
            if (pos != std::string::npos) {
                // Also eat a leading space if present (so we don't leave
                // a double-space where the placeholder was).
                std::size_t erase_start = pos;
                std::size_t erase_len = placeholder.size();
                if (pos > 0 && input[pos - 1] == ' ') {
                    erase_start = pos - 1;
                    erase_len += 1;
                }
                input.erase(erase_start, erase_len);
                // Adjust cursor if it was past the erased region.
                auto cursor = screen_state_->input_cursor;
                if (cursor != std::string::npos && cursor > erase_start) {
                    cursor = (cursor >= erase_start + erase_len)
                        ? cursor - erase_len
                        : erase_start;
                    screen_state_->input_cursor = cursor;
                }
            }
            paste_.pasted_contents().erase(id);  // just in case
            paste_.mark_completed(id);  // main thread
        }
    }
}

} // namespace loom::ui
