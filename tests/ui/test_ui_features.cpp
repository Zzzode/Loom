/// @file test_ui_features.cpp
/// @brief UI feature tests: AutocompleteSources, LiveTeamsUi, PasteCoordinator.
/// Split from test_ui_runtime.cpp (SLOC budget).

#include <cstdlib>
#if defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/loop.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.app.paste_coordinator;
import loom.ui.screens.repl_screen;
import loom.ui.screens.task_view_store;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.messages.message_image;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.teams.team_helpers;
import loom.teams.swarm.helpers;
import loom.constants.constants;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.features.teams.live_teammates;
import loom.platform.clipboard_coordinator;  // CopyResult (re-exported)
import loom.diagnostics.log;

namespace {
namespace fs = std::filesystem;
namespace acsrc = loom::ui::autocomplete_sources;
}









TEST(AutocompleteSources, AppendAndReadHistoryRoundTrip) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_hist_roundtrip_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    // Empty file => empty results.
    auto empty = acsrc::collect_history_suggestions("", 50);
    EXPECT_TRUE(empty.empty());

    // Append three prompts (chronological order: oldest first).
    acsrc::append_prompt_history("fix the flaky test", "sess-aaa", "/proj");
    acsrc::append_prompt_history("refactor auth module", "sess-bbb", "/proj");
    acsrc::append_prompt_history("write a unit test", "sess-ccc", "/proj");

    // Empty query => all entries, newest first ("write a unit test" first).
    auto all = acsrc::collect_history_suggestions("", 50);
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0].prompt_text, "write a unit test");
    EXPECT_EQ(all[1].prompt_text, "refactor auth module");
    EXPECT_EQ(all[2].prompt_text, "fix the flaky test");

    // Substring query "test" matches two entries (newest first).
    auto matched = acsrc::collect_history_suggestions("test", 50);
    ASSERT_EQ(matched.size(), 2u);
    EXPECT_EQ(matched[0].prompt_text, "write a unit test");
    EXPECT_EQ(matched[1].prompt_text, "fix the flaky test");

    // Case-insensitive: "AUTH" should match "refactor auth module".
    auto ci = acsrc::collect_history_suggestions("AUTH", 50);
    ASSERT_EQ(ci.size(), 1u);
    EXPECT_EQ(ci[0].prompt_text, "refactor auth module");

    // Cap at max_entries: request 1, get 1 (newest).
    auto capped = acsrc::collect_history_suggestions("", 1);
    ASSERT_EQ(capped.size(), 1u);
    EXPECT_EQ(capped[0].prompt_text, "write a unit test");

    // Dedup by display: append a duplicate of an existing prompt.
    acsrc::append_prompt_history("write a unit test", "sess-ddd", "/proj");
    auto deduped = acsrc::collect_history_suggestions("test", 50);
    ASSERT_EQ(deduped.size(), 2u) << "duplicate display should be deduped";

    fs::remove(hist_path);
}

