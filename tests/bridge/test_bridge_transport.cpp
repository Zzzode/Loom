/// @file test_bridge_transport.cpp
/// @brief Bridge messages, config, and transport tests (split from test_bridge.cpp).

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

TEST(BridgeMessages, DetectsAndNormalizesMalformedBase64Images) {
    loom::bridge::ContentBlock malformed = loom::bridge::ImageBlock{
        .type = loom::bridge::ContentBlockType::Image,
        .source = {.media_type = "", .data = "iVBORw0KGgo="},
    };

    EXPECT_TRUE(loom::bridge::is_malformed_base64_image(malformed));
    EXPECT_EQ(loom::bridge::detect_image_format_from_base64("iVBORw0KGgo="), "image/png");

    auto normalized = loom::bridge::normalize_image_blocks({malformed});
    ASSERT_EQ(normalized.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<loom::bridge::ImageBlock>(normalized.front()));
    EXPECT_EQ(std::get<loom::bridge::ImageBlock>(normalized.front()).source.media_type, "image/png");
}

TEST(BridgeMessages, ExtractsOnlyUserMessagesWithContent) {
    loom::bridge::SDKMessage ignored;
    ignored.type = "assistant";
    EXPECT_FALSE(loom::bridge::extract_inbound_message_fields(ignored).has_value());

    loom::bridge::SDKMessage user;
    user.type = "user";
    user.message.content = std::string("hello bridge");
    user.uuid = "uuid-1";

    auto extracted = loom::bridge::extract_inbound_message_fields(user);
    ASSERT_TRUE(extracted.has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(extracted->content));
    EXPECT_EQ(std::get<std::string>(extracted->content), "hello bridge");
    ASSERT_TRUE(extracted->uuid.has_value());
    EXPECT_EQ(*extracted->uuid, "uuid-1");
}

TEST(BridgeMessages, FlushGateBuffersUntilOpened) {
    std::vector<std::string> handled;
    loom::bridge::MessageFlushGate gate([&handled](const loom::bridge::SDKMessage& msg) {
        handled.push_back(msg.type);
    });

    loom::bridge::SDKMessage message;
    message.type = "user";
    gate.enqueue(message);
    EXPECT_EQ(gate.buffer().size(), 1u);
    EXPECT_TRUE(handled.empty());

    gate.open();
    EXPECT_TRUE(gate.is_open());
    EXPECT_TRUE(gate.buffer().empty());
    ASSERT_EQ(handled.size(), 1u);
    EXPECT_EQ(handled.front(), "user");
}

TEST(BridgeConfig, LoadsDefaultsEnvironmentAndJsonFile) {
    ScopedEnvVar clear_port("LOOM_BRIDGE_PORT");
    ScopedEnvVar clear_host("LOOM_BRIDGE_HOST");
    ScopedEnvVar clear_token("LOOM_BRIDGE_TOKEN");

    loom::bridge::BridgeConfigLoader loader;
    auto defaults = loader.load();
    ASSERT_TRUE(defaults.has_value());
    EXPECT_EQ(defaults->host, "localhost");
    EXPECT_EQ(defaults->port, 7860u);
    EXPECT_EQ(defaults->transport, loom::bridge::TransportType::websocket);

    auto path = unique_temp_file("_bridge_config.json");
    {
        std::ofstream out(path);
        out << R"({"transport":"http-polling","host":"127.0.0.1","port":9000,"path":"/x","auth_token":"tok","debug_mode":true,"auto_connect":false})";
    }

    auto loaded = loader.load_from_file(path.string());
    std::filesystem::remove(path);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->transport, loom::bridge::TransportType::http_polling);
    EXPECT_EQ(loaded->host, "127.0.0.1");
    EXPECT_EQ(loaded->port, 9000u);
    EXPECT_EQ(loaded->path, "/x");
    ASSERT_TRUE(loaded->auth_token.has_value());
    EXPECT_EQ(*loaded->auth_token, "tok");
    EXPECT_TRUE(loaded->debug_mode);
    EXPECT_FALSE(loaded->auto_connect);
}

TEST(BridgeConfig, RejectsMissingOrInvalidConfigFiles) {
    loom::bridge::BridgeConfigLoader loader;
    EXPECT_FALSE(loader.load_from_file("/definitely/not/present/bridge.json").has_value());

    auto path = unique_temp_file("_bridge_config_invalid.json");
    {
        std::ofstream out(path);
        out << R"({"transport":"stdio","port":70000})";
    }

    auto loaded = loader.load_from_file(path.string());
    std::filesystem::remove(path);
    EXPECT_FALSE(loaded.has_value());
}

