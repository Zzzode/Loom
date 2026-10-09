/// @file test_swarm.cpp
/// @brief Swarm backends, permission sync, and team file tests.

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

} // namespace

namespace loom_tmux_detection_test {
TEST(SwarmBackends, CaptureEnvReflectsTmuxPresence) {
    using loom::utils::swarm_backends::EnvironmentDetection;
    // Before capture simulating an empty env → not inside tmux.
    EnvironmentDetection::capture_env("", "");
    EXPECT_FALSE(EnvironmentDetection::is_inside_tmux_sync());
    EXPECT_FALSE(EnvironmentDetection::is_inside_tmux());

    // A leader running inside tmux has TMUX set (and a pane target).
    EnvironmentDetection::capture_env(
        "/tmp/tmux-1000/default,1234,0", "%12");
    EXPECT_TRUE(EnvironmentDetection::is_inside_tmux_sync());
    // Re-capture resets the cache, so the async/cached getter agrees.
    EXPECT_TRUE(EnvironmentDetection::is_inside_tmux());
    EXPECT_EQ(std::string(EnvironmentDetection::get_leader_pane_id()), "%12");

    // Restore to empty so other tests are unaffected.
    EnvironmentDetection::capture_env("", "");
    EXPECT_FALSE(EnvironmentDetection::is_inside_tmux());
}

}  // namespace loom_tmux_detection_test

namespace loom_pane_cleanup_test {

using loom::utils::swarm_backends::PaneBackend;
using loom::utils::swarm_backends::PaneId;
using loom::utils::swarm_backends::CreatePaneResult;
using loom::utils::swarm_backends::AgentColor;
using loom::utils::swarm_backends::BackendType;
using loom::utils::swarm_backends::PaneBackendExecutor;
using loom::utils::swarm_backends::TeammateSpawnConfig;

// In-memory pane backend that records kill calls (no tmux required).
class FakePaneBackend : public PaneBackend {
public:
    int create_calls = 0;
    std::vector<std::pair<PaneId, bool>> killed;
    bool available = true;
    bool inside = false;

    BackendType type() const override { return BackendType::Tmux; }
    std::string_view display_name() const override { return "fake-tmux"; }
    bool supports_hide_show() const override { return false; }
    bool is_available() const override { return available; }
    bool is_running_inside() const override { return inside; }

    CreatePaneResult create_teammate_pane(std::string_view, AgentColor) override {
        ++create_calls;
        return CreatePaneResult{.pane_id = PaneId{"%p" + std::to_string(create_calls)},
                               .is_first_teammate = (create_calls == 1)};
    }
    void send_command_to_pane(const PaneId&, std::string_view, bool) override {}
    void set_pane_border_color(const PaneId&, AgentColor, bool) override {}
    void set_pane_title(const PaneId&, std::string_view, AgentColor, bool) override {}
    void enable_pane_border_status(std::optional<std::string_view>, bool) override {}
    void rebalance_panes(std::string_view, bool) override {}
    bool kill_pane(const PaneId& id, bool ext) override {
        killed.emplace_back(id, ext);
        return true;
    }
    bool hide_pane(const PaneId&, bool) override { return false; }
    bool show_pane(const PaneId&, std::string_view, bool) override { return false; }
};

TEST(SwarmBackends, ExecutorDestructionKillsAllSpawnedPanes) {
    auto fake = std::make_shared<FakePaneBackend>();
    {
        PaneBackendExecutor exec(fake);
        TeammateSpawnConfig a{.name = "alpha", .team_name = "t1", .prompt = "task a"};
        TeammateSpawnConfig b{.name = "beta",  .team_name = "t1", .prompt = "task b"};
        EXPECT_TRUE(exec.spawn(a).success);
        EXPECT_TRUE(exec.spawn(b).success);
        EXPECT_EQ(fake->create_calls, 2);
        EXPECT_TRUE(fake->killed.empty());
    }
    // Destroying the executor kills every pane it spawned, once each.
    EXPECT_EQ(fake->killed.size(), 2u);
}

TEST(SwarmBackends, SpawnDeliversInitialPromptToMailbox) {
    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_pane_prompt_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    EnvironmentGuard runner("LOOM_TEAM_RUNTIME_DIR", runtime_dir.string());

    auto fake = std::make_shared<FakePaneBackend>();
    {
        PaneBackendExecutor exec(fake);
        TeammateSpawnConfig cfg{
            .name = "worker", .team_name = "t1", .prompt = "build the thing"};
        ASSERT_TRUE(exec.spawn(cfg).success);
        // Initial task is delivered to the pane teammate's file inbox.
        auto inbox = loom::utils::read_inbox("worker", std::optional<std::string_view>{"t1"});
        ASSERT_TRUE(inbox.has_value());
        ASSERT_EQ(inbox->size(), 1u);
        EXPECT_EQ((*inbox)[0].from, "team-lead");
        EXPECT_NE((*inbox)[0].text.find("build the thing"), std::string::npos);
    }
    fs::remove_all(runtime_dir);
}

}  // namespace loom_pane_cleanup_test

