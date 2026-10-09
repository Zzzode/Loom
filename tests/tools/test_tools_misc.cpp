/// @file test_tools_misc.cpp
/// @brief Miscellaneous tool tests (registry, MCP, skills, tasks, teams, etc.).

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

// RAII guard that closes a persistent REPL session on scope exit.
struct ReplSessionGuard {
    std::string id;
    explicit ReplSessionGuard(std::string s) : id(std::move(s)) {}
    ~ReplSessionGuard() {
        try { loom::tools::repl::close_session(id); } catch (...) {}
    }
    ReplSessionGuard(const ReplSessionGuard&) = delete;
    ReplSessionGuard& operator=(const ReplSessionGuard&) = delete;
};

// Helpers for the SkillTool smoke tests: a self-cleaning skill root directory
// plus a small response parser that returns the parsed doc and its root view.
namespace skill_test {

struct TempSkillRoot {
    fs::path path;
    /// The directory the SkillTool scans for skills (== path / "skills").
    fs::path skills_dir;
    /// The simulated HOME root; skills live under "<temp_home>/.loom/skills".
    /// Path-traversal tests drop a sibling file directly in temp_home to
    /// verify the loader rejects escaping the skill root.
    fs::path temp_home;
    /// Previous HOME env var, restored on destruction.
    std::optional<std::string> prev_home;

    TempSkillRoot() {
        auto base = fs::temp_directory_path();
        std::ostringstream ss;
        ss << "cc-skill-test-" << std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        temp_home = base / ss.str();
        skills_dir = temp_home / ".loom" / "skills";
        path = skills_dir;  // primary `path` alias used by write_skill
        fs::create_directories(skills_dir);
        // Set HOME so skill_root_dirs() discovers the temp skills dir.
        if (const char* existing = std::getenv("HOME")) {
            prev_home = existing;
        }
        setenv("HOME", temp_home.string().c_str(), 1);
    }
    ~TempSkillRoot() {
        if (prev_home) {
            setenv("HOME", prev_home->c_str(), 1);
        } else {
            unsetenv("HOME");
        }
        std::error_code ec;
        fs::remove_all(temp_home, ec);
    }
    TempSkillRoot(const TempSkillRoot&) = delete;
    TempSkillRoot& operator=(const TempSkillRoot&) = delete;

    void write_skill(const std::string& rel, std::string_view content) const {
        std::ofstream f{path / rel, std::ios::binary | std::ios::trunc};
        f << content;
    }

    // SkillTool resolves skill paths relative to the cwd, so tests chdir into
    // the temp root before invoking it.  Callers that need this should wrap
    // their test body in a CurrentPathGuard(path).
};

[[nodiscard]] auto parse_resp(const std::string& json_str)
    -> std::pair<loom::utils::json::JsonDoc, loom::utils::json::JsonVal> {
    auto doc = loom::utils::json::JsonDoc{};
    auto parsed = loom::utils::json::parse(json_str);
    if (parsed) doc = std::move(*parsed);
    return {std::move(doc), doc.root()};
}

}  // namespace skill_test

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

struct LocalBridgeIngressRequest {
    std::string target_session_id;
    std::string authorization;
    std::string cookie;
    std::string organization_uuid;
    std::string body;
};

class LocalBridgeIngressServer {
public:
    LocalBridgeIngressServer() {
        server_.Post(R"(/v1/sessions/([^/]+)/events)", [&](const httplib::Request& req, httplib::Response& res) {
            LocalBridgeIngressRequest request;
            request.target_session_id = req.matches.size() > 1 ? req.matches[1].str() : std::string{};
            request.authorization = req.get_header_value("Authorization");
            request.cookie = req.get_header_value("Cookie");
            request.organization_uuid = req.get_header_value("X-Organization-Uuid");
            request.body = req.body;
            {
                std::lock_guard lock(mutex_);
                requests_.push_back(std::move(request));
            }
            cv_.notify_all();
            res.status = 201;
            res.set_content(R"({"ok":true})", "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ > 0) {
            worker_ = std::jthread([this](std::stop_token) {
                server_.listen_after_bind();
            });
        }
    }

    ~LocalBridgeIngressServer() {
        server_.stop();
        if (worker_.joinable()) worker_.join();
    }

    [[nodiscard]] bool ready() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string base_url() const {
        return std::format("http://127.0.0.1:{}", port_);
    }

    [[nodiscard]] std::optional<std::vector<LocalBridgeIngressRequest>> wait_for_requests(
        std::size_t count,
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this, count] { return requests_.size() >= count; })) {
            return std::nullopt;
        }
        return requests_;
    }

private:
    httplib::Server server_;
    int port_{0};
    std::jthread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<LocalBridgeIngressRequest> requests_;
};

#ifndef _WIN32
class LocalUnixLineServer {
public:
    explicit LocalUnixLineServer(fs::path path) : path_(std::move(path)) {
        fs::remove(path_);
        listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) return;

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const auto path_text = path_.string();
        if (path_text.size() >= sizeof(addr.sun_path)) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }
        std::memcpy(addr.sun_path, path_text.data(), path_text.size());
        addr.sun_path[path_text.size()] = '\0';

        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }
        if (::listen(listen_fd_, 4) != 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            return;
        }

        worker_ = std::jthread([this](std::stop_token stop) {
            run(stop);
        });
    }

    ~LocalUnixLineServer() {
        if (worker_.joinable()) worker_.request_stop();
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
            listen_fd_ = -1;
        }
        fs::remove(path_);
    }

    [[nodiscard]] bool valid() const {
        return listen_fd_ >= 0;
    }

    [[nodiscard]] std::optional<std::string> wait_for_message(
        std::chrono::milliseconds timeout = std::chrono::seconds(3)
    ) {
        std::unique_lock lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !messages_.empty(); })) {
            return std::nullopt;
        }
        return messages_.front();
    }

private:
    void run(std::stop_token stop) {
        while (!stop.stop_requested()) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(listen_fd_, &fds);
            timeval timeout{0, 100'000};
            const int ready = ::select(listen_fd_ + 1, &fds, nullptr, nullptr, &timeout);
            if (ready <= 0) continue;

            const int client_fd = ::accept(listen_fd_, nullptr, nullptr);
            if (client_fd < 0) continue;
            std::string payload;
            std::array<char, 1024> buffer{};
            while (true) {
                const auto n = ::recv(client_fd, buffer.data(), buffer.size(), 0);
                if (n <= 0) break;
                payload.append(buffer.data(), static_cast<std::size_t>(n));
            }
            ::close(client_fd);
            {
                std::lock_guard lock(mutex_);
                messages_.push_back(std::move(payload));
            }
            cv_.notify_all();
            break;
        }
    }

    fs::path path_;
    int listen_fd_{-1};
    std::jthread worker_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> messages_;
};
#endif

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

// RFC-0001 B4: the core-settings MCP loader is a process-global function-local
// static; every test that installs one must clear it so later cases stay
// hermetic (no real-HOME ConfigManager reads, no detached connect threads).
struct CoreSettingsMcpLoaderGuard {
    ~CoreSettingsMcpLoaderGuard() {
        loom::tools::set_core_settings_mcp_loader(nullptr);
        (void)loom::tools::sync_native_mcp_servers({});
    }
};

struct McpSnapshotsSinkGuard {
    ~McpSnapshotsSinkGuard() {
        loom::tools::set_mcp_snapshots_sink(nullptr);
        (void)loom::tools::sync_native_mcp_servers({});
    }
};

std::optional<std::string> extract_background_task_id(std::string_view text) {
    constexpr std::string_view marker = "Task ID: ";
    const auto start = text.find(marker);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    const auto value_start = start + marker.size();
    const auto value_end = text.find_first_of("\r\n", value_start);
    return std::string(text.substr(value_start, value_end == std::string_view::npos
        ? std::string_view::npos
        : value_end - value_start));
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::optional<std::string> extract_background_pid(std::string_view text) {
    constexpr std::string_view marker = "PID: ";
    const auto start = text.find(marker);
    if (start == std::string_view::npos) {
        return std::nullopt;
    }
    const auto value_start = start + marker.size();
    const auto value_end = text.find_first_of("\r\n", value_start);
    return std::string(text.substr(value_start, value_end == std::string_view::npos
        ? std::string_view::npos
        : value_end - value_start));
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

class LocalSleepToolUseMessagesServer {
public:
    LocalSleepToolUseMessagesServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            std::size_t count = 0;
            {
                std::lock_guard lock(mutex_);
                count = ++request_count_;
                last_body_ = req.body;
            }
            cv_.notify_all();

            if (count == 1) {
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_sleep_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_sleep\",\"name\":\"sleep\",\"input\":{}}}\n\n"
                    "event: content_block_delta\n"
                    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"duration\\\":5,\\\"reason\\\":\\\"wait for cancellation\\\"}\"}}\n\n"
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
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_after_sleep\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"sleep finished\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":2}}\n\n"
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

    ~LocalSleepToolUseMessagesServer() {
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

    [[nodiscard]] std::size_t request_count() const {
        std::lock_guard lock(mutex_);
        return request_count_;
    }

    [[nodiscard]] std::optional<std::string> last_body() const {
        std::lock_guard lock(mutex_);
        return last_body_;
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::optional<std::string> last_body_;
};

class LocalBashToolUseMessagesServer {
public:
    LocalBashToolUseMessagesServer() {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            std::size_t count = 0;
            {
                std::lock_guard lock(mutex_);
                count = ++request_count_;
                last_body_ = req.body;
            }
            cv_.notify_all();

            if (count == 1) {
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_bash_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_bash\",\"name\":\"Bash\",\"input\":{}}}\n\n"
                    "event: content_block_delta\n"
                    "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"input_json_delta\",\"partial_json\":\"{\\\"command\\\":\\\"trap 'printf cancelled; exit 0' TERM; printf started; sleep 5; printf done\\\",\\\"description\\\":\\\"wait for cancellation\\\"}\"}}\n\n"
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
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_after_bash\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                "event: content_block_start\n"
                "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"text\",\"text\":\"\"}}\n\n"
                "event: content_block_delta\n"
                "data: {\"type\":\"content_block_delta\",\"index\":0,\"delta\":{\"type\":\"text_delta\",\"text\":\"bash finished\"}}\n\n"
                "event: content_block_stop\n"
                "data: {\"type\":\"content_block_stop\",\"index\":0}\n\n"
                "event: message_delta\n"
                "data: {\"type\":\"message_delta\",\"delta\":{\"stop_reason\":\"end_turn\",\"stop_sequence\":null},\"usage\":{\"output_tokens\":2}}\n\n"
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

    ~LocalBashToolUseMessagesServer() {
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

    [[nodiscard]] std::size_t request_count() const {
        std::lock_guard lock(mutex_);
        return request_count_;
    }

    [[nodiscard]] std::optional<std::string> last_body() const {
        std::lock_guard lock(mutex_);
        return last_body_;
    }

private:
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
    std::optional<std::string> last_body_;
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

class LocalPerTurnBashCommandMessagesServer {
public:
    explicit LocalPerTurnBashCommandMessagesServer(
        std::string command,
        std::string final_text = "bash command complete"
    ) : command_(std::move(command)), final_text_(std::move(final_text)) {
        server_.Post("/v1/messages", [&](const httplib::Request& req, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
                request_bodies_.push_back(req.body);
            }
            cv_.notify_all();

            const bool has_tool_result = req.body.find(R"("tool_result")") != std::string::npos;
            if (!has_tool_result) {
                const auto input_json = std::format(
                    R"({{"command":"{}","description":"run per-agent bash command"}})",
                    loom::tools::agent::json_escape_string(command_));
                const auto partial_json = loom::tools::agent::json_escape_string(input_json);
                res.set_content(
                    "event: message_start\n"
                    "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_bash_command_tool\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
                    "event: content_block_start\n"
                    "data: {\"type\":\"content_block_start\",\"index\":0,\"content_block\":{\"type\":\"tool_use\",\"id\":\"toolu_bash_command\",\"name\":\"Bash\",\"input\":{}}}\n\n"
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
                "data: {\"type\":\"message_start\",\"message\":{\"id\":\"msg_bash_command_done\",\"type\":\"message\",\"role\":\"assistant\",\"model\":\"loom-test\",\"content\":[]}}\n\n"
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

    ~LocalPerTurnBashCommandMessagesServer() {
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
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    ) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this, expected] { return request_count_ >= expected; });
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

class LocalSlowContentServer {
public:
    explicit LocalSlowContentServer(std::chrono::milliseconds delay)
        : delay_(delay) {
        server_.Get("/slow", [&](const httplib::Request&, httplib::Response& res) {
            {
                std::lock_guard lock(mutex_);
                ++request_count_;
            }
            cv_.notify_all();
            std::this_thread::sleep_for(delay_);
            res.set_content("slow web body", "text/plain");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        thread_ = std::thread([this] {
            server_.listen_after_bind();
        });
        server_.wait_until_ready();
    }

    ~LocalSlowContentServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    [[nodiscard]] bool valid() const {
        return port_ > 0;
    }

    [[nodiscard]] std::string url() const {
        return std::format("http://127.0.0.1:{}/slow", port_);
    }

    [[nodiscard]] bool wait_for_request(std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return request_count_ > 0; });
    }

private:
    std::chrono::milliseconds delay_;
    httplib::Server server_;
    int port_{0};
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::size_t request_count_{0};
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

TEST(ToolRegistry, ListsBuiltInTools) {
    auto names = loom::tools::registry::builtin_tool_names();
    EXPECT_FALSE(names.empty());
}

TEST(ToolRegistry, ContainsExpectedTools) {
    auto names = loom::tools::registry::builtin_tool_names();
    ASSERT_FALSE(names.empty());

    // Check that known tools are present
    bool has_bash = false;
    bool has_computer_use = false;
    bool has_lsp = false;
    bool has_skill = false;
    bool has_task_create = false;
    bool has_web_browser = false;
    for (const auto& name : names) {
        if (name == "Bash") has_bash = true;
        if (name == "computer_use") has_computer_use = true;
        if (name == "lsp") has_lsp = true;
        if (name == "skill") has_skill = true;
        if (name == "task_create") has_task_create = true;
        if (name == "web_browser") has_web_browser = true;
    }
    EXPECT_TRUE(has_bash);
    EXPECT_TRUE(has_computer_use);
    EXPECT_TRUE(has_lsp);
    EXPECT_TRUE(has_skill);
    EXPECT_TRUE(has_task_create);
    EXPECT_TRUE(has_web_browser);
}

TEST(ToolRegistry, CoreRegistryCanBeConstructed) {
    loom::tools::registry::ToolRegistry registry;
    EXPECT_EQ(registry.size(), 0u);  // Empty by default
}

TEST(ToolRegistry, RegistersRuntimeTools) {
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    EXPECT_GT(registry.size(), 0u);
    EXPECT_TRUE(registry.contains("Bash"));
    EXPECT_TRUE(registry.contains("computer_use"));
    EXPECT_TRUE(registry.contains("Read"));
    EXPECT_TRUE(registry.contains("web_browser"));
    EXPECT_TRUE(registry.contains("mcp"));
    EXPECT_TRUE(registry.contains("lsp"));
    EXPECT_TRUE(registry.contains("skill"));
    EXPECT_TRUE(registry.contains("task_create"));
}

TEST(ToolRegistry, RuntimeSimpleToolsHonorPermissionCheckOption) {
    std::vector<std::string> checked_tools;
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{
        .permission_check = [&checked_tools](
            std::string_view tool_name,
            std::string_view input_json,
            std::string_view tool_use_id
        ) {
            checked_tools.emplace_back(tool_name);
            EXPECT_FALSE(input_json.empty());
            EXPECT_TRUE(tool_use_id.empty());
            return loom::tools::AgentLivePermissionCheck{
                .allowed = tool_name != "testing",
                .message = std::string("blocked by test permission context"),
            };
        },
    });

    auto result = registry.execute("testing", loom::core::ToolInput::from_json(R"({"command":"unit"})"));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, loom::core::ErrorCode::ToolPermissionDenied);
    EXPECT_NE(result.error().message.find("testing"), std::string::npos);
    ASSERT_EQ(checked_tools.size(), 1u);
    EXPECT_EQ(checked_tools.front(), "testing");
}

TEST(Tools, RuntimeSimpleToolsFailClosedWithoutPermissionCheck) {
    // Regression test: when no live permission checker is supplied, runtime
    // tools must fail CLOSED for Write/Execute/Network operations. Previously
    // RuntimeFunctionTool::check_permission returned true unconditionally,
    // letting e.g. "script"/"repl"/"config"/"mcp" execute without any
    // permission gate. Read-only runtime tools remain allowed.
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry);  // fail-closed path under test

    const auto input = loom::core::ToolInput::from_json(R"({})");

    for (auto name : {"config", "script", "repl", "mcp", "notebook_edit", "powershell"}) {
        auto* tool = registry.get(name);
        ASSERT_NE(tool, nullptr) << name;
        EXPECT_FALSE(tool->check_permission(input))
            << "runtime tool '" << name << "' must fail-closed without a permission checker";
    }

    for (auto name : {"skill", "list_mcp_resources", "synthetic_output"}) {
        auto* tool = registry.get(name);
        ASSERT_NE(tool, nullptr) << name;
        EXPECT_TRUE(tool->check_permission(input))
            << "read-only runtime tool '" << name << "' should be allowed without a checker";
    }
}

TEST(Tools, LspToolUsesConfiguredLanguageServer) {
    auto root = fs::temp_directory_path() / "loom_runtime_lsp_tool_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom" / "plugins" / "lsp-runtime-fixture");
    const auto plugin_root = root / ".loom" / "plugins" / "lsp-runtime-fixture";
    const auto server_path = plugin_root / "server.js";
    const auto source_path = root / "sample.foo";
    {
        std::ofstream source(source_path);
        source << "function fixtureSymbol() { return fixtureCompletion; }\n";
    }
    {
        std::ofstream server(server_path);
        server << R"JS(
let buffer = Buffer.alloc(0);

function send(message) {
  const body = JSON.stringify(message);
  process.stdout.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
}

function position(line, character) {
  return { line, character };
}

function range(line, character) {
  return { start: position(line, character), end: position(line, character + 4) };
}

function handle(message) {
  const uri = message.params?.textDocument?.uri || 'file:///fixture';
  if (message.method === 'initialize') {
    send({ jsonrpc: '2.0', id: message.id, result: { capabilities: { textDocumentSync: 1 } } });
    return;
  }
  if (message.method === 'textDocument/didOpen') {
    send({
      jsonrpc: '2.0',
      method: 'textDocument/publishDiagnostics',
      params: {
        uri,
        diagnostics: [{
          range: range(1, 2),
          severity: 1,
          source: 'fixture',
          message: 'fixture diagnostic',
          code: 'F001'
        }]
      }
    });
    return;
  }
  if (message.method === 'textDocument/definition') {
    send({ jsonrpc: '2.0', id: message.id, result: [{ uri, range: range(7, 3) }] });
    return;
  }
  if (message.method === 'textDocument/references') {
    send({ jsonrpc: '2.0', id: message.id, result: [{ uri, range: range(8, 4) }] });
    return;
  }
  if (message.method === 'textDocument/completion') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { isIncomplete: false, items: [{ label: 'fixtureCompletion', kind: 3, detail: 'callable' }] }
    });
    return;
  }
  if (message.method === 'textDocument/hover') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { contents: { kind: 'markdown', value: 'fixture hover' }, range: range(2, 1) }
    });
    return;
  }
  if (message.method === 'textDocument/documentSymbol') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: [{
        name: 'fixtureSymbol',
        kind: 12,
        range: range(0, 9),
        selectionRange: range(0, 9)
      }]
    });
    return;
  }
  if (message.method === 'exit') process.exit(0);
}

process.stdin.on('data', chunk => {
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
    const headerEnd = buffer.indexOf('\r\n\r\n');
    if (headerEnd === -1) return;
    const header = buffer.subarray(0, headerEnd).toString();
    const match = /Content-Length:\s*(\d+)/i.exec(header);
    if (!match) process.exit(2);
    const length = Number(match[1]);
    const bodyStart = headerEnd + 4;
    if (buffer.length < bodyStart + length) return;
    const body = buffer.subarray(bodyStart, bodyStart + length).toString();
    buffer = buffer.subarray(bodyStart + length);
    handle(JSON.parse(body));
  }
});
process.stdin.resume();
)JS";
    }
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << R"JSON({
  "name": "lsp-runtime-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "lspServers": {
    "fixture": {
      "command": "node",
      "args": ["${LOOM_PLUGIN_ROOT}/server.js"],
      "extensionToLanguage": {".foo": "foo"}
    }
  }
})JSON";
    }

    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard plugin_cache_guard("LOOM_PLUGIN_CACHE_DIR", (root / ".loom" / "plugins").string());
    CurrentPathGuard cwd(root);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto execute_lsp = [&](std::string_view action) {
        loom::utils::json::JsonMutDoc doc;
        auto input = doc.object();
        input.add("action", doc.string(action));
        input.add("file_path", doc.string(source_path.string()));
        input.add("line", doc.number(static_cast<int64_t>(0)));
        input.add("character", doc.number(static_cast<int64_t>(9)));
        doc.set_root(input);
        return registry.execute("lsp", loom::core::ToolInput::from_json(doc.to_string()));
    };

    auto definition = execute_lsp("definition");
    ASSERT_TRUE(definition.has_value());
    EXPECT_FALSE(definition->is_error) << definition->content.front().text;
    EXPECT_NE(definition->content.front().text.find(":7:3"), std::string::npos);

    auto completion = execute_lsp("completion");
    ASSERT_TRUE(completion.has_value());
    EXPECT_FALSE(completion->is_error) << completion->content.front().text;
    EXPECT_NE(completion->content.front().text.find("fixtureCompletion callable"), std::string::npos);

    auto hover = execute_lsp("hover");
    ASSERT_TRUE(hover.has_value());
    EXPECT_FALSE(hover->is_error) << hover->content.front().text;
    EXPECT_NE(hover->content.front().text.find("fixture hover"), std::string::npos);

    auto symbols = execute_lsp("symbols");
    ASSERT_TRUE(symbols.has_value());
    EXPECT_FALSE(symbols->is_error) << symbols->content.front().text;
    EXPECT_NE(symbols->content.front().text.find("function fixtureSymbol"), std::string::npos);

    auto diagnostics = execute_lsp("diagnostics");
    ASSERT_TRUE(diagnostics.has_value());
    EXPECT_FALSE(diagnostics->is_error) << diagnostics->content.front().text;
    EXPECT_NE(diagnostics->content.front().text.find("fixture:1:2 fixture diagnostic"), std::string::npos);

    fs::remove_all(root);
}

TEST(ToolInput, HasFieldParsesTopLevelJsonKeys) {
    auto input = loom::core::ToolInput::from_json(R"({
      "cwd": null,
      "description": "command mentions timeout and nested_field",
      "nested": {"command": "pwd"}
    })");

    EXPECT_TRUE(loom::core::has_field(input, "cwd"));
    EXPECT_TRUE(loom::core::has_field(input, "description"));
    EXPECT_TRUE(loom::core::has_field(input, "nested"));
    EXPECT_FALSE(loom::core::has_field(input, "timeout"));
    EXPECT_FALSE(loom::core::has_field(input, "command"));
    EXPECT_FALSE(loom::core::has_field(input, "nested_field"));
    EXPECT_FALSE(loom::core::has_field(input, ""));
}

TEST(ToolInput, HasFieldReturnsFalseForInvalidOrNonObjectJson) {
    EXPECT_FALSE(loom::core::has_field(loom::core::ToolInput::from_json(R"("cwd")"), "cwd"));
    EXPECT_FALSE(loom::core::has_field(loom::core::ToolInput::from_json(R"(["cwd"])"), "cwd"));
    EXPECT_FALSE(loom::core::has_field(loom::core::ToolInput::from_json(R"({"cwd")"), "cwd"));
}

TEST(Tools, AgentShellTaskCleanupGuardStopsAgentOwnedBackgroundTasks) {
    loom::tools::BashTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "command": "trap 'printf stopped-by-agent; exit 0' TERM; printf guard-ready; sleep 5",
      "run_in_background": true,
      "agentId": "agent-cleanup-guard"
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    auto task_id = extract_background_task_id(result->content.front().text);
    ASSERT_TRUE(task_id.has_value()) << result->content.front().text;

    {
        loom::tools::agent::AgentShellTaskCleanupGuard guard{"agent-cleanup-guard"};
    }

    auto snapshot = loom::tools::bash::get_background_task_snapshot(*task_id);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_TRUE(snapshot->stopped);
    EXPECT_TRUE(snapshot->agent_id.has_value());
    EXPECT_EQ(*snapshot->agent_id, "agent-cleanup-guard");
}

