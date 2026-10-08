// app_handle_submit.cpp — impl unit for AppAdapter::HandleSubmit and
// AppAdapter::HandleCommand, kept OUT of app_autocomplete.cpp to stay
// under clang's 2GB source-location budget.
//
// Contains: HandleSubmit (streaming query + @-mention materialization),
//           HandleCommand (slash command dispatch).
//
// Splitting these out removes 4 heavy imports (figures, at_attachments,
// message_pipeline, types) from app_autocomplete.cpp.
//
// LLVM 23 (PR #179178) fixed the reduced-BMI operator-new ambiguity
// (LLVM #184957), so this unit now `import std;` like the rest of the tree.
module;

#include <cstdio>
#include <cstdlib>

module loom.ui.app.app;

import std;

import loom.query.query_engine;
import loom.commands.registry;
import loom.commands.command;
import loom.hooks.lifecycle_hooks;

// ── Base imports (shared with app_autocomplete.cpp) ─────────────────────
import loom.commands.registry;
import loom.tools.agent_runtime;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.prompt.file_index;
import loom.ui.prompt.fuzzy_rank_nucleo;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.text.parse_references;
import loom.skills.support;

// ── HandleSubmit-only imports (moved out of app_autocomplete.cpp) ───────
import loom.ui.foundation.clock;
import loom.ui.foundation.design_figures;
import loom.ui.prompt.at_attachments;
import loom.ui.messages.message_pipeline;
import loom.types.types;
import loom.commands.command;
import loom.ui.dialogs.triggers;
import loom.ui.dialogs.system;       // SessionPickerEntry (dsys alias)
import loom.vim.vim_mode;
import loom.session.storage;
import loom.query.assembly;
import loom.serdes.json;

namespace loom::ui {
// Defined in app_store_bridge.cpp (impl unit of this module).
CommandContext command_context_for_engine(loom::core::QueryEngine* engine,
                                          void* app_store, std::string cwd);

namespace repl = loom::ui::repl_screen;
namespace agent_runtime = loom::tools::agent_runtime;
namespace acsrc = loom::ui::autocomplete_sources;
namespace frn = loom::ui::prompt::fuzzy_rank_nucleo;
namespace fidx = loom::ui::prompt::file_index;
namespace atatt = loom::ui::prompt::at_attachments;
namespace figs = loom::ui::design::figures;
namespace pl = loom::ui::messages::pipeline;
namespace dtrig = loom::ui::dialogs::triggers;
namespace dsys = loom::ui::dialogs::system;

// ── Session picker helpers ──────────────────────────────────────────────

/// Format a time point as a human-readable age string ("2h ago").
static std::string format_session_age(std::chrono::system_clock::time_point tp) {
    auto elapsed = std::chrono::system_clock::now() - tp;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed).count();
    if (hours < 1) return "just now";
    if (hours < 24) return std::format("{}h ago", hours);
    return std::format("{}d ago", hours / 24);
}

/// Count messages in a session by counting non-empty lines in its
/// messages.jsonl file.  Returns 0 if the file does not exist.
[[nodiscard]] int count_session_messages(
    const std::filesystem::path& sessions_dir,
    const std::string& session_id)
{
    auto jsonl_path = sessions_dir / session_id / "messages.jsonl";
    if (!std::filesystem::exists(jsonl_path)) return 0;
    std::ifstream ifs(jsonl_path);
    std::string line;
    int count = 0;
    while (std::getline(ifs, line)) {
        if (!line.empty()) ++count;
    }
    return count;
}