TEST(AutocompleteSources, BuildHistorySuggestionsFormatting) {
    const auto hist_path = fs::temp_directory_path() /
        ("loom_hist_fmt_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".jsonl");
    ScopedEnvVar env("LOOM_HISTORY_FILE");
    env.set(hist_path.string());

    // Long prompt (>80 chars) should be truncated in display.
    acsrc::append_prompt_history(
        "this is a very long prompt that exceeds eighty characters in display "
        "length and should be truncated with ellipsis", "sess-xyz", "/proj");

    auto sugs = acsrc::build_history_suggestions("", 0, 5, 50);
    ASSERT_EQ(sugs.size(), 1u);
    EXPECT_LE(sugs[0].display_text.size(), 83u)  // 80 + "..."
        << "display should be truncated to ~80 chars";
    EXPECT_EQ(sugs[0].display_text.back(), '.')
        << "truncated display should end with '...'";
    // insert_text is the FULL prompt (not truncated).
    EXPECT_GT(sugs[0].insert_text.size(), sugs[0].display_text.size());
    EXPECT_EQ(sugs[0].replacement_start, 0u);
    EXPECT_EQ(sugs[0].replacement_end, 5u);
    EXPECT_FALSE(sugs[0].submit_on_return);
    EXPECT_EQ(sugs[0].id.substr(0, 8), "history:");

    fs::remove(hist_path);
}

TEST(AutocompleteSources, BuildAgentSuggestionsHasColors) {
    auto agents = acsrc::collect_agent_suggestions("");
    ASSERT_FALSE(agents.empty())
        << "expected at least one built-in agent definition";

    // The default "loom" agent should exist (catch-all).
    auto it = std::find_if(agents.begin(), agents.end(),
        [](const auto& a) { return a.name == "loom"; });
    ASSERT_NE(it, agents.end()) << "built-in 'loom' agent not found";

    // build_agent_suggestions with empty query returns all agents (fuzzy
    // match passes for everything when query is empty).
    auto sugs = acsrc::build_agent_suggestions("", "", 0, 7);
    ASSERT_FALSE(sugs.empty());
    bool found_loom = false;
    for (const auto& s : sugs) {
        EXPECT_FALSE(s.display_text.empty());
        EXPECT_TRUE(s.display_text.starts_with("@"))
            << "display should start with @";
        EXPECT_EQ(s.replacement_start, 0u);
        EXPECT_EQ(s.replacement_end, 7u);
        EXPECT_FALSE(s.submit_on_return);
        if (s.display_text == "@loom") {
            found_loom = true;
            EXPECT_FALSE(s.icon.empty()) << "loom agent should have an icon";
            EXPECT_FALSE(s.id.empty());
        }
    }
    EXPECT_TRUE(found_loom) << "@loom suggestion not found in results";

    // Fuzzy filter: query "xyz" should match nothing (no agent named xyz).
    auto filtered = acsrc::build_agent_suggestions("", "xyz_nonexistent", 0, 3);
    EXPECT_TRUE(filtered.empty());
}

TEST(LiveTeamsUi, StripRendersNameStatusAndTail) {
    namespace repl = loom::ui::repl_screen;
    namespace live = loom::ui::teams::live;

    repl::ReplScreenState s;
    s.chrome_store.model_display_name = "M";
    s.cwd = "/tmp/x";

    live::LiveTeammate a;
    a.agent_id = "a1";
    a.name = "alice";
    a.color = "cyan";
    a.status = "running";
    a.last_output_tail = "BUILD TARGET widgets OK";
    a.pane_id = "%3";

    live::LiveTeammate b;
    b.agent_id = "a2";
    b.name = "bob";
    b.color = "red";
    b.status = "idle";
    b.last_output_tail = "waiting for review";
    b.pane_id = "%4";

    s.task_view_store.live_teammates = {a, b};
    s.task_view_store.teammate_count = 2;

    const auto txt = strip_ansi(
        render_to_plain_text(repl::RenderReplScreen(s), 140, 40));
    EXPECT_NE(txt.find("alice"), std::string::npos);
    EXPECT_NE(txt.find("running"), std::string::npos);
    EXPECT_NE(txt.find("BUILD TARGET widgets OK"), std::string::npos);
    EXPECT_NE(txt.find("bob"), std::string::npos);
    EXPECT_NE(txt.find("idle"), std::string::npos);
    // Footer pill (prompt_input_footer ModeIndicator label "N teams").
    EXPECT_NE(txt.find("2 teams"), std::string::npos);

    repl::ReplScreenState empty;
    empty.cwd = "/tmp/x";
    const auto none = strip_ansi(
        render_to_plain_text(repl::RenderReplScreen(empty), 140, 40));
    EXPECT_EQ(none.find("@alice"), std::string::npos);
    EXPECT_EQ(none.find("teams"), std::string::npos);
}

TEST(LiveTeamsUi, SlashTeamsOpensOverviewModal) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_teams_modal_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    namespace live = loom::ui::teams::live;
    live::LiveTeammate a;
    a.agent_id = "a1";
    a.name = "alice";
    a.color = "cyan";
    a.status = "running";
    a.last_output_tail = "TAIL-MARKER-42";
    a.pane_id = "%3";
    std::vector<live::LiveTeammate> teammates{a};
    test_seams(app).set_live_teammates_for_testing(&teammates);
    ASSERT_EQ(test_seams(app).teams_overview_count_for_testing(), 1);

    test_seams(app).handle_submit_for_testing("/teams");
    ASSERT_TRUE(test_seams(app).teams_overview_open_for_testing());

    const auto txt = strip_ansi(
        render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(txt.find("alice"), std::string::npos);
    EXPECT_NE(txt.find("TAIL-MARKER-42"), std::string::npos);
    EXPECT_NE(txt.find("Esc close"), std::string::npos);

    // Unhandled Escape falls through to DispatchDialogQueueEvents' modal
    // fallback, which pops the stack.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Escape));
    EXPECT_FALSE(test_seams(app).teams_overview_open_for_testing());

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(LiveTeamsUi, TeammatePermissionRequestRoutesThroughToolPermission) {
    namespace sh = loom::utils::swarm_helpers;

    const auto runtime_dir = fs::temp_directory_path() /
        ("loom_teams_perm_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    fs::remove_all(runtime_dir);
    ScopedEnvVar runtime_guard("LOOM_TEAM_RUNTIME_DIR");
    runtime_guard.set(runtime_dir.string());
    ScopedEnvVar team_guard("LOOM_TEAM_NAME");
    team_guard.set("alpha");
    ScopedEnvVar agent_guard("LOOM_AGENT_NAME");
    agent_guard.unset();  // this process is the LEADER, not a pane teammate

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    auto engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
    auto commands = std::make_unique<loom::commands::AppCommandRegistry>();
    const auto storage_root = fs::temp_directory_path() /
        ("loom_teams_perm_storage_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        engine.get(), nullptr, commands.get(), storage_root, [] {});

    sh::SwarmPermissionRequestMessage request;
    request.type = "permission_request";
    request.request_id = sh::PermissionSync::generate_request_id();
    request.agent_id = "worker-a";
    request.tool_name = "Bash";
    request.tool_use_id = "toolu_perm_1";
    request.description = R"({"command":"rm -rf build"})";
    request.input_json = R"({"command":"rm -rf build"})";
    test_seams(app).enqueue_teammate_permission_for_testing(&request, "alpha");

    // The queued request drains on the next Custom event into the existing
    // ToolPermission overlay (Band3), and the event is consumed like the
    // pane-teammate prompt drain.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Custom));
    EXPECT_TRUE(test_seams(app).has_pending_dialog_for_testing());

    // Render surfaces the worker identity + tool name in the dialog.
    const auto txt = strip_ansi(
        render_to_plain_text(app->Render(), 140, 40));
    EXPECT_NE(txt.find("worker-a"), std::string::npos);
    EXPECT_NE(txt.find("Bash"), std::string::npos);

    // Approve ('y'): the stage-A success response is written to the worker's
    // mailbox and the overlay is dismissed.
    EXPECT_TRUE(app->OnEvent(ftxui::Event::Character('y')));
    EXPECT_EQ(test_seams(app).pending_teammate_permission_count_for_testing(), 0u);

    auto worker_inbox = loom::utils::read_inbox(
        "worker-a", std::optional<std::string_view>{"alpha"});
    ASSERT_TRUE(worker_inbox.has_value()) << worker_inbox.error();
    ASSERT_EQ(worker_inbox->size(), 1u);
    const auto response =
        sh::PermissionSync::parse_response((*worker_inbox)[0].text);
    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->subtype, "success");
    EXPECT_EQ(response->request_id, request.request_id);

    app.reset();
    std::error_code ec;
    fs::remove_all(runtime_dir, ec);
    fs::remove_all(storage_root, ec);
}

TEST(PasteCoordinator, SubmissionDrainPreservesNewDraftResults) {
    loom::ui::PasteCoordinator coordinator;
    const int submitted_id = coordinator.allocate_paste_id();
    const int draft_id = coordinator.allocate_paste_id();
    coordinator.SpawnPasteWorker(submitted_id, [] {}, true);
    coordinator.SpawnPasteWorker(draft_id, [] {}, true);

    std::unordered_map<int, loom::core::ImageBlock> images;
    std::unordered_set<int> failures;
    std::unordered_map<int, std::string> texts;
    coordinator.set_pending_drain_ids(std::unordered_set<int>{submitted_id});
    coordinator.drain_pending(images, failures, texts);
    ASSERT_EQ(images.size(), 1u);
    EXPECT_TRUE(images.contains(submitted_id));
    EXPECT_FALSE(images.contains(draft_id));
    EXPECT_TRUE(failures.empty());
    EXPECT_TRUE(texts.empty());
    coordinator.mark_completed(submitted_id);
    EXPECT_TRUE(coordinator.is_in_flight(draft_id));

    // Draining the submitted snapshot again must leave the draft result
    // queued; the normal draft drain receives it exactly once afterward.
    images.clear();
    coordinator.drain_pending(images, failures, texts);
    EXPECT_TRUE(images.empty());
    coordinator.set_pending_drain_ids(std::nullopt);
    coordinator.drain_pending(images, failures, texts);
    ASSERT_EQ(images.size(), 1u);
    EXPECT_TRUE(images.contains(draft_id));
    coordinator.mark_completed(draft_id);
    EXPECT_FALSE(coordinator.has_in_flight());
    images.clear();
    coordinator.drain_pending(images, failures, texts);
    EXPECT_TRUE(images.empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// RFC 0007 D-4: text selection threshold fix (dx²+dy² > 0, not > 9)
// ═══════════════════════════════════════════════════════════════════════════

TEST(TextSelectionThreshold, OneCellDisplacementActivatesSelection) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_threshold_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // FTXUI 5.x Mouse field order:
    //   Button, Motion, shift, meta, control, x, y
    // Motion events are encoded as Pressed with different coordinates.

    // Initial press at (5, 5) — sets potential, not active.
    auto press = ftxui::Mouse{
        ftxui::Mouse::Button::Left,
        ftxui::Mouse::Motion::Pressed,
        /*shift=*/false, /*meta=*/false, /*control=*/false,
        /*x=*/5, /*y=*/5,
    };
    app->OnEvent(ftxui::Event::Mouse("", press));
    EXPECT_TRUE(test_seams(app).text_selection_potential_for_testing());
    EXPECT_FALSE(test_seams(app).text_selection_active_for_testing());

    // Motion to (6, 5) — 1-cell displacement. With the old threshold
    // (dx²+dy² > 9), this would NOT activate. With the D-4 fix (> 0),
    // it must activate.
    auto motion = ftxui::Mouse{
        ftxui::Mouse::Button::Left,
        ftxui::Mouse::Motion::Pressed,
        /*shift=*/false, /*meta=*/false, /*control=*/false,
        /*x=*/6, /*y=*/5,
    };
    app->OnEvent(ftxui::Event::Mouse("", motion));
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelectionThreshold, SameCoordinateDoesNotActivateSelection) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_same_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Press at (10, 10).
    auto press = ftxui::Mouse{
        ftxui::Mouse::Button::Left,
        ftxui::Mouse::Motion::Pressed,
        false, false, false,
        10, 10,
    };
    app->OnEvent(ftxui::Event::Mouse("", press));
    EXPECT_TRUE(test_seams(app).text_selection_potential_for_testing());

    // Motion to same coordinate (10, 10) — zero displacement, must not
    // activate even with the > 0 threshold.
    auto same = ftxui::Mouse{
        ftxui::Mouse::Button::Left,
        ftxui::Mouse::Motion::Pressed,
        false, false, false,
        10, 10,
    };
    app->OnEvent(ftxui::Event::Mouse("", same));
    EXPECT_FALSE(test_seams(app).text_selection_active_for_testing());

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

// ═══════════════════════════════════════════════════════════════════════════
// Persistent selection + inline toast (Claude Code alignment)
// ═══════════════════════════════════════════════════════════════════════════

TEST(TextSelection, PersistsAfterRelease) {
    // After mouse release, the selection stays highlighted (active=true)
    // so the user can see what was copied.  Cleared on next interaction.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_persist_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Press at (5, 5) → potential.
    auto press = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 5, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", press));

    // Motion to (10, 5) → active.
    auto motion = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 10, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", motion));
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());

    // Release at (10, 5).  No screen in test, so copied won't be set,
    // but active must NOT be cleared.
    auto release = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Released,
        false, false, false, 10, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", release));
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing())
        << "selection should persist after release";
    EXPECT_FALSE(test_seams(app).text_selection_potential_for_testing())
        << "potential should be cleared after release";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelection, ClearsOnKeyPress) {
    // After a simulated copy, any key press clears the selection + toast.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_keyclear_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Simulate a completed copy.
    test_seams(app).simulate_copy_for_testing(42);
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());
    EXPECT_TRUE(test_seams(app).text_selection_copied_for_testing());

    // Send a key press — should clear the selection.
    app->OnEvent(ftxui::Event::Character('a'));
    EXPECT_FALSE(test_seams(app).text_selection_active_for_testing())
        << "key press should clear the selection";
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing())
        << "key press should clear the copied flag";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelection, ClearsOnMousePress) {
    // After a simulated copy, a new mouse press clears the old selection
    // before starting a new potential selection.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_mouseclear_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Simulate a completed copy.
    test_seams(app).simulate_copy_for_testing(10);
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());
    EXPECT_TRUE(test_seams(app).text_selection_copied_for_testing());

    // New mouse press — should clear the old selection and start fresh.
    auto press = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 3, 3,
    };
    app->OnEvent(ftxui::Event::Mouse("", press));
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing())
        << "new mouse press should clear the copied flag";
    EXPECT_TRUE(test_seams(app).text_selection_potential_for_testing())
        << "new mouse press should start a new potential selection";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelection, NewPressClearsActiveSelectionEvenWithoutCopied) {
    // Regression: after drag+release, active=true but copied=false (no
    // screen in test, or copy failed).  A new press must still clear the
    // old selection and start fresh — otherwise the old start position
    // is kept and the selection extends instead of resetting.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_nocopy_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Drag: press(5,5) → motion(10,5) → release(10,5).
    // No screen → copied stays false, but active stays true.
    auto press = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 5, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", press));
    auto motion = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 10, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", motion));
    auto release = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Released,
        false, false, false, 10, 5,
    };
    app->OnEvent(ftxui::Event::Mouse("", release));
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());

    // New press at (20, 10) — must clear old selection and start fresh.
    auto press2 = ftxui::Mouse{
        ftxui::Mouse::Button::Left, ftxui::Mouse::Motion::Pressed,
        false, false, false, 20, 10,
    };
    app->OnEvent(ftxui::Event::Mouse("", press2));
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing())
        << "new press should clear the old selection";
    EXPECT_TRUE(test_seams(app).text_selection_potential_for_testing())
        << "new press should start a new potential selection";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelection, SimulateCopySetsState) {
    // Verify simulate_copy_for_testing sets all fields correctly.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_sim_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    EXPECT_FALSE(test_seams(app).text_selection_active_for_testing());
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());

    test_seams(app).simulate_copy_for_testing(123);
    EXPECT_TRUE(test_seams(app).text_selection_active_for_testing());
    EXPECT_TRUE(test_seams(app).text_selection_copied_for_testing());
    EXPECT_EQ(test_seams(app).text_selection_copied_chars_for_testing(), 123u);

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(TextSelection, F2ToggleMouseTracking) {
    // F2 toggles mouse tracking at runtime when mouseMode: true.
    // When off, the terminal handles mouse natively (drag to select).
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_sel_toggle_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    // Explicit native mode opts out of the F2 application toggle.
    app->set_mouse_mode(false);
    app->OnEvent(ftxui::Event::F2);
    EXPECT_TRUE(test_seams(app).mouse_tracking_on_for_testing())
        << "F2 should be inactive when mouseMode is false";

    // Enable mouse mode (simulates mouseMode: true in config).
    app->set_mouse_mode(true);
    EXPECT_TRUE(test_seams(app).mouse_tracking_on_for_testing());

    // F2 toggles off.
    app->OnEvent(ftxui::Event::F2);
    EXPECT_FALSE(test_seams(app).mouse_tracking_on_for_testing())
        << "F2 should toggle mouse tracking off";

    // F2 toggles back on.
    app->OnEvent(ftxui::Event::F2);
    EXPECT_TRUE(test_seams(app).mouse_tracking_on_for_testing())
        << "F2 should toggle mouse tracking back on";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

#if defined(__APPLE__) || defined(__linux__)
class TextSelectionInteractiveCopy : public testing::TestWithParam<int> {};

TEST_P(TextSelectionInteractiveCopy, CopiesDisplayedTextAfterFrameIsCleared) {
    const auto storage_root = fs::temp_directory_path() /
        ("loom_selection_frame_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(storage_root);
    const auto copied_file = storage_root / "copied.txt";
    // Capture the actual native backend's stdin without changing the user's
    // clipboard. This exercises extraction, coordinator, spawn and pipe I/O.
    for (const auto* name : {"pbcopy", "xclip", "wl-copy"}) {
        const auto backend = storage_root / name;
        std::ofstream script(backend);
        script << "#!/bin/sh\n/bin/cat > \"$LOOM_SELECTION_COPY_FILE\"\n";
        script.close();
        fs::permissions(backend, fs::perms::owner_all);
    }
    ScopedEnvVar path_env("PATH");
    path_env.set(storage_root.string() + ":" +
                 (std::getenv("PATH") ? std::getenv("PATH") : ""));
    ScopedEnvVar output_env("LOOM_SELECTION_COPY_FILE");
    output_env.set(copied_file.string());
    ScopedEnvVar ssh_connection("SSH_CONNECTION");
    ScopedEnvVar ssh_tty("SSH_TTY");
    ssh_connection.unset();
    ssh_tty.unset();

    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    const std::string expected = GetParam() == 1
        ? "selection-中文🙂-probe" : "selection-copy-probe";
    config.model_params.model = expected;
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});
    app->SyncState();

    auto screen = ftxui::ScreenInteractive::FixedSize(120, 28);
    // Locate real rendered text before entering the interactive loop.
    ftxui::Render(screen, app->Render());
    int start_x = -1;
    int start_y = -1;
    int end_x = -1;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            std::string candidate;
            for (int end = x; end < screen.dimx(); ++end) {
                candidate += screen.PixelAt(end, y).character;
                if (candidate == expected) {
                    start_x = x;
                    start_y = y;
                    end_x = end;
                    break;
                }
                if (!expected.starts_with(candidate)) break;
            }
            if (start_x >= 0) break;
        }
        if (start_x >= 0) break;
    }
    ASSERT_GE(start_x, 0);
    app->set_screen(&screen);
    // Direct delivery lets us observe progress before Render drains the
    // background completion, without relying on backend timing.
    ftxui::Render(screen, app->Render());
    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Pressed,
        false, false, false, start_x, start_y}));
    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Released,
        false, false, false, end_x, start_y}));
    EXPECT_EQ(test_seams(app).copy_feedback_for_testing(), "Copying…");
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());
    // The real loop below starts a new selection and confirms its result.
    std::string copied;
    {
        // CTest supplies EOF on stdin. FTXUI's terminal reader otherwise
        // busy-spins on select/read, adding artificial CPU pressure.
        // Keep an input stream open as a real terminal would; mouse events
        // still travel through ScreenInteractive::PostEvent.
        struct StdinGuard {
            int saved;
            int read_end{-1};
            int write_end{-1};
            ~StdinGuard() {
                if (saved >= 0) {
                    (void)::dup2(saved, STDIN_FILENO);
                    ::close(saved);
                }
                if (read_end >= 0) ::close(read_end);
                if (write_end >= 0) ::close(write_end);
            }
        } stdin_guard{::dup(STDIN_FILENO)};
        ASSERT_GE(stdin_guard.saved, 0);
        ASSERT_EQ(::fcntl(stdin_guard.saved, F_SETFD, FD_CLOEXEC), 0);
        int input_pipe[2];
        ASSERT_EQ(::pipe(input_pipe), 0);
        stdin_guard.read_end = input_pipe[0];
        stdin_guard.write_end = input_pipe[1];
        ASSERT_EQ(::fcntl(input_pipe[0], F_SETFD, FD_CLOEXEC), 0);
        ASSERT_EQ(::fcntl(input_pipe[1], F_SETFD, FD_CLOEXEC), 0);
        ASSERT_EQ(::dup2(input_pipe[0], STDIN_FILENO), STDIN_FILENO);
        testing::internal::CaptureStdout();
        ftxui::Loop loop(&screen, app);
        loop.RunOnce();
        // Draw() flushed the visible frame and called Screen::Clear().
        EXPECT_EQ(screen.PixelAt(start_x, start_y).character, " ");
        const bool reverse = GetParam() == 1;
        // Terminal mouse reports are 1-based; ScreenInteractive subtracts
        // the frame origin before delivering events to AppAdapter.
        screen.PostEvent(ftxui::Event::Mouse("", {
            ftxui::Mouse::Left, ftxui::Mouse::Pressed,
            false, false, false, (reverse ? end_x : start_x) + 1, start_y + 1}));
        if (GetParam() != 1) loop.RunOnce();
        if (GetParam() == 2) {
            screen.PostEvent(ftxui::Event::Mouse("", {
                ftxui::Mouse::Left, ftxui::Mouse::Pressed,
                false, false, false, end_x, start_y + 1}));
            loop.RunOnce();
        }
        // Cases cover no motion, reversed batched press/release, and a drag
        // with an intermediate highlighted frame and a new release endpoint.
        screen.PostEvent(ftxui::Event::Mouse("", {
            ftxui::Mouse::Left, ftxui::Mouse::Released,
            false, false, false, (reverse ? start_x : end_x) + 1, start_y + 1}));
        loop.RunOnce();
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            loop.RunOnce();
            std::ifstream file(copied_file);
            copied.assign(std::istreambuf_iterator<char>(file), {});
            if (copied == expected &&
                test_seams(app).text_selection_copied_for_testing()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    const auto terminal_output = testing::internal::GetCapturedStdout();
    if (copied != expected) {
        for (const auto& error : loom::utils::log::get_in_memory_errors())
            ADD_FAILURE() << error.error;
    }
    EXPECT_EQ(copied, expected) << terminal_output;
    EXPECT_TRUE(test_seams(app).text_selection_copied_for_testing());
    app->set_screen(nullptr);
    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

INSTANTIATE_TEST_SUITE_P(
    RenderedFrames, TextSelectionInteractiveCopy, testing::Values(0, 1, 2));
#endif

TEST(CopyResultPipeline, StaleResultIsDiscarded) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_stale_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    namespace clip = loom::utils::clipboard;

    // Set generation to 1 (simulating a newer selection after the result).
    test_seams(app).set_copy_generation_for_testing(1);

    // Inject a stale result (generation 0).
    clip::CopyResult result;
    result.status = clip::CopyResult::Status::NativeCopied;
    result.backend_name = "xclip";
    result.bytes_sent = 42;
    test_seams(app).inject_copy_result_for_testing(
        0, std::move(result), "stale text");

    // Drain — the stale result should be discarded (no notification).
    test_seams(app).drain_copy_result_for_testing();
    EXPECT_TRUE(test_seams(app).footer_notification_text_for_testing().empty())
        << "stale result should not produce a notification";

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(CopyResultPipeline, NativeCopiedDoesNotShowFooterNotification) {
    // The inline toast confirms a completed copy, never an optimistic one.
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_copied_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    namespace clip = loom::utils::clipboard;

    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Pressed,
        false, false, false, 5, 5}));
    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Released,
        false, false, false, 10, 5}));
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());

    // Generation 0 matches the initial copy_generation_ (0).
    clip::CopyResult result;
    result.status = clip::CopyResult::Status::NativeCopied;
    result.backend_name = "xclip";
    result.bytes_sent = 42;
    test_seams(app).inject_copy_result_for_testing(
        0, std::move(result), "test text");

    test_seams(app).drain_copy_result_for_testing();
    EXPECT_TRUE(test_seams(app).text_selection_copied_for_testing());
    EXPECT_EQ(test_seams(app).text_selection_copied_chars_for_testing(), 9u);
    auto notification = test_seams(app).footer_notification_text_for_testing();
    EXPECT_TRUE(notification.empty())
        << "native copy success should NOT show footer notification "
           "(inline toast handles it), got: "
        << notification;
    (void)app->Render();
    EXPECT_EQ(test_seams(app).screen_state_for_testing()->copy_toast_text,
              "Copied 9 chars to clipboard");

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