TEST(Tools, TaskStopStopsBackgroundBashCommands) {
    loom::tools::BashTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "command": "trap 'printf stopped; exit 0' TERM; printf ready; sleep 5",
      "run_in_background": true
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->content.empty());
    auto task_id = extract_background_task_id(result->content.front().text);
    ASSERT_TRUE(task_id.has_value()) << result->content.front().text;

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    for (int attempt = 0; attempt < 20; ++attempt) {
        auto output = registry.execute("task_output", loom::core::ToolInput::from_json(
            std::format(R"({{"task_id":"{}"}})", *task_id)));
        ASSERT_TRUE(output.has_value());
        ASSERT_FALSE(output->content.empty());
        if (output->content.front().text.find("ready") != std::string::npos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(
        std::format(R"({{"task_id":"{}"}})", *task_id)));
    ASSERT_TRUE(stopped.has_value());
    EXPECT_FALSE(stopped->is_error);
    ASSERT_FALSE(stopped->content.empty());
    EXPECT_NE(stopped->content.front().text.find("Status: stopped"), std::string::npos);

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(
        std::format(R"({{"task_id":"{}"}})", *task_id)));
    ASSERT_TRUE(output.has_value());
    EXPECT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("Status: stopped"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("Output:"), std::string::npos);
    loom::tools::bash::drain_all_background_tasks();
}

TEST(Tools, TaskOutputAndStopAcceptBackgroundProcessPid) {
    loom::tools::BashTool tool;
    auto result = tool.execute(loom::core::ToolInput::from_json(R"({
      "command": "trap 'printf stopped-by-pid; exit 0' TERM; printf pid-ready; sleep 5",
      "run_in_background": true
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    auto pid = extract_background_pid(result->content.front().text);
    ASSERT_TRUE(pid.has_value()) << result->content.front().text;

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    std::string output_text;
    for (int attempt = 0; attempt < 20; ++attempt) {
        auto output = registry.execute("task_output", loom::core::ToolInput::from_json(
            std::format(R"({{"pid":{}}})", *pid)));
        ASSERT_TRUE(output.has_value());
        ASSERT_FALSE(output->is_error);
        ASSERT_FALSE(output->content.empty());
        output_text = output->content.front().text;
        if (output_text.find("pid-ready") != std::string::npos) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_NE(output_text.find("PID: " + *pid), std::string::npos);
    EXPECT_NE(output_text.find("pid-ready"), std::string::npos);

    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(
        std::format(R"({{"pid":{}}})", *pid)));
    ASSERT_TRUE(stopped.has_value());
    EXPECT_FALSE(stopped->is_error);
    ASSERT_FALSE(stopped->content.empty());
    EXPECT_NE(stopped->content.front().text.find("Status: stopped"), std::string::npos);

    auto final_output = registry.execute("task_output", loom::core::ToolInput::from_json(
        std::format(R"({{"pid":{}}})", *pid)));
    ASSERT_TRUE(final_output.has_value());
    EXPECT_FALSE(final_output->is_error);
    ASSERT_FALSE(final_output->content.empty());
    EXPECT_NE(final_output->content.front().text.find("Status: stopped"), std::string::npos);
    EXPECT_NE(final_output->content.front().text.find("Output:"), std::string::npos);
}

// RFC-0001 B12: with NO orchestration-installed SkillLoader executor, the
// 'skill' dispatch falls through to the terminal manual SKILL.md walk kept
// in loom_tools. This test deliberately instantiates NO FileToolServicesGuard
// (the process-global executor slot is unset), proving loom_tools stays
// self-contained for hermetic binaries.
TEST(Tools, SkillToolFallsBackToManualWalkWithoutExecutor) {
    // Distinctive body: pins that success comes from the manual SKILL.md walk
    // (which returns the file content), not a leaked executor returning nullopt
    // or an empty success.
    static constexpr std::string_view kManualWalkBody =
        "B12-MANUAL-WALK-MARKER: manual skill fallback body v1";
    auto root = fs::temp_directory_path() / "loom_skill_manual_walk_test";
    fs::remove_all(root);
    fs::create_directories(root / "skills" / "b12-manual-walk-skill");
    {
        std::ofstream skill(root / "skills" / "b12-manual-walk-skill" / "SKILL.md");
        skill << kManualWalkBody;
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry);

    CurrentPathGuard cwd(root);

    auto found = registry.execute("skill", loom::core::ToolInput::from_json(R"({
      "name": "b12-manual-walk-skill"
    })"));
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->is_error);
    ASSERT_FALSE(found->content.empty());
    // The simple resolver accepts any non-empty SKILL.md too; the body pin is
    // what proves the request was serviced from this exact fixture (a leaked
    // executor returning nullopt still lands in the same walk, but an empty or
    // different-content success can no longer satisfy the test).
    EXPECT_EQ(found->content.front().text, kManualWalkBody);

    auto missing = registry.execute("skill", loom::core::ToolInput::from_json(R"({
      "name": "b12-no-such-skill-fixture"
    })"));
    ASSERT_TRUE(missing.has_value());
    ASSERT_TRUE(missing->is_error);
    ASSERT_FALSE(missing->content.empty());
    EXPECT_NE(missing->content.front().text.find(
                  "Skill not found: b12-no-such-skill-fixture"),
              std::string::npos);

    auto unnamed = registry.execute("skill", loom::core::ToolInput::from_json(R"({})"));
    ASSERT_TRUE(unnamed.has_value());
    ASSERT_TRUE(unnamed->is_error);
    ASSERT_FALSE(unnamed->content.empty());
    EXPECT_NE(unnamed->content.front().text.find("skill requires name"),
              std::string::npos);

    fs::remove_all(root);
}

TEST(Tools, RuntimeRegistryEditToolEditsFileAndReturnsOutput) {
    auto root = fs::temp_directory_path() / "loom_runtime_registry_edit_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto path = root / "edit.txt";
    {
        std::ofstream out(path);
        out << "before edit";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    // Read the file first (required by Edit tool)
    auto read_result = registry.execute("Read", loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}"}})",
        loom::tools::agent::json_escape_string(path.string()))));
    ASSERT_TRUE(read_result.has_value());
    auto result = registry.execute("Edit", loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}","old_string":"before edit","new_string":"after edit"}})",
        loom::tools::agent::json_escape_string(path.string()))));

    ASSERT_TRUE(result.has_value()) << result.error().format();
    ASSERT_FALSE(result->content.empty());
    ASSERT_FALSE(result->is_error) << result->content.front().text;
    EXPECT_NE(result->content.front().text.find("has been updated"), std::string::npos);
    EXPECT_EQ(read_file(path), "after edit");

    fs::remove_all(root);
}

TEST(Tools, SwarmBackendsInProcessExecutorTracksActiveTeammates) {
    auto root = fs::temp_directory_path() / "loom_in_process_executor_mailbox_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard teammate_backend_guard("LOOM_TEAMMATE_BACKEND", "in-process");
    loom::utils::swarm_backends::BackendRegistry::reset();

    auto executor = loom::utils::swarm_backends::BackendRegistry::get_teammate_executor();
    ASSERT_TRUE(executor);
    EXPECT_EQ(executor->type(), loom::utils::swarm_backends::BackendType::InProcess);

    loom::utils::swarm_backends::TeammateSpawnConfig config{
        .name = "reviewer-one",
        .team_name = "migration-team",
        .color = std::nullopt,
        .plan_mode_required = false,
        .permission_mode = std::nullopt,
        .agent_type = std::nullopt,
        .prompt = "Review migration parity",
        .cwd = fs::current_path().string(),
        .model = std::nullopt,
        .system_prompt = std::nullopt,
        .system_prompt_mode = "default",
        .worktree_path = std::nullopt,
        .parent_session_id = "test-session",
        .permissions = {},
        .allow_permission_prompts = false,
    };
    auto spawned = executor->spawn(config);
    ASSERT_TRUE(spawned.success) << spawned.error.value_or("");
    EXPECT_EQ(spawned.agent_id, "reviewer-one@migration-team");
    ASSERT_TRUE(spawned.task_id.has_value());
    EXPECT_EQ(*spawned.task_id, "in-process:reviewer-one@migration-team");
    EXPECT_FALSE(spawned.pane_id.has_value());
    EXPECT_TRUE(executor->is_active("reviewer-one@migration-team"));

    executor->send_message(
        "reviewer-one@migration-team",
        loom::utils::swarm_backends::TeammateMessage{
            .text = "Please review the migration",
            .from = "team-lead",
            .color = std::optional<std::string>{"cyan"},
            .timestamp = std::nullopt,
            .summary = std::optional<std::string>{"review migration"},
        });
    auto inbox = loom::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"migration-team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().from, "team-lead");
    EXPECT_EQ(inbox->front().text, "Please review the migration");
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "review migration");

    EXPECT_TRUE(executor->terminate("reviewer-one@migration-team", "done"));
    EXPECT_TRUE(executor->is_active("reviewer-one@migration-team"));

    inbox = loom::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"migration-team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 2u);
    EXPECT_NE(inbox->back().text.find(R"("type":"shutdown_request")"), std::string::npos);
    EXPECT_NE(inbox->back().text.find(R"("reason":"done")"), std::string::npos);

    EXPECT_TRUE(executor->kill("reviewer-one@migration-team"));
    EXPECT_FALSE(executor->is_active("reviewer-one@migration-team"));

    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, SwarmBackendsPaneCommandPropagatesPermissionModeFlags) {
    EnvironmentGuard teammate_command_guard("LOOM_TEAMMATE_COMMAND", "/tmp/cc repl");

    loom::utils::swarm_backends::TeammateSpawnConfig config{
        .name = "reviewer-one",
        .team_name = "migration-team",
        .color = std::nullopt,
        .plan_mode_required = false,
        .permission_mode = std::optional<std::string>{"acceptEdits"},
        .agent_type = std::optional<std::string>{"verification"},
        .prompt = "Review migration parity",
        .cwd = "/tmp/cc repl worktree",
        .model = std::nullopt,
        .system_prompt = std::nullopt,
        .system_prompt_mode = "default",
        .worktree_path = std::nullopt,
        .parent_session_id = "test-session",
        .permissions = {},
        .allow_permission_prompts = false,
    };

    auto accept_edits = loom::utils::swarm_backends::detail::build_teammate_cli_command(config);
    EXPECT_NE(accept_edits.find("--permission-mode"), std::string::npos);
    EXPECT_NE(accept_edits.find("'acceptEdits'"), std::string::npos);
    EXPECT_NE(accept_edits.find("--agent-type"), std::string::npos);
    EXPECT_NE(accept_edits.find("'verification'"), std::string::npos);
    EXPECT_EQ(accept_edits.find("--dangerously-skip-permissions"), std::string::npos);

    config.permission_mode = "bypassPermissions";
    auto bypass = loom::utils::swarm_backends::detail::build_teammate_cli_command(config);
    EXPECT_NE(bypass.find("--dangerously-skip-permissions"), std::string::npos);
    EXPECT_EQ(bypass.find("--permission-mode"), std::string::npos);

    config.permission_mode = "auto";
    auto automatic = loom::utils::swarm_backends::detail::build_teammate_cli_command(config);
    EXPECT_NE(automatic.find("--permission-mode"), std::string::npos);
    EXPECT_NE(automatic.find("'auto'"), std::string::npos);
    EXPECT_EQ(automatic.find("--dangerously-skip-permissions"), std::string::npos);

    config.plan_mode_required = true;
    auto plan = loom::utils::swarm_backends::detail::build_teammate_cli_command(config);
    EXPECT_NE(plan.find("--plan-mode-required"), std::string::npos);
    EXPECT_EQ(plan.find("--permission-mode"), std::string::npos);
    EXPECT_EQ(plan.find("--dangerously-skip-permissions"), std::string::npos);
}

TEST(Tools, TeamHelpersResolveTeammateAgentTypeAndPlanMode) {
    struct ClearDynamicTeamContext {
        ~ClearDynamicTeamContext() {
            loom::utils::clear_dynamic_team_context();
        }
    } clear_dynamic_team_context;

    EnvironmentGuard agent_type_guard("LOOM_AGENT_TYPE", "verification");
    EnvironmentGuard plan_mode_guard("LOOM_PLAN_MODE_REQUIRED", "true");

    auto env_agent_type = loom::utils::get_agent_type();
    ASSERT_TRUE(env_agent_type.has_value());
    EXPECT_EQ(*env_agent_type, "verification");
    EXPECT_TRUE(loom::utils::is_plan_mode_required());

    loom::utils::set_dynamic_team_context(loom::utils::DynamicTeamContext{
        .agent_id = "reviewer-one@migration-team",
        .agent_name = "reviewer-one",
        .team_name = "migration-team",
        .agent_type = std::optional<std::string>{"Explore"},
        .color = "blue",
        .plan_mode_required = false,
        .parent_session_id = "leader-session",
    });

    auto dynamic_agent_type = loom::utils::get_agent_type();
    ASSERT_TRUE(dynamic_agent_type.has_value());
    EXPECT_EQ(*dynamic_agent_type, "Explore");
    EXPECT_FALSE(loom::utils::is_plan_mode_required());

    auto in_process_result = loom::utils::run_with_teammate_context(
        loom::utils::TeammateContext{
            .agent_id = "planner@migration-team",
            .agent_name = "planner",
            .team_name = "migration-team",
            .agent_type = std::optional<std::string>{"Plan"},
            .color = std::optional<std::string>{"green"},
            .plan_mode_required = true,
            .parent_session_id = "leader-session",
            .is_in_process = true,
        },
        [] {
            return std::pair{
                loom::utils::get_agent_type(),
                loom::utils::is_plan_mode_required(),
            };
        });
    ASSERT_TRUE(in_process_result.first.has_value());
    EXPECT_EQ(*in_process_result.first, "Plan");
    EXPECT_TRUE(in_process_result.second);
}

TEST(Tools, RuntimeSendMessageWritesNativeTeammateMailbox) {
    auto root = fs::temp_directory_path() / "loom_send_message_teammate_mailbox_test";
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
    auto spawned = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Spawn reviewer",
      "prompt": "Review migration parity",
      "name": "reviewer-one",
      "team_name": "migration-team",
      "subagent_type": "general-purpose"
    })"));
    ASSERT_TRUE(spawned.has_value());
    ASSERT_FALSE(spawned->is_error);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "reviewer-one",
      "team_name": "migration-team",
      "content": "Please review the parser migration",
      "summary": "review parser migration"
    })"));
    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("reviewer-one@migration-team"), std::string::npos);

    auto inbox = loom::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"migration-team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().from, "team-lead");
    EXPECT_EQ(inbox->front().text, "Please review the parser migration");
    EXPECT_FALSE(inbox->front().read);
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "review parser migration");
    EXPECT_TRUE(fs::exists(root / "teams" / "migration-team" / "inboxes" / "reviewer-one.json"));

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, RuntimeSendMessageAcceptsTsSchemaAndBroadcastsToTeamMailbox) {
    auto root = fs::temp_directory_path() / "loom_send_message_ts_broadcast_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto team = loom::tools::global_team_store().create(
        "broadcast-team-id",
        "Broadcast Team",
        {
            loom::tools::TeamMember{.agent_id = "team-lead@Broadcast Team"},
            loom::tools::TeamMember{.agent_id = "reviewer@Broadcast Team"},
            loom::tools::TeamMember{.agent_id = "planner@Broadcast Team"},
        });
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "*",
      "team_name": "Broadcast Team",
      "message": "Please sync on the migration audit",
      "summary": "migration audit sync"
    })"));
    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("Message broadcast to 2 teammate(s): reviewer, planner"), std::string::npos);

    auto reviewer_inbox = loom::utils::read_inbox("reviewer", std::optional<std::string_view>{"Broadcast Team"});
    ASSERT_TRUE(reviewer_inbox.has_value()) << reviewer_inbox.error();
    ASSERT_EQ(reviewer_inbox->size(), 1u);
    EXPECT_EQ(reviewer_inbox->front().from, "team-lead");
    EXPECT_EQ(reviewer_inbox->front().text, "Please sync on the migration audit");
    ASSERT_TRUE(reviewer_inbox->front().summary.has_value());
    EXPECT_EQ(*reviewer_inbox->front().summary, "migration audit sync");

    auto planner_inbox = loom::utils::read_inbox("planner", std::optional<std::string_view>{"Broadcast Team"});
    ASSERT_TRUE(planner_inbox.has_value()) << planner_inbox.error();
    ASSERT_EQ(planner_inbox->size(), 1u);
    EXPECT_EQ(planner_inbox->front().text, "Please sync on the migration audit");

    auto leader_inbox = loom::utils::read_inbox("team-lead", std::optional<std::string_view>{"Broadcast Team"});
    ASSERT_TRUE(leader_inbox.has_value()) << leader_inbox.error();
    EXPECT_TRUE(leader_inbox->empty());

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeSendMessageWritesStructuredTeamProtocolMessages) {
    auto root = fs::temp_directory_path() / "loom_send_message_structured_protocol_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto team = loom::tools::global_team_store().create(
        "protocol-team-id",
        "Protocol Team",
        {
            loom::tools::TeamMember{.agent_id = "team-lead@Protocol Team"},
            loom::tools::TeamMember{.agent_id = "reviewer@Protocol Team"},
            loom::tools::TeamMember{.agent_id = "planner@Protocol Team"},
        });
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto shutdown_request = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "reviewer",
      "team_name": "Protocol Team",
      "message": {
        "type": "shutdown_request",
        "reason": "Stop after final review"
      }
    })"));
    ASSERT_TRUE(shutdown_request.has_value());
    ASSERT_FALSE(shutdown_request->is_error);
    EXPECT_NE(shutdown_request->content.front().text.find("request_id: shutdown-"), std::string::npos);

    auto reviewer_inbox = loom::utils::read_inbox("reviewer", std::optional<std::string_view>{"Protocol Team"});
    ASSERT_TRUE(reviewer_inbox.has_value()) << reviewer_inbox.error();
    ASSERT_EQ(reviewer_inbox->size(), 1u);
    auto shutdown_request_json = loom::utils::json::parse(reviewer_inbox->front().text);
    ASSERT_TRUE(shutdown_request_json.has_value());
    EXPECT_EQ(shutdown_request_json->root().get_string("type"), "shutdown_request");
    EXPECT_EQ(shutdown_request_json->root().get_string("from"), "team-lead");
    EXPECT_EQ(shutdown_request_json->root().get_string("reason"), "Stop after final review");
    EXPECT_NE(shutdown_request_json->root().get_string("requestId").find("shutdown-"), std::string::npos);

    auto plan_response = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "planner",
      "team_name": "Protocol Team",
      "message": {
        "type": "plan_approval_response",
        "request_id": "plan-req-1",
        "approve": false,
        "feedback": "Revise the migration scope"
      }
    })"));
    ASSERT_TRUE(plan_response.has_value());
    ASSERT_FALSE(plan_response->is_error);
    EXPECT_NE(plan_response->content.front().text.find("request_id: plan-req-1"), std::string::npos);

    auto planner_inbox = loom::utils::read_inbox("planner", std::optional<std::string_view>{"Protocol Team"});
    ASSERT_TRUE(planner_inbox.has_value()) << planner_inbox.error();
    ASSERT_EQ(planner_inbox->size(), 1u);
    auto plan_json = loom::utils::json::parse(planner_inbox->front().text);
    ASSERT_TRUE(plan_json.has_value());
    EXPECT_EQ(plan_json->root().get_string("type"), "plan_approval_response");
    EXPECT_EQ(plan_json->root().get_string("requestId"), "plan-req-1");
    EXPECT_FALSE(plan_json->root().get("approved").as_bool());
    EXPECT_EQ(plan_json->root().get_string("feedback"), "Revise the migration scope");

    auto plan_approval = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "planner",
      "team_name": "Protocol Team",
      "message": {
        "type": "plan_approval_response",
        "requestId": "plan-req-2",
        "approve": "yes",
        "permission_mode": "acceptEdits"
      }
    })"));
    ASSERT_TRUE(plan_approval.has_value());
    ASSERT_FALSE(plan_approval->is_error);

    planner_inbox = loom::utils::read_inbox("planner", std::optional<std::string_view>{"Protocol Team"});
    ASSERT_TRUE(planner_inbox.has_value()) << planner_inbox.error();
    ASSERT_EQ(planner_inbox->size(), 2u);
    auto plan_approval_json = loom::utils::json::parse(planner_inbox->back().text);
    ASSERT_TRUE(plan_approval_json.has_value());
    EXPECT_EQ(plan_approval_json->root().get_string("type"), "plan_approval_response");
    EXPECT_EQ(plan_approval_json->root().get_string("requestId"), "plan-req-2");
    EXPECT_TRUE(plan_approval_json->root().get("approved").as_bool());
    EXPECT_EQ(plan_approval_json->root().get_string("permissionMode"), "acceptEdits");

    auto shutdown_rejection = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "team-lead",
      "team_name": "Protocol Team",
      "from_agent": "reviewer",
      "message": {
        "type": "shutdown_response",
        "request_id": "shutdown-req-1",
        "approve": false,
        "reason": "Need more time"
      }
    })"));
    ASSERT_TRUE(shutdown_rejection.has_value());
    ASSERT_FALSE(shutdown_rejection->is_error);

    auto leader_inbox = loom::utils::read_inbox("team-lead", std::optional<std::string_view>{"Protocol Team"});
    ASSERT_TRUE(leader_inbox.has_value()) << leader_inbox.error();
    ASSERT_EQ(leader_inbox->size(), 1u);
    auto shutdown_json = loom::utils::json::parse(leader_inbox->front().text);
    ASSERT_TRUE(shutdown_json.has_value());
    EXPECT_EQ(shutdown_json->root().get_string("type"), "shutdown_rejected");
    EXPECT_EQ(shutdown_json->root().get_string("requestId"), "shutdown-req-1");
    EXPECT_EQ(shutdown_json->root().get_string("from"), "reviewer");
    EXPECT_EQ(shutdown_json->root().get_string("reason"), "Need more time");

    auto shutdown_approval = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "team-lead",
      "team_name": "Protocol Team",
      "from_agent": "planner",
      "message": {
        "type": "shutdown_response",
        "requestId": "shutdown-req-2",
        "approved": true
      }
    })"));
    ASSERT_TRUE(shutdown_approval.has_value());
    ASSERT_FALSE(shutdown_approval->is_error);

    leader_inbox = loom::utils::read_inbox("team-lead", std::optional<std::string_view>{"Protocol Team"});
    ASSERT_TRUE(leader_inbox.has_value()) << leader_inbox.error();
    ASSERT_EQ(leader_inbox->size(), 2u);
    auto shutdown_approval_json = loom::utils::json::parse(leader_inbox->back().text);
    ASSERT_TRUE(shutdown_approval_json.has_value());
    EXPECT_EQ(shutdown_approval_json->root().get_string("type"), "shutdown_approved");
    EXPECT_EQ(shutdown_approval_json->root().get_string("requestId"), "shutdown-req-2");
    EXPECT_EQ(shutdown_approval_json->root().get_string("from"), "planner");

    auto misrouted_shutdown_response = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "planner",
      "team_name": "Protocol Team",
      "from_agent": "reviewer",
      "message": {
        "type": "shutdown_response",
        "request_id": "shutdown-req-wrong-target",
        "approve": true
      }
    })"));
    ASSERT_TRUE(misrouted_shutdown_response.has_value());
    EXPECT_TRUE(misrouted_shutdown_response->is_error);
    EXPECT_NE(misrouted_shutdown_response->content.front().text.find("shutdown_response must be sent to \"team-lead\""), std::string::npos);

    auto structured_broadcast = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "*",
      "team_name": "Protocol Team",
      "message": {
        "type": "shutdown_request",
        "reason": "Stop all teammates"
      }
    })"));
    ASSERT_TRUE(structured_broadcast.has_value());
    EXPECT_TRUE(structured_broadcast->is_error);
    EXPECT_NE(structured_broadcast->content.front().text.find("structured messages cannot be broadcast"), std::string::npos);

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

