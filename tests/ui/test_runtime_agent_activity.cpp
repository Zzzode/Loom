// RFC 0021: native terminal cells and the shared activity-row budget.
#include <cstdlib>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>
#include "test_ui_helpers.h"

import std;
import loom.ui.features.agents.runtime_agents;
import loom.ui.features.todos.todo_panel;
import loom.ui.features.teams.live_teammates;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.ui.foundation.theme_provider;
import loom.ui.screens.task_view_store;

namespace {
namespace runtime = loom::ui::agents::runtime;
namespace todo = loom::ui::todos;
namespace team = loom::ui::teams::live;
namespace repl = loom::ui::repl_screen;
using namespace ftxui;

struct ActivityEnv {
    std::string key;
    std::optional<std::string> previous;
    ActivityEnv(std::string name, const std::string& value) : key(std::move(name)) {
        if (const auto* old = std::getenv(key.c_str())) previous = old;
        setenv(key.c_str(), value.c_str(), 1);
    }
    ~ActivityEnv() {
        if (previous) setenv(key.c_str(), previous->c_str(), 1);
        else unsetenv(key.c_str());
    }
};

Screen paint(Element element, int columns, int rows) {
    auto screen = Screen::Create(Dimension::Fixed(columns), Dimension::Fixed(rows));
    Render(screen, std::move(element));
    return screen;
}

std::vector<runtime::Instance> agents(int count = 6) {
    std::vector<runtime::Instance> result;
    for (int i = 0; i < count; ++i) {
        result.push_back({std::to_string(i), "runtime-" + std::to_string(i), "",
            runtime::Status::Running, i % 2 != 0});
    }
    return result;
}

std::vector<todo::TodoDisplayItem> todos(int count = 4) {
    std::vector<todo::TodoDisplayItem> result;
    for (int i = 0; i < count; ++i) {
        todo::TodoDisplayItem item;
        item.content = "todo-" + std::to_string(i);
        result.push_back(item);
    }
    return result;
}

std::vector<team::LiveTeammate> teams(int count = 4) {
    std::vector<team::LiveTeammate> result;
    for (int i = 0; i < count; ++i) {
        team::LiveTeammate item;
        item.name = "team-" + std::to_string(i);
        item.status = "running";
        result.push_back(item);
    }
    return result;
}

std::string_view row_containing(const std::string& text, std::string_view label) {
    const auto position = text.find(label);
    if (position == std::string::npos) return {};
    const auto newline = text.rfind('\n', position);
    const auto start = newline == std::string::npos ? 0 : newline + 1;
    const auto end = text.find('\n', position);
    return std::string_view(text).substr(start, end == std::string::npos ? text.size() - start : end - start);
}

int required_rows(const Element& element) {
    element->ComputeRequirement();
    return element->requirement().min_y;
}

} // namespace

TEST(RuntimeAgentActivity, ThreeCanonicalGroupsReserveHeadersThenRunStableDetailRoundsThroughTwelveRows) {
    auto tasks = todos(6);
    for (auto& task : tasks) task.status = todo::TodoDisplayItem::Status::InProgress;
    for (int budget = 0; budget <= 12; ++budget) {
        SCOPED_TRACE(budget);
        auto element = runtime::RenderRuntimeAgentActivity(agents(), tasks, teams(6), 100, budget);
        EXPECT_EQ(required_rows(element), budget);
        if (budget == 0) continue;
        const auto text = strip_ansi(paint(element, 100, budget).ToString());
        EXPECT_NE(text.find("Tasks 6 active · 0/6 done"), std::string::npos);
        EXPECT_EQ(text.find("Subagents 6 running") != std::string::npos, budget >= 2);
        EXPECT_EQ(text.find("Agent Teams · 6 agents running") != std::string::npos, budget >= 3);
        const int detail_slots = std::max(0, budget - 3);
        for (int index = 0; index < 3; ++index) {
            EXPECT_EQ(text.find("todo-" + std::to_string(index)) != std::string::npos, detail_slots >= 3 * index + 1);
            EXPECT_EQ(text.find("runtime-" + std::to_string(index)) != std::string::npos, detail_slots >= 3 * index + 2);
            EXPECT_EQ(text.find("@team-" + std::to_string(index)) != std::string::npos, detail_slots >= 3 * index + 3);
        }
        EXPECT_EQ(text.find("hidden"), std::string::npos);
        EXPECT_EQ(text.find("Team Agent "), std::string::npos);
        EXPECT_EQ(text.find("Subagent "), std::string::npos);
        if (budget >= 3) {
            EXPECT_LT(text.find("Tasks"), text.find("Subagents"));
            EXPECT_LT(text.find("Subagents"), text.find("Agent Teams"));
        }
    }
}

