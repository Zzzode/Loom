// Native editing, lifetime, composition, and routing contracts for question panels.
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <gtest/gtest.h>

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.default_renderers;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;

namespace {
namespace dsys = loom::ui::dialogs::system;
namespace drender = loom::ui::dialogs::default_renderers;
namespace repl = loom::ui::repl_screen;
using ftxui::Event;

class QuestionPanel : public ::testing::Test {
protected:
    void SetUp() override {
        drender::register_default_renderers(registry);
        dsys::PromptDialogPayload question;
        question.id = "native-question";
        question.title = "Follow-up question";
        question.prompt_text = "Which framework should I use?";
        question.on_response = [this](std::optional<std::string> value) {
            answers.push_back(std::move(value));
        };
        payload = std::move(question);
        context.theme = {loom::ui::design::theme::ThemeVariant::Light,
                         &loom::ui::design::tokens::palette::light()};
        context.term_cols = 80;
        context.term_rows = 24;
    }

    dsys::PromptDialogPayload& Question() { return std::get<dsys::PromptDialogPayload>(payload); }

    bool Send(const Event& event) { return registry.handle_event(payload, event); }

    ftxui::Element Element(int width = 80) {
        context.term_cols = width;
        return registry.render(payload, context);
    }

    std::string Draw(int width = 80, int height = 16) {
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
        ftxui::Render(screen, Element(width));
        cursor = screen.cursor();
        std::string result;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x)
                result += screen.PixelAt(x, y).character;
            result += '\n';
        }
        return result;
    }

    dsys::DialogRendererRegistry registry;
    dsys::DialogPayloadVariant payload;
    dsys::DialogRenderContext context;
    std::vector<std::optional<std::string>> answers;
    ftxui::Screen::Cursor cursor{0, 0, ftxui::Screen::Cursor::Hidden};
};

TEST_F(QuestionPanel, InputBeforeFirstRenderIsRetainedAndSubmitted) {
    ASSERT_TRUE(Send(Event::Character("early answer")));
    EXPECT_NE(Draw().find("early answer"), std::string::npos);
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"early answer"});
}

TEST_F(QuestionPanel, DefaultValueIsEditableRatherThanAnImmutableChoice) {
    Question().default_value = "gtest";
    (void)Draw();
    EXPECT_TRUE(Send(Event::End));
    EXPECT_TRUE(Send(Event::Backspace));
    EXPECT_TRUE(Send(Event::Character("st")));
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"gtesst"});
}

TEST_F(QuestionPanel, EnterWithoutDefaultSubmitsPresentEmptyString) {
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    ASSERT_TRUE(answers.front().has_value());
    EXPECT_TRUE(answers.front()->empty());
}

TEST_F(QuestionPanel, DeletingDefaultSubmitsEmptyRatherThanRestoringDefault) {
    Question().default_value = "ab";
    EXPECT_TRUE(Send(Event::End));
    EXPECT_TRUE(Send(Event::Backspace));
    EXPECT_TRUE(Send(Event::Backspace));
    EXPECT_NE(Draw().find("Type your answer..."), std::string::npos);
    EXPECT_EQ(cursor.shape, ftxui::Screen::Cursor::BarBlinking);
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{""});
}

TEST_F(QuestionPanel, EscapeReturnsNulloptRatherThanDraftOrDefault) {
    Question().default_value = "original";
    EXPECT_TRUE(Send(Event::Character("changed")));
    EXPECT_TRUE(Send(Event::Escape));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_FALSE(answers.front().has_value());
}

TEST_F(QuestionPanel, DecisionIsOneShotAndLaterKeyboardEventsAreInert) {
    EXPECT_TRUE(Send(Event::Character("accepted")));
    EXPECT_TRUE(Send(Event::Return));
    EXPECT_TRUE(Send(Event::Return));
    EXPECT_TRUE(Send(Event::Escape));
    EXPECT_TRUE(Send(Event::Character("late")));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"accepted"});
}

TEST_F(QuestionPanel, CallbackCanSynchronouslyRemoveItsOwnPayload) {
    auto owner = std::make_unique<dsys::DialogPayloadVariant>(std::move(payload));
    std::get<dsys::PromptDialogPayload>(*owner).on_response =
        [&](std::optional<std::string> value) {
            answers.push_back(std::move(value));
            owner.reset();
        };
    EXPECT_TRUE(registry.handle_event(*owner, Event::Character("reentrant")));
    EXPECT_TRUE(registry.handle_event(*owner, Event::Return));
    EXPECT_FALSE(owner);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"reentrant"});
}

TEST_F(QuestionPanel, RenderAndResizeKeepTheSameEditableState) {
    (void)Draw();
    const auto state = Question().ui_state;
    ASSERT_TRUE(state);
    EXPECT_TRUE(Send(Event::Character("draft survives")));
    for (int width : {40, 120, 80}) {
        EXPECT_NE(Draw(width).find("draft survives"), std::string::npos);
        EXPECT_EQ(Question().ui_state, state);
    }
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"draft survives"});
}

