/// @file test_bridge_api.cpp
/// @brief Bridge API client and work-secret tests (split from test_bridge.cpp).

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

TEST(BridgeApi, ValidatesSafeIdsAndNormalizesSessionIds) {
    EXPECT_TRUE(loom::bridge::is_safe_bridge_id("env_abc-123"));
    EXPECT_FALSE(loom::bridge::is_safe_bridge_id("env/abc"));

    auto generated = loom::bridge::generate_session_id();
    EXPECT_EQ(generated.size(), 36u);
    EXPECT_EQ(generated.rfind("ses_", 0), 0u);
}

TEST(BridgeApi, RejectsUnsafeIdsBeforeNetworkCalls) {
    loom::bridge::BridgeApiClient client(loom::bridge::BridgeApiConfig{
        .base_url = "https://bridge.example.test",
        .access_token = "token",
        .runner_version = "test",
        .trusted_device_token = std::nullopt,
    });

    EXPECT_FALSE(client.poll_for_work("bad/id", "secret").has_value());
    EXPECT_FALSE(client.acknowledge_work("env", "bad/id", "session").has_value());
    EXPECT_FALSE(client.archive_session("bad/id").has_value());
}

TEST(BridgeWorkSecret, DecodesExtendedRemotePayload) {
    auto decoded = loom::bridge::decode_work_secret(bridge_test_encoded_work_secret());
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(decoded->version, 1);
    EXPECT_EQ(decoded->session_ingress_token, "session-token-from-secret");
    EXPECT_EQ(decoded->api_base_url, "http://session-ingress.local");
    ASSERT_TRUE(decoded->use_code_sessions.has_value());
    EXPECT_TRUE(*decoded->use_code_sessions);
    ASSERT_TRUE(decoded->code_session_mode.has_value());
    EXPECT_EQ(*decoded->code_session_mode, "code-session");

    ASSERT_EQ(decoded->sources_json.size(), 1u);
    EXPECT_NE(decoded->sources_json.front().find(R"("type":"github")"), std::string::npos);
    ASSERT_TRUE(decoded->auth_json.has_value());
    EXPECT_NE(decoded->auth_json->find(R"("name":"console")"), std::string::npos);
    ASSERT_TRUE(decoded->mcp_config_json.has_value());
    EXPECT_NE(decoded->mcp_config_json->find(R"("linear")"), std::string::npos);
    ASSERT_TRUE(decoded->environment_json.has_value());
    EXPECT_NE(decoded->environment_json->find(R"("REMOTE_FLAG":"enabled")"), std::string::npos);
    ASSERT_TRUE(decoded->environment_variables.contains("REMOTE_FLAG"));
    EXPECT_EQ(decoded->environment_variables.at("REMOTE_FLAG"), "enabled");
    ASSERT_TRUE(decoded->environment_variables.contains("REMOTE_COUNT"));
    EXPECT_EQ(decoded->environment_variables.at("REMOTE_COUNT"), "42");
    EXPECT_NE(decoded->raw_json.find(R"("session_ingress_token":"session-token-from-secret")"), std::string::npos);
}

