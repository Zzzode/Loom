/// @file test_lsp.cpp
/// @brief Service layer tests split from test_services.cpp.

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <CommonCrypto/CommonDigest.h>
#else
#include <openssl/sha.h>
#endif

#include <gtest/gtest.h>
#include <httplib.h>
#include <limits>



import std;
import loom.cli.ccr_client;
import loom.cli.sse_transport;
import loom.bridge.core;
import loom.config.config;
import loom.constants.paths;
import loom.services.api.client;
import loom.services.api.errors;
import loom.services.api.session_ingress;
import loom.services.api.streaming;
import loom.services.compact.api_microcompact;
import loom.services.lsp.LSPServerManager;
import loom.services.lsp.client;
import loom.services.mcp.client;
import loom.services.mcp.auth;
import loom.services.mcp.channel_permissions;
import loom.services.mcp.config;
import loom.services.mcp.connection_manager;
import loom.services.mcp.elicitation_handler;
import loom.services.mcp.headers_helper;
import loom.services.mcp.vscode_sdk_mcp;
import loom.services.memory.sessionMemory;
import loom.services.extract_memories;
import loom.services.mcp.types;
import loom.services.mcp.xaa;
import loom.services.mcp.xaa_idp_login;
import loom.services.mcp.oauth_port;
import loom.services.rate_limit;
import loom.services.token_estimation;
import loom.services.prompt_suggestion;
import loom.server.server_routes;
import loom.server.server_main;
import loom.session.storage;
import loom.session.history;
import loom.commands.config;
import loom.commands.mcp.core_settings_loader;
import loom.orchestration.tools.mcp;
import loom.query.query_engine;
import loom.memdir.paths;
import loom.tools.agent_runtime;
import loom.tools.team;
import loom.tools.tool;
import loom.types.types;
import loom.utils.error;
import loom.services.ide_integration;
import loom.serdes.json;
import loom.teams.team_helpers;
import loom.fs.atomic_replace;
import loom.daemon.worker_registry;
import loom.server.types;

namespace fs = std::filesystem;

namespace {

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

loom::services::lsp::LspClient make_lsp_for_parsing() {
    return loom::services::lsp::LspClient(loom::services::lsp::LspClient::Config{});
}

} // namespace

