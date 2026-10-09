/// @file test_agent.cpp
/// @brief Agent runtime and agent tool tests.

#pragma clang diagnostic ignored "-Wmissing-designated-field-initializers"

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <httplib.h>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#endif

import std;
import loom.tools.bash;
import loom.tools.computer_use;
import loom.tools.powershell;
import loom.tools.runtime_computer_use;
import loom.tools.runtime_shared_utils;
import loom.tools.runtime_team_shared;
import loom.tools.runtime_message_delivery;
import loom.tools.send_message;
import loom.tools.web_fetch;
import loom.tools.web_search;
import loom.tools.web_browser;
import loom.orchestration.tools.mcp;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.runtime_backends;
import loom.tools.runtime_backends.port;  // RFC-0001 B15: set_/clear_skill_loader_executor slot API
import loom.tools.image_codec.port;
import loom.tools.worktree;
import loom.orchestration.agent;
import loom.tools.agent_runtime;
import loom.tools.file_read;
import loom.tools.file_write;
import loom.tools.todo_write;
import loom.tools.notebook;
import loom.tools.registry;
import loom.tools.runtime_registry;
import loom.config.config;
import loom.tools.path_validation;
import loom.tools.bash_security;
import loom.tools.bash_permissions;
import loom.tools.task;
import loom.tools.team;
import loom.tools.team_create;
import loom.tools.team_delete;
import loom.tools.tool;
import loom.serdes.json;
import loom.security.tool_deny_rules;
import loom.query.query_engine;
import loom.teams.swarm.backends;
import loom.teams.swarm.helpers;
import loom.teams.team_helpers;
import loom.hooks.tool_permissions;
import loom.services.api.client;
import loom.services.mcp.types;
import loom.services.mcp.connection_manager;  // RFC-0001 B6: svc_mcp::McpServerSnapshot
import loom.tools.repl;
import loom.tools.skill;
import loom.orchestration.agent.utils;
import loom.tools.destructive_command_warning;

namespace fs = std::filesystem;

// The agent helpers exercised by the existing tests live in
// loom::tools::agent::utils after the agent_tool split; re-expose them through
// the loom::tools::agent namespace so the historical call sites still resolve.
namespace loom::tools::agent { using namespace utils; }

namespace {

// Tests exercise runtime-tool *executor* logic, not permission gating. Supply
// an allow-all live checker so the fail-closed default in RuntimeFunctionTool
// does not block them. Production paths must supply a real checker (or accept
// fail-closed denial for write/execute/network tools).
loom::tools::agent::AgentLivePermissionCheckFn test_allow_all_check() {
    auto allow = []([[maybe_unused]] std::string_view,
                    [[maybe_unused]] std::string_view,
                    [[maybe_unused]] std::string_view) {
        return loom::tools::agent::AgentLivePermissionCheck{.allowed = true};
    };
    return loom::tools::agent::AgentLivePermissionCheckFn{std::move(allow)};
}

struct CurrentPathGuard {
    fs::path previous;

    explicit CurrentPathGuard(const fs::path& next) : previous(fs::current_path()) {
        fs::current_path(next);
    }

    ~CurrentPathGuard() {
        std::error_code ec;
        fs::current_path(previous, ec);
    }
};

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;

    EnvironmentGuard(std::string key, const std::string& value) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        setenv(name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

struct EnvironmentUnsetGuard {
    std::string name;
    std::optional<std::string> previous;

    explicit EnvironmentUnsetGuard(std::string key) : name(std::move(key)) {
        if (const char* existing = std::getenv(name.c_str())) {
            previous = existing;
        }
        unsetenv(name.c_str());
    }

    ~EnvironmentUnsetGuard() {
        if (previous) {
            setenv(name.c_str(), previous->c_str(), 1);
        } else {
            unsetenv(name.c_str());
        }
    }
};

// RFC-0001 B11/B12: the image codec and the SkillLoader skill executor are
// process-global function-local statics installed by
// loom::orchestration::install_runtime_backends() (std::call_once; safe for
// the per-request server threads). Every test that reads an image or PDF
// through Read, exercises a computer_use screenshot/base64 path, or relies
// on SkillLoader directory/plugin discovery installs the real
// services-backed backends for its duration and clears the slots in the
// destructor so later cases stay hermetic. The constructor installs
// directly as well: call_once makes a repeat install a no-op after a
// previous guard's destructor cleared the slots.
struct FileToolServicesGuard {
    FileToolServicesGuard() {
        loom::orchestration::install_runtime_backends();
        loom::tools::image_codec::set_codec(
            loom::orchestration::make_image_codec());
        loom::tools::set_skill_loader_executor(
            loom::orchestration::make_skill_loader_executor());
    }
    ~FileToolServicesGuard() {
        loom::tools::image_codec::clear_codec();
        loom::tools::clear_skill_loader_executor();
    }
};

std::string read_file(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

loom::tools::AgentLivePermissionCheck check_agent_tool_permission_from_hook(
    loom::hooks::ToolPermissionHook& permission_hook,
    std::string_view tool_name,
    std::string_view input_json,
    std::string_view tool_use_id
) {
    permission_hook.set_current_tool_use_id(tool_use_id);
    auto response = permission_hook.can_use_response(tool_name, input_json);
    permission_hook.clear_current_tool_use_id();

    loom::tools::AgentLivePermissionCheck check;
    check.allowed = response.decision == loom::hooks::PermissionDecision::allow ||
                    response.decision == loom::hooks::PermissionDecision::allow_once;
    check.updated_input_json = std::move(response.updated_input_json);
    check.message = std::move(response.message);
    return check;
}

std::string shell_quote_for_test(std::string_view value) {
    std::string out = "'";
    for (char ch : value) {
        if (ch == '\'') {
            out += "'\\''";
        } else {
            out += ch;
        }
    }
    out += "'";
    return out;
}

class LocalSlowMessagesStreamServer {
public:
    explicit LocalSlowMessagesStreamServer(std::chrono::milliseconds delay)
        : delay_(delay) {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
                last_body_ = req.body;
            }
            cv_.notify_all();

            std::this_thread::sleep_for(delay_);
            res.set_content(
                "event: message_start\n"
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_slow\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"late stream response\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":3}}\n\n"
                "event: message_stop\n"
                "data: {\"type\":\"message_stop\"}\n\n",
                "text/event-stream");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalSlowMessagesStreamServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] bool wait_for_request(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return request_count_ > 0; });
    }

    [[nodiscard]] bool wait_for_request_count(
        std::size_t expected,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected] { return request_count_ >= expected; });
    }

    [[nodiscard]] std::size_t request_count() const {
        std::lock_guard lock(mutex_);
        return request_count_;
    }

    [[nodiscard]] std::optional<std::string> last_body() const {
        std::lock_guard lock(mutex_);
        return last_body_;
    }

private:
    std::chrono::milliseconds delay_;
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::optional<std::string> last_body_;
};

class LocalScriptedBashToolUseMessagesServer {
public:
    explicit LocalScriptedBashToolUseMessagesServer(
        std::string command,
        std::string final_text = "scripted bash complete"
    ) : command_(std::move(command)), final_text_(std::move(final_text)) {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            std::size_t count = 0;
            {
                std::lock_guard lock(mutex_);
                count = ++request_count_;
                request_bodies_.push_back(req.body);
            }
            cv_.notify_all();

            if (count == 1) {
                const auto input_json = std::format(
                    R"({{"command":"{}","description":"hook fixture"}})",
                    loom::tools::agent::json_escape_string(command_));
                const auto partial_json = loom::tools::agent::json_escape_string(input_json);
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_scripted_bash_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_bash_fixture\",\"name\":\"Bash\",\"input\":{}}}\n\n"
                    "event: content_block_delta\n"
                    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"" + partial_json + "\"}}\n\n"
                    "event: content_block_stop\n"
                    "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                    "event: message_delta\n"
                    "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":12}}\n\n"
                    "event: message_stop\n"
                    "data: {\"type\":\"message_stop\"}\n\n",
                    "text/event-stream");
                return;
            }

            const auto final_text_json = loom::tools::agent::json_escape_string(final_text_);
            res.set_content(
                "event: message_start\n"
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_after_scripted_bash\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"" + final_text_json + "\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":3}}\n\n"
                "event: message_stop\n"
                "data: {\"type\":\"message_stop\"}\n\n",
                "text/event-stream");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalScriptedBashToolUseMessagesServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] bool wait_for_request_count(
        std::size_t expected,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected] { return request_count_ >= expected; });
    }

    [[nodiscard]] std::optional<std::string> request_body(std::size_t index) const {
        std::lock_guard lock(mutex_);
        if (index >= request_bodies_.size()) return std::nullopt;
        return request_bodies_[index];
    }

    [[nodiscard]] std::size_t request_count() const {
        std::lock_guard lock(mutex_);
        return request_count_;
    }

private:
    std::string command_;
    std::string final_text_;
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::vector<std::string> request_bodies_;
};

class LocalScriptedToolUseMessagesServer {
public:
    LocalScriptedToolUseMessagesServer(
        std::string tool_name,
        std::string tool_input_json,
        std::string tool_use_id,
        std::string final_text = "scripted tool complete"
    ) : tool_name_(std::move(tool_name)),
        tool_input_json_(std::move(tool_input_json)),
        tool_use_id_(std::move(tool_use_id)),
        final_text_(std::move(final_text)) {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            std::size_t count = 0;
            {
                std::lock_guard lock(mutex_);
                count = ++request_count_;
                request_bodies_.push_back(req.body);
            }
            cv_.notify_all();

            if (count == 1) {
                const auto tool_name_json = loom::tools::agent::json_escape_string(tool_name_);
                const auto tool_use_id_json = loom::tools::agent::json_escape_string(tool_use_id_);
                const auto partial_json = loom::tools::agent::json_escape_string(tool_input_json_);
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_scripted_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"" + tool_use_id_json + "\",\"name\":\"" + tool_name_json + "\",\"input\":{}}}\n\n"
                    "event: content_block_delta\n"
                    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"" + partial_json + "\"}}\n\n"
                    "event: content_block_stop\n"
                    "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                    "event: message_delta\n"
                    "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":12}}\n\n"
                    "event: message_stop\n"
                    "data: {\"type\":\"message_stop\"}\n\n",
                    "text/event-stream");
                return;
            }

            const auto final_text_json = loom::tools::agent::json_escape_string(final_text_);
            res.set_content(
                "event: message_start\n"
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_after_scripted_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"" + final_text_json + "\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":3}}\n\n"
                "event: message_stop\n"
                "data: {\"type\":\"message_stop\"}\n\n",
                "text/event-stream");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalScriptedToolUseMessagesServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] bool wait_for_request_count(
        std::size_t expected,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected] { return request_count_ >= expected; });
    }

    [[nodiscard]] std::optional<std::string> request_body(std::size_t index) const {
        std::lock_guard lock(mutex_);
        if (index >= request_bodies_.size()) return std::nullopt;
        return request_bodies_[index];
    }

private:
    std::string tool_name_;
    std::string tool_input_json_;
    std::string tool_use_id_;
    std::string final_text_;
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::vector<std::string> request_bodies_;
};

class LocalPerTurnBashPwdMessagesServer {
public:
    LocalPerTurnBashPwdMessagesServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
                request_bodies_.push_back(req.body);
            }
            cv_.notify_all();

            const bool has_tool_result = req.body.find(R"("tool_result")") != std::string::npos;
            if (!has_tool_result) {
                const auto partial_json = loom::tools::agent::json_escape_string(R"({"command":"pwd","description":"print working directory"})");
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_pwd_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_pwd\",\"name\":\"Bash\",\"input\":{}}}\n\n"
                    "event: content_block_delta\n"
                    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"" + partial_json + "\"}}\n\n"
                    "event: content_block_stop\n"
                    "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                    "event: message_delta\n"
                    "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"tool_use\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":12}}\n\n"
                    "event: message_stop\n"
                    "data: {\"type\":\"message_stop\"}\n\n",
                    "text/event-stream");
                return;
            }

            res.set_content(
                "event: message_start\n"
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_pwd_done\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"pwd complete\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":3}}\n\n"
                "event: message_stop\n"
                "data: {\"type\":\"message_stop\"}\n\n",
                "text/event-stream");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalPerTurnBashPwdMessagesServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] bool wait_for_request_count(
        std::size_t expected,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected] { return request_count_ >= expected; });
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::vector<std::string> request_bodies_;
};

bool wait_for_native_agent_status(
    std::string_view agent_id,
    loom::tools::agent_runtime::NativeAgentStatus expected,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1'000)
) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        auto record = loom::tools::agent_runtime::native_agent_store().get(agent_id);
        if (record && record->status == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    auto record = loom::tools::agent_runtime::native_agent_store().get(agent_id);
    return record && record->status == expected;
}

} // namespace


TEST(Tools, AgentRuntimeLoadsMarkdownDefinitions) {
    auto root = fs::temp_directory_path() / "loom_agent_definition_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "reviewer.md");
        agent << R"MD(---
name: reviewer
description: Reviews code changes
model: haiku
tools: [Read, Grep, Bash]
disallowedTools: [Bash]
permissionMode: plan
effort: 77
memory: local
color: cyan
omitLoomMd: true
criticalSystemReminder_EXPERIMENTAL: Stay within the review scope.
maxTurns: 8
initialPrompt: Inspect only changed files first.
---
You review code changes and report risks.
)MD";
    }

    auto agents = loom::tools::agent_runtime::load_agent_definitions_from_dir(
        root / ".loom" / "agents",
        "projectSettings");

    ASSERT_EQ(agents.size(), 1u);
    EXPECT_EQ(agents.front().agent_type, "reviewer");
    EXPECT_EQ(agents.front().when_to_use, "Reviews code changes");
    EXPECT_EQ(agents.front().model, "haiku");
    ASSERT_EQ(agents.front().tools.size(), 3u);
    EXPECT_EQ(agents.front().tools.front(), "Read");
    ASSERT_EQ(agents.front().disallowed_tools.size(), 1u);
    EXPECT_EQ(agents.front().disallowed_tools.front(), "Bash");
    ASSERT_TRUE(agents.front().permission_mode.has_value());
    EXPECT_EQ(*agents.front().permission_mode, "plan");
    ASSERT_TRUE(agents.front().effort.has_value());
    EXPECT_EQ(*agents.front().effort, "77");
    ASSERT_TRUE(agents.front().memory.has_value());
    EXPECT_EQ(*agents.front().memory, "local");
    ASSERT_TRUE(agents.front().color.has_value());
    EXPECT_EQ(*agents.front().color, "cyan");
    EXPECT_TRUE(agents.front().omit_loom_md);
    ASSERT_TRUE(agents.front().critical_system_reminder.has_value());
    EXPECT_EQ(*agents.front().critical_system_reminder, "Stay within the review scope.");
    ASSERT_TRUE(agents.front().max_turns.has_value());
    EXPECT_EQ(*agents.front().max_turns, 8);
    ASSERT_TRUE(agents.front().initial_prompt.has_value());
    EXPECT_EQ(*agents.front().initial_prompt, "Inspect only changed files first.");

    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeLoadsJsonDefinitions) {
    auto root = fs::temp_directory_path() / "loom_agent_json_definition_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agents(root / ".loom" / "agents" / "agents.json");
        agents << R"JSON({
  "json-reviewer": {
    "description": "Reviews JSON-defined agents",
    "prompt": "Review JSON agent migrations.",
    "model": "haiku",
    "tools": ["Read", "Bash(git status)"],
    "disallowedTools": ["Write"],
    "permissionMode": "acceptEdits",
    "effort": "max",
    "memory": "project",
    "color": "green",
    "omitLoomMd": true,
    "criticalSystemReminder": "Stay in JSON parity scope.",
    "maxTurns": 4,
    "initialPrompt": "Start with the JSON definition.",
    "background": true,
    "isolation": "worktree",
    "requiredMcpServers": ["github"],
    "mcpServers": [
      "filesystem",
      {
        "review": {
          "type": "stdio",
          "command": "node",
          "args": ["server.js"],
          "env": {"TOKEN": "secret"}
        }
      }
    ],
    "hooks": {
      "SubagentStart": [
        {
          "matcher": "json-reviewer",
          "hooks": [
            {
              "type": "command",
              "command": "echo json-hook-started",
              "shell": "bash",
              "timeoutSeconds": 3,
              "if": "always"
            }
          ]
        }
      ]
    },
    "skills": ["review-skill"]
  },
  "numeric-effort-agent": {
    "description": "Uses numeric JSON effort",
    "prompt": "Verify numeric JSON effort parity.",
    "effort": 77
  }
})JSON";
    }

    auto agents = loom::tools::agent_runtime::load_agent_definitions_from_dir(
        root / ".loom" / "agents",
        "projectSettings");

    ASSERT_EQ(agents.size(), 2u);
    auto find_agent = [&](std::string_view type) -> const loom::tools::agent_runtime::AgentDefinition* {
        for (const auto& candidate : agents) {
            if (candidate.agent_type == type) return &candidate;
        }
        return nullptr;
    };

    const auto* agent_ptr = find_agent("json-reviewer");
    ASSERT_NE(agent_ptr, nullptr);
    const auto& agent = *agent_ptr;
    EXPECT_EQ(agent.agent_type, "json-reviewer");
    EXPECT_EQ(agent.when_to_use, "Reviews JSON-defined agents");
    EXPECT_EQ(agent.system_prompt, "Review JSON agent migrations.");
    EXPECT_EQ(agent.model, "haiku");
    ASSERT_EQ(agent.tools.size(), 2u);
    EXPECT_EQ(agent.tools[1], "Bash(git status)");
    ASSERT_EQ(agent.disallowed_tools.size(), 1u);
    EXPECT_EQ(agent.disallowed_tools.front(), "Write");
    ASSERT_TRUE(agent.permission_mode.has_value());
    EXPECT_EQ(*agent.permission_mode, "acceptEdits");
    ASSERT_TRUE(agent.effort.has_value());
    EXPECT_EQ(*agent.effort, "max");
    ASSERT_TRUE(agent.memory.has_value());
    EXPECT_EQ(*agent.memory, "project");
    ASSERT_TRUE(agent.color.has_value());
    EXPECT_EQ(*agent.color, "green");
    EXPECT_TRUE(agent.omit_loom_md);
    ASSERT_TRUE(agent.critical_system_reminder.has_value());
    EXPECT_EQ(*agent.critical_system_reminder, "Stay in JSON parity scope.");
    ASSERT_TRUE(agent.max_turns.has_value());
    EXPECT_EQ(*agent.max_turns, 4);
    ASSERT_TRUE(agent.initial_prompt.has_value());
    EXPECT_EQ(*agent.initial_prompt, "Start with the JSON definition.");
    EXPECT_TRUE(agent.background);
    ASSERT_TRUE(agent.isolation.has_value());
    EXPECT_EQ(*agent.isolation, "worktree");
    ASSERT_EQ(agent.required_mcp_servers.size(), 1u);
    EXPECT_EQ(agent.required_mcp_servers.front(), "github");
    ASSERT_EQ(agent.mcp_servers.size(), 1u);
    EXPECT_EQ(agent.mcp_servers.front(), "filesystem");
    ASSERT_EQ(agent.inline_mcp_servers.size(), 1u);
    EXPECT_EQ(agent.inline_mcp_servers.front().name, "review");
    EXPECT_EQ(agent.inline_mcp_servers.front().command, "node");
    ASSERT_EQ(agent.inline_mcp_servers.front().args.size(), 1u);
    EXPECT_EQ(agent.inline_mcp_servers.front().args.front(), "server.js");
    EXPECT_EQ(agent.inline_mcp_servers.front().env.at("TOKEN"), "secret");
    EXPECT_TRUE(agent.hooks_present);
    ASSERT_TRUE(agent.hooks.contains("SubagentStart"));
    ASSERT_EQ(agent.hooks.at("SubagentStart").size(), 1u);
    ASSERT_TRUE(agent.hooks.at("SubagentStart").front().matcher.has_value());
    EXPECT_EQ(*agent.hooks.at("SubagentStart").front().matcher, "json-reviewer");
    ASSERT_EQ(agent.hooks.at("SubagentStart").front().hooks.size(), 1u);
    const auto& hook = agent.hooks.at("SubagentStart").front().hooks.front();
    EXPECT_EQ(hook.command, "echo json-hook-started");
    EXPECT_EQ(hook.shell, "bash");
    ASSERT_TRUE(hook.timeout_seconds.has_value());
    EXPECT_EQ(*hook.timeout_seconds, 3);
    ASSERT_TRUE(hook.condition.has_value());
    EXPECT_EQ(*hook.condition, "always");
    ASSERT_EQ(agent.skills.size(), 1u);
    EXPECT_EQ(agent.skills.front(), "review-skill");

    const auto* numeric_effort = find_agent("numeric-effort-agent");
    ASSERT_NE(numeric_effort, nullptr);
    ASSERT_TRUE(numeric_effort->effort.has_value());
    EXPECT_EQ(*numeric_effort->effort, "77");

    fs::remove_all(root);
}
TEST(Tools, AgentRuntimeLoadsSettingsFlagAndPolicyAgentsInPriorityOrder) {
    auto root = fs::temp_directory_path() / "loom_agent_settings_priority_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    {
        std::ofstream settings(root / ".loom" / "settings.json");
        settings << R"JSON({
  "agents": {
    "shared-agent": {
      "description": "Project shared agent",
      "prompt": "project prompt",
      "model": "haiku"
    },
    "project-only": {
      "description": "Project only agent",
      "prompt": "project only prompt"
    }
  }
})JSON";
    }
    const auto policy_path = root / "policy-settings.json";
    {
        std::ofstream policy(policy_path);
        policy << R"JSON({
  "agents": {
    "shared-agent": {
      "description": "Policy shared agent",
      "prompt": "policy prompt",
      "model": "opus"
    },
    "policy-only": {
      "description": "Policy only agent",
      "prompt": "policy only prompt"
    }
  }
})JSON";
    }

    EnvironmentGuard flag_agents("LOOM_AGENTS_JSON", R"JSON({
  "shared-agent": {
    "description": "Flag shared agent",
    "prompt": "flag prompt",
    "model": "sonnet"
  },
  "flag-only": {
    "description": "Flag only agent",
    "prompt": "flag only prompt"
  }
})JSON");
    EnvironmentGuard policy_settings("LOOM_POLICY_SETTINGS", policy_path.string());

    auto agents = loom::tools::agent_runtime::get_all_agent_definitions(root);
    auto find_agent = [&](std::string_view type) -> const loom::tools::agent_runtime::AgentDefinition* {
        for (const auto& agent : agents) {
            if (agent.agent_type == type) return &agent;
        }
        return nullptr;
    };

    auto* shared = find_agent("shared-agent");
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->source, "policySettings");
    EXPECT_EQ(shared->when_to_use, "Policy shared agent");
    EXPECT_EQ(shared->system_prompt, "policy prompt");
    EXPECT_EQ(shared->model, "opus");

    auto* flag_only = find_agent("flag-only");
    ASSERT_NE(flag_only, nullptr);
    EXPECT_EQ(flag_only->source, "flagSettings");
    EXPECT_EQ(flag_only->system_prompt, "flag only prompt");

    auto* project_only = find_agent("project-only");
    ASSERT_NE(project_only, nullptr);
    EXPECT_EQ(project_only->source, "projectSettings");
    EXPECT_EQ(project_only->system_prompt, "project only prompt");

    auto* policy_only = find_agent("policy-only");
    ASSERT_NE(policy_only, nullptr);
    EXPECT_EQ(policy_only->source, "policySettings");

    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeSimpleModeOnlyLoadsBuiltInAgents) {
    auto root = fs::temp_directory_path() / "loom_agent_simple_mode_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "project-only.md");
        agent << R"MD(---
name: project-only
description: Project agent should be hidden in simple mode
---
Project prompt.
)MD";
    }
    const auto policy_path = root / "policy-settings.json";
    {
        std::ofstream policy(policy_path);
        policy << R"JSON({
  "agents": {
    "policy-only": {
      "description": "Policy agent should be hidden in simple mode",
      "prompt": "policy prompt"
    }
  }
})JSON";
    }

    EnvironmentUnsetGuard disable_guard("LOOM_AGENT_SDK_DISABLE_BUILTIN_AGENTS");
    EnvironmentGuard simple_mode("LOOM_SIMPLE", "1");
    EnvironmentGuard flag_agents("LOOM_AGENTS_JSON", R"JSON({
  "flag-only": {
    "description": "Flag agent should be hidden in simple mode",
    "prompt": "flag prompt"
  }
})JSON");
    EnvironmentGuard policy_settings("LOOM_POLICY_SETTINGS", policy_path.string());

    auto agents = loom::tools::agent_runtime::get_all_agent_definitions(root);
    auto has_agent = [&](std::string_view type) {
        return std::ranges::any_of(agents, [&](const auto& agent) {
            return agent.agent_type == type;
        });
    };

    EXPECT_TRUE(has_agent("general-purpose"));
    EXPECT_FALSE(has_agent("project-only"));
    EXPECT_FALSE(has_agent("flag-only"));
    EXPECT_FALSE(has_agent("policy-only"));

    fs::remove_all(root);
}