// ============================================================================
// PermissionSync mailbox protocol (stage A: protocol only, no TUI wiring)
// ============================================================================

namespace {

namespace sh = loom::utils::swarm_helpers;

struct PermissionRuntimeGuard {
    fs::path dir;
    EnvironmentGuard team_dir;

    PermissionRuntimeGuard()
        : dir(fs::temp_directory_path() /
              ("loom_perm_sync_" +
               std::to_string(std::chrono::steady_clock::now()
                                  .time_since_epoch().count()))),
          team_dir("LOOM_TEAM_RUNTIME_DIR", dir.string()) {
        fs::remove_all(dir);
    }

    ~PermissionRuntimeGuard() { fs::remove_all(dir); }
};

sh::SwarmPermissionRequestMessage make_permission_request() {
    return sh::SwarmPermissionRequestMessage{
        .type = "permission_request",
        .request_id = sh::PermissionSync::generate_request_id(),
        .agent_id = "worker-a",
        .tool_name = "Bash",
        .tool_use_id = "toolu_123",
        .description = R"({"command":"ls"})",
        .input_json = R"({"command":"ls"})",
    };
}

}  // namespace

TEST(SwarmPermissionSync, RequestLandsInLeaderMailbox) {
    PermissionRuntimeGuard guard;
    auto request = make_permission_request();

    ASSERT_TRUE(sh::PermissionSync::send_request_to_leader(request, "alpha"));

    auto inbox = loom::utils::read_inbox(
        "team-lead", std::optional<std::string_view>{"alpha"});
    ASSERT_TRUE(inbox.has_value()) << inbox.error();
    ASSERT_EQ(inbox->size(), 1u);
    EXPECT_EQ((*inbox)[0].from, "worker-a");

    // Verbatim input object is embedded in the frozen JSON shape.
    const std::string& text = (*inbox)[0].text;
    EXPECT_NE(text.find("\"type\":\"permission_request\""), std::string::npos);
    EXPECT_NE(text.find(R"("input":{"command":"ls"})"), std::string::npos);
    EXPECT_NE(text.find("\"permission_suggestions\":[]"), std::string::npos);

    auto parsed = sh::PermissionSync::parse_request(text);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->type, "permission_request");
    EXPECT_EQ(parsed->request_id, request.request_id);
    EXPECT_EQ(parsed->agent_id, "worker-a");
    EXPECT_EQ(parsed->tool_name, "Bash");
    EXPECT_EQ(parsed->tool_use_id, "toolu_123");
    EXPECT_EQ(parsed->description, R"({"command":"ls"})");
    EXPECT_EQ(parsed->input_json, R"({"command":"ls"})");

    // Request id format: perm-<unixms>-<7 base36 chars>.
    EXPECT_EQ(request.request_id.rfind("perm-", 0), 0u);
    const auto last_dash = request.request_id.rfind('-');
    ASSERT_NE(last_dash, std::string::npos);
    EXPECT_EQ(request.request_id.size() - last_dash - 1, 7u);

    // Non-object input falls back to {}.
    auto malformed = request;
    malformed.input_json = "not-json";
    const auto built = sh::PermissionSync::build_request_text(malformed);
    EXPECT_NE(built.find("\"input\":{}"), std::string::npos);

    // Unrelated text is not a request.
    EXPECT_FALSE(sh::PermissionSync::parse_request("hello team").has_value());
    EXPECT_FALSE(
        sh::PermissionSync::parse_request(R"({"type":"task"})").has_value());
}

