/// @file test_ui_features.cpp
/// @brief UI feature tests: AutocompleteSources, LiveTeamsUi, PasteCoordinator.
/// Split from test_ui_runtime.cpp (SLOC budget).

#include <cstdlib>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.app.paste_coordinator;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.messages.message_image;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.teams.team_helpers;
import loom.teams.swarm.helpers;
import loom.constants.constants;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.features.teams.live_teammates;

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