/// Extract the first user message text from a session's messages.jsonl
/// as a display title.  Handles both content formats the engine writes:
///   - plain string (single TextBlock):  {"role":"user","content":"hello"}
///   - content-block array (multi-block): {"role":"user","content":[{"type":"text","text":"hello"}]}
/// Returns "Session" if no user message with text is found.
[[nodiscard]] std::string first_user_message_as_title(
    const std::filesystem::path& sessions_dir,
    const std::string& session_id)
{
    auto docs = loom::session::load_messages(sessions_dir, session_id);
    for (std::size_t i = 0; i < docs.size(); ++i) {
        auto root = docs[i].root();
        if (!root.valid() || !root.is_obj()) continue;
        auto role_val = root.get("role");
        if (!role_val.is_str() || role_val.as_str() != std::string_view("user"))
            continue;
        auto content_val = root.get("content");
        if (!content_val.valid()) continue;

        std::string text;
        if (content_val.is_str()) {
            // Single TextBlock: content is a plain string.
            text = std::string(content_val.as_str());
        } else if (content_val.is_arr()) {
            // Multiple blocks: find the first {"type":"text","text":"…"}.
            for (std::size_t j = 0; j < content_val.size(); ++j) {
                auto block = content_val.at(j);
                if (!block.is_obj()) continue;
                auto type_val = block.get("type");
                if (!type_val.is_str() ||
                    type_val.as_str() != std::string_view("text"))
                    continue;
                auto text_val = block.get("text");
                if (!text_val.is_str()) continue;
                text = std::string(text_val.as_str());
                break;
            }
        }

        if (!text.empty()) {
            for (auto& c : text) if (c == '\n') c = ' ';
            if (text.size() > 60) text = text.substr(0, 60) + "…";
            return text;
        }
    }

    return "Session";
}

// ── HandleSubmit (moved out of app_autocomplete.cpp to reduce import closure) ──
void AppAdapter::HandleSubmit(const std::string& text,
                              repl::InputMode submit_mode,
                              bool wait_for_pastes) {
    if (wait_for_pastes && HasInFlightPasteReferences(text)) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        pending_paste_submission_ = PendingPasteSubmission{text, submit_mode, deadline};
        paste_submission_deadline_ms_.store(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline.time_since_epoch()).count());
        return;
    }

    // Parse [Image #N] / [...Truncated text #N] refs in the submitted text.
    const auto refs = loom::utils::parse_references(text);
    std::unordered_set<int> referenced_ids;
    int n_images = 0;
    for (const auto& r : refs) {
        if (paste_.pasted_contents().contains(r.id)) {
            referenced_ids.insert(r.id);
            ++n_images;
        }
    }
    const bool has_images = n_images > 0;
    if (text.empty() && !has_images) return;

    if (text.starts_with('/')) {
        this->HandleCommand(text);
        return;
    }

    // P0-1: bash (!) routing.
    const bool bash_by_mode   = submit_mode == repl::InputMode::Bash;
    const bool bash_by_prefix = text.starts_with('!');
    if (bash_by_mode || bash_by_prefix) {
        const std::string stripped(
            bash_by_prefix ? std::string(figs::strip_mode_prefix(text))
                           : text);
        if (!stripped.empty()) {
            this->RunLocalBashCommand(stripped);
            screen_state_->prompt_store.input_mode = repl::InputMode::Normal;
        }
        return;
    }

    if (query_running_.load()) return;

    screen_state_->prompt_store.submit_count++;

    // Persist to prompt history so @history / Ctrl+R can find this prompt.
    acsrc::append_prompt_history(text, current_session_id_, screen_state_->cwd);

    query_running_.store(true);
    screen_state_->query_running = true;
    last_submitted_text_ = text;
    repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::Requesting);
    screen_state_->task_view_store.spinner_verb = "Thinking";
    {
        std::lock_guard lk(result_mutex_);
        pending_error_.reset();
        streaming_text_.clear();
        streaming_text_index_.reset();
        streaming_markdown_.reset();
        streaming_tools_.clear();
        streaming_thinking_.clear();
        event_dedup_.clear();
    }

    conversation_projection_dirty_.store(true);
    welcome_animation_active_.store(false);

    // Snapshot only still-referenced images for this submission.
    std::vector<ImageBlock> attachments;
    attachments.reserve(referenced_ids.size());
    for (int id : referenced_ids) {
        if (auto it = paste_.pasted_contents().find(id); it != paste_.pasted_contents().end()) {
            attachments.push_back(std::move(it->second));
        }
    }
    for (int id : referenced_ids) paste_.pasted_contents().erase(id);

    // Expand [...Truncated text #N] refs into their full text.
    std::string expanded_text = loom::utils::expand_pasted_text_refs(
        text, [this](int id) -> std::optional<std::string> {
            auto it = paste_.pasted_text_contents().find(id);
            if (it != paste_.pasted_text_contents().end()) return it->second;
            return std::nullopt;
        });
    for (const auto& ref : refs) paste_.pasted_text_contents().erase(ref.id);

    query_thread_ = std::jthread([this, text = std::move(expanded_text), attachments = std::move(attachments)](std::stop_token st) {
        core::QueryOptions opts;
        for (const auto& img : attachments) {
            opts.attachments.push_back(img);
        }
        opts.on_commit = [this](const Message&) {
            conversation_projection_dirty_.store(true);
            TriggerStatuslineUpdate();
            PostRenderEvent();
        };
        opts.on_event = [this, &st](const core::StreamEvent& ev) {
            if (st.stop_requested()) return;
            handle_stream_event(ev);
        };

        // AT-02: materialize @-mention file references into content blocks.
        auto materialized = atatt::materialize_at_mentions(text, screen_state_->cwd);
        for (auto& b : materialized.blocks) {
            opts.attachments.push_back(std::move(b));
        }

        static_cast<loom::core::QueryEngine*>(engine_raw())->stream_query(materialized.text, opts);

        query_running_.store(false);
        screen_state_->query_running = false;
        PostRenderEvent();
    });
}