TEST_F(QuestionPanel, UnicodeNavigationAndDeletionOperateOnGlyphs) {
    EXPECT_TRUE(Send(Event::Character("甲🙂乙")));
    EXPECT_TRUE(Send(Event::End));
    EXPECT_TRUE(Send(Event::ArrowLeft));
    EXPECT_TRUE(Send(Event::Backspace));
    EXPECT_TRUE(Send(Event::Delete));
    EXPECT_TRUE(Send(Event::Home));
    EXPECT_TRUE(Send(Event::Character("首")));
    EXPECT_TRUE(Send(Event::End));
    EXPECT_TRUE(Send(Event::Character("尾")));
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"首甲尾"});
}

TEST_F(QuestionPanel, DefaultAndBatchedTextShareSingleLineNormalization) {
    Question().default_value = "甲\r\n乙\t🙂\x01";
    EXPECT_TRUE(Send(Event::End));
    EXPECT_TRUE(Send(Event::Character(std::string{"\rX\nY\tZ"} + "\xe2\x80\xa8" + "A" +
                                      "\xe2\x80\xa9" + "B\x02\x7f")));
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"甲 乙 🙂 X Y Z A B"});
}

TEST_F(QuestionPanel, EditingBoundariesStayLocalAndGlobalControlKeysPassThrough) {
    (void)Draw();
    for (const auto& event :
         {Event::Tab, Event::TabReverse, Event::ArrowUp, Event::ArrowDown, Event::Home, Event::End,
          Event::ArrowLeft, Event::ArrowRight, Event::Backspace, Event::Delete}) {
        EXPECT_TRUE(Send(event)) << event.input();
    }
    for (char control : {'\x03', '\x04', '\x0c'}) {
        EXPECT_FALSE(Send(Event::Character(control)));
    }
    EXPECT_TRUE(answers.empty());
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{""});
}

TEST_F(QuestionPanel, QueuedQuestionsKeepIndependentDrafts) {
    dsys::PromptDialogPayload next;
    next.id = "next-question";
    next.prompt_text = "Another question";
    next.on_response = [this](std::optional<std::string> value) {
        answers.push_back(std::move(value));
    };
    dsys::DialogPayloadVariant other{std::move(next)};
    EXPECT_TRUE(Send(Event::Character("first")));
    EXPECT_TRUE(registry.handle_event(other, Event::Character("second")));
    EXPECT_NE(Question().ui_state, std::get<dsys::PromptDialogPayload>(other).ui_state);
    EXPECT_TRUE(Send(Event::Return));
    EXPECT_TRUE(registry.handle_event(other, Event::Return));
    ASSERT_EQ(answers.size(), 2U);
    EXPECT_EQ(answers[0], std::optional<std::string>{"first"});
    EXPECT_EQ(answers[1], std::optional<std::string>{"second"});
}

TEST_F(QuestionPanel, ReturnedDomKeepsEditorAliveAfterPayloadIsDestroyed) {
    EXPECT_TRUE(Send(Event::Character("retained DOM")));
    auto element = Element();
    std::weak_ptr<void> lifetime = Question().ui_state;
    ASSERT_FALSE(lifetime.expired());
    payload = dsys::CostThresholdPayload{};
    EXPECT_FALSE(lifetime.expired());
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(16));
    ftxui::Render(screen, element);
    EXPECT_NE(screen.ToString().find("retained DOM"), std::string::npos);
    element.reset();
    EXPECT_TRUE(lifetime.expired());
}

TEST_F(QuestionPanel, OverlayClearsUnderlyingDividersOnlyWithinItsOwnBounds) {
    for (const int width : {40, 80, 120}) {
        SCOPED_TRACE(width);
        context.term_cols = width - 4;
        ftxui::Elements rows;
        for (int row = 0; row < 16; ++row)
            rows.push_back(ftxui::separator());
        auto underlay = ftxui::vbox(std::move(rows));
        auto panel =
            registry.render(payload, context) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, width - 4);
        auto margin = ftxui::filler() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2);
        auto overlay = ftxui::hbox(
            {margin, panel, ftxui::filler() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 2)});
        auto screen =
            ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(16));
        ftxui::Render(screen, ftxui::dbox({underlay, overlay}));
        bool found_question = false;
        for (int y = 0; y < 16; ++y) {
            std::string row;
            for (int x = 0; x < width; ++x)
                row += screen.PixelAt(x, y).character;
            if (row.find("Which framework should I use?") != std::string::npos) {
                found_question = true;
                EXPECT_EQ(screen.PixelAt(0, y).character, "─");
                EXPECT_EQ(screen.PixelAt(width - 1, y).character, "─");
                EXPECT_EQ(row.find("Which─framework"), std::string::npos);
            }
        }
        EXPECT_TRUE(found_question) << screen.ToString();
    }
}

