/// @file test_dialog_triggers.cpp
/// @brief Dialog trigger and quick-open tests: DialogTriggers, QuickOpen.
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

} // namespace
TEST(DialogTriggers, PushToolPermissionCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool responded = false;
    dtrig::PushToolPermission(
        queue, "Bash", "Run ls -la",
        [&](dsys::ToolPermissionPayload::Decision d, bool sandbox) {
            responded = true;
            (void)d;
            (void)sandbox;
        });

    EXPECT_TRUE(queue.has_overlay());
    auto peek = queue.peek_overlay();
    EXPECT_TRUE(peek.has_value());
    EXPECT_FALSE(responded);
}

TEST(DialogTriggers, PushMessageSelectorCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushMessageSelector(
        queue, {"opt1", "opt2", "opt3"}, "Choose...",
        [](int idx) { (void)idx; });

    EXPECT_TRUE(queue.has_any_bottom());
    auto peek = queue.peek_bottom(false);
    EXPECT_TRUE(peek.has_value());
}

TEST(DialogTriggers, PushCostThresholdCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushCostThreshold(
        queue, 5.2, std::optional<std::string>{"test-model.6"},
        []() {});

    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, PushSandboxPermissionCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushSandboxPermission(
        queue, "*.example.com",
        [](bool allow, bool always) { (void)allow; (void)always; });

    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, PushSettingsPanelCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushSettingsPanel(queue, "general", []() {});

    EXPECT_TRUE(queue.has_modal());
}

TEST(DialogTriggers, PushHelpViewCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushHelpView(queue, "commands", []() {});

    EXPECT_TRUE(queue.has_modal());
}

TEST(DialogTriggers, CommandMetadataCreateAgent) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "CREATE_AGENT");
    EXPECT_TRUE(pushed);
    // CreateAgentWizard is a full-screen standalone dialog (M7 §3.1).
    EXPECT_TRUE(queue.has_standalone());
    EXPECT_FALSE(queue.has_modal());
}

TEST(DialogTriggers, CommandMetadataEditAgent) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "EDIT_AGENT|my-agent");
    EXPECT_TRUE(pushed);
    // EditAgentWizard is a full-screen standalone dialog (M7 §3.1).
    EXPECT_TRUE(queue.has_standalone());
    EXPECT_FALSE(queue.has_modal());
}

TEST(DialogTriggers, CommandMetadataPluginDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(
        queue, "UI:plugins:manage-plugins");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());
}

TEST(DialogTriggers, CommandMetadataUnknownReturnsFalse) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "SOME_RANDOM_TAG");
    EXPECT_FALSE(pushed);
    EXPECT_TRUE(queue.empty());
}

TEST(DialogTriggers, CommandMetadataSettingsPanel) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:settings");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());
    queue.pop_modal();
    EXPECT_TRUE(queue.empty());
}

TEST(DialogTriggers, CommandMetadataHelpView) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:help");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());
    queue.pop_modal();
    EXPECT_TRUE(queue.empty());
}

TEST(DialogTriggers, CommandMetadataConfigDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:config");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());
    queue.pop_modal();
    EXPECT_TRUE(queue.empty());
}

TEST(DialogTriggers, CommandMetadataMCPDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:mcp");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());
    queue.pop_modal();
    EXPECT_TRUE(queue.empty());
}

TEST(DialogTriggers, CommandMetadataUndercoverCallout) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:undercover|1");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, CommandMetadataEffortCallout) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:effort|high");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, CommandMetadataRemoteCallout) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:remote|ssh-host");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, CommandMetadataLspRecommendation) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:lsp-rec|clangd");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, CommandMetadataPluginHint) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:plugin-hint|python-dev");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, PushLspRecommendationCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushLspRecommendation(
        queue, "clangd",
        [](bool install) { (void)install; });

    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, PushModelSwitchCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushModelSwitch(
        queue, "sonnet", "opus",
        [](bool confirm) { (void)confirm; });

    EXPECT_TRUE(queue.has_any_bottom());
}

TEST(DialogTriggers, MultipleDialogsQueueCorrectly) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    // Two bottom dialogs with different bands
    dtrig::PushCostThreshold(
        queue, 5.2, std::optional<std::string>{"sonnet"},
        []() {});
    dtrig::PushLspRecommendation(
        queue, "clangd", [](bool) {});

    // Both should be in the queue
    EXPECT_TRUE(queue.has_any_bottom());

    // Peek should show a dialog (higher priority one)
    auto peek = queue.peek_bottom(false);
    EXPECT_TRUE(peek.has_value());

    // Pop first one
    queue.pop_bottom(false);
    EXPECT_TRUE(queue.has_any_bottom());

    // Pop second one
    queue.pop_bottom(false);
    EXPECT_FALSE(queue.has_any_bottom());
}