TEST(RuntimeAgentActivity, GroupSubsetsUseOnlyActualRowsAndRespectCallerBudgetWithoutFiveRowCap) {
    for (int mask = 0; mask < 8; ++mask) for (int budget = 0; budget <= 12; ++budget) {
        auto tasks = mask & 1 ? todos(3) : todos(0);
        for (auto& task : tasks) task.status = todo::TodoDisplayItem::Status::InProgress;
        auto workers = agents(mask & 2 ? 3 : 0);
        auto members = teams(mask & 4 ? 3 : 0);
        const int groups = static_cast<int>(!tasks.empty()) + static_cast<int>(!workers.empty()) + static_cast<int>(!members.empty());
        auto element = runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, budget);
        SCOPED_TRACE(std::format("mask={},budget={}", mask, budget));
        EXPECT_EQ(required_rows(element), std::min(budget, groups * 4));
    }
    EXPECT_EQ(required_rows(runtime::RenderRuntimeAgentActivity(agents(), todos(), teams(), 80, -1)), 0);
}

TEST(RuntimeAgentActivity, TinyBudgetsChooseGroupStatePriorityThenRenderCanonicalOrder) {
    auto tasks = todos(1);
    auto workers = agents(1);
    auto members = teams(1);
    auto one = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 1), 100, 1).ToString());
    EXPECT_NE(one.find("Subagents 1 running"), std::string::npos);
    EXPECT_EQ(one.find("Tasks"), std::string::npos);
    auto two = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 2), 100, 2).ToString());
    EXPECT_LT(two.find("Subagents"), two.find("Agent Teams"));
    EXPECT_EQ(two.find("Tasks"), std::string::npos);
    workers[0].status = runtime::Status::Queued;
    members[0].status = "unknown";
    one = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 1), 100, 1).ToString());
    EXPECT_NE(one.find("Agent Teams · 1 agent unknown"), std::string::npos);
    members[0].status = "idle";
    one = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 1), 100, 1).ToString());
    EXPECT_NE(one.find("Subagents 1 queued"), std::string::npos);
    workers[0].status = runtime::Status::Completed;
    one = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 1), 100, 1).ToString());
    EXPECT_NE(one.find("Tasks 0/1 done"), std::string::npos);
    tasks[0].status = todo::TodoDisplayItem::Status::Completed;
    one = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, 1, true), 100, 1).ToString());
    EXPECT_NE(one.find("✓ Tasks 1/1 completed"), std::string::npos);
}

TEST(RuntimeAgentActivity, HeadingsAreBoldMutedAndChildrenStartTwoCellsFurtherRight) {
    auto tasks = todos(1);
    tasks[0].status = todo::TodoDisplayItem::Status::InProgress;
    for (const auto prefix : {std::string_view{}, std::string_view{"  ⎿  "}}) {
        const auto screen = paint(runtime::RenderRuntimeAgentActivity(agents(1), tasks, teams(1), 100, 6, false, prefix), 100, 6);
        const int heading_x = prefix.empty() ? 0 : 5;
        for (int row : {0, 2, 4}) {
            EXPECT_EQ(screen.PixelAt(heading_x, row).character, row == 0 ? "T" : row == 2 ? "S" : "A");
            EXPECT_TRUE(screen.PixelAt(heading_x, row).bold);
            EXPECT_EQ(screen.PixelAt(heading_x, row).foreground_color,
                loom::ui::design::theme::current_theme().palette->muted);
            for (int x = 0; x < heading_x; ++x)
                if (!(row == 0 && x == 2)) EXPECT_EQ(screen.PixelAt(x, row).character, " ");
        }
        for (int row : {1, 3, 5}) {
            for (int x = 0; x < heading_x + 2; ++x) EXPECT_EQ(screen.PixelAt(x, row).character, " ");
            EXPECT_NE(screen.PixelAt(heading_x + 2, row).character, " ");
            EXPECT_FALSE(screen.PixelAt(heading_x + 2, row).bold);
        }
        const auto text = strip_ansi(screen.ToString());
        const auto connector = text.find("⎿");
        EXPECT_EQ(connector != std::string::npos, !prefix.empty());
        if (connector != std::string::npos) EXPECT_EQ(text.find("⎿", connector + std::string("⎿").size()), std::string::npos);
        EXPECT_LT(text.find("todo-0"), text.find("Subagents"));
        EXPECT_LT(text.find("runtime-0"), text.find("Agent Teams"));
    }
}