TEST(CopyResultPipeline, NativeFailedFallsBackToOsc52) {
    loom::core::ToolRegistry tools;
    loom::core::QueryEngineConfig config;
    config.context_window.auto_compact = false;
    config.cwd = fs::temp_directory_path().string();
    loom::core::QueryEngine engine(std::move(config), tools);
    loom::commands::AppCommandRegistry commands;
    const auto storage_root = fs::temp_directory_path() /
        ("loom_osc52_" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    auto app = ftxui::Make<loom::ui::AppAdapter>(
        &engine, nullptr, &commands, storage_root, [] {});

    namespace clip = loom::utils::clipboard;

    // Generation 0 matches.
    clip::CopyResult result;
    result.status = clip::CopyResult::Status::NativeFailed;
    result.backend_name = "xclip";
    result.error_detail = "backend exited with status 1";
    test_seams(app).inject_copy_result_for_testing(
        0, std::move(result), "test text");

    testing::internal::CaptureStdout();
    test_seams(app).drain_copy_result_for_testing();
    const auto emitted = testing::internal::GetCapturedStdout();
    EXPECT_EQ(emitted, "\x1b]52;c;dGVzdCB0ZXh0\a");
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());
    EXPECT_TRUE(test_seams(app).footer_notification_text_for_testing().empty());
    (void)app->Render();
    EXPECT_EQ(test_seams(app).screen_state_for_testing()->copy_toast_text,
              "Copy sent to terminal · paste to verify");

    app.reset();
    std::error_code ec;
    fs::remove_all(storage_root, ec);
}