TEST(Tools, BuiltInAgentDefinitionsHonorNativeFeatureGates) {
    EnvironmentUnsetGuard disable_guard("LOOM_AGENT_SDK_DISABLE_BUILTIN_AGENTS");
    EnvironmentUnsetGuard explore_guard("LOOM_ENABLE_EXPLORE_PLAN_AGENTS");
    EnvironmentUnsetGuard legacy_explore_guard("BUILTIN_EXPLORE_PLAN_AGENTS");
    EnvironmentUnsetGuard verification_guard("LOOM_ENABLE_VERIFICATION_AGENT");
    EnvironmentUnsetGuard legacy_verification_guard("VERIFICATION_AGENT");
    EnvironmentUnsetGuard entrypoint_guard("LOOM_ENTRYPOINT");

    auto has_agent = [](const std::vector<loom::tools::agent_runtime::AgentDefinition>& agents, std::string_view type) {
        return std::ranges::any_of(agents, [&](const auto& agent) {
            return agent.agent_type == type;
        });
    };

    auto defaults = loom::tools::agent_runtime::built_in_agent_definitions();
    EXPECT_TRUE(has_agent(defaults, "general-purpose"));
    EXPECT_TRUE(has_agent(defaults, "statusline-setup"));
    EXPECT_TRUE(has_agent(defaults, "loom-guide"));
    EXPECT_FALSE(has_agent(defaults, "Explore"));
    EXPECT_FALSE(has_agent(defaults, "Plan"));
    EXPECT_FALSE(has_agent(defaults, "verification"));
    auto general = std::ranges::find_if(defaults, [](const auto& agent) {
        return agent.agent_type == "general-purpose";
    });
    ASSERT_NE(general, defaults.end());
    EXPECT_EQ(general->tools, std::vector<std::string>{"*"});

    {
        EnvironmentGuard explore_enabled("LOOM_ENABLE_EXPLORE_PLAN_AGENTS", "1");
        auto enabled = loom::tools::agent_runtime::built_in_agent_definitions();
        EXPECT_TRUE(has_agent(enabled, "Explore"));
        EXPECT_TRUE(has_agent(enabled, "Plan"));
    }

    {
        EnvironmentGuard verification_enabled("LOOM_ENABLE_VERIFICATION_AGENT", "1");
        auto enabled = loom::tools::agent_runtime::built_in_agent_definitions();
        EXPECT_TRUE(has_agent(enabled, "verification"));
    }

    {
        EnvironmentGuard sdk_entrypoint("LOOM_ENTRYPOINT", "sdk-ts");
        auto sdk_agents = loom::tools::agent_runtime::built_in_agent_definitions();
        EXPECT_FALSE(has_agent(sdk_agents, "loom-guide"));
    }

    {
        EnvironmentGuard disabled("LOOM_AGENT_SDK_DISABLE_BUILTIN_AGENTS", "1");
        auto interactive_agents = loom::tools::agent_runtime::built_in_agent_definitions();
        EXPECT_FALSE(interactive_agents.empty());
        EXPECT_TRUE(has_agent(interactive_agents, "general-purpose"));
    }

    {
        EnvironmentGuard sdk_entrypoint("LOOM_ENTRYPOINT", "sdk-cli");
        EnvironmentGuard disabled("LOOM_AGENT_SDK_DISABLE_BUILTIN_AGENTS", "1");
        EXPECT_TRUE(loom::tools::agent_runtime::built_in_agent_definitions().empty());
    }
}

TEST(Tools, AgentRuntimeResolvesLooseAgentTypeInputs) {
    EnvironmentGuard explore_enabled("LOOM_ENABLE_EXPLORE_PLAN_AGENTS", "1");
    auto agents = loom::tools::agent_runtime::built_in_agent_definitions();

    auto general = loom::tools::agent_runtime::resolve_requested_agent_type("General Purpose", agents);
    ASSERT_TRUE(general.has_value());
    EXPECT_EQ(*general, "general-purpose");

    auto plan = loom::tools::agent_runtime::resolve_requested_agent_type("Plan", agents);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(*plan, "Plan");

    auto explore = loom::tools::agent_runtime::resolve_requested_agent_type("Explore", agents);
    ASSERT_TRUE(explore.has_value());
    EXPECT_EQ(*explore, "Explore");

    EXPECT_FALSE(loom::tools::agent_runtime::resolve_requested_agent_type("missing-agent-type", agents).has_value());
}

TEST(Tools, AgentToolAcceptsTypeScriptInputShape) {
    EnvironmentGuard explore_enabled("LOOM_ENABLE_EXPLORE_PLAN_AGENTS", "1");
    loom::tools::AgentConfig config;
    config.max_depth = 0;
    loom::tools::AgentTool tool(config);

    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Inspect plan",
      "prompt": "Inspect the migration plan",
      "subagent_type": "Plan",
      "model": "haiku"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Agent recursion depth limit reached"), std::string::npos);
    EXPECT_EQ(result->content.front().text.find("Missing required"), std::string::npos);
}

TEST(Tools, AgentToolRejectsUnknownAgentTypesBeforeExecution) {
    loom::tools::AgentConfig config;
    config.max_depth = 0;
    loom::tools::AgentTool tool(config);

    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Missing agent",
      "prompt": "Use an unknown agent",
      "subagent_type": "loom-missing-agent-type"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Agent type 'loom-missing-agent-type' not found"), std::string::npos);
    EXPECT_EQ(result->content.front().text.find("recursion depth"), std::string::npos);
}

TEST(Tools, AgentToolLoadsProjectAgentDefinitions) {
    auto root = fs::temp_directory_path() / "loom_agent_tool_project_test";
    auto previous_cwd = fs::current_path();
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "project-reviewer.md");
        agent << R"MD(---
name: project-reviewer
description: Reviews project changes
model: haiku
tools: [Read, Grep]
maxTurns: 2
---
Review the project change and report concrete risks.
)MD";
    }

    fs::current_path(root);
    loom::tools::AgentConfig config;
    config.max_depth = 0;
    loom::tools::AgentTool tool(config);

    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Review changes",
      "prompt": "Review this migration change",
      "subagent_type": "project-reviewer"
    })"));

    fs::current_path(previous_cwd);
    fs::remove_all(root);

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Agent recursion depth limit reached"), std::string::npos);
    EXPECT_EQ(result->content.front().text.find("not found"), std::string::npos);
}