#ifndef _WIN32
TEST(Tools, RuntimeSendMessageDeliversPlainTextToUdsPeer) {
    auto root = fs::temp_directory_path() / "loom_send_message_uds_peer_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto socket_path = root / "peer.sock";
    LocalUnixLineServer server(socket_path);
    ASSERT_TRUE(server.valid());

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(std::format(R"({{
      "to": "uds:{}",
      "from_agent": "reviewer",
      "message": "Please inspect the peer session",
      "summary": "inspect peer session"
    }})", socket_path.string())));

    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("-> uds:"), std::string::npos);

    auto payload = server.wait_for_message();
    ASSERT_TRUE(payload.has_value());
    auto payload_json = loom::utils::json::parse(*payload);
    ASSERT_TRUE(payload_json.has_value()) << *payload;
    auto root_json = payload_json->root();
    EXPECT_EQ(root_json.get_string("type"), "cross_session_message");
    EXPECT_EQ(root_json.get_string("mode"), "prompt");
    EXPECT_EQ(root_json.get_string("from"), "reviewer");
    EXPECT_EQ(root_json.get_string("message"), "Please inspect the peer session");
    EXPECT_NE(root_json.get_string("value").find(R"(<cross-session-message from="reviewer">)"), std::string::npos);
    EXPECT_NE(root_json.get_string("value").find("Please inspect the peer session"), std::string::npos);

    fs::remove_all(root);
}
#endif

TEST(Tools, RuntimeSendMessageDeliversPlainTextToBridgePeer) {
    LocalBridgeIngressServer server;
    ASSERT_TRUE(server.ready());

    EnvironmentGuard endpoint_guard("LOOM_REMOTE_API_BASE_URL", server.base_url());
    EnvironmentGuard source_session_guard("LOOM_REMOTE_SESSION_ID", "session_source");
    EnvironmentGuard token_guard("LOOM_SESSION_ACCESS_TOKEN", "session-bridge-token");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "bridge:session_target",
      "from_agent": "reviewer",
      "message": "Please inspect the remote peer",
      "summary": "inspect remote peer"
    })"));

    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("-> bridge:session_target"), std::string::npos);

    auto requests = server.wait_for_requests(1);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 1u);
    EXPECT_EQ(requests->front().target_session_id, "session_target");
    EXPECT_EQ(requests->front().authorization, "Bearer session-bridge-token");
    EXPECT_NE(requests->front().body.find(R"("events":[)"), std::string::npos);
    EXPECT_NE(requests->front().body.find(R"("type":"user")"), std::string::npos);
    EXPECT_NE(requests->front().body.find(R"("role":"user")"), std::string::npos);
    EXPECT_NE(requests->front().body.find(R"("session_id":"session_target")"), std::string::npos);
    EXPECT_NE(requests->front().body.find(R"(<cross-session-message from=\"session_source\">)"), std::string::npos);
    EXPECT_NE(requests->front().body.find("Please inspect the remote peer"), std::string::npos);
    EXPECT_EQ(requests->front().body.find(R"(<cross-session-message from=\"reviewer\">)"), std::string::npos);
}

TEST(Tools, RuntimeSendMessageRejectsCrossSessionStructuredMessages) {
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto structured_uds = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "uds:/tmp/loom-peer.sock",
      "message": {
        "type": "shutdown_request",
        "reason": "done"
      }
    })"));
    ASSERT_TRUE(structured_uds.has_value());
    ASSERT_TRUE(structured_uds->is_error);
    EXPECT_NE(structured_uds->content.front().text.find("structured messages cannot be sent cross-session"), std::string::npos);

    auto structured_bridge = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "to": "bridge:session_123",
      "message": {
        "type": "shutdown_request",
        "reason": "done"
      }
    })"));
    ASSERT_TRUE(structured_bridge.has_value());
    ASSERT_TRUE(structured_bridge->is_error);
    EXPECT_NE(structured_bridge->content.front().text.find("structured messages cannot be sent cross-session"), std::string::npos);
}

