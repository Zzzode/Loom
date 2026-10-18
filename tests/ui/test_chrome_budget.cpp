#include <cstdlib>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.features.todos.todo_panel;
import loom.ui.chrome.ink_utils;
import loom.ui.features.teams.live_teammates;
import loom.ui.features.agents.runtime_agents;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.task_view_store;
import loom.ui.foundation.theme_provider;
import loom.serdes.json;

namespace {
namespace todo = loom::ui::todos;
namespace team = loom::ui::teams::live;
namespace repl = loom::ui::repl_screen;
using namespace ftxui;

struct EnvironmentGuard {
    std::string name;
    std::optional<std::string> previous;
    EnvironmentGuard(std::string key, std::string value) : name(std::move(key)) {
        if (const auto* existing = std::getenv(name.c_str())) previous = existing;
        setenv(name.c_str(), value.c_str(), 1);
    }
    ~EnvironmentGuard() {
        if (previous) setenv(name.c_str(), previous->c_str(), 1);
        else unsetenv(name.c_str());
    }
};
int height(const Element& element) {
    element->ComputeRequirement();
    return element->requirement().min_y;
}
std::string visible(const Screen& screen) {
    return strip_ansi(screen.ToString());
}
Screen paint(Element element, int columns, int rows) {
    auto screen = Screen::Create(Dimension::Fixed(columns), Dimension::Fixed(rows));
    Render(screen, std::move(element));
    return screen;
}
// Opt-in evidence export uses the same native pixels asserted by these tests.
// Normal test runs create no capture files.
void capture_repl(const Screen& screen, const repl::ReplScreenState& state) {
    const auto* capture_dir = std::getenv("LOOM_UI_CAPTURE_DIR");
    if (!capture_dir || *capture_dir == '\0') return;
    const auto* info = testing::UnitTest::GetInstance()->current_test_info();
    std::string name = std::string(info->test_suite_name()) + "." + info->name();
    for (char& ch : name) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
              || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
            ch = '_';
    }
    name += "-" + std::string(loom::ui::design::theme::variant_name(
        loom::ui::design::theme::current_theme().variant));
    name += std::format("-{}x{}", screen.dimx(), screen.dimy());
    const std::filesystem::path directory(capture_dir);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    ASSERT_FALSE(error) << error.message();
    auto write = [&](std::string_view extension, std::string_view content) {
        std::ofstream output(directory / (name + std::string(extension)),
                             std::ios::binary | std::ios::trunc);
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        EXPECT_TRUE(output.good()) << "Cannot export " << name << extension;
    };
    write(".txt", visible(screen));
    write(".ansi", screen.ToString());
    loom::utils::json::JsonMutDoc document;
    auto root = document.object();
    document.set_root(root);
    root.set("test", std::string_view(name));
    root.set("width", screen.dimx());
    root.set("height", screen.dimy());
    const auto cursor = screen.cursor();
    auto caret = document.object();
    caret.set("x", cursor.x);
    caret.set("y", cursor.y);
    root.add("cursor", caret);
    const auto& box = state.messages_store.scrollable_box;
    auto viewport = document.object();
    viewport.set("x_min", box.x_min);
    viewport.set("x_max", box.x_max);
    viewport.set("y_min", box.y_min);
    viewport.set("y_max", box.y_max);
    viewport.set("rows", std::max(0, box.y_max - box.y_min + 1));
    root.add("viewport", viewport);
    auto pixels = document.array();
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) {
            const auto& pixel = screen.PixelAt(x, y);
            auto item = document.object();
            item.set("x", x);
            item.set("y", y);
            item.set("char", std::string_view(pixel.character));
            item.set("fgPrint", std::string_view(pixel.foreground_color.Print(false)));
            item.set("bgPrint", std::string_view(pixel.background_color.Print(false)));
            item.set("dim", static_cast<bool>(pixel.dim));
            item.set("bold", static_cast<bool>(pixel.bold));
            item.set("inverted", static_cast<bool>(pixel.inverted));
            item.set("strike", static_cast<bool>(pixel.strikethrough));
            pixels.append(item);
        }
    }
    root.add("pixels", pixels);
    write(".json", document.to_pretty_string());
}
Screen paint_repl(repl::ReplScreenState& state, int columns, int rows) {
    EnvironmentGuard cols("COLUMNS", std::to_string(columns));
    EnvironmentGuard lines("LINES", std::to_string(rows));
    EnvironmentGuard fullscreen("LOOM_NO_FLICKER", "1");
    // The second frame consumes the real viewport measured in the first.
    auto first = paint(repl::RenderReplScreen(state), columns, rows);
    (void)first;
    auto final = paint(repl::RenderReplScreen(state), columns, rows);
    capture_repl(final, state);
    return final;
}
void populate(repl::ReplScreenState& state, int tasks = 10, int teammates = 20) {
    state.cwd = "/tmp/budget";
    state.status_line_enabled = true;
    state.status_line_command = ":";
    state.status_line_text = "FOOTER-MARK";
    for (int i = 0; i < tasks; ++i) {
        todo::TodoDisplayItem item;
        item.content = "task-" + std::to_string(i);
        item.status = i == 5 || i == 8 ? todo::TodoDisplayItem::Status::InProgress
                                      : todo::TodoDisplayItem::Status::Pending;
        state.todo_store.items.push_back(std::move(item));
    }
    for (int i = 0; i < teammates; ++i) {
        team::LiveTeammate item;
        item.agent_id = "agent-" + std::to_string(i);
        item.name = "worker-" + std::to_string(i);
        item.status = "running";
        item.color = "cyan";
        item.last_output_tail = "TAIL-" + std::to_string(i);
        state.task_view_store.live_teammates.push_back(std::move(item));
    }
    state.task_view_store.teammate_count = teammates;
}
void expect_caret(const Screen& screen) {
    const auto caret = screen.cursor();
    ASSERT_GE(caret.x, 0);
    ASSERT_LT(caret.x, screen.dimx());
    ASSERT_GE(caret.y, 0);
    ASSERT_LT(caret.y, screen.dimy());
    EXPECT_TRUE(screen.PixelAt(caret.x, caret.y).inverted);
}
int transcript_height(const repl::ReplScreenState& state) {
    const auto& box = state.messages_store.scrollable_box;
    return std::max(0, box.y_max - box.y_min + 1);
}
}  // namespace

