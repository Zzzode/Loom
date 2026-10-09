/// @file test_file_ops.cpp
/// @brief File read/write, path validation, notebook, and image codec tests.

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

} // namespace


TEST(Tools, FileReadAndWriteHonorAllowedDirectories) {
    auto root = fs::temp_directory_path() / "loom_file_permission_test";
    auto allowed = root / "allowed";
    auto sibling_with_prefix = root / "allowed2";
    auto blocked = root / "blocked";
    fs::remove_all(root);
    fs::create_directories(allowed);
    fs::create_directories(sibling_with_prefix);
    fs::create_directories(blocked);

    const auto allowed_file = allowed / "ok.txt";
    const auto sibling_file = sibling_with_prefix / "prefix.txt";
    const auto blocked_file = blocked / "no.txt";
    {
        std::ofstream out(allowed_file);
        out << "allowed";
    }
    {
        std::ofstream out(sibling_file);
        out << "prefix";
    }
    {
        std::ofstream out(blocked_file);
        out << "blocked";
    }

    loom::tools::FileReadTool read_tool;
    read_tool.set_allowed_directories({allowed.string()});
    EXPECT_TRUE(read_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}"}})",
        loom::tools::agent::json_escape_string(allowed_file.string())))));
    EXPECT_FALSE(read_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}"}})",
        loom::tools::agent::json_escape_string(blocked_file.string())))));
    EXPECT_FALSE(read_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}"}})",
        loom::tools::agent::json_escape_string(sibling_file.string())))));

    loom::tools::FileWriteTool write_tool;
    write_tool.set_allowed_directories({allowed.string()});
    EXPECT_TRUE(write_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}","content":"ok"}})",
        loom::tools::agent::json_escape_string((allowed / "write.txt").string())))));
    EXPECT_FALSE(write_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}","content":"no"}})",
        loom::tools::agent::json_escape_string((blocked / "write.txt").string())))));
    EXPECT_FALSE(write_tool.check_permission(loom::core::ToolInput::from_json(std::format(
        R"({{"file_path":"{}","content":"no"}})",
        loom::tools::agent::json_escape_string((sibling_with_prefix / "write.txt").string())))));

    fs::remove_all(root);
}

TEST(Tools, PathValidationRejectsSymlinkEscapingAllowedDir) {
    // Regression test: a symlink inside an allowed directory that points
    // outside it must be rejected. Previously path_validation used
    // lexically_normal() only, so the symlink was not followed and the path
    // appeared to stay within bounds — a directory-traversal bypass.
    auto root = fs::temp_directory_path() / "loom_symlink_escape_test";
    auto allowed = root / "allowed";
    auto outside = root / "outside";
    fs::remove_all(root);
    fs::create_directories(allowed);
    fs::create_directories(outside);

    const auto escape_link = allowed / "escape";
    std::error_code link_ec;
    fs::create_directory_symlink(outside, escape_link, link_ec);
    if (link_ec) {
        fs::remove_all(root);
        GTEST_SKIP() << "symlinks not supported on this filesystem";
    }

    loom::tools::path_validation::PathPermissionContext ctx{
        .cwd = root,
        .allowed_dirs = {allowed},
    };

    // Writing through the symlink resolves OUTSIDE allowed_dirs -> deny.
    const auto target_via_symlink = escape_link / "exfil.txt";
    auto r = loom::tools::path_validation::validate_path(
        target_via_symlink.string(), root, ctx,
        loom::tools::path_validation::FileOperationType::kCreate);
    EXPECT_FALSE(r.allowed)
        << "symlink escaping allowed_dirs must be rejected";

    // Sanity: a direct path inside allowed is still allowed.
    auto r_ok = loom::tools::path_validation::validate_path(
        (allowed / "inside.txt").string(), root, ctx,
        loom::tools::path_validation::FileOperationType::kCreate);
    EXPECT_TRUE(r_ok.allowed);

    fs::remove_all(root);
}