TEST(BridgeApi, ParsesRegistrationPollAndLifecycleResponsesFromServer) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::bridge::BridgeApiClient client(loom::bridge::BridgeApiConfig{
        .base_url = server.base_url(),
        .access_token = "oauth_token",
        .runner_version = "test-runner",
        .trusted_device_token = "trusted_device",
    });

    auto registration = client.register_environment(loom::bridge::BridgeConfig{
        .transport = loom::bridge::TransportType::websocket,
        .host = "127.0.0.1",
        .port = 7777,
        .path = "/bridge",
        .auth_token = std::nullopt,
    });
    ASSERT_TRUE(registration.has_value()) << registration.error().format();
    EXPECT_EQ(registration->environment_id, "env_backend_1");
    EXPECT_EQ(registration->environment_secret, "env_secret_1");

    auto work = client.poll_for_work(registration->environment_id, registration->environment_secret, 12345);
    ASSERT_TRUE(work.has_value()) << work.error().format();
    ASSERT_TRUE(work->has_value());
    EXPECT_EQ((*work)->id, "work_1");
    EXPECT_EQ((*work)->secret, bridge_test_encoded_work_secret());
    ASSERT_TRUE((*work)->data_type.has_value());
    EXPECT_EQ(*(*work)->data_type, "session");
    ASSERT_TRUE((*work)->data_id.has_value());
    EXPECT_EQ(*(*work)->data_id, "session_1");

    auto worker_registration = client.register_worker(
        server.base_url() + "/v1/code/sessions/session_1",
        "session-token-from-secret");
    ASSERT_TRUE(worker_registration.has_value()) << worker_registration.error().format();
    EXPECT_EQ(worker_registration->worker_epoch, 42);

    auto ack = client.acknowledge_work(registration->environment_id, (*work)->id, "session-token-from-secret");
    ASSERT_TRUE(ack.has_value()) << ack.error().format();

    auto heartbeat = client.heartbeat_work(registration->environment_id, (*work)->id, "session-token-from-secret");
    ASSERT_TRUE(heartbeat.has_value()) << heartbeat.error().format();
    EXPECT_TRUE(heartbeat->lease_extended);
    EXPECT_EQ(heartbeat->state, "active");
    ASSERT_TRUE(heartbeat->last_heartbeat.has_value());
    EXPECT_EQ(*heartbeat->last_heartbeat, "2026-06-07T00:00:00Z");
    ASSERT_TRUE(heartbeat->ttl_seconds.has_value());
    EXPECT_EQ(*heartbeat->ttl_seconds, 30);

    auto permission_response = client.send_permission_response_event(
        "session_1",
        loom::bridge::PermissionResponseEvent{
            .request_id = "permission-1",
            .response_json = R"({"behavior":"allow","updatedInput":{"cmd":"ls"}})",
            .subtype = "success",
            .error = std::nullopt,
        },
        "session-token-from-secret");
    ASSERT_TRUE(permission_response.has_value()) << permission_response.error().format();

    auto stop = client.stop_work(registration->environment_id, (*work)->id, true);
    ASSERT_TRUE(stop.has_value()) << stop.error().format();

    auto deregister = client.deregister_environment(registration->environment_id);
    ASSERT_TRUE(deregister.has_value()) << deregister.error().format();

    auto requests = server.wait_for_requests(8);
    ASSERT_TRUE(requests.has_value());
    ASSERT_EQ(requests->size(), 8u);

    EXPECT_EQ((*requests)[0].method, "POST");
    EXPECT_EQ((*requests)[0].path, "/v1/environments/bridge");
    EXPECT_NE((*requests)[0].headers.find("Authorization: Bearer oauth_token"), std::string::npos);
    EXPECT_NE((*requests)[0].headers.find("X-Trusted-Device-Token: trusted_device"), std::string::npos);
    EXPECT_NE((*requests)[0].body.find(R"("transport":"websocket")"), std::string::npos);

    EXPECT_EQ((*requests)[1].method, "GET");
    EXPECT_EQ((*requests)[1].path, "/v1/environments/env_backend_1/work/poll?reclaim_older_than_ms=12345");
    EXPECT_NE((*requests)[1].headers.find("Authorization: Bearer env_secret_1"), std::string::npos);

    EXPECT_EQ((*requests)[2].method, "POST");
    EXPECT_EQ((*requests)[2].path, "/v1/code/sessions/session_1/worker/register");
    EXPECT_NE((*requests)[2].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[2].body, "{}");

    EXPECT_EQ((*requests)[3].method, "POST");
    EXPECT_EQ((*requests)[3].path, "/v1/environments/env_backend_1/work/work_1/ack");
    EXPECT_NE((*requests)[3].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[3].body, "{}");

    EXPECT_EQ((*requests)[4].method, "POST");
    EXPECT_EQ((*requests)[4].path, "/v1/environments/env_backend_1/work/work_1/heartbeat");
    EXPECT_NE((*requests)[4].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_EQ((*requests)[4].body, "{}");

    EXPECT_EQ((*requests)[5].method, "POST");
    EXPECT_EQ((*requests)[5].path, "/v1/sessions/session_1/events");
    EXPECT_NE((*requests)[5].headers.find("Authorization: Bearer session-token-from-secret"), std::string::npos);
    EXPECT_NE((*requests)[5].body.find(R"("events":[)"), std::string::npos);
    EXPECT_NE((*requests)[5].body.find(R"("type":"control_response")"), std::string::npos);
    EXPECT_NE((*requests)[5].body.find(R"("request_id":"permission-1")"), std::string::npos);
    EXPECT_NE((*requests)[5].body.find(R"("behavior":"allow")"), std::string::npos);
    EXPECT_NE((*requests)[5].body.find(R"("updatedInput":{"cmd":"ls"})"), std::string::npos);

    EXPECT_EQ((*requests)[6].method, "POST");
    EXPECT_EQ((*requests)[6].path, "/v1/environments/env_backend_1/work/work_1/stop");
    EXPECT_NE((*requests)[6].headers.find("Authorization: Bearer oauth_token"), std::string::npos);
    EXPECT_EQ((*requests)[6].body, R"({"force":true})");

    EXPECT_EQ((*requests)[7].method, "DELETE");
    EXPECT_EQ((*requests)[7].path, "/v1/environments/bridge/env_backend_1");
    EXPECT_NE((*requests)[7].headers.find("Authorization: Bearer oauth_token"), std::string::npos);
}