TEST(ChromeBudget, HeadlessTerminalDimensionsKeepEnvironmentFallback) {
    EnvironmentGuard cols("COLUMNS", "101");
    EnvironmentGuard lines("LINES", "33");
    EXPECT_EQ(loom::ui::ink_utils::query_terminal_size(), std::make_pair(101, 33));
    setenv("COLUMNS", "0", 1);
    setenv("LINES", "-1", 1);
    EXPECT_EQ(loom::ui::ink_utils::query_terminal_size(), std::make_pair(80, 24));
}

TEST(ChromeBudget, TodoStrikethroughCoversOnlyVisibleLabelCells) {
    struct Case {
        int columns;
        std::string content;
        std::string label;
    };
    for (const auto& sample : std::vector<Case>{
            {80, "Run lifecycle tests", "Run lifecycle tests"},
            {30, "修复删除线范围", "修复删除线范围"},
            {12, "Long task description", "Long task…"},
            {9, "检查任务范围", "检查任…"},
            {2, "Short", ""},
            {1, "Short", ""}}) {
        for (auto status : {todo::TodoDisplayItem::Status::Completed,
                            todo::TodoDisplayItem::Status::Pending,
                            todo::TodoDisplayItem::Status::InProgress}) {
            SCOPED_TRACE(std::format("{} columns, content={}, status={}",
                sample.columns, sample.content, static_cast<int>(status)));
            todo::TodoDisplayItem item;
            item.content = sample.content;
            item.status = status;
            auto screen = paint(todo::RenderTodoRow(item, sample.columns), sample.columns, 1);
            EXPECT_NE(visible(screen).find(sample.label), std::string::npos);
            const int label_end = 2 + string_width(sample.label);
            for (int x = 0; x < sample.columns; ++x) {
                EXPECT_EQ(static_cast<bool>(screen.PixelAt(x, 0).strikethrough),
                    status == todo::TodoDisplayItem::Status::Completed && x >= 2 && x < label_end)
                    << "column " << x;
            }
        }
    }
}