TEST(Tools, NotebookEditPreservesNotebookJsonStructure) {
    auto root = fs::temp_directory_path() / "loom_notebook_roundtrip_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto notebook_path = root / "sample.ipynb";
    {
        std::ofstream notebook(notebook_path);
        notebook << R"JSON({
  "nbformat": 4,
  "nbformat_minor": 5,
  "metadata": {
    "language_info": {"name": "python"},
    "custom": {"keep": true}
  },
  "cells": [
    {
      "cell_type": "code",
      "id": "abc123",
      "metadata": {"tags": ["keep-me"]},
      "source": ["print('old')\n"],
      "execution_count": 12,
      "outputs": [
        {"output_type": "stream", "name": "stdout", "text": ["old\n"]}
      ]
    },
    {
      "cell_type": "markdown",
      "id": "md1",
      "metadata": {"collapsed": false},
      "source": "unchanged markdown"
    }
  ]
})JSON";
    }

    loom::tools::NotebookEditTool tool;
    auto result = tool.execute(loom::tools::NotebookEditRequest{
        .notebook_path = notebook_path,
        .operation = loom::tools::CellOperation::Update,
        .cell_index = 0,
        .target_index = std::nullopt,
        .cell_type = std::nullopt,
        .source = std::string(R"(print("new value"))"),
    });

    ASSERT_TRUE(result.has_value()) << std::string(loom::tools::format_error(result.error()));
    EXPECT_EQ(result->total_cells, 2u);

    auto parsed = loom::utils::json::parse_file(notebook_path);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto root_json = parsed->root();
    EXPECT_EQ(root_json.get("metadata").get("language_info").get_string("name"), "python");
    EXPECT_TRUE(root_json.get("metadata").get("custom").get("keep").as_bool());

    auto cells = root_json.get("cells");
    ASSERT_TRUE(cells.is_arr());
    ASSERT_EQ(cells.size(), 2u);
    auto first = cells.at(0);
    EXPECT_EQ(first.get_string("cell_type"), "code");
    EXPECT_EQ(first.get_string("id"), "abc123");
    EXPECT_EQ(first.get("metadata").get("tags").at(0).as_str(), "keep-me");
    EXPECT_EQ(first.get_string("source"), R"(print("new value"))");
    EXPECT_TRUE(first.get("execution_count").is_null());
    EXPECT_EQ(first.get("outputs").size(), 0u);

    auto second = cells.at(1);
    EXPECT_EQ(second.get_string("cell_type"), "markdown");
    EXPECT_EQ(second.get_string("id"), "md1");
    EXPECT_FALSE(second.get("metadata").get("collapsed").as_bool());
    EXPECT_EQ(second.get_string("source"), "unchanged markdown");

    fs::remove_all(root);
}

TEST(Tools, NotebookRuntimeAdapterAcceptsTypeScriptInputShape) {
    auto root = fs::temp_directory_path() / "loom_notebook_runtime_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto notebook_path = root / "runtime.ipynb";
    {
        std::ofstream notebook(notebook_path);
        notebook << R"JSON({
  "nbformat": 4,
  "nbformat_minor": 5,
  "metadata": {"language_info": {"name": "python"}},
  "cells": [
    {"cell_type": "markdown", "id": "first", "metadata": {}, "source": "before"}
  ]
})JSON";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto input = std::format(R"JSONFMT({{
      "notebook_path": "{}",
      "cell_id": "first",
      "edit_mode": "insert",
      "cell_type": "code",
      "new_source": "print(\"inserted\")"
    }})JSONFMT", notebook_path.string());
    auto result = registry.execute("notebook_edit", loom::core::ToolInput::from_json(input));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    EXPECT_NE(result->content.front().text.find("Inserted cell at index 1"), std::string::npos);

    auto parsed = loom::utils::json::parse_file(notebook_path);
    ASSERT_TRUE(parsed.has_value()) << parsed.error().message();
    auto cells = parsed->root().get("cells");
    ASSERT_EQ(cells.size(), 2u);
    EXPECT_EQ(cells.at(0).get_string("source"), "before");
    EXPECT_EQ(cells.at(1).get_string("cell_type"), "code");
    EXPECT_TRUE(cells.at(1).has("id"));
    EXPECT_EQ(cells.at(1).get_string("source"), R"(print("inserted"))");
    EXPECT_TRUE(cells.at(1).get("execution_count").is_null());
    EXPECT_EQ(cells.at(1).get("outputs").size(), 0u);

    fs::remove_all(root);
}

TEST(Tools, FileReadFormatsNotebookCellsForToolResult) {
    auto root = fs::temp_directory_path() / "loom_notebook_read_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto notebook_path = root / "read.ipynb";
    {
        std::ofstream notebook(notebook_path);
        notebook << R"JSON({
  "nbformat": 4,
  "nbformat_minor": 5,
  "metadata": {"language_info": {"name": "r"}},
  "cells": [
    {
      "cell_type": "code",
      "id": "code-cell",
      "metadata": {},
      "source": ["print(1)\n"],
      "execution_count": 1,
      "outputs": [
        {"output_type": "stream", "name": "stdout", "text": ["1\n"]}
      ]
    },
    {
      "cell_type": "markdown",
      "metadata": {},
      "source": "markdown text"
    }
  ]
})JSON";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto input = std::format(R"({{"file_path":"{}"}})", notebook_path.string());
    auto result = registry.execute("Read", loom::core::ToolInput::from_json(input));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    ASSERT_FALSE(result->content.empty());
    const auto& text = result->content.front().text;
    EXPECT_NE(text.find("Notebook:"), std::string::npos);
    EXPECT_NE(text.find(R"CHECK(<cell id="code-cell"><language>r</language>print(1))CHECK"), std::string::npos);
    EXPECT_NE(text.find("\n1\n"), std::string::npos);
    EXPECT_NE(text.find(R"(<cell id="cell-1"><cell_type>markdown</cell_type>markdown text</cell id="cell-1">)"), std::string::npos);
    EXPECT_EQ(text.find(R"("nbformat")"), std::string::npos);

    fs::remove_all(root);
}

