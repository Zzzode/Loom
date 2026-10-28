/// Opt-in real-provider semantic continuation gate for RFC 0025.
#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"
#include <gtest/gtest.h>

#include <cstdlib>
import std;
import loom.config.config;
import loom.query.assembly;
import loom.query.query_engine;
import loom.tools.tool;
import loom.types.types;
import loom.platform.env.env;

namespace {
namespace fs = std::filesystem;
using namespace loom::core;

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, std::string_view value)
        : name(std::move(key))
        , previous(loom::utils::env::get_env(name)) {
        if (!loom::utils::env::set_env(name, value))
            throw std::runtime_error("Unable to isolate live-test environment: " + name);
    }

    ~EnvironmentGuard() {
        if (previous)
            (void)loom::utils::env::set_env(name, *previous);
        else
            (void)loom::utils::env::unset_env(name);
    }
};

class LiveArithmetic final : public ITool {
public:
    const ToolDefinition& definition() const override { return definition_; }

    Result<ToolResult> execute(const ToolInput&) override {
        return ToolResult::success(std::to_string(19 + 28));
    }

    bool check_permission(const ToolInput&) const override { return true; }

private:
    ToolDefinition definition_{.name = "CheckpointArithmetic",
                               .description = "Return the verified sum of 19 and 28",
                               .permission = ToolPermission::ReadOnly};
};

std::string message_text(const Message& message) {
    std::string text;
    std::visit(
        [&](const auto& value) {
            for (const auto& block : value.content)
                if (const auto* part = std::get_if<TextBlock>(&block)) text += part->text;
        },
        message);
    return text;
}
}  // namespace

