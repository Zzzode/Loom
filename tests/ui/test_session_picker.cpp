/// @file test_session_picker.cpp
/// @brief Session picker dialog tests: filtering, navigation, rendering.
/// Split from test_dialog_system.cpp (SLOC budget).

#include <cstdlib>

#include <gtest/gtest.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>

import std;
import loom.ui.dialogs.system;
import loom.ui.dialogs.frame;
import loom.ui.dialogs.default_renderers;
import loom.ui.dialogs.modal_renderers;
import loom.ui.dialogs.bottom_renderers;
import loom.ui.dialogs.all_renderers;
import loom.ui.dialogs.triggers;
import loom.ui.dialogs.quick_open;
import loom.ui.dialogs.session_picker;
import loom.ui.dialogs.sandbox_permission;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_tokens;
import loom.constants.product;

namespace {

namespace dsys = loom::ui::dialogs::system;

// ============================================================
// SessionPicker test helpers
// ============================================================

dsys::SessionPickerPayload MakePickerPayload(
    std::vector<dsys::SessionPickerEntry> sessions,
    std::function<void(const std::string&)> on_select) {
    dsys::SessionPickerPayload p;
    p.id = "test-session-picker";
    p.query = "";
    p.sessions = std::move(sessions);
    p.selected_index = 0;
    p.on_select = std::move(on_select);
    return p;
}

std::vector<dsys::SessionPickerEntry> MakeTestSessions() {
    return {
        {.session_id = "aaa11111", .title = "Fix the login bug",
         .age_string = "2h ago", .message_count = 10, .cwd = "/home/user/project-a", .model = "test"},
        {.session_id = "bbb22222", .title = "Add dark mode",
         .age_string = "5h ago", .message_count = 25, .cwd = "/home/user/project-b", .model = "test"},
        {.session_id = "ccc33333", .title = "Refactor parser",
         .age_string = "1d ago", .message_count = 8, .cwd = "/home/user/project-c", .model = "test"},
    };
}

} // namespace

TEST(SessionPicker, FiltersByTitle) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.query = "login";
    auto filtered = loom::ui::dialogs::session_picker::filter_sessions(p.sessions, p.query);
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].session_id, "aaa11111");

    p.query = "dark";
    filtered = loom::ui::dialogs::session_picker::filter_sessions(p.sessions, p.query);
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].session_id, "bbb22222");

    p.query = "nonexistent";
    filtered = loom::ui::dialogs::session_picker::filter_sessions(p.sessions, p.query);
    EXPECT_TRUE(filtered.empty());
}

TEST(SessionPicker, FiltersByCwd) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.query = "project-b";
    auto filtered = loom::ui::dialogs::session_picker::filter_sessions(p.sessions, p.query);
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].session_id, "bbb22222");
}

TEST(SessionPicker, EmptyQueryShowsAll) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    auto filtered = loom::ui::dialogs::session_picker::filter_sessions(p.sessions, p.query);
    EXPECT_EQ(filtered.size(), 3u);
}

TEST(SessionPicker, NavigatesUpDown) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    // Down wraps
    EXPECT_EQ(p.selected_index, 0);
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(p.selected_index, 1);
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(p.selected_index, 2);
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(p.selected_index, 0);  // wrap to top

    // Up wraps
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(p.selected_index, 2);  // wrap to bottom
}

TEST(SessionPicker, NavigatesJK) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Character('j')));
    EXPECT_EQ(p.selected_index, 1);
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Character('k')));
    EXPECT_EQ(p.selected_index, 0);
}

TEST(SessionPicker, EnterSelects) {
    auto sessions = MakeTestSessions();
    std::string selected_id;
    auto p = MakePickerPayload(sessions, [&](const std::string& id) { selected_id = id; });

    p.selected_index = 1;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Return));
    EXPECT_EQ(selected_id, "bbb22222");
}