TEST(QuickOpen, FuzzyFilterMatchesLabel) {
    namespace qo = loom::ui::dialogs::quick_open;

    std::vector<dsys::QuickOpenItem> items = {
        {"Settings", "Open settings panel", "", "Commands"},
        {"Help", "View help", "", "Commands"},
        {"Export", "Export conversation", "", "Commands"},
    };

    auto filtered = qo::filter_items(items, "set");
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].label, "Settings");
}

TEST(QuickOpen, FuzzyFilterMatchesDescription) {
    namespace qo = loom::ui::dialogs::quick_open;

    std::vector<dsys::QuickOpenItem> items = {
        {"Settings", "Open settings panel", "", "Commands"},
        {"Help", "View help docs", "", "Commands"},
    };

    auto filtered = qo::filter_items(items, "panel");
    EXPECT_EQ(filtered.size(), 1u);
    EXPECT_EQ(filtered[0].label, "Settings");
}

TEST(QuickOpen, FuzzyFilterCaseInsensitive) {
    namespace qo = loom::ui::dialogs::quick_open;

    std::vector<dsys::QuickOpenItem> items = {
        {"Settings", "Open settings", "", "Commands"},
    };

    auto filtered1 = qo::filter_items(items, "SET");
    auto filtered2 = qo::filter_items(items, "set");
    EXPECT_EQ(filtered1.size(), filtered2.size());
}

TEST(QuickOpen, EmptyQueryReturnsAll) {
    namespace qo = loom::ui::dialogs::quick_open;

    std::vector<dsys::QuickOpenItem> items = {
        {"A", "desc a", "", "Cat1"},
        {"B", "desc b", "", "Cat2"},
    };

    auto filtered = qo::filter_items(items, "");
    EXPECT_EQ(filtered.size(), 2u);
}

TEST(QuickOpen, EventNavigation) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    p.items = {
        {"First", "desc 1", "", "Cat"},
        {"Second", "desc 2", "", "Cat"},
        {"Third", "desc 3", "", "Cat"},
    };
    p.selected_index = 0;

    // Down navigation
    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(p.selected_index, 1);

    // Up navigation
    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(p.selected_index, 0);

    // Wrap around from bottom
    p.selected_index = 2;
    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::ArrowDown));
    EXPECT_EQ(p.selected_index, 0);

    // Wrap around from top
    p.selected_index = 0;
    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::ArrowUp));
    EXPECT_EQ(p.selected_index, 2);
}

TEST(QuickOpen, EventCharacterAddsToQuery) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    p.query = "";

    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::Character('a')));
    EXPECT_EQ(p.query, "a");

    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::Character('b')));
    EXPECT_EQ(p.query, "ab");
}

TEST(QuickOpen, EventBackspaceRemovesChar) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    p.query = "hello";

    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::Backspace));
    EXPECT_EQ(p.query, "hell");
}

TEST(QuickOpen, EventReturnInvokesCallback) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    p.items = {{"Item", "desc", "", "Cat"}};
    p.selected_index = 0;

    bool called = false;
    int result_idx = -1;
    bool result_confirmed = false;
    p.on_result = [&](int idx, bool confirmed) {
        called = true;
        result_idx = idx;
        result_confirmed = confirmed;
    };

    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::Return));
    EXPECT_TRUE(called);
    EXPECT_EQ(result_idx, 0);
    EXPECT_TRUE(result_confirmed);
}

TEST(QuickOpen, EventEscapeCancels) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    bool called = false;
    p.on_result = [&](int idx, bool confirmed) {
        called = true;
        EXPECT_EQ(idx, -1);
        EXPECT_FALSE(confirmed);
    };

    EXPECT_TRUE(qo::HandleQuickOpenEvent(p, ftxui::Event::Escape));
    EXPECT_TRUE(called);
}

TEST(QuickOpen, RenderProducesOutput) {
    namespace qo = loom::ui::dialogs::quick_open;

    dsys::QuickOpenPayload p;
    p.id = "test-qo";
    p.items = {
        {"Settings", "Open settings", "⌘,", "Commands"},
        {"Help", "View help", "?", "Commands"},
    };
    p.query = "";
    p.selected_index = 0;

    dsys::DialogRenderContext ctx;
    ctx.term_cols = 80;
    ctx.term_rows = 24;

    auto element = qo::RenderQuickOpen(p, ctx);
    EXPECT_NE(element.get(), nullptr);

    // Render to screen to verify it produces text
    auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(60),
                                        ftxui::Dimension::Fixed(15));
    ftxui::Render(screen, element);

    std::string output = screen.ToString();
    EXPECT_FALSE(output.empty());
    // Should contain the title
    EXPECT_NE(output.find("Quick"), std::string::npos);
    // Should contain item labels
    EXPECT_NE(output.find("Settings"), std::string::npos);
    EXPECT_NE(output.find("Help"), std::string::npos);
}