TEST(Tools, RuntimeSendMessageRestoresPersistedTeammateMailboxAfterStoreReload) {
    auto root = fs::temp_directory_path() / "loom_send_message_teammate_reload_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard teammate_backend_guard("LOOM_TEAMMATE_BACKEND", "in-process");
    loom::utils::swarm_backends::BackendRegistry::reset();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    auto team = loom::tools::global_team_store().create("restart-team-id", "Restart Team", {});
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));

    loom::tools::AgentTool tool;
    auto spawned = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Spawn reload reviewer",
      "prompt": "Wait for cross-process messages",
      "name": "reviewer-one",
      "team_name": "Restart Team",
      "subagent_type": "general-purpose"
    })"));
    ASSERT_TRUE(spawned.has_value());
    ASSERT_FALSE(spawned->is_error);
    auto persisted_records = loom::tools::agent_runtime::load_all_native_agent_records();
    EXPECT_TRUE(std::ranges::any_of(persisted_records, [](const auto& record) {
        return record.agent_id == "reviewer-one@Restart Team";
    }));

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "reviewer-one",
      "team_name": "Restart Team",
      "content": "Review after a runtime restart",
      "summary": "restart delivery"
    })"));
    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("reviewer-one@Restart Team"), std::string::npos);

    auto restored_record = loom::tools::agent_runtime::native_agent_store().get("reviewer-one@Restart Team");
    ASSERT_TRUE(restored_record.has_value());
    ASSERT_EQ(restored_record->pending_messages.size(), 1u);
    EXPECT_NE(restored_record->pending_messages.front().find("Review after a runtime restart"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(restored_record->transcript, [](const auto& line) {
        return line.find("Review after a runtime restart") != std::string::npos;
    }));

    auto inbox = loom::utils::read_inbox("reviewer-one", std::optional<std::string_view>{"Restart Team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().from, "team-lead");
    EXPECT_EQ(inbox->front().text, "Review after a runtime restart");
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "restart delivery");
    EXPECT_TRUE(fs::exists(root / "teams" / "restart-team" / "inboxes" / "reviewer-one.json"));

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTaskToolsExposeNativeBackgroundAgents) {
    auto root = fs::temp_directory_path() / "loom_native_agent_task_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::AgentTool tool;
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async",
      "prompt": "Wait for task inspection",
      "name": "task-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value());
    ASSERT_FALSE(started->is_error);
    ASSERT_FALSE(started->content.empty());
    EXPECT_NE(started->content.front().text.find("outputFile:"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("task-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->transcript_path.has_value());
    ASSERT_TRUE(record->output_file_path.has_value());
    EXPECT_NE(started->content.front().text.find(*record->output_file_path), std::string::npos);
    EXPECT_TRUE(fs::exists(*record->transcript_path));
    EXPECT_TRUE(fs::is_symlink(*record->output_file_path));
    EXPECT_EQ(fs::read_symlink(*record->output_file_path), fs::path{*record->transcript_path});

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto listed = registry.execute("task_list", loom::core::ToolInput::from_json("{}"));
    ASSERT_TRUE(listed.has_value());
    ASSERT_FALSE(listed->is_error);
    ASSERT_FALSE(listed->content.empty());
    EXPECT_NE(listed->content.front().text.find("task-agent [queued]"), std::string::npos);
    EXPECT_NE(listed->content.front().text.find("output_file:"), std::string::npos);
    EXPECT_NE(listed->content.front().text.find(*record->output_file_path), std::string::npos);

    auto got = registry.execute("task_get", loom::core::ToolInput::from_json(R"({
      "task_id": "task-agent"
    })"));
    ASSERT_TRUE(got.has_value());
    ASSERT_FALSE(got->is_error);
    ASSERT_FALSE(got->content.empty());
    EXPECT_NE(got->content.front().text.find("Agent general-purpose: task-agent"), std::string::npos);

    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
      "task_id": "task-agent"
    })"));
    ASSERT_TRUE(stopped.has_value());
    ASSERT_FALSE(stopped->is_error);
    ASSERT_FALSE(stopped->content.empty());
    EXPECT_NE(stopped->content.front().text.find("task-agent [cancelled]"), std::string::npos);
    EXPECT_NE(stopped->content.front().text.find("stop requested"), std::string::npos);

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "task-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<task_notification>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<status>stopped</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("stop requested"), std::string::npos);

    std::ifstream stopped_output_file(*record->output_file_path);
    std::string stopped_output_text(
        (std::istreambuf_iterator<char>(stopped_output_file)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(stopped_output_text.find("system: agent cancelled: stop requested"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, StandaloneTaskToolsExposeNativeBackgroundAgents) {
    auto root = fs::temp_directory_path() / "loom_standalone_native_agent_task_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "standalone-native-agent",
        .agent_type = "general-purpose",
        .description = "Standalone native task",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .output = "standalone done",
        .transcript = {"assistant: standalone done"},
    });

    loom::tools::TaskListTool list_tool;
    auto listed = list_tool.execute();
    auto listed_native = std::ranges::find_if(listed, [](const auto* task) {
        return task && task->id == "standalone-native-agent";
    });
    ASSERT_NE(listed_native, listed.end());
    EXPECT_EQ((*listed_native)->status, loom::tools::TaskStatus::Completed);
    EXPECT_EQ((*listed_native)->description, "Standalone native task");

    loom::tools::TaskGetTool get_tool;
    auto got = get_tool.execute("standalone-native-agent");
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ((*got)->id, "standalone-native-agent");
    EXPECT_EQ((*got)->result, std::optional<std::string>{"standalone done"});

    loom::tools::TaskOutputTool output_tool;
    auto output = output_tool.execute("standalone-native-agent");
    ASSERT_TRUE(output.has_value());
    EXPECT_NE(output->find("standalone done"), std::string_view::npos);
    EXPECT_NE(output->find("<task_notification>"), std::string_view::npos);
    EXPECT_NE(output->find("<status>completed</status>"), std::string_view::npos);

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "standalone-stop-agent",
        .agent_type = "general-purpose",
        .description = "Standalone stop task",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });
    loom::tools::TaskStopTool stop_tool;
    auto stopped = stop_tool.execute("standalone-stop-agent");
    ASSERT_TRUE(stopped.has_value());
    auto stopped_record = loom::tools::agent_runtime::native_agent_store().get("standalone-stop-agent");
    ASSERT_TRUE(stopped_record.has_value());
    EXPECT_EQ(stopped_record->status, loom::tools::agent_runtime::NativeAgentStatus::Cancelled);
    ASSERT_TRUE(stopped_record->error.has_value());
    EXPECT_EQ(*stopped_record->error, "stop requested");

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    { std::error_code ec; fs::remove_all(root, ec); }
}

TEST(Tools, TaskStopCancelsRunningBackgroundAgentDuringModelStream) {
    auto root = fs::temp_directory_path() / "loom_native_agent_stream_cancel_test";
    fs::remove_all(root);
    fs::create_directories(root);
    LocalSlowMessagesStreamServer server(std::chrono::milliseconds(750));
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "stream-cancel-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async and cancel while streaming",
      "prompt": "Wait for cancellation during the model stream",
      "name": "stream-cancel-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    ASSERT_TRUE(server.wait_for_request());
    auto body = server.last_body();
    ASSERT_TRUE(body.has_value());
    EXPECT_NE(body->find("Wait for cancellation during the model stream"), std::string::npos);

    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
      "task_id": "stream-cancel-agent"
    })"));
    ASSERT_TRUE(stopped.has_value());
    ASSERT_FALSE(stopped->is_error);
    ASSERT_FALSE(stopped->content.empty());
    EXPECT_NE(stopped->content.front().text.find("stream-cancel-agent [cancelled]"), std::string::npos);

    bool observed_stream_cancel = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto record = loom::tools::agent_runtime::native_agent_store().get("stream-cancel-agent");
        ASSERT_TRUE(record.has_value());
        if (record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled &&
            record->error &&
            record->error->find("while waiting for model stream") != std::string::npos) {
            observed_stream_cancel = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(observed_stream_cancel);

    std::this_thread::sleep_for(std::chrono::milliseconds(900));
    auto record = loom::tools::agent_runtime::native_agent_store().get("stream-cancel-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Cancelled);
    ASSERT_TRUE(record->error.has_value());
    EXPECT_NE(record->error->find("while waiting for model stream"), std::string::npos);
    EXPECT_FALSE(record->output.has_value());

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "stream-cancel-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<status>stopped</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("while waiting for model stream"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, TaskStopCancelsRunningBackgroundAgentDuringSleepToolExecution) {
    auto root = fs::temp_directory_path() / "loom_native_agent_sleep_cancel_test";
    fs::remove_all(root);
    fs::create_directories(root);
    LocalSleepToolUseMessagesServer server;
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "sleep-cancel-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async and cancel during sleep",
      "prompt": "Use sleep until I stop you",
      "name": "sleep-cancel-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    ASSERT_TRUE(server.wait_for_request_count(1));

    // Wait until agent has parsed the SSE response (assistant transcript entry)
    // and entered tool execution before issuing cancel.
    for (int i = 0; i < 500; ++i) {
        auto rec = loom::tools::agent_runtime::native_agent_store().get("sleep-cancel-agent");
        if (rec && std::ranges::any_of(rec->transcript, [](const auto& e) {
            return e.find("assistant:") != std::string::npos;
        })) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto stop_started = std::chrono::steady_clock::now();
    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
      "task_id": "sleep-cancel-agent"
    })"));
    ASSERT_TRUE(stopped.has_value());
    ASSERT_FALSE(stopped->is_error);

    bool observed_sleep_cancel = false;
    for (int attempt = 0; attempt < 250; ++attempt) {
        auto record = loom::tools::agent_runtime::native_agent_store().get("sleep-cancel-agent");
        ASSERT_TRUE(record.has_value());
        if (record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled &&
            record->error &&
            record->error->find("while executing tool sleep") != std::string::npos) {
            observed_sleep_cancel = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(observed_sleep_cancel);
    EXPECT_LT(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_started).count(),
        5000);
    EXPECT_EQ(server.request_count(), 1u);

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "sleep-cancel-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<status>stopped</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("while executing tool sleep"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, TaskStopCancelsRunningBackgroundAgentDuringWebFetchToolExecution) {
    auto root = fs::temp_directory_path() / "loom_native_agent_webfetch_cancel_test";
    fs::remove_all(root);
    fs::create_directories(root);
    LocalSlowContentServer content_server(std::chrono::seconds(2));
    ASSERT_TRUE(content_server.valid());
    const auto fetch_input = std::format(
        R"({{"url":"{}"}})",
        loom::tools::agent::json_escape_string(content_server.url()));
    LocalScriptedToolUseMessagesServer server(
        "WebFetch",
        fetch_input,
        "toolu_webfetch_cancel",
        "should not continue after web fetch cancellation");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "webfetch-cancel-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async and cancel during WebFetch",
      "prompt": "Use WebFetch until I stop you",
      "name": "webfetch-cancel-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    ASSERT_TRUE(server.wait_for_request_count(1));
    ASSERT_TRUE(content_server.wait_for_request());

    const auto stop_started = std::chrono::steady_clock::now();
    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
      "task_id": "webfetch-cancel-agent"
    })"));
    ASSERT_TRUE(stopped.has_value());
    ASSERT_FALSE(stopped->is_error);

    bool observed_webfetch_cancel = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto record = loom::tools::agent_runtime::native_agent_store().get("webfetch-cancel-agent");
        ASSERT_TRUE(record.has_value());
        if (record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled &&
            record->error &&
            record->error->find("while executing tool WebFetch") != std::string::npos) {
            observed_webfetch_cancel = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(observed_webfetch_cancel);
    EXPECT_LT(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_started).count(),
        2000);
    EXPECT_FALSE(server.wait_for_request_count(2, std::chrono::milliseconds(250)));

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "webfetch-cancel-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<status>stopped</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("while executing tool WebFetch"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, TaskStopCancelsRunningBackgroundAgentDuringBashToolExecution) {
    auto root = fs::temp_directory_path() / "loom_native_agent_bash_cancel_test";
    fs::remove_all(root);
    fs::create_directories(root);
    LocalBashToolUseMessagesServer server;
    ASSERT_TRUE(server.valid());
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "bash-cancel-test-model");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::tools::AgentTool tool({}, 0, &registry);
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async and cancel during bash",
      "prompt": "Use Bash until I stop you",
      "name": "bash-cancel-agent",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value()) << started.error().format();
    ASSERT_FALSE(started->is_error);
    ASSERT_TRUE(server.wait_for_request_count(1));

    // Wait until agent has parsed the SSE response and entered tool execution
    for (int i = 0; i < 500; ++i) {
        auto rec = loom::tools::agent_runtime::native_agent_store().get("bash-cancel-agent");
        if (rec && std::ranges::any_of(rec->transcript, [](const auto& e) {
            return e.find("assistant:") != std::string::npos;
        })) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    const auto stop_started = std::chrono::steady_clock::now();
    auto stopped = registry.execute("task_stop", loom::core::ToolInput::from_json(R"({
      "task_id": "bash-cancel-agent"
    })"));
    ASSERT_TRUE(stopped.has_value());
    ASSERT_FALSE(stopped->is_error);

    bool observed_bash_cancel = false;
    for (int attempt = 0; attempt < 250; ++attempt) {
        auto record = loom::tools::agent_runtime::native_agent_store().get("bash-cancel-agent");
        ASSERT_TRUE(record.has_value());
        if (record->status == loom::tools::agent_runtime::NativeAgentStatus::Cancelled &&
            record->error &&
            record->error->find("while executing tool Bash") != std::string::npos) {
            observed_bash_cancel = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_TRUE(observed_bash_cancel);
    EXPECT_LT(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - stop_started).count(),
        5000);
    EXPECT_EQ(server.request_count(), 1u);

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "bash-cancel-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<status>stopped</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("while executing tool Bash"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTaskOutputIncludesNativeAgentCompletionNotification) {
    auto root = fs::temp_directory_path() / "loom_native_agent_completion_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "completed-agent",
        .agent_type = "reviewer",
        .name = "completed reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .worktree_path = (root / "agent-worktree").string(),
        .worktree_branch = "loom-agent-completed-agent",
        .transcript = {"user: work", "assistant: done"},
    });
    loom::tools::agent_runtime::native_agent_store().mark_completed("completed-agent", "done");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "completed-agent"
    })"));

    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("done"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("assistant: done"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<task_notification>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<status>completed</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<result>done</result>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("worktree_path: " + (root / "agent-worktree").string()), std::string::npos);
    EXPECT_NE(output->content.front().text.find("worktree_branch: loom-agent-completed-agent"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<worktree_path>" + (root / "agent-worktree").string()), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<worktree_branch>loom-agent-completed-agent</worktree_branch>"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTaskUpdateMarksNativeAgentFailedWithOutputArtifactAndNotification) {
    auto root = fs::temp_directory_path() / "loom_native_agent_failure_artifact_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "failed-agent",
        .agent_type = "reviewer",
        .name = "failed reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
        .transcript = {"user: inspect failure path"},
    });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto update = registry.execute("task_update", loom::core::ToolInput::from_json(R"({
      "task_id": "failed-agent",
      "status": "failed",
      "result": "agent crashed while reading bindings"
    })"));
    ASSERT_TRUE(update.has_value());
    ASSERT_FALSE(update->is_error);

    auto record = loom::tools::agent_runtime::native_agent_store().get("failed-agent");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Failed);
    ASSERT_TRUE(record->error.has_value());
    EXPECT_EQ(*record->error, "agent crashed while reading bindings");
    ASSERT_TRUE(record->output_file_path.has_value());
    EXPECT_TRUE(fs::exists(*record->output_file_path));
    EXPECT_TRUE(fs::is_symlink(*record->output_file_path));

    std::ifstream artifact(*record->output_file_path);
    std::string artifact_text(
        (std::istreambuf_iterator<char>(artifact)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(artifact_text.find("user: inspect failure path"), std::string::npos);
    EXPECT_NE(artifact_text.find("system: agent failed: agent crashed while reading bindings"), std::string::npos);

    auto output = registry.execute("task_output", loom::core::ToolInput::from_json(R"({
      "task_id": "failed-agent"
    })"));
    ASSERT_TRUE(output.has_value());
    ASSERT_FALSE(output->is_error);
    ASSERT_FALSE(output->content.empty());
    EXPECT_NE(output->content.front().text.find("<task_notification>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<status>failed</status>"), std::string::npos);
    EXPECT_NE(output->content.front().text.find("<result>agent crashed while reading bindings</result>"), std::string::npos);

    auto notifications = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    ASSERT_EQ(notifications.size(), 1u);
    EXPECT_NE(notifications.front().find("<task_id>failed-agent</task_id>"), std::string::npos);
    EXPECT_NE(notifications.front().find("<status>failed</status>"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, NativeAgentNotificationsAreConsumedOnce) {
    auto root = fs::temp_directory_path() / "loom_native_agent_notification_once_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "notify-agent",
        .agent_type = "reviewer",
        .name = "notify reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
    });
    loom::tools::agent_runtime::native_agent_store().mark_completed("notify-agent", "review complete");

    auto first = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    ASSERT_EQ(first.size(), 1u);
    EXPECT_NE(first.front().find("<task_notification>"), std::string::npos);
    EXPECT_NE(first.front().find("<status>completed</status>"), std::string::npos);
    EXPECT_NE(first.front().find("<result>review complete</result>"), std::string::npos);

    auto second = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    EXPECT_TRUE(second.empty());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    EXPECT_TRUE(restored.empty());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, NativeAgentRecordPersistsWorktreeMetadata) {
    auto root = fs::temp_directory_path() / "loom_native_agent_worktree_metadata_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "metadata-agent",
        .agent_type = "reviewer",
        .cwd = (root / "worktree").string(),
        .isolation = "worktree",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Queued,
        .worktree_path = (root / "worktree").string(),
        .worktree_branch = "loom-agent-metadata-agent",
        .worktree_base_commit = "abc123",
        .worktree_git_root = root.string(),
    });

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored = loom::tools::agent_runtime::native_agent_store().get("metadata-agent");
    ASSERT_TRUE(restored.has_value());
    ASSERT_TRUE(restored->worktree_path.has_value());
    EXPECT_EQ(*restored->worktree_path, (root / "worktree").string());
    ASSERT_TRUE(restored->worktree_branch.has_value());
    EXPECT_EQ(*restored->worktree_branch, "loom-agent-metadata-agent");
    ASSERT_TRUE(restored->worktree_base_commit.has_value());
    EXPECT_EQ(*restored->worktree_base_commit, "abc123");
    ASSERT_TRUE(restored->worktree_git_root.has_value());
    EXPECT_EQ(*restored->worktree_git_root, root.string());
    EXPECT_FALSE(restored->worktree_cleanup_performed);

    loom::tools::agent_runtime::native_agent_store().mark_worktree_cleaned("metadata-agent");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto cleaned = loom::tools::agent_runtime::native_agent_store().get("metadata-agent");
    ASSERT_TRUE(cleaned.has_value());
    EXPECT_TRUE(cleaned->worktree_cleanup_performed);
    EXPECT_FALSE(cleaned->worktree_path.has_value());
    EXPECT_FALSE(cleaned->cwd.has_value());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, NativeAgentRecordPersistsSidechainJsonlAndResumesFromIt) {
    auto root = fs::temp_directory_path() / "loom_native_agent_sidechain_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "sidechain-agent",
        .agent_type = "reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .transcript = {"user: inspect generated bindings", "assistant: bindings reviewed"},
    });

    auto record = loom::tools::agent_runtime::native_agent_store().get("sidechain-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->transcript_path.has_value());
    ASSERT_TRUE(record->sidechain_jsonl_path.has_value());
    EXPECT_TRUE(fs::exists(*record->transcript_path));
    EXPECT_TRUE(fs::exists(*record->sidechain_jsonl_path));

    std::ifstream sidechain_in(*record->sidechain_jsonl_path);
    std::string sidechain_text(
        (std::istreambuf_iterator<char>(sidechain_in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(sidechain_text.find(R"("type":"user")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"assistant")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("parentUuid":null)"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("parentUuid":"sidechain-agent-0")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("isSidechain":true)"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("agentId":"sidechain-agent")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("message":{"role":"user","content":[{"type":"text","text":"inspect generated bindings"}]})"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("agent_id":"sidechain-agent")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("role":"user")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("raw":"assistant: bindings reviewed")"), std::string::npos);

    std::error_code ec;
    fs::remove(*record->transcript_path, ec);
    ASSERT_FALSE(fs::exists(*record->transcript_path));

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto resumed = loom::tools::agent_runtime::resume_agent("sidechain-agent");
    ASSERT_TRUE(resumed.has_value()) << resumed.error();
    ASSERT_EQ(resumed->transcript.size(), 2u);
    EXPECT_EQ(resumed->transcript.front(), "user: inspect generated bindings");
    EXPECT_EQ(resumed->transcript.back(), "assistant: bindings reviewed");

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, NativeAgentResumeReadsTypeScriptSidechainTranscriptEntries) {
    auto root = fs::temp_directory_path() / "loom_native_agent_ts_sidechain_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "ts-sidechain-agent",
        .agent_type = "reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .output = "completed from persisted TS transcript",
        .transcript = {"system: placeholder"},
    });

    auto record = loom::tools::agent_runtime::native_agent_store().get("ts-sidechain-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->transcript_path.has_value());
    ASSERT_TRUE(record->sidechain_jsonl_path.has_value());

    std::error_code ec;
    fs::remove(*record->transcript_path, ec);
    ASSERT_FALSE(fs::exists(*record->transcript_path));

    {
        std::ofstream sidechain(*record->sidechain_jsonl_path, std::ios::trunc);
        sidechain << R"({"type":"user","uuid":"u1","parentUuid":null,"isSidechain":true,"agentId":"ts-sidechain-agent","message":{"role":"user","content":[{"type":"text","text":"Inspect TS persisted prompt"}]}})" << '\n';
        sidechain << R"({"type":"assistant","uuid":"a1","parentUuid":"u1","isSidechain":true,"agentId":"ts-sidechain-agent","message":{"role":"assistant","content":[{"type":"text","text":"TS persisted answer"},{"type":"tool_use","id":"tool-1","name":"Read","input":{"file_path":"README.md"}}]}})" << '\n';
        sidechain << R"({"type":"user","uuid":"u2","parentUuid":"a1","isSidechain":true,"agentId":"ts-sidechain-agent","message":{"role":"user","content":[{"type":"tool_result","tool_use_id":"tool-1","content":[{"type":"text","text":"README content"}]}]}})" << '\n';
    }

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto resumed = loom::tools::agent_runtime::resume_agent("ts-sidechain-agent");
    ASSERT_TRUE(resumed.has_value()) << resumed.error();
    ASSERT_EQ(resumed->transcript.size(), 3u);
    EXPECT_EQ(resumed->transcript[0], "user: Inspect TS persisted prompt");
    EXPECT_NE(resumed->transcript[1].find("assistant: TS persisted answer"), std::string::npos);
    EXPECT_NE(resumed->transcript[1].find("[tool_use:Read]"), std::string::npos);
    EXPECT_EQ(resumed->transcript[2], "user: tool_result: README content");

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, NativeAgentStructuredSidechainPreservesToolUseAndResultBlocks) {
    auto root = fs::temp_directory_path() / "loom_native_agent_structured_sidechain_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "structured-agent",
        .agent_type = "reviewer",
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Running,
    });

    loom::services::api::Message assistant;
    assistant.role = "assistant";
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::Text,
        .text = "I will inspect README.",
    });
    assistant.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolUse,
        .tool_use_id = "tool-structured-1",
        .tool_name = "Read",
        .tool_input_json = R"({"file_path":"README.md","limit":20})",
    });
    loom::tools::agent_runtime::native_agent_store().append_sidechain_message(
        "structured-agent",
        assistant.role,
        loom::tools::agent::message_content_sidechain_json(assistant),
        loom::tools::agent::message_content_text(assistant));

    loom::services::api::Message tool_result;
    tool_result.role = "user";
    tool_result.content.push_back(loom::services::api::ContentBlock{
        .type = loom::services::api::ContentBlockType::ToolResult,
        .text = "README content",
        .tool_use_id = "tool-structured-1",
    });
    loom::tools::agent_runtime::native_agent_store().append_sidechain_message(
        "structured-agent",
        tool_result.role,
        loom::tools::agent::message_content_sidechain_json(tool_result),
        loom::tools::agent::message_content_text(tool_result));

    auto record = loom::tools::agent_runtime::native_agent_store().get("structured-agent");
    ASSERT_TRUE(record.has_value());
    ASSERT_EQ(record->sidechain_entries.size(), 2u);
    ASSERT_TRUE(record->sidechain_jsonl_path.has_value());
    ASSERT_TRUE(fs::exists(*record->sidechain_jsonl_path));

    std::ifstream sidechain_in(*record->sidechain_jsonl_path);
    std::string sidechain_text(
        (std::istreambuf_iterator<char>(sidechain_in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(sidechain_text.find(R"("type":"tool_use")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("id":"tool-structured-1")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("name":"Read")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("file_path":"README.md")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("limit":20)"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("type":"tool_result")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("tool_use_id":"tool-structured-1")"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("content":[{"type":"text","text":"README content"}])"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("parentUuid":null)"), std::string::npos);
    EXPECT_NE(sidechain_text.find(R"("parentUuid":"structured-agent-0")"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored = loom::tools::agent_runtime::native_agent_store().get("structured-agent");
    ASSERT_TRUE(restored.has_value());
    ASSERT_EQ(restored->sidechain_entries.size(), 2u);
    ASSERT_EQ(restored->transcript.size(), 2u);
    EXPECT_NE(restored->transcript[0].find("[tool_use:Read]"), std::string::npos);
    EXPECT_NE(restored->transcript[1].find("tool_result: README content"), std::string::npos);

    auto resumed = loom::tools::agent_runtime::resume_agent("structured-agent");
    ASSERT_TRUE(resumed.has_value()) << resumed.error();
    ASSERT_EQ(resumed->transcript.size(), 3u);
    EXPECT_NE(resumed->transcript[0].find("[tool_use:Read]"), std::string::npos);
    EXPECT_NE(resumed->transcript[1].find("tool_result: README content"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeSendMessageDeliversToBackgroundAgentQueue) {
    auto root = fs::temp_directory_path() / "loom_send_message_runtime_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::AgentTool tool;
    auto started = tool.execute(loom::core::ToolInput::from_json(R"({
      "description": "Run async",
      "prompt": "Wait for coordination",
      "name": "message-target",
      "run_in_background": true
    })"));
    ASSERT_TRUE(started.has_value());
    ASSERT_FALSE(started->is_error);

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "message-target",
      "content": "Review the migration diff",
      "priority": "high"
    })"));

    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("Delivered message"), std::string::npos);
    EXPECT_NE(delivered->content.front().text.find("message-target"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("message-target");
    ASSERT_TRUE(record.has_value());
    ASSERT_FALSE(record->transcript.empty());
    EXPECT_NE(record->transcript.back().find("Review the migration diff"), std::string::npos);
    ASSERT_EQ(record->pending_messages.size(), 1u);
    EXPECT_NE(record->pending_messages.front().find("[Message from team-lead priority=high]"), std::string::npos);
    EXPECT_NE(record->pending_messages.front().find("Review the migration diff"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored = loom::tools::agent_runtime::native_agent_store().get("message-target");
    ASSERT_TRUE(restored.has_value());
    ASSERT_FALSE(restored->transcript.empty());
    EXPECT_NE(restored->transcript.back().find("Review the migration diff"), std::string::npos);
    ASSERT_EQ(restored->pending_messages.size(), 1u);
    EXPECT_NE(restored->pending_messages.front().find("Review the migration diff"), std::string::npos);

    auto pending = loom::tools::agent_runtime::native_agent_store().take_pending_messages("message-target");
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_NE(pending.front().find("priority=high"), std::string::npos);
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto consumed = loom::tools::agent_runtime::native_agent_store().get("message-target");
    ASSERT_TRUE(consumed.has_value());
    EXPECT_TRUE(consumed->pending_messages.empty());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeSendMessageQueuesStoppedNativeAgentForResume) {
    auto root = fs::temp_directory_path() / "loom_send_message_resume_runtime_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "runtime").string());
    EnvironmentUnsetGuard api_key_guard("LOOM_API_KEY");
    EnvironmentUnsetGuard loom_token_guard("LOOM_AUTH_TOKEN");
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::agent_runtime::native_agent_store().upsert(loom::tools::agent_runtime::NativeAgentRecord{
        .agent_id = "resume-target",
        .agent_type = "general-purpose",
        .description = "Stopped native agent",
        .name = "stopped-agent",
        .cwd = root.string(),
        .background = true,
        .status = loom::tools::agent_runtime::NativeAgentStatus::Completed,
        .output = "old completed output",
        .capabilities = {"Read"},
        .transcript = {"user: original prompt", "assistant: old completed output"},
        .progress = 1.0,
        .notification_delivered = true,
    });

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "stopped-agent",
      "content": "Resume with this follow-up",
      "priority": "normal"
    })"));

    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("queued for background resume"), std::string::npos);
    EXPECT_NE(delivered->content.front().text.find("background resume deferred: no API credentials are configured"), std::string::npos);

    auto record = loom::tools::agent_runtime::native_agent_store().get("resume-target");
    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(record->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
    EXPECT_FALSE(record->output.has_value());
    EXPECT_FALSE(record->error.has_value());
    ASSERT_TRUE(record->progress.has_value());
    EXPECT_DOUBLE_EQ(*record->progress, 0.0);
    EXPECT_FALSE(record->cancel_requested);
    EXPECT_FALSE(record->notification_delivered);
    ASSERT_EQ(record->pending_messages.size(), 1u);
    EXPECT_NE(record->pending_messages.front().find("Resume with this follow-up"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& line) {
        return line.find("resume requested from pending message") != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(record->transcript, [](const auto& line) {
        return line.find("Resume with this follow-up") != std::string::npos;
    }));

    auto notifications = loom::tools::agent_runtime::native_agent_store().take_pending_task_notifications();
    EXPECT_TRUE(notifications.empty());

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored = loom::tools::agent_runtime::native_agent_store().get("resume-target");
    ASSERT_TRUE(restored.has_value());
    EXPECT_EQ(restored->status, loom::tools::agent_runtime::NativeAgentStatus::Queued);
    ASSERT_EQ(restored->pending_messages.size(), 1u);
    EXPECT_NE(restored->pending_messages.front().find("Resume with this follow-up"), std::string::npos);

    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}
TEST(Tools, RuntimeTeamCreateRegistersMembersAndSharedTasks) {
    auto root = fs::temp_directory_path() / "loom_team_runtime_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto created = registry.execute("team_create", loom::core::ToolInput::from_json(R"({
      "team_id": "runtime-team-members",
      "team_name": "Runtime Team Members",
      "members": [
        {"agent_id": "team-researcher", "role": "worker"},
        {"agent_id": "team-reviewer", "role": "reviewer"}
      ],
      "task_list": [
        {"id": "task-1", "description": "Inspect migration parity", "assigned_to": "team-researcher"}
      ]
    })"));

    ASSERT_TRUE(created.has_value());
    ASSERT_FALSE(created->is_error);
    ASSERT_FALSE(created->content.empty());
    auto created_json = loom::utils::json::parse(created->content.front().text);
    ASSERT_TRUE(created_json.has_value());
    auto created_root = created_json->root();
    EXPECT_EQ(created_root.get_string("team_name"), "Runtime Team Members");
    EXPECT_EQ(created_root.get_string("team_id"), "runtime-team-members");
    EXPECT_EQ(created_root.get_string("lead_agent_id"), "team-lead@Runtime Team Members");
    EXPECT_EQ(created_root.get_int("members"), 2);
    EXPECT_EQ(created_root.get_int("tasks"), 1);
    EXPECT_EQ(created_root.get_int("member_inboxes_initialized"), 2);
    EXPECT_EQ(created_root.get_int("task_assignments_enqueued"), 1);
    EXPECT_TRUE(created_root.get("team_config_written").as_bool());
    EXPECT_TRUE(created_root.get("task_list_written").as_bool());

    auto record = loom::tools::agent_runtime::native_agent_store().get("team-reviewer");
    ASSERT_TRUE(record.has_value());
    ASSERT_TRUE(record->team_name.has_value());
    EXPECT_EQ(*record->team_name, "Runtime Team Members");

    auto team = loom::tools::global_team_store().get("runtime-team-members");
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));
    ASSERT_EQ((*team)->task_list.size(), 1u);
    ASSERT_TRUE((*team)->task_list.front().assigned_to.has_value());
    EXPECT_EQ(*(*team)->task_list.front().assigned_to, "team-researcher");
    auto member = std::ranges::find_if((*team)->members, [](const auto& candidate) {
        return candidate.agent_id == "team-researcher";
    });
    ASSERT_NE(member, (*team)->members.end());
    EXPECT_EQ(member->status, loom::tools::MemberStatus::Working);
    ASSERT_TRUE(member->current_task.has_value());
    EXPECT_EQ(*member->current_task, "task-1");

    auto researcher = loom::tools::agent_runtime::native_agent_store().get("team-researcher");
    ASSERT_TRUE(researcher.has_value());
    EXPECT_EQ(researcher->status, loom::tools::agent_runtime::NativeAgentStatus::Running);
    ASSERT_EQ(researcher->pending_messages.size(), 1u);
    EXPECT_NE(researcher->pending_messages.front().find("[Team task task-1 assigned by Runtime Team Members]"), std::string::npos);
    EXPECT_NE(researcher->pending_messages.front().find("Inspect migration parity"), std::string::npos);
    EXPECT_TRUE(std::ranges::any_of(researcher->transcript, [](const auto& entry) {
        return entry.find("team task assigned task-1: Inspect migration parity") != std::string::npos;
    }));
    EXPECT_TRUE(fs::exists(root / "teams" / "runtime-team-members.json"));
    auto team_dir = root / "teams" / "runtime-team-members";
    EXPECT_EQ(created_root.get_string("team_dir"), team_dir.string());
    EXPECT_EQ(created_root.get_string("team_file_path"), (team_dir / "config.json").string());
    EXPECT_TRUE(fs::exists(team_dir / "config.json"));
    EXPECT_TRUE(fs::exists(team_dir / "inboxes" / "team-researcher.json"));
    EXPECT_TRUE(fs::exists(team_dir / "inboxes" / "team-reviewer.json"));
    ASSERT_TRUE(fs::exists(team_dir / "tasks.json"));
    std::ifstream tasks_in(team_dir / "tasks.json");
    std::string tasks_text(
        (std::istreambuf_iterator<char>(tasks_in)),
        std::istreambuf_iterator<char>());
    EXPECT_NE(tasks_text.find(R"("id":"task-1")"), std::string::npos);
    EXPECT_NE(tasks_text.find(R"("assigned_to":"team-researcher")"), std::string::npos);

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    auto restored_team = loom::tools::global_team_store().get("runtime-team-members");
    ASSERT_TRUE(restored_team.has_value()) << std::string(loom::tools::format_error(restored_team.error()));
    ASSERT_EQ((*restored_team)->task_list.size(), 1u);
    EXPECT_EQ((*restored_team)->task_list.front().id, "task-1");
    auto restored_member = std::ranges::find_if((*restored_team)->members, [](const auto& candidate) {
        return candidate.agent_id == "team-researcher";
    });
    ASSERT_NE(restored_member, (*restored_team)->members.end());
    EXPECT_EQ(restored_member->status, loom::tools::MemberStatus::Working);
    auto restored_researcher = loom::tools::agent_runtime::native_agent_store().get("team-researcher");
    ASSERT_TRUE(restored_researcher.has_value());
    ASSERT_EQ(restored_researcher->pending_messages.size(), 1u);
    EXPECT_NE(restored_researcher->pending_messages.front().find("Inspect migration parity"), std::string::npos);

    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "team-reviewer",
      "content": "Review team output"
    })"));

    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    EXPECT_NE(delivered->content.front().text.find("team-reviewer"), std::string::npos);

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTeamCreateCanStartNativeAgentsAndResumeThemWithSendMessage) {
    auto root = fs::temp_directory_path() / "loom_team_create_native_start_test";
    fs::remove_all(root);
    fs::create_directories(root);
    LocalSlowMessagesStreamServer server(std::chrono::milliseconds(1));
    ASSERT_TRUE(server.valid());
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "team-create-native-start-model");
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto created = registry.execute("team_create", loom::core::ToolInput::from_json(R"({
      "team_id": "native-start-team-id",
      "team_name": "Native Start Team",
      "start_native_agents": true,
      "members": [
        {"agent_id": "planner@native-start-team", "prompt": "Plan the native team launch", "subagent_type": "general-purpose"},
        {"agent_id": "reviewer@native-start-team", "subagent_type": "general-purpose"}
      ],
      "task_list": [
        {"id": "review-task", "description": "Review the native team launch", "assigned_to": "reviewer@native-start-team"}
      ]
    })"));

    ASSERT_TRUE(created.has_value());
    ASSERT_FALSE(created->is_error);
    ASSERT_FALSE(created->content.empty());
    auto created_json = loom::utils::json::parse(created->content.front().text);
    ASSERT_TRUE(created_json.has_value());
    auto created_root = created_json->root();
    EXPECT_EQ(created_root.get_string("team_name"), "Native Start Team");
    EXPECT_EQ(created_root.get_string("team_id"), "native-start-team-id");
    EXPECT_EQ(created_root.get_string("lead_agent_id"), "team-lead@Native Start Team");
    EXPECT_EQ(created_root.get_int("native_agents_started"), 2);
    EXPECT_TRUE(created_root.get("team_config_written").as_bool());
    EXPECT_TRUE(fs::exists(created_root.get_string("team_file_path")));
    ASSERT_TRUE(server.wait_for_request_count(2));
    EXPECT_TRUE(wait_for_native_agent_status(
        "planner@native-start-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));
    EXPECT_TRUE(wait_for_native_agent_status(
        "reviewer@native-start-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));

    auto planner = loom::tools::agent_runtime::native_agent_store().get("planner@native-start-team");
    ASSERT_TRUE(planner.has_value());
    ASSERT_TRUE(planner->team_name.has_value());
    EXPECT_EQ(*planner->team_name, "Native Start Team");
    ASSERT_TRUE(planner->output_file_path.has_value());
    EXPECT_TRUE(fs::is_symlink(*planner->output_file_path));
    EXPECT_TRUE(std::ranges::any_of(planner->transcript, [](const auto& line) {
        return line.find("Plan the native team launch") != std::string::npos;
    }));

    auto reviewer = loom::tools::agent_runtime::native_agent_store().get("reviewer@native-start-team");
    ASSERT_TRUE(reviewer.has_value());
    EXPECT_TRUE(std::ranges::any_of(reviewer->transcript, [](const auto& line) {
        return line.find("[Team task review-task assigned by Native Start Team]") != std::string::npos;
    }));
    EXPECT_EQ(reviewer->output, std::optional<std::string>{"late stream response"});

    auto wait_for_member_done = [](std::string_view member_id) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            auto team = loom::tools::global_team_store().get("native-start-team-id");
            if (team) {
                auto member = std::ranges::find_if((*team)->members, [&](const auto& candidate) {
                    return candidate.agent_id == member_id;
                });
                if (member != (*team)->members.end() &&
                    member->status == loom::tools::MemberStatus::Done &&
                    member->last_result &&
                    member->last_result->find("late stream response") != std::string::npos) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    };
    EXPECT_TRUE(wait_for_member_done("planner@native-start-team"));
    EXPECT_TRUE(wait_for_member_done("reviewer@native-start-team"));

    auto delivered = registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "reviewer@native-start-team",
      "content": "Continue reviewing the launched team path",
      "from_agent": "team-lead"
    })"));
    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("background resume started"), std::string::npos);
    ASSERT_TRUE(server.wait_for_request_count(3));
    EXPECT_TRUE(wait_for_native_agent_status(
        "reviewer@native-start-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));
    reviewer = loom::tools::agent_runtime::native_agent_store().get("reviewer@native-start-team");
    ASSERT_TRUE(reviewer.has_value());
    EXPECT_TRUE(std::ranges::any_of(reviewer->transcript, [](const auto& line) {
        return line.find("Continue reviewing the launched team path") != std::string::npos;
    }));

    const auto planner_output_file = fs::path{*planner->output_file_path};
    const auto reviewer_output_file = fs::path{*reviewer->output_file_path};
    auto deleted = registry.execute("team_delete", loom::core::ToolInput::from_json(R"({
      "team_id": "native-start-team-id"
    })"));
    ASSERT_TRUE(deleted.has_value());
    ASSERT_FALSE(deleted->is_error);
    ASSERT_FALSE(deleted->content.empty());
    EXPECT_NE(deleted->content.front().text.find("Deleted team Native Start Team (native-start-team-id)"), std::string::npos);
    EXPECT_FALSE(fs::exists(root / "teams" / "native-start-team-id.json"));
    EXPECT_FALSE(fs::exists(root / "teams" / "native-start-team"));
    EXPECT_FALSE(fs::exists(planner_output_file) || fs::is_symlink(planner_output_file));
    EXPECT_FALSE(fs::exists(reviewer_output_file) || fs::is_symlink(reviewer_output_file));

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTeamCreateStartedNativeTeammateResumesAfterRegistryRestart) {
    auto root = fs::temp_directory_path() / "loom_team_create_restart_resume_test";
    { std::error_code ec; fs::remove_all(root, ec); }
    fs::create_directories(root);
    LocalSlowMessagesStreamServer server(std::chrono::milliseconds(1));
    ASSERT_TRUE(server.valid());
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "team-create-restart-resume-model");
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    {
        loom::core::ToolRegistry initial_registry;
        loom::tools::register_runtime_tools(initial_registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        auto created = initial_registry.execute("team_create", loom::core::ToolInput::from_json(R"({
          "team_id": "restart-resume-team-id",
          "team_name": "Restart Resume Team",
          "start_native_agents": true,
          "members": [
            {"agent_id": "reviewer@restart-resume-team", "prompt": "Run the initial restart-resume task", "subagent_type": "general-purpose"}
          ]
        })"));
        ASSERT_TRUE(created.has_value());
        ASSERT_FALSE(created->is_error);
        ASSERT_TRUE(server.wait_for_request_count(1));
        ASSERT_TRUE(wait_for_native_agent_status(
            "reviewer@restart-resume-team",
            loom::tools::agent_runtime::NativeAgentStatus::Completed,
            std::chrono::seconds(3)));
    }

    auto completed = loom::tools::agent_runtime::native_agent_store().get("reviewer@restart-resume-team");
    ASSERT_TRUE(completed.has_value());
    EXPECT_EQ(completed->output, std::optional<std::string>{"late stream response"});
    ASSERT_TRUE(completed->team_name.has_value());
    EXPECT_EQ(*completed->team_name, "Restart Resume Team");
    EXPECT_FALSE(completed->name.has_value());

    // Wait for the detached background thread to finish update_teammate_completion_status()
    // after mark_completed(). The thread holds references to team store data.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry restarted_registry;
    loom::tools::register_runtime_tools(restarted_registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto delivered = restarted_registry.execute("send_message", loom::core::ToolInput::from_json(R"({
      "target_agent": "reviewer",
      "team_name": "Restart Resume Team",
      "content": "Continue after a fresh runtime registry",
      "summary": "restart resume follow-up"
    })"));
    ASSERT_TRUE(delivered.has_value());
    ASSERT_FALSE(delivered->is_error);
    ASSERT_FALSE(delivered->content.empty());
    EXPECT_NE(delivered->content.front().text.find("reviewer@restart-resume-team"), std::string::npos);
    EXPECT_NE(delivered->content.front().text.find("background resume started"), std::string::npos);
    ASSERT_TRUE(server.wait_for_request_count(2));
    ASSERT_TRUE(wait_for_native_agent_status(
        "reviewer@restart-resume-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));

    auto resumed = loom::tools::agent_runtime::native_agent_store().get("reviewer@restart-resume-team");
    ASSERT_TRUE(resumed.has_value());
    EXPECT_TRUE(resumed->pending_messages.empty());
    EXPECT_TRUE(std::ranges::any_of(resumed->transcript, [](const auto& line) {
        return line.find("Continue after a fresh runtime registry") != std::string::npos;
    }));
    auto last_request = server.last_body();
    ASSERT_TRUE(last_request.has_value());
    EXPECT_NE(last_request->find("Continue after a fresh runtime registry"), std::string::npos);

    auto inbox = loom::utils::read_inbox("reviewer", std::optional<std::string_view>{"Restart Resume Team"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ(inbox->front().text, "Continue after a fresh runtime registry");
    ASSERT_TRUE(inbox->front().summary.has_value());
    EXPECT_EQ(*inbox->front().summary, "restart resume follow-up");

    auto team = [&]() -> std::expected<loom::tools::Team*, loom::tools::TeamError> {
        for (int i = 0; i < 50; ++i) {
            auto t = loom::tools::global_team_store().get("restart-resume-team-id");
            if (t.has_value()) return t;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return loom::tools::global_team_store().get("restart-resume-team-id");
    }();
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));
    auto member = std::ranges::find_if((*team)->members, [](const auto& candidate) {
        return candidate.agent_id == "reviewer@restart-resume-team";
    });
    ASSERT_NE(member, (*team)->members.end());
    EXPECT_EQ(member->status, loom::tools::MemberStatus::Done);
    ASSERT_TRUE(member->last_result.has_value());
    EXPECT_NE(member->last_result->find("late stream response"), std::string::npos);

    // Allow detached background threads to exit before destroying the local HTTP server.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    { std::error_code ec; fs::remove_all(root, ec); }
}

TEST(Tools, RuntimeTeamCreateStartsNativeAgentsWithWorktreeIsolation) {
    if (std::system("git --version >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "git is required for team worktree isolation";
    }

    auto root = fs::temp_directory_path() / "loom_team_create_worktree_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream readme(root / "README.md");
        readme << "team worktree isolation\n";
    }
    ASSERT_EQ(std::system(std::format("git -C \"{}\" init -q --template=", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.email test@example.com", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" config user.name Test", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" add README.md", root.string()).c_str()), 0);
    ASSERT_EQ(std::system(std::format("git -C \"{}\" commit -q --no-verify -m init", root.string()).c_str()), 0);

    LocalPerTurnBashCommandMessagesServer server(
        "printf team-worktree > team_member_marker.txt; pwd",
        "team worktree complete");
    ASSERT_TRUE(server.valid());
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard api_key_guard("LOOM_API_KEY", "test-key");
    EnvironmentGuard base_url_guard("LOOM_BASE_URL", server.base_url());
    EnvironmentGuard model_guard("LOOM_MODEL", "team-worktree-test-model");
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto created = registry.execute("team_create", loom::core::ToolInput::from_json(std::format(R"({{
      "team_id": "worktree-team-id",
      "team_name": "Worktree Team",
      "start_native_agents": true,
      "cwd": "{}",
      "isolation": "worktree",
      "mode": "acceptEdits",
      "members": [
        {{"agent_id": "alpha@worktree-team", "prompt": "Write the worktree marker", "subagent_type": "general-purpose"}},
        {{"agent_id": "beta@worktree-team", "prompt": "Write the worktree marker", "subagent_type": "general-purpose"}}
      ]
    }})", loom::tools::agent::json_escape_string(root.string()))));

    ASSERT_TRUE(created.has_value());
    ASSERT_FALSE(created->is_error);
    ASSERT_FALSE(created->content.empty());
    auto created_json = loom::utils::json::parse(created->content.front().text);
    ASSERT_TRUE(created_json.has_value());
    auto created_root = created_json->root();
    EXPECT_EQ(created_root.get_string("team_name"), "Worktree Team");
    EXPECT_EQ(created_root.get_int("native_agents_started"), 2);

    ASSERT_TRUE(server.wait_for_request_count(4, std::chrono::seconds(5)));
    EXPECT_TRUE(wait_for_native_agent_status(
        "alpha@worktree-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));
    EXPECT_TRUE(wait_for_native_agent_status(
        "beta@worktree-team",
        loom::tools::agent_runtime::NativeAgentStatus::Completed,
        std::chrono::seconds(3)));

    auto alpha = loom::tools::agent_runtime::native_agent_store().get("alpha@worktree-team");
    auto beta = loom::tools::agent_runtime::native_agent_store().get("beta@worktree-team");
    ASSERT_TRUE(alpha.has_value());
    ASSERT_TRUE(beta.has_value());
    ASSERT_TRUE(alpha->worktree_path.has_value());
    ASSERT_TRUE(beta->worktree_path.has_value());
    ASSERT_TRUE(alpha->worktree_branch.has_value());
    ASSERT_TRUE(beta->worktree_branch.has_value());
    ASSERT_TRUE(alpha->cwd.has_value());
    ASSERT_TRUE(beta->cwd.has_value());
    ASSERT_TRUE(alpha->isolation.has_value());
    ASSERT_TRUE(beta->isolation.has_value());
    ASSERT_TRUE(alpha->mode.has_value());
    ASSERT_TRUE(beta->mode.has_value());
    ASSERT_TRUE(alpha->worktree_git_root.has_value());
    ASSERT_TRUE(beta->worktree_git_root.has_value());
    const auto alpha_path = fs::path{*alpha->worktree_path};
    const auto beta_path = fs::path{*beta->worktree_path};
    EXPECT_NE(alpha_path, beta_path);
    EXPECT_EQ(alpha_path, fs::weakly_canonical(root) / ".loom" / "worktrees" / "alpha_worktree-team");
    EXPECT_EQ(beta_path, fs::weakly_canonical(root) / ".loom" / "worktrees" / "beta_worktree-team");
    EXPECT_EQ(*alpha->cwd, alpha_path.string());
    EXPECT_EQ(*beta->cwd, beta_path.string());
    EXPECT_EQ(*alpha->isolation, "worktree");
    EXPECT_EQ(*beta->isolation, "worktree");
    EXPECT_EQ(*alpha->mode, "acceptEdits");
    EXPECT_EQ(*beta->mode, "acceptEdits");
    EXPECT_EQ(*alpha->worktree_git_root, fs::weakly_canonical(root).string());
    EXPECT_EQ(*beta->worktree_git_root, fs::weakly_canonical(root).string());
    const auto config_text = read_file(root / "teams" / "worktree-team" / "config.json");
    EXPECT_NE(config_text.find(R"("agentId":"alpha@worktree-team")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("agentId":"beta@worktree-team")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("tmuxPaneId":"in-process")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("backendType":"in-process")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("mode":"acceptEdits")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("cwd":")" + alpha_path.string() + R"(")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("cwd":")" + beta_path.string() + R"(")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("worktreePath":")" + alpha_path.string() + R"(")"), std::string::npos);
    EXPECT_NE(config_text.find(R"("worktreePath":")" + beta_path.string() + R"(")"), std::string::npos);
    EXPECT_TRUE(fs::exists(alpha_path / ".git"));
    EXPECT_TRUE(fs::exists(beta_path / ".git"));
    EXPECT_EQ(read_file(alpha_path / "team_member_marker.txt"), "team-worktree");
    EXPECT_EQ(read_file(beta_path / "team_member_marker.txt"), "team-worktree");
    EXPECT_FALSE(fs::exists(root / "team_member_marker.txt"));
    EXPECT_FALSE(alpha->worktree_cleanup_performed);
    EXPECT_FALSE(beta->worktree_cleanup_performed);
    EXPECT_TRUE(std::ranges::any_of(alpha->transcript, [&](const auto& entry) {
        return entry.find(alpha_path.string()) != std::string::npos;
    }));
    EXPECT_TRUE(std::ranges::any_of(beta->transcript, [&](const auto& entry) {
        return entry.find(beta_path.string()) != std::string::npos;
    }));

    auto deleted = registry.execute("team_delete", loom::core::ToolInput::from_json(R"({
      "team_id": "worktree-team-id"
    })"));
    ASSERT_TRUE(deleted.has_value());
    ASSERT_FALSE(deleted->is_error);
    ASSERT_FALSE(deleted->content.empty());
    EXPECT_NE(deleted->content.front().text.find("worktree_cleanup_attempts: 2"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("worktrees_retained: 2"), std::string::npos);

    auto remove_worktree = [&](const fs::path& path, std::string_view branch) {
        (void)std::system(std::format(
            "git -C {} worktree remove --force {} >/dev/null 2>&1",
            shell_quote_for_test(root.string()),
            shell_quote_for_test(path.string())).c_str());
        (void)std::system(std::format(
            "git -C {} branch -D {} >/dev/null 2>&1",
            shell_quote_for_test(root.string()),
            shell_quote_for_test(branch)).c_str());
    };
    remove_worktree(alpha_path, *alpha->worktree_branch);
    remove_worktree(beta_path, *beta->worktree_branch);
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    { std::error_code ec; fs::remove_all(root, ec); }
}

TEST(Tools, StandaloneTeamCreateAndDeleteDelegateToRuntimeTeamStore) {
    auto root = fs::temp_directory_path() / "loom_standalone_team_tools_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::tools::team_create::TeamCreateTool create_tool;
    auto created = create_tool.execute(loom::core::ToolInput::from_json(R"({
      "team_id": "standalone-team-id",
      "team_name": "Standalone Team",
      "members": [
        {"agent_id": "standalone-worker", "role": "worker"}
      ],
      "task_list": [
        {"id": "standalone-task", "description": "Exercise standalone team tool", "assigned_to": "standalone-worker"}
      ]
    })"));

    ASSERT_TRUE(created.has_value()) << created.error().format();
    ASSERT_FALSE(created->is_error);
    ASSERT_FALSE(created->content.empty());
    auto created_json = loom::utils::json::parse(created->content.front().text);
    ASSERT_TRUE(created_json.has_value());
    auto created_root = created_json->root();
    EXPECT_EQ(created_root.get_string("team_name"), "Standalone Team");
    EXPECT_EQ(created_root.get_string("team_id"), "standalone-team-id");
    EXPECT_EQ(created_root.get_string("lead_agent_id"), "team-lead@Standalone Team");
    EXPECT_TRUE(fs::exists(root / "teams" / "standalone-team-id.json"));
    EXPECT_TRUE(fs::exists(created_root.get_string("team_file_path")));
    EXPECT_TRUE(fs::exists(root / "teams" / "standalone-team" / "inboxes" / "standalone-worker.json"));
    EXPECT_TRUE(fs::exists(root / "teams" / "standalone-team" / "tasks.json"));

    auto team = loom::tools::global_team_store().get("standalone-team-id");
    ASSERT_TRUE(team.has_value()) << std::string(loom::tools::format_error(team.error()));
    ASSERT_EQ((*team)->members.size(), 1u);
    EXPECT_EQ((*team)->members.front().agent_id, "standalone-worker");
    ASSERT_EQ((*team)->task_list.size(), 1u);
    ASSERT_TRUE((*team)->task_list.front().assigned_to.has_value());
    EXPECT_EQ(*(*team)->task_list.front().assigned_to, "standalone-worker");

    loom::tools::team_delete::TeamDeleteTool delete_tool;
    auto deleted = delete_tool.execute(loom::core::ToolInput::from_json(R"({
      "team_id": "standalone-team-id"
    })"));

    ASSERT_TRUE(deleted.has_value()) << deleted.error().format();
    ASSERT_FALSE(deleted->is_error);
    ASSERT_FALSE(deleted->content.empty());
    EXPECT_NE(deleted->content.front().text.find("Deleted team Standalone Team (standalone-team-id)"), std::string::npos);
    EXPECT_FALSE(fs::exists(root / "teams" / "standalone-team-id.json"));
    EXPECT_FALSE(fs::exists(root / "teams" / "standalone-team"));

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeTeamDeleteCancelsNativeTeammatesAndCleansArtifacts) {
    auto root = fs::temp_directory_path() / "loom_team_delete_cleanup_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    EnvironmentGuard agent_runtime_dir_guard("LOOM_AGENT_RUNTIME_DIR", (root / "agents").string());
    EnvironmentGuard backend_guard("LOOM_TEAMMATE_BACKEND", "in-process");
    loom::utils::swarm_backends::BackendRegistry::reset();
    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto created = registry.execute("team_create", loom::core::ToolInput::from_json(R"({
      "team_id": "cleanup-team-id",
      "team_name": "cleanup-team",
      "members": [
        {"agent_id": "reviewer@cleanup-team", "role": "reviewer"}
      ]
    })"));
    ASSERT_TRUE(created.has_value());
    ASSERT_FALSE(created->is_error);

    auto executor = loom::utils::swarm_backends::BackendRegistry::get_teammate_executor(true);
    loom::utils::swarm_backends::TeammateSpawnConfig spawn_config{
        .name = "reviewer",
        .team_name = "cleanup-team",
        .color = std::nullopt,
        .plan_mode_required = false,
        .permission_mode = std::nullopt,
        .agent_type = std::nullopt,
        .prompt = "Review cleanup behavior",
        .cwd = root.string(),
        .model = std::nullopt,
        .system_prompt = std::nullopt,
        .system_prompt_mode = "default",
        .worktree_path = std::nullopt,
        .parent_session_id = {},
        .permissions = {},
        .allow_permission_prompts = false,
    };
    auto spawned = executor->spawn(spawn_config);
    ASSERT_TRUE(spawned.success) << spawned.error.value_or("spawn failed");
    EXPECT_TRUE(executor->is_active("reviewer@cleanup-team"));

    loom::tools::agent_runtime::NativeAgentRecord record{};
    record.agent_id = "reviewer@cleanup-team";
    record.agent_type = "reviewer";
    record.name = "reviewer";
    record.team_name = "cleanup-team";
    record.cwd = root.string();
    record.background = true;
    record.status = loom::tools::agent_runtime::NativeAgentStatus::Running;
    record.teammate_backend = "in-process";
    record.teammate_task_id = spawned.task_id;
    loom::tools::agent_runtime::native_agent_store().upsert(std::move(record));
    loom::tools::agent_runtime::native_agent_store().set_worktree_metadata(
        "reviewer@cleanup-team",
        (root / "missing-worktree").string(),
        "loom-agent-reviewer",
        "base",
        root.string());
    auto artifact_record = loom::tools::agent_runtime::native_agent_store().get("reviewer@cleanup-team");
    ASSERT_TRUE(artifact_record.has_value());
    ASSERT_TRUE(artifact_record->output_file_path.has_value());
    ASSERT_TRUE(artifact_record->transcript_path.has_value());
    ASSERT_TRUE(artifact_record->sidechain_jsonl_path.has_value());
    const auto output_artifact = fs::path{*artifact_record->output_file_path};
    const auto transcript_artifact = fs::path{*artifact_record->transcript_path};
    const auto sidechain_artifact = fs::path{*artifact_record->sidechain_jsonl_path};
    EXPECT_TRUE(fs::is_symlink(output_artifact));
    EXPECT_TRUE(fs::exists(transcript_artifact));
    EXPECT_TRUE(fs::exists(sidechain_artifact));

    auto mailbox = loom::utils::write_to_mailbox(
        "reviewer",
        loom::utils::TeammateMessage{
            .from = "team-lead",
            .text = "Initial message",
            .timestamp = {},
            .read = false,
            .color = std::nullopt,
            .summary = std::nullopt,
        },
        std::optional<std::string_view>{"cleanup-team"});
    ASSERT_TRUE(mailbox.has_value()) << mailbox.error();
    EXPECT_TRUE(fs::exists(root / "teams" / "cleanup-team" / "inboxes" / "reviewer.json"));
    EXPECT_TRUE(fs::exists(root / "teams" / "cleanup-team-id.json"));

    loom::tools::BashTool bash_tool;
    auto shell_task = bash_tool.execute(loom::core::ToolInput::from_json(R"({
      "command": "trap 'printf stopped-by-team-delete; exit 0' TERM; printf team-shell-ready; sleep 5",
      "run_in_background": true,
      "agentId": "reviewer@cleanup-team"
    })"));
    ASSERT_TRUE(shell_task.has_value());
    ASSERT_FALSE(shell_task->is_error);
    ASSERT_FALSE(shell_task->content.empty());
    auto shell_task_id = extract_background_task_id(shell_task->content.front().text);
    ASSERT_TRUE(shell_task_id.has_value()) << shell_task->content.front().text;
    auto shell_before_delete = loom::tools::bash::get_background_task_snapshot(*shell_task_id);
    ASSERT_TRUE(shell_before_delete.has_value());
    ASSERT_TRUE(shell_before_delete->agent_id.has_value());
    EXPECT_EQ(*shell_before_delete->agent_id, "reviewer@cleanup-team");
    EXPECT_TRUE(shell_before_delete->running);

    auto deleted = registry.execute("team_delete", loom::core::ToolInput::from_json(R"({
      "team_name": "cleanup-team"
    })"));
    ASSERT_TRUE(deleted.has_value());
    ASSERT_FALSE(deleted->is_error);
    ASSERT_FALSE(deleted->content.empty());
    EXPECT_NE(deleted->content.front().text.find("Deleted team cleanup-team (cleanup-team-id)"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("cancelled_agents: 1"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("teammate_terminations: 1"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("teammate_kills: 1"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("background_shell_tasks_stopped: 1"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("transcript_artifacts_removed: 3"), std::string::npos);
    EXPECT_NE(deleted->content.front().text.find("worktree_cleanup_attempts: 1"), std::string::npos);

    EXPECT_FALSE(executor->is_active("reviewer@cleanup-team"));
    auto shell_after_delete = loom::tools::bash::get_background_task_snapshot(*shell_task_id);
    ASSERT_TRUE(shell_after_delete.has_value());
    EXPECT_TRUE(shell_after_delete->stopped);
    EXPECT_FALSE(fs::exists(output_artifact));
    EXPECT_FALSE(fs::exists(transcript_artifact));
    EXPECT_FALSE(fs::exists(sidechain_artifact));
    EXPECT_FALSE(fs::exists(root / "teams" / "cleanup-team-id.json"));
    EXPECT_FALSE(fs::exists(root / "teams" / "cleanup-team"));
    auto missing_team = loom::tools::global_team_store().get("cleanup-team-id");
    EXPECT_FALSE(missing_team.has_value());

    auto cancelled = loom::tools::agent_runtime::native_agent_store().get("reviewer@cleanup-team");
    ASSERT_TRUE(cancelled.has_value());
    EXPECT_EQ(cancelled->status, loom::tools::agent_runtime::NativeAgentStatus::Cancelled);
    EXPECT_TRUE(cancelled->cancel_requested);
    EXPECT_TRUE(cancelled->worktree_cleanup_performed);
    EXPECT_FALSE(cancelled->worktree_path.has_value());
    ASSERT_TRUE(cancelled->error.has_value());
    EXPECT_EQ(*cancelled->error, "team deleted: cleanup-team");

    loom::tools::global_team_store().clear_for_testing();
    loom::tools::agent_runtime::native_agent_store().clear_for_testing();
    loom::utils::swarm_backends::BackendRegistry::reset();
    fs::remove_all(root);
}

TEST(Tools, TeamStoreUpdatesMemberStatusAndPersists) {
    auto root = fs::temp_directory_path() / "loom_team_member_status_test";
    fs::remove_all(root);
    fs::create_directories(root);
    EnvironmentGuard team_dir_guard("LOOM_TEAM_RUNTIME_DIR", (root / "teams").string());
    loom::tools::global_team_store().clear_for_testing();

    auto created = loom::tools::global_team_store().create("status-team", "Status Team", {
        loom::tools::TeamMember{
            .agent_id = "status-agent",
            .role = loom::tools::MemberRole::Reviewer,
            .status = loom::tools::MemberStatus::Working,
            .current_task = "task-1",
        },
    });
    ASSERT_TRUE(created.has_value()) << std::string(loom::tools::format_error(created.error()));

    auto updated = loom::tools::global_team_store().update_member_status(
        "Status Team",
        "status-agent",
        loom::tools::MemberStatus::Done,
        "review complete");
    ASSERT_TRUE(updated.has_value()) << std::string(loom::tools::format_error(updated.error()));

    loom::tools::global_team_store().clear_for_testing();
    auto restored = loom::tools::global_team_store().get("status-team");
    ASSERT_TRUE(restored.has_value()) << std::string(loom::tools::format_error(restored.error()));
    ASSERT_EQ((*restored)->members.size(), 1u);
    EXPECT_EQ((*restored)->members.front().status, loom::tools::MemberStatus::Done);
    EXPECT_FALSE((*restored)->members.front().current_task.has_value());
    ASSERT_TRUE((*restored)->members.front().last_result.has_value());
    EXPECT_EQ(*(*restored)->members.front().last_result, "review complete");

    auto missing = loom::tools::global_team_store().update_member_status(
        "status-team",
        "missing-agent",
        loom::tools::MemberStatus::Error,
        "failed");
    EXPECT_FALSE(missing.has_value());

    loom::tools::global_team_store().clear_for_testing();
    fs::remove_all(root);
}

TEST(Tools, RuntimeWorkflowExecutesJsonDefinition) {
    auto root = fs::temp_directory_path() / "loom_runtime_workflow_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto workflow_path = root / "workflow.json";
    {
        std::ofstream workflow(workflow_path);
        workflow << R"JSON({
  "name": "migration-check",
  "variables": {
    "greeting": "hello",
    "run_extra": "false"
  },
  "steps": [
    {"id": "assign", "type": "assign", "action": "target=world"},
    {"id": "log", "type": "log", "action": "${greeting}-${target}"},
    {"id": "condition", "type": "condition", "action": "true"},
    {"id": "command", "type": "command", "command": "printf cmd-${target}"},
    {"id": "repeat", "type": "loop", "command": "printf ${repeat.index}", "maxIterations": 3},
    {"id": "skipped", "type": "log", "action": "should-not-run", "condition": "${run_extra}"}
  ]
})JSON";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    loom::utils::json::JsonMutDoc doc;
    auto input = doc.object();
    input.add("file", doc.string(workflow_path.string()));
    doc.set_root(input);

    auto result = registry.execute("workflow", loom::core::ToolInput::from_json(doc.to_string()));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error) << result->content.front().text;
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Workflow migration-check completed"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("Steps executed: 5"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("Steps skipped: 1"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("hello-world"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("cmd-world"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("012"), std::string::npos);

    fs::remove_all(root);
}

TEST(Tools, TodoWriteParsesTypeScriptInputShape) {
    loom::tools::clear_all_todos_for_testing();
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto result = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "todos": [
        {"content":"Inspect migration gaps","status":"in_progress","activeForm":"Inspecting migration gaps"},
        {"content":"Run native validation","status":"pending","activeForm":"Running native validation"}
      ]
    })"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("2 total, 2 added"), std::string::npos);
}

TEST(Tools, TodoWriteClearsAllDoneReplacementLists) {
    loom::tools::clear_all_todos_for_testing();
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto initial = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "todos": [
        {"content":"Implement parser","status":"in_progress","activeForm":"Implementing parser"},
        {"content":"Verify parser","status":"pending","activeForm":"Verifying parser"}
      ]
    })"));
    ASSERT_TRUE(initial.has_value());
    ASSERT_FALSE(initial->is_error);

    auto completed = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "todos": [
        {"content":"Implement parser","status":"completed","activeForm":"Implementing parser"},
        {"content":"Verify parser","status":"completed","activeForm":"Verifying parser"}
      ]
    })"));

    ASSERT_TRUE(completed.has_value());
    ASSERT_FALSE(completed->is_error);
    ASSERT_FALSE(completed->content.empty());
    EXPECT_NE(completed->content.front().text.find("0 total"), std::string::npos);
}

TEST(Tools, TodoWriteScopesItemsByAgentId) {
    loom::tools::clear_all_todos_for_testing();
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto agent_a = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "agent_id": "agent-a",
      "todos": [
        {"content":"Implement agent A work","status":"in_progress","activeForm":"Implementing agent A work"}
      ]
    })"));
    ASSERT_TRUE(agent_a.has_value());
    ASSERT_FALSE(agent_a->is_error);

    auto agent_b = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "agentId": "agent-b",
      "todos": [
        {"content":"Implement agent B work","status":"in_progress","activeForm":"Implementing agent B work"}
      ]
    })"));
    ASSERT_TRUE(agent_b.has_value());
    ASSERT_FALSE(agent_b->is_error);

    EXPECT_EQ(loom::tools::todo_count_for_agent("agent-a"), 1U);
    EXPECT_EQ(loom::tools::todo_count_for_agent("agent-b"), 1U);

    auto clear_a = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "agent_id": "agent-a",
      "todos": [
        {"content":"Implement agent A work","status":"completed","activeForm":"Implementing agent A work"}
      ]
    })"));
    ASSERT_TRUE(clear_a.has_value());
    ASSERT_FALSE(clear_a->is_error);

    EXPECT_EQ(loom::tools::todo_count_for_agent("agent-a"), 0U);
    EXPECT_EQ(loom::tools::todo_count_for_agent("agent-b"), 1U);
}

TEST(Tools, TodoWriteCleanupRemovesAgentScopedTodos) {
    loom::tools::clear_all_todos_for_testing();
    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto result = registry.execute("todo_write", loom::core::ToolInput::from_json(R"({
      "agent_id": "cleanup-agent",
      "todos": [
        {"content":"Clean up scoped todos","status":"in_progress","activeForm":"Cleaning up scoped todos"}
      ]
    })"));
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    EXPECT_EQ(loom::tools::todo_count_for_agent("cleanup-agent"), 1U);

    EXPECT_TRUE(loom::tools::clear_todos_for_agent("cleanup-agent"));
    EXPECT_EQ(loom::tools::todo_count_for_agent("cleanup-agent"), 0U);
    EXPECT_EQ(loom::tools::todo_scope_count_for_testing(), 0U);
}

TEST(Tools, GlobFiltersByPattern) {
    auto root = fs::temp_directory_path() / "loom_glob_test";
    fs::remove_all(root);
    fs::create_directories(root / "src");
    {
        std::ofstream(root / "src" / "match.cpp") << "int main() {}\n";
        std::ofstream(root / "src" / "skip.txt") << "not source\n";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("Glob", loom::core::ToolInput::from_json(
        std::format(R"({{"pattern":"**/*.cpp","path":"{}"}})", root.string())));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    EXPECT_NE(result->content.front().text.find("match.cpp"), std::string::npos);
    EXPECT_EQ(result->content.front().text.find("skip.txt"), std::string::npos);
    fs::remove_all(root);
}

TEST(Tools, GrepUsesPathAndRegex) {
    auto root = fs::temp_directory_path() / "loom_grep_test";
    fs::remove_all(root);
    fs::create_directories(root / "src");
    {
        std::ofstream(root / "src" / "match.cpp") << "alpha_123\nbeta\n";
        std::ofstream(root / "src" / "skip.cpp") << "gamma\n";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("Grep", loom::core::ToolInput::from_json(
        std::format(R"({{"pattern":"alpha_[0-9]+","path":"{}"}})", (root / "src").string())));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    EXPECT_NE(result->content.front().text.find("match.cpp"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("alpha_123"), std::string::npos);
    EXPECT_EQ(result->content.front().text.find("gamma"), std::string::npos);
    fs::remove_all(root);
}

TEST(Tools, McpToolCallsNativeStdioServer) {
    auto root = fs::temp_directory_path() / "loom_mcp_stdio_test";
    fs::remove_all(root);
    fs::create_directories(root);
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
        capabilities: { tools: {}, resources: {} },
        serverInfo: { name: 'fixture', version: '1.0.0' }
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
    return;
  }
  if (request.method === 'resources/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: { resources: [{ uri: 'fixture://one', name: 'one', mimeType: 'text/plain' }] }
    });
    return;
  }
  if (request.method === 'resources/read') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: { contents: [{ uri: request.params.uri, mimeType: 'text/plain', text: 'resource-body' }] }
    });
  }
});
)JS";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "echo_fixture",
            .command = "node",
            .args = {server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());

    auto restarted = loom::tools::restart_native_mcp_server("echo_fixture");
    ASSERT_TRUE(restarted.has_value()) << restarted.error();
    EXPECT_EQ(restarted->status, "ready");
    ASSERT_EQ(restarted->tools.size(), 1u);
    EXPECT_EQ(restarted->tools.front().name, "echo");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("mcp", loom::core::ToolInput::from_json(
        R"({"server_name":"echo_fixture","tool_name":"echo","arguments":{"value":"hello"}})"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_EQ(result->content.front().text, "echo:hello");

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, NativeMcpRuntimeLoadsRemoteConfigWithOAuthFromConfigFiles) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_mcp_remote_config_runtime_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    // RFC-0001 B4: the core ConfigManager layer now reaches the runtime only
    // through the production-installed loader sink.
    CoreSettingsMcpLoaderGuard loader_guard;
    loom::commands::install_core_settings_mcp_loader();

    {
        std::ofstream config(root / ".loom" / "settings.json");
        config << R"JSON({
  "mcpServers": {
    "remote_fixture": {
      "type": "http",
      "url": "https://mcp.example.com/mcp",
      "headers": {"X-Test": "present"},
      "headersHelper": "node headers.js",
      "oauth": {
        "authServerMetadataUrl": "https://auth.example.com/.well-known/oauth-authorization-server",
        "callbackPort": 19485,
        "clientId": "client-1",
        "xaa": true
      }
    }
  }
})JSON";
    }

    {
        CurrentPathGuard cwd(root);
        auto reloaded = loom::tools::reload_native_mcp_servers_from_config();
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
        auto configured = loom::tools::native_mcp_configured_server("remote_fixture");
        ASSERT_TRUE(configured.has_value());
        EXPECT_EQ(configured->transport, loom::services::mcp::TransportType::StreamableHttp);
        EXPECT_EQ(configured->url, "https://mcp.example.com/mcp");
        EXPECT_EQ(configured->headers.at("X-Test"), "present");
        EXPECT_EQ(configured->headers_helper, "node headers.js");
        ASSERT_TRUE(configured->oauth.has_value());
        ASSERT_TRUE(configured->oauth->auth_server_metadata_url.has_value());
        EXPECT_EQ(*configured->oauth->auth_server_metadata_url, "https://auth.example.com/.well-known/oauth-authorization-server");
        ASSERT_TRUE(configured->oauth->callback_port.has_value());
        EXPECT_EQ(*configured->oauth->callback_port, 19485);
        ASSERT_TRUE(configured->oauth->client_id.has_value());
        EXPECT_EQ(*configured->oauth->client_id, "client-1");
        EXPECT_TRUE(configured->oauth->xaa);
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, CoreSettingsMcpLoaderFeedsLazyLoad) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_mcp_loader_feed_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    CoreSettingsMcpLoaderGuard loader_guard;

    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            loom::tools::NativeMcpConfiguredServer server;
            server.name = "loader_fixture";
            server.transport = loom::services::mcp::TransportType::StreamableHttp;
            server.url = "https://loader.example.com/mcp";
            server.headers.emplace("X-Loader", "yes");
            loom::tools::CoreSettingsMcpLayer layer;
            layer.servers.push_back(std::move(server));
            return layer;
        });

    {
        CurrentPathGuard cwd(root);
        auto reloaded = loom::tools::reload_native_mcp_servers_from_config();
        ASSERT_TRUE(reloaded.has_value()) << reloaded.error();

        auto configured = loom::tools::native_mcp_configured_server("loader_fixture");
        ASSERT_TRUE(configured.has_value());
        EXPECT_EQ(configured->name, "loader_fixture");
        EXPECT_EQ(configured->url, "https://loader.example.com/mcp");
        EXPECT_EQ(configured->transport, loom::services::mcp::TransportType::StreamableHttp);
        EXPECT_EQ(configured->headers.at("X-Loader"), "yes");
    }

    fs::remove_all(root);
}

TEST(Tools, CoreSettingsMcpLoaderErrorPropagates) {
    CoreSettingsMcpLoaderGuard loader_guard;
    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            return std::unexpected(std::string("boom"));
        });

    auto reloaded = loom::tools::reload_native_mcp_servers_from_config();
    ASSERT_FALSE(reloaded.has_value());
    EXPECT_NE(reloaded.error().find("boom"), std::string::npos) << reloaded.error();
}

// RFC-0001 B followup c17a — the composition-layer loader also carries the
// configured XAA IdP callback port (settings.xaaIdp.callbackPort) into the
// runtime, where the XAA login path forwards it to authenticate_xaa(). This
// pins that seam hermetically (a fake loader, no ConfigManager): a loader that
// yields a port surfaces it through native_mcp_xaa_callback_port(), and a
// loader with no port leaves it unset (random-port behavior preserved).
TEST(Tools, CoreSettingsLoaderCarriesXaaCallbackPort) {
    CoreSettingsMcpLoaderGuard loader_guard;

    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            loom::tools::CoreSettingsMcpLayer layer;
            layer.xaa_callback_port = 19485;
            return layer;
        });

    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());
    auto port = loom::tools::native_mcp_xaa_callback_port();
    ASSERT_TRUE(port.has_value());
    EXPECT_EQ(*port, 19485);

    // A loader with no configured port leaves it unset.
    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            return loom::tools::CoreSettingsMcpLayer{};
        });
    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());
    EXPECT_FALSE(loom::tools::native_mcp_xaa_callback_port().has_value());
}

// RFC-0001 followup c20 — the composition-layer loader also carries the IdP
// client secret (from the hardened ~/.config/loom/xaa/idp_tokens.json store,
// keyed by settings.xaaIdp.issuer) into the runtime, where the XAA login path
// forwards it to perform_mcp_oauth_flow(). This pins that seam hermetically
// (a fake loader, no ConfigManager, no filesystem): a loader that yields a
// secret surfaces it through native_mcp_xaa_idp_client_secret(), and a loader
// with no secret leaves it unset (PKCE-only behavior preserved). Same shape as
// CoreSettingsLoaderCarriesXaaCallbackPort above.
TEST(Tools, CoreSettingsLoaderCarriesXaaIdpClientSecret) {
    CoreSettingsMcpLoaderGuard loader_guard;

    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            loom::tools::CoreSettingsMcpLayer layer;
            layer.xaa_idp_client_secret = "test-secret";
            return layer;
        });

    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());
    auto secret = loom::tools::native_mcp_xaa_idp_client_secret();
    ASSERT_TRUE(secret.has_value());
    EXPECT_EQ(*secret, "test-secret");

    // A loader with no stored secret leaves it unset.
    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            return loom::tools::CoreSettingsMcpLayer{};
        });
    ASSERT_TRUE(loom::tools::reload_native_mcp_servers_from_config().has_value());
    EXPECT_FALSE(loom::tools::native_mcp_xaa_idp_client_secret().has_value());
}

// RFC-0001 B6: the additive snapshots sink receives exactly one vector per
// all_statuses call, carrying the same server ids/statuses the returned
// statuses show; once cleared it is never called again.
TEST(Tools, McpSnapshotsSinkReceivesOneVectorPerStatusRead) {
    McpSnapshotsSinkGuard sink_guard;

    // Synced stdio servers are never connected in this case, so their
    // snapshots are deterministically Disconnected ("not started") — no node
    // fixture and no detached auto-connect thread needed. sync() marks the
    // runtime loaded, so all_statuses() takes no ensure_loaded config path.
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "sink_alpha",
            .command = "true",
            .args = {},
            .env = {},
        },
        loom::tools::NativeMcpConfiguredServer{
            .name = "sink_beta",
            .command = "true",
            .args = {},
            .env = {},
        },
    }).has_value());

    struct Recorder {
        std::vector<std::vector<loom::services::mcp::McpServerSnapshot>> calls;
    } recorder;
    loom::tools::set_mcp_snapshots_sink(
        [&recorder](std::vector<loom::services::mcp::McpServerSnapshot> snapshots) {
            recorder.calls.push_back(std::move(snapshots));
        });

    auto expect_snapshots_match = [&recorder](
                                      const std::vector<loom::tools::NativeMcpServerStatus>& statuses,
                                      std::size_t call_index) {
        ASSERT_LT(call_index, recorder.calls.size());
        const auto& snaps = recorder.calls[call_index];
        ASSERT_EQ(snaps.size(), statuses.size());
        for (const auto& status : statuses) {
            const auto snapshot_it = std::ranges::find(snaps, status.name,
                &loom::services::mcp::McpServerSnapshot::name);
            ASSERT_NE(snapshot_it, snaps.end()) << status.name;
            EXPECT_EQ(snapshot_it->status, loom::services::mcp::ConnectionStatus::Disconnected);
            EXPECT_EQ(status.status, "not started");
        }
    };

    auto first = loom::tools::native_mcp_statuses();
    ASSERT_EQ(recorder.calls.size(), 1u);
    ASSERT_EQ(first.size(), 2u);
    expect_snapshots_match(first, 0);

    auto second = loom::tools::native_mcp_statuses();
    EXPECT_EQ(recorder.calls.size(), 2u);
    EXPECT_EQ(second.size(), 2u);
    expect_snapshots_match(second, 1);

    // Cleared sink: further status reads must not reach it.
    loom::tools::set_mcp_snapshots_sink(nullptr);
    auto third = loom::tools::native_mcp_statuses();
    auto fourth = loom::tools::native_mcp_statuses();
    EXPECT_EQ(recorder.calls.size(), 2u);
    EXPECT_EQ(third.size(), 2u);
    EXPECT_EQ(fourth.size(), 2u);
}

// RFC-0001 B6: the ensure_loaded_from_config() failure path still returns {}
// ahead of any sink fire — identical failure semantics to the pre-sink code.
TEST(Tools, McpSnapshotsSinkNotFiredWhenConfigLoadFails) {
    McpSnapshotsSinkGuard sink_guard;
    CoreSettingsMcpLoaderGuard loader_guard;
    loom::tools::set_core_settings_mcp_loader(
        []() -> std::expected<loom::tools::CoreSettingsMcpLayer, std::string> {
            return std::unexpected(std::string("sink-boom"));
        });

    int sink_calls = 0;
    loom::tools::set_mcp_snapshots_sink(
        [&sink_calls](std::vector<loom::services::mcp::McpServerSnapshot>) {
            ++sink_calls;
        });

    auto reloaded = loom::tools::reload_native_mcp_servers_from_config();
    ASSERT_FALSE(reloaded.has_value());

    auto statuses = loom::tools::native_mcp_statuses();
    EXPECT_TRUE(statuses.empty());
    EXPECT_EQ(sink_calls, 0);
}

TEST(Tools, McpAuthUsesNativeOAuthFlowForConfiguredRemoteServers) {
    EnvironmentGuard xaa_guard("LOOM_ENABLE_XAA", "0");
    loom::tools::NativeMcpConfiguredServer server;
    server.name = "auth_fixture";
    server.transport = loom::services::mcp::TransportType::StreamableHttp;
    server.url = "https://mcp.example.com/mcp";
    server.oauth = loom::services::mcp::McpOAuthConfig{.xaa = true};
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({server}).has_value());

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("mcp_auth", loom::core::ToolInput::from_json(
        R"({"server_name":"auth_fixture"})"));

    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Failed to start OAuth flow"), std::string::npos);
    EXPECT_NE(result->content.front().text.find("XAA is not enabled"), std::string::npos);

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
}

TEST(Tools, McpToolReturnsErrorWhenNativeServerIsMissing) {
    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
    auto result = registry.execute("mcp", loom::core::ToolInput::from_json(
        R"({"server_name":"missing_fixture","tool_name":"echo","arguments":{"value":"hello"}})"));

    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("MCP server not found"), std::string::npos);
}

TEST(Tools, McpRuntimeLoadsPluginManifestMcpServers) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_plugin_mcp_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard plugin_cache_guard(
        "LOOM_PLUGIN_CACHE_DIR",
        (root / ".loom" / "plugins").string()
    );

    const auto plugin_root = root / ".loom" / "plugins" / "mcp-fixture";
    fs::create_directories(plugin_root);
    const auto server_path = plugin_root / "server.js";
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
        serverInfo: { name: 'plugin-mcp-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{ name: 'plugin_echo', description: 'Echo from plugin MCP', inputSchema: { type: 'object' } }]
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
        content: [{
          type: 'text',
          text: [
            'plugin',
            request.params.arguments.value,
            process.env.PLUGIN_MCP_FIXTURE,
            process.env.PLUGIN_MCP_TOKEN,
            process.env.LOOM_PLUGIN_ROOT
          ].join(':')
        }]
      }
    });
  }
});
)JS";
    }
    {
        std::ofstream settings(root / ".loom" / "settings.json");
        settings << R"JSON({
  "pluginConfigs": {
    "mcp-fixture": {
      "options": {
        "suffix": "top-level",
        "token": "shared-token"
      },
      "mcpServers": {
        "echo": {
          "suffix": "configured",
          "token": "secret-token"
        }
      }
    }
  }
})JSON";
    }
    {
        std::ofstream defaults(plugin_root / ".mcp.json");
        defaults << R"JSON({
  "mcpServers": {
    "echo": {
      "command": "missing-plugin-mcp-command"
    }
  }
})JSON";
    }
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << std::format(R"JSON({{
  "name": "mcp-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "mcpServers": {{
    "echo": {{
      "type": "stdio",
      "command": "node",
      "args": ["${{LOOM_PLUGIN_ROOT}}/server.js"],
      "env": {{
        "PLUGIN_MCP_FIXTURE": "${{user_config.suffix}}",
        "PLUGIN_MCP_TOKEN": "${{user_config.token}}"
      }}
    }}
  }}
}})JSON");
    }
    {
        std::ofstream entry(plugin_root / "plugin.js");
        entry << "process.exit(0)\n";
    }

    {
        CurrentPathGuard cwd(root);
        auto servers = loom::tools::discover_plugin_native_mcp_servers();
        auto it = std::ranges::find_if(servers, [](const auto& server) {
            return server.name == "plugin:mcp-fixture:echo";
        });
        ASSERT_NE(it, servers.end());
        EXPECT_EQ(it->command, "node");
        ASSERT_EQ(it->args.size(), 1u);
        EXPECT_EQ(it->args.front(), server_path.string());
        EXPECT_EQ(it->env.at("PLUGIN_MCP_FIXTURE"), "configured");
        EXPECT_EQ(it->env.at("PLUGIN_MCP_TOKEN"), "secret-token");
        EXPECT_EQ(it->env.at("LOOM_PLUGIN_ROOT"), plugin_root.string());
        EXPECT_EQ(
            it->env.at("LOOM_PLUGIN_DATA"),
            (root / ".loom" / "plugins" / "data" / "mcp-fixture").string()
        );

        auto synced = loom::tools::sync_native_mcp_servers(std::move(servers));
        ASSERT_TRUE(synced.has_value()) << synced.error();

        auto restarted = loom::tools::restart_native_mcp_server("plugin:mcp-fixture:echo");
        ASSERT_TRUE(restarted.has_value()) << restarted.error();
        EXPECT_EQ(restarted->status, "ready");
        ASSERT_EQ(restarted->tools.size(), 1u);
        EXPECT_EQ(restarted->tools.front().name, "plugin_echo");

        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        auto result = registry.execute("mcp", loom::core::ToolInput::from_json(R"({
          "server_name": "plugin:mcp-fixture:echo",
          "tool_name": "plugin_echo",
          "arguments": {"value": "hello"}
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_EQ(
            result->content.front().text,
            "plugin:hello:configured:secret-token:" + plugin_root.string()
        );
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(Tools, McpRuntimeLoadsPluginMcpbServers) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_plugin_mcpb_test";
    fs::remove_all(root);
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard plugin_cache_guard(
        "LOOM_PLUGIN_CACHE_DIR",
        (root / ".loom" / "plugins").string()
    );

    const auto plugin_root = root / ".loom" / "plugins" / "mcpb-fixture";
    const auto bundle_src = root / "bundle-src";
    fs::create_directories(plugin_root);
    fs::create_directories(bundle_src);
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << R"JSON({
  "name": "mcpb-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "mcpServers": ["bundle.mcpb"]
})JSON";
    }
    {
        std::ofstream entry(plugin_root / "plugin.js");
        entry << "process.exit(0)\n";
    }
    {
        std::ofstream manifest(bundle_src / "manifest.json");
        manifest << R"JSON({
  "name": "bundle",
  "version": "1.0.0",
  "server": {
    "type": "stdio",
    "command": "node",
    "args": ["${LOOM_PLUGIN_ROOT}/server.js"],
    "env": {
      "PLUGIN_MCPB_VALUE": "from-bundle"
    }
  }
})JSON";
    }
    {
        std::ofstream server(bundle_src / "server.js");
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
        serverInfo: { name: 'mcpb-fixture', version: '1.0.0' }
      }
    });
    return;
  }
  if (request.method === 'tools/list') {
    send({
      jsonrpc: '2.0',
      id: request.id,
      result: {
        tools: [{ name: 'bundle_echo', description: 'Echo from MCPB', inputSchema: { type: 'object' } }]
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
        content: [{
          type: 'text',
          text: ['mcpb', request.params.arguments.value, process.env.PLUGIN_MCPB_VALUE, process.env.LOOM_PLUGIN_ROOT].join(':')
        }]
      }
    });
  }
});
)JS";
    }

    const auto bundle_path = plugin_root / "bundle.mcpb";
    const auto zip_command = std::format(
        "cd {} && zip -qr {} .",
        shell_quote_for_test(bundle_src.string()),
        shell_quote_for_test(bundle_path.string()));
    if (std::system(zip_command.c_str()) != 0) {
        GTEST_SKIP() << "zip command is not available";
    }

    {
        CurrentPathGuard cwd(root);
        auto servers = loom::tools::discover_plugin_native_mcp_servers();
        auto it = std::ranges::find_if(servers, [](const auto& server) {
            return server.name == "plugin:mcpb-fixture:bundle";
        });
        ASSERT_NE(it, servers.end());
        EXPECT_EQ(it->command, "node");
        ASSERT_EQ(it->args.size(), 1u);
        EXPECT_NE(it->args.front().find("mcpb/bundle/server.js"), std::string::npos);
        EXPECT_EQ(it->env.at("PLUGIN_MCPB_VALUE"), "from-bundle");
        EXPECT_NE(it->env.at("LOOM_PLUGIN_ROOT").find("mcpb/bundle"), std::string::npos);

        auto synced = loom::tools::sync_native_mcp_servers(std::move(servers));
        ASSERT_TRUE(synced.has_value()) << synced.error();
        auto restarted = loom::tools::restart_native_mcp_server("plugin:mcpb-fixture:bundle");
        ASSERT_TRUE(restarted.has_value()) << restarted.error();
        EXPECT_EQ(restarted->status, "ready");
        ASSERT_EQ(restarted->tools.size(), 1u);
        EXPECT_EQ(restarted->tools.front().name, "bundle_echo");

        loom::core::ToolRegistry registry;
        loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});
        auto result = registry.execute("mcp", loom::core::ToolInput::from_json(R"({
          "server_name": "plugin:mcpb-fixture:bundle",
          "tool_name": "bundle_echo",
          "arguments": {"value": "hello"}
        })"));

        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(result->is_error);
        ASSERT_FALSE(result->content.empty());
        EXPECT_NE(result->content.front().text.find("mcpb:hello:from-bundle:"), std::string::npos);
        EXPECT_NE(result->content.front().text.find("mcpb/bundle"), std::string::npos);
    }

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

TEST(ToolInput, HasFieldQueriesTopLevelKeysViaCanonicalJson) {
    auto input = loom::core::ToolInput::from_json(R"({"command":"run","args":[1,2]})");
    EXPECT_TRUE(loom::core::has_field(input, "command"));
    EXPECT_TRUE(loom::core::has_field(input, "args"));
    EXPECT_FALSE(loom::core::has_field(input, "missing"));
    EXPECT_FALSE(loom::core::has_field(input, ""));            // empty key is never present
    EXPECT_FALSE(loom::core::has_field(loom::core::ToolInput::from_json(R"({})"), "command"));
    EXPECT_FALSE(loom::core::has_field(loom::core::ToolInput::from_json("not json"), "command"));
}

// ---------------------------------------------------------------------------
// §13 #1: tests for extracted runtime subsystems
//   * loom.tools.runtime_shared_utils
//   * loom.tools.runtime_team_shared
//   * loom.tools.runtime_message_delivery
// ---------------------------------------------------------------------------

TEST(RuntimeSharedUtils, EscapeAndQuote) {
    namespace u = loom::tools::runtime_shared_utils;
    // XML escaping
    EXPECT_EQ(u::escape_xml("<&>"), "&lt;&amp;&gt;");
    EXPECT_EQ(u::escape_xml("a\"b'c"), "a&quot;b&apos;c");
    EXPECT_EQ(u::escape_xml("plain"), "plain");
    // POSIX shell quoting
    EXPECT_EQ(u::shell_quote("foo"), "'foo'");
    EXPECT_EQ(u::shell_quote("a'b"), "'a'\\''b'");
}

TEST(RuntimeSharedUtils, PathAndDirectorySanitisation) {
    namespace u = loom::tools::runtime_shared_utils;
    namespace fs = std::filesystem;

    EXPECT_EQ(u::safe_runtime_dir_component("Hello World!", "fallback"), "Hello_World_");
    EXPECT_EQ(u::safe_runtime_dir_component("", "fallback"), "fallback");

    // Segment-aware prefix checks
    EXPECT_TRUE(u::path_has_prefix(fs::path{"/tmp/team/a"}, fs::path{"/tmp/team"}));
    EXPECT_FALSE(u::path_has_prefix(fs::path{"/tmp/team-other"}, fs::path{"/tmp/team"}));

    EXPECT_TRUE(u::normalized_absolute_path(fs::path{"."}).is_absolute());
}

TEST(RuntimeSharedUtils, DeliveryIdsAndPendingMessageFormat) {
    namespace u = loom::tools::runtime_shared_utils;

    auto a = u::runtime_delivery_message_id();
    auto b = u::runtime_delivery_message_id();
    EXPECT_NE(a, b);
    EXPECT_TRUE(a.starts_with("msg-"));

    // Monotonic: lexicographic compare should hold because the numeric portion
    // is fixed-width nanoseconds; verify by parsing out the numeric part.
    auto as_num = [](std::string_view id) -> uint64_t {
        id.remove_prefix(std::string_view{"msg-"}.size());
        return std::stoull(std::string{id});
    };
    EXPECT_LT(as_num(a), as_num(b));

    auto formatted = u::format_agent_pending_user_message(
        "alice", loom::tools::MessagePriority::High, "hello");
    EXPECT_NE(formatted.find("alice"), std::string::npos);
    EXPECT_NE(formatted.find("high"), std::string::npos);
    EXPECT_NE(formatted.find("hello"), std::string::npos);
}

TEST(RuntimeTeamShared, ParseHelpersAndS2Structs) {
    namespace ts = loom::tools::runtime_team_shared;
    namespace json = loom::utils::json;

    // json_string (3 call sites in runtime_registry depend on exact semantics)
    auto d1 = json::parse(R"({"name":"x"})");
    ASSERT_TRUE(d1.has_value());
    EXPECT_EQ(ts::json_string(d1->root(), "name"), "x");
    auto d2 = json::parse(R"({"n":1})");
    ASSERT_TRUE(d2.has_value());
    EXPECT_EQ(ts::json_string(d2->root(), "name"), std::nullopt);
    auto d3 = json::parse("not json");
    EXPECT_FALSE(d3.has_value());

    // TeamMemberRole parsing
    EXPECT_EQ(ts::parse_team_member_role("leader"), loom::tools::MemberRole::Leader);
    EXPECT_EQ(ts::parse_team_member_role("worker"), loom::tools::MemberRole::Worker);
    EXPECT_EQ(ts::parse_team_member_role("????"), loom::tools::MemberRole::Worker);

    // Team creation aggregates are default constructible and carry data.
    ts::TeamDeletionCleanupSummary cleanup{};
    EXPECT_EQ(cleanup.native_agents_seen, 0u);
    cleanup.cancelled_agents = 5;
    EXPECT_EQ(cleanup.cancelled_agents, 5u);

    ts::TeamCreationArtifactsSummary creation{};
    EXPECT_TRUE(creation.team_dir.empty());
    EXPECT_FALSE(creation.team_config_written);

    ts::TeamConfigMemberRuntimeState state{};
    state.is_active = true;
    state.color = "blue";
    EXPECT_TRUE(state.is_active.value());
    EXPECT_EQ(state.color.value(), "blue");
}

TEST(RuntimeTeamShared, S2HelpersAndS3Writers) {
    namespace ts = loom::tools::runtime_team_shared;
    namespace fs = std::filesystem;

    // Directory / name helpers
    EXPECT_EQ(ts::ts_sanitized_team_dir_name("My Team!", "team"), "my-team-");
    EXPECT_EQ(ts::team_lead_agent_id("t1"), "team-lead@t1");
    EXPECT_EQ(ts::team_agent_name_from_id("worker@t1"), "worker");
    EXPECT_EQ(ts::team_member_inbox_name("worker@t1"), "worker");

    // Lifecycle predicate support
    using TS = loom::tools::agent_runtime::NativeAgentStatus;
    EXPECT_TRUE(ts::is_terminal_default(TS::Completed));
    EXPECT_TRUE(ts::is_terminal_default(TS::Failed));
    EXPECT_TRUE(ts::is_terminal_default(TS::Cancelled));
    EXPECT_FALSE(ts::is_terminal_default(TS::Queued));
    EXPECT_FALSE(ts::is_terminal_default(TS::Running));

    // Filesystem writers: create temp dirs and verify the output files exist
    // with a sane payload (JSON-parsable).
    std::error_code ec;
    auto tmp = fs::temp_directory_path(ec) /
        ("loom_s3_test_" + std::to_string(std::rand()));
    std::vector<fs::path> cleanup_paths{tmp};
    auto guard = std::shared_ptr<void>(nullptr, [&](void*) {
        for (auto& p : cleanup_paths) fs::remove_all(p, ec);
    });

    auto inbox = tmp / "inboxes" / "alice.json";
    EXPECT_TRUE(ts::write_empty_inbox_if_missing(inbox));
    EXPECT_TRUE(fs::exists(inbox, ec));
    {
        std::ifstream f(inbox);
        std::string s; std::getline(f, s);
        EXPECT_EQ(s, "[]");
    }

    // Empty task snapshot writes an empty array.
    auto tasks = tmp / "tasks.json";
    EXPECT_TRUE(ts::write_team_task_snapshot(tasks, {}));
    auto read_file = [](const fs::path& p) -> std::string {
        std::ifstream ifs(p);
        std::stringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    };
    auto parsed = loom::utils::json::parse(read_file(tasks));
    EXPECT_TRUE(parsed && parsed->root().is_arr());

    // Team config writer: build a Team with one member, verify the resulting
    // JSON is parseable and contains the lead agent id.
    loom::tools::Team team{.id = "id-t", .name = "t"};
    team.members.push_back({
        .agent_id = "worker@t",
        .role = loom::tools::MemberRole::Worker,
        .status = loom::tools::MemberStatus::Idle,
    });
    auto cfg = tmp / "settings.json";
    EXPECT_TRUE(ts::write_team_config_file(cfg, team));
    auto cfg_parsed = loom::utils::json::parse(read_file(cfg));
    ASSERT_TRUE(cfg_parsed.has_value());
    EXPECT_EQ(std::string(cfg_parsed->root().get("leadAgentId").as_str()), "team-lead@t");
    EXPECT_EQ(std::string(cfg_parsed->root().get("name").as_str()), "t");
}

TEST(RuntimeMessageDelivery, PeerAddressAndCrossSessionPayloads) {
    namespace md = loom::tools::runtime_message_delivery;

    auto uds = md::parse_runtime_peer_address("uds:/tmp/foo.sock");
    EXPECT_EQ(uds.scheme, md::RuntimePeerAddressScheme::Uds);
    EXPECT_EQ(uds.target, "/tmp/foo.sock");

    auto bridge = md::parse_runtime_peer_address("bridge:abc-def");
    EXPECT_EQ(bridge.scheme, md::RuntimePeerAddressScheme::Bridge);
    EXPECT_EQ(bridge.target, "abc-def");

    auto plain = md::parse_runtime_peer_address("teammate-name");
    EXPECT_EQ(plain.scheme, md::RuntimePeerAddressScheme::Other);
    EXPECT_EQ(plain.target, "teammate-name");

    // Small JSON object builder — escape correctness is critical.
    auto obj = md::build_runtime_json_object(
        {{"greeting", "hello \"world\""}, {"path", "a/b"}},
        {{"ok", true}});
    auto parsed = loom::utils::json::parse(obj);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(std::string(parsed->root().get("greeting").as_str()), "hello \"world\"");
    EXPECT_EQ(std::string(parsed->root().get("path").as_str()), "a/b");
    EXPECT_TRUE(parsed->root().get("ok").as_bool());

    // Cross-session prompt wraps sender safely.
    auto prompt = md::build_cross_session_prompt("alice", "say <hi>");
    EXPECT_NE(prompt.find("from=\"alice\""), std::string::npos);
    EXPECT_NE(prompt.find("&lt;hi&gt;"), std::string::npos);

    // UDS payload has a trailing newline for line-based transport.
    auto uds_payload = md::build_uds_cross_session_payload("alice", "hello");
    EXPECT_TRUE(uds_payload.ends_with('\n'));
    EXPECT_NE(uds_payload.find("cross_session_message"), std::string::npos);
}

TEST(RuntimeMessageDelivery, StructuredPayloads) {
    namespace md = loom::tools::runtime_message_delivery;
    namespace json = loom::utils::json;

    // Shutdown request generates a typed payload and a request_id.
    auto s1_d = json::parse(R"({"type":"shutdown_request","reason":"wind down"})");
    ASSERT_TRUE(s1_d.has_value());
    auto built = md::build_structured_send_message_payload(s1_d->root(), "lead");
    ASSERT_TRUE(built.has_value());
    EXPECT_FALSE(built->request_id->empty());
    EXPECT_TRUE(built->text.starts_with('{'));
    auto parsed = loom::utils::json::parse(built->text);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(std::string(parsed->root().get("type").as_str()), "shutdown_request");
    EXPECT_EQ(std::string(parsed->root().get("from").as_str()), "lead");
    EXPECT_EQ(std::string(parsed->root().get("reason").as_str()), "wind down");

    // Shutdown response with explicit approval.
    auto s2_d = json::parse(R"({"type":"shutdown_response","request_id":"R-123","approve":false,"reason":"still working"})");
    ASSERT_TRUE(s2_d.has_value());
    auto denied = md::build_structured_send_message_payload(s2_d->root(), "worker");
    ASSERT_TRUE(denied.has_value());
    auto parsed2 = loom::utils::json::parse(denied->text);
    ASSERT_TRUE(parsed2.has_value());
    EXPECT_EQ(std::string(parsed2->root().get("type").as_str()), "shutdown_rejected");
    EXPECT_EQ(std::string(parsed2->root().get("reason").as_str()), "still working");

    // Plan approval response round-trips all optional fields.
    auto s3_d = json::parse(R"({"type":"plan_approval_response","request_id":"P-1","approve":true,"feedback":"looks good","permission_mode":"ask"})");
    ASSERT_TRUE(s3_d.has_value());
    auto plan = md::build_structured_send_message_payload(s3_d->root(), "lead");
    ASSERT_TRUE(plan.has_value());
    auto parsed3 = loom::utils::json::parse(plan->text);
    ASSERT_TRUE(parsed3.has_value());
    EXPECT_TRUE(parsed3->root().get("approved").as_bool());
    EXPECT_EQ(std::string(parsed3->root().get("feedback").as_str()), "looks good");
    EXPECT_EQ(std::string(parsed3->root().get("permissionMode").as_str()), "ask");

    // Semantic bool parsing via the dispatcher path: string "approved" works.
    auto s4_d = json::parse(R"({"type":"shutdown_response","request_id":"R-2","approved":"approved"})");
    ASSERT_TRUE(s4_d.has_value());
    auto approved = md::build_structured_send_message_payload(s4_d->root(), "worker");
    ASSERT_TRUE(approved.has_value());
    auto parsed4 = loom::utils::json::parse(approved->text);
    ASSERT_TRUE(parsed4.has_value());
    EXPECT_EQ(std::string(parsed4->root().get("type").as_str()), "shutdown_approved");

    // Invalid types are rejected.
    auto s5_d = json::parse(R"({"type":"bogus"})");
    ASSERT_TRUE(s5_d.has_value());
    auto bad = md::build_structured_send_message_payload(s5_d->root(), "lead");
    EXPECT_FALSE(bad.has_value());
    EXPECT_NE(bad.error().find("unsupported"), std::string::npos);
}

TEST(RuntimeMessageDelivery, SessionIdAndEnvSafety) {
    namespace md = loom::tools::runtime_message_delivery;

    EXPECT_TRUE(md::is_safe_runtime_session_id("abc-123_X"));
    EXPECT_FALSE(md::is_safe_runtime_session_id("a/b"));
    EXPECT_FALSE(md::is_safe_runtime_session_id(""));

    EXPECT_EQ(md::strip_runtime_trailing_slashes("/foo/bar//"), "/foo/bar");

    // Credential check returns false when neither env var is set.
    ::unsetenv("LOOM_API_KEY");
    ::unsetenv("LOOM_AUTH_TOKEN");
    EXPECT_FALSE(md::runtime_has_agent_api_credentials());

    // Resume cwd prefers worktree when it exists; falls back to cwd otherwise.
    loom::tools::agent_runtime::NativeAgentRecord rec{};
    rec.cwd = "/tmp";
    EXPECT_EQ(md::native_agent_resume_cwd(rec), std::optional<std::string>{"/tmp"});
    rec.worktree_path = "/no/such/dir/does-not-exist-12345";
    EXPECT_EQ(md::native_agent_resume_cwd(rec), std::optional<std::string>{"/tmp"});
}

TEST(RuntimeMessageDelivery, SendMessageDispatcherRejectsMalformedInput) {
    namespace md = loom::tools::runtime_message_delivery;
    using loom::tools::agent_runtime::NativeAgentStatus;

    // Missing recipient.
    auto r1 = md::execute_send_message("{}", nullptr, [](NativeAgentStatus) { return false; });
    ASSERT_TRUE(r1.has_value());
    EXPECT_TRUE(r1->is_error);

    // Missing content / message.
    auto r2 = md::execute_send_message(
        R"({"to":"someone"})", nullptr, [](NativeAgentStatus) { return false; });
    ASSERT_TRUE(r2.has_value());
    EXPECT_TRUE(r2->is_error);
}

// ─── P1-01/02: REPLTool persistent sessions + bridge tests ─
TEST(ReplTool, CreateAndEvalPython) {
    if (::system("command -v python3 >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "python3 not available on PATH";
    }

    auto created = loom::tools::repl::create_session("python3", /*requested_id=*/"ut-python-basic");
    ASSERT_TRUE(created.has_value()) << created.error();
    ReplSessionGuard guard(*created);

    auto eval = loom::tools::repl::eval_session(*created, "1+1\n", std::chrono::seconds(10));
    ASSERT_TRUE(eval.has_value()) << eval.error();
    auto [out, err] = *eval;
    EXPECT_TRUE(err.empty()) << "stderr: " << err;
    EXPECT_NE(out.find("2"), std::string::npos) << "stdout was: '" << out << "'";
}

TEST(ReplTool, PersistsCrossEvals) {
    if (::system("command -v python3 >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "python3 not available on PATH";
    }

    auto created = loom::tools::repl::create_session("python3", "ut-python-persist");
    ASSERT_TRUE(created.has_value()) << created.error();
    ReplSessionGuard guard(*created);

    auto r1 = loom::tools::repl::eval_session(*created, "x = 5\n", std::chrono::seconds(5));
    ASSERT_TRUE(r1.has_value()) << r1.error();

    auto r2 = loom::tools::repl::eval_session(*created, "x * 3\n", std::chrono::seconds(5));
    ASSERT_TRUE(r2.has_value()) << r2.error();
    auto [out, err] = *r2;
    EXPECT_TRUE(err.empty());
    EXPECT_NE(out.find("15"), std::string::npos) << "stdout was: '" << out << "'";

    auto hist = loom::tools::repl::history_session(*created);
    EXPECT_GE(hist.size(), 2u);
}

TEST(ReplTool, SwitchLanguages) {
    if (::system("command -v node >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "node not available on PATH";
    }

    auto created = loom::tools::repl::create_session("node", "ut-node-basic");
    ASSERT_TRUE(created.has_value()) << created.error();
    ReplSessionGuard guard(*created);

    auto eval = loom::tools::repl::eval_session(*created, "2**10\n", std::chrono::seconds(10));
    ASSERT_TRUE(eval.has_value()) << eval.error();
    auto [out, err] = *eval;
    (void)err;
    EXPECT_NE(out.find("1024"), std::string::npos) << "stdout was: '" << out << "'";
}

TEST(ReplTool, MissingBinary) {
    auto created = loom::tools::repl::create_session("nonesuchlang", "ut-nonexistent");
    ASSERT_FALSE(created.has_value());
    EXPECT_FALSE(created.error().empty());
}

TEST(ReplTool, TimeoutKillsSession) {
    if (::system("command -v python3 >/dev/null 2>&1") != 0) {
        GTEST_SKIP() << "python3 not available on PATH";
    }

    auto created = loom::tools::repl::create_session("python3", "ut-python-timeout");
    ASSERT_TRUE(created.has_value()) << created.error();
    ReplSessionGuard guard(*created);

    auto eval = loom::tools::repl::eval_session(
        *created, "import time; time.sleep(10)\n", std::chrono::milliseconds{150});
    ASSERT_FALSE(eval.has_value());
    EXPECT_NE(eval.error().find("timed out"), std::string::npos)
        << "error was: " << eval.error();
}

// ─── P0-02: SkillTool validation + frontmatter parse tests ───────────────────
TEST(SkillTool, ValidatesFrontmatter) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    constexpr std::string_view content = R"md(---
name: sample-skill
version: 1.0.0
description: "Sample skill for testing"
author: test
tags: [code, test]
allowed_tools: ["bash","file_read","file_write","grep","glob"]
model: "opus"
effort: 3
context_modifiers:
  effort: 4
  max_tokens: 8192
safe: true
should_use_sandbox: true
---
# Sample Body

Hello, world.
)md";
    root.write_skill("sample-skill.md", content);

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"execute","skill_path":"sample-skill"})");
    ASSERT_TRUE(r.has_value()) << "expected value, got error: " << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    EXPECT_TRUE(rootv.get("ok").is_bool() && rootv.get("ok").as_bool());
    auto skill = rootv.get("skill");
    EXPECT_TRUE(skill.is_obj());
    EXPECT_EQ(std::string(skill.get("name").as_str()), "sample-skill");
    EXPECT_EQ(std::string(skill.get("version").as_str()), "1.0.0");
    auto plan = rootv.get("execution_plan");
    EXPECT_TRUE(plan.is_obj()) << "execution_plan missing";
    EXPECT_TRUE(plan.get("steps").is_arr());
    auto at = plan.get("allowed_tools");
    EXPECT_TRUE(at.is_arr());
    EXPECT_EQ(at.size(), 5u);
    if (plan.get("effort").is_num()) {
        EXPECT_GE(static_cast<int>(plan.get("effort").as_int()), 3);
    }
    EXPECT_EQ(std::string(plan.get("model_override").as_str()), "opus");
}

// ─── SkillTool list / search / install / update actions ──────────────────────
TEST(SkillTool, ListActionReturnsCatalog) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    fs::create_directories(root.path / "code-skill");
    fs::create_directories(root.path / "docs-skill");
    root.write_skill("code-skill/SKILL.md",
        "---\nname: code-skill\ndescription: \"Writes code\"\n---\n# body\n");
    root.write_skill("docs-skill/SKILL.md",
        "---\nname: docs-skill\ndescription: \"Writes docs\"\n---\n# body\n");

    auto r = loom::tools::skill::execute_skill_tool_simple(R"({"action":"list"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    EXPECT_TRUE(rootv.get("ok").as_bool());
    EXPECT_TRUE(rootv.get("skills").is_arr());
    EXPECT_EQ(rootv.get("skills").size(), 2u);
    EXPECT_EQ(rootv.get("count").as_int(), 2);
}

TEST(SkillTool, SearchActionFiltersByName) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    fs::create_directories(root.path / "code-skill");
    fs::create_directories(root.path / "docs-skill");
    root.write_skill("code-skill/SKILL.md",
        "---\nname: code-skill\ndescription: \"Writes code\"\n---\n# body\n");
    root.write_skill("docs-skill/SKILL.md",
        "---\nname: docs-skill\ndescription: \"Writes docs\"\n---\n# body\n");

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"search","skill_path":"code"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    EXPECT_TRUE(rootv.get("ok").as_bool());
    EXPECT_EQ(rootv.get("count").as_int(), 1);
}

TEST(SkillTool, InstallActionReturnsHonestError) {
    using namespace skill_test;
    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"install","skill_path":"some-skill"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    EXPECT_FALSE(rootv.get("ok").as_bool());
    EXPECT_TRUE(rootv.get("error").is_str());
    EXPECT_NE(std::string(rootv.get("error").as_str()).find("source"), std::string::npos);
}

TEST(SkillTool, UpdateActionReturnsHonestError) {
    using namespace skill_test;
    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"update","skill_path":"some-skill"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    EXPECT_FALSE(rootv.get("ok").as_bool());
    EXPECT_TRUE(rootv.get("error").is_str());
}

TEST(SkillTool, BlocksPathTraversal) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    {
        std::ofstream of(root.temp_home / "outside.md");
        of << "# outside\n";
    }

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"execute","skill_path":"../../../etc/passwd"})");
    EXPECT_FALSE(r.has_value());
    if (!r.has_value()) {
        std::string err = r.error();
        std::string lower;
        for (char c : err) lower.push_back(std::tolower(static_cast<unsigned char>(c)));
        EXPECT_TRUE(lower.find("unsafe") != std::string::npos ||
                    lower.find("traversal") != std::string::npos ||
                    lower.find("not exist") != std::string::npos)
            << "got error: " << err;
    }

    auto r2 = loom::tools::skill::execute_skill_tool_simple(
        R"({"action":"execute","skill_path":"/etc/passwd"})");
    EXPECT_FALSE(r2.has_value());
    if (!r2.has_value()) {
        std::string err = r2.error();
        std::string lower;
        for (char c : err) lower.push_back(std::tolower(static_cast<unsigned char>(c)));
        EXPECT_TRUE(lower.find("unsafe") != std::string::npos ||
                    lower.find("traversal") != std::string::npos)
            << "got error: " << err;
    }
}

TEST(SkillTool, TemplateExpansionWorks) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    constexpr std::string_view content = R"md(---
name: template-test
version: 0.1.0
---
Body start
ARGS: ${ARGUMENTS}
DIR: ${LOOM_SKILL_DIR}
SID: ${LOOM_SESSION_ID}
MYARG: ${target_file}
Body end
)md";
    root.write_skill("tmpl.md", content);

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"tmpl",
             "arguments":{"target_file":"foo.cpp","verbose":"1"},
             "session_id":"sess-abc-123"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    auto body = std::string(rootv.get("inline_skill_body").as_str());
    EXPECT_NE(body.find(R"("target_file": "foo.cpp")"), std::string::npos) << body;
    EXPECT_NE(body.find(R"("verbose": "1")"), std::string::npos) << body;
    std::string want_dir = root.skills_dir.string();
    EXPECT_NE(body.find(want_dir), std::string::npos)
        << "expected dir '" << want_dir << "' not in body: " << body;
    EXPECT_NE(body.find("SID: sess-abc-123"), std::string::npos) << body;
    EXPECT_NE(body.find("MYARG: foo.cpp"), std::string::npos) << body;
    auto ea = rootv.get("expanded_args");
    EXPECT_TRUE(ea.is_obj());
    EXPECT_EQ(std::string(ea.get("target_file").as_str()), "foo.cpp");
}

TEST(SkillTool, ContextModifiersCascade) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    constexpr std::string_view fm = R"md(---
name: cascade
version: 0.1.0
effort: 3
allowed_tools: ["bash", "file_read"]
model: "sonnet"
---
body
)md";
    root.write_skill("cascade.md", fm);

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"cascade","context_modifiers":{"effort":"5"}})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    auto plan = rootv.get("execution_plan");
    ASSERT_TRUE(plan.is_obj());
    ASSERT_TRUE(plan.get("effort").is_num());
    EXPECT_EQ(static_cast<int>(plan.get("effort").as_int()), 5);

    auto r2 = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"cascade"})");
    ASSERT_TRUE(r2.has_value()) << r2.error();
    auto [d2, rv2] = parse_resp(*r2);
    ASSERT_TRUE(d2);
    auto p2 = rv2.get("execution_plan");
    ASSERT_TRUE(p2.get("effort").is_num());
    EXPECT_EQ(static_cast<int>(p2.get("effort").as_int()), 3);
}