// The same range calculation drives copied text and visible highlights.
class LinearSelectionGeometry : public testing::TestWithParam<bool> {};

TEST_P(LinearSelectionGeometry, SelectsFirstTailWholeMiddleAndLastPrefix) {
    auto content = ftxui::vbox({ftxui::text("abcdefgh"), ftxui::text("ijklmnop"),
                               ftxui::text("qrstuvwx")});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(8),
                                       ftxui::Dimension::Fixed(3));
    ftxui::Render(screen, content);
    const auto background = screen.PixelAt(0, 0).background_color;
    const bool reverse = GetParam();
    const int x0 = reverse ? 4 : 2, y0 = reverse ? 2 : 0;
    const int x1 = reverse ? 2 : 4, y1 = reverse ? 0 : 2;
    EXPECT_EQ(loom::ui::AppTestingSeams::selected_text_for_testing(
                  screen, x0, y0, x1, y1), "cdefgh\nijklmnop\nqrstu");
    ftxui::Render(screen, loom::ui::AppTestingSeams::selection_highlight_for_testing(
                              content, x0, y0, x1, y1));
    const auto highlight = screen.PixelAt(2, 0).background_color;
    ASSERT_NE(highlight, background);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool selected = (y == 0 && x >= 2) || y == 1 || (y == 2 && x <= 4);
            EXPECT_EQ(screen.PixelAt(x, y).background_color,
                      selected ? highlight : background) << x << ',' << y;
        }
    }
}

