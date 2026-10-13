// app_prompt_suggestion_wiring.cpp — impl unit for loom.ui.app. Keeps the heavy
// loom.services.prompt_suggestion import (1004-line service) OUT of app.cppm so
// app's BMI stays under clang's 2GB source-location budget — test_ui.cpp and
// other loom.ui importers would otherwise blow the budget transitively. Defines
// wire_prompt_suggestion_hook(), which is declared (not defined) in app.cppm.
//
// Phase A (import std): this impl unit deliberately keeps std TEXTUAL and
// does NOT `import std;`. clang 22.1.8's reduced-BMI writer (LLVM #184957,
// fixed in clang 23 / PR #179178) leaks a duplicate aligned operator new into
// impl units of a primary whose GMF pulls libc++ textually (app.cppm via FTXUI),
// causing "operator new is ambiguous". Re-evaluate after the toolchain upgrade.
module;

module loom.ui.app.app;

import std;

import loom.query.query_engine;
import loom.hooks.lifecycle_hooks;

import loom.types.types;
import loom.hooks.lifecycle_hooks;
import loom.services.prompt_suggestion;

namespace loom::ui {

// SL-11: register a QueryEnd hook that reads the engine conversation, runs the
// deterministic PromptSuggestionService ranker (no LLM / speculation — those
// subsystems don't exist in cpp), and publishes an owned result for the UI
// thread to apply to the empty-prompt renderer.
void wire_prompt_suggestion_hook(void* hooks_v, void* engine_v,
                                 std::function<void(std::optional<std::string>)> publish) {
    auto& hooks = *static_cast<loom::hooks::LifecycleHookRegistry*>(hooks_v);
    auto* engine = static_cast<core::QueryEngine*>(engine_v);
    hooks.on_query_end([engine, publish = std::move(publish)](const loom::hooks::QueryEndEvent& ev) {
        if (!ev.success) {
            publish(std::nullopt);
            return;
        }
        try {
            loom::services::prompt_suggestion::SuggestionRequest req;
            for (const auto& msg : engine->get_conversation()) {
                std::visit([&](const auto& m) {
                    using M = std::remove_cvref_t<decltype(m)>;
                    std::string role;
                    if constexpr (std::is_same_v<M, loom::core::UserMessage>)
                        role = "user";
                    else if constexpr (std::is_same_v<M, loom::core::AssistantMessage> ||
                                       std::is_same_v<M, loom::core::ToolUseMessage>)
                        role = "assistant";
                    else
                        return;  // System / ToolResult skipped
                    std::string body;
                    for (const auto& blk : m.content) {
                        if (auto* tb = std::get_if<loom::core::TextBlock>(&blk)) {
                            if (!body.empty()) body.push_back('\n');
                            body += tb->text;
                        }
                    }
                    if (body.empty()) return;
                    req.recent_turns.push_back(
                        {std::move(role), std::move(body), m.timestamp});
                }, msg);
            }
            req.max_suggestions = 1;
            req.include_speculative = true;
            const auto ranked =
                loom::services::prompt_suggestion::PromptSuggestionService{}.suggest(req);
            if (ranked && !ranked->empty() && !(*ranked)[0].text.empty()) {
                publish((*ranked)[0].text);
            } else {
                publish(std::nullopt);
            }
        } catch (...) {
            publish(std::nullopt);
        }
    });
}

}  // namespace loom::ui