TEST(SwarmPermissionSync, ApprovedRoundTrip) {
    using namespace std::chrono_literals;
    PermissionRuntimeGuard guard;
    auto request = make_permission_request();

    std::optional<sh::SwarmPermissionResponseMessage> received;
    std::thread waiter([&] {
        received = sh::PermissionSync::request_and_await(
            request, "alpha", 5s, 50ms);
    });

    // Leader side: read the request and approve it.
    std::this_thread::sleep_for(100ms);
    auto leader_inbox = loom::utils::read_inbox(
        "team-lead", std::optional<std::string_view>{"alpha"});
    ASSERT_TRUE(leader_inbox.has_value());
    ASSERT_EQ(leader_inbox->size(), 1u);
    auto incoming = sh::PermissionSync::parse_request((*leader_inbox)[0].text);
    ASSERT_TRUE(incoming.has_value());

    sh::SwarmPermissionResponseMessage response;
    response.type = "permission_response";
    response.request_id = incoming->request_id;
    response.subtype = "success";
    ASSERT_TRUE(sh::PermissionSync::send_response_to_worker(
        "worker-a", response, "alpha"));

    waiter.join();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->type, "permission_response");
    EXPECT_EQ(received->subtype, "success");
    EXPECT_FALSE(received->error.has_value());

    // Remove-on-consume: the response envelope is gone from the worker inbox.
    auto worker_inbox = loom::utils::read_inbox(
        "worker-a", std::optional<std::string_view>{"alpha"});
    ASSERT_TRUE(worker_inbox.has_value());
    EXPECT_TRUE(worker_inbox->empty());
}

TEST(SwarmPermissionSync, DeniedRoundTrip) {
    using namespace std::chrono_literals;
    PermissionRuntimeGuard guard;
    auto request = make_permission_request();

    std::optional<sh::SwarmPermissionResponseMessage> received;
    std::thread waiter([&] {
        received = sh::PermissionSync::request_and_await(
            request, "alpha", 5s, 50ms);
    });

    std::this_thread::sleep_for(100ms);
    sh::SwarmPermissionResponseMessage response;
    response.type = "permission_response";
    response.request_id = request.request_id;
    response.subtype = "error";
    response.error = "Permission denied by team lead";
    ASSERT_TRUE(sh::PermissionSync::send_response_to_worker(
        "worker-a", response, "alpha"));

    waiter.join();
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ(received->subtype, "error");
    ASSERT_TRUE(received->error.has_value());
    EXPECT_EQ(*received->error, "Permission denied by team lead");

    // The frozen error JSON shape round-trips through the parser.
    const auto text = sh::PermissionSync::build_response_text(response);
    EXPECT_NE(text.find("\"type\":\"permission_response\""), std::string::npos);
    EXPECT_NE(text.find("\"subtype\":\"error\""), std::string::npos);
    auto reparsed = sh::PermissionSync::parse_response(text);
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(reparsed->subtype, "error");
    EXPECT_EQ(reparsed->error, "Permission denied by team lead");
}