TEST(LspConfig, LoadsPluginLspServersFromManifestAndRoutesExtension) {
    auto root = fs::weakly_canonical(fs::temp_directory_path()) / "loom_plugin_lsp_test";
    fs::remove_all(root);
    fs::create_directories(root / ".loom");
    EnvironmentGuard home_guard("HOME", root.string());
    EnvironmentGuard plugin_cache_guard(
        "LOOM_PLUGIN_CACHE_DIR",
        (root / ".loom" / "plugins").string()
    );

    const auto plugin_root = root / ".loom" / "plugins" / "lsp-fixture";
    fs::create_directories(plugin_root / "workspace");
    const auto log_path = root / "lsp-log.jsonl";
    const auto server_path = plugin_root / "server.js";
    {
        std::ofstream server(server_path);
        server << R"JS(
const fs = require('node:fs');
const logPath = process.env.PLUGIN_LSP_LOG;
let buffer = Buffer.alloc(0);

function send(message) {
  const body = JSON.stringify(message);
  process.stdout.write(`Content-Length: ${Buffer.byteLength(body)}\r\n\r\n${body}`);
}

function handle(message) {
  if (logPath) {
    fs.appendFileSync(logPath, `${JSON.stringify(message)}\n`);
  }
  if (message.method === 'initialize') {
    send({
      jsonrpc: '2.0',
      id: message.id,
      result: { capabilities: { textDocumentSync: 1 } },
    });
  }
  if (message.method === 'exit') {
    process.exit(0);
  }
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
        std::ofstream settings(root / ".loom" / "settings.json");
        settings << R"JSON({
  "pluginConfigs": {
    "lsp-fixture": {
      "options": {
        "mode": "configured"
      }
    }
  }
})JSON";
    }
    {
        std::ofstream defaults(plugin_root / ".lsp.json");
        defaults << R"JSON({
  "fixture": {
    "command": "missing-lsp-command",
    "extensionToLanguage": {".foo": "foo-default"}
  }
})JSON";
    }
    {
        std::ofstream manifest(plugin_root / "plugin.json");
        manifest << R"JSON({
  "name": "lsp-fixture",
  "version": "1.0.0",
  "entry_point": "plugin.js",
  "lspServers": {
    "fixture": {
      "command": "node",
      "args": [
        "${LOOM_PLUGIN_ROOT}/server.js",
        "${user_config.mode}",
        "${PLUGIN_LSP_MISSING:-fallback}"
      ],
      "extensionToLanguage": {".foo": "foo-plugin"},
      "env": {
        "PLUGIN_LSP_MODE": "${user_config.mode}",
        "PLUGIN_LSP_ROOT": "${LOOM_PLUGIN_ROOT}",
        "PLUGIN_LSP_LOG": ")JSON" << log_path.string() << R"JSON("
      },
      "workspaceFolder": "${LOOM_PLUGIN_ROOT}/workspace",
      "initializationOptions": {"mode": "configured", "feature": true}
    }
  }
})JSON";
    }

    {
        CurrentPathGuard cwd(root);
        auto servers = loom::services::lsp::discover_plugin_lsp_servers();
        auto it = std::ranges::find_if(servers, [](const auto& server) {
            return server.name == "plugin:lsp-fixture:fixture";
        });
        ASSERT_NE(it, servers.end());
        EXPECT_EQ(it->config.command, "node");
        ASSERT_EQ(it->config.args.size(), 3u);
        EXPECT_EQ(it->config.args[0], server_path.string());
        EXPECT_EQ(it->config.args[1], "configured");
        EXPECT_EQ(it->config.args[2], "fallback");
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_MODE"), "configured");
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_ROOT"), plugin_root.string());
        EXPECT_EQ(it->config.env.at("PLUGIN_LSP_LOG"), log_path.string());
        EXPECT_EQ(it->config.env.at("LOOM_PLUGIN_ROOT"), plugin_root.string());
        EXPECT_EQ(
            it->config.env.at("LOOM_PLUGIN_DATA"),
            (root / ".loom" / "plugins" / "data" / "lsp-fixture").string()
        );
        ASSERT_TRUE(it->config.workspace_folder.has_value());
        EXPECT_EQ(*it->config.workspace_folder, (plugin_root / "workspace").string());
        EXPECT_NE(it->config.initialization_options_json.find("\"mode\":\"configured\""), std::string::npos);
        EXPECT_NE(it->config.initialization_options_json.find("\"feature\":true"), std::string::npos);
        EXPECT_EQ(it->config.extension_to_language.at("foo"), "foo-plugin");

        auto manager = loom::services::lsp::create_lsp_server_manager();
        auto initialized = manager->initialize();
        ASSERT_TRUE(initialized.has_value()) << initialized.error().message();
        auto* routed = manager->get_server_for_file((root / "sample.foo").string());
        ASSERT_NE(routed, nullptr);
        EXPECT_EQ(routed->name, "plugin:lsp-fixture:fixture");
        EXPECT_EQ(routed->config.extension_to_language.at("foo"), "foo-plugin");

        auto opened = manager->open_file((root / "sample.foo").string(), "let x = 1;");
        ASSERT_TRUE(opened.has_value()) << opened.error().message();

        std::string log;
        for (int attempt = 0; attempt < 50; ++attempt) {
            std::ifstream input(log_path);
            if (input) {
                std::stringstream buffer;
                buffer << input.rdbuf();
                log = buffer.str();
                if (log.find("\"method\":\"textDocument/didOpen\"") != std::string::npos) break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{20});
        }

        const auto initialize_pos = log.find("\"method\":\"initialize\"");
        const auto initialized_pos = log.find("\"method\":\"initialized\"");
        const auto did_open_pos = log.find("\"method\":\"textDocument/didOpen\"");
        EXPECT_NE(initialize_pos, std::string::npos) << log;
        EXPECT_NE(initialized_pos, std::string::npos) << log;
        EXPECT_NE(did_open_pos, std::string::npos) << log;
        if (initialize_pos != std::string::npos &&
            initialized_pos != std::string::npos &&
            did_open_pos != std::string::npos) {
            EXPECT_LT(initialize_pos, initialized_pos);
            EXPECT_LT(initialized_pos, did_open_pos);
        }
        EXPECT_NE(log.find("\"initializationOptions\":{\"mode\":\"configured\",\"feature\":true}"), std::string::npos) << log;
        EXPECT_NE(log.find("\"rootPath\":\"" + (plugin_root / "workspace").string() + "\""), std::string::npos) << log;
        EXPECT_NE(log.find("\"languageId\":\"foo-plugin\""), std::string::npos) << log;

        auto shutdown = manager->shutdown();
        EXPECT_TRUE(shutdown.has_value());
    }

    fs::remove_all(root);
}


// LSP response parsers — exercised against canned JSON fixtures (no server).
// These cover the previously-stubbed parsers that dropped most response data.
// ---------------------------------------------------------------------------


TEST(LspClientParser, ParseHoverExtractsMarkupStringAndRange) {
    auto client = make_lsp_for_parsing();
    auto hover = client.parse_hover(
        R"({"contents":{"kind":"markdown","value":"fn doc"},"range":{"start":{"line":1,"character":2},"end":{"line":1,"character":4}}})");
    ASSERT_TRUE(hover.has_value()) << static_cast<int>(hover.error());
    EXPECT_EQ(std::get<std::string>(hover->contents), "fn doc");
    ASSERT_TRUE(hover->range.has_value());
    EXPECT_EQ(hover->range->start.line, 1);
    EXPECT_EQ(hover->range->end.character, 4);
}