TEST(SessionPicker, EscapeCancels) {
    auto sessions = MakeTestSessions();
    std::string selected_id = "untouched";
    auto p = MakePickerPayload(sessions, [&](const std::string& id) { selected_id = id; });

    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Escape));
    EXPECT_TRUE(selected_id.empty());
}

TEST(SessionPicker, TypingFiltersAndResetsSelection) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.selected_index = 2;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Character('l')));
    EXPECT_EQ(p.query, "l");
    EXPECT_EQ(p.selected_index, 0);  // reset on filter change
}

TEST(SessionPicker, BackspaceEditsQuery) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.query = "log";
    p.selected_index = 1;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Backspace));
    EXPECT_EQ(p.query, "lo");
    EXPECT_EQ(p.selected_index, 0);  // reset on filter change
}

TEST(SessionPicker, PageUpDown) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.selected_index = 2;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::PageUp));
    EXPECT_EQ(p.selected_index, 0);  // 2 - 8 clamped to 0

    p.selected_index = 0;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::PageDown));
    EXPECT_EQ(p.selected_index, 2);  // 0 + 8 clamped to count-1=2
}

TEST(SessionPicker, HomeEnd) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    p.selected_index = 1;
    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::Home));
    EXPECT_EQ(p.selected_index, 0);

    EXPECT_TRUE(loom::ui::dialogs::session_picker::HandleSessionPickerEvent(p, ftxui::Event::End));
    EXPECT_EQ(p.selected_index, 2);
}

TEST(SessionPicker, TriggerPushesStandalone) {
    dsys::DialogQueue queue;
    auto sessions = MakeTestSessions();

    std::string selected_id;
    loom::ui::dialogs::triggers::PushSessionPicker(
        queue, std::move(sessions),
        [&](const std::string& id) { selected_id = id; });

    // The picker is a standalone dialog.
    EXPECT_TRUE(queue.has_standalone());
    auto peeked = queue.peek_standalone_mut();
    ASSERT_TRUE(peeked.has_value());
    auto* payload = std::get_if<dsys::SessionPickerPayload>(&peeked->get());
    ASSERT_NE(payload, nullptr);
    EXPECT_EQ(payload->sessions.size(), 3u);

    // Simulate Enter on the first item.
    payload->selected_index = 0;
    loom::ui::dialogs::session_picker::HandleSessionPickerEvent(*payload, ftxui::Event::Return);

    // The trigger wrapper popped the standalone dialog.
    EXPECT_FALSE(queue.has_standalone());
    // The callback received the session ID.
    EXPECT_EQ(selected_id, "aaa11111");
}

TEST(SessionPicker, TriggerEscapePopsStandalone) {
    dsys::DialogQueue queue;
    auto sessions = MakeTestSessions();

    std::string selected_id = "untouched";
    loom::ui::dialogs::triggers::PushSessionPicker(
        queue, std::move(sessions),
        [&](const std::string& id) { selected_id = id; });

    EXPECT_TRUE(queue.has_standalone());
    auto peeked = queue.peek_standalone_mut();
    ASSERT_TRUE(peeked.has_value());
    auto* payload = std::get_if<dsys::SessionPickerPayload>(&peeked->get());
    ASSERT_NE(payload, nullptr);

    // Escape fires on_select("") which the trigger wrapper uses to pop.
    loom::ui::dialogs::session_picker::HandleSessionPickerEvent(*payload, ftxui::Event::Escape);
    EXPECT_FALSE(queue.has_standalone());
    EXPECT_TRUE(selected_id.empty());
}

TEST(SessionPicker, RendersWithoutCrash) {
    auto sessions = MakeTestSessions();
    auto p = MakePickerPayload(sessions, nullptr);

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 100;
    ctx.term_rows = 30;

    // Render should not crash.
    auto el = loom::ui::dialogs::session_picker::RenderSessionPicker(p, ctx);
    EXPECT_NE(el.get(), nullptr);
}