TEST(ChromeBudget, TodoBudgetsZeroThroughFiveIncludeCountsAndStableActivePriority) {
    repl::ReplScreenState state;
    populate(state);
    state.todo_store.items.back().status = todo::TodoDisplayItem::Status::Completed;
    const auto before = state.todo_store.items;
    for (int rows = 0; rows <= 5; ++rows) {
        auto element = todo::RenderTodoPanel(state.todo_store.items, 80, rows);
        EXPECT_EQ(height(element), rows);
        const auto text = visible(paint(element, 80, 5));
        if (rows == 0) {
            EXPECT_EQ(text.find("Tasks"), std::string::npos);
            continue;
        }
        EXPECT_NE(text.find(std::format("Tasks 1/10 done (+{} hidden)", 11 - rows)), std::string::npos);
        if (rows >= 2) EXPECT_NE(text.find("task-5"), std::string::npos);
        if (rows >= 3) EXPECT_LT(text.find("task-5"), text.find("task-8"));
        if (rows >= 4) EXPECT_LT(text.find("task-8"), text.find("task-0"));
    }
    for (std::size_t i = 0; i < before.size(); ++i) {
        EXPECT_EQ(state.todo_store.items[i].content, before[i].content);
        EXPECT_EQ(state.todo_store.items[i].status, before[i].status);
    }
}
TEST(ChromeBudget, TeamBudgetsZeroThroughFiveCountOmittedWithoutReordering) {
    repl::ReplScreenState state;
    populate(state);
    for (int rows = 0; rows <= 5; ++rows) {
        auto element = team::RenderLiveTeammateStrip(state.task_view_store.live_teammates, 80, rows);
        EXPECT_EQ(height(element), rows);
        const auto text = visible(paint(element, 80, 5));
        if (rows == 0) EXPECT_EQ(text.find("Teams"), std::string::npos);
        else EXPECT_NE(text.find(std::format("Teams 20 (+{} hidden)", 21 - rows)), std::string::npos);
        if (rows >= 3) EXPECT_LT(text.find("@worker-0"), text.find("@worker-1"));
    }
    EXPECT_EQ(state.task_view_store.live_teammates.front().agent_id, "agent-0");
    EXPECT_EQ(state.task_view_store.live_teammates.back().agent_id, "agent-19");
}
TEST(ChromeBudget, StandaloneDefaultsAndSmallCompleteTeamsRemainUnbounded) {
    repl::ReplScreenState state;
    populate(state, 10, 2);
    EXPECT_EQ(height(todo::RenderTodoPanel(state.todo_store.items)), 11);
    EXPECT_EQ(height(team::RenderLiveTeammateStrip(state.task_view_store.live_teammates)), 2);
    auto element = team::RenderLiveTeammateStrip(state.task_view_store.live_teammates, 80, 3);
    EXPECT_EQ(height(element), 2);
    EXPECT_EQ(visible(paint(element, 80, 2)).find("Teams"), std::string::npos);
    EXPECT_EQ(height(todo::RenderTodoPanel({}, 80, 4)), 0);
    EXPECT_EQ(height(team::RenderLiveTeammateStrip({}, 80, 4)), 0);
}
TEST(ChromeBudget, UnicodeTruncationUsesCellsAndZeroCapacityIsEmpty) {
    EXPECT_EQ(todo::detail::one_line("中文😀 tail", 0), "");
    EXPECT_EQ(team::one_line_tail("中文😀 tail", 0), "");
    for (int columns = 1; columns <= 16; ++columns) {
        auto task = todo::detail::one_line("中文😀 e\xcc\x81 text", static_cast<std::size_t>(columns));
        auto tail = team::one_line_tail("中文😀 e\xcc\x81 text", static_cast<std::size_t>(columns));
        EXPECT_LE(string_width(task), columns);
        EXPECT_LE(string_width(tail), columns);
        EXPECT_EQ(task, tail);
    }
}
TEST(ChromeBudget, NarrowRowsPreserveGlyphNameStatusAndTailOrder) {
    todo::TodoDisplayItem item;
    item.content = "中文😀 task";
    team::LiveTeammate peer;
    peer.name = "中文😀 very-long-name";
    peer.status = "running";
    peer.last_output_tail = "尾部😀 output";
    for (int columns = 1; columns <= 30; ++columns) {
        auto task = todo::RenderTodoRow(item, columns);
        auto row = team::RenderLiveTeammateRow(peer, columns);
        EXPECT_EQ(height(task), 1);
        EXPECT_EQ(height(row), 1);
        EXPECT_EQ(row->requirement().min_x, columns);
        const auto text = visible(paint(row, columns, 1));
        EXPECT_NE(text.find("●"), std::string::npos);
        if (columns >= 18) {
            EXPECT_LT(text.find("@"), text.find("ru"));
            EXPECT_LT(text.find("ru"), text.find("尾"));
        }
    }
    EXPECT_EQ(visible(paint(todo::RenderTodoRow(item, 1), 1, 1)), "○");
    EXPECT_EQ(visible(paint(team::RenderLiveTeammateRow(peer, 1), 1, 1)), "●");
}
TEST(ChromeBudget, CompletedTextAndLiveTailUseReadableUndimmedForegrounds) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    todo::TodoDisplayItem task;
    task.content = "completed";
    task.status = todo::TodoDisplayItem::Status::Completed;
    auto screen = paint(todo::RenderTodoRow(task, 30), 30, 1);
    EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, pal.muted);
    EXPECT_EQ(screen.PixelAt(2, 0).foreground_color, pal.muted);
    EXPECT_FALSE(screen.PixelAt(2, 0).dim);
    team::LiveTeammate peer;
    peer.name = "a";
    peer.status = "idle";
    peer.last_output_tail = "TAIL";
    auto row = paint(team::RenderLiveTeammateRow(peer, 30), 30, 1);
    for (int column = 0; column < 30; ++column) {
        if (row.PixelAt(column, 0).character == "T") {
            EXPECT_EQ(row.PixelAt(column, 0).foreground_color, pal.muted);
            EXPECT_FALSE(row.PixelAt(column, 0).dim);
        }
    }
}
TEST(ChromeBudget, DenseEightyByTwentyFourKeepsLayeredHeadersAndCurrentTask) {
    repl::ReplScreenState state;
    populate(state);
    auto screen = paint_repl(state, 80, 24);
    EXPECT_GE(transcript_height(state), 12);
    const auto text = visible(screen);
    EXPECT_NE(text.find("Background agents running"), std::string::npos);
    EXPECT_NE(text.find("Tasks 2 active · 0/10 done"), std::string::npos);
    EXPECT_NE(text.find("Agent Teams · 20 agents running"), std::string::npos);
    EXPECT_NE(text.find("task-5"), std::string::npos);
    EXPECT_LT(text.find("Tasks 2 active · 0/10 done"), text.find("task-5"));
    EXPECT_LT(text.find("task-5"), text.find("Agent Teams · 20 agents running"));
    EXPECT_EQ(text.find("@worker-0"), std::string::npos);
    EXPECT_EQ(text.find("hidden"), std::string::npos);
    EXPECT_NE(text.find("FOOTER-MARK"), std::string::npos);
    expect_caret(screen);
    EXPECT_EQ(state.todo_store.items.size(), 10u);
    EXPECT_EQ(state.task_view_store.live_teammates.size(), 20u);
}
TEST(ChromeBudget, NarrowLongAsciiAndUnicodePreserveEightRowsCaretAndFooter) {
    for (bool unicode : {false, true}) {
        repl::ReplScreenState state;
        populate(state);
        for (int i = 0; i < 80; ++i) state.input_text += unicode ? "中文😀" : "abcdefgh";
        const auto before = state.input_text;
        auto screen = paint_repl(state, 40, 24);
        EXPECT_GE(transcript_height(state), 8);
        EXPECT_NE(visible(screen).find("FOOTER-MARK"), std::string::npos);
        expect_caret(screen);
        EXPECT_EQ(state.input_text, before);
    }
}
TEST(ChromeBudget, ResizeRetainsHeldInputBytesCaretAndViewport) {
    repl::ReplScreenState state;
    populate(state);
    state.input_text = std::string(500, 'x') + "中文😀";
    auto wide = paint_repl(state, 80, 24);
    expect_caret(wide);
    const auto held = state.prompt_render_input;
    const auto bytes = state.input_text;
    auto narrow = paint_repl(state, 40, 24);
    expect_caret(narrow);
    EXPECT_GE(transcript_height(state), 8);
    auto resized = paint_repl(state, 80, 30);
    expect_caret(resized);
    EXPECT_EQ(state.prompt_render_input, held);
    EXPECT_EQ(state.input_text, bytes);
}
TEST(ChromeBudget, EmptyActivityConsumesNoRowsAndTinyFixedChromeTakesPriority) {
    repl::ReplScreenState empty;
    populate(empty, 0, 0);
    auto baseline = paint_repl(empty, 80, 24);
    EXPECT_EQ(visible(baseline).find("Tasks"), std::string::npos);
    EXPECT_EQ(visible(baseline).find("Agents"), std::string::npos);
    EXPECT_EQ(visible(baseline).find("Background agents"), std::string::npos);
    EXPECT_NE(visible(baseline).find("FOOTER-MARK"), std::string::npos);
    expect_caret(baseline);
    const int empty_height = transcript_height(empty);
    repl::ReplScreenState dense;
    populate(dense);
    auto populated = paint_repl(dense, 80, 24);
    EXPECT_NE(visible(populated).find("Tasks 2 active · 0/10 done"), std::string::npos);
    EXPECT_NE(visible(populated).find("Agent Teams · 20 agents running"), std::string::npos);
    EXPECT_NE(visible(populated).find("Background agents running"), std::string::npos);
    EXPECT_NE(visible(populated).find("FOOTER-MARK"), std::string::npos);
    expect_caret(populated);
    // Three activity rows plus the static background work row use four rows.
    EXPECT_EQ(empty_height - transcript_height(dense), 4);
    dense.input_text = std::string(400, 'x');
    auto small = paint_repl(dense, 40, 12);
    EXPECT_EQ(visible(small).find("Tasks"), std::string::npos);
    EXPECT_EQ(visible(small).find("Agent Teams · 20 agents running"), std::string::npos);
    EXPECT_NE(visible(small).find("Background agents running"), std::string::npos);
    expect_caret(small);
}
TEST(ChromeBudget, LayeredAgentDetailsUseSpareCapacityAfterCurrentTask) {
    repl::ReplScreenState few_tasks;
    populate(few_tasks, 1, 20);
    auto screen = paint_repl(few_tasks, 80, 30);
    const auto text = visible(screen);
    EXPECT_NE(text.find("Tasks 0/1 done"), std::string::npos);
    EXPECT_NE(text.find("Agent Teams · 20 agents running"), std::string::npos);
    EXPECT_NE(text.find("Agent Teams"), std::string::npos);
    EXPECT_NE(text.find("@worker-2"), std::string::npos);
    EXPECT_EQ(text.find("task-0"), std::string::npos);
    repl::ReplScreenState few_teams;
    populate(few_teams, 10, 1);
    const auto transferred = visible(paint_repl(few_teams, 80, 30));
    EXPECT_NE(transferred.find("Tasks 2 active · 0/10 done"), std::string::npos);
    EXPECT_NE(transferred.find("Agent Teams · 1 agent running"), std::string::npos);
    EXPECT_NE(transferred.find("task-5"), std::string::npos);
    EXPECT_NE(transferred.find("task-8"), std::string::npos);
    EXPECT_NE(transferred.find("task-0"), std::string::npos);
    EXPECT_NE(transferred.find("@worker-0"), std::string::npos);
    EXPECT_LT(transferred.find("task-8"), transferred.find("Agent Teams · 1 agent running"));
    EXPECT_LT(transferred.find("Agent Teams · 1 agent running"), transferred.find("@worker-0"));
    EXPECT_EQ(transferred.find("hidden"), std::string::npos);
}