TEST(SwarmPermissionSync, TimeoutReturnsNullopt) {
    using namespace std::chrono_literals;
    PermissionRuntimeGuard guard;
    auto request = make_permission_request();

    const auto start = std::chrono::steady_clock::now();
    auto received = sh::PermissionSync::request_and_await(
        request, "alpha", 150ms, 30ms);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_FALSE(received.has_value());  // fail closed
    EXPECT_GE(elapsed, 140ms);
    EXPECT_LT(elapsed, 1000ms);
}

TEST(SwarmPermissionSync, IdempotentConsume) {
    PermissionRuntimeGuard guard;
    auto request = make_permission_request();

    // Pre-seed the worker inbox with two identical responses (the leader
    // retried, or a slow poll raced a resend).
    sh::SwarmPermissionResponseMessage response;
    response.type = "permission_response";
    response.request_id = request.request_id;
    response.subtype = "success";
    ASSERT_TRUE(sh::PermissionSync::send_response_to_worker(
        "worker-a", response, "alpha"));
    ASSERT_TRUE(sh::PermissionSync::send_response_to_worker(
        "worker-a", response, "alpha"));

    auto first = sh::PermissionSync::poll_response(
        "worker-a", "alpha", request.request_id);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->subtype, "success");

    auto second = sh::PermissionSync::poll_response(
        "worker-a", "alpha", request.request_id);
    EXPECT_FALSE(second.has_value());

    // Responses for a different request id remain untouched in the inbox...
    sh::SwarmPermissionResponseMessage other;
    other.type = "permission_response";
    other.request_id = request.request_id + "-other";
    other.subtype = "error";
    other.error = "nope";
    ASSERT_TRUE(sh::PermissionSync::send_response_to_worker(
        "worker-a", other, "alpha"));
    auto other_poll = sh::PermissionSync::poll_response(
        "worker-a", "alpha", other.request_id);
    ASSERT_TRUE(other_poll.has_value());
    EXPECT_EQ(other_poll->subtype, "error");
}

// ── Stage D-reconn: canonical team config.json + external tmux re-attach ─────

namespace loom_team_file_test {

using namespace loom::utils;

// Points LOOM_TEAM_RUNTIME_DIR at a unique temp directory and removes it on
// teardown, so config.json tests never touch the real .loom/teams tree.
struct TeamRuntimeDirGuard {
    fs::path dir;
    EnvironmentGuard env;

    TeamRuntimeDirGuard()
        : dir(fs::temp_directory_path() /
              ("cc-teamfile-" + std::to_string(static_cast<long long>(getpid())))),
          env("LOOM_TEAM_RUNTIME_DIR", dir.string()) {
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir);
    }

    ~TeamRuntimeDirGuard() {
        std::error_code ec;
        fs::remove_all(dir, ec);
        loom::utils::clear_dynamic_team_context();
    }

    TeamRuntimeDirGuard(const TeamRuntimeDirGuard&) = delete;
    TeamRuntimeDirGuard& operator=(const TeamRuntimeDirGuard&) = delete;
};

TeamFileRecord sample_team_file() {
    TeamFileRecord file;
    file.name = "migration-team";
    file.description = "stage d round trip";
    file.created_at = 42;
    file.lead_agent_id = "team-lead@migration-team";
    file.members = {
        TeamMemberRecord{
            .agent_id = "researcher@migration-team",
            .name = "researcher",
            .color = std::optional<std::string>{"blue"},
            .backend = std::optional<std::string>{"tmux"},
            .tmux_pane_id = "%7",
            .mode = std::optional<std::string>{"default"},
            .joined_at = 1,
            .cwd = "/x",
            .plan_mode_required = false,
            .is_active = true,
            .subscriptions = {},
        },
        TeamMemberRecord{
            .agent_id = "reviewer@migration-team",
            .name = "reviewer",
            .color = std::optional<std::string>{"green"},
            .backend = std::optional<std::string>{"in-process"},
            .tmux_pane_id = "",
            .joined_at = 2,
            .cwd = "/y",
        },
    };
    return file;
}