TEST(Tools, FileReadReturnsImageContentBlock) {
    FileToolServicesGuard services_guard;
    auto root = fs::temp_directory_path() / "loom_image_read_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto image_path = root / "pixel.png";
    {
        const unsigned char png_header[] = {
            0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
            0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R',
            0x00, 0x00, 0x00, 0x01,
            0x00, 0x00, 0x00, 0x02,
        };
        std::ofstream image(image_path, std::ios::binary);
        image.write(reinterpret_cast<const char*>(png_header), sizeof(png_header));
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto input = std::format(R"({{"file_path":"{}"}})", image_path.string());
    auto result = registry.execute("Read", loom::core::ToolInput::from_json(input));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    ASSERT_EQ(result->content.size(), 2u);
    EXPECT_NE(result->content[0].text.find("Image file read:"), std::string::npos);
    EXPECT_EQ(result->content[1].format, std::optional<std::string>{"image"});
    EXPECT_EQ(result->content[1].media_type, std::optional<std::string>{"image/png"});
    ASSERT_TRUE(result->content[1].data.has_value());
    EXPECT_TRUE(result->content[1].data->starts_with("iVBOR"));

    fs::remove_all(root);
}

TEST(Tools, FileReadReturnsPdfDocumentBlock) {
    FileToolServicesGuard services_guard;
    auto root = fs::temp_directory_path() / "loom_pdf_read_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto pdf_path = root / "doc.pdf";
    {
        std::ofstream pdf(pdf_path, std::ios::binary);
        pdf << "%PDF-1.4\n%%EOF\n";
    }

    loom::core::ToolRegistry registry;
    loom::tools::register_runtime_tools(registry, loom::tools::RuntimeToolOptions{.permission_check = test_allow_all_check()});

    auto input = std::format(R"({{"file_path":"{}"}})", pdf_path.string());
    auto result = registry.execute("Read", loom::core::ToolInput::from_json(input));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->is_error);
    ASSERT_EQ(result->content.size(), 2u);
    EXPECT_NE(result->content[0].text.find("PDF file read:"), std::string::npos);
    EXPECT_EQ(result->content[1].format, std::optional<std::string>{"document"});
    EXPECT_EQ(result->content[1].media_type, std::optional<std::string>{"application/pdf"});
    ASSERT_TRUE(result->content[1].data.has_value());
    EXPECT_TRUE(result->content[1].data->starts_with("JVBER"));

    fs::remove_all(root);
}

// RFC-0001 B11: the installed services-backed codec parses a real PNG IHDR
// through the port (width/height/size plus the services' mime and summary).
TEST(Tools, ImageCodecGetInfoMapsPngHeaderToMetadata) {
    FileToolServicesGuard services_guard;
    auto root = fs::temp_directory_path() / "loom_image_codec_info_test";
    fs::remove_all(root);
    fs::create_directories(root);
    auto image_path = root / "header.png";
    {
        const unsigned char png_header[] = {
            0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
            0x00, 0x00, 0x00, 0x0d, 'I', 'H', 'D', 'R',
            0x00, 0x00, 0x00, 0x03,
            0x00, 0x00, 0x00, 0x02,
        };
        std::ofstream image(image_path, std::ios::binary);
        image.write(reinterpret_cast<const char*>(png_header), sizeof(png_header));
    }

    const auto& codec = loom::tools::image_codec::codec();
    auto info = codec.get_info(image_path);

    ASSERT_TRUE(info.has_value()) << info.error();
    EXPECT_EQ(info->width, 3u);
    EXPECT_EQ(info->height, 2u);
    EXPECT_EQ(info->size_bytes, std::size_t{24});
    EXPECT_EQ(info->mime, "image/png");
    EXPECT_EQ(info->summary, "3x2 png (24 bytes)");

    fs::remove_all(root);
}

// RFC-0001 B11: base64 encode/decode round-trip through the installed codec.
TEST(Tools, ImageCodecBase64RoundTripsBytes) {
    FileToolServicesGuard services_guard;
    const std::vector<std::uint8_t> bytes{1, 2, 3, 4};

    const auto& codec = loom::tools::image_codec::codec();
    const auto encoded = codec.to_base64(std::span<const std::uint8_t>(bytes));
    EXPECT_EQ(encoded, "AQIDBA==");

    auto decoded = codec.from_base64(encoded);
    ASSERT_TRUE(decoded.has_value()) << decoded.error();
    EXPECT_EQ(*decoded, bytes);
}