TEST(Tools, AgentToolAppliesInitialPromptAndToolRestrictionsInExecutionPlan) {
    EnvironmentUnsetGuard auto_memory_guard("LOOM_DISABLE_AUTO_MEMORY");
    auto root = fs::temp_directory_path() / "loom_agent_plan_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "restricted.md");
        agent << R"MD(---
name: restricted-reviewer
description: Reviews with restricted tools
model: test-model
tools: [Read, Bash]
disallowedTools: [Bash]
permissionMode: acceptEdits
effort: high
memory: project
color: purple
omitLoomMd: true
criticalSystemReminder: Stay focused on migration risk.
initialPrompt: First inspect the diff.
maxTurns: 2
---
Review with a narrow tool set.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Review this change.";
        request.subagent_type = "restricted-reviewer";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        EXPECT_EQ(plan->agent_type, "restricted-reviewer");
        EXPECT_EQ(plan->model, "test-model");
        EXPECT_EQ(plan->max_turns, 2);
        ASSERT_TRUE(plan->mode.has_value());
        EXPECT_EQ(*plan->mode, "acceptEdits");
        ASSERT_TRUE(plan->effort.has_value());
        EXPECT_EQ(*plan->effort, "high");
        ASSERT_TRUE(plan->memory.has_value());
        EXPECT_EQ(*plan->memory, "project");
        ASSERT_TRUE(plan->color.has_value());
        EXPECT_EQ(*plan->color, "purple");
        EXPECT_TRUE(plan->omit_loom_md);
        ASSERT_TRUE(plan->critical_system_reminder.has_value());
        EXPECT_EQ(*plan->critical_system_reminder, "Stay focused on migration risk.");
        EXPECT_NE(plan->system_prompt.find("- effort: high"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("- memory: project"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("- color: purple"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("- omit_loom_md: true"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("<critical_system_reminder>"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("# Persistent Agent Memory"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find("project-scope"), std::string::npos);
        EXPECT_NE(plan->system_prompt.find((root / ".loom" / "agent-memory" / "restricted-reviewer").string()), std::string::npos);
        EXPECT_NE(plan->prompt.find("First inspect the diff.\n\nReview this change."), std::string::npos);
        EXPECT_TRUE(std::ranges::contains(plan->allowed_tools, "Read"));
        EXPECT_TRUE(std::ranges::contains(plan->allowed_tools, "Bash"));
        EXPECT_TRUE(std::ranges::contains(plan->allowed_tools, "Write"));
        EXPECT_TRUE(std::ranges::contains(plan->allowed_tools, "Edit"));
        EXPECT_TRUE(fs::exists(root / ".loom" / "agent-memory" / "restricted-reviewer"));
        ASSERT_EQ(plan->disallowed_tools.size(), 1u);
        EXPECT_EQ(plan->disallowed_tools.front(), "Bash");
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolPermissionModeHonorsParentPrecedenceInExecutionPlan) {
    auto root = fs::temp_directory_path() / "loom_agent_permission_mode_precedence_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "permission-planner.md");
        agent << R"MD(---
name: permission-planner
description: Plans with explicit agent permission mode
permissionMode: plan
---
Plan the assigned work.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Plan a migration change.";
        request.subagent_type = "permission-planner";

        auto default_plan = loom::tools::agent::build_agent_execution_plan(request, loom::tools::AgentConfig{});
        ASSERT_TRUE(default_plan.has_value()) << default_plan.error();
        ASSERT_TRUE(default_plan->mode.has_value());
        EXPECT_EQ(*default_plan->mode, "plan");
        EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
            "ExitPlanMode",
            false,
            false,
            std::optional<std::string_view>{std::string_view{*default_plan->mode}}));

        for (const auto parent_mode : std::array<std::string_view, 3>{
                 "acceptEdits",
                 "bypassPermissions",
                 "auto",
             }) {
            loom::tools::AgentConfig config;
            config.parent_permission_mode = std::string{parent_mode};

            auto protected_plan = loom::tools::agent::build_agent_execution_plan(request, config);
            ASSERT_TRUE(protected_plan.has_value()) << protected_plan.error();
            ASSERT_TRUE(protected_plan->mode.has_value());
            EXPECT_EQ(*protected_plan->mode, parent_mode);
            EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
                "ExitPlanMode",
                false,
                false,
                std::optional<std::string_view>{std::string_view{*protected_plan->mode}}));
        }
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolHonorsDisabledAutoMemoryForAgentDefinitions) {
    EnvironmentGuard disable_memory("LOOM_DISABLE_AUTO_MEMORY", "1");
    auto root = fs::temp_directory_path() / "loom_agent_memory_disabled_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "memory-disabled.md");
        agent << R"MD(---
name: memory-disabled
description: Agent with memory disabled by env
model: haiku
tools: [Grep]
memory: local
---
Do focused work.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Check memory env behavior.";
        request.subagent_type = "memory-disabled";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_TRUE(plan->memory.has_value());
        EXPECT_EQ(*plan->memory, "local");
        ASSERT_EQ(plan->allowed_tools.size(), 1u);
        EXPECT_EQ(plan->allowed_tools.front(), "Grep");
        EXPECT_EQ(plan->system_prompt.find("# Persistent Agent Memory"), std::string::npos);
        EXPECT_FALSE(fs::exists(root / ".loom" / "agent-memory-local" / "memory-disabled"));
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolAppliesAgentEffortToApiRequest) {
    EnvironmentUnsetGuard user_type_guard("USER_TYPE");

    {
        loom::services::api::CreateMessageRequest request;
        request.model = "test-model-20260601";
        loom::tools::agent::apply_agent_effort_to_request(
            request,
            std::optional<std::string>{" high "});

        ASSERT_TRUE(request.output_effort.has_value());
        EXPECT_EQ(*request.output_effort, "high");
        EXPECT_TRUE(std::ranges::contains(request.betas, "effort-2025-11-24"));
    }

    {
        loom::services::api::CreateMessageRequest request;
        request.model = "test-model-20260601";
        loom::tools::agent::apply_agent_effort_to_request(
            request,
            std::optional<std::string>{"max"});

        ASSERT_TRUE(request.output_effort.has_value());
        EXPECT_EQ(*request.output_effort, "max");
    }

    {
        loom::services::api::CreateMessageRequest request;
        request.model = "test-model";
        loom::tools::agent::apply_agent_effort_to_request(
            request,
            std::optional<std::string>{"high"});

        ASSERT_TRUE(request.output_effort.has_value());
        EXPECT_EQ(*request.output_effort, "high");
        EXPECT_TRUE(std::ranges::contains(request.betas, "effort-2025-11-24"));
    }

    {
        loom::services::api::CreateMessageRequest request;
        request.model = "test-model-20260601";
        loom::tools::agent::apply_agent_effort_to_request(
            request,
            std::optional<std::string>{"77"});

        EXPECT_FALSE(request.output_effort.has_value());
    }

    {
        loom::services::api::CreateMessageRequest request;
        request.model = "test-model-20260601";
        loom::tools::agent::apply_agent_effort_to_request(
            request,
            std::optional<std::string>{"77"});

        EXPECT_FALSE(request.output_effort.has_value());
    }
}

TEST(Tools, AgentToolPropagatesParentAgentIdIntoExecutionPlanAndRecord) {
    auto root = fs::temp_directory_path() / "loom_agent_parent_id_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "child-reviewer.md");
        agent << R"MD(---
name: child-reviewer
description: Reviews as a nested child agent
model: haiku
---
Review as a child agent.
)MD";
    }

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        config.parent_agent_id = "parent-agent-1";

        loom::tools::agent::AgentToolRequest request;
        request.description = "Nested child review";
        request.prompt = "Review nested context.";
        request.subagent_type = "child-reviewer";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_TRUE(plan->parent_agent_id.has_value());
        EXPECT_EQ(*plan->parent_agent_id, "parent-agent-1");
        EXPECT_NE(plan->system_prompt.find("- parent_agent_id: parent-agent-1"), std::string::npos);

        loom::tools::agent::upsert_agent_record_for_plan(*plan);
        auto record = loom::tools::agent_runtime::native_agent_store().get(plan->agent_id);
        ASSERT_TRUE(record.has_value());
        ASSERT_TRUE(record->parent_agent_id.has_value());
        EXPECT_EQ(*record->parent_agent_id, "parent-agent-1");
        ASSERT_TRUE(record->description.has_value());
        EXPECT_EQ(*record->description, "Nested child review");
        EXPECT_EQ(record->agent_type, "child-reviewer");

        loom::tools::agent_runtime::native_agent_store().clear_for_testing();
        auto restored = loom::tools::agent_runtime::native_agent_store().get(plan->agent_id);
        ASSERT_TRUE(restored.has_value());
        ASSERT_TRUE(restored->description.has_value());
        EXPECT_EQ(*restored->description, "Nested child review");

        loom::tools::AgentTool tool(config);
        auto child_config = tool.child_config(plan->agent_id);
        ASSERT_TRUE(child_config.parent_agent_id.has_value());
        EXPECT_EQ(*child_config.parent_agent_id, plan->agent_id);
    }

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolMarksForkChildContextAndRejectsImplicitNestedFork) {
    auto root = fs::temp_directory_path() / "loom_agent_live_fork_guard_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto parsed = loom::tools::agent::parse_agent_tool_request(loom::core::ToolInput::from_json(R"({
      "description": "Fork worker",
      "prompt": "Continue the forked work",
      "subagent_type": "general-purpose",
      "querySource": "agent:builtin:fork"
    })"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    auto plan = loom::tools::agent::build_agent_execution_plan(*parsed, loom::tools::AgentConfig{});
    ASSERT_TRUE(plan.has_value()) << plan.error();
    EXPECT_TRUE(plan->fork_child_context);
    EXPECT_TRUE(loom::tools::agent::should_reject_fork_child_agent_call(
        *plan,
        "Agent",
        R"({"description":"nested fork","prompt":"Split this work again"})"));
    EXPECT_FALSE(loom::tools::agent::should_reject_fork_child_agent_call(
        *plan,
        "Agent",
        R"({"description":"explicit child","prompt":"Run explicit child","subagent_type":"general-purpose"})"));
    EXPECT_FALSE(loom::tools::agent::should_reject_fork_child_agent_call(
        *plan,
        "Read",
        R"({"file_path":"README.md"})"));

    auto boilerplate = loom::tools::agent::parse_agent_tool_request(loom::core::ToolInput::from_json(R"({
      "description": "Fork worker from transcript",
      "prompt": "<fork-boilerplate>\nSTOP. READ THIS FIRST.\n</fork-boilerplate>",
      "subagent_type": "general-purpose"
    })"));
    ASSERT_TRUE(boilerplate.has_value()) << boilerplate.error();
    auto boilerplate_plan = loom::tools::agent::build_agent_execution_plan(*boilerplate, loom::tools::AgentConfig{});
    ASSERT_TRUE(boilerplate_plan.has_value()) << boilerplate_plan.error();
    EXPECT_TRUE(boilerplate_plan->fork_child_context);

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "persisted-fork-worker",
        .agent_type = "general-purpose",
        .cwd = root.string(),
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .capabilities = {"fork-subagent", "Read"},
        .transcript = {"system: forked from parent-agent"},
    });
    loom::tools::agent::AgentToolRequest resumed;
    resumed.agent_id_override = "persisted-fork-worker";
    resumed.resume_existing = true;
    resumed.prompt = "Resume persisted fork worker.";
    resumed.subagent_type = "general-purpose";
    auto resumed_plan = loom::tools::agent::build_agent_execution_plan(resumed, loom::tools::AgentConfig{});
    ASSERT_TRUE(resumed_plan.has_value()) << resumed_plan.error();
    EXPECT_TRUE(resumed_plan->fork_child_context);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolAcceptsForkParentPromptContextAndExactTools) {
    auto root = fs::temp_directory_path() / "loom_agent_fork_context_plan_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto parsed = loom::tools::agent::parse_agent_tool_request(loom::core::ToolInput::from_json(R"({
      "description": "Fork with inherited context",
      "prompt": "Your directive: inspect parser parity",
      "subagent_type": "general-purpose",
      "parent_system_prompt": "parent rendered system prompt bytes",
      "useExactTools": true,
      "exactTools": ["Read", "Agent"],
      "forkContextMessages": [
        {
          "type": "assistant",
          "message": {
            "role": "assistant",
            "content": [
              {"type":"text","text":"parent answer"},
              {"type":"tool_use","id":"read-1","name":"Read","input":{"file_path":"README.md"}}
            ]
          }
        },
        {
          "role": "user",
          "content": [
            {"type":"tool_result","tool_use_id":"read-1","content":[{"type":"text","text":"README content"}]}
          ]
        }
      ]
    })"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error();

    auto plan = loom::tools::agent::build_agent_execution_plan(*parsed, loom::tools::AgentConfig{});
    ASSERT_TRUE(plan.has_value()) << plan.error();
    EXPECT_TRUE(plan->system_prompt_overridden);
    EXPECT_EQ(plan->system_prompt, "parent rendered system prompt bytes");
    EXPECT_EQ(plan->system_prompt.find("- agent_id:"), std::string::npos);
    EXPECT_TRUE(plan->use_exact_tools);
    ASSERT_EQ(plan->exact_tools.size(), 2u);
    EXPECT_TRUE(loom::tools::agent::exact_tools_allow_tool(*plan, "Read"));
    EXPECT_TRUE(loom::tools::agent::exact_tools_allow_tool(*plan, "Agent"));
    EXPECT_FALSE(loom::tools::agent::exact_tools_allow_tool(*plan, "Write"));

    ASSERT_EQ(plan->fork_context_messages.size(), 2u);
    EXPECT_EQ(plan->fork_context_messages[0].role, "assistant");
    ASSERT_EQ(plan->fork_context_messages[0].content.size(), 2u);
    EXPECT_EQ(plan->fork_context_messages[0].content[0].text, "parent answer");
    EXPECT_EQ(plan->fork_context_messages[0].content[1].type, loom::services::api::ContentBlockType::ToolUse);
    EXPECT_EQ(plan->fork_context_messages[0].content[1].tool_use_id, "read-1");
    EXPECT_EQ(plan->fork_context_messages[0].content[1].tool_name, "Read");
    EXPECT_NE(plan->fork_context_messages[0].content[1].tool_input_json.find("README.md"), std::string::npos);
    EXPECT_EQ(plan->fork_context_messages[1].role, "user");
    ASSERT_EQ(plan->fork_context_messages[1].content.size(), 1u);
    EXPECT_EQ(plan->fork_context_messages[1].content[0].type, loom::services::api::ContentBlockType::ToolResult);
    EXPECT_EQ(plan->fork_context_messages[1].content[0].tool_use_id, "read-1");
    EXPECT_EQ(plan->fork_context_messages[1].content[0].text, "README content");

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolBuildsTsForkContextFromParentAssistantMessage) {
    auto root = fs::temp_directory_path() / "loom_agent_live_parent_fork_context_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto parsed = loom::tools::agent::parse_agent_tool_request(loom::core::ToolInput::from_json(R"({
      "description": "Implicit fork with live parent message",
      "prompt": "Audit parser migration parity",
      "parentSystemPrompt": "rendered parent prompt bytes",
      "exactTools": ["Read", "Bash", "Agent"],
      "parentAssistantMessage": {
        "type": "assistant",
        "message": {
          "role": "assistant",
          "content": [
            {"type":"thinking","thinking":"parent private reasoning","signature":"sig-1"},
            {"type":"text","text":"I will split this into a fork."},
            {"type":"tool_use","id":"agent-1","name":"Agent","input":{"description":"fork","prompt":"Audit parser migration parity"}},
            {"type":"tool_use","id":"read-1","name":"Read","input":{"file_path":"README.md"}}
          ]
        }
      }
    })"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error();

    auto plan = loom::tools::agent::build_agent_execution_plan(*parsed, loom::tools::AgentConfig{});
    ASSERT_TRUE(plan.has_value()) << plan.error();
    EXPECT_TRUE(plan->fork_child_context);
    EXPECT_TRUE(plan->fork_context_includes_prompt);
    EXPECT_TRUE(plan->system_prompt_overridden);
    EXPECT_EQ(plan->system_prompt, "rendered parent prompt bytes");
    EXPECT_TRUE(plan->use_exact_tools);
    EXPECT_TRUE(loom::tools::agent::exact_tools_allow_tool(*plan, "Agent"));
    EXPECT_FALSE(loom::tools::agent::exact_tools_allow_tool(*plan, "Write"));

    ASSERT_EQ(plan->fork_context_messages.size(), 2u);
    EXPECT_EQ(plan->fork_context_messages[0].role, "assistant");
    ASSERT_EQ(plan->fork_context_messages[0].content.size(), 4u);
    EXPECT_EQ(plan->fork_context_messages[0].content[0].type, loom::services::api::ContentBlockType::Thinking);
    EXPECT_EQ(plan->fork_context_messages[0].content[2].type, loom::services::api::ContentBlockType::ToolUse);
    EXPECT_EQ(plan->fork_context_messages[0].content[2].tool_use_id, "agent-1");
    EXPECT_EQ(plan->fork_context_messages[0].content[3].tool_use_id, "read-1");

    EXPECT_EQ(plan->fork_context_messages[1].role, "user");
    ASSERT_EQ(plan->fork_context_messages[1].content.size(), 3u);
    const std::string placeholder = "Fork started \u2014 processing in background";
    EXPECT_EQ(plan->fork_context_messages[1].content[0].type, loom::services::api::ContentBlockType::ToolResult);
    EXPECT_EQ(plan->fork_context_messages[1].content[0].tool_use_id, "agent-1");
    EXPECT_EQ(plan->fork_context_messages[1].content[0].text, placeholder);
    EXPECT_EQ(plan->fork_context_messages[1].content[1].tool_use_id, "read-1");
    EXPECT_EQ(plan->fork_context_messages[1].content[1].text, placeholder);
    EXPECT_EQ(plan->fork_context_messages[1].content[2].type, loom::services::api::ContentBlockType::Text);
    EXPECT_NE(plan->fork_context_messages[1].content[2].text.find("<fork-boilerplate>"), std::string::npos);
    EXPECT_NE(plan->fork_context_messages[1].content[2].text.find("Your response MUST begin with \"Scope:\""), std::string::npos);
    EXPECT_NE(plan->fork_context_messages[1].content[2].text.find("Your directive: Audit parser migration parity"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolInjectsImplicitForkInputsAtAgentCallSite) {
    auto root = fs::temp_directory_path() / "loom_agent_implicit_fork_injection_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent::AgentExecutionPlan parent_plan;
    parent_plan.agent_id = "parent-agent";
    parent_plan.agent_type = "general-purpose";
    parent_plan.prompt = "Parent task";
    parent_plan.model = "test-model";
    parent_plan.system_prompt = "parent rendered system prompt bytes";

    loom::services::api::Message parent_assistant;
    parent_assistant.role = "assistant";
    parent_assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::Text,
        .text = "Spawning an implicit fork",
    });
    parent_assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "agent-tool-1",
        .tool_name = "Agent",
        .tool_input_json = R"({"description":"fork","prompt":"Audit migration"})",
    });

    std::vector<loom::services::api::ToolDefinition> parent_tools{
        {.name = "Read", .description = "read", .input_schema_json = "{}"},
        {.name = "Agent", .description = "agent", .input_schema_json = "{}"},
        {.name = "Bash", .description = "bash", .input_schema_json = "{}"},
    };

    auto injected = loom::tools::agent::build_implicit_fork_agent_input_json(
        R"({"description":"fork","prompt":"Audit migration","run_in_background":false})",
        parent_plan,
        parent_assistant,
        parent_tools);
    auto parsed_json = loom::utils::json::parse(injected);
    ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().format();
    auto root_json = parsed_json->root();
    EXPECT_TRUE(root_json.get("run_in_background").is_bool());
    EXPECT_TRUE(root_json.get("run_in_background").as_bool());
    EXPECT_EQ(root_json.get("querySource").as_str(), "agent:builtin:fork");
    EXPECT_EQ(root_json.get("parentSystemPrompt").as_str(), "parent rendered system prompt bytes");
    ASSERT_TRUE(root_json.get("exactTools").is_arr());
    EXPECT_EQ(root_json.get("exactTools").size(), 3u);
    ASSERT_TRUE(root_json.get("parentAssistantMessage").is_obj());

    auto parsed_request = loom::tools::agent::parse_agent_tool_request(loom::core::ToolInput::from_json(injected));
    ASSERT_TRUE(parsed_request.has_value()) << parsed_request.error();
    auto child_plan = loom::tools::agent::build_agent_execution_plan(*parsed_request, loom::tools::AgentConfig{});
    ASSERT_TRUE(child_plan.has_value()) << child_plan.error();
    EXPECT_TRUE(child_plan->background);
    EXPECT_TRUE(child_plan->fork_child_context);
    EXPECT_TRUE(child_plan->fork_context_includes_prompt);
    EXPECT_TRUE(child_plan->system_prompt_overridden);
    EXPECT_EQ(child_plan->system_prompt, "parent rendered system prompt bytes");
    ASSERT_EQ(child_plan->fork_context_messages.size(), 2u);
    ASSERT_EQ(child_plan->fork_context_messages[1].content.size(), 2u);
    EXPECT_EQ(child_plan->fork_context_messages[1].content[0].type, loom::services::api::ContentBlockType::ToolResult);
    EXPECT_EQ(child_plan->fork_context_messages[1].content[0].tool_use_id, "agent-tool-1");
    EXPECT_NE(child_plan->fork_context_messages[1].content[1].text.find("Your directive: Audit migration"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPermissionRulesMatchToolNamesFromParameterizedSpecs) {
    EXPECT_TRUE(loom::tools::agent::tool_name_allowed_by_definition(
        "Bash",
        {"Read", "Bash(git status)"}));
    EXPECT_TRUE(loom::tools::agent::tool_name_allowed_by_definition(
        "Agent",
        {"Agent(reviewer,planner)"}));
    EXPECT_FALSE(loom::tools::agent::tool_name_allowed_by_definition(
        "Write",
        {"Read", "Bash(git status)"}));

    EXPECT_TRUE(loom::tools::agent::tool_name_disallowed_by_definition(
        "Bash",
        {"Bash(rm -rf /tmp/example)"}));
    EXPECT_TRUE(loom::tools::agent::tool_name_disallowed_by_definition(
        "Agent",
        {"Agent(project-reviewer)"}));
    EXPECT_FALSE(loom::tools::agent::tool_name_disallowed_by_definition(
        "Read",
        {"Bash(git status)"}));

    EXPECT_TRUE(loom::tools::agent::agent_type_allowed_by_permission_rules(
        "project-reviewer",
        {"Agent(project-reviewer,planner)"},
        {}));
    EXPECT_FALSE(loom::tools::agent::agent_type_allowed_by_permission_rules(
        "general-purpose",
        {"Agent(project-reviewer,planner)"},
        {}));
    EXPECT_FALSE(loom::tools::agent::agent_type_allowed_by_permission_rules(
        "project-reviewer",
        {"Agent"},
        {"Agent(project-reviewer)"}));
}

TEST(Tools, AgentToolBaseFilteringMatchesTypeScriptToolSets) {
    EnvironmentUnsetGuard nested_guard("LOOM_ENABLE_NESTED_AGENTS");

    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "task_output",
        true,
        false));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "TaskOutput",
        true,
        false));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "Agent",
        true,
        false));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "enter_plan_mode",
        true,
        false));

    EXPECT_TRUE(loom::tools::agent::all_agent_disallows_tool("exit_plan_mode"));
    EXPECT_TRUE(loom::tools::agent::custom_agent_disallows_tool("exit_plan_mode"));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "exit_plan_mode",
        true,
        false));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "exit_plan_mode",
        false,
        false));
    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "ExitPlanMode",
        true,
        false,
        std::optional<std::string_view>{"plan"}));

    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "mcp__linear__list_issues",
        false,
        true));
    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "todo_write",
        false,
        true));
    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "TodoWrite",
        false,
        true));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "task_list",
        false,
        true));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "send_message",
        false,
        true));

    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "task_list",
        false,
        true,
        std::nullopt,
        true));
    EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
        "send_message",
        false,
        true,
        std::nullopt,
        true));
    EXPECT_FALSE(loom::tools::agent::agent_base_filter_allows_tool(
        "Agent",
        false,
        true,
        std::nullopt,
        true));
    {
        EnvironmentGuard nested_agents("LOOM_ENABLE_NESTED_AGENTS", "1");
        EXPECT_TRUE(loom::tools::agent::agent_base_filter_allows_tool(
            "Agent",
            false,
            true,
            std::nullopt,
            true));
    }
}

TEST(Tools, AgentToolAgentTypePermissionRulesDoNotConstrainWorkerTools) {
    loom::tools::AgentConfig agent_only_config;
    agent_only_config.allowed_tools = {"Agent(restricted-reviewer)"};
    loom::tools::AgentTool agent_only_tool(agent_only_config);

    EXPECT_TRUE(agent_only_tool.is_tool_allowed("Agent"));
    EXPECT_TRUE(agent_only_tool.is_tool_allowed("Read"));
    EXPECT_TRUE(agent_only_tool.is_tool_allowed("Bash"));

    loom::tools::AgentConfig mixed_config;
    mixed_config.allowed_tools = {"Agent(restricted-reviewer)", "Read"};
    loom::tools::AgentTool mixed_tool(mixed_config);

    EXPECT_TRUE(mixed_tool.is_tool_allowed("Agent"));
    EXPECT_TRUE(mixed_tool.is_tool_allowed("Read"));
    EXPECT_FALSE(mixed_tool.is_tool_allowed("Bash"));
}

TEST(Tools, AgentToolRestrictsAgentTypesFromParameterizedPermissionSpecs) {
    auto root = fs::temp_directory_path() / "loom_agent_type_permission_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "restricted-reviewer.md");
        agent << R"MD(---
name: restricted-reviewer
description: Reviews only when explicitly allowed
---
Review the task.
)MD";
    }

    CurrentPathGuard cwd(root);
    loom::tools::AgentConfig config;
    config.max_depth = 0;
    config.allowed_tools = {"Agent(restricted-reviewer)"};
    loom::tools::AgentTool tool(config);

    auto denied = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run general",
      "prompt": "Use a general agent",
      "subagent_type": "general-purpose"
    })"));
    ASSERT_TRUE(denied.has_value());
    ASSERT_TRUE(denied->is_error);
    ASSERT_FALSE(denied->content.empty());
    EXPECT_NE(denied->content.front().text.find("not allowed by current Agent tool permission rules"), std::string::npos);
    EXPECT_EQ(denied->content.front().text.find("recursion depth"), std::string::npos);

    auto allowed = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run reviewer",
      "prompt": "Use the reviewer",
      "subagent_type": "restricted-reviewer"
    })"));
    ASSERT_TRUE(allowed.has_value());
    ASSERT_TRUE(allowed->is_error);
    ASSERT_FALSE(allowed->content.empty());
    EXPECT_NE(allowed->content.front().text.find("Agent recursion depth limit reached"), std::string::npos);

    fs::remove_all(root);
}