TEST(TeamFile, WriteReadRoundTrip) {
    TeamRuntimeDirGuard guard;
    auto file = sample_team_file();

    ASSERT_TRUE(write_team_file("migration-team", file));
    const auto reread = read_team_file("migration-team");
    ASSERT_TRUE(reread.has_value());
    EXPECT_EQ(reread->name, "migration-team");
    EXPECT_EQ(reread->lead_agent_id, "team-lead@migration-team");
    EXPECT_EQ(reread->created_at, 42);
    ASSERT_EQ(reread->description.value_or(""), "stage d round trip");
    ASSERT_EQ(reread->members.size(), 2u);
    EXPECT_TRUE(reread->hidden_pane_ids.empty());

    const auto& researcher = reread->members[0];
    EXPECT_EQ(researcher.agent_id, "researcher@migration-team");
    EXPECT_EQ(researcher.tmux_pane_id, "%7");
    EXPECT_EQ(researcher.backend.value_or(""), "tmux");
    EXPECT_EQ(researcher.color.value_or(""), "blue");
    EXPECT_EQ(researcher.mode.value_or(""), "default");
    EXPECT_EQ(researcher.cwd, "/x");
    EXPECT_TRUE(researcher.is_active);

    const auto& reviewer = reread->members[1];
    EXPECT_EQ(reviewer.backend.value_or(""), "in-process");
    EXPECT_TRUE(reviewer.tmux_pane_id.empty());
    EXPECT_EQ(reviewer.color.value_or(""), "green");
}

TEST(TeamFile, RoleResolution) {
    TeamRuntimeDirGuard guard;
    ASSERT_TRUE(write_team_file("migration-team", sample_team_file()));

    const auto path = team_file_path("migration-team");
    EXPECT_NE(path.rfind("/migration-team/config.json"), std::string::npos);

    // A null/empty agent id is the leader (reconnection.ts:51 isLeader = !agentId).
    const auto leader =
        compute_initial_team_context("migration-team", "team-lead", std::nullopt);
    ASSERT_TRUE(leader.has_value());
    EXPECT_TRUE(leader->is_leader);
    EXPECT_FALSE(leader->self_agent_id.has_value());
    EXPECT_EQ(leader->self_agent_name, "team-lead");
    EXPECT_EQ(leader->lead_agent_id, "team-lead@migration-team");
    EXPECT_EQ(leader->team_file_path, path);

    const auto worker = compute_initial_team_context(
        "migration-team", "researcher",
        std::optional<std::string_view>{"researcher@migration-team"});
    ASSERT_TRUE(worker.has_value());
    EXPECT_FALSE(worker->is_leader);
    EXPECT_EQ(worker->self_agent_id.value_or(""), "researcher@migration-team");
    EXPECT_EQ(worker->self_agent_name, "researcher");
    EXPECT_EQ(worker->lead_agent_id, "team-lead@migration-team");

    // Missing team file and missing identity both resolve to nullopt.
    EXPECT_FALSE(compute_initial_team_context(
        "ghost-team", "team-lead", std::nullopt).has_value());
    EXPECT_FALSE(compute_initial_team_context(
        "migration-team", "", std::nullopt).has_value());
    EXPECT_FALSE(compute_initial_team_context(
        "", "team-lead", std::nullopt).has_value());

    const auto file = read_team_file("migration-team");
    ASSERT_TRUE(file.has_value());
    const auto researcher = find_team_member(*file, "researcher");
    ASSERT_TRUE(researcher.has_value());
    EXPECT_EQ(researcher->tmux_pane_id, "%7");
    EXPECT_FALSE(find_team_member(*file, "ghost").has_value());
}

} // namespace loom_team_file_test