TEST_P(LinearSelectionGeometry, PreservesLeadingEmptyRowAndInteriorSpaces) {
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(8),
                                       ftxui::Dimension::Fixed(3));
    ftxui::Render(screen, ftxui::vbox({ftxui::text(""), ftxui::text("a b"),
                                     ftxui::text("xyz")}));
    const bool reverse = GetParam();
    EXPECT_EQ(loom::ui::AppTestingSeams::selected_text_for_testing(
                  screen, reverse ? 1 : 5, reverse ? 2 : 0,
                  reverse ? 5 : 1, reverse ? 0 : 2), "\na b\nxy");
}

INSTANTIATE_TEST_SUITE_P(ForwardAndReverse, LinearSelectionGeometry,
                        testing::Bool());

class WideSelectionGeometry
    : public testing::TestWithParam<std::tuple<bool, int, int>> {};

TEST_P(WideSelectionGeometry, HighlightsAndCopiesOwnersAndContinuationsTogether) {
    auto content = ftxui::vbox({ftxui::text("a中🙂 z"), ftxui::text("b中🙂 q")});
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(8),
                                       ftxui::Dimension::Fixed(2));
    ftxui::Render(screen, content);
    ASSERT_EQ(screen.PixelAt(2, 0).character, ""); // CJK continuation.
    ASSERT_EQ(screen.PixelAt(4, 1).character, ""); // Emoji continuation.
    const auto background = screen.PixelAt(0, 0).background_color;
    const auto [reverse, first_column, last_column] = GetParam();
    const int x0 = reverse ? last_column : first_column, y0 = reverse ? 1 : 0;
    const int x1 = reverse ? first_column : last_column, y1 = reverse ? 0 : 1;
    EXPECT_EQ(loom::ui::AppTestingSeams::selected_text_for_testing(
                  screen, x0, y0, x1, y1), "中🙂 z\nb中🙂");
    ftxui::Render(screen, loom::ui::AppTestingSeams::selection_highlight_for_testing(
                              content, x0, y0, x1, y1));
    const auto highlight = screen.PixelAt(1, 0).background_color;
    ASSERT_NE(highlight, background);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool selected = y == 0 ? x >= 1 : x <= 4;
            EXPECT_EQ(screen.PixelAt(x, y).background_color,
                      selected ? highlight : background) << x << ',' << y;
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Endpoints, WideSelectionGeometry,
    testing::Combine(testing::Bool(), testing::Values(1, 2), testing::Values(3, 4)));

