/// @file test_bridge_daemon.cpp
/// @brief Bridge daemon lifecycle, fork/exec, RPC, and backoff tests
///        (split from test_bridge.cpp).

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>

#include "bridge_test_helpers.hpp"

import std;
import loom.bridge.api;
import loom.bridge.config;
import loom.bridge.messages;
import loom.bridge.session_id_compat;
import loom.bridge.transport;
import loom.bridge.work_secret;
import loom.daemon.daemon_client;
import loom.daemon.daemon_server;

using namespace loom::test::bridge;

TEST(BridgeDaemon, PollsWorkAcknowledgesAndSpawnsSession) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon.pid"),
        .port_file = unique_temp_file("_daemon.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
    });

    std::vector<std::string> spawned_task_ids;
    daemon.set_session_spawner([&](std::string_view task_id) -> std::expected<std::string, std::string> {
        spawned_task_ids.push_back(std::string(task_id));
        return "daemon-session-1";
    });

    auto spawned = daemon.poll_for_work_once();
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(spawned->has_value());
    EXPECT_EQ(**spawned, "daemon-session-1");

    ASSERT_EQ(spawned_task_ids.size(), 1u);
    EXPECT_EQ(spawned_task_ids.front(), "work_1");

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().id, "daemon-session-1");
    EXPECT_EQ(sessions.front().pid, -1);
    EXPECT_EQ(sessions.front().status, "running");
    EXPECT_EQ(sessions.front().task_id, "work_1");
    ASSERT_TRUE(sessions.front().remote_session_id.has_value());
    EXPECT_EQ(*sessions.front().remote_session_id, "session_1");
    ASSERT_TRUE(sessions.front().session_ingress_token.has_value());
    EXPECT_EQ(*sessions.front().session_ingress_token, "session-token-from-secret");
    ASSERT_TRUE(sessions.front().session_api_base_url.has_value());
    EXPECT_EQ(*sessions.front().session_api_base_url, "http://session-ingress.local");
    ASSERT_TRUE(sessions.front().use_code_sessions.has_value());
    EXPECT_TRUE(*sessions.front().use_code_sessions);
    ASSERT_TRUE(sessions.front().code_session_mode.has_value());
    EXPECT_EQ(*sessions.front().code_session_mode, "code-session");
    ASSERT_TRUE(sessions.front().worker_epoch.has_value());
    EXPECT_EQ(*sessions.front().worker_epoch, 42);
    ASSERT_EQ(sessions.front().work_secret_sources_json.size(), 1u);
    EXPECT_NE(sessions.front().work_secret_sources_json.front().find(R"("id":"source-1")"), std::string::npos);
    ASSERT_TRUE(sessions.front().work_secret_auth_json.has_value());
    EXPECT_NE(sessions.front().work_secret_auth_json->find(R"("name":"console")"), std::string::npos);
    ASSERT_TRUE(sessions.front().work_secret_mcp_config_json.has_value());
    EXPECT_NE(sessions.front().work_secret_mcp_config_json->find(R"("linear")"), std::string::npos);
    ASSERT_TRUE(sessions.front().work_secret_environment_json.has_value());
    EXPECT_NE(sessions.front().work_secret_environment_json->find(R"("REMOTE_FLAG":"enabled")"), std::string::npos);
    ASSERT_TRUE(sessions.front().work_secret_environment_variables.contains("REMOTE_FLAG"));
    EXPECT_EQ(sessions.front().work_secret_environment_variables.at("REMOTE_FLAG"), "enabled");
    ASSERT_TRUE(sessions.front().work_secret_raw_json.has_value());
    EXPECT_NE(sessions.front().work_secret_raw_json->find(R"("session_ingress_token":"session-token-from-secret")"), std::string::npos);

    auto requests = server.wait_for_requests(3);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 3u);
    EXPECT_EQ((*requests)[0].method, "GET");
    EXPECT_EQ((*requests)[0].path, "/v1/environments/env_backend_1/work/poll");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer env_secret_1"), std::string::npos);

    EXPECT_EQ((*requests)[1].method, "POST");
    EXPECT_EQ((*requests)[1].path, "/v1/environments/env_backend_1/work/work_1/ack");
    EXPECT_NE((*requests)[1].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[1].body, "{}");

    EXPECT_EQ((*requests)[2].method, "POST");
    EXPECT_EQ((*requests)[2].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_NE((*requests)[2].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[2].body, "{}");

    auto completed = daemon.complete_session("daemon-session-1", "completed");
    ASSERT_TRUE(completed.has_value()) << completed.error();

    auto completion_requests = server.wait_for_requests(5);
    ASSERT_TRUE(completion_requests.has_value());
    ASSERT_EQ(completion_requests->size(), 5u);
    EXPECT_EQ((*completion_requests)[3].method, "POST");
    EXPECT_EQ((*completion_requests)[3].path, "/v1/environments/env_backend_1/work/work_1/stop");
    EXPECT_NE((*completion_requests)[3].headers.find("Authorization: Bearer oauth_token"), std::string::npos);
    EXPECT_EQ((*completion_requests)[3].body, R"({"force":false})");

    EXPECT_EQ((*completion_requests)[4].method, "POST");
    EXPECT_EQ((*completion_requests)[4].path, "/v1/sessions/session_1/archive");
    EXPECT_NE((*completion_requests)[4].headers.find("Authorization: Bearer oauth_token"), std::string::npos);
}