TEST(ChromeBudget, OneActivityRowUsesCanonicalTieOrderForEquallyActiveGroups) {
    repl::ReplScreenState state;
    populate(state);
    // The static background work row shares the existing twelve-row cap.
    // Five input rows leave one activity row; equally active groups use canonical order.
    state.input_text = std::string(160, 'x');
    auto screen = paint_repl(state, 40, 24);
    const auto text = visible(screen);
    EXPECT_NE(text.find("Background agents running"), std::string::npos);
    EXPECT_EQ(text.find("Agent Teams"), std::string::npos);
    EXPECT_NE(text.find("Tasks 2 active · 0/10 done"), std::string::npos);
    EXPECT_EQ(text.find("hidden"), std::string::npos);
    EXPECT_EQ(text.find("@worker-0"), std::string::npos);
    EXPECT_EQ(text.find("task-5"), std::string::npos);
    EXPECT_GE(transcript_height(state), 8);
    expect_caret(screen);
}

TEST(ChromeBudget, TodoEnvelopeRequiresVisibleWorkRow) {
    for (int columns : {40, 80}) {
        for (int phase : {0, 1, 2}) {
            for (int work : {0, 1, 2}) {
                SCOPED_TRACE(std::format("columns={}, phase={}, work={}", columns, phase, work));
                repl::ReplScreenState state;
                populate(state, 3, 0);
                state.todo_store.items.front().content = "task-0 " + std::string(columns - 10, 'x') + "Z";
                if (phase == 0) {
                    state.todo_store.items.front().status = todo::TodoDisplayItem::Status::InProgress;
                } else {
                    for (auto& item : state.todo_store.items)
                        item.status = todo::TodoDisplayItem::Status::Completed;
                }
                state.todo_store.completed_collapsed = phase == 2;
                state.query_running = work != 0;
                state.task_view_store.spinner_mode = work == 0
                    ? repl::SpinnerMode::Hidden : repl::SpinnerMode::Thinking;
                state.task_view_store.spinner_verb = "Working";
                // An outer focus owner hides work chrome even during a query.
                if (work == 2) state.mode = repl::ReplMode::SettingsView;
                const auto screen = paint_repl(state, columns, 30);
                int header = -1;
                std::string line;
                for (int y = 0; y < screen.dimy(); ++y) {
                    std::string row;
                    for (int x = 0; x < screen.dimx(); ++x)
                        row += screen.PixelAt(x, y).character;
                    if (row.find("Tasks") != std::string::npos) {
                        header = y;
                        line = std::move(row);
                        break;
                    }
                }
                ASSERT_GE(header, 0);
                if (work == 1) {
                    EXPECT_TRUE(line.starts_with("  ⎿  ")) << line;
                    ASSERT_GT(header, 0);
                    std::string parent;
                    for (int x = 0; x < screen.dimx(); ++x)
                        parent += screen.PixelAt(x, header - 1).character;
                    EXPECT_NE(parent.find("Working"), std::string::npos) << parent;
                } else {
                    EXPECT_TRUE(line.starts_with(phase == 2 ? "✓ Tasks" : "Tasks")) << line;
                    EXPECT_EQ(line.find("⎿"), std::string::npos);
                }
                if (phase != 2) {
                    const int indent = work == 1 ? 7 : 2;
                    EXPECT_EQ(screen.PixelAt(indent, header + 1).character, phase == 0 ? "●" : "✓");
                    EXPECT_EQ(screen.PixelAt(indent + 2, header + 1).character, "t");
                    EXPECT_EQ(screen.PixelAt(columns - 1, header + 1).character, "…");
                }
                EXPECT_GE(transcript_height(state), 12);
                expect_caret(screen);
            }
        }
    }
}