TEST(CompactionLive, MainPreservesLateConstraintAndCompletedToolProgress) {
    const char* settings_path = std::getenv("LOOM_COMPACTION_LIVE_SETTINGS");
    if (settings_path == nullptr || std::string_view(settings_path).empty())
        GTEST_SKIP() << "Set LOOM_COMPACTION_LIVE_SETTINGS to an isolated provider settings.json "
                        "to run paid live validation";
    const char* evidence_path = std::getenv("LOOM_COMPACTION_LIVE_EVIDENCE");
    ASSERT_NE(evidence_path, nullptr) << "Live validation requires an explicit evidence directory";
    const auto evidence = fs::absolute(evidence_path) / "main";
    fs::create_directories(evidence);
    const auto root = evidence / "runtime";
    fs::create_directories(root);
    ConfigManager manager{fs::path(settings_path)};
    const auto loaded = manager.load(LoadOptions{.quiet = true});
    ASSERT_TRUE(loaded) << loaded.error().format();
    fs::create_directories(evidence / "config");
    EnvironmentGuard config_guard{"LOOM_CONFIG_DIR", (evidence / "config").string()};
    EnvironmentGuard extraction_guard{"LOOM_ENABLE_MEMORY_EXTRACTION", "0"};
    EnvironmentGuard auto_memory_guard{"LOOM_DISABLE_AUTO_MEMORY", "1"};
    loom::query::AssemblyOverrides overrides;
    overrides.cwd = root.string();
    if (const char* wire = std::getenv("LOOM_COMPACTION_LIVE_WIRE_API")) overrides.wire_api = wire;
    auto resolved = loom::query::resolve_engine_config(manager.settings(), overrides);
    ASSERT_TRUE(resolved) << resolved.error().format();
    auto settings = std::move(*resolved);
    if (const char* token = std::getenv("LOOM_COMPACTION_LIVE_AUTH_TOKEN"))
        settings.auth_token = token;
    else if (const char* token = std::getenv("LOOM_AUTH_TOKEN"))
        settings.auth_token = token;
    ASSERT_FALSE(settings.api_key.empty() && settings.auth_token.empty())
        << "No configured provider credential";
    settings.context_window.auto_compact = false;
    settings.context_window.max_context_tokens = 8192;
    settings.model_params.max_tokens = 1024;
    settings.retry_policy.max_retries = 0;
    settings.thinking_config.mode = ThinkingConfig::Mode::Disabled;
    settings.native_computer_tool = false;
    settings.custom_system_prompt =
        "Execute only the local verification task. Never commit, push, publish or modify "
        "repository files. Respect the checkpoint and current user request.";
    ToolRegistry registry;
    registry.register_tool(std::make_unique<LiveArithmetic>());
    const auto arithmetic = registry.execute("CheckpointArithmetic", ToolInput::from_json("{}"));
    ASSERT_TRUE(arithmetic);
    ASSERT_FALSE(arithmetic->is_error);
    ASSERT_EQ(arithmetic->content.front().text, "47");
    // No mutation-capable tool is exposed to either the checkpoint or business model.
    settings.tools.clear();
    QueryEngine engine(settings, registry);
    engine.set_session_storage(evidence / "sessions");
    SystemMessage system{};
    system.id = MessageId{"controlled-live-system"};
    system.content.emplace_back(TextBlock{*settings.custom_system_prompt});
    engine.restore_conversation({Message{std::move(system)}});
    engine.set_dump_prompts_dir(evidence / "dump-prompts");
    const auto nonce = "LATE_CONSTRAINT_" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    UserMessage old{};
    old.id = MessageId{"live-original-task"};
    std::string background;
    for (int i = 0; i < 80; ++i)
        background +=
            "Background observation " + std::to_string(i) +
            ": the local fixture uses existing interfaces and reversible local validation.\n";
    old.content.emplace_back(TextBlock{
        background +
        "\nCritical constraint: NEVER COMMIT, PUSH OR PUBLISH. Preserve and repeat the exact "
        "marker " +
        nonce + " in the final answer. The verified arithmetic result must also be reported."});
    engine.append_message_for_testing(old);
    AssistantMessage call{};
    call.id = MessageId{"live-completed-tool"};
    call.content.emplace_back(ToolUseBlock{ToolUseId{"live-sum"}, "CheckpointArithmetic", "{}"});
    engine.append_message_for_testing(call);
    ToolResultMessage tool_result{};
    tool_result.id = MessageId{"live-completed-result"};
    tool_result.tool_use_id = ToolUseId{"live-sum"};
    tool_result.tool_name = "CheckpointArithmetic";
    tool_result.content.emplace_back(TextBlock{arithmetic->content.front().text});
    engine.append_message_for_testing(tool_result);
    AssistantMessage progress{};
    progress.id = MessageId{"live-progress"};
    progress.content.emplace_back(
        TextBlock{background + "\nCompleted: CheckpointArithmetic executed successfully; returned "
                               "47. No repository files changed."});
    engine.append_message_for_testing(progress);
    UserMessage current{};
    current.id = MessageId{"live-current-request"};
    const std::string current_text =
        "CURRENT_REQUEST_EXACT: Finish verification. Report the preserved exact marker from the "
        "original task, the completed arithmetic result, and the prohibition on "
        "commit/push/publication. Do not execute any tool.";
    current.content.emplace_back(TextBlock{current_text});
    engine.append_message_for_testing(current);
    const auto before = engine.get_conversation();
    const auto compacted = engine.compact_conversation();
    ASSERT_TRUE(compacted) << compacted.error().format();
    const auto after = engine.get_conversation();
    std::string summary;
    bool preserved_current = false;
    for (const auto& message : after) {
        const auto text = message_text(message);
        if (text.find("## Goal") != std::string::npos) summary += text;
        if (text == current_text) preserved_current = true;
    }
    std::ofstream(evidence / "checkpoint.txt") << summary;
    ASSERT_NE(summary.find(nonce), std::string::npos) << summary;
    ASSERT_NE(summary.find("47"), std::string::npos) << summary;
    for (const auto word : {"commit", "push", "publish"}) {
        auto lower = summary;
        std::ranges::transform(lower, lower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        ASSERT_TRUE(
            lower.find(word) != std::string::npos ||
            (std::string_view(word) == "publish" && lower.find("publication") != std::string::npos))
            << word << ": " << summary;
    }
    ASSERT_TRUE(preserved_current);
    const auto answer =
        engine.query("Continue CURRENT_REQUEST_EXACT using the preserved task state.");
    ASSERT_TRUE(answer) << answer.error().format();
    const auto text = message_text(answer->message);
    std::ofstream(evidence / "final-answer.txt") << text;
    EXPECT_NE(text.find(nonce), std::string::npos) << text;
    EXPECT_NE(text.find("47"), std::string::npos) << text;
    for (const auto word : {"commit", "push", "publish"}) {
        auto lower = text;
        std::ranges::transform(lower, lower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        EXPECT_TRUE(
            lower.find(word) != std::string::npos ||
            (std::string_view(word) == "publish" && lower.find("publication") != std::string::npos))
            << word << ": " << text;
    }
    std::ofstream manifest(evidence / "manifest.txt");
    manifest << "test=CompactionLive.MainPreservesLateConstraintAndCompletedToolProgress\nmodel="
             << settings.model_params.model << "\nwire=" << settings.wire_api.value_or("messages")
             << "\nmessages_before=" << before.size() << "\nmessages_after=" << after.size()
             << "\ninput_tokens=" << engine.get_usage().input_tokens
             << "\noutput_tokens=" << engine.get_usage().output_tokens << '\n';
}