TEST(BridgeDaemon, HeartbeatsRunningRemoteWorkSessions) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_heartbeat.pid"),
        .port_file = unique_temp_file("_daemon_heartbeat.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
    });

    daemon.set_session_spawner([](std::string_view) -> std::expected<std::string, std::string> {
        return "daemon-session-heartbeat";
    });

    auto spawned = daemon.poll_for_work_once();
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(spawned->has_value());

    auto heartbeat = daemon.heartbeat_sessions_once();
    ASSERT_TRUE(heartbeat.has_value()) << heartbeat.error();
    EXPECT_EQ(*heartbeat, 1u);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().id, "daemon-session-heartbeat");
    ASSERT_TRUE(sessions.front().last_heartbeat_at.has_value());
    ASSERT_TRUE(sessions.front().last_heartbeat_state.has_value());
    EXPECT_EQ(*sessions.front().last_heartbeat_state, "active");
    ASSERT_TRUE(sessions.front().heartbeat_ttl_seconds.has_value());
    EXPECT_EQ(*sessions.front().heartbeat_ttl_seconds, 30);
    EXPECT_FALSE(sessions.front().last_heartbeat_error.has_value());
    EXPECT_EQ(sessions.front().heartbeat_failures, 0);

    auto requests = server.wait_for_requests(4);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 4u);
    EXPECT_EQ((*requests)[2].method, "POST");
    EXPECT_EQ((*requests)[2].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_NE((*requests)[2].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[2].body, "{}");
    EXPECT_EQ((*requests)[3].method, "POST");
    EXPECT_EQ((*requests)[3].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_NE((*requests)[3].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[3].body, "{}");
}

TEST(BridgeDaemon, ForkExecsHeadlessSessionAndReportsCompletion) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_process.pid"),
        .port_file = unique_temp_file("_daemon_process.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = "/usr/bin/true",
    });

    auto spawned = daemon.poll_for_work_once();
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(spawned->has_value());

    std::vector<loom::daemon::DaemonSession> sessions;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        sessions = daemon.sessions();
        if (!sessions.empty() && sessions.front().status != "running") break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");
    EXPECT_TRUE(sessions.front().completion_reported);
    ASSERT_TRUE(sessions.front().remote_session_id.has_value());
    EXPECT_EQ(*sessions.front().remote_session_id, "session_1");

    auto requests = server.wait_for_requests(5);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 5u);
    EXPECT_EQ((*requests)[0].method, "GET");
    EXPECT_EQ((*requests)[0].path, "/v1/environments/env_backend_1/work/poll");
    EXPECT_EQ((*requests)[1].method, "POST");
    EXPECT_EQ((*requests)[1].path, "/v1/environments/env_backend_1/work/work_1/ack");
    EXPECT_EQ((*requests)[2].method, "POST");
    EXPECT_EQ((*requests)[2].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_EQ((*requests)[3].method, "POST");
    EXPECT_EQ((*requests)[3].path, "/v1/environments/env_backend_1/work/work_1/stop");
    EXPECT_EQ((*requests)[3].body, R"({"force":false})");
    EXPECT_EQ((*requests)[4].method, "POST");
    EXPECT_EQ((*requests)[4].path, "/v1/sessions/session_1/archive");
}