TEST(SkillTool, MissingFileReturnsError) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"definitely-not-installed-skill-xyz"})");
    EXPECT_FALSE(r.has_value());
    if (!r.has_value()) EXPECT_FALSE(r.error().empty());
}

TEST(SkillTool, ForkModeAddsFlag) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    constexpr std::string_view fm = R"md(---
name: forky
version: 0.1.0
fork: true
fork_reason: "Needs full sandbox"
---
body
)md";
    root.write_skill("forky.md", fm);

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"forky"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    auto plan = rootv.get("execution_plan");
    ASSERT_TRUE(plan.is_obj());
    auto fcr = plan.get("fork_context_required");
    EXPECT_TRUE(fcr.is_bool() && fcr.as_bool());
    auto fr = plan.get("fork_reason");
    EXPECT_TRUE(fr.is_str());
    EXPECT_EQ(std::string(fr.as_str()), "Needs full sandbox");
}

TEST(SkillTool, SafePropertiesFiltered) {
    using namespace skill_test;
    TempSkillRoot root;
    CurrentPathGuard cwd_guard(root.temp_home);
    constexpr std::string_view fm = R"md(---
name: dangerous-skill
version: 0.0.1
description: "has unsafe keys"
author: test
SOME_UNSAFE_DANGEROUS_PROPERTY: evil
SECRET_TOKEN: "hunter2"
---
body
)md";
    root.write_skill("unsafe.md", fm);

    auto r = loom::tools::skill::execute_skill_tool_simple(
        R"({"skill_path":"unsafe"})");
    ASSERT_TRUE(r.has_value()) << r.error();
    auto [doc, rootv] = parse_resp(*r);
    ASSERT_TRUE(doc);
    auto sf = rootv.get("frontmatter_safe");
    ASSERT_TRUE(sf.is_obj());
    EXPECT_EQ(std::string(sf.get("name").as_str()), "dangerous-skill");
    EXPECT_EQ(std::string(sf.get("version").as_str()), "0.0.1");
    EXPECT_EQ(std::string(sf.get("description").as_str()), "has unsafe keys");
    EXPECT_EQ(std::string(sf.get("author").as_str()), "test");
    EXPECT_FALSE(sf.get("SOME_UNSAFE_DANGEROUS_PROPERTY").valid())
        << "unsafe key leaked into frontmatter_safe";
    EXPECT_FALSE(sf.get("SECRET_TOKEN").valid())
        << "SECRET_TOKEN leaked into frontmatter_safe";
}