TEST(Tools, AgentToolPreloadsSkillsFromDefinition) {
    auto root = fs::temp_directory_path() / "loom_agent_skills_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    fs::create_directories(root / ".loom" / "skills" / "review-skill");
    {
        std::ofstream agent(root / ".loom" / "agents" / "skillful.md");
        agent << R"MD(---
name: skillful-reviewer
description: Reviews with preloaded skills
skills: [review-skill, missing-skill]
---
Review with a preloaded workflow.
)MD";
    }
    {
        std::ofstream skill(root / ".loom" / "skills" / "review-skill" / "SKILL.md");
        skill << R"MD(---
description: Review skill
---
Inspect the patch before reporting findings.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Review this change.";
        request.subagent_type = "skillful-reviewer";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_EQ(plan->preloaded_skill_messages.size(), 1u);
        EXPECT_NE(plan->preloaded_skill_messages.front().find("review-skill"), std::string::npos);
        EXPECT_NE(plan->preloaded_skill_messages.front().find("Inspect the patch"), std::string::npos);
        EXPECT_EQ(plan->preloaded_skill_messages.front().find("missing-skill"), std::string::npos);
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolLoadsPluginAgentsAndPluginSkills) {
    // RFC-0001 B12: the 'plugin-fixture:review-skill' skill execution below
    // goes through the orchestration-installed SkillLoader executor (plugin
    // component discovery runs per call against this test's cwd).
    FileToolServicesGuard services_guard;
    auto root = fs::temp_directory_path() / "loom_plugin_agent_skills_test";
    fs::remove_all(root);
    const auto plugin_root = root / ".loom" / "plugins" / "plugin-fixture";
    fs::create_directories(plugin_root / "agents");
    fs::create_directories(plugin_root / "skills" / "review-skill");
    const auto server_path = plugin_root / "server.js";
    {
        std::ofstream entry(plugin_root / "plugin.js");
        entry << "process.exit(0)\n";
    }
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: process.env.SERVER_NAME || 'plugin-agent-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{
          name: process.env.TOOL_NAME || 'lookup',
          description: ['plugin tool', process.env.SERVER_NAME].filter(Boolean).join(':'),
          inputSchema: { type: 'object' }
        }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << R"JSON({
  "name": "plugin-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "description": "Plugin fixture"
})JSON";
    }
    {
        std::ofstream agent(plugin_root / "agents" / "reviewer.md");
        agent << std::format(R"MD(---
name: reviewer
description: Reviews using plugin resources
skills: [review-skill]
requiredMcpServers: [review-context]
mcpServers:
  - review-context
  - inline-review:
      type: stdio
      command: node
      args:
        - "{}"
      env:
        SERVER_NAME: plugin-fixture:inline-review
        TOOL_NAME: inline_lookup
hooks:
  SubagentStart:
    - command: "echo plugin-hook-started"
---
Review with plugin context.
)MD", server_path.string());
    }
    {
        std::ofstream skill(plugin_root / "skills" / "review-skill" / "SKILL.md");
        skill << R"MD(---
description: Plugin review skill
---
Use the plugin review checklist.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        auto agents = loom::tools::agent_runtime::get_all_agent_definitions();
        auto it = std::ranges::find_if(agents, [](const auto& agent) {
            return agent.agent_type == "plugin-fixture:reviewer";
        });
        ASSERT_NE(it, agents.end());
        EXPECT_EQ(it->source, "plugin");
        EXPECT_TRUE(it->hooks_present);
        EXPECT_TRUE(it->hooks.contains("SubagentStart"));
        ASSERT_EQ(it->required_mcp_servers.size(), 1u);
        EXPECT_EQ(it->required_mcp_servers.front(), "plugin:plugin-fixture:review-context");
        ASSERT_EQ(it->mcp_servers.size(), 1u);
        EXPECT_EQ(it->mcp_servers.front(), "plugin:plugin-fixture:review-context");
        ASSERT_EQ(it->inline_mcp_servers.size(), 1u);
        EXPECT_EQ(it->inline_mcp_servers.front().name, "plugin:plugin-fixture:inline-review");

        auto synced = loom::tools::sync_native_mcp_servers({
            loom::tools::NativeMcpConfiguredServer{
                .name = "plugin:plugin-fixture:review-context",
                .command = "node",
                .args = {server_path.string()},
                .env = {{"SERVER_NAME", "plugin:plugin-fixture:review-context"}, {"TOOL_NAME", "review_lookup"}},
            },
        });
        ASSERT_TRUE(synced.has_value());
        auto restarted = loom::tools::restart_native_mcp_server("plugin:plugin-fixture:review-context");
        ASSERT_TRUE(restarted.has_value()) << restarted.error();
        ASSERT_EQ(restarted->status, "ready");

        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Review this change.";
        request.subagent_type = "plugin-fixture:reviewer";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_EQ(plan->preloaded_skill_messages.size(), 1u);
        EXPECT_NE(plan->preloaded_skill_messages.front().find("plugin-fixture:review-skill"), std::string::npos);
        EXPECT_NE(plan->preloaded_skill_messages.front().find("Use the plugin review checklist"), std::string::npos);
        ASSERT_EQ(plan->agent_mcp_servers.size(), 2u);
        EXPECT_EQ(plan->agent_mcp_servers[0], "plugin:plugin-fixture:review-context");
        EXPECT_EQ(plan->agent_mcp_servers[1], "plugin:plugin-fixture:inline-review");
        ASSERT_EQ(plan->agent_mcp_tools.size(), 2u);
        EXPECT_EQ(plan->agent_mcp_tools[0].server_name, "plugin:plugin-fixture:review-context");
        EXPECT_EQ(plan->agent_mcp_tools[0].tool_name, "review_lookup");
        EXPECT_EQ(plan->agent_mcp_tools[1].server_name, "plugin:plugin-fixture:inline-review");
        EXPECT_EQ(plan->agent_mcp_tools[1].tool_name, "inline_lookup");
        ASSERT_TRUE(plan->agent_mcp_context_message.has_value());
        EXPECT_NE(plan->agent_mcp_context_message->find("plugin:plugin-fixture:review-context/review_lookup"), std::string::npos);
        EXPECT_NE(plan->agent_mcp_context_message->find("plugin:plugin-fixture:inline-review/inline_lookup"), std::string::npos);
        EXPECT_TRUE(plan->frontmatter_hooks.contains("SubagentStart"));

        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        auto skill = registry.execute("skill", loom::core::ToolInput::from_json(R"({
          "name": "plugin-fixture:review-skill"
        })"));
        ASSERT_TRUE(skill.has_value());
        ASSERT_FALSE(skill->is_error);
        ASSERT_FALSE(skill->content.empty());
        EXPECT_NE(skill->content.front().text.find("Use the plugin review checklist"), std::string::npos);
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, AgentToolAcceptsBackgroundAndIsolationDefinitionFeatures) {
    auto root = fs::temp_directory_path() / "loom_agent_background_definition_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "async.md");
        agent << R"MD(---
name: async-reviewer
description: Reviews in background native modes
background: true
isolation: worktree
skills: [review]
---
Review asynchronously.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        config.max_depth = 0;
        loom::tools::AgentTool tool(config);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Async review",
          "prompt": "Review this change",
          "subagent_type": "async-reviewer"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("Agent recursion depth limit reached"), std::string::npos);
        EXPECT_EQ(result->content.front().text.find("features not yet supported"), std::string::npos);
        EXPECT_EQ(result->content.front().text.find("isolation"), std::string::npos);
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolExecutesDefinitionHooksForBackgroundAgents) {
    auto root = fs::temp_directory_path() / "loom_agent_hooks_definition_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto marker = root / "hook-marker.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "hooked.md");
        agent << R"MD(---
name: hooked-reviewer
description: Reviews with hooks
hooks:
  SubagentStart:
    - command: "printf start-$LOOM_HOOK_AGENT_ID > )MD" << marker.string() << R"MD(; echo hook-started"
---
Review with hooks.
)MD";
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentTool tool;

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Hooked review",
          "prompt": "Review this change",
          "subagent_type": "hooked-reviewer",
          "run_in_background": true,
          "name": "hooked-agent"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("Queued background agent hooked-agent"), std::string::npos);
        ASSERT_TRUE(fs::exists(marker));
        std::ifstream marker_in(marker);
        std::string marker_text;
        std::getline(marker_in, marker_text);
        EXPECT_EQ(marker_text, "start-hooked-agent");

        auto record = loom::tools::agent_runtime::native_agent_store().get("hooked-agent");
        ASSERT_TRUE(record.has_value());
        ASSERT_GE(record->transcript.size(), 1u);
        EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
            return entry.find("hook-started") != std::string::npos;
        }));
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolExecutesSubagentStopHookWhenBackgroundAgentIsCancelled) {
    auto root = fs::temp_directory_path() / "loom_agent_stop_hook_cancel_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto marker = root / "stop-hook-marker.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "hooked-stop.md");
        agent << R"MD(---
name: hooked-stop-reviewer
description: Reviews with stop hooks
hooks:
  SubagentStop:
    - command: "printf stop-$LOOM_HOOK_AGENT_ID > )MD" << shell_quote_for_test(marker.string()) << R"MD(; echo hook-stopped"
---
Review with stop hooks.
)MD";
    }

    LocalSlowMessagesStreamServer server(std::chrono::milliseconds(750));
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "stop-hook-cancel-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto started = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Run async and cancel with stop hook",
          "prompt": "Wait for cancellation and run stop hook",
          "subagent_type": "hooked-stop-reviewer",
          "run_in_background": true,
          "name": "hooked-stop-agent"
        })"));
        ASSERT_TRUE(started.has_value()) << started.error().format();
        ASSERT_FALSE(started->is_error);
        ASSERT_TRUE(server.wait_for_request());

        auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
          "task_id": "hooked-stop-agent"
        })"));
        ASSERT_TRUE(stopped.has_value());
        ASSERT_FALSE(stopped->is_error);

        bool observed_marker = false;
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (fs::exists(marker)) {
                observed_marker = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ASSERT_TRUE(observed_marker);
        std::ifstream marker_in(marker);
        std::string marker_text;
        std::getline(marker_in, marker_text);
        EXPECT_EQ(marker_text, "stop-hooked-stop-agent");

        ASSERT_TRUE(wait_for_native_agent_status(
            "hooked-stop-agent",
            loom::tools::agent_runtime::NativeAgentStatus::Cancelled,
            std::chrono::seconds(3)));
        auto record = loom::tools::agent_runtime::native_agent_store().get("hooked-stop-agent");
        ASSERT_TRUE(record.has_value());
        EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Cancelled);
        ASSERT_TRUE(record->error.has_value());
        EXPECT_NE(record->error->find("while waiting for model stream"), std::string::npos);
        EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
            return entry.find("hook SubagentStop: hook-stopped") != std::string::npos;
        }));
    }

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolRunsFrontmatterToolHooksAroundNativeToolUse) {
    auto root = fs::temp_directory_path() / "loom_agent_tool_hooks_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto pre_marker = root / "pre-hook-marker.txt";
    auto post_marker = root / "post-hook-marker.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "tool-hooks.md");
        agent << R"MD(---
name: tool-hook-reviewer
description: Reviews with tool hooks
hooks:
  PreToolUse:
    Bash:
      - command: "printf pre-$LOOM_HOOK_TOOL_NAME-$LOOM_HOOK_TOOL_USE_ID > )MD" << shell_quote_for_test(pre_marker.string()) << R"MD(; echo pre-ran"
  PostToolUse:
    Bash:
      - command: "printf post-$LOOM_HOOK_TOOL_NAME-$LOOM_HOOK_TOOL_USE_ID-$LOOM_HOOK_TOOL_OUTPUT_PREVIEW > )MD" << shell_quote_for_test(post_marker.string()) << R"MD(; printf '{\"hookSpecificOutput\":{\"hookEventName\":\"PostToolUse\",\"additionalContext\":\"post context visible\"}}'"
---
Review with tool hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server("printf tool-output", "tool hook complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "tool-hook-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Run tool hooks",
          "prompt": "Call Bash and finish",
          "subagent_type": "tool-hook-reviewer",
          "name": "tool-hook-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
    }

    ASSERT_TRUE(fs::exists(pre_marker));
    EXPECT_EQ(read_file(pre_marker), "pre-Bash-toolu_bash_fixture");
    ASSERT_TRUE(fs::exists(post_marker));
    EXPECT_EQ(read_file(post_marker), "post-Bash-toolu_bash_fixture-tool-output");
    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("post context visible"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("tool-hook-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PreToolUse:Bash: pre-ran") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PostToolUse:Bash:") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPreToolFrontmatterHookCanDenyNativeToolUse) {
    auto root = fs::temp_directory_path() / "loom_agent_pre_tool_deny_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto executed_marker = root / "bash-executed.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "deny-tool.md");
        agent << R"MD(---
name: deny-tool-reviewer
description: Denies Bash through a pre hook
hooks:
  PreToolUse:
    Bash:
      - command: "printf '{\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"permissionDecision\":\"deny\",\"permissionDecisionReason\":\"blocked by pre hook\"}}'"
---
Review with deny hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server(
        "printf executed > " + shell_quote_for_test(executed_marker.string()),
        "pre hook deny complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "pre-tool-deny-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Deny Bash",
          "prompt": "Call Bash and finish",
          "subagent_type": "deny-tool-reviewer",
          "name": "pre-deny-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
    }

    EXPECT_FALSE(fs::exists(executed_marker));
    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("Tool execution denied by PreToolUse hook"), std::string::npos);
    EXPECT_NE(second_body->find("blocked by pre hook"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("pre-deny-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PreToolUse:Bash:") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPreToolFrontmatterHookCanUpdateNativeToolInput) {
    auto root = fs::temp_directory_path() / "loom_agent_pre_tool_update_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto marker = root / "bash-updated-input.txt";
    auto hook_json = root / "updated-input-hook.json";
    {
        std::ofstream hook(hook_json);
        hook << R"({"hookSpecificOutput":{"hookEventName":"PreToolUse","updatedInput":{"command":")"
            << loom::tools::agent::json_escape_string("printf rewritten > " + shell_quote_for_test(marker.string()))
            << R"(","description":"rewritten by hook"},"additionalContext":"updated input context"}})";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "update-tool.md");
        agent << R"MD(---
name: update-tool-reviewer
description: Updates Bash input through a pre hook
hooks:
  PreToolUse:
    Bash:
      - command: "cat )MD" << shell_quote_for_test(hook_json.string()) << R"MD("