TEST(BridgeDaemon, ForkExecsNativeHeadlessSessionThroughRemoteLifecycle) {
    const auto native_binary = native_loom_binary_path();
    if (native_binary.empty() || !std::filesystem::exists(native_binary)) {
        GTEST_SKIP() << "native loom binary is not available";
    }

    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());
    server.set_work_secret(bridge_test_encoded_work_secret(server.base_url()));

    const auto env_root = unique_temp_file("_native_headless_home");
    std::filesystem::remove_all(env_root);
    std::filesystem::create_directories(env_root);
    ScopedEnvVar api_key("LOOM_API_KEY", "fake-key");
    ScopedEnvVar api_base("LOOM_BASE_URL", server.base_url());
    ScopedEnvVar xdg_config("XDG_CONFIG_HOME", env_root.string());
    ScopedEnvVar home("HOME", env_root.string());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_native_process.pid"),
        .port_file = unique_temp_file("_daemon_native_process.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = native_binary.string(),
    });

    auto spawned = daemon.poll_for_work_once();
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(spawned->has_value());
    const auto session_id = **spawned;

    bool saw_assistant = false;
    bool saw_result = false;
    std::vector<std::string> lines;
    for (int i = 0; i < 300; ++i) {
        lines = daemon.session_stdout_lines(session_id);
        for (const auto& line : lines) {
            if (line.find("native daemon bridge product reply") != std::string::npos &&
                line.find(R"("type":"assistant")") != std::string::npos) {
                saw_assistant = true;
            }
            if (line.find("native daemon bridge product reply") != std::string::npos &&
                line.find(R"("type":"result")") != std::string::npos) {
                saw_result = true;
            }
        }
        if (saw_assistant && saw_result) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    ASSERT_GT(sessions.front().pid, 0);
    if (!saw_assistant || !saw_result) {
        ::kill(sessions.front().pid, SIGTERM);
        daemon.reap_sessions();
    }
    ASSERT_TRUE(saw_assistant) << "stdout:\n" << [&] {
        std::ostringstream out;
        for (const auto& line : lines) out << line << '\n';
        return out.str();
    }();
    ASSERT_TRUE(saw_result) << "stdout:\n" << [&] {
        std::ostringstream out;
        for (const auto& line : lines) out << line << '\n';
        return out.str();
    }();

    auto requests_before_shutdown = server.wait_for_requests(1);
    ASSERT_TRUE(requests_before_shutdown.has_value());
    const auto request_count_before_shutdown = requests_before_shutdown->size();

    ::kill(sessions.front().pid, SIGTERM);
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        sessions = daemon.sessions();
        if (!sessions.empty() && sessions.front().status != "running") break;
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
    }

    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");
    EXPECT_TRUE(sessions.front().completion_reported);
    ASSERT_TRUE(sessions.front().remote_session_id.has_value());
    EXPECT_EQ(*sessions.front().remote_session_id, "session_1");

    auto requests = server.wait_for_requests(request_count_before_shutdown + 2, std::chrono::seconds{5});
    ASSERT_TRUE(requests.has_value());

    auto saw_request = [&](std::string_view method, std::string_view path) {
        return std::ranges::any_of(*requests, [&](const LocalBridgeApiRequest& request) {
            return request.method == method && request.path == path;
        });
    };
    auto saw_body = [&](std::string_view path, std::string_view text) {
        return std::ranges::any_of(*requests, [&](const LocalBridgeApiRequest& request) {
            return request.path == path && request.body.find(text) != std::string::npos;
        });
    };

    EXPECT_TRUE(saw_request("GET", "/v1/environments/env_backend_1/work/poll"));
    EXPECT_TRUE(saw_request("POST", "/v1/environments/env_backend_1/work/work_1/ack"));
    EXPECT_TRUE(saw_request("POST", "/v1/code/sessions/session_1/worker/register"));
    EXPECT_TRUE(saw_request("GET", "/v1/code/sessions/session_1/worker/events/stream"));
    EXPECT_TRUE(saw_request("POST", "/v1/code/sessions/session_1/worker/heartbeat"));
    EXPECT_TRUE(saw_request("POST", "/v1/code/sessions/session_1/worker/events/delivery"));
    EXPECT_TRUE(saw_request("PUT", "/v1/code/sessions/session_1/worker"));
    EXPECT_TRUE(saw_request("POST", "/v1/messages"));
    EXPECT_TRUE(saw_request("POST", "/v1/environments/env_backend_1/work/work_1/stop"));
    EXPECT_TRUE(saw_request("POST", "/v1/sessions/session_1/archive"));
    EXPECT_TRUE(saw_body("/v1/messages", "run native daemon bridge product e2e"));
    EXPECT_TRUE(saw_body("/v1/code/sessions/session_1/worker/events", R"("status":"started")"));
    EXPECT_TRUE(saw_body("/v1/code/sessions/session_1/worker/events", R"("type":"assistant")"));
    EXPECT_TRUE(saw_body("/v1/code/sessions/session_1/worker/events", R"("type":"result")"));
    EXPECT_TRUE(saw_body("/v1/code/sessions/session_1/worker/events", R"("status":"stopped")"));

    std::filesystem::remove_all(env_root);
}