TEST(ChromeBudget, OneActivityRowKeepsRunningAgentsAheadOfCompletedTasks) {
    for (bool working : {false, true}) {
        repl::ReplScreenState state;
        populate(state, 3, 2);
        for (auto& item : state.todo_store.items)
            item.status = todo::TodoDisplayItem::Status::Completed;
        state.query_running = working;
        state.task_view_store.spinner_mode = working
            ? repl::SpinnerMode::Thinking : repl::SpinnerMode::Hidden;
        state.task_view_store.spinner_verb = "Working";
        // Foreground and static background work each reserve the same row.
        state.input_text = std::string(160, 'x');
        for (bool collapsed : {false, true}) {
            state.todo_store.completed_collapsed = collapsed;
            const auto screen = paint_repl(state, 40, 24);
            const auto text = visible(screen);
            EXPECT_NE(text.find("Agent Teams 2 running"), std::string::npos);
            EXPECT_NE(text.find("instances"), std::string::npos);
            EXPECT_EQ(text.find("Tasks"), std::string::npos);
            EXPECT_EQ(text.find("hidden"), std::string::npos);
            EXPECT_NE(text.find(working ? "Working" : "Background agents running"), std::string::npos);
            EXPECT_NE(text.find("⎿"), std::string::npos);
            EXPECT_EQ(text.find("task-0"), std::string::npos);
            EXPECT_GE(transcript_height(state), 8);
            expect_caret(screen);
        }
    }
}