TEST(LinearSelection, EmptyFrameAndOutOfBoundsEndpointsAreSafe) {
    auto empty = ftxui::Screen::Create(ftxui::Dimension::Fixed(0),
                                      ftxui::Dimension::Fixed(0));
    EXPECT_TRUE(loom::ui::AppTestingSeams::selected_text_for_testing(
                    empty, -1, -1, 99, 99).empty());
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(3),
                                       ftxui::Dimension::Fixed(1));
    ftxui::Render(screen, ftxui::text("abc"));
    EXPECT_EQ(loom::ui::AppTestingSeams::selected_text_for_testing(
                  screen, -1, -1, 99, 99), "abc");
}

class SelectionCopyFeedback : public testing::Test {
protected:
    loom::core::ToolRegistry tools;
    loom::commands::AppCommandRegistry commands;
    std::unique_ptr<loom::core::QueryEngine> engine;
    std::shared_ptr<loom::ui::AppAdapter> app;
    fs::path storage;

    void SetUp() override {
        loom::core::QueryEngineConfig config;
        config.context_window.auto_compact = false;
        config.cwd = fs::temp_directory_path().string();
        engine = std::make_unique<loom::core::QueryEngine>(std::move(config), tools);
        storage = fs::temp_directory_path() / ("loom_copy_feedback_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
        app = ftxui::Make<loom::ui::AppAdapter>(
            engine.get(), nullptr, &commands, storage, [] {});
    }
    void TearDown() override {
        app.reset();
        engine.reset();
        std::error_code ec;
        fs::remove_all(storage, ec);
    }
    void inject(loom::utils::clipboard::CopyResult::Status status) {
        loom::utils::clipboard::CopyResult result;
        result.status = status;
        test_seams(app).inject_copy_result_for_testing(0, std::move(result), "中文🙂");
    }
    std::string feedback() {
        (void)app->Render();
        return test_seams(app).screen_state_for_testing()->copy_toast_text.value_or("");
    }
};

TEST_F(SelectionCopyFeedback, UnsupportedNativeCopyReportsOnlyTerminalRequest) {
    inject(loom::utils::clipboard::CopyResult::Status::Unsupported);
    testing::internal::CaptureStdout();
    test_seams(app).drain_copy_result_for_testing();
    EXPECT_EQ(testing::internal::GetCapturedStdout(), "\x1b]52;c;5Lit5paH8J+Zgg==\a");
    EXPECT_EQ(feedback(), "Copy sent to terminal · paste to verify");
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());
}

#if defined(__APPLE__) || defined(__linux__)
TEST_F(SelectionCopyFeedback, TerminalWriteFailureShowsFailedInsteadOfCopied) {
    inject(loom::utils::clipboard::CopyResult::Status::Unsupported);
    // Restore and clear stdout before any assertions; discard buffered failed
    // OSC bytes so they cannot later reach the user's real terminal.
    struct ReadOnlyStdout {
        int saved = ::dup(STDOUT_FILENO);
        bool ready = false;
        ReadOnlyStdout() {
            std::fflush(stdout);
            const int fd = ::open("/dev/null", O_RDONLY);
            if (fd >= 0) {
                ready = saved >= 0 && ::dup2(fd, STDOUT_FILENO) >= 0;
                ::close(fd);
            }
            std::clearerr(stdout);
        }
        ~ReadOnlyStdout() {
            if (ready && std::ferror(stdout)) (void)std::freopen("/dev/null", "w", stdout);
            if (saved >= 0) {
                (void)::dup2(saved, STDOUT_FILENO);
                ::close(saved);
            }
            std::clearerr(stdout);
        }
    };
    bool redirected;
    {
        ReadOnlyStdout output;
        redirected = output.ready;
        if (redirected) test_seams(app).drain_copy_result_for_testing();
    }
    ASSERT_TRUE(redirected);
    EXPECT_EQ(feedback(), "Copy failed · try again");
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());
}
#endif

