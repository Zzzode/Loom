// RFC 0023: current-session instance browser component behavior.
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.features.agents.instance_browser;
import loom.ui.features.agents.runtime_agents;
import loom.ui.foundation.theme_provider;

namespace {
namespace browser = loom::ui::agents::instances;
namespace runtime = loom::ui::agents::runtime;
using namespace ftxui;

struct Fixture {
    std::shared_ptr<browser::BrowserState> state = std::make_shared<browser::BrowserState>();
    std::vector<browser::Entry> entries;
    std::map<std::string, browser::Detail> details;
    int closes = 0;
    std::vector<std::string> stops;
    std::vector<std::pair<std::string, std::string>> continuations;

    Component create(bool open_detail = false) {
        browser::Options options;
        options.state = state;
        options.open_detail = open_detail;
        options.list = [this] { return entries; };
        options.detail = [this](std::string_view id) -> std::optional<browser::Detail> {
            const auto it = details.find(std::string(id));
            if (it == details.end()) return std::nullopt;
            return it->second;
        };
        options.on_continue = [this](std::string id, std::string prompt) {
            continuations.emplace_back(std::move(id), std::move(prompt));
        };
        options.on_stop = [this](std::string id) { stops.push_back(std::move(id)); };
        options.on_close = [this] { ++closes; };
        return browser::CreateBrowser(std::move(options));
    }

    void add(std::string id, bool active = true, bool team = false) {
        browser::Entry entry;
        entry.id = id;
        entry.name = "worker-" + id;
        entry.status = active ? runtime::Status::Running : runtime::Status::Failed;
        entry.active = active;
        entry.team = team;
        entries.push_back(entry);
        browser::Detail detail;
        detail.entry = entry;
        detail.can_continue = !active && !team;
        detail.can_stop = active;
        detail.continue_reason = "This instance is still active.";
        detail.stop_reason = "No owned execution is active.";
        details.emplace(id, detail);
    }

    std::string paint(const Component& component, int columns = 80, int rows = 24) {
        state->columns = columns;
        state->rows = rows;
        auto screen = Screen::Create(Dimension::Fixed(columns), Dimension::Fixed(rows));
        Render(screen, component->Render());
        return strip_ansi(screen.ToString());
    }
};
}  // namespace