---
Review with update hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server(
        "printf original > " + shell_quote_for_test(marker.string()),
        "pre hook update complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "pre-tool-update-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Update Bash input",
          "prompt": "Call Bash and finish",
          "subagent_type": "update-tool-reviewer",
          "name": "pre-update-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
    }

    ASSERT_TRUE(fs::exists(marker));
    EXPECT_EQ(read_file(marker), "rewritten");
    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("updated input context"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("pre-update-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PreToolUse:Bash updated input:") != std::string::npos &&
            entry.find("rewritten by hook") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolLivePermissionHookDeniesChildReadWriteEditAndBash) {
    auto root = fs::temp_directory_path() / "loom_agent_live_permission_deny_test";
    fs::remove_all(root);
    fs::create_directories(root);

    auto read_path = root / "read-denied-source.txt";
    {
        std::ofstream out(read_path);
        out << "read source";
    }
    auto write_path = root / "write-denied.txt";
    auto edit_path = root / "edit-denied.txt";
    {
        std::ofstream out(edit_path);
        out << "before edit";
    }
    auto bash_marker = root / "bash-denied.txt";

    struct PermissionCase {
        std::string tool_name;
        std::string input_json;
        std::string tool_use_id;
        std::string agent_name;
    };

    const auto cases = std::vector<PermissionCase>{
        PermissionCase{
            .tool_name = "Read",
            .input_json = std::format(
                R"({{"file_path":"{}"}})",
                loom::tools::agent::json_escape_string(read_path.string())),
            .tool_use_id = "toolu_live_deny_read",
            .agent_name = "live-deny-read-agent",
        },
        PermissionCase{
            .tool_name = "Write",
            .input_json = std::format(
                R"({{"file_path":"{}","content":"should not write"}})",
                loom::tools::agent::json_escape_string(write_path.string())),
            .tool_use_id = "toolu_live_deny_write",
            .agent_name = "live-deny-write-agent",
        },
        PermissionCase{
            .tool_name = "Edit",
            .input_json = std::format(
                R"({{"file_path":"{}","old_string":"before edit","new_string":"after edit"}})",
                loom::tools::agent::json_escape_string(edit_path.string())),
            .tool_use_id = "toolu_live_deny_edit",
            .agent_name = "live-deny-edit-agent",
        },
        PermissionCase{
            .tool_name = "Bash",
            .input_json = std::format(
                R"({{"command":"{}","description":"write denied marker"}})",
                loom::tools::agent::json_escape_string(
                    "printf denied > " + shell_quote_for_test(bash_marker.string()))),
            .tool_use_id = "toolu_live_deny_bash",
            .agent_name = "live-deny-bash-agent",
        },
    };

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard model_guard("LOOM_MODEL", "live-permission-deny-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.tool_name);
        LocalScriptedToolUseMessagesServer server(
            tc.tool_name,
            tc.input_json,
            tc.tool_use_id,
            "permission deny complete");
        ASSERT_TRUE(server.valid());
        EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());

        std::vector<loom::hooks::PermissionContext> calls;
        loom::hooks::ToolPermissionHook permission_hook;
        permission_hook.set_auto_approve(false);
        permission_hook.set_working_dir(root.string());
        permission_hook.set_ask_user_response_fn(
            [&calls, tool_name = tc.tool_name](const loom::hooks::PermissionContext& ctx) {
                calls.push_back(ctx);
                loom::hooks::PermissionResponse response;
                response.decision = loom::hooks::PermissionDecision::deny;
                response.message = "live deny " + tool_name;
                return response;
            });

        {
            CurrentPathGuard cwd(root);
            loom::core::ToolRegistry registry;
            loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
                .permission_check = [&permission_hook](
                    std::string_view tool_name,
                    std::string_view input_json,
                    std::string_view tool_use_id
                ) {
                    return check_agent_tool_permission_from_hook(
                        permission_hook,
                        tool_name,
                        input_json,
                        tool_use_id);
                },
                .permission_hook_valid_for_background = false,
            });
            auto result = registry.execute("Agent", loom::core::ToolInput::from_json(std::format(R"({{
              "description": "Deny {}",
              "prompt": "Call {} and finish",
              "subagent_type": "general-purpose",
              "name": "{}"
            }})", tc.tool_name, tc.tool_name, tc.agent_name)));
            ASSERT_TRUE(result.has_value()) << result.error().message;
            ASSERT_FALSE(result->is_error);
            ASSERT_TRUE(server.wait_for_request_count(2));
        }

        ASSERT_EQ(calls.size(), 1u);
        EXPECT_EQ(calls.front().tool_name, tc.tool_name);
        EXPECT_EQ(calls.front().tool_use_id, tc.tool_use_id);
        EXPECT_FALSE(calls.front().args.empty());

        auto second_body = server.request_body(1);
        ASSERT_TRUE(second_body.has_value());
        EXPECT_NE(second_body->find("Tool execution denied by permission hook"), std::string::npos);
        EXPECT_NE(second_body->find("live deny " + tc.tool_name), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(write_path));
    EXPECT_EQ(read_file(edit_path), "before edit");
    EXPECT_FALSE(fs::exists(bash_marker));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolBackgroundAgentPreservesLivePermissionHook) {
    auto root = fs::temp_directory_path() / "loom_agent_background_live_permission_test";
    fs::remove_all(root);
    fs::create_directories(root);

    auto bash_marker = root / "background-bash-denied.txt";
    const auto command = "printf background-denied > " + shell_quote_for_test(bash_marker.string());
    LocalScriptedToolUseMessagesServer server(
        "Bash",
        std::format(
            R"({{"command":"{}","description":"write background marker"}})",
            loom::tools::agent::json_escape_string(command)),
        "toolu_background_live_deny_bash",
        "background permission deny complete");
    ASSERT_TRUE(server.valid());

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "background-live-permission-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    std::vector<loom::hooks::PermissionContext> calls;
    loom::hooks::ToolPermissionHook permission_hook;
    permission_hook.set_auto_approve(false);
    permission_hook.set_working_dir(root.string());
    permission_hook.set_ask_user_response_fn([&calls](const loom::hooks::PermissionContext& ctx) {
        calls.push_back(ctx);
        loom::hooks::PermissionResponse response;
        response.decision = loom::hooks::PermissionDecision::deny;
        response.message = "background live deny Bash";
        return response;
    });

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
            .permission_check = [&permission_hook](
                std::string_view tool_name,
                std::string_view input_json,
                std::string_view tool_use_id
            ) {
                return check_agent_tool_permission_from_hook(
                    permission_hook,
                    tool_name,
                    input_json,
                    tool_use_id);
            },
            .permission_hook_valid_for_background = true,
        });

        auto result = registry.execute("Agent", loom::core::ToolInput::from_json(R"({
          "description": "Deny background Bash",
          "prompt": "Call Bash and finish",
          "subagent_type": "general-purpose",
          "run_in_background": true,
          "name": "background-live-permission-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().message;
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
        ASSERT_TRUE(wait_for_native_agent_status(
            "background-live-permission-agent",
            loom::tools::agent_runtime::NativeAgentStatus::Completed,
            std::chrono::seconds(3)));
    }

    ASSERT_EQ(calls.size(), 1u);
    EXPECT_EQ(calls.front().tool_name, "Bash");
    EXPECT_EQ(calls.front().tool_use_id, "toolu_background_live_deny_bash");
    EXPECT_FALSE(calls.front().args.empty());
    EXPECT_FALSE(fs::exists(bash_marker));

    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("Tool execution denied by permission hook"), std::string::npos);
    EXPECT_NE(second_body->find("background live deny Bash"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("background-live-permission-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolLivePermissionHookCanAllowAndUpdateChildToolInputs) {
    auto root = fs::temp_directory_path() / "loom_agent_live_permission_update_test";
    fs::remove_all(root);
    fs::create_directories(root);

    auto read_original = root / "read-original.txt";
    auto read_updated = root / "read-updated.txt";
    {
        std::ofstream out(read_original);
        out << "original read content";
    }
    {
        std::ofstream out(read_updated);
        out << "updated read content";
    }

    auto write_original = root / "write-original.txt";
    auto write_updated = root / "write-updated.txt";
    auto edit_original = root / "edit-original.txt";
    auto edit_updated = root / "edit-updated.txt";
    {
        std::ofstream out(edit_original);
        out << "original edit before";
    }
    {
        std::ofstream out(edit_updated);
        out << "before edit";
    }
    auto bash_original = root / "bash-original.txt";
    auto bash_updated = root / "bash-updated.txt";

    struct PermissionCase {
        std::string tool_name;
        std::string input_json;
        std::string updated_input_json;
        std::string tool_use_id;
        std::string agent_name;
    };

    const auto cases = std::vector<PermissionCase>{
        PermissionCase{
            .tool_name = "Read",
            .input_json = std::format(
                R"({{"file_path":"{}"}})",
                loom::tools::agent::json_escape_string(read_original.string())),
            .updated_input_json = std::format(
                R"({{"file_path":"{}"}})",
                loom::tools::agent::json_escape_string(read_updated.string())),
            .tool_use_id = "toolu_live_update_read",
            .agent_name = "live-update-read-agent",
        },
        PermissionCase{
            .tool_name = "Write",
            .input_json = std::format(
                R"({{"file_path":"{}","content":"original write content"}})",
                loom::tools::agent::json_escape_string(write_original.string())),
            .updated_input_json = std::format(
                R"({{"file_path":"{}","content":"updated write content"}})",
                loom::tools::agent::json_escape_string(write_updated.string())),
            .tool_use_id = "toolu_live_update_write",
            .agent_name = "live-update-write-agent",
        },
        PermissionCase{
            .tool_name = "Edit",
            .input_json = std::format(
                R"({{"file_path":"{}","old_string":"original edit before","new_string":"original edit after"}})",
                loom::tools::agent::json_escape_string(edit_original.string())),
            .updated_input_json = std::format(
                R"({{"file_path":"{}","old_string":"before edit","new_string":"after edit"}})",
                loom::tools::agent::json_escape_string(edit_updated.string())),
            .tool_use_id = "toolu_live_update_edit",
            .agent_name = "live-update-edit-agent",
        },
        PermissionCase{
            .tool_name = "Bash",
            .input_json = std::format(
                R"({{"command":"{}","description":"write original marker"}})",
                loom::tools::agent::json_escape_string(
                    "printf original > " + shell_quote_for_test(bash_original.string()))),
            .updated_input_json = std::format(
                R"({{"command":"{}","description":"write updated marker"}})",
                loom::tools::agent::json_escape_string(
                    "printf updated > " + shell_quote_for_test(bash_updated.string()))),
            .tool_use_id = "toolu_live_update_bash",
            .agent_name = "live-update-bash-agent",
        },
    };

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard model_guard("LOOM_MODEL", "live-permission-update-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    for (const auto& tc : cases) {
        SCOPED_TRACE(tc.tool_name);
        LocalScriptedToolUseMessagesServer server(
            tc.tool_name,
            tc.input_json,
            tc.tool_use_id,
            "permission update complete");
        ASSERT_TRUE(server.valid());
        EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());

        std::vector<loom::hooks::PermissionContext> calls;
        loom::hooks::ToolPermissionHook permission_hook;
        permission_hook.set_auto_approve(false);
        permission_hook.set_working_dir(root.string());
        permission_hook.set_ask_user_response_fn(
            [&calls, updated_input_json = tc.updated_input_json](const loom::hooks::PermissionContext& ctx) {
                calls.push_back(ctx);
                loom::hooks::PermissionResponse response;
                response.decision = loom::hooks::PermissionDecision::allow;
                response.updated_input_json = updated_input_json;
                return response;
            });

        {
            CurrentPathGuard cwd(root);
            loom::core::ToolRegistry registry;
            loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
                .permission_check = [&permission_hook](
                    std::string_view tool_name,
                    std::string_view input_json,
                    std::string_view tool_use_id
                ) {
                    return check_agent_tool_permission_from_hook(
                        permission_hook,
                        tool_name,
                        input_json,
                        tool_use_id);
                },
                .permission_hook_valid_for_background = false,
            });
            auto result = registry.execute("Agent", loom::core::ToolInput::from_json(std::format(R"({{
              "description": "Update {}",
              "prompt": "Call {} and finish",
              "subagent_type": "general-purpose",
              "name": "{}"
            }})", tc.tool_name, tc.tool_name, tc.agent_name)));
            ASSERT_TRUE(result.has_value()) << result.error().message;
            ASSERT_FALSE(result->is_error);
            ASSERT_TRUE(server.wait_for_request_count(2));
        }

        ASSERT_EQ(calls.size(), 1u);
        EXPECT_EQ(calls.front().tool_name, tc.tool_name);
        EXPECT_EQ(calls.front().tool_use_id, tc.tool_use_id);
        EXPECT_FALSE(calls.front().args.empty());

        auto record = loom::tools::agent_runtime::native_agent_store().get(tc.agent_name);
        ASSERT_TRUE(record.has_value());
        EXPECT_TRUE(std::ranges::any_of(record->transcript, [&](const auto& entry) {
            return entry.find("permission hook " + tc.tool_name + " updated input:") != std::string::npos;
        }));

        if (tc.tool_name == "Read") {
            auto second_body = server.request_body(1);
            ASSERT_TRUE(second_body.has_value());
            EXPECT_NE(second_body->find("updated read content"), std::string::npos);
            EXPECT_EQ(second_body->find("original read content"), std::string::npos);
        }
    }

    EXPECT_FALSE(fs::exists(write_original));
    ASSERT_TRUE(fs::exists(write_updated));
    EXPECT_EQ(read_file(write_updated), "updated write content");
    EXPECT_EQ(read_file(edit_original), "original edit before");
    EXPECT_EQ(read_file(edit_updated), "after edit");
    EXPECT_FALSE(fs::exists(bash_original));
    ASSERT_TRUE(fs::exists(bash_updated));
    EXPECT_EQ(read_file(bash_updated), "updated");

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPreToolHookCanPreventContinuationAfterToolExecution) {
    auto root = fs::temp_directory_path() / "loom_agent_pre_tool_stop_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto pre_marker = root / "pre-stop-hook-marker.txt";
    auto bash_marker = root / "pre-stop-bash-executed.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "stop-after-pre-tool.md");
        agent << R"MD(---
name: stop-after-pre-tool-reviewer
description: Stops continuation after pre-hooked Bash
hooks:
  PreToolUse:
    Bash:
      - command: "printf pre-stop-$LOOM_HOOK_TOOL_NAME-$LOOM_HOOK_TOOL_USE_ID > )MD" << shell_quote_for_test(pre_marker.string()) << R"MD(; printf '{\"continue\":false,\"stopReason\":\"stop after pre hook\",\"hookSpecificOutput\":{\"hookEventName\":\"PreToolUse\",\"additionalContext\":\"pre stop context\"}}'"
---
Review with pre stop hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server(
        "printf tool-output > " + shell_quote_for_test(bash_marker.string()),
        "should not be requested");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "pre-tool-stop-hook-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Stop after pre-hooked Bash",
          "prompt": "Call Bash and finish",
          "subagent_type": "stop-after-pre-tool-reviewer",
          "name": "pre-stop-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("stop after pre hook"), std::string::npos);
        ASSERT_TRUE(server.wait_for_request_count(1));
        EXPECT_FALSE(server.wait_for_request_count(2, std::chrono::milliseconds(250)));
        EXPECT_EQ(server.request_count(), 1u);
    }

    ASSERT_TRUE(fs::exists(pre_marker));
    EXPECT_EQ(read_file(pre_marker), "pre-stop-Bash-toolu_bash_fixture");
    ASSERT_TRUE(fs::exists(bash_marker));
    EXPECT_EQ(read_file(bash_marker), "tool-output");

    auto record = loom::tools::agent_runtime::native_agent_store().get("pre-stop-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);
    ASSERT_TRUE(record->output.has_value());
    EXPECT_NE(record->output->find("stop after pre hook"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook stopped continuation: stop after pre hook") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolRunsPostToolUseFailureHookForFailedNativeToolUse) {
    auto root = fs::temp_directory_path() / "loom_agent_post_tool_failure_hook_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto failure_marker = root / "failure-hook-marker.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "failure-hook.md");
        agent << R"MD(---
name: failure-hook-reviewer
description: Runs failure hooks
hooks:
  PostToolUseFailure:
    Bash:
      - command: "printf failure-$LOOM_HOOK_TOOL_NAME-$LOOM_HOOK_TOOL_USE_ID > )MD" << shell_quote_for_test(failure_marker.string()) << R"MD(; printf '{\"hookSpecificOutput\":{\"hookEventName\":\"PostToolUseFailure\",\"additionalContext\":\"failure context visible\"}}'"
---
Review with failure hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server("printf failing; exit 7", "failure hook complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "post-tool-failure-hook-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Fail Bash",
          "prompt": "Call Bash and finish",
          "subagent_type": "failure-hook-reviewer",
          "name": "post-failure-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
    }

    ASSERT_TRUE(fs::exists(failure_marker));
    EXPECT_EQ(read_file(failure_marker), "failure-Bash-toolu_bash_fixture");
    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("Exit code: 7"), std::string::npos);
    EXPECT_NE(second_body->find("failure context visible"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("post-failure-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PostToolUseFailure:Bash:") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPostToolHookCanPreventContinuation) {
    auto root = fs::temp_directory_path() / "loom_agent_post_tool_stop_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    auto post_marker = root / "post-stop-hook-marker.txt";
    {
        std::ofstream agent(root / ".loom" / "agents" / "stop-after-tool.md");
        agent << R"MD(---
name: stop-after-tool-reviewer
description: Stops continuation after Bash
hooks:
  PostToolUse:
    Bash:
      - command: "printf post-stop-$LOOM_HOOK_TOOL_NAME-$LOOM_HOOK_TOOL_USE_ID > )MD" << shell_quote_for_test(post_marker.string()) << R"MD(; printf '{\"continue\":false,\"stopReason\":\"stop after post hook\",\"hookSpecificOutput\":{\"hookEventName\":\"PostToolUse\",\"additionalContext\":\"post stop context\"}}'"
---
Review with stop hooks.
)MD";
    }

    LocalScriptedBashToolUseMessagesServer server("printf tool-output", "should not be requested");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "post-tool-stop-hook-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Stop after Bash",
          "prompt": "Call Bash and finish",
          "subagent_type": "stop-after-tool-reviewer",
          "name": "post-stop-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("stop after post hook"), std::string::npos);
        ASSERT_TRUE(server.wait_for_request_count(1));
        EXPECT_FALSE(server.wait_for_request_count(2, std::chrono::milliseconds(250)));
        EXPECT_EQ(server.request_count(), 1u);
    }

    ASSERT_TRUE(fs::exists(post_marker));
    EXPECT_EQ(read_file(post_marker), "post-stop-Bash-toolu_bash_fixture");

    auto record = loom::tools::agent_runtime::native_agent_store().get("post-stop-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Completed);
    ASSERT_TRUE(record->output.has_value());
    EXPECT_NE(record->output->find("stop after post hook"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook stopped continuation: stop after post hook") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPostToolHookCanUpdateMcpToolOutput) {
    auto root = fs::temp_directory_path() / "loom_agent_post_tool_mcp_update_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    const auto mcp_server_path = root / "server.js";
    const auto hook_json = root / "updated-mcp-output-hook.json";
    {
        std::ofstream server(mcp_server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: 'agent-mcp-update-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{ name: 'echo', description: 'Echo value', inputSchema: { type: 'object' } }]
      }
    });
    return;
  }
  if (request.method === 'tools/call') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        isError: false,
        content: [{ type: 'text', text: 'echo:' + request.params.arguments.value }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream hook(hook_json);
        hook << R"({"hookSpecificOutput":{"hookEventName":"PostToolUse","updatedMCPToolOutput":"rewritten mcp output","additionalContext":"mcp updated context"}})";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "mcp-output-hook.md");
        agent << R"MD(---
name: mcp-output-hook-reviewer
description: Updates MCP tool output through a post hook
mcpServers: [echo_fixture]
hooks:
  PostToolUse:
    mcp:
      - command: "cat )MD" << shell_quote_for_test(hook_json.string()) << R"MD("
---
Review with MCP output hooks.
)MD";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "echo_fixture",
            .command = "node",
            .args = {mcp_server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());
    auto restarted = loom::tools::restart_native_mcp_server("echo_fixture");
    ASSERT_TRUE(restarted.has_value()) << restarted.error();
    ASSERT_EQ(restarted->status, "ready");

    LocalScriptedToolUseMessagesServer server(
        "mcp",
        R"({"server_name":"echo_fixture","tool_name":"echo","arguments":{"value":"hello"}})",
        "toolu_mcp_fixture",
        "mcp hook complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "post-tool-mcp-update-hook-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        loom::tools::AgentTool tool({}, 0, &registry);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Update MCP output",
          "prompt": "Call MCP and finish",
          "subagent_type": "mcp-output-hook-reviewer",
          "name": "mcp-output-hook-agent"
        })"));
        ASSERT_TRUE(result.has_value()) << result.error().format();
        ASSERT_FALSE(result->is_error);
        ASSERT_TRUE(server.wait_for_request_count(2));
    }

    auto second_body = server.request_body(1);
    ASSERT_TRUE(second_body.has_value());
    EXPECT_NE(second_body->find("rewritten mcp output"), std::string::npos);
    EXPECT_NE(second_body->find("mcp updated context"), std::string::npos);
    EXPECT_EQ(second_body->find("echo:hello"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("mcp-output-hook-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& entry) {
        return entry.find("hook PostToolUse:mcp updated MCP output: rewritten mcp output") != std::string::npos;
    }));

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolExtractsSubagentStartHookAdditionalContext) {
    const auto context = loom::tools::agent::hook_additional_context_from_output(R"JSON({
      "hookSpecificOutput": {
        "hookEventName": "SubagentStart",
        "additionalContext": "Prefer inspecting generated bindings first."
      }
    })JSON");
    ASSERT_TRUE(context.has_value());
    EXPECT_EQ(*context, "Prefer inspecting generated bindings first.");

    EXPECT_FALSE(loom::tools::agent::hook_additional_context_from_output("plain hook log").has_value());
    EXPECT_FALSE(loom::tools::agent::hook_additional_context_from_output(R"JSON({
      "hookSpecificOutput": {
        "hookEventName": "PreToolUse",
        "additionalContext": "wrong event"
      }
    })JSON").has_value());

    std::vector<loom::services::api::Message> messages;
    loom::tools::agent::append_hook_additional_context_messages(
        messages,
        {*context, "Also check task notifications."});

    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages.front().role, "user");
    ASSERT_EQ(messages.front().content.size(), 1u);
    const auto& text = messages.front().content.front().text;
    EXPECT_NE(text.find("<hook_additional_context hook=\"SubagentStart\">"), std::string::npos);
    EXPECT_NE(text.find("Prefer inspecting generated bindings first."), std::string::npos);
    EXPECT_NE(text.find("Also check task notifications."), std::string::npos);
}

TEST(Tools, AgentToolRejectsMissingRequiredMcpServers) {
    auto root = fs::temp_directory_path() / "loom_agent_required_mcp_missing_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "linear.md");
        agent << R"MD(---
name: linear-reviewer
description: Requires Linear MCP tools
requiredMcpServers: [linear]
---
Review Linear context.
)MD";
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        config.max_depth = 0;
        loom::tools::AgentTool tool(config);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Linear review",
          "prompt": "Review with Linear context",
          "subagent_type": "linear-reviewer"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("requires MCP servers matching: linear"), std::string::npos);
        EXPECT_NE(result->content.front().text.find("MCP servers with tools: none"), std::string::npos);
        EXPECT_EQ(result->content.front().text.find("recursion depth"), std::string::npos);
    }

    fs::remove_all(root);
}

