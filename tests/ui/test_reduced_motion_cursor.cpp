/// @file test_reduced_motion_cursor.cpp
/// @brief Native terminal-cursor accessibility at the existing app root.
#include <cstdlib>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.declared_cursor;
import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;

namespace {
namespace theme = loom::ui::design::theme;
namespace dc = loom::ui::common::declared_cursor;
namespace repl = loom::ui::repl_screen;
namespace dialogs = loom::ui::dialogs::system;
namespace renderers = loom::ui::dialogs::default_renderers;
using namespace ftxui;

struct RestoreTheme {
    theme::Theme original = theme::current_theme();
    ~RestoreTheme() { theme::set_theme(original); }
};
struct EnvironmentGuard {
    std::string key;
    std::optional<std::string> original;
    EnvironmentGuard(std::string name, std::string value) : key(std::move(name)) {
        if (const auto* old = std::getenv(key.c_str())) original = old;
        setenv(key.c_str(), value.c_str(), 1);
    }
    ~EnvironmentGuard() {
        if (original) setenv(key.c_str(), original->c_str(), 1);
        else unsetenv(key.c_str());
    }
};
void Motion(bool reduced) {
    auto current = theme::current_theme();
    current.a11y.reduced_motion = reduced;
    theme::set_theme(current);
}
Screen PaintRoot(Element element, int width = 80, int height = 24) {
    auto screen = Screen::Create(Dimension::Fixed(width), Dimension::Fixed(height));
    // AppAdapter::Render uses this same existing cursor-reset root.
    Render(screen, std::move(element) | dc::cursor_reset());
    return screen;
}
void ExpectPosition(Screen::Cursor a, Screen::Cursor b) {
    EXPECT_EQ(a.x, b.x);
    EXPECT_EQ(a.y, b.y);
}
std::string Text(const Screen& screen) {
    std::string value;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x < screen.dimx(); ++x) value += screen.PixelAt(x, y).character;
        value += '\n';
    }
    return value;
}
}