TEST(RuntimeAgentActivity, RunningRoundsPrecedeAttentionQueuedAndPendingWithoutMutatingInputs) {
    auto workers = agents(4);
    workers[0].status = runtime::Status::Queued;
    workers[1].status = runtime::Status::Failed;
    workers[2].status = runtime::Status::Completed;
    auto members = teams(2);
    members[0].status = "unknown";
    auto tasks = todos(2);
    const auto original_workers = workers;
    const auto original_tasks = tasks;
    for (int budget = 3; budget <= 10; ++budget) {
        const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, tasks, members, 120, budget), 120, budget).ToString());
        EXPECT_EQ(text.find("runtime-3") != std::string::npos, budget >= 4);
        EXPECT_EQ(text.find("@team-1") != std::string::npos, budget >= 5);
        EXPECT_EQ(text.find("runtime-1"), std::string::npos);
        EXPECT_EQ(text.find("@team-0") != std::string::npos, budget >= 6);
        EXPECT_EQ(text.find("runtime-0") != std::string::npos, budget >= 7);
        EXPECT_EQ(text.find("todo-0") != std::string::npos, budget >= 8);
        EXPECT_EQ(text.find("todo-1") != std::string::npos, budget >= 9);
        EXPECT_EQ(text.find("runtime-2"), std::string::npos);
        EXPECT_LT(text.find("Tasks"), text.find("Subagents"));
        EXPECT_LT(text.find("Subagents"), text.find("Agent Teams"));
    }
    EXPECT_EQ(workers, original_workers);
    EXPECT_EQ(tasks, original_tasks);
    EXPECT_EQ(members[0].status, "unknown");
    EXPECT_EQ(members[1].status, "running");
}

TEST(RuntimeAgentActivity, ExhaustedRunningGroupsYieldImmediatelyToOtherGroups) {
    auto tasks = todos(4);
    for (auto& task : tasks) task.status = todo::TodoDisplayItem::Status::InProgress;
    const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(agents(1), tasks, teams(1), 100, 9), 100, 9).ToString());
    for (const auto item : {"todo-0", "todo-1", "todo-2", "todo-3", "runtime-0", "@team-0"})
        EXPECT_NE(text.find(item), std::string::npos);
}

TEST(RuntimeAgentActivity, EachAgentGroupCountsOnlyItsOwnCurrentWorkAndFoldsItsHistory) {
    auto workers = agents(4);
    workers[0].status = runtime::Status::Completed;
    workers[1].status = runtime::Status::Cancelled;
    workers[2].status = runtime::Status::Failed;
    auto members = teams(3);
    members[0].status = "idle";
    members[1].status = "unknown";
    const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, {}, members, 120, 8), 120, 8).ToString());
    EXPECT_NE(text.find("Subagents 1 running · 3 ended · 1 failed"), std::string::npos);
    EXPECT_NE(text.find("Agent Teams · 1 agent running · 1 agent unknown"), std::string::npos);
    for (const auto hidden : {"completed", "cancelled", "idle", "runtime-0", "runtime-1", "@team-0"})
        EXPECT_EQ(text.find(hidden), std::string::npos);
    EXPECT_EQ(text.find("runtime-2"), std::string::npos);
    EXPECT_NE(row_containing(text, "runtime-3").find("running"), std::string_view::npos);
}