TEST(Tools, AgentToolLoadsAgentSpecificMcpServers) {
    auto root = fs::temp_directory_path() / "loom_agent_mcp_servers_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: 'agent-mcp-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{ name: 'lookup', description: 'Lookup agent context', inputSchema: { type: 'object' } }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "mcp-agent.md");
        agent << R"MD(---
name: mcp-agent
description: Uses an agent-specific MCP server
tools: [Read]
mcpServers: [agent_fixture]
---
Use the agent-specific MCP server.
)MD";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "agent_fixture",
            .command = "node",
            .args = {server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Use MCP context.";
        request.subagent_type = "mcp-agent";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_EQ(plan->agent_mcp_servers.size(), 1u);
        EXPECT_EQ(plan->agent_mcp_servers.front(), "agent_fixture");
        ASSERT_EQ(plan->agent_mcp_tools.size(), 1u);
        EXPECT_EQ(plan->agent_mcp_tools.front().server_name, "agent_fixture");
        EXPECT_EQ(plan->agent_mcp_tools.front().tool_name, "lookup");
        ASSERT_TRUE(plan->agent_mcp_context_message.has_value());
        EXPECT_NE(plan->agent_mcp_context_message->find("agent_fixture/lookup"), std::string::npos);
        ASSERT_EQ(plan->allowed_tools.size(), 1u);
        EXPECT_EQ(plan->allowed_tools.front(), "Read");
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, AgentToolLoadsInlineAgentMcpServersWithoutDroppingReferencedServers) {
    auto root = fs::temp_directory_path() / "loom_agent_inline_mcp_servers_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: process.env.SERVER_NAME || 'agent-inline-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{
          name: process.env.TOOL_NAME || 'lookup',
          description: ['tool', process.env.SERVER_NAME, process.env.INLINE_TOKEN].filter(Boolean).join(':'),
          inputSchema: { type: 'object' }
        }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "inline-mcp-agent.md");
        agent << std::format(R"MD(---
name: inline-mcp-agent
description: Uses referenced and inline MCP servers
mcpServers:
  - existing_fixture
  - inline_fixture:
      type: stdio
      command: node
      args:
        - "{}"
      env:
        SERVER_NAME: inline_fixture
        TOOL_NAME: inline_lookup
        INLINE_TOKEN: secret-token
---
Use both MCP servers.
)MD", server_path.string());
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "existing_fixture",
            .command = "node",
            .args = {server_path.string()},
            .env = {{"SERVER_NAME", "existing_fixture"}, {"TOOL_NAME", "existing_lookup"}},
        },
    });
    ASSERT_TRUE(synced.has_value());

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Use MCP context.";
        request.subagent_type = "inline-mcp-agent";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, config);

        ASSERT_TRUE(plan.has_value()) << plan.error();
        ASSERT_EQ(plan->agent_mcp_servers.size(), 2u);
        EXPECT_EQ(plan->agent_mcp_servers[0], "existing_fixture");
        EXPECT_EQ(plan->agent_mcp_servers[1], "inline_fixture");
        ASSERT_EQ(plan->agent_mcp_tools.size(), 2u);
        EXPECT_EQ(plan->agent_mcp_tools[0].server_name, "existing_fixture");
        EXPECT_EQ(plan->agent_mcp_tools[0].tool_name, "existing_lookup");
        EXPECT_EQ(plan->agent_mcp_tools[1].server_name, "inline_fixture");
        EXPECT_EQ(plan->agent_mcp_tools[1].tool_name, "inline_lookup");
        EXPECT_NE(plan->agent_mcp_tools[1].description.find("secret-token"), std::string::npos);
        ASSERT_TRUE(plan->agent_mcp_context_message.has_value());
        EXPECT_NE(plan->agent_mcp_context_message->find("existing_fixture/existing_lookup"), std::string::npos);
        EXPECT_NE(plan->agent_mcp_context_message->find("inline_fixture/inline_lookup"), std::string::npos);
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, AgentToolCleansInlineMcpServerConfiguration) {
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "inline_restore_fixture",
            .command = "node",
            .args = {"old-server.js"},
            .env = {{"TOKEN", "old"}},
        },
    }).has_value());

    std::vector<loom::tools::agent_runtime::AgentInlineMcpServerConfig> inline_servers{
        loom::tools::agent_runtime::AgentInlineMcpServerConfig{
            .name = "inline_restore_fixture",
            .transport = "stdio",
            .command = "node",
            .args = {"new-server.js"},
            .env = {{"TOKEN", "new"}},
        },
        loom::tools::agent_runtime::AgentInlineMcpServerConfig{
            .name = "inline_remove_fixture",
            .transport = "stdio",
            .command = "node",
            .args = {"temporary-server.js"},
            .env = {},
        },
    };

    auto states = loom::tools::agent::prepare_agent_inline_mcp_servers(inline_servers);
    ASSERT_TRUE(states.has_value()) << states.error();
    ASSERT_EQ(states->size(), 2u);

    auto overwritten = loom::tools::native_mcp_configured_server("inline_restore_fixture");
    ASSERT_TRUE(overwritten.has_value());
    ASSERT_EQ(overwritten->args.size(), 1u);
    EXPECT_EQ(overwritten->args.front(), "new-server.js");
    EXPECT_EQ(overwritten->env.at("TOKEN"), "new");

    auto temporary = loom::tools::native_mcp_configured_server("inline_remove_fixture");
    ASSERT_TRUE(temporary.has_value());
    ASSERT_EQ(temporary->args.size(), 1u);
    EXPECT_EQ(temporary->args.front(), "temporary-server.js");

    {
        loom::tools::agent::AgentMcpCleanupGuard cleanup{
            .agent_id = "mcp-cleanup-agent",
            .inline_servers = *states,
        };
    }

    auto restored = loom::tools::native_mcp_configured_server("inline_restore_fixture");
    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(restored->args.size(), 1u);
    EXPECT_EQ(restored->args.front(), "old-server.js");
    EXPECT_EQ(restored->env.at("TOKEN"), "old");
    EXPECT_FALSE(loom::tools::native_mcp_configured_server("inline_remove_fixture").has_value());

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
}

TEST(Tools, AgentToolCleansInlineMcpServersWhenPlanBuildFails) {
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "inline_plan_failure_restore",
            .command = "node",
            .args = {"old-server.js"},
            .env = {{"TOKEN", "old"}},
        },
    }).has_value());

    auto root = fs::temp_directory_path() / "loom_agent_inline_mcp_failure_cleanup_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: process.env.SERVER_NAME || 'inline-plan-failure-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{
          name: process.env.TOOL_NAME || 'lookup',
          description: ['tool', process.env.SERVER_NAME].filter(Boolean).join(':'),
          inputSchema: { type: 'object' }
        }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "inline-failure.md");
        agent << std::format(R"MD(---
name: inline-failure-agent
description: Fails after configuring inline MCP servers
requiredMcpServers: [definitely-missing-required-server]
mcpServers:
  - inline_plan_failure_restore:
      type: stdio
      command: node
      args: ["{}"]
      env:
        TOKEN: new
        SERVER_NAME: inline_plan_failure_restore
        TOOL_NAME: restore_lookup
  - inline_plan_failure_remove:
      type: stdio
      command: node
      args: ["{}"]
      env:
        SERVER_NAME: inline_plan_failure_remove
        TOOL_NAME: remove_lookup
---
Review with inline MCP cleanup on failure.
)MD", server_path.string(), server_path.string());
    }

    {
        CurrentPathGuard cwd(root);
        loom::tools::agent::AgentToolRequest request;
        request.prompt = "Trigger required MCP validation failure.";
        request.subagent_type = "inline-failure-agent";

        auto plan = loom::tools::agent::build_agent_execution_plan(request, loom::tools::AgentConfig{});
        ASSERT_FALSE(plan.has_value());
        EXPECT_NE(plan.error().find("requires MCP servers matching"), std::string::npos);
    }

    auto restored = loom::tools::native_mcp_configured_server("inline_plan_failure_restore");
    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(restored->args.size(), 1u);
    EXPECT_EQ(restored->args.front(), "old-server.js");
    EXPECT_EQ(restored->env.at("TOKEN"), "old");
    EXPECT_FALSE(loom::tools::native_mcp_configured_server("inline_plan_failure_remove").has_value());

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, AgentToolAcceptsReadyRequiredMcpServers) {
    auto root = fs::temp_directory_path() / "loom_agent_required_mcp_ready_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });

function send(message) {
  process.stdout.write(JSON.stringify(message) + '\n');
}

rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {} },
        serverInfo: { name: 'linear-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{ name: 'lookup', description: 'Lookup issue', inputSchema: { type: 'object' } }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream agent(root / ".loom" / "agents" / "linear.md");
        agent << R"MD(---
name: linear-reviewer
description: Requires Linear MCP tools
requiredMcpServers: [linear]
---
Review Linear context.
)MD";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "linear_fixture",
            .command = "node",
            .args = {server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());
    auto restarted = loom::tools::restart_native_mcp_server("linear_fixture");
    ASSERT_TRUE(restarted.has_value()) << restarted.error();
    ASSERT_EQ(restarted->status, "ready");
    ASSERT_EQ(restarted->tools.size(), 1u);

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentConfig config;
        config.max_depth = 0;
        loom::tools::AgentTool tool(config);

        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Linear review",
          "prompt": "Review with Linear context",
          "subagent_type": "linear-reviewer"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("Agent recursion depth limit reached"), std::string::npos);
        EXPECT_EQ(result->content.front().text.find("requires MCP servers"), std::string::npos);
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, AgentToolAcceptsBackgroundNativeParameters) {
    loom::tools::AgentTool tool;

    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async",
      "prompt": "Run in the background",
      "name": "reviewer-one",
      "mode": "default",
      "run_in_background": true
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Queued background agent reviewer-one"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("reviewer-one");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->agent_type, "general-purpose");
    EXPECT_FALSE(record->isolation.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
}

TEST(Tools, AgentToolResumeExistingBackgroundPreservesNativeHistoryAndPendingQueue) {
    auto root = fs::temp_directory_path() / "loom_agent_resume_existing_preserve_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "resume-existing",
        .agent_type = "general-purpose",
        .description = "Existing background agent",
        .cwd = root.string(),
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .sidechain_entries = {
            R"({"type":"user","uuid":"resume-existing-0","parentUuid":null,"isSidechain":true,"agentId":"resume-existing","message":{"role":"user","content":[{"type":"text","text":"original context"}]}})",
        },
        .pending_messages = {"[Message from team-lead priority=normal]\nContinue the old job"},
        .transcript = {"user: original context", "assistant: partial result"},
        .progress = 0.5,
    });

    loom::tools::AgentTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "agent_id": "resume-existing",
      "resume_existing": true,
      "description": "Existing background agent",
      "prompt": "Resume this existing background agent.",
      "subagent_type": "general-purpose",
      "run_in_background": true
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Queued background agent resume-existing"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("resume-existing");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
    ASSERT_GE(record->transcript.size(), 3u);
    EXPECT_EQ(record->transcript[0], "user: original context");
    EXPECT_EQ(record->transcript[1], "assistant: partial result");
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& line) {
        return line.find("Resume this existing background agent") != std::string::npos;
    }));
    ASSERT_EQ(record->pending_messages.size(), 1u);
    EXPECT_NE(record->pending_messages.front().find("Continue the old job"), std::string::npos);
    ASSERT_EQ(record->sidechain_entries.size(), 1u);
    EXPECT_NE(record->sidechain_entries.front().find("original context"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}
TEST(Tools, AgentToolSpawnsTeammateWithDeterministicAgentId) {
    auto root = fs::temp_directory_path() / "loom_agent_teammate_spawn_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard teammate_backend_guard("LOOM_TEAMMATE_BACKEND", "in-process");
    loom::utils::swarm_backends::BackendRegistry::reset();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto team = loom::tools::global_team_store().create("migration-team", "migration-team", {});
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));

    loom::tools::AgentTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Spawn reviewer",
      "prompt": "Review migration parity",
      "name": "reviewer-one",
      "team_name": "migration-team",
      "subagent_type": "general-purpose",
      "mode": "plan"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Spawned successfully"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("agent_id: reviewer-one@migration-team"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("backend: in-process"), std::string::npos);
    EXPECT_NE(
        result->content.front().text.find("task_id: in-process:reviewer-one@migration-team"),
        std::string::npos);
    EXPECT_NE(result->content.front().text.find("status: teammate_spawned"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("reviewer-one@migration-team");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(record->background);
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
    ASSERT_TRUE(record->team_name.has_value());
    EXPECT_EQ(*record->team_name, "migration-team");
    ASSERT_TRUE(record->mode.has_value());
    EXPECT_EQ(*record->mode, "plan");
    ASSERT_TRUE(record->teammate_backend.has_value());
    EXPECT_EQ(*record->teammate_backend, "in-process");
    ASSERT_TRUE(record->teammate_task_id.has_value());
    EXPECT_EQ(*record->teammate_task_id, "in-process:reviewer-one@migration-team");
    EXPECT_FALSE(record->teammate_pane_id.has_value());
    ASSERT_TRUE(record->parent_session_id.has_value());
    EXPECT_EQ(*record->parent_session_id, "native-session");

    auto restored_team = loom::tools::global_team_store().get("migration-team");
    ASSERT_TRUE(restored_team.has_value()) << std::string(loom::tools::format_error(restored_team.error()));
    auto member = std::ranges::find_if((*restored_team)->members, [](const auto& candidate) {
        return candidate.agent_id == "reviewer-one@migration-team";
    });
    ASSERT_NE(member, (*restored_team)->members.end());
    EXPECT_EQ(member->role, loom::tools::MemberRole::Worker);
    EXPECT_EQ(member->status, loom::tools::MemberStatus::Working);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto listed = registry.execute("task_list", loom::core::ToolInput::from_json("{}"));
    ASSERT_TRUE(listed.has_value());
    ASSERT_FALSE(listed->is_error);
    ASSERT_FALSE(listed->content.empty());
    EXPECT_NE(listed->content.front().text.find("reviewer-one@migration-team [queued]"), std::string::npos);
    EXPECT_NE(listed->content.front().text.find("teammate_backend: in-process"), std::string::npos);
    EXPECT_NE(
        listed->content.front().text.find("teammate_task_id: in-process:reviewer-one@migration-team"),
        std::string::npos);

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, AgentToolSpawnsTeammateWithUniqueNameWhenTeamAlreadyHasMember) {
    auto root = fs::temp_directory_path() / "loom_agent_teammate_unique_spawn_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard teammate_backend_guard("LOOM_TEAMMATE_BACKEND", "in-process");
    loom::utils::swarm_backends::BackendRegistry::reset();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto team = loom::tools::global_team_store().create("migration-team", "migration-team", {
        loom::tools::TeamMember{
            .agent_id = "reviewer-one@migration-team",
            .role = loom::tools::MemberRole::Worker,
            .status = loom::tools::MemberStatus::Working,
        },
        loom::tools::TeamMember{
            .agent_id = "reviewer-one-2@migration-team",
            .role = loom::tools::MemberRole::Worker,
            .status = loom::tools::MemberStatus::Working,
        },
    });
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));

    loom::tools::AgentTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Spawn duplicate reviewer",
      "prompt": "Review migration parity again",
      "name": "reviewer-one",
      "team_name": "migration-team",
      "subagent_type": "general-purpose"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("agent_id: reviewer-one-3@migration-team"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("name: reviewer-one-3"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("reviewer-one-3@migration-team");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->name.has_value());
    EXPECT_EQ(*record->name, "reviewer-one-3");
    ASSERT_TRUE(record->teammate_task_id.has_value());
    EXPECT_EQ(*record->teammate_task_id, "in-process:reviewer-one-3@migration-team");

    auto restored_team = loom::tools::global_team_store().get("migration-team");
    ASSERT_TRUE(restored_team.has_value()) << std::string(loom::tools::format_error(restored_team.error()));
    auto member = std::ranges::find_if((*restored_team)->members, [](const auto& candidate) {
        return candidate.agent_id == "reviewer-one-3@migration-team";
    });
    ASSERT_NE(member, (*restored_team)->members.end());

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, AgentToolRejectsNestedTeammateSpawnFromTeamContext) {
    struct ClearDynamicTeamContext {
        ~ClearDynamicTeamContext() {
            loom::utils::clear_dynamic_team_context();
        }
    } clear_dynamic_team_context;

    loom::utils::set_dynamic_team_context(loom::utils::DynamicTeamContext{
        .agent_id = "worker-one@migration-team",
        .agent_name = "worker-one",
        .team_name = "migration-team",
        .agent_type = std::nullopt,
        .color = "blue",
        .plan_mode_required = false,
        .parent_session_id = "leader-session",
    });

    loom::tools::AgentTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Spawn nested teammate",
      "prompt": "Try to spawn another teammate",
      "name": "nested-worker",
      "subagent_type": "general-purpose"
    })"));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Teammates cannot spawn other teammates"), std::string::npos);
}

TEST(Tools, AgentToolRejectsBackgroundAgentFromInProcessTeammateContext) {
    auto ctx = loom::utils::create_teammate_context(
        "worker-one@migration-team",
        "worker-one",
        "migration-team",
        "leader-session",
        false,
        std::optional<std::string_view>{"blue"});

    auto result = loom::utils::run_with_teammate_context(ctx, [] {
        loom::tools::AgentTool tool;
        return tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Spawn async subagent",
          "prompt": "Try to spawn a background subagent",
          "subagent_type": "general-purpose",
          "run_in_background": true
        })"));
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("In-process teammates cannot spawn background agents"), std::string::npos);
}

TEST(Tools, AgentRuntimeBuildsTeammateAppendSystemPromptFromAgentType) {
    struct ClearDynamicTeamContext {
        ~ClearDynamicTeamContext() {
            loom::utils::clear_dynamic_team_context();
        }
    } clear_dynamic_team_context;

    auto root = fs::temp_directory_path() / "loom_teammate_agent_type_prompt_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "agents");
    {
        std::ofstream agent(root / ".loom" / "agents" / "reviewer.md");
        agent << R"MD(---
name: reviewer
description: Reviews migration parity
---
You review C++ migration parity and report missing behavior.
)MD";
    }

    loom::utils::set_dynamic_team_context(loom::utils::DynamicTeamContext{
        .agent_id = "reviewer-one@migration-team",
        .agent_name = "reviewer-one",
        .team_name = "migration-team",
        .agent_type = std::optional<std::string>{"reviewer"},
        .color = "blue",
        .plan_mode_required = false,
        .parent_session_id = "leader-session",
    });

    auto prompt = loom::tools::agent_runtime::build_teammate_append_system_prompt(
        std::optional<std::string>{"existing append prompt"},
        root);
    ASSERT_TRUE(prompt.has_value());
    EXPECT_NE(prompt->find("existing append prompt"), std::string::npos);
    EXPECT_NE(prompt->find("Agent Teammate Communication"), std::string::npos);
    EXPECT_NE(prompt->find("# Custom Agent Instructions"), std::string::npos);
    EXPECT_NE(prompt->find("You review C++ migration parity"), std::string::npos);

    loom::utils::clear_dynamic_team_context();
    fs::remove_all(root);
}

