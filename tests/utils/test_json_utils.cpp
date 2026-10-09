/// @file test_json_utils.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the JSON utilities (loom.serdes.json parse/serialize, control-message key compat) suites.

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.text.string;
import loom.text.string_utils;
import loom.containers.array_utils;
import loom.serdes.json;
import loom.utils.error;
import loom.containers.circular_buffer;
import loom.model.token_budget;
import loom.config.settings_sources;
import loom.net.http.ssrf_guard;
import loom.plugins.plugin_identifier;
import loom.plugins.plugin_dependency_resolver;
import loom.config.settings_paths;
import loom.config.settings_merge;
import loom.plugins.plugin_marketplace_rules;
import loom.plugins.plugin_versioning;
import loom.plugins.plugin_loader;
import loom.parsing.cli.argument_substitution;
import loom.text.semantic_boolean;
import loom.text.semantic_number;
import loom.ui.chrome.terminal_io;
import loom.commands.review.review_remote;
import loom.security.query_guard;
import loom.agent.agent_id;
import loom.security.auto_mode_denials;
import loom.diagnostics.activity_manager;
import loom.platform.env.env_utils;
import loom.cache.cache_paths;
import loom.platform.binary_check;
import loom.skills.hints;
import loom.scm.git.commit_attribution;
import loom.crypto.hash;
import loom.types.tagged_id;
import loom.ui.messages.message_predicates;
import loom.types.wire.content_array;
import loom.containers.object_group_by;
import loom.process.timeouts;
import loom.parsing.cli.slash_command_parsing;
import loom.containers.set_utils;
import loom.text.words;
import loom.diagnostics.fps_tracker;
import loom.security.privacy_level;
import loom.tools.support.script_tool_enabled;
import loom.prompt.support.prompt_category;
import loom.teams.control_message_compat;
import loom.security.sanitization;
import loom.text.diff_utils;
import loom.process.shell.shell_providers;
import loom.config.settings;
import loom.scm.git.git_diff;
import loom.net.http.proxy_utils;
import loom.net.http.github_utils;
import loom.plugins.marketplace;
import loom.platform.clipboard;
import loom.text.parse_references;
import loom.memdir.memdir;

TEST(ControlMessageCompat, NormalizesRequestIdKeysWithSnakeCasePrecedence) {
    loom::utils::control_message_compat::ControlMessageLike message;
    message.fields["requestId"] = "camel-root";
    message.has_response = true;
    message.response["requestId"] = "camel-response";

    loom::utils::control_message_compat::normalize_control_message_keys(message);
    EXPECT_FALSE(message.fields.contains("requestId"));
    EXPECT_EQ(message.fields.at("request_id"), "camel-root");
    EXPECT_FALSE(message.response.contains("requestId"));
    EXPECT_EQ(message.response.at("request_id"), "camel-response");

    message.fields["requestId"] = "ignored";
    message.fields["request_id"] = "snake-wins";
    loom::utils::control_message_compat::normalize_control_message_keys(message);
    EXPECT_EQ(message.fields.at("request_id"), "snake-wins");
    EXPECT_TRUE(message.fields.contains("requestId"));
}

TEST(JsonUtils, ParseValidJson) {
    auto doc = loom::utils::json::parse(R"({"name":"test","value":42})");
    ASSERT_TRUE(doc.has_value());
    EXPECT_EQ(doc->get_string("name"), "test");
    EXPECT_EQ(doc->get_int("value"), 42);
}

TEST(JsonUtils, AsDoubleReadsIntegerAndRealNumbers) {
    auto doc = loom::utils::json::parse(R"({"whole":1,"fractional":0.5})");
    ASSERT_TRUE(doc.has_value());
    EXPECT_DOUBLE_EQ(doc->root().get("whole").as_double(), 1.0);
    EXPECT_DOUBLE_EQ(doc->root().get("fractional").as_double(), 0.5);
}

TEST(JsonUtils, ParseInvalidJsonReturnsError) {
    auto doc = loom::utils::json::parse("{invalid json}");
    EXPECT_FALSE(doc.has_value());
}

TEST(JsonUtils, SerializeToString) {
    auto obj = loom::utils::json::object();
    obj.set("key", "value");
    obj.set("num", 123);

    auto str = obj.serialize();
    EXPECT_TRUE(str.find("\"key\"") != std::string::npos);
    EXPECT_TRUE(str.find("\"value\"") != std::string::npos);
    EXPECT_TRUE(str.find("123") != std::string::npos);
}

TEST(JsonUtils, ArrayOperations) {
    auto arr = loom::utils::json::array();
    arr.push(1);
    arr.push(2);
    arr.push(3);

    EXPECT_EQ(arr.size(), 3u);
    EXPECT_EQ(arr.get_int(0), 1);
    EXPECT_EQ(arr.get_int(2), 3);
}