TEST(RuntimeAgentActivity, FullyFoldedGroupsUseThreeHeadersAndPreserveCompletionCheckmark) {
    auto workers = agents(2);
    workers[0].status = runtime::Status::Completed;
    workers[1].status = runtime::Status::Cancelled;
    auto tasks = todos(2);
    for (auto& task : tasks) task.status = todo::TodoDisplayItem::Status::Completed;
    auto members = teams(1);
    members[0].status = "idle";
    for (int budget = 0; budget <= 12; ++budget) {
        auto element = runtime::RenderRuntimeAgentActivity(workers, tasks, members, 100, budget, true);
        EXPECT_EQ(required_rows(element), std::min(3, budget));
        const auto text = strip_ansi(paint(element, 100, 3).ToString());
        EXPECT_EQ(text.find("✓ Tasks 2/2 completed") != std::string::npos, budget >= 1);
        EXPECT_EQ(text.find("Subagents 2 ended · /subagents") != std::string::npos, budget >= 2);
        EXPECT_EQ(text.find("Agent Teams · 1 agent idle") != std::string::npos, budget >= 3);
        EXPECT_EQ(text.find("todo-"), std::string::npos);
        EXPECT_EQ(text.find("runtime-"), std::string::npos);
        EXPECT_EQ(text.find("@team-"), std::string::npos);
    }
}

TEST(RuntimeAgentActivity, TaskOnlyPartialCompletionRetainsActiveFirstThenRemainingSourceOrder) {
    auto tasks = todos(4);
    tasks[0].status = todo::TodoDisplayItem::Status::Completed;
    tasks[2].status = todo::TodoDisplayItem::Status::InProgress;
    tasks[3].status = todo::TodoDisplayItem::Status::Completed;
    const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity({}, tasks, {}, 100, 12, true), 100, 5).ToString());
    EXPECT_NE(text.find("Tasks 1 active · 2/4 done"), std::string::npos);
    EXPECT_LT(text.find("todo-2"), text.find("todo-0"));
    EXPECT_LT(text.find("todo-0"), text.find("todo-1"));
    EXPECT_LT(text.find("todo-1"), text.find("todo-3"));
    const auto mixed = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(agents(1), tasks, {}, 100, 12), 100, 5).ToString());
    EXPECT_EQ(mixed.find("todo-0"), std::string::npos);
    EXPECT_EQ(mixed.find("todo-3"), std::string::npos);
    EXPECT_LT(mixed.find("todo-2"), mixed.find("todo-1"));
}

TEST(RuntimeAgentActivity, AllCompletedTaskGraceAppliesToTaskOnlyAndMixedAfterLiveWork) {
    auto tasks = todos(3);
    for (auto& task : tasks) task.status = todo::TodoDisplayItem::Status::Completed;
    for (bool mixed : {false, true}) {
        const auto workers = agents(mixed ? 1 : 0);
        auto grace = runtime::RenderRuntimeAgentActivity(workers, tasks, {}, 100, 12, false);
        EXPECT_EQ(required_rows(grace), mixed ? 6 : 4);
        const auto text = strip_ansi(paint(grace, 100, 6).ToString());
        EXPECT_NE(text.find("Tasks 3/3 completed"), std::string::npos);
        EXPECT_EQ(text.find("✓ Tasks"), std::string::npos);
        EXPECT_LT(text.find("todo-0"), text.find("todo-1"));
        EXPECT_LT(text.find("todo-1"), text.find("todo-2"));
        auto folded = runtime::RenderRuntimeAgentActivity(workers, tasks, {}, 100, 12, true);
        EXPECT_EQ(required_rows(folded), mixed ? 3 : 1);
        EXPECT_NE(strip_ansi(paint(folded, 100, 3).ToString()).find("✓ Tasks 3/3 completed"), std::string::npos);
    }
    const auto live_first = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(agents(1), tasks, {}, 100, 3, false), 100, 3).ToString());
    EXPECT_NE(live_first.find("runtime-0"), std::string::npos);
    EXPECT_EQ(live_first.find("todo-"), std::string::npos);
}

TEST(RuntimeAgentActivity, LifecycleStatusRemainsExplicitInItsGroupWithoutPercentClaims) {
    for (const auto status : {runtime::Status::Queued, runtime::Status::Running,
             runtime::Status::Completed, runtime::Status::Failed, runtime::Status::Cancelled}) {
        auto workers = agents(1);
        workers[0].status = status;
        const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(workers, {}, {}, 100, 2), 100, 2).ToString());
        EXPECT_NE(text.find(runtime::is_active(status) ? runtime::status_name(status) : status == runtime::Status::Failed ? "failed" : "ended"), std::string::npos);
        EXPECT_EQ(text.find("runtime-0") != std::string::npos, runtime::is_active(status));
        EXPECT_NE(text.find("/subagents"), std::string::npos);
        EXPECT_EQ(text.find("attention"), std::string::npos);
        EXPECT_EQ(text.find('%'), std::string::npos);
    }
}