class SelectionCopyCancellation : public SelectionCopyFeedback,
                                  public testing::WithParamInterface<int> {};

TEST_P(SelectionCopyCancellation, DismissalInvalidatesPendingTerminalFallback) {
    test_seams(app).simulate_copy_for_testing(3);
    inject(loom::utils::clipboard::CopyResult::Status::Unsupported);
    testing::internal::CaptureStdout();
    if (GetParam() == 0) app->OnEvent(ftxui::Event::Character('a'));
    if (GetParam() == 1) app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Pressed, false, false, false, 1, 1}));
    if (GetParam() == 2) app->OnEvent(ftxui::Event::F2);
    EXPECT_EQ(testing::internal::GetCapturedStdout().find("\x1b]52;"), std::string::npos);
    EXPECT_TRUE(feedback().empty());
    EXPECT_FALSE(test_seams(app).text_selection_copied_for_testing());
    // Completion arriving after cancellation is stale as well.
    inject(loom::utils::clipboard::CopyResult::Status::Unsupported);
    testing::internal::CaptureStdout();
    test_seams(app).drain_copy_result_for_testing();
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
    EXPECT_TRUE(feedback().empty());
}

INSTANTIATE_TEST_SUITE_P(KeyPressMousePressAndF2, SelectionCopyCancellation,
                        testing::Values(0, 1, 2));

TEST_F(SelectionCopyFeedback, ExplicitNativeModeDoesNotHandleSelectionEvents) {
    app->set_mouse_mode(false);
    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Pressed, false, false, false, 1, 1}));
    app->OnEvent(ftxui::Event::Mouse("", {
        ftxui::Mouse::Left, ftxui::Mouse::Released, false, false, false, 5, 1}));
    EXPECT_FALSE(test_seams(app).text_selection_active_for_testing());
    EXPECT_FALSE(test_seams(app).text_selection_potential_for_testing());
    EXPECT_TRUE(feedback().empty());
}

TEST(MessageInteractions, ReplShortcutsPreservePromptAndThinkingExpansion) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    state->input_text = "draft prompt";
    state->input_cursor = 5;
    repl::MessageDisplayEntry tool;
    tool.id = "tool-1";
    tool.role = "tool";
    tool.tool_name = "Read";
    for (int i = 0; i < 14; ++i) tool.content_preview += "result-" + std::to_string(i) + "\n";
    state->messages_store.messages.push_back(tool);
    state->expanded_keys.insert("thinking-row");
    auto component = repl::ReplScreen(state, {});
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x05')));
    EXPECT_TRUE(state->expanded_keys.contains("Read"));
    EXPECT_TRUE(state->expanded_keys.contains("thinking-row"));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x05')));
    EXPECT_FALSE(state->expanded_keys.contains("Read"));
    EXPECT_TRUE(state->expanded_keys.contains("thinking-row"));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x0F')));
    EXPECT_TRUE(state->is_transcript_mode);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x05')));
    EXPECT_TRUE(state->show_all_in_transcript);
    EXPECT_FALSE(state->expanded_keys.contains("Read"));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('\x0F')));
    EXPECT_FALSE(state->is_transcript_mode);
    EXPECT_FALSE(state->show_all_in_transcript);
    EXPECT_EQ(state->input_text, "draft prompt");
    EXPECT_EQ(state->input_cursor, 5u);
}

TEST(MessageInteractions, ReplToolActionClickExpandsAndCollapsesRealOutput) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    repl::MessageDisplayEntry tool;
    tool.id = "clickable-tool";
    tool.role = "tool";
    tool.tool_name = "Bash";
    for (int i = 0; i < 14; ++i) tool.content_preview += "result-" + std::to_string(i) + "\n";
    state->messages_store.messages.push_back(tool);
    auto component = repl::ReplScreen(state, {});
    auto render = [&] {
        return strip_ansi(render_to_plain_text(component->Render(), 120, 40));
    };
    auto click_action = [&](std::string_view label) {
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(120), ftxui::Dimension::Fixed(40));
        ftxui::Render(screen, component->Render());
        for (int y = 0; y < 40; ++y) {
            std::string line;
            for (int x = 0; x < 120; ++x) line += screen.PixelAt(x, y).character;
            auto x = line.find(label);
            if (x != std::string::npos) {
                ftxui::Mouse mouse;
                mouse.button = ftxui::Mouse::Left;
                mouse.motion = ftxui::Mouse::Released;
                // Prefix includes the multi-byte connector. Find the matching
                // screen cell rather than using the UTF-8 byte offset as x.
                for (int col = 0; col < 120; ++col) {
                    if (screen.PixelAt(col, y).character == "[" && screen.PixelAt(col, y).underlined) {
                        mouse.x = col;
                        mouse.y = y;
                        return component->OnEvent(ftxui::Event::Mouse("", mouse));
                    }
                }
            }
        }
        return false;
    };
    EXPECT_NE(render().find("[Show details]"), std::string::npos) << render();
    EXPECT_TRUE(click_action("[Show details]"));
    EXPECT_TRUE(state->expanded_keys.contains("chain:clickable-tool"));
    EXPECT_NE(render().find("[Ctrl+E] Toggle tools"), std::string::npos) << render();
    EXPECT_TRUE(click_action("[Show more]"));
    EXPECT_TRUE(state->expanded_keys.contains("Bash"));
    EXPECT_NE(render().find("result-13"), std::string::npos);
    EXPECT_TRUE(click_action("[Show less]"));
    EXPECT_FALSE(state->expanded_keys.contains("Bash"));
    EXPECT_EQ(render().find("result-13"), std::string::npos);
    EXPECT_TRUE(click_action("[Hide details]"));
    EXPECT_FALSE(state->expanded_keys.contains("chain:clickable-tool"));
    EXPECT_NE(render().find("[Show details]"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('\x0F'));
    EXPECT_EQ(render().find("[Ctrl+E] Toggle tools"), std::string::npos);
    EXPECT_TRUE(click_action("[Show more]"));
    EXPECT_TRUE(state->expanded_keys.contains("Bash"));
}