TEST(ChromeBudget, CompletedTasksKeepDistinctCompletionHeaderWhenSpaceAllows) {
    for (const bool working : {false, true}) for (const bool collapsed : {false, true}) {
        SCOPED_TRACE(std::format("working={},collapsed={}", working, collapsed));
        repl::ReplScreenState state;
        populate(state, 3, 2);
        for (auto& item : state.todo_store.items) item.status = todo::TodoDisplayItem::Status::Completed;
        state.todo_store.completed_collapsed = collapsed;
        state.query_running = working;
        state.task_view_store.spinner_mode = working ? repl::SpinnerMode::Thinking : repl::SpinnerMode::Hidden;
        state.task_view_store.spinner_verb = "Working";
        const auto screen = paint_repl(state, 80, 30);
        const auto text = visible(screen);
        EXPECT_NE(text.find("Tasks 3/3 completed"), std::string::npos);
        EXPECT_NE(text.find("Agent Teams · 2 agents running"), std::string::npos);
        EXPECT_LT(text.find("Tasks 3/3 completed"), text.find("Agent Teams · 2 agents running"));
        EXPECT_NE(text.find("@worker-0"), std::string::npos);
        EXPECT_EQ(text.find("task-0") != std::string::npos, !collapsed);
        EXPECT_EQ(text.find("✓ Tasks 3/3 completed") != std::string::npos, collapsed);
        EXPECT_EQ(text.find("hidden"), std::string::npos);
        EXPECT_NE(text.find("FOOTER-MARK"), std::string::npos);
        EXPECT_GE(transcript_height(state), 12);
        expect_caret(screen);
    }
}

TEST(ChromeBudget, ActiveSpinnerPreservesTranscriptCaretAndFooterAcrossFrames) {
    repl::ReplScreenState state;
    populate(state);
    state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
    state.task_view_store.spinner_verb = "Working";
    auto normal = paint_repl(state, 80, 24);
    EXPECT_GE(transcript_height(state), 12);
    EXPECT_NE(visible(normal).find("FOOTER-MARK"), std::string::npos);
    EXPECT_NE(visible(normal).find("Tasks"), std::string::npos);
    EXPECT_NE(visible(normal).find("Agent Teams · 20 agents running"), std::string::npos);
    expect_caret(normal);
    state.input_text = std::string(500, 'x') + "中文😀";
    const auto bytes = state.input_text;
    auto narrow = paint_repl(state, 40, 24);
    EXPECT_GE(transcript_height(state), 8);
    EXPECT_NE(visible(narrow).find("FOOTER-MARK"), std::string::npos);
    expect_caret(narrow);
    EXPECT_EQ(state.input_text, bytes);
}

TEST(ChromeBudget, WorkRowAlignsWithPromptAndTasksUseResponseEnvelope) {
    for (const int columns : {40, 80, 120}) {
        repl::ReplScreenState state;
        populate(state, 3, 0);
        state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
        state.task_view_store.spinner_verb = "Working";
        auto screen = paint_repl(state, columns, 30);
        int work_y = -1;
        int header_y = -1;
        for (int y = 0; y < screen.dimy(); ++y) {
            if (screen.PixelAt(2, y).character == "W" &&
                screen.PixelAt(3, y).character == "o") work_y = y;
            if (screen.PixelAt(2, y).character == "⎿") header_y = y;
        }
        ASSERT_GE(work_y, 0);
        ASSERT_EQ(header_y, work_y + 1);
        EXPECT_EQ(screen.PixelAt(0, header_y).character, " ");
        EXPECT_EQ(screen.PixelAt(1, header_y).character, " ");
        EXPECT_EQ(screen.PixelAt(5, header_y).character, "T");
        EXPECT_EQ(screen.PixelAt(2, header_y).foreground_color,
                  loom::ui::design::theme::current_theme().palette->muted);
        for (int row = 1; row <= 3; ++row) {
            for (int x = 0; x < 7; ++x)
                EXPECT_EQ(screen.PixelAt(x, header_y + row).character, " ");
            EXPECT_EQ(screen.PixelAt(7, header_y + row).character, "○");
        }
        // The prompt's horizontal rule starts at the same left edge as work.
        EXPECT_EQ(screen.PixelAt(0, header_y + 4).character, "─");
        expect_caret(screen);
    }
}