TEST(RuntimeAgentActivity, NarrowSelectedHeaderRetainsOwnCountsWithoutCombiningRoles) {
    auto tasks = todos(2);
    tasks[0].status = todo::TodoDisplayItem::Status::InProgress;
    const auto text = strip_ansi(paint(runtime::RenderRuntimeAgentActivity(agents(3), tasks, teams(2), 40, 1), 40, 1).ToString());
    EXPECT_NE(text.find("Tasks 1 active · 0/2 done"), std::string::npos);
    EXPECT_EQ(text.find("Subagents"), std::string::npos);
    EXPECT_EQ(text.find("Agent Teams"), std::string::npos);
}

TEST(RuntimeAgentActivity, CellClippingPreservesZeroNarrowIndentAndCjkControlBoundaries) {
    auto workers = agents(1);
    workers[0].name = "研究\n代理\x1b[31m甲乙丙丁戊己庚辛";
    auto tasks = todos(1);
    tasks[0].status = todo::TodoDisplayItem::Status::InProgress;
    tasks[0].content = "计划\n下一行\x1b[31m甲乙丙丁戊己庚辛";
    auto members = teams(1);
    members[0].name = "团队\n代理\x1b[31m甲乙丙丁戊己庚辛";
    for (const auto prefix : {std::string_view{}, std::string_view{"  ⎿  "}})
        for (const int columns : {0, 1, 2, 3, 5, 6, 7, 8, 23, 40, 80, 120}) for (int budget = 0; budget <= 12; ++budget) {
            SCOPED_TRACE(std::format("prefix={},columns={},budget={}", prefix, columns, budget));
            auto element = runtime::RenderRuntimeAgentActivity(workers, tasks, members, columns, budget, false, prefix);
            EXPECT_LE(required_rows(element), budget);
            const auto screen = paint(vbox({hbox({element, text("|")}), text("SENTINEL")}), std::max(10, columns + 1), budget + 2);
            if (budget > 0) EXPECT_EQ(screen.PixelAt(columns, 0).character, "|");
            EXPECT_NE(strip_ansi(screen.ToString()).find("SENTINEL"), std::string::npos);
            for (int y = 0; y < screen.dimy(); ++y) for (int x = 0; x < screen.dimx(); ++x)
                EXPECT_EQ(screen.PixelAt(x, y).character.find('\x1b'), std::string::npos);
            if (!prefix.empty() && columns <= 2 && budget > 0)
                for (int x = 0; x < columns; ++x) EXPECT_EQ(screen.PixelAt(x, 0).character, " ");
        }
}

namespace {
Screen paint_repl(repl::ReplScreenState& state, int columns = 100, int rows = 30) {
    ActivityEnv cols("COLUMNS", std::to_string(columns));
    ActivityEnv lines("LINES", std::to_string(rows));
    ActivityEnv fullscreen("LOOM_NO_FLICKER", "1");
    auto first = paint(repl::RenderReplScreen(state), columns, rows);
    (void)first;
    return paint(repl::RenderReplScreen(state), columns, rows);
}
int row_of(const Screen& screen, std::string_view label) {
    for (int y = 0; y < screen.dimy(); ++y) {
        std::string row;
        for (int x = 0; x < screen.dimx(); ++x) row += screen.PixelAt(x, y).character;
        if (row.find(label) != std::string::npos) return y;
    }
    return -1;
}
} // namespace

TEST(RuntimeAgentActivity, HiddenParentSpinnerKeepsActualBackgroundQueuedOrRunningStatus) {
    for (const auto status : {runtime::Status::Queued, runtime::Status::Running}) {
        repl::ReplScreenState state;
        state.task_view_store.runtime_agents = agents(1);
        state.task_view_store.runtime_agents[0].background = true;
        state.task_view_store.runtime_agents[0].status = status;
        state.task_view_store.spinner_mode = repl::SpinnerMode::Hidden;
        const auto screen = paint_repl(state);
        const auto text = strip_ansi(screen.ToString());
        EXPECT_NE(text.find(status == runtime::Status::Running
            ? "Background agents running" : "Background agents queued"), std::string::npos);
        EXPECT_EQ(text.find("Thinking"), std::string::npos);
        EXPECT_EQ(text.find("tok/s"), std::string::npos);
        EXPECT_EQ(text.find("esc to interrupt"), std::string::npos);
        EXPECT_FALSE(repl::IsToolAnimating(state.task_view_store));
    }
}