TEST(InstanceBrowser, EmptyStatesKeepCategoriesHistoryAndExitDiscoverable) {
    Fixture fixture;
    auto component = fixture.create();
    EXPECT_NE(fixture.paint(component, 40).find("No active instances."), std::string::npos);
    EXPECT_TRUE(component->OnEvent(Event::Tab));
    EXPECT_NE(fixture.paint(component, 40).find("History"), std::string::npos);
    EXPECT_NE(fixture.paint(component, 40).find("No ended instances."), std::string::npos);
    EXPECT_TRUE(component->OnEvent(Event::ArrowRight));
    EXPECT_NE(fixture.paint(component).find("Team Agents"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(Event::Escape));
    EXPECT_EQ(fixture.closes, 1);
}

TEST(InstanceBrowser, SelectionUsesStableIdAcrossReorderAndHandlesRemovedRows) {
    Fixture fixture;
    fixture.add("one");
    fixture.add("two");
    auto component = fixture.create();
    (void)fixture.paint(component);
    EXPECT_EQ(fixture.state->selected_id, "one");
    EXPECT_TRUE(component->OnEvent(Event::ArrowDown));
    EXPECT_EQ(fixture.state->selected_id, "two");
    std::ranges::reverse(fixture.entries);
    (void)fixture.paint(component);
    EXPECT_EQ(fixture.state->selected_id, "two");
    fixture.entries.erase(fixture.entries.begin());
    (void)fixture.paint(component);
    EXPECT_EQ(fixture.state->selected_id, "one");
    fixture.entries.clear();
    (void)fixture.paint(component);
    EXPECT_TRUE(fixture.state->selected_id.empty());
}

TEST(InstanceBrowser, HistoryAndTeamCategoriesDoNotMixTaskOrInstanceRows) {
    Fixture fixture;
    fixture.add("active");
    fixture.add("failed", false);
    fixture.add("team", true, true);
    auto component = fixture.create();
    auto text = fixture.paint(component);
    EXPECT_NE(text.find("worker-active"), std::string::npos);
    EXPECT_EQ(text.find("worker-failed"), std::string::npos);
    EXPECT_EQ(text.find("worker-team"), std::string::npos);
    component->OnEvent(Event::Tab);
    text = fixture.paint(component);
    EXPECT_NE(text.find("worker-failed"), std::string::npos);
    EXPECT_EQ(text.find("worker-active"), std::string::npos);
    component->OnEvent(Event::ArrowRight);
    component->OnEvent(Event::Tab);
    text = fixture.paint(component);
    EXPECT_NE(text.find("worker-team"), std::string::npos);
    EXPECT_EQ(text.find("worker-failed"), std::string::npos);
}

TEST(InstanceBrowser, DetailsStopAndEditableContinueUseSelectedIdentityAndCapabilities) {
    Fixture fixture;
    fixture.add("running");
    fixture.add("failed", false);
    auto component = fixture.create();
    (void)fixture.paint(component);
    component->OnEvent(Event::Return);
    EXPECT_NE(fixture.paint(component).find("Subagent · running"), std::string::npos);
    component->OnEvent(Event::Character('c'));
    component->OnEvent(Event::Return);
    EXPECT_TRUE(fixture.continuations.empty());
    component->OnEvent(Event::Character('s'));
    ASSERT_EQ(fixture.stops.size(), 1U);
    EXPECT_EQ(fixture.stops.front(), "running");
    component->OnEvent(Event::Escape);
    component->OnEvent(Event::Tab);
    (void)fixture.paint(component);
    component->OnEvent(Event::Return);
    component->OnEvent(Event::Character('s'));
    EXPECT_EQ(fixture.stops.size(), 1U);
    component->OnEvent(Event::Character('c'));
    EXPECT_NE(fixture.paint(component).find("Continuation instruction"), std::string::npos);
    component->OnEvent(Event::Character(" additional evidence"));
    component->OnEvent(Event::Return);
    ASSERT_EQ(fixture.continuations.size(), 1U);
    EXPECT_EQ(fixture.continuations.front().first, "failed");
    EXPECT_NE(fixture.continuations.front().second.find("additional evidence"), std::string::npos);
    EXPECT_EQ(fixture.closes, 0);
}

TEST(InstanceBrowser, DirectDetailHandlesUnavailableOwnerAndEscapeInTwoLevels) {
    Fixture fixture;
    fixture.state->selected_id = "foreign";
    auto component = fixture.create(true);
    EXPECT_NE(fixture.paint(component).find("unavailable in this session"), std::string::npos);
    component->OnEvent(Event::Escape);
    EXPECT_EQ(fixture.closes, 0);
    component->OnEvent(Event::Escape);
    EXPECT_EQ(fixture.closes, 1);
}

TEST(InstanceBrowser, DetailSanitizesAnsiOscControlsPreservesUtf8AndBoundsTranscriptTail) {
    Fixture fixture;
    fixture.add("failed", false);
    fixture.state->selected_id = "failed";
    auto& detail = fixture.details.at("failed");
    detail.error = "\x1b[31m预算耗尽\x1b[0m\a\x1b]8;;https://"
                   "invalid.example\a错误\x1b]8;;\a\x1bPPRIVATE-PROTOCOL\x1b\\";
    for (int i = 0; i < 200; ++i)
        detail.transcript += std::format("transcript-line-{}\n", i);
    detail.output = std::string(17000, 'x') + "END-OUTPUT";
    auto component = fixture.create(true);
    auto text = fixture.paint(component, 40, 24);
    EXPECT_NE(text.find("预算耗尽错误"), std::string::npos);
    EXPECT_EQ(text.find("invalid.example"), std::string::npos);
    EXPECT_EQ(text.find("PRIVATE-PROTOCOL"), std::string::npos);
    EXPECT_EQ(text.find("[31m"), std::string::npos);
    bool saw_truncation = false;
    bool saw_tail = false;
    bool saw_old = false;
    for (int i = 0; i < 80; ++i) {
        component->OnEvent(Event::PageDown);
        text = fixture.paint(component, 40, 24);
        saw_truncation |= text.find("[truncated; showing tail]") != std::string::npos;
        saw_tail |= text.find("transcript-line-199") != std::string::npos;
        saw_old |= text.find("transcript-line-0\n") != std::string::npos;
    }
    EXPECT_TRUE(saw_truncation);
    EXPECT_TRUE(saw_tail);
    EXPECT_FALSE(saw_old);
}

TEST(InstanceBrowser, NarrowLayoutsKeepStatusActionsAndScrollHintWithinCells) {
    for (const int columns : {40, 60, 120}) {
        Fixture fixture;
        fixture.add("failed", false, true);
        fixture.state->selected_id = "failed";
        fixture.details.at("failed").error = "A bounded failure reason.";
        auto component = fixture.create(true);
        auto screen = Screen::Create(Dimension::Fixed(columns + 1), Dimension::Fixed(24));
        fixture.state->columns = columns;
        Render(screen, hbox({component->Render(), text("|")}));
        EXPECT_EQ(screen.PixelAt(columns, 0).character, "|");
        const auto text = strip_ansi(screen.ToString());
        EXPECT_NE(text.find("Team Agent · failed"), std::string::npos);
        EXPECT_NE(text.find("Failure reason"), std::string::npos);
        EXPECT_NE(text.find("Esc back"), std::string::npos);
    }
}

TEST(InstanceBrowser, RetainedSelectionReopensListUnlessDetailWasExplicitlyRequested) {
    Fixture fixture;
    fixture.add("retained");
    fixture.state->selected_id = "retained";
    auto component = fixture.create();
    auto text = fixture.paint(component);
    EXPECT_NE(text.find("Agent instances"), std::string::npos);
    EXPECT_NE(text.find("worker-retained"), std::string::npos);
    EXPECT_EQ(text.find("Subagent · running"), std::string::npos);
    EXPECT_EQ(fixture.state->selected_id, "retained");
    component->OnEvent(Event::Return);
    EXPECT_NE(fixture.paint(component).find("Subagent · running"), std::string::npos);
}

TEST(InstanceBrowser, ModalClearsUnderlyingTranscriptAndEditorAcrossItsWholeAllocatedBox) {
    for (const int columns : {40, 60, 120})
        for (const bool detail : {false, true}) {
            Fixture fixture;
            fixture.state->columns = columns;
            fixture.state->rows = 24;
            if (detail) fixture.state->selected_id = "unavailable";
            auto component = fixture.create(detail);
            Elements transcript;
            for (int row = 0; row < 24; ++row)
                transcript.push_back(text(std::string(columns + 4, 'Z')));
            auto screen = Screen::Create(Dimension::Fixed(columns + 4), Dimension::Fixed(24));
            Render(screen,
                   dbox({vbox(std::move(transcript)), hbox({component->Render(), filler()})}));
            for (int row = 0; row < 24; ++row)
                for (int column = 0; column < columns; ++column) {
                    SCOPED_TRACE(std::format("columns={},detail={},cell={},{}", columns, detail,
                                             column, row));
                    EXPECT_NE(screen.PixelAt(column, row).character, "Z");
                }
            EXPECT_EQ(screen.PixelAt(columns - 1, 5).background_color,
                      loom::ui::design::theme::current_theme().palette->background);
            EXPECT_EQ(screen.PixelAt(columns, 5).character, "Z");
            EXPECT_EQ(screen.PixelAt(columns - 1, 5).character, " ");
        }
}

TEST(InstanceBrowser, EnterOpensSelectedConversationAndIStillShowsDetails) {
    Fixture fixture;
    fixture.add("first");
    fixture.add("second");
    std::vector<std::string> opened;
    browser::Options options;
    options.state = fixture.state;
    options.list = [&] { return fixture.entries; };
    options.on_open = [&](std::string id) { opened.push_back(std::move(id)); };
    auto component = browser::CreateBrowser(std::move(options));
    (void)fixture.paint(component);
    component->OnEvent(Event::ArrowDown);
    component->OnEvent(Event::Return);
    ASSERT_EQ(opened, std::vector<std::string>{"second"});
    EXPECT_NE(fixture.paint(component).find("Enter session"), std::string::npos);
    component->OnEvent(Event::Character('i'));
    EXPECT_NE(fixture.paint(component).find("Agent instance"), std::string::npos);
    EXPECT_EQ(opened.size(), 1u);
}