TEST(BridgeTransport, WebSocketConnectSendsAndDisconnects) {
    LocalBridgeWebSocketServer server;
    ASSERT_TRUE(server.ready());

    loom::bridge::WebSocketTransport transport;
    std::vector<loom::bridge::TransportState> transitions;
    std::vector<std::string> received;
    std::mutex received_mutex;
    std::condition_variable received_cv;
    transport.on_state_change([&transitions](loom::bridge::TransportState, loom::bridge::TransportState next) {
        transitions.push_back(next);
    });
    transport.on_message([&](loom::bridge::BridgeMessage msg) {
        {
            std::lock_guard lock(received_mutex);
            received.push_back(msg.id);
        }
        received_cv.notify_all();
    });

    const auto url = std::format("ws://127.0.0.1:{}/bridge", server.port());
    ASSERT_TRUE(transport.connect(url, std::nullopt).has_value());
    EXPECT_TRUE(transport.is_connected());

    loom::bridge::BridgeMessage message{
        .id = "msg-1",
        .type = "request",
        .method = "ping",
        .payload = R"({"ok":true})",
        .priority = loom::bridge::MessagePriority::high,
        .timestamp = std::chrono::system_clock::now(),
        .correlation_id = "corr-1",
    };
    ASSERT_TRUE(transport.send(message).has_value());
    ASSERT_EQ(transport.sent_frames().size(), 1u);
    EXPECT_NE(transport.sent_frames().front().find("msg-1"), std::string::npos);

    auto server_frames = server.wait_for_frames(1);
    ASSERT_TRUE(server_frames.has_value());
    ASSERT_EQ(server_frames->size(), 1u);
    EXPECT_NE(server_frames->front().find("msg-1"), std::string::npos);
    EXPECT_NE(server_frames->front().find(R"("method":"ping")"), std::string::npos);

    {
        std::unique_lock lock(received_mutex);
        ASSERT_TRUE(received_cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return !received.empty();
        }));
    }
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received.front(), "srv-1");

    transport.disconnect();
    EXPECT_FALSE(transport.is_connected());
    ASSERT_FALSE(transitions.empty());
    EXPECT_EQ(transitions.back(), loom::bridge::TransportState::disconnected);
}

TEST(BridgeTransport, FlushGateQueuesUntilOpened) {
    loom::bridge::TransportFlushGate gate;
    std::vector<std::string> sent;
    gate.set_sender([&sent](loom::bridge::BridgeMessage msg) { sent.push_back(msg.id); });

    gate.close();
    loom::bridge::BridgeMessage queued;
    queued.id = "queued";
    queued.type = "event";
    gate.enqueue(queued);
    EXPECT_EQ(gate.pending_count(), 1u);
    EXPECT_TRUE(sent.empty());

    gate.open();
    EXPECT_EQ(gate.pending_count(), 0u);
    ASSERT_EQ(sent.size(), 1u);
    EXPECT_EQ(sent.front(), "queued");
}

TEST(BridgeTransport, CapacityWakeInvokesCallbackOnReleaseBelowCapacity) {
    loom::bridge::CapacityWake capacity;
    bool notified = false;
    capacity.set_capacity(2);
    capacity.on_available([&] { notified = true; });

    capacity.record_usage(2);
    EXPECT_FALSE(capacity.has_capacity());

    capacity.release(1);
    EXPECT_TRUE(capacity.has_capacity());
    EXPECT_TRUE(notified);
}

TEST(BridgeTransport, HttpPollingPostsMessagesAndPollsInboundEvents) {
    LocalBridgeApiHttpServer server;
    ASSERT_TRUE(server.ready());

    loom::bridge::HttpPollingTransport transport;
    std::vector<loom::bridge::BridgeMessage> received;
    std::mutex received_mutex;
    std::condition_variable received_cv;
    transport.on_message([&](loom::bridge::BridgeMessage msg) {
        {
            std::lock_guard lock(received_mutex);
            received.push_back(std::move(msg));
        }
        received_cv.notify_all();
    });

    ASSERT_TRUE(transport.connect(server.base_url() + "/bridge", "poll-token").has_value());
    ASSERT_TRUE(transport.is_connected());

    loom::bridge::BridgeMessage outbound{
        .id = "client-1",
        .type = "event",
        .method = "client/ping",
        .payload = R"({"hello":true})",
        .priority = loom::bridge::MessagePriority::normal,
        .timestamp = std::chrono::system_clock::now(),
        .correlation_id = std::nullopt,
    };
    ASSERT_TRUE(transport.send(outbound).has_value());

    auto requests = server.wait_for_requests(2);
    ASSERT_TRUE(requests.has_value());
    bool saw_post = false;
    bool saw_poll = false;
    for (const auto& request : *requests) {
        if (request.method == "POST" && request.path == "/bridge/messages") {
            saw_post = true;
            EXPECT_NE(request.headers.find("Authorization: Bearer poll-token"), std::string::npos);
            EXPECT_NE(request.body.find(R"("id":"client-1")"), std::string::npos);
            EXPECT_NE(request.body.find(R"("method":"client/ping")"), std::string::npos);
        }
        if (request.method == "GET" && request.path == "/bridge/poll") {
            saw_poll = true;
            EXPECT_NE(request.headers.find("Authorization: Bearer poll-token"), std::string::npos);
        }
    }
    EXPECT_TRUE(saw_post);
    EXPECT_TRUE(saw_poll);

    {
        std::unique_lock lock(received_mutex);
        ASSERT_TRUE(received_cv.wait_for(lock, std::chrono::seconds(3), [&] {
            return !received.empty();
        }));
    }
    ASSERT_FALSE(received.empty());
    EXPECT_EQ(received.front().id, "poll-1");
    EXPECT_EQ(received.front().method, "server/poll");
    EXPECT_EQ(received.front().priority, loom::bridge::MessagePriority::high);
    ASSERT_TRUE(received.front().correlation_id.has_value());
    EXPECT_EQ(*received.front().correlation_id, "corr-1");
    for (const auto& message : received) {
        EXPECT_NE(message.id, "client-1");
    }

    transport.disconnect();
    EXPECT_FALSE(transport.is_connected());
}