TEST(RuntimeAgentActivity, TeamOnlyRunningStatusDisappearsAfterLastCompletion) {
    repl::ReplScreenState state;
    state.task_view_store.live_teammates = teams(1);
    state.task_view_store.runtime_agents = agents(1);
    state.task_view_store.runtime_agents[0].status = runtime::Status::Completed;
    const auto running = paint_repl(state);
    EXPECT_GE(row_of(running, "Background agents running"), 0);
    state.task_view_store.live_teammates[0].status = "idle";
    const auto completed = paint_repl(state);
    EXPECT_EQ(row_of(completed, "Background agents"), -1);
    EXPECT_GE(row_of(completed, "Subagents 1 ended"), 0);
    EXPECT_GE(row_of(completed, "Agent Teams · 1 agent idle"), 0);
    state.task_view_store.runtime_agents.clear();
    state.task_view_store.live_teammates[0].status = "running";
    EXPECT_GE(row_of(paint_repl(state), "Agent Teams"), 0);
    EXPECT_GE(row_of(paint_repl(state), "Background agents running"), 0);
}

TEST(RuntimeAgentActivity, ForegroundStatusTakesPrecedenceWithoutChangingSpinnerContract) {
    repl::ReplScreenState state;
    state.task_view_store.runtime_agents = agents(1);
    state.task_view_store.runtime_agents[0].background = true;
    state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
    state.task_view_store.spinner_verb = "FOREGROUND-WORK";
    state.query_running = true;
    const auto text = strip_ansi(paint_repl(state).ToString());
    EXPECT_NE(text.find("FOREGROUND-WORK"), std::string::npos);
    EXPECT_EQ(text.find("Background agents"), std::string::npos);
    EXPECT_TRUE(repl::IsToolAnimating(state.task_view_store));
    state.task_view_store.spinner_mode = repl::SpinnerMode::Hidden;
    EXPECT_NE(strip_ansi(paint_repl(state).ToString()).find("Background agents running"), std::string::npos);
}

TEST(RuntimeAgentActivity, NonBackgroundOrdinaryAndAttentionAloneDoNotInventBackgroundWork) {
    repl::ReplScreenState state;
    state.task_view_store.runtime_agents = agents(1);
    EXPECT_EQ(row_of(paint_repl(state), "Background agents"), -1);
    state.task_view_store.runtime_agents[0].background = true;
    state.task_view_store.runtime_agents[0].status = runtime::Status::Failed;
    state.task_view_store.live_teammates = teams(1);
    state.task_view_store.live_teammates[0].status = "unknown";
    EXPECT_EQ(row_of(paint_repl(state), "Background agents"), -1);
    EXPECT_GE(row_of(paint_repl(state), "Subagents 1 ended · 1 failed"), 0);
    EXPECT_GE(row_of(paint_repl(state), "Agent Teams · 1 agent unknown"), 0);
}

TEST(RuntimeAgentActivity, ModalAndCurrentErrorSuppressBackgroundWorkEmphasis) {
    for (bool failure : {false, true}) {
        repl::ReplScreenState state;
        state.task_view_store.live_teammates = teams(1);
        if (failure) {
            repl::MessageDisplayEntry error;
            error.id = "current-model-error";
            error.role = "system";
            error.is_model_error = true;
            error.is_error = true;
            error.content_preview = "Transport failed";
            state.messages_store.messages.push_back(error);
        } else {
            state.mode = repl::ReplMode::SettingsView;
        }
        EXPECT_EQ(row_of(paint_repl(state), "Background agents"), -1);
    }
}

