/// @file test_streaming_replay.cpp
/// @brief RFC 0003 streaming payload replay tests: checkpoint smoke checks
///        plus golden snapshot comparison (Phase 2).
///
/// Each test loads a JSONL fixture from tests/fixtures/streaming_sessions/,
/// plays it through loom::testing::StreamReplayHarness, and compares every
/// checkpoint's rendered screen against a golden file under
/// tests/fixtures/streaming_snapshots/.
///
/// To regenerate golden files after an intentional rendering change:
///   LOOM_UPDATE_SNAPSHOTS=1 ctest --preset debug -R StreamingReplay

#include <gtest/gtest.h>

#include <cstdlib>
#include <iterator>

#include <ftxui/component/component.hpp>

#include "streaming_replay.hpp"
#include "invariant_checker.hpp"

import std;
import loom.commands.registry;
import loom.tools.tool;

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
        ("loom_streaming_replay_" + std::string(fixture_name) + "_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

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
    return fs::path(LOOM_TESTS_DIR) / "fixtures" /
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

    // Re-normalize the golden with the current normalize_screen so goldens
    // written by older builds (before temp-path / folder-pill normalization
    // was added) still match.  normalize_screen is idempotent for
    // already-normalized content, and the actual render is normalized by
    // the harness before it reaches us.
    expected = loom::testing::normalize_screen(expected);

    EXPECT_EQ(expected, actual_text)
        << "Snapshot mismatch for: " << fixture_name << "." << checkpoint_name
        << "\nIf this is intentional, regenerate with:\n"
        << "  LOOM_UPDATE_SNAPSHOTS=1 ctest -R StreamingReplay";
}

}  // namespace