TEST(SkillTool, MalformedInputJson) {
    auto r = loom::tools::skill::execute_skill_tool_simple("this is not json {{{");
    EXPECT_FALSE(r.has_value());
    if (!r.has_value()) EXPECT_FALSE(r.error().empty());
    auto r2 = loom::tools::skill::execute_skill_tool_simple("");
    EXPECT_FALSE(r2.has_value());
    auto r3 = loom::tools::skill::execute_skill_tool_simple("{}");
    EXPECT_FALSE(r3.has_value());
}

// Regression for the worktree command-injection fix (audit 2026-06-18 P0/W1):
// branch_name and the worktree path must be POSIX single-quoted before being
// interpolated into a `git worktree ...` std::system() call. Pinning the exact
// quoted output catches any future regression that drops the quoting.
TEST(WorktreeShellQuote, EscapesInjectionPayloads) {
    using loom::tools::worktree_shell_quote;
    // Empty / benign inputs round-trip as simple quoted tokens.
    EXPECT_EQ(worktree_shell_quote(""), "''");
    EXPECT_EQ(worktree_shell_quote("feature-branch"), "'feature-branch'");
    // Embedded single quote -> POSIX '\'' escape idiom.
    EXPECT_EQ(worktree_shell_quote("a'b"), "'a'\\''b'");
    // Command-injection payloads become inert single-quoted tokens: the text
    // is preserved but wrapped so the shell treats it as one literal argument.
    EXPECT_EQ(worktree_shell_quote("; rm -rf /"), "'; rm -rf /'");
    EXPECT_EQ(worktree_shell_quote("$(whoami)"), "'$(whoami)'");
    EXPECT_EQ(worktree_shell_quote("`touch /tmp/pwned`"), "'`touch /tmp/pwned`'");
    // Every result is wrapped in outer single quotes.
    for (const char* s : {"", "abc", "a'b", "; rm -rf /", "$(x)"}) {
        const auto q = worktree_shell_quote(s);
        ASSERT_GE(q.size(), 2u);
        EXPECT_EQ(q.front(), '\'');
        EXPECT_EQ(q.back(), '\'');
    }
}