TEST(Tools, AgentToolCreatesWorktreeForIsolatedBackgroundAgent) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git is required for worktree isolation";
    }

    auto root = fs::temp_directory_path() / "loom_agent_worktree_isolation_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream readme(root / "README.md");
        readme << "worktree isolation\n";
    }
    ASSERT_EQ(std::system(std::format("git -C \"{}\" init -q --template=", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.email test@example.com", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.name Test", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" add README.md", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" commit -q --no-verify -m init", root.string()).c_str()), 0);

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentTool tool;
        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Run isolated",
          "prompt": "Inspect the isolated checkout",
          "name": "isolated-agent",
          "run_in_background": true,
          "isolation": "worktree"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("Queued background agent isolated-agent"), std::string::npos);
    }

    auto record = loom::tools::agent_runtime::native_agent_store().get("isolated-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->cwd.has_value());
    auto worktree_path = fs::path{*record->cwd};
    EXPECT_TRUE(fs::exists(worktree_path / ".git"));
    EXPECT_EQ(worktree_path, fs::weakly_canonical(root) / ".loom" / "worktrees" / "isolated-agent");
    ASSERT_TRUE(record->isolation.has_value());
    EXPECT_EQ(*record->isolation, "worktree");
    ASSERT_TRUE(record->worktree_path.has_value());
    EXPECT_EQ(*record->worktree_path, worktree_path.string());
    ASSERT_TRUE(record->worktree_branch.has_value());
    EXPECT_EQ(*record->worktree_branch, "loom-agent-isolated-agent");
    ASSERT_TRUE(record->worktree_base_commit.has_value());
    ASSERT_TRUE(record->worktree_git_root.has_value());
    EXPECT_EQ(*record->worktree_git_root, fs::weakly_canonical(root).string());

    auto cleanup = loom::tools::agent::cleanup_agent_worktree("isolated-agent");
    EXPECT_TRUE(cleanup.attempted);
    EXPECT_TRUE(cleanup.removed);
    EXPECT_FALSE(fs::exists(worktree_path));

    auto cleaned = loom::tools::agent_runtime::native_agent_store().get("isolated-agent");
    ASSERT_TRUE(cleaned.has_value());
    EXPECT_TRUE(cleaned->worktree_cleanup_performed);
    EXPECT_FALSE(cleaned->worktree_path.has_value());
    EXPECT_FALSE(cleaned->cwd.has_value());

    EXPECT_NE(std::system(std::format(
        "git -C \"{}\" rev-parse --verify loom-agent-isolated-agent >/dev/null 2>&1",
        root.string()).c_str()), 0);
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolPreservesChangedWorktreeAndReportsPath) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git is required for worktree isolation";
    }

    auto root = fs::temp_directory_path() / "loom_agent_worktree_dirty_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream readme(root / "README.md");
        readme << "worktree dirty preservation\n";
    }
    ASSERT_EQ(std::system(std::format("git -C \"{}\" init -q --template=", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.email test@example.com", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.name Test", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" add README.md", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" commit -q --no-verify -m init", root.string()).c_str()), 0);

    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        CurrentPathGuard cwd(root);
        loom::tools::AgentTool tool;
        auto result = tool.execute(loom::core::ToolInput::from_json(R"({
          "description": "Run isolated",
          "prompt": "Leave changed worktree",
          "name": "dirty-agent",
          "run_in_background": true,
          "isolation": "worktree"
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(result->is_error);
    }

    auto record = loom::tools::agent_runtime::native_agent_store().get("dirty-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->worktree_path.has_value());
    auto worktree_path = fs::path{*record->worktree_path};
    {
        std::ofstream dirty(worktree_path / "dirty.txt");
        dirty << "agent changes\n";
    }

    auto cleanup = loom::tools::agent::cleanup_agent_worktree("dirty-agent");
    EXPECT_TRUE(cleanup.attempted);
    EXPECT_FALSE(cleanup.removed);
    EXPECT_TRUE(cleanup.changed);
    EXPECT_TRUE(fs::exists(worktree_path));

    auto retained = loom::tools::agent_runtime::native_agent_store().get("dirty-agent");
    ASSERT_TRUE(retained.has_value());
    ASSERT_TRUE(retained->worktree_path.has_value());
    EXPECT_EQ(*retained->worktree_path, worktree_path.string());
    EXPECT_FALSE(retained->worktree_cleanup_performed);

    loom::tools::agent_runtime::native_agent_store().mark_completed("dirty-agent", "dirty worktree retained");
    auto notifications = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    ASSERT_EQ(notifications.size(), 1u);
    EXPECT_NE(notifications.front().find("<worktree_path>"), std::string::npos);
    EXPECT_NE(notifications.front().find(worktree_path.string()), std::string::npos);
    EXPECT_NE(notifications.front().find("<worktree_branch>loom-agent-dirty-agent</worktree_branch>"), std::string::npos);

    (void)std::system(std::format("git -C \"{}\" worktree remove --force \"{}\" >/dev/null 2>&1",
        root.string(), worktree_path.string()).c_str());
    (void)std::system(std::format("git -C \"{}\" branch -D loom-agent-dirty-agent >/dev/null 2>&1",
        root.string()).c_str());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolUpdatesProgressAfterStartingApiStream) {
    auto root = fs::temp_directory_path() / "loom_native_agent_progress_test";
    { std::error_code ec; fs::remove_all(root, ec); }
    fs::create_directories(root);
    LocalSlowMessagesStreamServer server(std::chrono::milliseconds(750));
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "stream-progress-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Track async progress while streaming",
      "prompt": "Wait for the slow stream to complete",
      "name": "stream-progress-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    ASSERT_TRUE(server.wait_for_request());

    bool observed_running_progress = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto record = loom::tools::agent_runtime::native_agent_store().get("stream-progress-agent");
        ASSERT_TRUE(record.has_value());
        if (record->status == loom::tools::agent_runtime::NativeAgentStatus::Running &&
            record->progress &&
            *record->progress > 0.0 &&
            *record->progress < 1.0) {
            observed_running_progress = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(observed_running_progress);
    ASSERT_TRUE(wait_for_native_agent_status(
        "stream-progress-agent",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));
    auto completed = loom::tools::agent_runtime::native_agent_store().get("stream-progress-agent");
    ASSERT_TRUE(completed.has_value());
    ASSERT_TRUE(completed->progress.has_value());
    EXPECT_DOUBLE_EQ(*completed->progress, 1.0);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    { std::error_code ec; fs::remove_all(root, ec); }
}

TEST(Tools, AgentToolBuildsFilteredStructuredResumeMessagesFromSidechain) {
    std::vector<std::string> entries{
        R"({"type":"user","uuid":"u1","parentUuid":null,"isSidechain":true,"agentId":"resume-filtered","message":{"role":"user","content":[{"type":"text","text":"First user"}]}})",
        R"({"type":"assistant","uuid":"a-whitespace","parentUuid":"u1","isSidechain":true,"agentId":"resume-filtered","message":{"role":"assistant","content":[{"type":"text","text":"\n\t  "}]}})",
        R"({"type":"user","uuid":"u2","parentUuid":"a-whitespace","isSidechain":true,"agentId":"resume-filtered","message":{"role":"user","content":[{"type":"text","text":"Second user"}]}})",
        R"({"type":"assistant","uuid":"a-thinking","parentUuid":"u2","isSidechain":true,"agentId":"resume-filtered","message":{"role":"assistant","content":[{"type":"thinking","thinking":"orphaned reasoning","signature":"sig"}]}})",
        R"({"type":"assistant","uuid":"a-unresolved","parentUuid":"a-thinking","isSidechain":true,"agentId":"resume-filtered","message":{"role":"assistant","content":[{"type":"text","text":"I will call a missing tool"},{"type":"tool_use","id":"missing-tool","name":"Read","input":{"file_path":"missing.md"}}]}})",
        R"({"type":"assistant","uuid":"a-resolved","parentUuid":"a-unresolved","isSidechain":true,"agentId":"resume-filtered","message":{"role":"assistant","content":[{"type":"text","text":"I will read README"},{"type":"tool_use","id":"read-ok","name":"Read","input":{"file_path":"README.md"}}]}})",
        R"({"type":"user","uuid":"u3","parentUuid":"a-resolved","isSidechain":true,"agentId":"resume-filtered","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"read-ok","content":[{"type":"text","text":"README content"}]}]}})",
    };

    auto messages = loom::tools::agent::resume_messages_from_sidechain_entries(entries);
    ASSERT_EQ(messages.size(), 3u);
    EXPECT_EQ(messages[0].role, "user");
    ASSERT_EQ(messages[0].content.size(), 2u);
    EXPECT_EQ(messages[0].content[0].text, "First user");
    EXPECT_EQ(messages[0].content[1].text, "Second user");
    EXPECT_EQ(messages[1].role, "assistant");
    ASSERT_EQ(messages[1].content.size(), 2u);
    EXPECT_EQ(messages[1].content[0].text, "I will read README");
    EXPECT_EQ(messages[1].content[1].type, loom::services::api::ContentBlockType::ToolUse);
    EXPECT_EQ(messages[1].content[1].tool_use_id, "read-ok");
    EXPECT_EQ(messages[2].role, "user");
    ASSERT_EQ(messages[2].content.size(), 1u);
    EXPECT_EQ(messages[2].content[0].type, loom::services::api::ContentBlockType::ToolResult);
    EXPECT_EQ(messages[2].content[0].tool_use_id, "read-ok");
    EXPECT_EQ(messages[2].content[0].text, "README content");
}

TEST(Tools, AgentToolReplaysResumeContentReplacementRecordsFromSidechain) {
    std::vector<std::string> entries{
        R"({"type":"user","uuid":"u1","parentUuid":null,"isSidechain":true,"agentId":"resume-replacement","message":{"role":"user","content":[{"type":"text","text":"Read a large report"}]}})",
        R"({"type":"assistant","uuid":"a1","parentUuid":"u1","isSidechain":true,"agentId":"resume-replacement","message":{"role":"assistant","content":[{"type":"tool_use","id":"large-result-1","name":"Bash","input":{"command":"cat report.txt"}}]}})",
        R"({"type":"user","uuid":"u2","parentUuid":"a1","isSidechain":true,"agentId":"resume-replacement","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"large-result-1","content":[{"type":"text","text":"FULL LARGE RESULT"}]}]}})",
        R"({"type":"content-replacement","sessionId":"session-1","agentId":"resume-replacement","replacements":[{"kind":"tool-result","toolUseId":"large-result-1","replacement":"[persisted preview for large-result-1]"}]})",
    };

    auto messages = loom::tools::agent::resume_messages_from_sidechain_entries(entries);
    ASSERT_EQ(messages.size(), 3u);
    EXPECT_EQ(messages[2].role, "user");
    ASSERT_EQ(messages[2].content.size(), 1u);
    EXPECT_EQ(messages[2].content[0].type, loom::services::api::ContentBlockType::ToolResult);
    EXPECT_EQ(messages[2].content[0].tool_use_id, "large-result-1");
    EXPECT_EQ(messages[2].content[0].text, "[persisted preview for large-result-1]");
}