TEST(BridgeDaemon, ForkExecsHeadlessSessionWithCcrSdkUrl) {
    auto script = unique_temp_file("_headless_child_args.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "printf '%s\\n' \"$@\"\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    auto secret = loom::bridge::decode_work_secret(bridge_test_encoded_work_secret());
    ASSERT_TRUE(secret.has_value()) << secret.error();
    loom::bridge::WorkResponse work{
        .id = "work_1",
        .data_type = std::string("session"),
        .data_id = std::string("session_1"),
        .secret = bridge_test_encoded_work_secret(),
    };

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_sdk_url.pid"),
        .port_file = unique_temp_file("_daemon_sdk_url.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = "",
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto spawned = daemon.spawn_session_with_bridge_context("work_1", &work, &*secret);
    ASSERT_TRUE(spawned.has_value()) << spawned.error();

    std::vector<std::string> lines;
    bool saw_sdk_url_flag = false;
    bool saw_sdk_url = false;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        lines = daemon.session_stdout_lines(*spawned);
        for (const auto& line : lines) {
            if (line == "--sdk-url") saw_sdk_url_flag = true;
            if (line == "http://session-ingress.local/v1/code/sessions/session_1") saw_sdk_url = true;
        }
        auto sessions = daemon.sessions();
        if (saw_sdk_url_flag && saw_sdk_url && !sessions.empty() && sessions.front().status == "completed") break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_FALSE(lines.empty());
    EXPECT_TRUE(saw_sdk_url_flag);
    EXPECT_TRUE(saw_sdk_url);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");

    std::filesystem::remove(script);
}

TEST(BridgeDaemon, ForkExecsHeadlessSessionWithV1SessionIngressSdkUrl) {
    auto script = unique_temp_file("_headless_child_v1_args.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "printf '%s\\n' \"$@\"\n"
            << "printf 'LOOM_POST_FOR_SESSION_INGRESS_V2=%s\\n' \"$LOOM_POST_FOR_SESSION_INGRESS_V2\"\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    auto secret = loom::bridge::decode_work_secret(bridge_test_encoded_v1_work_secret());
    ASSERT_TRUE(secret.has_value()) << secret.error();
    loom::bridge::WorkResponse work{
        .id = "work_1",
        .data_type = std::string("session"),
        .data_id = std::string("session_1"),
        .secret = bridge_test_encoded_v1_work_secret(),
    };

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_v1_sdk_url.pid"),
        .port_file = unique_temp_file("_daemon_v1_sdk_url.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = "",
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto spawned = daemon.spawn_session_with_bridge_context("work_1", &work, &*secret);
    ASSERT_TRUE(spawned.has_value()) << spawned.error();

    std::vector<std::string> lines;
    bool saw_sdk_url_flag = false;
    bool saw_sdk_url = false;
    bool saw_v1_post_env = false;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        lines = daemon.session_stdout_lines(*spawned);
        for (const auto& line : lines) {
            if (line == "--sdk-url") saw_sdk_url_flag = true;
            if (line == "ws://127.0.0.1:19191/v2/session_ingress/ws/session_1") saw_sdk_url = true;
            if (line == "LOOM_POST_FOR_SESSION_INGRESS_V2=1") saw_v1_post_env = true;
        }
        auto sessions = daemon.sessions();
        if (saw_sdk_url_flag && saw_sdk_url && saw_v1_post_env
            && !sessions.empty() && sessions.front().status == "completed") break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_FALSE(lines.empty());
    EXPECT_TRUE(saw_sdk_url_flag);
    EXPECT_TRUE(saw_sdk_url);
    EXPECT_TRUE(saw_v1_post_env);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");

    std::filesystem::remove(script);
}

TEST(BridgeDaemon, ForkExecsHeadlessSessionWithCcrWorkerEpochEnvironment) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    auto script = unique_temp_file("_headless_child_ccr_env.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "for arg in \"$@\"; do printf 'ARG:%s\\n' \"$arg\"; done\n"
            << "printf 'ENV:LOOM_WORKER_EPOCH=%s\\n' \"$LOOM_WORKER_EPOCH\"\n"
            << "printf 'ENV:LOOM_USE_CCR_V2=%s\\n' \"$LOOM_USE_CCR_V2\"\n"
            << "printf 'ENV:LOOM_REMOTE_API_BASE_URL=%s\\n' \"$LOOM_REMOTE_API_BASE_URL\"\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    auto secret = loom::bridge::decode_work_secret(bridge_test_encoded_work_secret());
    ASSERT_TRUE(secret.has_value()) << secret.error();
    loom::bridge::WorkResponse work{
        .id = "work_1",
        .data_type = std::string("session"),
        .data_id = std::string("session_1"),
        .secret = bridge_test_encoded_work_secret(),
    };

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_ccr_env.pid"),
        .port_file = unique_temp_file("_daemon_ccr_env.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto spawned = daemon.spawn_session_with_bridge_context("work_1", &work, &*secret);
    ASSERT_TRUE(spawned.has_value()) << spawned.error();

    std::vector<std::string> lines;
    const auto expected_sdk_url = "ARG:" + server.base_url() + "/v1/code/sessions/session_1";
    bool saw_sdk_url = false;
    bool saw_worker_epoch = false;
    bool saw_ccr_v2 = false;
    bool saw_api_base = false;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        lines = daemon.session_stdout_lines(*spawned);
        for (const auto& line : lines) {
            if (line == expected_sdk_url) saw_sdk_url = true;
            if (line == "ENV:LOOM_WORKER_EPOCH=42") saw_worker_epoch = true;
            if (line == "ENV:LOOM_USE_CCR_V2=1") saw_ccr_v2 = true;
            if (line == "ENV:LOOM_REMOTE_API_BASE_URL=" + server.base_url()) saw_api_base = true;
        }
        auto sessions = daemon.sessions();
        if (saw_sdk_url && saw_worker_epoch && saw_ccr_v2 && saw_api_base
            && !sessions.empty() && sessions.front().status == "completed") break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_FALSE(lines.empty());
    EXPECT_TRUE(saw_sdk_url);
    EXPECT_TRUE(saw_worker_epoch);
    EXPECT_TRUE(saw_ccr_v2);
    EXPECT_TRUE(saw_api_base);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    ASSERT_TRUE(sessions.front().worker_epoch.has_value());
    EXPECT_EQ(*sessions.front().worker_epoch, 42);

    auto requests = server.wait_for_requests(1);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 1u);
    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[0].body, "{}");

    std::filesystem::remove(script);
}

TEST(BridgeDaemon, PipesHeadlessChildStdinAndCapturesStdout) {
    auto script = unique_temp_file("_headless_child.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "while IFS= read -r line; do\n"
            << "  printf '%s\\n' \"$line\"\n"
            << "  printf '%s\\n' '{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"child reply\"}]}}'\n"
            << "done\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_stdio.pid"),
        .port_file = unique_temp_file("_daemon_stdio.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = "",
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto spawned = daemon.spawn_session("stdio-work");
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(daemon.send_session_stdin(*spawned,
        R"({"type":"user","message":{"role":"user","content":"hello child"},"session_id":"session_1"})" "\n").has_value());
    ASSERT_TRUE(daemon.close_session_stdin(*spawned).has_value());

    std::vector<std::string> lines;
    std::vector<loom::daemon::DaemonSession> sessions;
    for (int i = 0; i < 500; ++i) {
        lines = daemon.session_stdout_lines(*spawned);
        sessions = daemon.sessions();
        if (lines.size() >= 2 && !sessions.empty() && sessions.front().stdout_closed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    ASSERT_GE(lines.size(), 2u);
    EXPECT_NE(lines[0].find(R"("content":"hello child")"), std::string::npos);
    EXPECT_EQ(lines[1], R"({"type":"assistant","message":{"content":[{"type":"text","text":"child reply"}]}})");

    daemon.reap_sessions();
    sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");
    EXPECT_EQ(sessions.front().stdin_fd, -1);
    EXPECT_TRUE(sessions.front().stdout_closed);

    std::filesystem::remove(script);
}

TEST(BridgeDaemon, RpcStdinRoutesRemoteInputToHeadlessChild) {
    auto script = unique_temp_file("_headless_child_rpc.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "while IFS= read -r line; do\n"
            << "  printf '%s\\n' \"$line\"\n"
            << "  printf '%s\\n' '{\"type\":\"result\",\"result\":\"rpc child reply\"}'\n"
            << "done\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_rpc_stdio.pid"),
        .port_file = unique_temp_file("_daemon_rpc_stdio.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = "",
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto started = daemon.start();
    ASSERT_TRUE(started.has_value()) << started.error();

    loom::daemon::DaemonClient client;
    ASSERT_TRUE(client.connect(*started).has_value());
    auto spawned = client.spawn("rpc-stdio-work");
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    auto session_id = extract_json_string_field(*spawned, "session_id");
    ASSERT_FALSE(session_id.empty()) << *spawned;

    auto sent = client.send_stdin(
        session_id,
        R"({"type":"user","message":{"role":"user","content":"rpc hello child"},"session_id":"session_1"})" "\n");
    ASSERT_TRUE(sent.has_value()) << sent.error();
    auto closed = client.close_stdin(session_id);
    ASSERT_TRUE(closed.has_value()) << closed.error();

    std::string stdout_response;
    bool saw_reply = false;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        auto stdout_result = client.stdout_lines(session_id);
        ASSERT_TRUE(stdout_result.has_value()) << stdout_result.error();
        stdout_response = *stdout_result;
        if (stdout_response.find("rpc child reply") != std::string::npos) saw_reply = true;
        auto sessions = daemon.sessions();
        if (saw_reply && !sessions.empty() && sessions.front().stdout_closed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    EXPECT_NE(stdout_response.find("rpc hello child"), std::string::npos);
    EXPECT_NE(stdout_response.find("rpc child reply"), std::string::npos);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().status, "completed");
    EXPECT_EQ(sessions.front().stdin_fd, -1);
    EXPECT_TRUE(sessions.front().stdout_closed);

    client.disconnect();
    daemon.stop();
    std::filesystem::remove(script);
}

TEST(BridgeDaemon, RpcEventRoutesRemotePayloadToHeadlessChildStdin) {
    auto script = unique_temp_file("_headless_child_event_rpc.sh");
    {
        std::ofstream out(script);
        out << "#!/bin/sh\n"
            << "while IFS= read -r line; do\n"
            << "  printf '%s\\n' \"$line\"\n"
            << "done\n";
    }
    std::filesystem::permissions(
        script,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write |
            std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace);

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_rpc_event.pid"),
        .port_file = unique_temp_file("_daemon_rpc_event.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = "",
        .bridge_environment_id = "",
        .bridge_environment_secret = "",
        .bridge_access_token = "",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
        .session_binary = script.string(),
    });

    auto started = daemon.start();
    ASSERT_TRUE(started.has_value()) << started.error();

    loom::daemon::DaemonClient client;
    ASSERT_TRUE(client.connect(*started).has_value());
    auto spawned = client.spawn("rpc-event-work");
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    auto session_id = extract_json_string_field(*spawned, "session_id");
    ASSERT_FALSE(session_id.empty()) << *spawned;

    auto sent = client.send_event(
        session_id,
        R"({"type":"user","message":{"role":"user","content":"remote event hello"},"session_id":"session_1"})");
    ASSERT_TRUE(sent.has_value()) << sent.error();
    auto closed = client.close_stdin(session_id);
    ASSERT_TRUE(closed.has_value()) << closed.error();

    std::string stdout_response;
    bool saw_hello = false;
    for (int i = 0; i < 500; ++i) {
        daemon.reap_sessions();
        auto stdout_result = client.stdout_lines(session_id);
        ASSERT_TRUE(stdout_result.has_value()) << stdout_result.error();
        stdout_response = *stdout_result;
        if (stdout_response.find("remote event hello") != std::string::npos) saw_hello = true;
        auto sessions = daemon.sessions();
        if (saw_hello && !sessions.empty() && sessions.front().stdout_closed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    EXPECT_NE(stdout_response.find("remote event hello"), std::string::npos);

    auto raw_lines = daemon.session_stdout_lines(session_id);
    ASSERT_FALSE(raw_lines.empty());
    EXPECT_NE(raw_lines.front().find(R"("type":"user")"), std::string::npos);
    EXPECT_NE(raw_lines.front().find("remote event hello"), std::string::npos);

    auto sessions = daemon.sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions.front().delivered_remote_events, 1u);
    ASSERT_TRUE(sessions.front().last_delivered_remote_event_type.has_value());
    EXPECT_EQ(*sessions.front().last_delivered_remote_event_type, "user");
    EXPECT_FALSE(sessions.front().last_remote_event_error.has_value());
    EXPECT_EQ(sessions.front().status, "completed");

    client.disconnect();
    daemon.stop();
    std::filesystem::remove(script);
}

TEST(BridgeDaemon, PollAuthFailureUpdatesBackoffAndResetsAfterSuccess) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());
    server.fail_next_work_polls(1, 401, "Unauthorized", R"({"error":"unauthorized"})");

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_auth.pid"),
        .port_file = unique_temp_file("_daemon_auth.port"),
        .poll_interval = std::chrono::seconds{1},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
    });

    auto failed = daemon.poll_for_work_once();
    ASSERT_FALSE(failed.has_value());
    EXPECT_NE(failed.error().find("[E200]"), std::string::npos);
    EXPECT_NE(failed.error().find("HTTP 401"), std::string::npos);

    auto failed_state = daemon.backoff_state();
    EXPECT_EQ(failed_state.consecutive_poll_failures, 1);
    EXPECT_EQ(failed_state.current_poll_backoff, std::chrono::seconds{1});
    EXPECT_TRUE(failed_state.auth_failed);
    ASSERT_TRUE(failed_state.last_poll_error.has_value());
    EXPECT_NE(failed_state.last_poll_error->find("HTTP 401"), std::string::npos);

    daemon.set_session_spawner([](std::string_view) -> std::expected<std::string, std::string> {
        return "daemon-session-after-auth";
    });

    auto recovered = daemon.poll_for_work_once();
    ASSERT_TRUE(recovered.has_value()) << recovered.error();
    ASSERT_TRUE(recovered->has_value());
    EXPECT_EQ(**recovered, "daemon-session-after-auth");

    auto recovered_state = daemon.backoff_state();
    EXPECT_EQ(recovered_state.consecutive_poll_failures, 0);
    EXPECT_EQ(recovered_state.current_poll_backoff, std::chrono::seconds{0});
    EXPECT_FALSE(recovered_state.last_poll_error.has_value());
    EXPECT_FALSE(recovered_state.auth_failed);

    auto requests = server.wait_for_requests(4);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 4u);
    EXPECT_EQ((*requests)[0].method, "GET");
    EXPECT_EQ((*requests)[0].path, "/v1/environments/env_backend_1/work/poll");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer env_secret_1"), std::string::npos);
    EXPECT_EQ((*requests)[1].method, "GET");
    EXPECT_EQ((*requests)[1].path, "/v1/environments/env_backend_1/work/poll");
    EXPECT_EQ((*requests)[2].method, "POST");
    EXPECT_EQ((*requests)[2].path, "/v1/environments/env_backend_1/work/work_1/ack");
    EXPECT_EQ((*requests)[3].method, "POST");
    EXPECT_EQ((*requests)[3].path, "/v1/code/sessions/session_1/worker/register");
}

TEST(BridgeDaemon, ConsecutivePollFailuresUseExponentialBackoffCappedAtFiveMinutes) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_backoff.pid"),
        .port_file = unique_temp_file("_daemon_backoff.port"),
        .poll_interval = std::chrono::seconds{5},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
    });

    const std::vector<std::chrono::seconds> expected_backoffs{
        std::chrono::seconds{5},
        std::chrono::seconds{10},
        std::chrono::seconds{20},
        std::chrono::seconds{40},
        std::chrono::seconds{80},
        std::chrono::seconds{160},
        std::chrono::seconds{300},
        std::chrono::seconds{300},
    };
    constexpr std::size_t http_attempts_per_failed_call = 4;
    server.fail_next_work_polls(
        static_cast<int>(expected_backoffs.size() * http_attempts_per_failed_call),
        500,
        "Internal Server Error",
        R"({"error":"server error"})");

    for (std::size_t i = 0; i < expected_backoffs.size(); ++i) {
        auto failed = daemon.poll_for_work_once();
        ASSERT_FALSE(failed.has_value()) << "failure index " << i;
        auto state = daemon.backoff_state();
        EXPECT_EQ(state.consecutive_poll_failures, static_cast<int>(i + 1));
        EXPECT_EQ(state.current_poll_backoff, expected_backoffs[i]);
        EXPECT_FALSE(state.auth_failed);
        ASSERT_TRUE(state.last_poll_error.has_value());
        EXPECT_NE(state.last_poll_error->find("HTTP 500"), std::string::npos);
    }

    auto requests = server.wait_for_requests(expected_backoffs.size() * http_attempts_per_failed_call);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), expected_backoffs.size() * http_attempts_per_failed_call);
    for (const auto& request : *requests) {
        EXPECT_EQ(request.method, "GET");
        EXPECT_EQ(request.path, "/v1/environments/env_backend_1/work/poll");
        EXPECT_NE(request.headers.find("Authorization: Bearer env_secret_1"), std::string::npos);
    }
}