TEST(DialogTriggers, CommandMetadataQuickOpen) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool pushed = dtrig::PushFromCommandMetadata(queue, "UI:quick-open");
    EXPECT_TRUE(pushed);
    EXPECT_TRUE(queue.has_modal());

    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::QuickOpen);
}

TEST(DialogTriggers, PushQuickOpenCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    std::vector<dsys::QuickOpenItem> items = {
        {"Test", "test item", "", "Test"},
    };

    dtrig::PushQuickOpen(queue, std::move(items), "te",
                         [](int, bool) {});

    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());

    const auto& payload = peek->get();
    auto* p = std::get_if<dsys::QuickOpenPayload>(&payload);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->query, "te");
    EXPECT_EQ(p->items.size(), 1u);
}

TEST(DialogTriggers, PushAboutDialogCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool closed = false;
    dtrig::PushAboutDialog(queue, [&] { closed = true; });

    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::AboutDialog);

    auto* p = std::get_if<dsys::AboutDialogPayload>(&peek->get());
    ASSERT_NE(p, nullptr);
}

TEST(DialogTriggers, PushTasksViewCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushTasksView(queue, [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::TasksView);
}

TEST(DialogTriggers, PushTeamsViewCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushTeamsView(queue, [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::TeamsView);
}

TEST(DialogTriggers, PushExportDialogCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    bool responded = false;
    dtrig::PushExportDialog(queue, "markdown",
        [&](bool ok) { responded = true; EXPECT_TRUE(ok); });

    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::ExportDialog);

    auto* p = std::get_if<dsys::ExportDialogPayload>(&peek->get());
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->format, "markdown");
}

TEST(DialogTriggers, PushDiffDialogCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushDiffDialog(queue, "Changes", "old", "new",
        [](bool) {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::DiffDialog);
}

TEST(DialogTriggers, PushGlobalSearchCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushGlobalSearch(queue, "test", [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::GlobalSearch);

    auto* p = std::get_if<dsys::GlobalSearchPayload>(&peek->get());
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->query, "test");
}

TEST(DialogTriggers, PushHistorySearchCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushHistorySearch(queue, "project",
        [](std::string_view) {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::HistorySearch);
}

TEST(DialogTriggers, PushFeedbackSurveyCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushFeedbackSurvey(queue, [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::FeedbackSurvey);
}

TEST(DialogTriggers, PushManagedSecurityCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushManagedSettingsSecurity(queue, [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::ManagedSettingsSecurity);
}

TEST(DialogTriggers, PushPluginDialogCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushPluginDialog(queue, 0, "", [] {});
    EXPECT_TRUE(queue.has_modal());
    auto peek = queue.peek_modal();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::PluginDialog);
}

TEST(DialogTriggers, PushTrustDialogCreatesDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    dtrig::PushTrustDialog(queue, "example.com",
        [](bool) {});
    // TrustDialog is a full-screen standalone dialog (M7 §3.1).
    EXPECT_TRUE(queue.has_standalone());
    EXPECT_FALSE(queue.has_modal());
    auto peek = queue.peek_standalone();
    ASSERT_TRUE(peek.has_value());
    EXPECT_EQ(dsys::type_of(*peek), dsys::DialogType::TrustDialog);
}

TEST(DialogTriggers, MetadataAboutDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "UI:about"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::AboutDialog);
}

TEST(DialogTriggers, MetadataTasksView) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "UI:tasks"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::TasksView);
}

TEST(DialogTriggers, MetadataTeamsView) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "UI:teams"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::TeamsView);
}

TEST(DialogTriggers, MetadataExportDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "UI:export"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::ExportDialog);
}

TEST(DialogTriggers, MetadataDiffDialog) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "DIFF_DIALOG"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::DiffDialog);
}

TEST(DialogTriggers, MetadataFeedbackSurvey) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "UI:feedback"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::FeedbackSurvey);
}

TEST(DialogTriggers, MetadataGlobalSearch) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "GLOBAL_SEARCH"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::GlobalSearch);
}

TEST(DialogTriggers, MetadataHistorySearch) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "HISTORY_SEARCH"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()), dsys::DialogType::HistorySearch);
}

TEST(DialogTriggers, MetadataManagedSecurity) {
    dsys::DialogQueue queue;
    namespace dtrig = loom::ui::dialogs::triggers;

    EXPECT_TRUE(dtrig::PushFromCommandMetadata(queue, "MANAGED_SETTINGS_SECURITY"));
    EXPECT_TRUE(queue.has_modal());
    EXPECT_EQ(dsys::type_of(*queue.peek_modal()),
              dsys::DialogType::ManagedSettingsSecurity);
}
