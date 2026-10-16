// RFC 0020: completion grace is UI state; completed task data outlives it.
#include <cstdlib>
#include <gtest/gtest.h>
#include <ftxui/component/component.hpp>
#include <ftxui/screen/screen.hpp>
#include "test_ui_helpers.h"

import std;
import loom.ui.app.app;
import loom.ui.screens.repl_state;
import loom.ui.screens.repl_screen;
import loom.ui.features.todos.todo_panel;
import loom.ui.foundation.clock;
import loom.query.query_engine;
import loom.tools.todo_write;
import loom.tools.tool;
import loom.commands.registry;

namespace {
using namespace std::chrono_literals;
namespace ui = loom::ui;
namespace tools = loom::tools;

class TodoCompletion : public ::testing::Test {
protected:
    void SetUp() override {
        tools::clear_all_todos_for_testing();
        storage = std::filesystem::temp_directory_path() /
            ("loom-todo-completion-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        loom::core::QueryEngineConfig config;
        config.cwd = std::filesystem::temp_directory_path().string();
        config.context_window.auto_compact = false;
        engine = std::make_unique<loom::core::QueryEngine>(std::move(config), registry);
        app = ftxui::Make<ui::AppAdapter>(engine.get(), nullptr, &commands, storage, [] {});
        At(0ms);
    }
    void TearDown() override {
        app.reset();
        engine.reset();
        tools::clear_all_todos_for_testing();
        ui::clock::set_steady_now_for_testing(std::nullopt);
        std::error_code ignored;
        std::filesystem::remove_all(storage, ignored);
    }
    void At(std::chrono::milliseconds offset) {
        ui::clock::set_steady_now_for_testing(std::chrono::steady_clock::time_point(100s) + offset);
    }
    void Write(tools::TodoStatus status, std::string content = "Inspect source") {
        tools::TodoWriteRequest request;
        for (const auto& [id, label] : std::vector<std::pair<std::string, std::string>>{
                {"a", std::move(content)}, {"b", "Run native checks"}, {"c", "Record results"}}) {
            tools::TodoItem item;
            item.id = id;
            item.content = label;
            item.status = id == "a" ? status : tools::TodoStatus::Completed;
            request.items.push_back(std::move(item));
        }
        auto result = tools::todo_write_store().execute(std::move(request));
        ASSERT_TRUE(result);
    }
    void Drain() { (void)app->Render(); }
    const auto& State() { return ui::test_seams(app).screen_state_for_testing()->todo_store; }
    loom::core::ToolRegistry registry;
    loom::commands::AppCommandRegistry commands;
    std::unique_ptr<loom::core::QueryEngine> engine;
    std::shared_ptr<ui::AppAdapter> app;
    std::filesystem::path storage;
};
}

TEST_F(TodoCompletion, IdleCompletionCollapsesAtGraceBoundaryWithoutDeletingData) {
    Write(tools::TodoStatus::InProgress);
    Drain();
    EXPECT_FALSE(State().completed_at);
    EXPECT_FALSE(State().completed_collapsed);
    At(1s);
    Write(tools::TodoStatus::Completed);
    Drain();
    const auto observed = State().completed_at;
    ASSERT_TRUE(observed);
    At(3999ms);
    Drain();
    EXPECT_EQ(State().completed_at, observed);
    EXPECT_FALSE(State().completed_collapsed);
    At(4s);
    Drain();
    EXPECT_TRUE(State().completed_collapsed);
    EXPECT_EQ(State().items.size(), 3u);
    EXPECT_EQ(tools::todo_write_store().get_items().size(), 3u);
}

TEST_F(TodoCompletion, ReopenedChangedAndExplicitlyClearedListsResetGrace) {
    Write(tools::TodoStatus::Completed);
    Drain();
    At(3s);
    Drain();
    ASSERT_TRUE(State().completed_collapsed);
    Write(tools::TodoStatus::Pending);
    Drain();
    EXPECT_FALSE(State().completed_at);
    EXPECT_FALSE(State().completed_collapsed);
    At(4s);
    Write(tools::TodoStatus::Completed);
    Drain();
    At(6s);
    Write(tools::TodoStatus::Completed, "Inspect changed source");
    Drain();
    At(7s);
    Drain();
    EXPECT_FALSE(State().completed_collapsed);
    At(9s);
    Drain();
    EXPECT_TRUE(State().completed_collapsed);
    ASSERT_TRUE(tools::todo_write_store().execute({}));
    Drain();
    EXPECT_TRUE(State().items.empty());
    EXPECT_FALSE(State().completed_at);
    EXPECT_FALSE(State().completed_collapsed);
}

TEST_F(TodoCompletion, CompletionRenderUsesOneSummaryRowAfterGrace) {
    Write(tools::TodoStatus::Completed);
    Drain();
    auto render = [&](int columns, bool collapsed) {
        auto element = ui::todos::RenderTodoPanel(State().items, columns, 4, collapsed);
        element->ComputeRequirement();
        EXPECT_EQ(element->requirement().min_y, collapsed ? 1 : 4);
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(columns),
            ftxui::Dimension::Fixed(4));
        ftxui::Render(screen, element);
        auto text = strip_ansi(screen.ToString());
        EXPECT_NE(text.find("Tasks 3/3 completed"), std::string::npos);
        EXPECT_EQ(text.find("Inspect source") != std::string::npos, !collapsed);
    };
    for (int columns : {35, 75, 115}) render(columns, false);
    At(3s);
    Drain();
    for (int columns : {35, 75, 115}) render(columns, State().completed_collapsed);
}

TEST_F(TodoCompletion, ClearResetsDefaultCompletionWithoutClearingAgentTasks) {
    Write(tools::TodoStatus::Completed);
    tools::TodoWriteRequest agent_request;
    agent_request.agent_id = "independent-agent";
    tools::TodoItem item;
    item.id = "agent-work";
    item.content = "Agent work";
    agent_request.items.push_back(std::move(item));
    ASSERT_TRUE(tools::todo_write_store().execute(std::move(agent_request)));
    Drain();
    At(3s);
    Drain();
    ASSERT_TRUE(State().completed_collapsed);
    app->HandleCommand("/clear");
    Drain();
    EXPECT_TRUE(State().items.empty());
    EXPECT_FALSE(State().completed_at);
    EXPECT_FALSE(State().completed_collapsed);
    EXPECT_EQ(tools::todo_count_for_agent("independent-agent"), 1u);
}