TEST(MessageInteractions, TranscriptCapUsesKeyboardHintForMessageLimit) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    for (int i = 0; i < 32; ++i) {
        repl::MessageDisplayEntry row;
        row.id = "transcript-row-" + std::to_string(i);
        row.role = "user";
        row.content_preview = "message-" + std::to_string(i);
        state->messages_store.messages.push_back(std::move(row));
    }
    auto component = repl::ReplScreen(state, {});
    component->OnEvent(ftxui::Event::Character('\x0F'));
    state->messages_store.scroll_pinned_to_bottom = false;
    auto before = strip_ansi(render_to_plain_text(component->Render(), 120, 110));
    EXPECT_NE(before.find("[Ctrl+E] Show all messages"), std::string::npos);
    EXPECT_EQ(before.find("message-0"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('\x05'));
    auto after = strip_ansi(render_to_plain_text(component->Render(), 120, 110));
    EXPECT_EQ(after.find("Show all messages"), std::string::npos);
    EXPECT_NE(after.find("message-0"), std::string::npos);
}

TEST(MessageInteractions, CurrentErrorShortcutsAreShownOnlyInTheirScreenContext) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    repl::MessageDisplayEntry error;
    error.id = "error-card";
    error.role = "system";
    error.is_error = true;
    error.is_model_error = true;
    error.can_retry = true;
    error.content_preview = "provider failed";
    state->messages_store.messages.push_back(error);
    int retried = 0;
    repl::ReplScreenCallbacks callbacks;
    callbacks.on_retry = [&] { ++retried; };
    auto component = repl::ReplScreen(state, callbacks);
    auto rendered = [&] { return strip_ansi(render_to_plain_text(component->Render(), 120, 40)); };
    EXPECT_NE(rendered().find("[R] Retry request"), std::string::npos);
    EXPECT_NE(rendered().find("[Esc] Dismiss error"), std::string::npos);
    EXPECT_EQ(rendered().find("[Enter]"), std::string::npos);
    EXPECT_NE(rendered().find("Model request failed"), std::string::npos);
    EXPECT_EQ(rendered().find("System"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('R')));
    EXPECT_EQ(retried, 1);
    state->input_text = "draft";
    EXPECT_EQ(rendered().find("[R] Retry request"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('r'));
    EXPECT_EQ(retried, 1);
    state->input_text.clear();
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_TRUE(state->messages_store.messages.empty());
    EXPECT_EQ(rendered().find("Dismiss error"), std::string::npos);
}

TEST(MessageInteractions, ToolAndCommandFailuresDoNotOfferRequestRecoveryOrDeleteResults) {
    namespace repl = loom::ui::repl_screen;
    for (const auto& role : {"tool", "system"}) {
        auto state = std::make_shared<repl::ReplScreenState>();
        repl::MessageDisplayEntry error;
        error.id = "execution-error";
        error.role = role;
        error.is_error = true;
        error.content_preview = "Permission denied for tool Bash";
        if (error.role == "tool") error.tool_name = "Bash";
        state->messages_store.messages.push_back(error);
        int retried = 0;
        repl::ReplScreenCallbacks callbacks;
        callbacks.on_retry = [&] { ++retried; };
        auto component = repl::ReplScreen(state, callbacks);
        auto text = strip_ansi(render_to_plain_text(component->Render(), 120, 40));
        EXPECT_EQ(text.find("Retry request"), std::string::npos);
        EXPECT_EQ(text.find("Dismiss error"), std::string::npos);
        EXPECT_EQ(text.find("Model request failed"), std::string::npos);
        component->OnEvent(ftxui::Event::Character('R'));
        EXPECT_EQ(retried, 0);
        EXPECT_EQ(state->input_text, "R");
        state->input_text.clear();
        component->OnEvent(ftxui::Event::Escape);
        ASSERT_EQ(state->messages_store.messages.size(), 1u);
        EXPECT_EQ(state->messages_store.messages.front().id, error.id);
    }
}

TEST(MessageInteractions, ModelRequestRecoveryRequiresRetryabilityAndIdleState) {
    namespace repl = loom::ui::repl_screen;
    auto state = std::make_shared<repl::ReplScreenState>();
    repl::MessageDisplayEntry error;
    error.id = "model-error";
    error.role = "system";
    error.is_error = true;
    error.is_model_error = true;
    error.content_preview = "HTTP 401: invalid API credentials";
    state->messages_store.messages.push_back(error);
    int retried = 0, interrupted = 0;
    repl::ReplScreenCallbacks callbacks;
    callbacks.on_retry = [&] { ++retried; };
    callbacks.on_interrupt = [&] { ++interrupted; };
    auto component = repl::ReplScreen(state, callbacks);
    const auto render = [&] {
        return strip_ansi(render_to_plain_text(component->Render(), 120, 40));
    };
    EXPECT_NE(render().find("Model request failed"), std::string::npos);
    EXPECT_NE(render().find("HTTP 401: invalid API credentials"), std::string::npos);
    EXPECT_EQ(render().find("Retry request"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('R'));
    EXPECT_EQ(retried, 0);
    state->input_text.clear();
    state->messages_store.messages.back().can_retry = true;
    state->query_running = true;
    state->task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
    EXPECT_EQ(render().find("Retry request"), std::string::npos);
    EXPECT_EQ(render().find("Dismiss error"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('R'));
    EXPECT_EQ(retried, 0);
    state->input_text.clear();
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Escape));
    EXPECT_EQ(interrupted, 1);
    ASSERT_EQ(state->messages_store.messages.size(), 1u);
    state->query_running = false;
    state->task_view_store.spinner_mode = repl::SpinnerMode::Hidden;
    EXPECT_NE(render().find("[R] Retry request"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('R')));
    EXPECT_EQ(retried, 1);
    state->messages_store.messages.back().session_expired = true;
    EXPECT_EQ(render().find("Retry request"), std::string::npos);
    component->OnEvent(ftxui::Event::Character('R'));
    EXPECT_EQ(retried, 1);
}