namespace loom_external_reattach_test {

namespace sb = loom::utils::swarm_backends;

TEST(SwarmBackends, ExternalReattachArgvAndPolicy) {
    namespace detail = sb::detail;
    using detail::plan_external_swarm_view;
    using detail::tmux_has_session_argv;
    using detail::tmux_list_windows_argv;
    using detail::tmux_new_session_argv;
    using detail::tmux_new_window_argv;
    using detail::tmux_list_panes_argv;
    using detail::tmux_split_window_argv;
    using sb::detail::ExternalSessionAction;

    const auto has = tmux_has_session_argv("loom-swarm");
    EXPECT_EQ(has.program, "tmux");
    EXPECT_EQ(has.args,
              (std::vector<std::string>{"has-session", "-t", "loom-swarm"}));

    const auto windows = tmux_list_windows_argv("loom-swarm");
    EXPECT_EQ(windows.args,
              (std::vector<std::string>{"list-windows", "-t", "loom-swarm",
                                        "-F", "#{window_name}"}));

    const auto fresh = tmux_new_session_argv("loom-swarm", "swarm-view");
    EXPECT_EQ(fresh.args,
              (std::vector<std::string>{"new-session", "-d", "-s",
                                        "loom-swarm", "-n", "swarm-view",
                                        "-P", "-F", "#{pane_id}"}));

    const auto window = tmux_new_window_argv("loom-swarm", "swarm-view");
    EXPECT_NE(std::ranges::find(window.args, "new-window"), window.args.end());

    const auto panes = tmux_list_panes_argv("loom-swarm:swarm-view");
    EXPECT_EQ(panes.args,
              (std::vector<std::string>{"list-panes", "-t",
                                        "loom-swarm:swarm-view", "-F",
                                        "#{pane_id}"}));

    const auto vertical = tmux_split_window_argv("%9", true);
    EXPECT_NE(std::ranges::find(vertical.args, "-v"), vertical.args.end());
    const auto horizontal = tmux_split_window_argv("%9", false);
    EXPECT_NE(std::ranges::find(horizontal.args, "-h"), horizontal.args.end());

    // join_shell single-quotes every argument for the real shell runner.
    const auto line = vertical.join_shell();
    EXPECT_NE(line.find("tmux 'split-window'"), std::string::npos);
    EXPECT_NE(line.find("'-v'"), std::string::npos);
    EXPECT_NE(line.find("'%9'"), std::string::npos);

    EXPECT_EQ(plan_external_swarm_view(false, false, 0, false).action,
              ExternalSessionAction::CreateSession);
    EXPECT_EQ(plan_external_swarm_view(true, false, 1, false).action,
              ExternalSessionAction::CreateWindow);
    EXPECT_EQ(plan_external_swarm_view(true, true, 1, false).action,
              ExternalSessionAction::ReuseExistingWindow);

    // Fresh window with one unused pane: take pane 0.
    EXPECT_TRUE(plan_external_swarm_view(true, true, 1, false).reuse_first_pane);
    // Leader restart with live teammate panes: must split, not hijack pane 0.
    EXPECT_FALSE(plan_external_swarm_view(true, true, 3, false).reuse_first_pane);
    // Pane 0 already handed out in this process: split.
    EXPECT_FALSE(plan_external_swarm_view(true, true, 1, true).reuse_first_pane);
}

// RAII restore of the injected shell seams so later tests see real shells.
struct ShellRunnerGuard {
    ShellRunnerGuard() = default;
    ~ShellRunnerGuard() { sb::detail::reset_shell_runners_for_test(); }
    ShellRunnerGuard(const ShellRunnerGuard&) = delete;
    ShellRunnerGuard& operator=(const ShellRunnerGuard&) = delete;
};

TEST(SwarmBackends, ExternalReattachUsesListWindowsSeam) {
    using loom::utils::swarm_backends::AgentColor;
    using loom::utils::swarm_backends::EnvironmentDetection;
    using loom::utils::swarm_backends::TmuxBackend;
    namespace detail = loom::utils::swarm_backends::detail;

    // External path requires not running inside tmux.
    EnvironmentDetection::capture_env("", "");
    struct TmuxEnvGuard {
        ~TmuxEnvGuard() { EnvironmentDetection::capture_env("", ""); }
    } env_guard;

    ShellRunnerGuard shell_guard;

    // Scripted tmux: the swarm session and swarm-view window already exist and
    // hold three live panes (leader restart scenario).
    detail::set_shell_runners_for_test(
        [](std::string_view command) -> int {
            if (command.find("has-session") != std::string_view::npos) return 0;
            return 0;
        },
        [](std::string_view command) -> std::string {
            if (command.find("list-windows") != std::string_view::npos) {
                return "swarm-view";
            }
            if (command.find("list-panes") != std::string_view::npos) {
                return "%1\n%2\n%3";
            }
            if (command.find("split-window") != std::string_view::npos) {
                return "%42";
            }
            return {};
        });

    TmuxBackend backend;
    const auto result = backend.create_teammate_pane("researcher", AgentColor::Blue);
    EXPECT_EQ(result.pane_id, "%42");
    EXPECT_FALSE(result.is_first_teammate);
}

} // namespace loom_external_reattach_test