TEST(LspClientParser, ParseLocationsReadsUriAndRange) {
    auto client = make_lsp_for_parsing();
    auto locs = client.parse_locations(
        R"json([{"uri":"file:///a.cpp","range":{"start":{"line":0,"character":3},"end":{"line":0,"character":7}}}])json");
    ASSERT_TRUE(locs.has_value()) << static_cast<int>(locs.error());
    ASSERT_EQ(locs->size(), 1u);
    EXPECT_EQ((*locs)[0].uri, "file:///a.cpp");
    EXPECT_EQ((*locs)[0].range.start.character, 3);
    EXPECT_EQ((*locs)[0].range.end.character, 7);
}

TEST(LspClientParser, ParseDocumentSymbolsHandlesHierarchy) {
    auto client = make_lsp_for_parsing();
    auto syms = client.parse_document_symbols(
        R"([{"name":"main","kind":12,"range":{"start":{"line":0,"character":0},"end":{"line":2,"character":0}},"selectionRange":{"start":{"line":0,"character":0},"end":{"line":0,"character":4}},"children":[{"name":"x","kind":13,"range":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}},"selectionRange":{"start":{"line":1,"character":0},"end":{"line":1,"character":1}}}]}])");
    ASSERT_TRUE(syms.has_value());
    ASSERT_EQ(syms->size(), 1u);
    EXPECT_EQ((*syms)[0].name, "main");
    ASSERT_TRUE((*syms)[0].children.has_value());
    EXPECT_EQ((*syms)[0].children->size(), 1u);
    EXPECT_EQ((*syms)[0].children->front().name, "x");
}

TEST(LspClientParser, ParseCodeActionsReadsTitleKindPreferred) {
    auto client = make_lsp_for_parsing();
    auto actions = client.parse_code_actions(
        R"([{"title":"Fix me","kind":"quickfix","isPreferred":true}])");
    ASSERT_TRUE(actions.has_value());
    ASSERT_EQ(actions->size(), 1u);
    EXPECT_EQ((*actions)[0].title, "Fix me");
    ASSERT_TRUE((*actions)[0].kind.has_value());
    EXPECT_EQ((*actions)[0].kind.value(), "quickfix");
    EXPECT_TRUE((*actions)[0].is_preferred.value_or(false));
}

TEST(LspClientParser, ParseTextEditsReadsRangeAndNewText) {
    auto client = make_lsp_for_parsing();
    auto edits = client.parse_text_edits(
        R"([{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":5}},"newText":"hello"}])");
    ASSERT_TRUE(edits.has_value());
    ASSERT_EQ(edits->size(), 1u);
    EXPECT_EQ((*edits)[0].new_text, "hello");
    EXPECT_EQ((*edits)[0].range.end.character, 5);
}

TEST(LspClientParser, ParseCompletionListEnrichesItems) {
    auto client = make_lsp_for_parsing();
    auto list = client.parse_completion_list(
        R"json({"isIncomplete":true,"items":[{"label":"foo","kind":3,"detail":"(int)","documentation":"doc","insertText":"foo()","sortText":"a"}]})json");
    ASSERT_TRUE(list.has_value());
    EXPECT_TRUE(list->is_incomplete);
    ASSERT_EQ(list->items.size(), 1u);
    const auto& item = list->items.front();
    EXPECT_EQ(item.label, "foo");
    ASSERT_TRUE(item.detail.has_value());  EXPECT_EQ(*item.detail, "(int)");
    ASSERT_TRUE(item.documentation.has_value()); EXPECT_EQ(*item.documentation, "doc");
    ASSERT_TRUE(item.insert_text.has_value());  EXPECT_EQ(*item.insert_text, "foo()");
}

TEST(LspClientParser, ParseInitializeResultPopulatesCapabilities) {
    auto client = make_lsp_for_parsing();
    auto init = client.parse_initialize_result(
        R"({"capabilities":{"hoverProvider":true,"definitionProvider":true,"completionProvider":{"triggerCharacters":["."]}},"serverInfo":{"name":"clangd","version":"17.0"}})");
    ASSERT_TRUE(init.has_value());
    EXPECT_TRUE(init->capabilities.hover_provider.value_or(false));
    EXPECT_TRUE(init->capabilities.definition_provider.value_or(false));
    ASSERT_TRUE(init->capabilities.completion_provider.has_value());
    ASSERT_TRUE(init->server_info.has_value());
    EXPECT_NE(init->server_info->find("clangd"), std::string::npos);
}

// End of file