TEST_F(QuestionPanel, NativeCursorMarksTheEndOfTheFocusedAnswerAtEveryWidth) {
    const std::string placeholder = "Type your answer...";
    for (int width : {40, 80, 120}) {
        SCOPED_TRACE(width);
        std::istringstream rows(Draw(width));
        std::string row;
        int y = 0;
        bool found = false;
        while (std::getline(rows, row)) {
            const auto start = row.find(placeholder);
            if (start != std::string::npos) {
                found = true;
                EXPECT_EQ(cursor.shape, ftxui::Screen::Cursor::BarBlinking);
                EXPECT_EQ(cursor.x, ftxui::string_width(row.substr(0, start)));
                EXPECT_EQ(cursor.y, y);
                EXPECT_GE(cursor.x, 3);
                EXPECT_LT(cursor.x, width - 3);
            }
            ++y;
        }
        EXPECT_TRUE(found);
    }
    const std::string answer = "caret answer";
    EXPECT_TRUE(Send(Event::Character(answer)));
    for (int width : {40, 80, 120}) {
        SCOPED_TRACE(width);
        std::istringstream rows(Draw(width));
        std::string row;
        int y = 0;
        bool found = false;
        while (std::getline(rows, row)) {
            const auto start = row.find(answer);
            if (start != std::string::npos) {
                found = true;
                EXPECT_EQ(cursor.shape, ftxui::Screen::Cursor::BarBlinking);
                EXPECT_EQ(cursor.x, ftxui::string_width(row.substr(0, start + answer.size())));
                EXPECT_EQ(cursor.y, y);
                EXPECT_GE(cursor.x, 3);
                EXPECT_LT(cursor.x, width - 3);
            }
            ++y;
        }
        EXPECT_TRUE(found);
    }
}

TEST_F(QuestionPanel, LongAnswerScrollsHorizontallyAndSurvivesResize) {
    const std::string answer = std::string(140, 'a') + "VISIBLE-TAIL";
    EXPECT_TRUE(Send(Event::Character(answer)));
    EXPECT_TRUE(Send(Event::End));
    for (int width : {40, 80, 120, 40}) {
        SCOPED_TRACE(width);
        const auto rendered = Draw(width);
        EXPECT_NE(rendered.find("VISIBLE-TAIL"), std::string::npos);
        EXPECT_EQ(cursor.shape, ftxui::Screen::Cursor::BarBlinking);
        EXPECT_GE(cursor.x, 3);
        EXPECT_LT(cursor.x, width - 3);
        EXPECT_GE(cursor.y, 0);
        EXPECT_LT(cursor.y, 16);
        std::istringstream rows(rendered);
        std::string cursor_row;
        for (int y = 0; y <= cursor.y; ++y)
            std::getline(rows, cursor_row);
        EXPECT_NE(cursor_row.find("VISIBLE-TAIL"), std::string::npos);
    }
    EXPECT_TRUE(Send(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{answer});
}

TEST_F(QuestionPanel, VisibleQuestionConsumesInputWithoutChangingReplDraftFlags) {
    auto state = std::make_shared<repl::ReplScreenState>();
    drender::register_default_renderers(state->dialog_store.dialog_renderers);
    state->dialog_store.dialog_queue.push(std::move(payload));
    auto component = repl::ReplScreen(state, {});
    EXPECT_TRUE(component->OnEvent(Event::Character("actual answer")));
    EXPECT_TRUE(component->OnEvent(Event::Tab));
    EXPECT_TRUE(component->OnEvent(Event::TabReverse));
    EXPECT_TRUE(state->input_text.empty());
    EXPECT_FALSE(state->is_prompt_input_active);
    EXPECT_TRUE(component->OnEvent(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"actual answer"});
}

TEST_F(QuestionPanel, SettingsSuppressionPreservesQuestionDraftAndRouting) {
    auto state = std::make_shared<repl::ReplScreenState>();
    drender::register_default_renderers(state->dialog_store.dialog_renderers);
    EXPECT_TRUE(Send(Event::Character("saved answer")));
    state->dialog_store.dialog_queue.push(std::move(payload));
    auto component = repl::ReplScreen(state, {});
    state->mode = repl::ReplMode::SettingsView;
    EXPECT_TRUE(component->OnEvent(Event::Character("hidden")));
    EXPECT_TRUE(answers.empty());
    state->mode = repl::ReplMode::Normal;
    EXPECT_TRUE(component->OnEvent(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"saved answer"});
    EXPECT_TRUE(state->input_text.empty());
}

TEST_F(QuestionPanel, TypingSuppressionKeepsQuestionDraftSeparateFromReplDraft) {
    auto state = std::make_shared<repl::ReplScreenState>();
    drender::register_default_renderers(state->dialog_store.dialog_renderers);
    EXPECT_TRUE(Send(Event::Character("saved answer")));
    state->dialog_store.dialog_queue.push(std::move(payload));
    state->is_prompt_input_active = true;
    auto component = repl::ReplScreen(state, {});
    EXPECT_TRUE(component->OnEvent(Event::Character("repl draft")));
    EXPECT_EQ(state->input_text, "repl draft");
    EXPECT_TRUE(answers.empty());
    state->input_text.clear();
    state->is_prompt_input_active = false;
    EXPECT_TRUE(component->OnEvent(Event::Return));
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front(), std::optional<std::string>{"saved answer"});
    EXPECT_TRUE(state->input_text.empty());
}
}  // namespace