TEST(StreamingReplay, TextSimple) {
    auto snapshots = play_fixture("text_simple");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("text_simple", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("text_simple", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ThinkingThenText) {
    auto snapshots = play_fixture("thinking_then_text");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("thinking_then_text", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("thinking_then_text", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ThinkingLongTruncated) {
    auto snapshots = play_fixture("thinking_long_truncated");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("thinking_long_truncated", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("thinking_long_truncated", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ThinkingMultiple) {
    auto snapshots = play_fixture("thinking_multiple");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("thinking_multiple", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("thinking_multiple", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ToolCallBash) {
    auto snapshots = play_fixture("tool_call_bash");
    expect_checkpoints(snapshots, {"after_tool_result", "after_end_query"});
    expect_streaming_snapshot("tool_call_bash", "after_tool_result",
                              snapshots.at("after_tool_result"));
    expect_streaming_snapshot("tool_call_bash", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ToolCallThenText) {
    auto snapshots = play_fixture("tool_call_then_text");
    expect_checkpoints(snapshots, {"after_tool_commit", "after_text_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("tool_call_then_text", "after_tool_commit",
                              snapshots.at("after_tool_commit"));
    expect_streaming_snapshot("tool_call_then_text", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("tool_call_then_text", "after_end_query",
                              snapshots.at("after_end_query"));
}

// ── Bug 1 regression: tool_use + text in the same turn ───────────────
//
// Bug 1: streaming_text_ was not cleared after tool completion when
// tool_use and text blocks arrived in the SAME turn. The streaming text
// row persisted alongside the committed row after __end_query__, producing
// a duplicate. Existing fixtures (tool_call_then_text) have tool_use and
// text in SEPARATE turns, so StreamStart clears streaming state before the
// bug manifests — this fixture is the only one that exercises the
// same-turn path.

TEST(StreamingReplay, ToolUseAndTextSameTurn) {
    auto snapshots = play_fixture("tool_use_and_text_same_turn");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("tool_use_and_text_same_turn", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("tool_use_and_text_same_turn", "after_end_query",
                              snapshots.at("after_end_query"));

    static constexpr std::string_view kText =
        "I'll check the git status for you. The working tree has changes "
        "across the streaming replay test infrastructure.";

    // After __commit__ (query still running): committed + streaming
    // coexistence is expected (up to 2 occurrences — INV-03 accepts this).
    EXPECT_LE(loom::testing::InvariantChecker::count_text_on_screen(
                  snapshots.at("after_commit"), kText), 2)
        << "committed + streaming coexistence should be at most 2";

    // After __end_query__: the streaming row must be cleared — exactly 1
    // occurrence (the committed row only). Without the Bug 1 fix, the
    // streaming text persists and this count is 2.
    EXPECT_EQ(loom::testing::InvariantChecker::count_text_on_screen(
                  snapshots.at("after_end_query"), kText), 1)
        << "streaming text not cleared after tool completion (Bug 1)";
}

TEST(StreamingReplay, MultiTurnWithThinking) {
    auto snapshots = play_fixture("multi_turn_with_thinking");
    expect_checkpoints(snapshots, {"after_tool_commit", "after_text_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("multi_turn_with_thinking", "after_tool_commit",
                              snapshots.at("after_tool_commit"));
    expect_streaming_snapshot("multi_turn_with_thinking", "after_text_commit",
                              snapshots.at("after_text_commit"));
    expect_streaming_snapshot("multi_turn_with_thinking", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, ErrorOverloaded) {
    auto snapshots = play_fixture("error_overloaded");
    expect_checkpoints(snapshots, {"after_error"});
    expect_streaming_snapshot("error_overloaded", "after_error",
                              snapshots.at("after_error"));
}

TEST(StreamingReplay, ErrorRateLimit) {
    auto snapshots = play_fixture("error_rate_limit");
    expect_checkpoints(snapshots, {"after_error"});
    expect_streaming_snapshot("error_rate_limit", "after_error",
                              snapshots.at("after_error"));
}

// ── Phase 5: edge-case fixtures (RFC 0003 §6.3) ─────────────────────

TEST(StreamingReplay, EmptyContent) {
    auto snapshots = play_fixture("empty_content");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("empty_content", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("empty_content", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, VeryLongText) {
    auto snapshots = play_fixture("very_long_text");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("very_long_text", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("very_long_text", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, CjkAndEmoji) {
    auto snapshots = play_fixture("cjk_and_emoji");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("cjk_and_emoji", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("cjk_and_emoji", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, MultipleToolsParallel) {
    auto snapshots = play_fixture("multiple_tools_parallel");
    expect_checkpoints(snapshots, {"after_tools", "after_commit",
                                   "after_end_query"});
    expect_streaming_snapshot("multiple_tools_parallel", "after_tools",
                              snapshots.at("after_tools"));
    expect_streaming_snapshot("multiple_tools_parallel", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("multiple_tools_parallel", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, StreamInterrupted) {
    auto snapshots = play_fixture("stream_interrupted");
    expect_checkpoints(snapshots, {"after_interrupt"});
    expect_streaming_snapshot("stream_interrupted", "after_interrupt",
                              snapshots.at("after_interrupt"));
}

TEST(StreamingReplay, ToolResultWithImage) {
    auto snapshots = play_fixture("tool_result_with_image");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("tool_result_with_image", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("tool_result_with_image", "after_end_query",
                              snapshots.at("after_end_query"));
}

TEST(StreamingReplay, DuplicateEvents) {
    auto snapshots = play_fixture("duplicate_events");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("duplicate_events", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("duplicate_events", "after_end_query",
                              snapshots.at("after_end_query"));
}

// ── Grace-expiry test (RFC 0003 §8.3, INV-07) ────────────────────────
//
// Completed thinking rows are NEVER hidden — the old 30s hide filter was
// removed. Instead the 3s collapse grace (was_recently_streaming in
// messages_list_payload_row.cpp) keeps a just-finished thinking row
// EXPANDED for 3s after it stops being the streaming tail, then collapses
// it to the "∴ Thought for Xs <summary> (ctrl+o to expand)" label.
//
// This test pins the clock, replays the fixture, then advances the clock
// and re-renders — it cannot use play_fixture() because it needs the
// clock seam and a re-render after play() returns.

namespace {

/// "∴ Thinking…" — U+2234 + " Thinking" + U+2026. The EXPANDED label,
/// shown only while a thinking row is the streaming tail (or forced
/// expanded by the grace/transcript path).
constexpr std::string_view kExpandedThinkingLabel =
    "\xE2\x88\xB4 Thinking\xE2\x80\xA6";

/// " (ctrl+o to expand)" — the COLLAPSED-label hint (kCtrlOHint in
/// thinking_message.cppm). It is emitted ONLY by
/// RenderThinkingMessageCollapsed, so its presence marks a collapsed
/// thinking row. The collapsed label reads
/// `∴ Thinking  <summary> (ctrl+o to expand)` (duration=0 for a committed
/// block), so the hint — not the glyph — is the reliable marker.
constexpr std::string_view kCollapsedThinkingHint = " (ctrl+o to expand)";

}  // namespace

TEST(StreamingReplay, ThinkingGraceExpiry) {
    // Pin the clock to a fixed instant so every replay event (including
    // the ContentBlockStop that writes streaming_ended_at) is timestamped
    // at t0. Advancing to t0 + 4s then simulates collapse-grace expiry
    // without sleeping.
    const auto t0 = std::chrono::steady_clock::time_point{
        std::chrono::seconds(1'000'000)};

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_grace_expiry_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Disable chain compression so the grace-period behavior (expanded →
    // collapsed thinking row) is tested without chain compression hiding
    // the thinking row entirely.
    loom::ui::test_seams(app).set_disable_chain_compression_for_testing(true);

    auto steps = loom::testing::load_fixture("thinking_grace_expiry");
    loom::testing::InvariantChecker checker(steps);
    loom::testing::StreamReplayHarness harness(*app, engine);
    auto snapshots = harness.play_with_invariants(steps,
        [&checker](std::string_view screen, std::size_t step_idx) {
            checker.check(screen, step_idx);
        }, 120, 40, t0);

    expect_checkpoints(snapshots, {"before_expiry", "after_end_query"});
    expect_streaming_snapshot("thinking_grace_expiry", "before_expiry",
                              snapshots.at("before_expiry"));
    expect_streaming_snapshot("thinking_grace_expiry", "after_end_query",
                              snapshots.at("after_end_query"));

    // After __end_query__ the idle path (SyncState) projects committed
    // rows only — the in-flight streaming projection is gone. The
    // committed thinking row was the streaming tail while it streamed
    // (mark_streaming recorded t0), so within the 3s collapse grace it
    // stays EXPANDED: the "∴ Thinking…" label is present and the
    // collapsed-label hint is absent. Clear the in-flight preview map so
    // it cannot shadow the committed row.
    loom::ui::test_seams(app).clear_streaming_thinking_for_testing();
    const std::string within_grace = harness.render_now();
    EXPECT_NE(within_grace.find(kExpandedThinkingLabel), std::string::npos)
        << "thinking row should stay expanded within the 3s collapse grace";
    EXPECT_EQ(within_grace.find(kCollapsedThinkingHint), std::string::npos)
        << "collapsed thinking label should be absent within the 3s grace";

    // Advance the clock past the 3s collapse grace and re-render: the row
    // collapses to the "∴ Thought for Xs <summary> (ctrl+o to expand)"
    // label (INV-07).
    loom::ui::clock::set_steady_now_for_testing(
        t0 + std::chrono::seconds(4));
    const std::string after_expiry = harness.render_now();
    EXPECT_NE(after_expiry.find(kCollapsedThinkingHint), std::string::npos)
        << "thinking row should collapse after the 3s grace";
    EXPECT_EQ(after_expiry.find(kExpandedThinkingLabel), std::string::npos)
        << "expanded thinking label should be gone after collapse";

    // Clear the clock override so it cannot leak into later tests.
    loom::ui::clock::set_steady_now_for_testing(std::nullopt);

    // Tear down the app before removing its storage root.
    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

// ── 3s collapse-grace test (RFC 0003 §8.3) ────────────────────────────
//
// The 3s collapse grace (kThinkingCollapseGrace in
// messages_list_payload_row.cpp) keeps a thinking row EXPANDED for 3s after
// it stops being the streaming tail, via the was_recently_streaming() map.
//
// The in-flight streaming projection (streaming_thinking_ in
// app_render_event.cpp) would otherwise shadow it: it re-projects the
// thinking block as an expanded "∴ Thinking…" row while the query is still
// running. This test therefore clears the streaming-thinking map
// (clear_streaming_thinking_for_testing) right after the fixture plays, so
// the committed row takes over and the 3s collapse grace becomes the
// deciding factor. The clock is pinned so the mark_streaming() timestamps
// and the was_recently_streaming() checks are deterministic.
//
// Like ThinkingGraceExpiry, this cannot use play_fixture(): it needs the
// clock seam, a post-play clear, and re-renders after advancing the clock.

TEST(StreamingReplay, ThinkingCollapseGrace) {
    // Pin the clock so every mark_streaming() call during replay records
    // t0, and the post-play was_recently_streaming() checks measure elapsed
    // time against t0.
    const auto t0 = std::chrono::steady_clock::time_point{
        std::chrono::seconds(2'000'000)};

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);

    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_collapse_grace_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));

    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Disable chain compression so the grace-period behavior (expanded →
    // collapsed thinking row) is tested without chain compression hiding
    // the thinking row entirely.
    loom::ui::test_seams(app).set_disable_chain_compression_for_testing(true);

    // thinking_collapse_grace is a thinking-only fixture: while the
    // thinking block streams it is the sole row (row 0, streaming tail), so
    // mark_streaming(0) records t0. After __commit__ + __end_query__ the
    // committed thinking row is also at row 0, letting was_recently_streaming
    // govern it once the in-flight streaming projection is cleared below.
    auto steps = loom::testing::load_fixture("thinking_collapse_grace");
    loom::testing::InvariantChecker checker(steps);
    loom::testing::StreamReplayHarness harness(*app, engine);
    auto snapshots = harness.play_with_invariants(steps,
        [&checker](std::string_view screen, std::size_t step_idx) {
            checker.check(screen, step_idx);
        }, 120, 40, t0);

    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});
    expect_streaming_snapshot("thinking_collapse_grace", "after_commit",
                              snapshots.at("after_commit"));
    expect_streaming_snapshot("thinking_collapse_grace", "after_end_query",
                              snapshots.at("after_end_query"));

    // The in-flight streaming projection would keep re-projecting the
    // thinking block as an expanded row while the query is still running.
    // Clear the streaming-thinking map so the committed thinking row takes
    // over and the 3s collapse grace (was_recently_streaming) governs it
    // instead. The row was marked at t0 while it was the streaming tail,
    // so it stays expanded within the 3s window.
    loom::ui::test_seams(app).clear_streaming_thinking_for_testing();

    // Within the 3s collapse grace (clock still at t0): the committed
    // thinking row is EXPANDED — the "∴ Thinking…" label is present and the
    // collapsed-label hint is absent.
    const std::string within_grace = harness.render_now();
    EXPECT_NE(within_grace.find(kExpandedThinkingLabel), std::string::npos)
        << "thinking row should stay expanded within the 3s collapse grace";
    EXPECT_EQ(within_grace.find(kCollapsedThinkingHint), std::string::npos)
        << "collapsed thinking label should be absent within the 3s grace";

    // Advance the clock past the 3s collapse grace and re-render: the row
    // collapses to the "∴ Thinking  <summary> (ctrl+o to expand)" label.
    loom::ui::clock::set_steady_now_for_testing(
        t0 + std::chrono::seconds(4));
    const std::string after_grace = harness.render_now();
    EXPECT_NE(after_grace.find(kCollapsedThinkingHint), std::string::npos)
        << "thinking row should collapse after the 3s grace";
    EXPECT_EQ(after_grace.find(kExpandedThinkingLabel), std::string::npos)
        << "expanded thinking label should be gone after collapse";

    // Clear the clock override so it cannot leak into later tests.
    loom::ui::clock::set_steady_now_for_testing(std::nullopt);

    // Tear down the app before removing its storage root.
    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

// ── Clock-reset test (RFC 0003 §8.3) ──────────────────────────────────
//
// The harness clears the process-global clock override at the start of
// every play() / play_with_invariants() (streaming_replay.hpp), so a
// simulated time installed by one fixture cannot leak into the next. This
// test installs an override, plays an unrelated fixture through the same
// harness path, and verifies the override was cleared.

TEST(StreamingReplay, ClockResetBetweenFixtures) {
    // A fixed instant far from any real steady_clock reading.
    const auto t0 = std::chrono::steady_clock::time_point{
        std::chrono::seconds(3'000'000)};

    // Simulate a leftover override from a previous fixture.
    loom::ui::clock::set_steady_now_for_testing(t0);
    ASSERT_EQ(loom::ui::clock::steady_now(), t0)
        << "clock override should be active before play()";

    // play_fixture() builds a fresh harness and plays through
    // play_with_invariants(), which clears the override at startup.
    auto snapshots = play_fixture("text_simple");
    expect_checkpoints(snapshots, {"after_commit", "after_end_query"});

    // The override must be gone: steady_now() no longer returns t0.
    EXPECT_NE(loom::ui::clock::steady_now(), t0)
        << "harness must clear the clock override at the start of play()";

    // Clean up (defensive — play() already cleared it).
    loom::ui::clock::set_steady_now_for_testing(std::nullopt);
}