TEST(ReducedMotionCursor, RootConvertsOnlyBlinkingShapesAfterChildChoosesPosition) {
    RestoreTheme restore;
    const std::array<std::pair<Screen::Cursor::Shape, Screen::Cursor::Shape>, 7> cases{{
        {Screen::Cursor::BlockBlinking, Screen::Cursor::Block},
        {Screen::Cursor::UnderlineBlinking, Screen::Cursor::Underline},
        {Screen::Cursor::BarBlinking, Screen::Cursor::Bar},
        {Screen::Cursor::Hidden, Screen::Cursor::Hidden},
        {Screen::Cursor::Block, Screen::Cursor::Block},
        {Screen::Cursor::Underline, Screen::Cursor::Underline},
        {Screen::Cursor::Bar, Screen::Cursor::Bar},
    }};
    for (const auto [shape, expected] : cases) {
        SCOPED_TRACE(static_cast<int>(shape));
        const auto content = [&] {
            return vbox({text("offset"), hbox({text("   "),
                vbox({text("first"), text("middle"), text("last")})
                    | dc::declared_cursor(true, 2, 1, shape)})});
        };
        Motion(false);
        const auto normal = PaintRoot(content());
        EXPECT_EQ(normal.cursor().shape, shape);
        EXPECT_EQ(normal.cursor().x, 5);
        EXPECT_EQ(normal.cursor().y, 2);
        Motion(true);
        const auto quiet = PaintRoot(content());
        EXPECT_EQ(quiet.cursor().shape, expected);
        ExpectPosition(normal.cursor(), quiet.cursor());
        EXPECT_EQ(Text(normal), Text(quiet));
    }
    const auto idle = PaintRoot(text("idle"));
    EXPECT_EQ(idle.cursor().shape, Screen::Cursor::Hidden);
    EXPECT_EQ(idle.cursor().x, 79);
    EXPECT_EQ(idle.cursor().y, 23);
}
TEST(ReducedMotionCursor, ActualInputKeepsOffsetMiddleUnicodeCursorAndEditing) {
    RestoreTheme restore;
    std::string content = "ab中文cd";
    int position = 5;  // Before 文: two ASCII cells plus one full-width glyph.
    InputOption options;
    options.multiline = false;
    options.cursor_position = &position;
    auto input = Input(&content, "placeholder", options);
    auto component = Renderer(input, [&] {
        return vbox({text("offset"), hbox({text("   "),
            input->Render() | size(WIDTH, EQUAL, 30)})});
    });
    Motion(false);
    const auto normal = PaintRoot(component->Render());
    EXPECT_EQ(normal.cursor().shape, Screen::Cursor::BarBlinking);
    EXPECT_EQ(normal.cursor().x, 7);
    EXPECT_EQ(normal.cursor().y, 1);
    Motion(true);
    const auto quiet = PaintRoot(component->Render());
    EXPECT_EQ(quiet.cursor().shape, Screen::Cursor::Bar);
    ExpectPosition(normal.cursor(), quiet.cursor());
    EXPECT_EQ(Text(normal), Text(quiet));
    EXPECT_EQ(position, 5);
    EXPECT_EQ(content, "ab中文cd");
    ASSERT_TRUE(component->OnEvent(Event::Character("X")));
    EXPECT_EQ(content, "ab中X文cd");
    EXPECT_EQ(position, 6);
    const auto edited = PaintRoot(component->Render());
    EXPECT_EQ(edited.cursor().shape, Screen::Cursor::Bar);
    EXPECT_EQ(edited.cursor().x, quiet.cursor().x + 1);
    EXPECT_EQ(edited.cursor().y, quiet.cursor().y);
    ASSERT_TRUE(component->OnEvent(Event::ArrowLeft));
    EXPECT_EQ(position, 5);
    ASSERT_TRUE(component->OnEvent(Event::ArrowLeft));
    EXPECT_EQ(position, 2);
    const auto moved = PaintRoot(component->Render());
    EXPECT_EQ(moved.cursor().x, 5);
    EXPECT_EQ(moved.cursor().shape, Screen::Cursor::Bar);
    Motion(false);
    const auto resumed = PaintRoot(component->Render());
    EXPECT_EQ(resumed.cursor().shape, Screen::Cursor::BarBlinking);
    ExpectPosition(moved.cursor(), resumed.cursor());
}
TEST(ReducedMotionCursor, UnfocusedNativeInputLeavesRootHiddenRatherThanTakingCursor) {
    RestoreTheme restore;
    std::string content = "answer";
    auto input = Input(&content, "placeholder");
    auto button = Button("another owner", [] {});
    auto container = Container::Vertical({input, button});
    container->SetActiveChild(button);
    Motion(false);
    const auto normal = PaintRoot(container->Render());
    EXPECT_EQ(normal.cursor().shape, Screen::Cursor::Hidden);
    EXPECT_FALSE(input->Focused());
    Motion(true);
    const auto quiet = PaintRoot(container->Render());
    EXPECT_EQ(quiet.cursor().shape, Screen::Cursor::Hidden);
    ExpectPosition(normal.cursor(), quiet.cursor());
    EXPECT_EQ(Text(normal), Text(quiet));
    EXPECT_NE(Text(quiet).find("answer"), std::string::npos);
    EXPECT_FALSE(input->Focused());
    EXPECT_EQ(content, "answer");
}
TEST(ReducedMotionCursor, ActualQueuedQuestionUsesSteadyNativeCursorWithoutChangingDraftOrDispatch) {
    RestoreTheme restore;
    EnvironmentGuard columns("COLUMNS", "80");
    EnvironmentGuard lines("LINES", "24");
    EnvironmentGuard fullscreen("LOOM_NO_FLICKER", "1");
    auto state = std::make_shared<repl::ReplScreenState>();
    state->cwd = "/tmp/motion";
    renderers::register_default_renderers(state->dialog_store.dialog_renderers);
    std::vector<std::optional<std::string>> answers;
    dialogs::PromptDialogPayload question;
    question.id = "motion-native-question";
    question.title = "Question";
    question.prompt_text = "Which implementation?";
    question.on_response = [&](std::optional<std::string> value) { answers.push_back(std::move(value)); };
    state->dialog_store.dialog_queue.push(std::move(question));
    auto component = repl::ReplScreen(state, {});
    const auto draw = [&] {
        (void)PaintRoot(component->Render());
        return PaintRoot(component->Render());
    };
    Motion(false);
    const auto normal = draw();
    EXPECT_EQ(normal.cursor().shape, Screen::Cursor::BarBlinking);
    EXPECT_NE(Text(normal).find("Type your answer..."), std::string::npos);
    Motion(true);
    const auto quiet = draw();
    EXPECT_EQ(quiet.cursor().shape, Screen::Cursor::Bar);
    ExpectPosition(normal.cursor(), quiet.cursor());
    EXPECT_TRUE(answers.empty());
    ASSERT_TRUE(component->OnEvent(Event::Character("ab中文cd")));
    ASSERT_TRUE(component->OnEvent(Event::ArrowLeft));
    ASSERT_TRUE(component->OnEvent(Event::ArrowLeft));
    const auto middle = draw();
    EXPECT_EQ(middle.cursor().shape, Screen::Cursor::Bar);
    EXPECT_GE(middle.cursor().x, 3);
    EXPECT_LT(middle.cursor().x, 77);
    ASSERT_TRUE(component->OnEvent(Event::Character("X")));
    const auto edited = draw();
    EXPECT_EQ(edited.cursor().shape, Screen::Cursor::Bar);
    EXPECT_EQ(edited.cursor().x, middle.cursor().x + 1);
    EXPECT_EQ(edited.cursor().y, middle.cursor().y);
    EXPECT_NE(Text(edited).find("ab中文Xcd"), std::string::npos);
    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(state->input_text.empty());
    ASSERT_TRUE(component->OnEvent(Event::Return));
    ASSERT_EQ(answers.size(), 1u);
    EXPECT_EQ(answers.front(), std::optional<std::string>("ab中文Xcd"));
    EXPECT_TRUE(state->input_text.empty());
}