// ── Stream-event handler (extracted from the on_event lambda so tests can
//    inject events directly — RFC 0003) ──────────────────────────────────
void AppAdapter::handle_stream_event(const core::StreamEvent& ev) {
    bool apply_event = true;

    std::visit([this, &apply_event](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, core::StreamStart>) {
            std::lock_guard lk(result_mutex_);
            streaming_text_.clear();
            streaming_text_index_.reset();
            streaming_markdown_.reset();
            streaming_tools_.clear();
            streaming_thinking_.clear();
            event_dedup_.clear_indices();
            apply_event = false;
            return;
        }

        if constexpr (std::is_same_v<T, core::ContentBlockStart>) {
            apply_event = event_dedup_.should_accept_start(e.index);
            if (!apply_event) return;
            std::lock_guard lk(result_mutex_);
            if (const auto* tool = std::get_if<core::ToolUseBlock>(&e.block)) {
                streaming_tools_[e.index] = StreamingToolPreview{
                    .tool_name = tool->name,
                    .tool_use_id = tool->id.value,
                    .input_json = tool->input_json == "{}" ? std::string{} : tool->input_json,
                    .result_preview = {},
                    .compact_preview = {},
                    .error_code = 0,
                    .truncated = false,
                    .complete = false,
                    .is_error = false,
                };
                repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::ToolUse);
                screen_state_->task_view_store.spinner_verb = tool->name;
            } else if (const auto* thinking = std::get_if<core::ThinkingBlock>(&e.block)) {
                streaming_thinking_[e.index] = StreamingThinkingPreview{
                    .text = thinking->thinking,
                    .complete = false,
                    .streaming_ended_at = std::nullopt,
                    .streaming_started_at = clock::steady_now(),
                };
                repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::Thinking);
            } else if (std::get_if<core::TextBlock>(&e.block)) {
                // Track the text block's actual index so the streaming
                // projection places it in the correct order (the model can
                // emit text before tool_use: thinking → text → tool_use).
                streaming_text_index_ = e.index;
            }
        } else if constexpr (std::is_same_v<T, core::ContentBlockDelta>) {
            apply_event = event_dedup_.should_accept_delta(e.index);
            if (!apply_event) return;
            std::lock_guard lk(result_mutex_);
            if (auto tool = streaming_tools_.find(e.index); tool != streaming_tools_.end()) {
                tool->second.input_json += e.delta_text;
            } else if (auto thinking = streaming_thinking_.find(e.index); thinking != streaming_thinking_.end()) {
                thinking->second.text += e.delta_text;
            } else {
                streaming_text_ += e.delta_text;
                repl::SetSpinner(screen_state_->task_view_store, repl::SpinnerMode::Responding);
                screen_state_->task_view_store.spinner_verb = std::nullopt;
            }
        } else if constexpr (std::is_same_v<T, core::ContentBlockStop>) {
            apply_event = event_dedup_.should_accept_stop(e.index);
            if (!apply_event) return;
            std::lock_guard lk(result_mutex_);
            if (auto tool = streaming_tools_.find(e.index); tool != streaming_tools_.end())
                tool->second.complete = true;
            if (auto thinking = streaming_thinking_.find(e.index); thinking != streaming_thinking_.end()) {
                thinking->second.complete = true;
                thinking->second.streaming_ended_at =
                    clock::steady_now();
                const auto due = std::chrono::duration_cast<std::chrono::milliseconds>(
                    thinking->second.streaming_ended_at->time_since_epoch()).count() + 3000;
                auto scheduled = thinking_collapse_deadline_ms_.load();
                while ((scheduled == 0 || due < scheduled) &&
                       !thinking_collapse_deadline_ms_.compare_exchange_weak(scheduled, due)) {}
                // Cache the duration so committed thinking entries (which
                // lack timing info) can show "∴ Thought for Xs".
                if (thinking->second.streaming_started_at) {
                    auto dur = std::chrono::duration_cast<
                        std::chrono::milliseconds>(
                            *thinking->second.streaming_ended_at -
                            *thinking->second.streaming_started_at);
                    thinking_duration_cache_[thinking->second.text] = dur;
                }
            }
        } else if constexpr (std::is_same_v<T, core::ToolExecutionStart>) {
            apply_event = event_dedup_.should_accept_exec_start(e.tool_use_id);
            if (!apply_event) return;
            std::lock_guard lk(result_mutex_);
            for (auto& [idx, preview] : streaming_tools_) {
                if (preview.tool_use_id == e.tool_use_id &&
                    !preview.exec_done &&
                    preview.result_preview.empty()) {
                    preview.result_preview = "Starting…";
                    break;
                }
            }
        } else if constexpr (std::is_same_v<T, core::ToolExecutionProgress>) {
            std::lock_guard lk(result_mutex_);
            for (auto& [idx, preview] : streaming_tools_) {
                if (preview.tool_use_id == e.tool_use_id && !preview.exec_done) {
                    preview.result_preview = e.partial_result;
                    break;
                }
            }
        } else if constexpr (std::is_same_v<T, core::ToolExecutionEnd>) {
            apply_event = event_dedup_.should_accept_exec_end(e.tool_use_id);
            if (!apply_event) return;
            std::lock_guard lk(result_mutex_);
            for (auto& [idx, preview] : streaming_tools_) {
                if (preview.tool_use_id == e.tool_use_id) {
                    preview.result_preview = e.result;
                    preview.is_error       = e.is_error;
                    preview.exec_done      = true;
                    const auto aug = pl::augment_tool_result(
                        preview.result_preview, preview.is_error);
                    preview.truncated       = aug.truncated;
                    preview.error_code      = aug.error_code;
                    preview.compact_preview = std::move(aug.preview);
                    break;
                }
            }
        } else if constexpr (std::is_same_v<T, core::StreamError>) {
            std::lock_guard lk(result_mutex_);
            pending_error_ = e.message;
        }
    }, ev);

    if (apply_event) {
        TriggerStatuslineUpdate();
        PostRenderEvent();
    }
}