TEST(BridgeDaemon, HeartbeatFailureRecordsErrorAndResetsAfterSuccess) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::daemon::DaemonServer daemon(loom::daemon::DaemonConfig{
        .port = 0,
        .pid_file = unique_temp_file("_daemon_heartbeat_failure.pid"),
        .port_file = unique_temp_file("_daemon_heartbeat_failure.port"),
        .poll_interval = std::chrono::seconds{30},
        .heartbeat_interval = std::chrono::seconds{60},
        .max_sessions = 2,
        .work_api_url = server.base_url(),
        .bridge_environment_id = "env_backend_1",
        .bridge_environment_secret = "env_secret_1",
        .bridge_access_token = "oauth_token",
        .bridge_runner_version = "test-runner",
        .trusted_device_token = std::nullopt,
    });
    daemon.set_session_spawner([](std::string_view) -> std::expected<std::string, std::string> {
        return "daemon-session-heartbeat-failure";
    });

    auto spawned = daemon.poll_for_work_once();
    ASSERT_TRUE(spawned.has_value()) << spawned.error();
    ASSERT_TRUE(spawned->has_value());

    server.fail_next_heartbeats(4, 503, "Service Unavailable", R"({"error":"temporary outage"})");
    auto failed_heartbeat = daemon.heartbeat_sessions_once();
    ASSERT_FALSE(failed_heartbeat.has_value());
    EXPECT_NE(failed_heartbeat.error().find("HTTP 503"), std::string::npos);

    auto failed_sessions = daemon.sessions();
    ASSERT_EQ(failed_sessions.size(), 1u);
    EXPECT_EQ(failed_sessions.front().heartbeat_failures, 1);
    ASSERT_TRUE(failed_sessions.front().last_heartbeat_error.has_value());
    EXPECT_NE(failed_sessions.front().last_heartbeat_error->find("HTTP 503"), std::string::npos);
    EXPECT_FALSE(failed_sessions.front().last_heartbeat_at.has_value());

    auto recovered_heartbeat = daemon.heartbeat_sessions_once();
    ASSERT_TRUE(recovered_heartbeat.has_value()) << recovered_heartbeat.error();
    EXPECT_EQ(*recovered_heartbeat, 1u);

    auto recovered_sessions = daemon.sessions();
    ASSERT_EQ(recovered_sessions.size(), 1u);
    EXPECT_EQ(recovered_sessions.front().heartbeat_failures, 0);
    EXPECT_FALSE(recovered_sessions.front().last_heartbeat_error.has_value());
    ASSERT_TRUE(recovered_sessions.front().last_heartbeat_at.has_value());
    ASSERT_TRUE(recovered_sessions.front().last_heartbeat_state.has_value());
    EXPECT_EQ(*recovered_sessions.front().last_heartbeat_state, "active");
    ASSERT_TRUE(recovered_sessions.front().heartbeat_ttl_seconds.has_value());
    EXPECT_EQ(*recovered_sessions.front().heartbeat_ttl_seconds, 30);

    auto requests = server.wait_for_requests(8);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 8u);
    EXPECT_EQ((*requests)[0].path, "/v1/environments/env_backend_1/work/poll");
    EXPECT_EQ((*requests)[1].path, "/v1/environments/env_backend_1/work/work_1/ack");
    EXPECT_EQ((*requests)[2].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_EQ((*requests)[3].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_EQ((*requests)[4].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_EQ((*requests)[5].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_EQ((*requests)[6].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_EQ((*requests)[7].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
}