// ============================================================
// Tool deny rules — pure grammar/matcher tests
// TS REF: src/utils/permissions/permissions.ts:238-269
// ============================================================
namespace loom_deny_rules_test {

using loom::utils::tool_deny_rules::DenyToolView;
using loom::utils::tool_deny_rules::is_tool_denied;

// Test-only ergonomic wrapper: initializer_list -> span (braced lists do not
// implicitly convert to std::span).
bool denied(std::initializer_list<std::string> rules,
            const DenyToolView& view) {
    const std::vector<std::string> rule_vector(rules);
    return is_tool_denied(rule_vector, view);
}

TEST(ToolDenyRules, BasicToolAndContentRules) {
    // A whole-tool rule strips the tool; an unrelated rule does not.
    EXPECT_TRUE(denied({"Bash"}, {"Bash", std::nullopt, std::nullopt}));
    EXPECT_FALSE(denied({"Read"}, {"Bash", std::nullopt, std::nullopt}));

    // Content rules never strip a tool regardless of content.
    EXPECT_FALSE(denied({"Bash(npm install)"},
                               {"Bash", std::nullopt, std::nullopt}));
    // TS REF: permissionRuleParser.ts:126-128 — content "" or "*" is dropped
    // and the rule becomes a whole-tool rule, so "Bash(*)" DOES strip Bash.
    // (The spec's ts_behavior narrative confirms this; a literal "star is
    // content" reading of the tests entry would contradict the TS parser.)
    EXPECT_TRUE(denied({"Bash(*)"},
                              {"Bash", std::nullopt, std::nullopt}));
    // Bare "Bash" strips too.
    EXPECT_TRUE(denied({"Bash(*)", "Bash"},
                              {"Bash", std::nullopt, std::nullopt}));

    // Content rule with escaped parentheses parses as CONTENT (parens are
    // literal), so it never strips the whole tool — unlike "Bash()"/"Bash(*)"
    // which become whole-tool rules.
    EXPECT_FALSE(denied({R"(Bash(foo\(bar\)))"},
                                {"Bash", std::nullopt, std::nullopt}));
    // Trailing content after the closing paren is a malformed tool-name rule
    // in TS and must not match the bare tool.
    EXPECT_FALSE(denied({"Bash(x)tail"},
                                {"Bash", std::nullopt, std::nullopt}));

    // Legacy aliases resolve to canonical tool names.
    EXPECT_TRUE(denied({"Task"}, {"Agent", std::nullopt, std::nullopt}));
    EXPECT_TRUE(denied({"KillShell"},
                              {"TaskStop", std::nullopt, std::nullopt}));
    EXPECT_TRUE(denied({"AgentOutputTool"},
                              {"TaskOutput", std::nullopt, std::nullopt}));
    EXPECT_TRUE(denied({"BashOutputTool"},
                              {"TaskOutput", std::nullopt, std::nullopt}));

    // Unknown tools / empty lists match nothing and never throw.
    EXPECT_FALSE(denied({"NoSuchTool"},
                               {"Bash", std::nullopt, std::nullopt}));
    EXPECT_FALSE(denied({}, {"Bash", std::nullopt, std::nullopt}));
}

TEST(ToolDenyRules, McpServerAndExactToolRules) {
    const DenyToolView linear_view{
        "list_issues", std::string{"linear"}, std::string{"list_issues"}};

    // Server-scope rules strip every tool of the server.
    EXPECT_TRUE(denied({"mcp__linear"}, linear_view));
    EXPECT_TRUE(denied({"mcp__linear__*"}, linear_view));
    // Exact-tool rule strips the matching tool.
    EXPECT_TRUE(denied({"mcp__linear__list_issues"}, linear_view));
    // An exact-tool rule for another tool matches only itself.
    EXPECT_FALSE(denied({"mcp__linear__create_issue"}, linear_view));

    // A different server is unaffected.
    const DenyToolView github_view{
        "pr_list", std::string{"github"}, std::string{"pr_list"}};
    EXPECT_FALSE(denied({"mcp__linear"}, github_view));

    // Punctuated raw server names normalize to the rule spelling.
    const DenyToolView dotted_server{
        "do_thing", std::string{"my.server"}, std::string{"do_thing"}};
    EXPECT_TRUE(denied({"mcp__my_server"}, dotted_server));

    // Garbage inputs return bool and never crash/fatal-fail.
    const DenyToolView bash_view{"Bash", std::nullopt, std::nullopt};
    for (const std::string& garbage :
         {"mcp__", "mcp", "((", "Bash(", "mcp____x", ""}) {
        EXPECT_NO_FATAL_FAILURE((void)denied({garbage}, bash_view));
        EXPECT_NO_FATAL_FAILURE((void)denied({garbage}, linear_view));
    }
}

TEST(ToolDenyRules, NormalizationAndCheckName) {
    using loom::utils::tool_deny_rules::mcp_info_from_string;
    using loom::utils::tool_deny_rules::normalize_name_for_mcp;
    using loom::utils::tool_deny_rules::permission_check_name;

    EXPECT_EQ(normalize_name_for_mcp("my.server"), "my_server");
    EXPECT_EQ(normalize_name_for_mcp("a b"), "a_b");
    // Underscore runs are preserved: the vendor-prefix branch that used to
    // collapse them is gone, so this is now the only behaviour.
    EXPECT_EQ(normalize_name_for_mcp("a__b"), "a__b");
    EXPECT_EQ(normalize_name_for_mcp("a  b"), "a__b");

    DenyToolView v{"t", std::string{"My.Server"}, std::string{"Do Thing"}};
    EXPECT_EQ(permission_check_name(v), "mcp__My_Server__Do_Thing");

    auto info = mcp_info_from_string("mcp__srv__a__b");
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->server, "srv");
    ASSERT_TRUE(info->tool.has_value());
    EXPECT_EQ(*info->tool, "a__b");
    EXPECT_FALSE(mcp_info_from_string("mcp__").has_value());
    EXPECT_FALSE(mcp_info_from_string("mcp").has_value());
    EXPECT_FALSE(mcp_info_from_string("foo__bar").has_value());
}