// ── HandleCommand (moved out of app_autocomplete.cpp to reduce import closure) ──
void AppAdapter::HandleCommand(std::string_view cmd) {
    std::string command = trim_ascii_copy(cmd);
    std::string_view normalized = command;
    if (normalized.empty()) return;

    if (normalized == "/exit" || normalized == "/quit") {
        if (on_exit_) on_exit_();
        return;
    }
    if (normalized == "/clear") {
        static_cast<loom::core::QueryEngine*>(engine_raw())->clear_conversation();
        local_command_messages_.clear();
        screen_state_->messages_store.divider_index.reset();
        screen_state_->messages_store.unseen_divider.reset();
        screen_state_->messages_store.unseen_message_count = 0;
        screen_state_->messages_store.pill_visible = false;
        ResetScrollToBottom(screen_state_->messages_store);
        this->SyncState();
        return;
    }
    if (normalized == "/compact") {
        // P2 gap stashed-prompt: stash current input before compact so
        // the user's typed text survives the context compression.
        // The stash preserves input across operations that would
        // otherwise lose it (compact, tool-use).
        const auto& input = screen_state_->input_text;
        if (!input.empty()) {
            const auto refs = loom::utils::parse_references(input);
            std::unordered_map<int, ImageBlock> ref_images;
            std::unordered_map<int, std::string> ref_texts;
            for (const auto& r : refs) {
                if (auto it = paste_.pasted_contents().find(r.id);
                    it != paste_.pasted_contents().end()) {
                    ref_images[r.id] = it->second;
                }
                if (auto it = paste_.pasted_text_contents().find(r.id);
                    it != paste_.pasted_text_contents().end()) {
                    ref_texts[r.id] = it->second;
                }
            }
            repl::StashCurrentPrompt(screen_state_,
                std::move(ref_images), std::move(ref_texts));
            repl::set_prompt_input_text(screen_state_, {}, 0);
        }
        auto result = static_cast<loom::core::QueryEngine*>(engine_raw())->compact_conversation();
        if (result) {
            this->SyncState();
            // Restore stash after the operation that triggered it
            // finishes.
            if (repl::HasStashedPrompt(screen_state_)) {
                std::unordered_map<int, ImageBlock> ri;
                std::unordered_map<int, std::string> rt;
                repl::RestoreStashedPrompt(screen_state_, &ri, &rt);
                for (auto& [id, img] : ri) paste_.pasted_contents()[id] = std::move(img);
                for (auto& [id, txt] : rt) paste_.pasted_text_contents()[id] = std::move(txt);
            }
        }
        return;
    }
    if (normalized == "/cost") {
        auto usage = static_cast<loom::core::QueryEngine*>(engine_raw())->get_usage();
        auto cost = static_cast<loom::core::QueryEngine*>(engine_raw())->budget_tracker().current_spend_usd;
        screen_state_->task_view_store.spinner_tip = std::format(
            "Cost: ${:.4f} | In: {} | Out: {} | Ctx: {:.0f}%",
            cost, usage.input_tokens, usage.output_tokens,
            static_cast<loom::core::QueryEngine*>(engine_raw())->context_utilization() * 100.0);
        return;
    }
    if (normalized.starts_with("/model")) {
        auto args_start = normalized.find(' ');
        if (args_start != std::string_view::npos) {
            auto new_model = normalized.substr(args_start + 1);
            auto params = static_cast<loom::core::QueryEngine*>(engine_raw())->model_params();
            std::string old_model = params.model;
            params.model = std::string(new_model);
            static_cast<loom::core::QueryEngine*>(engine_raw())->set_model_params(std::move(params));

            // M7.5: Show model switch confirmation banner
            dtrig::PushModelSwitch(
                screen_state_->dialog_store.dialog_queue,
                old_model,
                std::string(new_model),
                [this](bool confirm) {
                    (void)confirm; // always confirmed via /model command
                    screen_state_->dialog_store.PopBottom(
                        /*is_prompt_input_active=*/false);
                    PostRenderEvent();
                });
        }
        this->SyncState();
        return;
    }
    if (normalized.starts_with("/vim")) {
        auto args_start = normalized.find(' ');
        if (args_start == std::string_view::npos)
            set_vim_enabled(!vim_enabled());
        else {
            auto arg = normalized.substr(args_start + 1);
            set_vim_enabled(arg == "on" || arg == "1");
        }
        if (vim_enabled()) loom::vim::enable_vim_mode();
        else loom::vim::disable_vim_mode();
        this->TriggerStatuslineUpdate();
        return;
    }

    if (normalized == "/config" || normalized == "/settings") {
        screen_state_->mode = repl::ReplMode::SettingsView;
        screen_state_->dialog_store.settings_config = nullptr;
        return;
    }

    if (normalized == "/agents" || normalized == "/agents list") {
        this->OpenAgentsMenu();
        return;
    }

    if (normalized == "/teams") {
        this->OpenTeamsOverview();
        return;
    }

    if (normalized == "/skills") {
        this->OpenSkillsMenu();
        return;
    }

    if (auto parsed = loom::core::CommandRegistry::parse(normalized)) {
        const bool known_command =
            static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw()) && static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->has_command(parsed->name);
        if (!known_command) {
            if (auto skill =
                    acsrc::find_skill_suggestion(screen_state_->cwd, parsed->name)) {
                // SL-10: reject skills marked user_invocable=false (model-only).
                if (!skill->user_invocable) {
                    AppendLocalCommandInputMessage(std::string(normalized));
                    AppendLocalCommandMessage(
                        std::format("This skill can only be invoked by Loom, not "
                                    "directly by users. Ask Loom to use the \"{}\" skill.",
                                    skill->name),
                        true);
                    return;
                }
                std::string user_text;
                const auto args_start = normalized.find(' ');
                if (args_start != std::string_view::npos) {
                    user_text = trim_ascii_copy(normalized.substr(args_start + 1));
                }
                loom::utils::skill_usage::record_skill_usage(skill->name);  // SL-04
                this->HandleSubmit(acsrc::skill_invocation_prompt(*skill, user_text));
                return;
            }
            // SL-09: unknown-command file-path disambiguation
            const auto looks_like_command = [](std::string_view name) {
                return !name.empty() &&
                    name.find_first_not_of(
                        "abcdefghijklmnopqrstuvwxyz"
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "0123456789:-_") == std::string_view::npos;
            };
            if (looks_like_command(parsed->name)) {
                std::error_code ec;
                const std::filesystem::path candidate{"./" + parsed->name};
                if (std::filesystem::exists(candidate, ec) && !ec) {
                    AppendLocalCommandInputMessage(std::string(normalized));
                    AppendLocalCommandMessage(
                        std::format("\"/{}\" is not a command or skill, but \"{}\" "
                                    "exists in the current directory. If you meant to "
                                    "reference the file, drop the leading slash.",
                                    parsed->name, candidate.string()),
                        false);
                    return;
                }
            }
        }
    }

    if (static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())) {
        auto result = static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->execute(
            command,
            command_context_for_engine(static_cast<loom::core::QueryEngine*>(engine_raw()), app_store_raw(), screen_state_->cwd));
        if (result) {
            if (result->status == CommandStatus::Injected) {
                this->HandleSubmit(result->message);
                return;
            }
            if (result->metadata == "EXIT" && on_exit_) {
                on_exit_();
                return;
            }
            // M7.5: Try to interpret command metadata as a dialog trigger.
            if (result->metadata && !result->metadata->empty()) {
                if (*result->metadata == "UI:permissions") {
                    screen_state_->dialog_store.settings_initial_tab = 3;
                    screen_state_->dialog_store.settings_component.reset();
                    screen_state_->mode = repl::ReplMode::SettingsView;
                    this->TriggerStatuslineUpdate();
                    PostRenderEvent();
                    return;
                }
                // ── /resume: open the interactive session picker or
                //    directly resume a session by ID ──
                if (result->metadata->starts_with("UI:resume")) {
                    auto* engine = static_cast<loom::core::QueryEngine*>(engine_raw());
                    auto sessions_dir = engine && engine->sessions_dir()
                        ? *engine->sessions_dir()
                        : this->sessions_dir();

                    if (*result->metadata == "UI:resume") {
                        // ── Open the interactive session picker dialog ──
                        auto metas = loom::session::list_recent_sessions(sessions_dir, 50);
                        std::vector<dsys::SessionPickerEntry> entries;
                        entries.reserve(metas.size());
                        for (auto& m : metas) {
                            // Count messages from the actual file on disk
                            // rather than trusting metadata.json's
                            // message_count, which is 0 for sessions that
                            // use messages.jsonl (the engine appends to the
                            // file but doesn't update the metadata count).
                            int actual_count = count_session_messages(
                                sessions_dir, m.session_id);
                            if (actual_count == 0) continue;
                            auto title = m.title.value_or("Session");
                            if (title.empty() || title == "Session") {
                                title = first_user_message_as_title(
                                    sessions_dir, m.session_id);
                            }
                            entries.push_back(dsys::SessionPickerEntry{
                                .session_id = m.session_id,
                                .title = std::move(title),
                                .age_string = format_session_age(m.last_active),
                                .message_count = actual_count,
                                .cwd = m.cwd.string(),
                                .model = m.model,
                            });
                        }
                        dtrig::PushSessionPicker(
                            screen_state_->dialog_store.dialog_queue,
                            std::move(entries),
                            [this](const std::string& id) {
                                if (!id.empty()) {
                                    this->HandleCommand("/resume " + id);
                                }
                            });
                        AppendLocalCommandMessage(result->message, false);
                        PostRenderEvent();
                        return;
                    }

                    // ── Direct resume: "UI:resume:<session_id>" ──
                    auto session_id = result->metadata->substr(
                        std::string("UI:resume:").size());
                    std::vector<loom::core::Message> messages;

                    // Load messages.jsonl (the current format).
                    auto docs = loom::session::load_messages(sessions_dir, session_id);
                    messages.reserve(docs.size());
                    for (std::size_t i = 0; i < docs.size(); ++i) {
                        auto parsed = loom::query::parse_session_message_value(
                            docs[i].root(), i);
                        if (parsed) messages.push_back(std::move(*parsed));
                    }

                    engine->restore_conversation(std::move(messages));
                    engine->set_session_id(session_id);
                    // Refresh the transcript to show the restored messages.
                    local_command_messages_.clear();
                    screen_state_->messages_store.divider_index.reset();
                    screen_state_->messages_store.unseen_divider.reset();
                    screen_state_->messages_store.unseen_message_count = 0;
                    screen_state_->messages_store.pill_visible = false;
                    ResetScrollToBottom(screen_state_->messages_store);
                    this->SyncState();
                    AppendLocalCommandMessage(result->message, false);
                    PostRenderEvent();
                    return;
                }
                auto enqueue_fn = [this](std::string_view c) {
                    if (c.starts_with('/')) {
                        this->HandleCommand(c);
                    }
                };
                if (dtrig::PushFromCommandMetadata(
                        screen_state_->dialog_store.dialog_queue,
                        *result->metadata,
                        enqueue_fn))
                {
                    AppendLocalCommandMessage(result->message, !result->ok || result->status == core::CommandStatus::Failed);
                    PostRenderEvent();
                    return;
                }
            }
            AppendLocalCommandMessage(result->message, !result->ok || result->status == core::CommandStatus::Failed);
            return;
        }
        AppendLocalCommandMessage(result.error().message, true);
        return;
    } else {
        AppendLocalCommandMessage("Command registry is not available.", true);
    }
}

}  // namespace loom::ui
