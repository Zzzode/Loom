/// @file test_streaming_migration.cpp
/// @brief RFC 0003 Phase 4: App-side E2E tests migrated to fixture-driven
///        streaming replay.
///
/// Each test loads a JSONL fixture from tests/fixtures/streaming_sessions/,
/// plays it through loom::testing::StreamReplayHarness, and asserts on the
/// rendered screen via golden snapshots plus targeted substring checks that
/// preserve the original test's regression intent.
///
/// These tests replace the HTTP-mock-server E2E tests that exercised the
/// same App-side rendering paths but were slow (~500ms each) and brittle.
/// Wire-level and engine-loop tests remain on HTTP mock servers (RFC 0003
/// §5.4, N4).
///
/// To regenerate golden files after an intentional rendering change:
///   LOOM_UPDATE_SNAPSHOTS=1 ctest --preset debug -R StreamingMigration

#include <gtest/gtest.h>

#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

#include <ftxui/component/component.hpp>

#include "streaming_replay.hpp"
#include "invariant_checker.hpp"
#include "screen_normalize.hpp"

import std;
import loom.commands.registry;
import loom.tools.tool;
import loom.session.app_storage;

namespace {

namespace fs = std::filesystem;

/// Play a fixture through a fresh AppAdapter + QueryEngine and return the
/// checkpoint snapshots. All objects are local and destroyed before return.
[[nodiscard]] std::map<std::string, std::string> play_fixture(
    std::string_view fixture_name) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_streaming_migration_" + std::string(fixture_name) + "_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    loom::utils::SessionStorage storage(storage_root);

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, &storage, [] {});

    // StreamingMigration tests verify individual row rendering (tool result
    // cards, ⎿ connectors, raw result formats) — bypass chain compression
    // so those rows remain visible.
    loom::ui::test_seams(app.get())
        .set_disable_chain_compression_for_testing(true);

    auto steps = loom::testing::load_fixture(fixture_name);
    loom::testing::InvariantChecker checker(steps);
    loom::testing::StreamReplayHarness harness(*app, engine);
    auto snapshots = harness.play_with_invariants(steps,
        [&checker](std::string_view screen, std::size_t step_idx) {
            checker.check(screen, step_idx);
        });

    // Tear down the app before removing its storage root so background
    // flush threads cannot repopulate the directory mid-remove_all.
    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);

    return snapshots;
}

/// Assert that a snapshot map contains every expected checkpoint and that
/// each snapshot is non-empty.
void expect_checkpoints(
    const std::map<std::string, std::string>& snapshots,
    std::initializer_list<std::string_view> expected) {
    for (const auto name : expected) {
        auto it = snapshots.find(std::string(name));
        ASSERT_NE(it, snapshots.end())
            << "missing checkpoint '" << name << "'";
        EXPECT_FALSE(it->second.empty())
            << "checkpoint '" << name << "' produced an empty snapshot";
    }
}

// ── Golden file path ────────────────────────────────────────────────

[[nodiscard]] fs::path streaming_golden_path(std::string_view fixture_name,
                                             std::string_view checkpoint_name) {
    return fs::path(__FILE__).parent_path() / "fixtures" /
           "streaming_snapshots" /
           (std::string(fixture_name) + "." +
            std::string(checkpoint_name) + ".txt");
}

// ── Snapshot comparison ─────────────────────────────────────────────