TEST(ChromeBudget, AutocompleteHeightLeavesCaretFooterAndTranscriptVisible) {
    repl::ReplScreenState state;
    populate(state);
    state.autocomplete_suggestions = {
        {.display_text = "/help", .description = "Help", .insert_text = "/help "},
        {.display_text = "/history", .description = "History", .insert_text = "/history "},
        {.display_text = "/model", .description = "Model", .insert_text = "/model "},
        {.display_text = "/status", .description = "Status", .insert_text = "/status "},
        {.display_text = "/config", .description = "Selected-last", .insert_text = "/config "},
    };
    state.autocomplete_index = 4;
    state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
    state.task_view_store.spinner_verb = "Working";
    // The default standalone builder keeps five rows plus overlay framing;
    // a finite caller budget changes only the visible projection.
    EXPECT_EQ(height(repl::RenderPromptSuggestions(state, 80, true, 24)), 7);
    auto one = repl::RenderPromptSuggestions(state, 80, true, 24, 1);
    EXPECT_EQ(height(one), 3);
    EXPECT_NE(visible(paint(one, 80, 3)).find("/config"), std::string::npos);
    EXPECT_FALSE(repl::RenderPromptSuggestions(state, 80, true, 24, 0));
    auto wide = paint_repl(state, 80, 24);
    EXPECT_GE(transcript_height(state), 8);
    EXPECT_EQ(visible(wide).find("Tasks"), std::string::npos);
    EXPECT_NE(visible(wide).find("/config"), std::string::npos);
    EXPECT_NE(visible(wide).find("FOOTER-MARK"), std::string::npos);
    expect_caret(wide);
    state.input_text = std::string(300, 'x') + "中文😀";
    const auto bytes = state.input_text;
    auto narrow = paint_repl(state, 40, 24);
    EXPECT_GE(transcript_height(state), 8);
    EXPECT_EQ(visible(narrow).find("Tasks"), std::string::npos);
    EXPECT_NE(visible(narrow).find("/config"), std::string::npos);
    EXPECT_NE(visible(narrow).find("FOOTER-MARK"), std::string::npos);
    expect_caret(narrow);
    EXPECT_EQ(state.input_text, bytes);
    EXPECT_EQ(state.autocomplete_index, 4);
    ASSERT_EQ(state.autocomplete_suggestions.size(), 5u);
    EXPECT_EQ(state.autocomplete_suggestions.front().insert_text, "/help ");
    EXPECT_EQ(state.autocomplete_suggestions.back().insert_text, "/config ");
    // At short heights fixed controls take priority; hiding the popup must
    // leave the complete suggestion list and selection intact.
    auto tiny = paint_repl(state, 40, 12);
    EXPECT_EQ(visible(tiny).find("/config"), std::string::npos);
    EXPECT_EQ(visible(tiny).find("Tasks"), std::string::npos);
    expect_caret(tiny);
    EXPECT_EQ(state.autocomplete_index, 4);
    EXPECT_EQ(state.autocomplete_suggestions.size(), 5u);
    EXPECT_EQ(state.input_text, bytes);
}

TEST(ChromeBudget, AllThemeVariantsRenderDenseBashWithSpinnerAndRestoreTheme) {
    namespace theme = loom::ui::design::theme;
    const auto original = theme::current_theme();
    {
        struct RestoreTheme {
            theme::Theme value;
            ~RestoreTheme() { theme::set_theme(value); }
        } restore{original};
        for (const auto variant : std::array{
            theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
            theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
            theme::ThemeVariant::Monochrome, theme::ThemeVariant::LightAnsi,
            theme::ThemeVariant::DarkAnsi}) {
            SCOPED_TRACE(theme::variant_name(variant));
            theme::set_theme(variant);
            repl::ReplScreenState state;
            populate(state);
            state.prompt_store.input_mode = repl::InputMode::Bash;
            state.input_text = "!echo Loom";
            state.task_view_store.spinner_mode = repl::SpinnerMode::Thinking;
            state.task_view_store.spinner_verb = "Working";
            auto screen = paint_repl(state, 80, 30);
            const auto text = visible(screen);
            EXPECT_NE(text.find("echo Loom"), std::string::npos);
            EXPECT_NE(text.find("Tasks 2 active · 0/10 done"), std::string::npos);
            EXPECT_NE(text.find("Agent Teams · 20 agents running"), std::string::npos);
            EXPECT_NE(text.find("task-5"), std::string::npos);
            EXPECT_NE(text.find("@worker-0"), std::string::npos);
            EXPECT_EQ(text.find("hidden"), std::string::npos);
            EXPECT_NE(text.find("for bash mode"), std::string::npos);
            EXPECT_GE(transcript_height(state), 12);
            expect_caret(screen);
            EXPECT_EQ(state.input_text, "!echo Loom");
            EXPECT_EQ(state.todo_store.items.size(), 10u);
            EXPECT_EQ(state.task_view_store.live_teammates.size(), 20u);
            EXPECT_EQ(theme::current_theme().variant, variant);
            EXPECT_EQ(theme::current_theme().palette, theme::palette_for_variant(variant));
        }
    }
    EXPECT_EQ(theme::current_theme().variant, original.variant);
    EXPECT_EQ(theme::current_theme().palette, original.palette);
    EXPECT_EQ(theme::current_theme().a11y.reduced_motion, original.a11y.reduced_motion);
}