TEST(RuntimeAgentActivity, ClipboardFeedbackSharesForegroundAndBackgroundWorkRowsWithoutMovingEditor) {
    for (bool foreground : {false, true}) for (const int columns : {40, 60, 120}) {
        SCOPED_TRACE(std::format("foreground={},columns={}", foreground, columns));
        repl::ReplScreenState state;
        state.cwd = "/tmp/runtime-activity";
        state.input_text = "EDITOR-MARK";
        state.status_line_enabled = true;
        state.status_line_command = ":";
        state.status_line_text = "FOOTER-MARK";
        state.task_view_store.live_teammates = teams(1);
        if (foreground) {
            state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
            state.task_view_store.spinner_verb = "WORK";
            state.query_running = true;
        }
        const auto before = paint_repl(state, columns);
        const auto work_label = foreground ? "WORK" : "Background agents running";
        const int work_row = row_of(before, work_label);
        ASSERT_GE(work_row, 0);
        state.copy_toast_text = columns == 40 ? "COPIED" : "Copied 5 chars to clipboard";
        const auto after = paint_repl(state, columns);
        EXPECT_EQ(row_of(after, *state.copy_toast_text), work_row);
        EXPECT_EQ(row_of(after, work_label), work_row);
        EXPECT_EQ(row_of(after, "EDITOR-MARK"), row_of(before, "EDITOR-MARK"));
        EXPECT_EQ(row_of(after, "FOOTER-MARK"), row_of(before, "FOOTER-MARK"));
        EXPECT_EQ(state.input_text, "EDITOR-MARK");
    }
}

TEST(RuntimeAgentActivity, ResponseEnvelopeExistsOnlyWithPositiveAllocatedForegroundOrBackgroundWidth) {
    constexpr int columns = 80;
    for (bool foreground : {false, true}) for (int work_columns : {0, 1, 2}) {
        SCOPED_TRACE(std::format("foreground={},work_columns={}", foreground, work_columns));
        repl::ReplScreenState state;
        state.task_view_store.runtime_agents = agents(1);
        state.task_view_store.runtime_agents[0].background = true;
        state.task_view_store.live_teammates = teams(1);
        state.todo_store.items = todos(1);
        state.todo_store.items[0].status = todo::TodoDisplayItem::Status::InProgress;
        state.input_text = "EDITOR-MARK";
        if (foreground) {
            state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
            state.task_view_store.spinner_verb = "WORK";
            state.query_running = true;
        }
        state.copy_toast_text = std::string(columns - 4 - work_columns, 'R');
        const auto screen = paint_repl(state, columns, 60);
        const auto text = strip_ansi(screen.ToString());
        const int task_row = row_of(screen, "Tasks 1 active");
        const int subagent_row = row_of(screen, "Subagents 1 running");
        const int team_row = row_of(screen, "Agent Teams");
        ASSERT_GE(task_row, 0);
        ASSERT_GE(subagent_row, 0);
        ASSERT_GE(team_row, 0);
        const int heading_x = work_columns > 0 ? 5 : 0;
        EXPECT_EQ(screen.PixelAt(heading_x, task_row).character, "T");
        EXPECT_EQ(screen.PixelAt(heading_x, subagent_row).character, "S");
        EXPECT_EQ(screen.PixelAt(heading_x, team_row).character, "A");
        for (const int heading_row : {task_row, subagent_row, team_row}) {
            for (int x = 0; x < heading_x + 2; ++x)
                EXPECT_EQ(screen.PixelAt(x, heading_row + 1).character, " ");
            EXPECT_NE(screen.PixelAt(heading_x + 2, heading_row + 1).character, " ");
        }
        const auto connector = text.find("⎿");
        EXPECT_EQ(connector != std::string::npos, work_columns > 0);
        if (connector != std::string::npos)
            EXPECT_EQ(text.find("⎿", connector + std::string("⎿").size()), std::string::npos);
        EXPECT_GE(row_of(screen, *state.copy_toast_text), 0);
        EXPECT_GE(row_of(screen, "EDITOR-MARK"), 0);
        EXPECT_GE(screen.cursor().y, 0);
        EXPECT_LT(screen.cursor().y, screen.dimy());
    }
}