// A tool/description containing raw JSON control bytes must be escaped so it
// cannot corrupt the inbox JSON and permanently block all later messages.
TEST(SwarmPermissionSync, MailboxSurvivesRawControlBytes) {
    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_ctrlbytes_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    EnvironmentGuard runner("LOOM_TEAM_RUNTIME_DIR", runtime_dir.string());

    // Raw 0x01 / backspace / form-feed embedded in message text.
    std::string nasty;
    nasty.push_back(static_cast<char>(0x01));
    nasty += "a\bb\fc\"d\\e";
    ASSERT_TRUE(loom::utils::write_to_mailbox(
        "worker", loom::utils::TeammateMessage{.from = "team-lead", .text = nasty},
        std::optional<std::string_view>{"ctlteam"}).has_value());

    // The inbox must still parse (it would throw/return error if invalid JSON
    // were written), and a normal follow-up message must be deliverable.
    auto first = loom::utils::read_inbox("worker", std::optional<std::string_view>{"ctlteam"});
    ASSERT_TRUE(first.has_value());
    ASSERT_EQ(first->size(), 1u);

    ASSERT_TRUE(loom::utils::write_to_mailbox(
        "worker", loom::utils::TeammateMessage{.from = "team-lead", .text = "second"},
        std::optional<std::string_view>{"ctlteam"}).has_value());
    auto both = loom::utils::read_inbox("worker", std::optional<std::string_view>{"ctlteam"});
    ASSERT_TRUE(both.has_value());
    EXPECT_EQ(both->size(), 2u);
    EXPECT_EQ((*both)[1].text, "second");

    fs::remove_all(runtime_dir);
}