TEST(Tools, AgentToolPersistsLiveContentReplacementRecordsForLargeToolResults) {
    auto root = fs::temp_directory_path() / "loom_agent_live_content_replacement_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "live-replacement",
        .agent_type = "general-purpose",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    std::string large_result(210'000, 'x');
    std::vector<loom::services::api::Message> messages;
    loom::services::api::Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "huge-1",
        .tool_name = "Bash",
        .tool_input_json = R"({"command":"cat huge.log"})",
    });
    messages.push_back(std::move(assistant));
    loom::services::api::Message result;
    result.role = "user";
    result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = large_result,
        .tool_use_id = "huge-1",
    });
    messages.push_back(std::move(result));

    loom::tools::agent::AgentContentReplacementState state;
    auto replaced = loom::tools::agent::apply_agent_tool_result_budget("live-replacement", messages, state);
    EXPECT_EQ(replaced.newly_replaced, 1u);
    EXPECT_EQ(replaced.reapplied, 0u);
    ASSERT_EQ(messages[1].content.size(), 1u);
    EXPECT_NE(messages[1].content[0].text.find("<persisted-output>"), std::string::npos);
    EXPECT_NE(messages[1].content[0].text.find("Full output saved to:"), std::string::npos);
    EXPECT_NE(messages[1].content[0].text.find("Preview (first 2000 bytes):"), std::string::npos);
    EXPECT_LT(messages[1].content[0].text.size(), large_result.size());

    auto persisted_path = root / "runtime" / "tool-results" / "live-replacement-huge-1.txt";
    ASSERT_TRUE(fs::exists(persisted_path));
    EXPECT_EQ(read_file(persisted_path), large_result);

    auto record = loom::tools::agent_runtime::native_agent_store().get("live-replacement");
    ASSERT_TRUE(record.has_value());
    ASSERT_EQ(record->sidechain_entries.size(), 1u);
    EXPECT_NE(record->sidechain_entries.front().find(R"("type":"content-replacement")"), std::string::npos);
    EXPECT_NE(record->sidechain_entries.front().find(R"("agentId":"live-replacement")"), std::string::npos);
    EXPECT_NE(record->sidechain_entries.front().find(R"("toolUseId":"huge-1")"), std::string::npos);

    const auto replacement_text = messages[1].content[0].text;
    messages[1].content[0].text = large_result;
    auto reapplied = loom::tools::agent::apply_agent_tool_result_budget("live-replacement", messages, state);
    EXPECT_EQ(reapplied.newly_replaced, 0u);
    EXPECT_EQ(reapplied.reapplied, 1u);
    EXPECT_EQ(messages[1].content[0].text, replacement_text);
    auto after_reapply = loom::tools::agent_runtime::native_agent_store().get("live-replacement");
    ASSERT_TRUE(after_reapply.has_value());
    EXPECT_EQ(after_reapply->sidechain_entries.size(), 1u);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolSkipsLiveContentReplacementForUnboundedToolResults) {
    auto root = fs::temp_directory_path() / "loom_agent_unbounded_content_replacement_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "unbounded-replacement",
        .agent_type = "general-purpose",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto* read_tool = registry.get("Read");
    ASSERT_NE(read_tool, nullptr);
    EXPECT_TRUE(read_tool->definition().max_result_size_unbounded);
    auto* bash_tool = registry.get("Bash");
    ASSERT_NE(bash_tool, nullptr);
    EXPECT_FALSE(bash_tool->definition().max_result_size_unbounded);
    EXPECT_EQ(bash_tool->definition().max_result_size_chars, 30'000u);
    auto skip_names = loom::tools::agent::unbounded_tool_result_budget_names(registry.get_visible_definitions());
    EXPECT_TRUE(skip_names.contains("read"));

    auto make_messages = [](std::string tool_name, std::string tool_use_id, const std::string& text) {
        std::vector<loom::services::api::Message> messages;
        loom::services::api::Message assistant;
        assistant.role = "assistant";
        assistant.content.push_back(loom::services::api::ContentBlock{
            .type = loom::services::api::ContentBlockType::ToolUse,
            .tool_use_id = tool_use_id,
            .tool_name = std::move(tool_name),
            .tool_input_json = "{}",
        });
        messages.push_back(std::move(assistant));
        loom::services::api::Message result;
        result.role = "user";
        result.content.push_back(loom::services::api::ContentBlock{
            .type = loom::services::api::ContentBlockType::ToolResult,
            .text = text,
            .tool_use_id = std::move(tool_use_id),
        });
        messages.push_back(std::move(result));
        return messages;
    };

    std::string large_result(210'000, 'r');
    auto read_messages = make_messages("Read", "read-huge", large_result);
    loom::tools::agent::AgentContentReplacementState read_state;
    auto skipped = loom::tools::agent::apply_agent_tool_result_budget(
        "unbounded-replacement",
        read_messages,
        read_state,
        skip_names);
    EXPECT_EQ(skipped.newly_replaced, 0u);
    EXPECT_EQ(skipped.reapplied, 0u);
    EXPECT_EQ(read_messages[1].content[0].text, large_result);
    auto skipped_record = loom::tools::agent_runtime::native_agent_store().get("unbounded-replacement");
    ASSERT_TRUE(skipped_record.has_value());
    EXPECT_TRUE(skipped_record->sidechain_entries.empty());

    auto bash_messages = make_messages("Bash", "bash-huge", large_result);
    loom::tools::agent::AgentContentReplacementState bash_state;
    auto replaced = loom::tools::agent::apply_agent_tool_result_budget(
        "unbounded-replacement",
        bash_messages,
        bash_state,
        skip_names);
    EXPECT_EQ(replaced.newly_replaced, 1u);
    EXPECT_NE(bash_messages[1].content[0].text.find("<persisted-output>"), std::string::npos);
    auto replaced_record = loom::tools::agent_runtime::native_agent_store().get("unbounded-replacement");
    ASSERT_TRUE(replaced_record.has_value());
    ASSERT_EQ(replaced_record->sidechain_entries.size(), 1u);
    EXPECT_NE(replaced_record->sidechain_entries.front().find(R"("toolUseId":"bash-huge")"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolUsesFiniteToolResultThresholdsBeforeAggregateBudget) {
    auto root = fs::temp_directory_path() / "loom_agent_finite_threshold_replacement_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "finite-threshold",
        .agent_type = "general-purpose",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto thresholds = loom::tools::agent::tool_result_budget_thresholds(registry.get_visible_definitions());
    ASSERT_TRUE(thresholds.contains("bash"));
    EXPECT_EQ(thresholds["bash"], 30'000u);
    ASSERT_TRUE(thresholds.contains("grep"));
    EXPECT_EQ(thresholds["grep"], 20'000u);

    std::string bash_result(40'000, 'b');
    std::vector<loom::services::api::Message> messages;
    loom::services::api::Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "bash-40k",
        .tool_name = "Bash",
        .tool_input_json = R"({"command":"cat mid.log"})",
    });
    messages.push_back(std::move(assistant));
    loom::services::api::Message result;
    result.role = "user";
    result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = bash_result,
        .tool_use_id = "bash-40k",
    });
    messages.push_back(std::move(result));

    loom::tools::agent::AgentContentReplacementState state;
    auto replaced = loom::tools::agent::apply_agent_tool_result_budget(
        "finite-threshold",
        messages,
        state,
        loom::tools::agent::unbounded_tool_result_budget_names(registry.get_visible_definitions()),
        thresholds);
    EXPECT_EQ(replaced.newly_replaced, 1u);
    EXPECT_EQ(replaced.reapplied, 0u);
    EXPECT_NE(messages[1].content[0].text.find("<persisted-output>"), std::string::npos);
    EXPECT_LT(messages[1].content[0].text.size(), bash_result.size());
    auto persisted_path = root / "runtime" / "tool-results" / "finite-threshold-bash-40k.txt";
    ASSERT_TRUE(fs::exists(persisted_path));
    EXPECT_EQ(read_file(persisted_path), bash_result);

    auto record = loom::tools::agent_runtime::native_agent_store().get("finite-threshold");
    ASSERT_TRUE(record.has_value());
    ASSERT_EQ(record->sidechain_entries.size(), 1u);
    EXPECT_NE(record->sidechain_entries.front().find(R"("toolUseId":"bash-40k")"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolUsesGrowthBookToolResultThresholdOverrides) {
    auto root = fs::temp_directory_path() / "loom_agent_gb_threshold_override_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard override_guard(
        "LOOM_INTERNAL_FC_OVERRIDES",
        R"({"tengu_satin_quoll":{"Bash":50000,"Read":1}})");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "gb-threshold",
        .agent_type = "general-purpose",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto thresholds = loom::tools::agent::tool_result_budget_thresholds(registry.get_visible_definitions());
    ASSERT_TRUE(thresholds.contains("bash"));
    EXPECT_EQ(thresholds["bash"], 50'000u);
    EXPECT_FALSE(thresholds.contains("read"));

    std::string bash_result(40'000, 'b');
    std::vector<loom::services::api::Message> messages;
    loom::services::api::Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "bash-override-40k",
        .tool_name = "Bash",
        .tool_input_json = R"({"command":"cat mid.log"})",
    });
    messages.push_back(std::move(assistant));
    loom::services::api::Message result;
    result.role = "user";
    result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = bash_result,
        .tool_use_id = "bash-override-40k",
    });
    messages.push_back(std::move(result));

    loom::tools::agent::AgentContentReplacementState state;
    auto replaced = loom::tools::agent::apply_agent_tool_result_budget(
        "gb-threshold",
        messages,
        state,
        loom::tools::agent::unbounded_tool_result_budget_names(registry.get_visible_definitions()),
        thresholds);
    EXPECT_EQ(replaced.newly_replaced, 0u);
    EXPECT_EQ(messages[1].content[0].text, bash_result);
    EXPECT_TRUE(state.seen_ids.contains("bash-override-40k"));
    EXPECT_FALSE(fs::exists(root / "runtime" / "tool-results" / "gb-threshold-bash-override-40k.txt"));

    auto record = loom::tools::agent_runtime::native_agent_store().get("gb-threshold");
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(record->sidechain_entries.empty());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolUsesGrowthBookAggregateBudgetOverride) {
    auto root = fs::temp_directory_path() / "loom_agent_gb_aggregate_override_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard override_guard("LOOM_INTERNAL_FC_OVERRIDES", R"({"tengu_hawthorn_window":10000})");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "gb-aggregate",
        .agent_type = "general-purpose",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    EXPECT_EQ(loom::tools::agent::agent_per_message_budget_limit(), 10'000u);
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto thresholds = loom::tools::agent::tool_result_budget_thresholds(registry.get_visible_definitions());
    ASSERT_TRUE(thresholds.contains("grep"));
    EXPECT_EQ(thresholds["grep"], 20'000u);

    std::string larger_result(8'000, 'g');
    std::string smaller_result(7'000, 'h');
    std::vector<loom::services::api::Message> messages;
    loom::services::api::Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "grep-8k",
        .tool_name = "Grep",
        .tool_input_json = R"({"pattern":"g"})",
    });
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "grep-7k",
        .tool_name = "Grep",
        .tool_input_json = R"({"pattern":"h"})",
    });
    messages.push_back(std::move(assistant));
    loom::services::api::Message result;
    result.role = "user";
    result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = larger_result,
        .tool_use_id = "grep-8k",
    });
    result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = smaller_result,
        .tool_use_id = "grep-7k",
    });
    messages.push_back(std::move(result));

    loom::tools::agent::AgentContentReplacementState state;
    auto replaced = loom::tools::agent::apply_agent_tool_result_budget(
        "gb-aggregate",
        messages,
        state,
        loom::tools::agent::unbounded_tool_result_budget_names(registry.get_visible_definitions()),
        thresholds);
    EXPECT_EQ(replaced.newly_replaced, 1u);
    EXPECT_NE(messages[1].content[0].text.find("<persisted-output>"), std::string::npos);
    EXPECT_EQ(messages[1].content[1].text, smaller_result);
    EXPECT_TRUE(fs::exists(root / "runtime" / "tool-results" / "gb-aggregate-grep-8k.txt"));
    EXPECT_FALSE(fs::exists(root / "runtime" / "tool-results" / "gb-aggregate-grep-7k.txt"));

    auto record = loom::tools::agent_runtime::native_agent_store().get("gb-aggregate");
    ASSERT_TRUE(record.has_value());
    ASSERT_EQ(record->sidechain_entries.size(), 1u);
    EXPECT_NE(record->sidechain_entries.front().find(R"("toolUseId":"grep-8k")"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeForkAddsDirectiveWorktreeNoticeAndMetadata) {
    auto root = fs::temp_directory_path() / "loom_agent_runtime_fork_worktree_test";
    fs::remove_all(root);
    fs::create_directories(root / "parent");
    fs::create_directories(root / "worktree");
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "fork-parent",
        .agent_type = "runtime",
        .cwd = (root / "parent").string(),
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .capabilities = {"Read", "Bash"},
        .transcript = {"user: parent context", "assistant: parent result"},
    });
    loom::tools::agent_runtime::native_agent_store().append_sidechain_message(
        "fork-parent",
        "assistant",
        R"([{"type":"text","text":"parent inspected file"},{"type":"tool_use","id":"fork-parent-tool-1","name":"Read","input":{"file_path":"README.md"}}])",
        "parent inspected file");
    loom::tools::agent_runtime::native_agent_store().append_sidechain_message(
        "fork-parent",
        "user",
        R"([{"type":"tool_result","tool_use_id":"fork-parent-tool-1","content":[{"type":"text","text":"README content"}]}])",
        "README content");
    auto parent_record = loom::tools::agent_runtime::native_agent_store().get("fork-parent");
    ASSERT_TRUE(parent_record.has_value());
    parent_record->sidechain_entries.push_back(
        R"({"type":"content-replacement","sessionId":"session-1","agentId":"fork-parent","replacements":[{"kind":"tool-result","toolUseId":"fork-parent-tool-1","replacement":"[persisted parent preview]"}]})");
    loom::tools::agent_runtime::native_agent_store().upsert(std::move(*parent_record));

    loom::tools::agent_runtime::AgentRuntimeConfig child_config{
        .agent_id = "fork-child",
        .working_dir = (root / "worktree").string(),
        .capabilities = {"Read"},
        .worktree_path = (root / "worktree").string(),
        .worktree_branch = "loom-agent-fork-child",
        .worktree_base_commit = "base-commit",
        .worktree_git_root = root.string(),
        .fork_directive = "Inspect only the parser migration",
        .allow_fork = true,
    };
    auto child = loom::tools::agent_runtime::fork_subagent("fork-parent", child_config);
    ASSERT_TRUE(child.has_value()) << child.error();
    EXPECT_EQ(*child, "fork-child");

    auto child_record = loom::tools::agent_runtime::native_agent_store().get("fork-child");
    ASSERT_TRUE(child_record.has_value());
    EXPECT_TRUE(std::ranges::contains(child_record->capabilities, "fork-subagent"));
    ASSERT_TRUE(child_record->worktree_path.has_value());
    EXPECT_EQ(*child_record->worktree_path, (root / "worktree").string());
    ASSERT_TRUE(child_record->worktree_branch.has_value());
    EXPECT_EQ(*child_record->worktree_branch, "loom-agent-fork-child");
    ASSERT_FALSE(child_record->transcript.empty());
    EXPECT_TRUE(std::ranges::any_of(child_record->transcript, [](const auto& line) {
        return line.find("<fork-boilerplate>") != std::string::npos &&
            line.find("Your directive: Inspect only the parser migration") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(child_record->transcript, [](const auto& line) {
        return line.find("translate them to your worktree root") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(child_record->transcript, [](const auto& line) {
        return line.find("[tool_use:Read]") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(child_record->transcript, [](const auto& line) {
        return line.find("tool_result: README content") != std::string::npos;
    }));
    ASSERT_GE(child_record->sidechain_entries.size(), 4u);
    ASSERT_TRUE(child_record->sidechain_jsonl_path.has_value());
    ASSERT_TRUE(fs::exists(*child_record->sidechain_jsonl_path));

    std::ifstream sidechain_in(*child_record->sidechain_jsonl_path);
    std::string sidechain_text(
        (std::istreambuf_iterator<char>(sidechain_in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(sidechain_text.find(R"("agentId":"fork-child")"), std::string::npos);
    EXPECT_EQ(sidechain_text.find(R"("agentId":"fork-parent")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"tool_use")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("id":"fork-parent-tool-1")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("name":"Read")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"tool_result")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("tool_use_id":"fork-parent-tool-1")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"content-replacement")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("replacement":"[persisted parent preview]")"), std::string::npos);
    EXPECT_NE(sidechain_text.find("Your directive: Inspect only the parser migration"), std::string::npos);
    auto child_resume_messages = loom::tools::agent::resume_messages_from_sidechain_entries(child_record->sidechain_entries);
    ASSERT_GE(child_resume_messages.size(), 3u);
    auto child_tool_result = std::ranges::find_if(child_resume_messages, [](const auto& message) {
        return message.role == "user" &&
            std::ranges::any_of(message.content, [](const auto& block) {
                return block.type == loom::services::api::ContentBlockType::ToolResult &&
                    block.tool_use_id == "fork-parent-tool-1" &&
                    block.text == "[persisted parent preview]";
            });
    });
    EXPECT_NE(child_tool_result, child_resume_messages.end());

    loom::tools::agent_runtime::AgentRuntimeConfig recursive_config{
        .agent_id = "fork-grandchild",
        .working_dir = (root / "worktree").string(),
        .capabilities = {"Read"},
        .fork_directive = "Try to fork recursively",
        .allow_fork = true,
    };
    auto recursive = loom::tools::agent_runtime::fork_subagent("fork-child", recursive_config);
    ASSERT_FALSE(recursive.has_value());
    EXPECT_NE(recursive.error().find("Fork is not available inside a forked worker"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored_child = loom::tools::agent_runtime::native_agent_store().get("fork-child");
    ASSERT_TRUE(restored_child.has_value());
    ASSERT_GE(restored_child->sidechain_entries.size(), 4u);
    EXPECT_TRUE(std::ranges::any_of(restored_child->transcript, [](const auto& line) {
        return line.find("[tool_use:Read]") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeForkAddsPlaceholderToolResultsForUnresolvedToolUses) {
    auto root = fs::temp_directory_path() / "loom_agent_runtime_fork_placeholder_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "fork-placeholder-parent",
        .agent_type = "runtime",
        .cwd = root.string(),
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
        .capabilities = {"Read"},
        .transcript = {"user: parent context"},
    });
    loom::tools::agent_runtime::native_agent_store().append_sidechain_message(
        "fork-placeholder-parent",
        "assistant",
        R"([{"type":"text","text":"about to read"},{"type":"tool_use","id":"unresolved-read-1","name":"Read","input":{"file_path":"README.md"}}])",
        "about to read");

    loom::tools::agent_runtime::AgentRuntimeConfig child_config{
        .agent_id = "fork-placeholder-child",
        .working_dir = root.string(),
        .capabilities = {"Read"},
        .fork_directive = "Continue without waiting for the read result",
        .allow_fork = true,
    };
    auto child = loom::tools::agent_runtime::fork_subagent("fork-placeholder-parent", child_config);
    ASSERT_TRUE(child.has_value()) << child.error();

    auto child_record = loom::tools::agent_runtime::native_agent_store().get("fork-placeholder-child");
    ASSERT_TRUE(child_record.has_value());
    ASSERT_TRUE(child_record->sidechain_jsonl_path.has_value());
    std::ifstream sidechain_in(*child_record->sidechain_jsonl_path);
    std::string sidechain_text(
        (std::istreambuf_iterator<char>(sidechain_in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(sidechain_text.find(R"("agentId":"fork-placeholder-child")"), std::string::npos);
    EXPECT_EQ(sidechain_text.find(R"("agentId":"fork-placeholder-parent")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"tool_use")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"tool_result")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("tool_use_id":"unresolved-read-1")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"(Fork started \u2014 processing in background)"), std::string::npos);
    EXPECT_NE(sidechain_text.find("Your directive: Continue without waiting for the read result"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(child_record->transcript, [](const auto& line) {
        return line.find("tool_result: Fork started") != std::string::npos &&
            line.find("Your directive: Continue without waiting for the read result") != std::string::npos;
    }));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeResumeTouchesExistingWorktreeAndFallsBackWhenMissing) {
    auto root = fs::temp_directory_path() / "loom_agent_runtime_resume_worktree_test";
    fs::remove_all(root);
    fs::create_directories(root / "existing-worktree");
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto old_time = fs::file_time_type::clock::now() - std::chrono::hours(2);
    std::error_code ec;
    fs::last_write_time(root / "existing-worktree", old_time, ec);
    ASSERT_FALSE(ec);
    auto before = fs::last_write_time(root / "existing-worktree");

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "resume-existing",
        .agent_type = "runtime",
        .cwd = (root / "existing-worktree").string(),
        .isolation = "worktree",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .worktree_path = (root / "existing-worktree").string(),
        .worktree_branch = "loom-agent-resume-existing",
        .transcript = {"user: existing worktree"},
    });
    auto resumed_existing = loom::tools::agent_runtime::resume_agent("resume-existing");
    ASSERT_TRUE(resumed_existing.has_value()) << resumed_existing.error();
    auto after = fs::last_write_time(root / "existing-worktree");
    EXPECT_GT(after, before);
    auto existing_record = loom::tools::agent_runtime::native_agent_store().get("resume-existing");
    ASSERT_TRUE(existing_record.has_value());
    EXPECT_TRUE(existing_record->worktree_path.has_value());

    auto missing = root / "missing-worktree";
    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "resume-missing",
        .agent_type = "runtime",
        .cwd = missing.string(),
        .isolation = "worktree",
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .worktree_path = missing.string(),
        .worktree_branch = "loom-agent-resume-missing",
        .transcript = {"user: missing worktree"},
    });
    auto resumed_missing = loom::tools::agent_runtime::resume_agent("resume-missing");
    ASSERT_TRUE(resumed_missing.has_value()) << resumed_missing.error();
    EXPECT_TRUE(std::ranges::any_of(resumed_missing->transcript, [](const auto& line) {
        return line.find("falling back to parent cwd") != std::string::npos;
    }));
    auto missing_record = loom::tools::agent_runtime::native_agent_store().get("resume-missing");
    ASSERT_TRUE(missing_record.has_value());
    EXPECT_TRUE(missing_record->worktree_cleanup_performed);
    EXPECT_FALSE(missing_record->worktree_path.has_value());
    EXPECT_FALSE(missing_record->cwd.has_value());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentRuntimeTracksLifecycleForkAndResume) {
    auto root = fs::temp_directory_path() / "loom_agent_runtime_lifecycle_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::AgentRuntimeConfig parent_config{
        .agent_id = "runtime-parent",
        .working_dir = root.string(),
        .capabilities = {"Read", "Bash"},
    };
    auto parent = loom::tools::agent_runtime::run_agent(parent_config);
    ASSERT_TRUE(parent.has_value()) << parent.error();
    EXPECT_EQ(parent->agent_id, "runtime-parent");
    EXPECT_EQ(parent->exit_code, 0);
    EXPECT_NE(parent->output.find(root.string()), std::string::npos);
    EXPECT_EQ(
        loom::tools::agent_runtime::get_agent_lifecycle("runtime-parent"),
        loom::tools::agent_runtime::AgentLifecycle::Completed);
    EXPECT_TRUE(fs::exists(root / "runtime" / "runtime-parent.json"));
    EXPECT_TRUE(fs::exists(root / "runtime" / "runtime-parent.transcript"));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored_parent = loom::tools::agent_runtime::resume_agent("runtime-parent");
    ASSERT_TRUE(restored_parent.has_value()) << restored_parent.error();
    EXPECT_EQ(restored_parent->agent_id, "runtime-parent");
    EXPECT_FALSE(restored_parent->transcript.empty());
    EXPECT_EQ(
        loom::tools::agent_runtime::get_agent_lifecycle("runtime-parent"),
        loom::tools::agent_runtime::AgentLifecycle::Completed);

    loom::tools::agent_runtime::AgentRuntimeConfig child_config{
        .agent_id = "runtime-child",
        .working_dir = root.string(),
        .capabilities = {"Read"},
        .allow_fork = true,
    };
    auto child = loom::tools::agent_runtime::fork_subagent("runtime-parent", child_config);
    ASSERT_TRUE(child.has_value()) << child.error();
    EXPECT_EQ(*child, "runtime-child");

    auto child_record = loom::tools::agent_runtime::native_agent_store().get("runtime-child");
    ASSERT_TRUE(child_record.has_value());
    ASSERT_TRUE(child_record->parent_agent_id.has_value());
    EXPECT_EQ(*child_record->parent_agent_id, "runtime-parent");
    EXPECT_EQ(child_record->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
    EXPECT_EQ(
        loom::tools::agent_runtime::get_agent_lifecycle("runtime-child"),
        loom::tools::agent_runtime::AgentLifecycle::Starting);

    auto resumed = loom::tools::agent_runtime::resume_agent("runtime-child");
    ASSERT_TRUE(resumed.has_value()) << resumed.error();
    EXPECT_EQ(resumed->agent_id, "runtime-child");
    EXPECT_NE(resumed->output.find("queued"), std::string::npos);
    ASSERT_FALSE(resumed->transcript.empty());
    EXPECT_TRUE(std::ranges::any_of(resumed->transcript, [](const auto& line) {
        return line.find("runtime-parent") != std::string::npos;
    }));
    EXPECT_EQ(resumed->transcript.back(), "system: forked from runtime-parent");

    loom::tools::agent_runtime::AgentRuntimeConfig grandchild_config{
        .agent_id = "runtime-grandchild",
        .working_dir = root.string(),
        .capabilities = {"Read"},
        .allow_fork = true,
    };
    auto recursive_child = loom::tools::agent_runtime::fork_subagent("runtime-child", grandchild_config);
    ASSERT_FALSE(recursive_child.has_value());
    EXPECT_NE(recursive_child.error().find("Fork is not available inside a forked worker"), std::string::npos);

    ASSERT_FALSE(parent->transcript.empty());
    auto parent_record = loom::tools::agent_runtime::native_agent_store().get("runtime-parent");
    ASSERT_TRUE(parent_record.has_value());
    ASSERT_TRUE(parent_record->progress.has_value());
    EXPECT_DOUBLE_EQ(*parent_record->progress, 1.0);

    loom::tools::agent_runtime::native_agent_store().request_cancel("runtime-child", "test cancel");
    EXPECT_EQ(
        loom::tools::agent_runtime::get_agent_lifecycle("runtime-child"),
        loom::tools::agent_runtime::AgentLifecycle::Cancelled);
    auto cancelled = loom::tools::agent_runtime::resume_agent("runtime-child");
    ASSERT_TRUE(cancelled.has_value()) << cancelled.error();
    EXPECT_EQ(cancelled->exit_code, 130);
    ASSERT_TRUE(cancelled->error.has_value());
    EXPECT_EQ(*cancelled->error, "test cancel");

    child_config.allow_fork = false;
    auto denied = loom::tools::agent_runtime::fork_subagent("runtime-parent", child_config);
    EXPECT_FALSE(denied.has_value());

    auto missing = loom::tools::agent_runtime::resume_agent("missing-agent");
    EXPECT_FALSE(missing.has_value());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, AgentToolBackgroundAgentCwdIsScopedPerToolWithoutChangingProcessCwd) {
    auto root = fs::temp_directory_path() / "loom_agent_cwd_isolation_test";
    fs::remove_all(root);
    fs::create_directories(root / "agent-a");
    fs::create_directories(root / "agent-b");
    const auto original_cwd = fs::current_path();
    LocalPerTurnBashPwdMessagesServer server;
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "cwd-isolation-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);

    auto first = tool.execute(loom::core::ToolInput::from_json(std::format(R"({{
      "description": "Run pwd in agent A",
      "prompt": "Run pwd",
      "agent_id": "cwd-agent-a",
      "run_in_background": true,
      "cwd": "{}"
    }})", loom::tools::agent::json_escape_string((root / "agent-a").string()))));
    ASSERT_TRUE(first.has_value()) << first.error().format();
    ASSERT_FALSE(first->is_error);

    auto second = tool.execute(loom::core::ToolInput::from_json(std::format(R"({{
      "description": "Run pwd in agent B",
      "prompt": "Run pwd",
      "agent_id": "cwd-agent-b",
      "run_in_background": true,
      "cwd": "{}"
    }})", loom::tools::agent::json_escape_string((root / "agent-b").string()))));
    ASSERT_TRUE(second.has_value()) << second.error().format();
    ASSERT_FALSE(second->is_error);

    ASSERT_TRUE(server.wait_for_request_count(4));
    EXPECT_TRUE(wait_for_native_agent_status(
        "cwd-agent-a",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));
    EXPECT_TRUE(wait_for_native_agent_status(
        "cwd-agent-b",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));

    auto agent_a = loom::tools::agent_runtime::native_agent_store().get("cwd-agent-a");
    auto agent_b = loom::tools::agent_runtime::native_agent_store().get("cwd-agent-b");
    ASSERT_TRUE(agent_a.has_value());
    ASSERT_TRUE(agent_b.has_value());
    EXPECT_TRUE(std::ranges::any_of(agent_a->transcript, [&](const auto& entry) {
        return entry.find((root / "agent-a").string()) != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(agent_b->transcript, [&](const auto& entry) {
        return entry.find((root / "agent-b").string()) != std::string::npos;
    }));

    std::error_code cwd_error;
    const auto cwd_after_agents = fs::current_path(cwd_error);
    EXPECT_FALSE(cwd_error);
    if (!cwd_error) {
        EXPECT_EQ(cwd_after_agents, original_cwd);
    }
    fs::current_path(original_cwd, cwd_error);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}