TEST(ChromeBudget, CopyFeedbackSharesWorkRowAndPreservesChromeGeometry) {
    for (int columns : {40, 80, 120}) for (int rows : {24, 40}) {
        for (bool running : {false, true}) for (bool multiline : {false, true}) {
            SCOPED_TRACE(std::format("{}x{}, running={}, multiline={}",
                columns, rows, running, multiline));
            repl::ReplScreenState state;
            populate(state, 2, 1);
            state.input_text = multiline ? "draft\nsecond line" : "draft";
            state.query_running = running;
            if (running) {
                state.task_view_store.spinner_mode = repl::SpinnerMode::Responding;
                state.task_view_store.spinner_verb = "Working";
                state.task_view_store.estimated_generation_tps = 42.1;
            }
            auto before = paint_repl(state, columns, rows);
            const auto viewport = state.messages_store.scrollable_box;
            const auto row_of = [](const Screen& screen, std::string_view label) {
                for (int y = 0; y < screen.dimy(); ++y) {
                    std::string row;
                    for (int x = 0; x < screen.dimx(); ++x)
                        row += screen.PixelAt(x, y).character;
                    if (row.find(label) != std::string::npos) return y;
                }
                return -1;
            };
            const int expected_row = row_of(before, running ? "Working" : "Background agents running");
            ASSERT_GE(expected_row, 0);
            state.copy_toast_text = "Copied 5 chars to clipboard";
            auto after = paint_repl(state, columns, rows);
            ASSERT_EQ(row_of(after, "Copied"), expected_row);
            EXPECT_EQ(row_of(after, "Copied 5 chars to clipboard"), expected_row);
            EXPECT_EQ(state.messages_store.scrollable_box.y_max, viewport.y_max);
            EXPECT_EQ(row_of(after, "FOOTER-MARK"), row_of(before, "FOOTER-MARK"));
            if (columns >= 80) {
                EXPECT_EQ(row_of(after, running ? "Working" : "Background agents running"), expected_row);
                if (running) EXPECT_EQ(row_of(after, "~42.1 tok/s"), expected_row);
            } else {
                // The toast takes priority within forty cells. The remaining
                // work prefix stays in its original row; its full label and
                // throughput are allowed to clip rather than moving chrome.
                EXPECT_EQ(after.PixelAt(2, expected_row).character, running ? "W" : "B");
            }
            for (int y = 0; y < rows; ++y) {
                if (y == expected_row) continue;
                for (int x = 0; x < columns; ++x)
                    EXPECT_EQ(after.PixelAt(x, y).character, before.PixelAt(x, y).character)
                        << "Unrelated cell moved at " << x << ',' << y;
            }
            EXPECT_EQ(state.input_text, multiline ? "draft\nsecond line" : "draft");
            expect_caret(after);
        }
    }
}


TEST(ChromeBudget, ThreePeerGroupsHaveIndentedChildrenWithinMeasuredChrome) {
    for (const auto dimensions : {std::pair{40, 24}, std::pair{60, 30},
                                  std::pair{80, 40}, std::pair{120, 48}}) {
        SCOPED_TRACE(std::format("{}x{}", dimensions.first, dimensions.second));
        repl::ReplScreenState state;
        populate(state, 4, 2);
        state.todo_store.items.front().status = todo::TodoDisplayItem::Status::InProgress;
        state.task_view_store.runtime_agents = {
            {"sub-one", "SUB-ONE", "", loom::ui::agents::runtime::Status::Running, true},
            {"sub-two", "SUB-TWO", "", loom::ui::agents::runtime::Status::Running, true}};
        state.input_text = "EDITOR-MARK";
        const auto screen = paint_repl(state, dimensions.first, dimensions.second);
        const auto text = visible(screen);
        const auto locate = [&](std::string_view label) {
            for (int y = 0; y < screen.dimy(); ++y) {
                std::string row;
                for (int x = 0; x < screen.dimx(); ++x) row += screen.PixelAt(x, y).character;
                if (row.find(label) != std::string::npos) return y;
            }
            return -1;
        };
        const int task_y = locate("Tasks 1 active");
        const int sub_y = locate("Subagents 2 running");
        const int team_y = locate("Agent Teams");
        ASSERT_GE(task_y, 0);
        ASSERT_GT(sub_y, task_y);
        ASSERT_GT(team_y, sub_y);
        EXPECT_EQ(screen.PixelAt(2, task_y).character, "⎿");
        for (const int y : {task_y, sub_y, team_y}) {
            EXPECT_TRUE(screen.PixelAt(5, y).bold);
            if (y != task_y)
                for (int x = 0; x < 5; ++x) EXPECT_EQ(screen.PixelAt(x, y).character, " ");
        }
        if (dimensions.second >= 30) {
            for (const auto label : {"task-0", "SUB-ONE", "@worker-0"}) {
                const int y = locate(label);
                ASSERT_GE(y, 0) << label;
                for (int x = 0; x < 7; ++x) EXPECT_EQ(screen.PixelAt(x, y).character, " ");
            }
            EXPECT_GT(locate("task-0"), task_y);
            EXPECT_LT(locate("task-0"), sub_y);
            EXPECT_GT(locate("SUB-ONE"), sub_y);
            EXPECT_LT(locate("SUB-ONE"), team_y);
            EXPECT_GT(locate("@worker-0"), team_y);
        }
        if (dimensions.second >= 40) {
            EXPECT_GE(locate("task-3"), 0);
            EXPECT_GE(locate("SUB-TWO"), 0);
            EXPECT_GE(locate("@worker-1"), 0);
            EXPECT_GT(team_y - task_y + 3, 5);
        }
        EXPECT_EQ(std::count(text.begin(), text.end(), '\x1b'), 0);
        EXPECT_NE(text.find("EDITOR-MARK"), std::string::npos);
        EXPECT_NE(text.find("FOOTER-MARK"), std::string::npos);
        EXPECT_GE(transcript_height(state), 12);
        EXPECT_LE(dimensions.second - transcript_height(state), dimensions.second / 2 + 3);
        expect_caret(screen);
        EXPECT_EQ(state.todo_store.items.size(), 4u);
        EXPECT_EQ(state.task_view_store.runtime_agents.size(), 2u);
        EXPECT_EQ(state.task_view_store.live_teammates.size(), 2u);
    }
}