/// Compare a checkpoint's rendered screen text against its golden file.
///
/// With LOOM_UPDATE_SNAPSHOTS=1 the golden file is (re)written and the
/// check passes; otherwise the screen text must match byte-for-byte.
void expect_streaming_snapshot(std::string_view fixture_name,
                               std::string_view checkpoint_name,
                               std::string_view actual_text) {
    fs::path path = streaming_golden_path(fixture_name, checkpoint_name);

    // Update mode: write golden file and pass
    if (std::getenv("LOOM_UPDATE_SNAPSHOTS") != nullptr) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path);
        out << actual_text << '\n';
        SUCCEED() << "Updated snapshot: " << fixture_name << "."
                  << checkpoint_name;
        return;
    }

    // Golden file must exist
    if (!fs::exists(path)) {
        FAIL() << "Golden file not found: " << path.filename().string()
               << "\nRun with LOOM_UPDATE_SNAPSHOTS=1 to generate it.";
    }

    // Read expected
    std::ifstream in(path);
    std::string expected((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    // Writer adds a trailing newline; strip it for comparison
    if (!expected.empty() && expected.back() == '\n')
        expected.pop_back();

    EXPECT_EQ(expected, actual_text)
        << "Snapshot mismatch for: " << fixture_name << "." << checkpoint_name
        << "\nIf this is intentional, regenerate with:\n"
        << "  LOOM_UPDATE_SNAPSHOTS=1 ctest -R StreamingMigration";
}

/// Assert that `needle` appears in `haystack`.
void expect_contains(std::string_view haystack, std::string_view needle,
                     std::string_view context) {
    EXPECT_NE(haystack.find(needle), std::string_view::npos)
        << "Expected to find '" << needle << "' in screen (" << context
        << ").";
}

/// Assert that `needle` does NOT appear in `haystack`.
void expect_not_contains(std::string_view haystack, std::string_view needle,
                         std::string_view context) {
    EXPECT_EQ(haystack.find(needle), std::string_view::npos)
        << "Did NOT expect to find '" << needle << "' in screen (" << context
        << ").";
}

// ============================================================
// Migrated from test_ui_e2e.cpp
// ============================================================

/// Migrated from E2E_Gate.StatuslineVisibleAfterSubmit.
///
/// Original intent: after a query completes, the statusline (bottom bar
/// with context usage / model / branch) must still be visible. Regression
/// guard for "statusline vanished after MCP connect".
TEST(StreamingMigration, StatuslineVisibleAfterSubmit) {
    auto snapshots = play_fixture("e2e_statusline");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("e2e_statusline", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("e2e_statusline", "after_end_query",
                              snapshots.at("after_end_query"));

    // The statusline footer shows context usage "0K/200.0K" (0 tokens used,
    // 200k window). This is stable across runs and proves the bottom bar is
    // present after the query completes.
    expect_contains(snapshots.at("after_end_query"), "0K/200.0K",
                    "statusline context usage");
}

/// Migrated from E2E_Gate.McpToolUseBlockRendersWithNameAndInput.
///
/// Original intent: a tool_use block renders with the humanized tool name
/// ("analyze_image" → "Analyze Image") and the assistant's text reply is
/// visible after the tool result round-trip.
TEST(StreamingMigration, McpToolUseBlockRendersWithNameAndInput) {
    auto snapshots = play_fixture("e2e_tool_use_render");
    expect_checkpoints(snapshots, {"after_tool_commit", "after_text_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("e2e_tool_use_render", "after_tool_commit",
                              snapshots.at("after_tool_commit"));
    expect_streaming_snapshot("e2e_tool_use_render", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("e2e_tool_use_render", "after_end_query",
                              snapshots.at("after_end_query"));

    const auto& screen = snapshots.at("after_end_query");
    expect_contains(screen, "Analyze Image", "humanized tool name");
    expect_contains(screen, "screenshot", "assistant text reply");
}

/// Migrated from E2E_Gate.McpToolResultRendersAsSeparateCard.
///
/// Original intent: the tool result renders as a SEPARATE card (with the
/// ⎿ connector), not swallowed into the tool_use card's Output section.
/// Regression guard for role="tool" messages being routed to
/// AssistantToolUse rendering instead of UserToolResult.
TEST(StreamingMigration, McpToolResultRendersAsSeparateCard) {
    auto snapshots = play_fixture("e2e_tool_result_separate_card");
    expect_checkpoints(snapshots, {"after_tool_result", "after_text_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("e2e_tool_result_separate_card",
                              "after_tool_result",
                              snapshots.at("after_tool_result"));
    expect_streaming_snapshot("e2e_tool_result_separate_card",
                              "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("e2e_tool_result_separate_card",
                              "after_end_query",
                              snapshots.at("after_end_query"));

    const auto& screen = snapshots.at("after_end_query");
    expect_contains(screen, "Analyze Image", "tool_use card name");
    expect_contains(screen, "solid black square", "tool result content");

    // The ⎿ connector (U+23BF) marks the separate UserToolResult card.
    const std::string connector = "\xe2\x8e\xbf";
    const auto conn_pos = screen.find(connector);
    EXPECT_NE(conn_pos, std::string_view::npos)
        << "No ⎿ connector found — tool result is not a separate card.";
    if (conn_pos != std::string_view::npos) {
        // Result content must appear after the connector.
        EXPECT_NE(screen.find("solid black square", conn_pos),
                  std::string_view::npos)
            << "Result content not found after ⎿ connector.";
    }

    expect_contains(screen, "screenshot", "assistant text reply");
}

/// Migrated from E2E_Gate.McpResultSummaryFormatShownRaw.
///
/// Original intent: the MCP result format ("analyze_image_result_summary:
/// [{"text": ...}]") is shown raw, not unwrapped. TS parity — TS does NOT
/// unwrap _result_summary or [{"text":...}] arrays.
TEST(StreamingMigration, McpResultSummaryFormatShownRaw) {
    auto snapshots = play_fixture("e2e_result_summary_raw");
    expect_checkpoints(snapshots, {"after_tool_result", "after_text_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("e2e_result_summary_raw", "after_tool_result",
                              snapshots.at("after_tool_result"));
    expect_streaming_snapshot("e2e_result_summary_raw", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("e2e_result_summary_raw", "after_end_query",
                              snapshots.at("after_end_query"));

    const auto& screen = snapshots.at("after_end_query");
    expect_contains(screen, "result_summary", "raw result_summary prefix");
    expect_contains(screen, "[{\"text\"", "raw JSON array wrapper");
    expect_contains(screen, "dark terminal", "result content text");
}

/// Migrated from E2E_Gate.McpToolResultSuppressedInToolUseCardDuringStreaming.
///
/// Original intent: after a tool completes, the tool_use card must NOT show
/// the result in its "Output:" section. The result appears only in the
/// separate ⎿ UserToolResult card. Regression guard for result_preview
/// being forwarded after ToolExecutionEnd (exec_done=true).
TEST(StreamingMigration, McpToolResultSuppressedInToolUseCardDuringStreaming) {
    auto snapshots = play_fixture("e2e_no_output_in_tooluse");
    expect_checkpoints(snapshots, {"after_tool_exec", "after_tool_result",
                                   "after_text_commit", "after_end_query"});
    expect_streaming_snapshot("e2e_no_output_in_tooluse", "after_tool_exec",
                              snapshots.at("after_tool_exec"));
    expect_streaming_snapshot("e2e_no_output_in_tooluse", "after_tool_result",
                              snapshots.at("after_tool_result"));
    expect_streaming_snapshot("e2e_no_output_in_tooluse", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("e2e_no_output_in_tooluse", "after_end_query",
                              snapshots.at("after_end_query"));

    const auto& screen = snapshots.at("after_end_query");
    expect_contains(screen, "Analyze Image", "tool_use card name");

    // The ⎿ connector must be visible (separate result row).
    const std::string connector = "\xe2\x8e\xbf";
    EXPECT_NE(screen.find(connector), std::string_view::npos)
        << "No ⎿ connector found — tool result is not a separate card.";

    expect_contains(screen, "completely black square", "result content");
    expect_contains(screen, "_result_summary", "raw result_summary prefix");
    expect_contains(screen, "screenshot", "assistant text reply");

    // CRITICAL: "Output:" must NOT appear within 300 chars after the first
    // "Analyze Image" (the tool_use card header). Before the fix, the
    // streaming tool_use entry forwarded result_preview after
    // ToolExecutionEnd, causing an "Output:" section in the tool_use card.
    const auto first_tool = screen.find("Analyze Image");
    if (first_tool != std::string_view::npos) {
        const auto search_end =
            std::min(first_tool + 300, screen.size());
        const std::string after_tool =
            std::string(screen.substr(first_tool, search_end - first_tool));
        EXPECT_EQ(after_tool.find("Output:"), std::string_view::npos)
            << "'Output:' found inside the tool_use card area — the result "
               "should only be in the separate ⎿ row.";
    }
}

/// Migrated from E2E_Gate.FullConversationGoldenSnapshot.
///
/// Original intent: after a complete interaction (user → tool → result →
/// assistant), every major section is present. Catches any visual
/// regression in the full conversation flow.
TEST(StreamingMigration, FullConversationGoldenSnapshot) {
    auto snapshots = play_fixture("e2e_full_conversation");
    expect_checkpoints(snapshots, {"after_user", "after_tool_result",
                                   "after_text_commit", "after_end_query"});
    expect_streaming_snapshot("e2e_full_conversation", "after_user",
                              snapshots.at("after_user"));
    expect_streaming_snapshot("e2e_full_conversation", "after_tool_result",
                              snapshots.at("after_tool_result"));
    expect_streaming_snapshot("e2e_full_conversation", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("e2e_full_conversation", "after_end_query",
                              snapshots.at("after_end_query"));

    // Structural assertions: every major section must be present.
    // NOTE: the original E2E test computed `missing` but never asserted on
    // it (dead code). The tool name renders humanized ("analyze_image" →
    // "Analyze Image"), so we assert on the rendered form.
    const auto& screen = snapshots.at("after_end_query");
    expect_contains(screen, "Loom", "logo/header");
    expect_contains(screen, "describe this", "user message");
    expect_contains(screen, "Analyze Image", "tool use block (humanized name)");
    expect_contains(screen, "terminal", "assistant reply content");
}

// ============================================================
// Migrated from test_ui_runtime.cpp
// ============================================================

/// Migrated from AppRuntime.StreamingToolUseShowsSpinnerAndLoadingState.
///
/// Original intent: while a tool_use block is streaming, the tool name is
/// visible on screen. After the query completes, the loading spinner is
/// gone.
TEST(StreamingMigration, StreamingToolUseShowsSpinnerAndLoadingState) {
    auto snapshots = play_fixture("e2e_streaming_tool_use");
    expect_checkpoints(snapshots, {"during_streaming", "after_end_query"});
    expect_streaming_snapshot("e2e_streaming_tool_use", "during_streaming",
                              snapshots.at("during_streaming"));
    expect_streaming_snapshot("e2e_streaming_tool_use", "after_end_query",
                              snapshots.at("after_end_query"));

    // During streaming: the tool name must be visible.
    expect_contains(snapshots.at("during_streaming"), "Bash",
                    "streamed tool name");

    // After query end: the chrome spinner line ("<spinner> <verb>…") must
    // be gone — loading is complete.
    expect_not_contains(snapshots.at("after_end_query"),
                        "<spinner> <verb>", "chrome spinner after end_query");
}

/// Migrated from AppRuntime.StreamingThinkingShowsSpinnerAndFinalContent.
///
/// Original intent: while a thinking block is streaming, the thinking
/// content is visible. After the query completes, the final assistant text
/// is visible.
TEST(StreamingMigration, StreamingThinkingShowsSpinnerAndFinalContent) {
    auto snapshots = play_fixture("e2e_streaming_thinking");
    expect_checkpoints(snapshots, {"during_thinking", "after_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("e2e_streaming_thinking", "during_thinking",
                              snapshots.at("during_thinking"));
    expect_streaming_snapshot("e2e_streaming_thinking", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("e2e_streaming_thinking", "after_end_query",
                              snapshots.at("after_end_query"));

    // During streaming: the thinking content must be visible.
    expect_contains(snapshots.at("during_thinking"),
                    "private streaming thinking", "streamed thinking content");

    // After query end: the final assistant text must be visible.
    expect_contains(snapshots.at("after_end_query"),
                    "visible answer after thinking", "final assistant text");
}

}  // namespace