TEST(JsonUtils, NestedObjectAccess) {
    auto doc = loom::utils::json::parse(R"({"outer":{"inner":"deep"}})");
    ASSERT_TRUE(doc.has_value());
    auto inner = doc->get_object("outer");
    ASSERT_TRUE(inner.has_value());
    EXPECT_EQ(inner->get_string("inner"), "deep");
}

TEST(JsonUtils, NullAndMissingFields) {
    auto doc = loom::utils::json::parse(R"({"key":null})");
    ASSERT_TRUE(doc.has_value());
    EXPECT_TRUE(doc->is_null("key"));
    EXPECT_FALSE(doc->has("nonexistent"));
}

// ─── loom.serdes.json parser coverage (guards the parse/parse_file/to_string/
// chained-get surface used across services) ──────────────────────────────────
TEST(JsonCCUtils, ParsesPrimitivesAndCollections) {
    auto doc_null = loom::utils::json::parse("null");
    ASSERT_TRUE(doc_null.has_value());
    EXPECT_TRUE(doc_null->root().is_null());

    auto doc_b = loom::utils::json::parse("true");
    ASSERT_TRUE(doc_b.has_value());
    EXPECT_TRUE(doc_b->root().is_bool());
    EXPECT_EQ(doc_b->root().as_bool(), true);

    auto doc_i = loom::utils::json::parse("42");
    ASSERT_TRUE(doc_i.has_value());
    EXPECT_EQ(doc_i->root().as_int(), 42);

    auto doc_s = loom::utils::json::parse("\"hello\"");
    ASSERT_TRUE(doc_s.has_value());
    EXPECT_EQ(doc_s->root().as_str(), "hello");

    auto doc_arr = loom::utils::json::parse("[1, 2, 3]");
    ASSERT_TRUE(doc_arr.has_value());
    EXPECT_TRUE(doc_arr->root().is_arr());
    EXPECT_EQ(doc_arr->root().size(), 3u);

    auto doc_obj = loom::utils::json::parse(R"({"a": 1, "b": "x"})");
    ASSERT_TRUE(doc_obj.has_value());
    auto root = doc_obj->root();
    EXPECT_TRUE(root.is_obj());
    EXPECT_EQ(root.get("a").as_int(), 1);
    EXPECT_EQ(root.get("b").as_str(), "x");
}

TEST(JsonCCUtils, ParsesNestedStructures) {
    auto r = loom::utils::json::parse(R"({"list": [1, {"k": true}], "n": null})");
    ASSERT_TRUE(r.has_value());
    auto root = r->root();
    EXPECT_TRUE(root.is_obj());
    auto list = root.get("list");
    EXPECT_TRUE(list.is_arr());
    EXPECT_EQ(list.at(0).as_int(), 1);
    EXPECT_TRUE(list.at(1).get("k").is_bool());
    EXPECT_TRUE(root.get("n").is_null());
}

TEST(JsonCCUtils, ParseFileReturnsErrorOnMissingFile) {
    auto r = loom::utils::json::parse_file("/nonexistent/cc-json-read-test.json");
    EXPECT_FALSE(r.has_value());
}

TEST(JsonCCUtils, ParseFileReturnsErrorOnInvalidJson) {
    auto tmp = std::filesystem::temp_directory_path() / "cc-json-read-test.json";
    { std::ofstream f(tmp); f << "{invalid}"; }
    auto r = loom::utils::json::parse_file(tmp);
    EXPECT_FALSE(r.has_value());
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

TEST(JsonCCUtils, ParseFileRoundTripsValidJson) {
    auto tmp = std::filesystem::temp_directory_path() / "cc-json-read-rt.json";
    { std::ofstream f(tmp); f << R"({"key": "value", "n": 5})"; }
    auto r = loom::utils::json::parse_file(tmp);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->root().get("key").as_str(), "value");
    EXPECT_EQ(r->root().get("n").as_int(), 5);
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

TEST(JsonCCUtils, RoundTripsThroughJsonToString) {
    auto r = loom::utils::json::parse(R"({"key": "value", "n": 5})");
    ASSERT_TRUE(r.has_value());
    std::string s = loom::utils::json::to_string(*r);
    auto r2 = loom::utils::json::parse(s);
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2->root().get("key").as_str(), "value");
    EXPECT_EQ(r2->root().get("n").as_int(), 5);
}

TEST(JsonCCUtils, JsonGetPathAccess) {
    auto r = loom::utils::json::parse(R"({"a": {"b": "deep"}})");
    ASSERT_TRUE(r.has_value());
    auto root = r->root();
    EXPECT_EQ(root.get("a").get("b").as_str(), "deep");
    EXPECT_FALSE(root.get("a").get("missing").valid());
    EXPECT_FALSE(root.get("nonexistent").valid());
}