// ============================================================
// QueryEngine integration: deny rules filter the request tools array
// TS REF: src/tools.ts:319,369; permissions.ts:287-292
// ============================================================
namespace deny_engine {

using loom::core::InputSchema;
using loom::core::QueryEngine;
using loom::core::QueryEngineConfig;
using loom::core::ToolDefinition;
using loom::core::ToolPermission;
using loom::core::ToolRegistry;
using loom::utils::json::parse;

struct Fixture {
    ToolRegistry registry;
    std::filesystem::path cwd;
    const std::vector<std::string> expected_all = {
        "Read", "Bash", "list_issues", "create_issue", "pr_list"};

    static ToolDefinition make_mcp_def(std::string name, std::string server) {
        ToolDefinition def;
        def.name = name;
        def.description = name + " test tool";
        def.input_schema = InputSchema{};
        def.permission = ToolPermission::Network;
        def.is_hidden = false;
        def.category = std::string{"mcp:"} + server;
        return def;
    }

    QueryEngineConfig make_config(std::vector<std::string> deny_rules) {
        QueryEngineConfig config;
        config.api_key = "test-key";
        config.base_url = "http://127.0.0.1:1";  // never contacted
        config.retry_policy.max_retries = 0;
        config.cwd = cwd.string();
        config.always_deny_rules = std::move(deny_rules);
        config.tools = {
            ToolDefinition{
                .name = "Read",
                .description = "Reads files",
                .input_schema = InputSchema{},
                .permission = ToolPermission::ReadOnly,
                .is_hidden = false,
                .category = std::nullopt,
            },
            ToolDefinition{
                .name = "Bash",
                .description = "Executes commands",
                .input_schema = InputSchema{},
                .permission = ToolPermission::Execute,
                .is_hidden = false,
                .category = std::nullopt,
            },
        };
        config.dynamic_tools_provider = [] {
            std::vector<ToolDefinition> defs;
            defs.push_back(make_mcp_def("list_issues", "linear"));
            defs.push_back(make_mcp_def("create_issue", "linear"));
            defs.push_back(make_mcp_def("pr_list", "github"));
            return defs;
        };
        return config;
    }

    std::vector<std::string> tool_names(
        const std::vector<std::string>& deny_rules) {
        QueryEngine engine(make_config(deny_rules), registry);
        const std::string body = engine.build_request_body_for_testing();
        auto doc = parse(body);
        EXPECT_TRUE(doc.has_value()) << body;
        std::vector<std::string> names;
        if (doc) {
            const auto tools = doc->root().get("tools");
            EXPECT_TRUE(tools.is_arr()) << body;
            if (tools.is_arr()) {
                tools.iter([&](auto element) {
                    names.emplace_back(element.get("name").as_str());
                });
            }
        }
        return names;
    }

    void setup(const std::string& suffix) {
        cwd = std::filesystem::temp_directory_path() /
              ("loom-deny-rules-" + std::to_string(::getpid()) + "-" +
               suffix);
        std::filesystem::remove_all(cwd);
        std::filesystem::create_directories(cwd);
    }

    void teardown() {
        std::filesystem::remove_all(cwd);
    }
};

TEST(ToolDenyRulesQueryEngine, EmptyRulesListAllFiveTools) {
    Fixture f;
    f.setup("empty");
    auto names = f.tool_names({});
    std::vector<std::string> sorted = names;
    std::ranges::sort(sorted);
    std::vector<std::string> expected = f.expected_all;
    std::ranges::sort(expected);
    EXPECT_EQ(sorted, expected);
    f.teardown();
}

TEST(ToolDenyRulesQueryEngine, DeniesBuiltinBash) {
    Fixture f;
    f.setup("bash");
    auto names = f.tool_names({"Bash"});
    EXPECT_EQ(std::ranges::count(names, std::string{"Bash"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"Read"}), 0);
    f.teardown();
}

TEST(ToolDenyRulesQueryEngine, DeniesMcpServer) {
    Fixture f;
    f.setup("server");
    auto names = f.tool_names({"mcp__linear"});
    EXPECT_EQ(std::ranges::count(names, std::string{"list_issues"}), 0);
    EXPECT_EQ(std::ranges::count(names, std::string{"create_issue"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"pr_list"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"Read"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"Bash"}), 0);
    f.teardown();
}

TEST(ToolDenyRulesQueryEngine, DeniesMcpExactTool) {
    Fixture f;
    f.setup("exact");
    auto names = f.tool_names({"mcp__linear__list_issues"});
    EXPECT_EQ(std::ranges::count(names, std::string{"list_issues"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"create_issue"}), 0);
    EXPECT_NE(std::ranges::count(names, std::string{"pr_list"}), 0);
    f.teardown();
}

TEST(ToolDenyRulesQueryEngine, UnknownAndContentRulesChangeNothing) {
    Fixture f;
    f.setup("none");
    const std::vector<std::string> rules = {
        "NoSuchTool", "mcp__nonexistent_server", "Bash(rm -rf)"};
    auto names = f.tool_names(rules);
    std::vector<std::string> sorted_names = names;
    std::ranges::sort(sorted_names);
    std::vector<std::string> expected = f.expected_all;
    std::ranges::sort(expected);
    EXPECT_EQ(sorted_names, expected);

    // Empty deny list yields the identical set (existing E2E gate path).
    auto names_empty = f.tool_names({});
    std::vector<std::string> sorted_empty = names_empty;
    std::ranges::sort(sorted_empty);
    EXPECT_EQ(sorted_empty, expected);
    f.teardown();
}

}  // namespace deny_engine
}  // namespace loom_deny_rules_test

namespace loom_native_computer_tool_test {

using namespace loom::core;

TEST(ToolDenyRulesQueryEngine, NativeComputerToolEmitsComputer20241022Schema) {
    QueryEngineConfig config;
    config.base_url = "http://127.0.0.1:1";  // never contacted
    config.tools.push_back(ToolDefinition{
        .name = "computer_use",
        .description = "ignored for native computer tool",
        .input_schema = InputSchema{},
        .permission = ToolPermission::Execute,
        .is_hidden = false,
        .category = "computer_use",
    });
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);

    const std::string body = engine.build_request_body_for_testing();
    auto parsed = loom::utils::json::parse(body);
    ASSERT_TRUE(parsed.has_value()) << body;
    const auto tools = parsed->root().get("tools");
    ASSERT_TRUE(tools.is_arr()) << body;
    ASSERT_EQ(tools.size(), 1u) << body;
    const auto t = tools.at(0);
    // Emitted under the wire name "computer".
    EXPECT_EQ(std::string(t.get("name").as_str()), "computer");
    EXPECT_EQ(std::string(t.get("type").as_str()), "computer_20241022");
    EXPECT_TRUE(t.get("display_width_px").is_num());
    EXPECT_TRUE(t.get("display_height_px").is_num());
    EXPECT_TRUE(t.get("display_number").is_num());
    // Native computer tool has NO input_schema (params are fixed by the API).
    EXPECT_FALSE(t.get("input_schema").valid()) << body;
    EXPECT_FALSE(t.get("description").valid()) << body;
}

TEST(ToolDenyRulesQueryEngine, RegularFunctionToolUnaffectedByComputerShape) {
    QueryEngineConfig config;
    config.base_url = "http://127.0.0.1:1";  // never contacted
    config.tools.push_back(ToolDefinition{
        .name = "Bash",
        .description = "Run a shell command",
        .input_schema = InputSchema{},
        .permission = ToolPermission::Execute,
        .is_hidden = false,
        .category = std::nullopt,
    });
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);
    const std::string body = engine.build_request_body_for_testing();
    auto parsed = loom::utils::json::parse(body);
    ASSERT_TRUE(parsed.has_value()) << body;
    const auto t = parsed->root().get("tools").at(0);
    EXPECT_EQ(std::string(t.get("name").as_str()), "Bash");
    EXPECT_EQ(std::string(t.get("type").as_str()), "function");
    EXPECT_TRUE(t.get("input_schema").valid());
    EXPECT_FALSE(t.get("display_width_px").valid());
}

}  // namespace loom_native_computer_tool_test

namespace loom_mcp_input_schema_test {

using namespace loom::core;

// A connected stdio MCP server exposing a tool with a nested inputSchema
// must have that schema emitted verbatim in the API request body instead of
// the simplified empty-object schema the collector stores.
TEST(McpToolSchemaQueryEngine, VerbatimNestedSchemaEmitted) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() /
        ("loom_mcp_schema_test_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });
function send(message) { process.stdout.write(JSON.stringify(message) + '\n'); }
rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({ jsonrpc: '2.0', id: request.id, result: {
      protocolVersion: '2024-11-05',
      capabilities: { tools: {} },
      serverInfo: { name: 'schema-fixture', version: '1.0.0' }
    }});
    return;
  }
  if (request.method === 'tools/list') {
    send({ jsonrpc: '2.0', id: request.id, result: { tools: [{
      name: 'nested_lookup',
      description: 'Nested schema fixture',
      inputSchema: {
        type: 'object',
        properties: {
          query: { type: 'string', description: 'search text' },
          opts: { type: 'object', properties: {
            limit: { type: 'integer', minimum: 1 },
            tags: { type: 'array', items: { type: 'string' } }
          }}
        },
        required: ['query'],
        $schema: 'http://json-schema.org/draft-07/schema#'
      }
    }]}});
    return;
  }
});
)JS";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "schema_fixture",
            .command = "node",
            .args = {server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());
    auto restarted = loom::tools::restart_native_mcp_server("schema_fixture");
    ASSERT_TRUE(restarted.has_value()) << restarted.error();
    ASSERT_EQ(restarted->tools.size(), 1u);
    EXPECT_EQ(restarted->tools.front().name, "nested_lookup");

    QueryEngineConfig config;
    config.api_key = "test-key";
    config.base_url = "http://127.0.0.1:1";  // never contacted
    config.retry_policy.max_retries = 0;
    config.cwd = root.string();
    config.dynamic_tools_provider = [] {
        return loom::tools::collect_mcp_tool_definitions();
    };
    config.mcp_input_schema_provider = [] {
        return loom::tools::collect_mcp_input_schemas();
    };
    ToolRegistry registry;
    QueryEngine engine(std::move(config), registry);

    const std::string body = engine.build_request_body_for_testing();
    auto parsed = loom::utils::json::parse(body);
    ASSERT_TRUE(parsed.has_value()) << body;
    const auto tools = parsed->root().get("tools");
    ASSERT_TRUE(tools.is_arr()) << body;

    bool found = false;
    tools.iter([&](auto element) {
        if (std::string(element.get("name").as_str()) != "nested_lookup") return;
        found = true;
        EXPECT_EQ(std::string(element.get("type").as_str()), "function");
        const auto schema = element.get("input_schema");
        ASSERT_TRUE(schema.is_obj()) << body;
        const auto props = schema.get("properties");
        ASSERT_TRUE(props.is_obj()) << body;
        // Nested object/array shapes must survive verbatim.
        const auto opts = props.get("opts");
        ASSERT_TRUE(opts.is_obj()) << body;
        const auto opts_props = opts.get("properties");
        ASSERT_TRUE(opts_props.is_obj()) << body;
        EXPECT_EQ(std::string(opts_props.get("limit").get("type").as_str()),
                  "integer");
        const auto tags = opts_props.get("tags");
        ASSERT_TRUE(tags.is_obj()) << body;
        EXPECT_EQ(std::string(tags.get("items").get("type").as_str()),
                  "string");
        // Required list and vendor keys preserved.
        EXPECT_EQ(std::string(schema.get("$schema").as_str()),
                  "http://json-schema.org/draft-07/schema#");
        const auto required = schema.get("required");
        ASSERT_TRUE(required.is_arr()) << body;
        bool has_query = false;
        required.iter([&](auto r) {
            if (std::string(r.as_str()) == "query") has_query = true;
        });
        EXPECT_TRUE(has_query) << body;
    });
    EXPECT_TRUE(found) << body;

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

// Raw-name MCP calls (missing-tool fallback used by computer-use screenshot
// responses) must preserve image content blocks, not flatten them to text.
TEST(McpToolSchemaQueryEngine, ResultConversionPreservesScreenshotImage) {
    using loom::services::mcp::ContentItem;
    loom::tools::McpToolResult mcp_result{
        .content = "screenshot taken",
        .content_items = {
            ContentItem{
                .type = "text",
                .text = "screenshot taken",
            },
            ContentItem{
                .type = "image",
                .text = {},
                .media_type = std::string{"image/png"},
                .data = std::string{"BASE64PNGDATA"},
            },
        },
        .content_type = "text",
    };

    auto converted = loom::tools::mcp_result_to_tool_result(mcp_result);
    ASSERT_EQ(converted.content.size(), 2u);
    EXPECT_EQ(converted.content[0].format.value_or(""), "text");
    EXPECT_EQ(converted.content[0].text, "screenshot taken");
    ASSERT_TRUE(converted.content[1].format.has_value());
    EXPECT_EQ(*converted.content[1].format, "image");
    EXPECT_EQ(converted.content[1].media_type.value_or(""), "image/png");
    EXPECT_EQ(converted.content[1].data.value_or(""), "BASE64PNGDATA");
    EXPECT_FALSE(converted.is_error);

    // With only a flattened payload, conversion yields a single text block.
    loom::tools::McpToolResult text_only{
        .content = "plain", .content_items = {}, .content_type = "text"};
    auto text_result = loom::tools::mcp_result_to_tool_result(text_only);
    ASSERT_EQ(text_result.content.size(), 1u);
    EXPECT_EQ(text_result.content[0].text, "plain");
}

// When a "computer-use" MCP server is connected, native computer actions are
// forwarded to it and its screenshot image block is preserved.
TEST(McpToolSchemaQueryEngine, ComputerActionRoutesToComputerUseMcpServer) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() /
        ("loom_computer_use_mcp_test_" + std::to_string(::getpid()));
    fs::remove_all(root);
    fs::create_directories(root);
    const auto server_path = root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const readline = require('node:readline');
const rl = readline.createInterface({ input: process.stdin });
function send(message) { process.stdout.write(JSON.stringify(message) + '\n'); }
rl.on('line', line => {
  const request = JSON.parse(line);
  if (request.method === 'initialize') {
    send({ jsonrpc: '2.0', id: request.id, result: {
      protocolVersion: '2024-11-05',
      capabilities: { tools: {} },
      serverInfo: { name: 'computer-use', version: '1.0.0' }
    }});
    return;
  }
  if (request.method === 'tools/list') {
    send({ jsonrpc: '2.0', id: request.id, result: { tools: [{
      name: 'computer',
      description: 'Native computer tool',
      inputSchema: { type: 'object' }
    }]}});
    return;
  }
  if (request.method === 'tools/call') {
    send({ jsonrpc: '2.0', id: request.id, result: {
      isError: false,
      content: [
        { type: 'text', text: 'clicked' },
        { type: 'image', data: 'UE5HT05OTkc=', mimeType: 'image/png' }
      ]
    }});
    return;
  }
});
)JS";
    }

    auto synced = loom::tools::sync_native_mcp_servers({
        loom::tools::NativeMcpConfiguredServer{
            .name = "computer-use",
            .command = "node",
            .args = {server_path.string()},
            .env = {},
        },
    });
    ASSERT_TRUE(synced.has_value());
    auto restarted = loom::tools::restart_native_mcp_server("computer-use");
    ASSERT_TRUE(restarted.has_value()) << restarted.error();
    EXPECT_EQ(restarted->status, "ready");

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(
        registry,
        loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    // The registry dispatches the native computer action under its internal
    // "computer_use" name; the request serializer emits it as "computer".
    auto result = registry.execute(
        "computer_use",
        loom::core::ToolInput::from_json(
            R"({"action":"left_click","coordinate":[100,200]})"));
    ASSERT_TRUE(result.has_value());
    ASSERT_FALSE(result->is_error);
    bool saw_image = false;
    for (const auto& c : result->content) {
        if (c.format == "image") {
            saw_image = true;
            EXPECT_EQ(c.media_type.value_or(""), "image/png");
            EXPECT_EQ(c.data.value_or(""), "UE5HT05OTkc=");
        }
    }
    EXPECT_TRUE(saw_image);

    ASSERT_TRUE(loom::tools::sync_native_mcp_servers({}).has_value());
    fs::remove_all(root);
}

}  // namespace loom_mcp_input_schema_test