// Cross-process flock must serialize concurrent inbox read-modify-write from
// separate processes; without it two forked writers lose messages.
TEST(SwarmPermissionSync, CrossProcessFlockSerializesInboxWrites) {
    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_flock_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    EnvironmentGuard runner("LOOM_TEAM_RUNTIME_DIR", runtime_dir.string());
    constexpr std::string_view kTeam = "flockteam";
    constexpr int kPerChild = 25;

    auto child_loop = [kTeam](int tag) {
        for (int i = 0; i < kPerChild; ++i) {
            for (int attempt = 0; attempt < 10; ++attempt) {
                auto written = loom::utils::write_to_mailbox(
                    "worker",
                    loom::utils::TeammateMessage{
                        .from = tag == 0 ? "child-a" : "child-b",
                        .text = std::format("msg-{}-{}", tag, i),
                    },
                    std::optional<std::string_view>{kTeam});
                if (written.has_value()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        _exit(0);
    };

    const pid_t pid_a = fork();
    ASSERT_GE(pid_a, 0);
    if (pid_a == 0) child_loop(0);
    const pid_t pid_b = fork();
    ASSERT_GE(pid_b, 0);
    if (pid_b == 0) child_loop(1);

    int status_a = 0, status_b = 0;
    ASSERT_EQ(waitpid(pid_a, &status_a, 0), pid_a);
    ASSERT_EQ(waitpid(pid_b, &status_b, 0), pid_b);
    ASSERT_TRUE(WIFEXITED(status_a) && WEXITSTATUS(status_a) == 0);
    ASSERT_TRUE(WIFEXITED(status_b) && WEXITSTATUS(status_b) == 0);

    auto messages =
        loom::utils::read_inbox("worker", std::optional<std::string_view>{kTeam});
    ASSERT_TRUE(messages.has_value());
    // No lost updates: both children performed kPerChild successful appends.
    EXPECT_EQ(messages->size(), static_cast<std::size_t>(2 * kPerChild));

    fs::remove_all(runtime_dir);
}

// Leader AlwaysAllow must round-trip a permission_updates addRules grant,
// and the worker store must auto-allow the tool afterwards (persisted to
// disk so a fresh store instance simulating a pane restart also sees it).
TEST(SwarmPermissionSync, AlwaysAllowUpdatesPersistAndGrant) {
    namespace sh = loom::utils::swarm_helpers;
    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_allow_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    EnvironmentGuard runner("LOOM_TEAM_RUNTIME_DIR", runtime_dir.string());
    const std::string team = "allowteam";
    const std::string agent = "worker";

    sh::SwarmPermissionResponseMessage response;
    response.type = "permission_response";
    response.request_id = "perm-1";
    response.subtype = "success";
    response.permission_updates_json =
        sh::build_always_allow_updates_json("Bash");
    const std::string text = sh::PermissionSync::build_response_text(response);

    auto parsed = sh::PermissionSync::parse_response(text);
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->permission_updates_json.has_value());
    EXPECT_NE(parsed->permission_updates_json->find("addRules"),
              std::string::npos);

    // Fresh store instance: grants persist on disk (pane restart semantics).
    {
        sh::WorkerPermissionGrants grants(team, agent);
        EXPECT_FALSE(grants.allows("Bash"));
        grants.apply_updates(*parsed->permission_updates_json);
        EXPECT_TRUE(grants.allows("Bash"));
        EXPECT_FALSE(grants.allows("Edit"));
    }
    {
        sh::WorkerPermissionGrants restarted(team, agent);
        EXPECT_TRUE(restarted.allows("Bash"));
    }

    // Non-allow / malformed updates are ignored.
    sh::WorkerPermissionGrants strict(team, agent);
    strict.apply_updates(R"([{"type":"setMode","destination":"session","mode":"bypassPermissions"}])");
    strict.apply_updates("not-json");
    EXPECT_FALSE(strict.allows("Edit"));

    fs::remove_all(runtime_dir);
}

// Approval dialog input formatting: Edit payloads render as a -/+ diff;
// regular payloads pretty-print; oversized input truncates.
TEST(SwarmPermissionSync, PermissionInputFormatting) {
    namespace sh = loom::utils::swarm_helpers;

    const std::string edit = sh::format_permission_request_input(
        "Edit",
        R"({"file_path":"/tmp/a.txt","old_string":"one\ntwo","new_string":"one\n2"})");
    EXPECT_NE(edit.find("/tmp/a.txt"), std::string::npos);
    EXPECT_NE(edit.find("- two"), std::string::npos);
    EXPECT_NE(edit.find("+ 2"), std::string::npos);

    const std::string bash = sh::format_permission_request_input(
        "Bash", R"({"command":"ls -la","timeout":30})");
    EXPECT_NE(bash.find("\"command\""), std::string::npos);
    EXPECT_NE(bash.find("ls -la"), std::string::npos);

    const std::string truncated = sh::format_permission_request_input(
        "Bash", std::string("{\"command\":\"") +
                    std::string(5000, 'x') + "\"}",
        /*max_chars=*/100);
    EXPECT_LE(truncated.size(), 120u);
    EXPECT_NE(truncated.find("[truncated]"), std::string::npos);
}