TEST(RuntimeAgentActivity, OversizedCjkClipboardFeedbackCannotWrapOrConsumeEditorRows) {
    repl::ReplScreenState state;
    state.task_view_store.live_teammates = teams(1);
    state.input_text = "EDITOR-MARK";
    const auto before = paint_repl(state, 40, 24);
    const auto viewport = state.messages_store.scrollable_box;
    state.copy_toast_text = "已复制到剪贴板甲乙丙丁戊己庚辛壬癸甲乙丙丁戊己庚辛壬癸";
    const auto after = paint_repl(state, 40, 24);
    EXPECT_GE(row_of(after, "已复制到剪贴板"), 0);
    EXPECT_EQ(row_of(after, "EDITOR-MARK"), row_of(before, "EDITOR-MARK"));
    EXPECT_EQ(after.cursor().y, before.cursor().y);
    EXPECT_EQ(state.messages_store.scrollable_box.y_max, viewport.y_max);
    EXPECT_EQ(state.messages_store.scrollable_box.y_min, viewport.y_min);
}

TEST(RuntimeAgentActivity, ShortAndNarrowReplKeepsBackgroundStatusEditorFooterAndCaret) {
    for (const auto dimensions : {std::pair{40, 12}, std::pair{60, 24}, std::pair{80, 40}, std::pair{120, 60}}) {
        SCOPED_TRACE(std::format("{}x{}", dimensions.first, dimensions.second));
        repl::ReplScreenState state;
        state.cwd = "/tmp/runtime-activity";
        state.task_view_store.runtime_agents = agents();
        state.todo_store.items = todos();
        state.todo_store.items[0].status = todo::TodoDisplayItem::Status::InProgress;
        state.task_view_store.live_teammates = teams();
        state.status_line_enabled = true;
        state.status_line_command = ":";
        state.status_line_text = "FOOTER-MARK";
        state.input_text = "EDITOR-MARK";
        const auto screen = paint_repl(state, dimensions.first, dimensions.second);
        const auto text = strip_ansi(screen.ToString());
        EXPECT_NE(text.find("EDITOR-MARK"), std::string::npos);
        EXPECT_NE(text.find("Background agents running"), std::string::npos);
        if (dimensions.second < 24) {
            EXPECT_EQ(text.find("FOOTER-MARK"), std::string::npos);
            EXPECT_NE(text.find("? for shortcuts"), std::string::npos);
        } else EXPECT_NE(text.find("FOOTER-MARK"), std::string::npos);
        const auto cursor = screen.cursor();
        EXPECT_GE(cursor.x, 0);
        EXPECT_LT(cursor.x, dimensions.first);
        EXPECT_GE(cursor.y, 0);
        EXPECT_LT(cursor.y, dimensions.second);
    }
}

TEST(RuntimeAgentActivity, FailedRowsFoldAndHistoryHeadingHitTargetsResetForOmittedFrames) {
    auto workers = agents(2);
    workers[0].status = runtime::Status::Failed;
    workers[1].status = runtime::Status::Completed;
    runtime::ActivityHitTargets hits;
    auto element = runtime::RenderRuntimeAgentActivity(workers, {}, teams(1), 60, 4, false, {}, &hits);
    const auto screen = paint(element, 60, 4);
    const auto text = strip_ansi(screen.ToString());
    EXPECT_TRUE(hits.subagents_valid);
    EXPECT_TRUE(hits.teams_valid);
    EXPECT_TRUE(hits.subagents.Contain(1, 0));
    EXPECT_NE(text.find("2 ended · 1 failed · /subagents"), std::string::npos);
    EXPECT_EQ(text.find("runtime-"), std::string::npos);
    EXPECT_EQ(text.find("attention"), std::string::npos);
    (void)runtime::RenderRuntimeAgentActivity(workers, {}, {}, 60, 0, false, {}, &hits);
    EXPECT_FALSE(hits.subagents_valid);
    EXPECT_FALSE(hits.teams_valid);
}

TEST(RuntimeAgentActivity, NarrowHistoryHeadingKeepsCountAndEntryWithoutExpandingFailures) {
    auto workers = agents(2);
    workers[0].status = runtime::Status::Failed;
    workers[1].status = runtime::Status::Cancelled;
    for (const int columns : {40, 60, 120}) {
        auto element = runtime::RenderRuntimeAgentActivity(workers, {}, {}, columns, 12);
        EXPECT_EQ(required_rows(element), 1);
        const auto text = strip_ansi(paint(element, columns, 1).ToString());
        EXPECT_NE(text.find("2 ended"), std::string::npos);
        EXPECT_NE(text.find("1 failed"), std::string::npos);
        EXPECT_NE(text.find("/subagents"), std::string::npos);
        EXPECT_EQ(text.find("runtime-"), std::string::npos);
    }
}
