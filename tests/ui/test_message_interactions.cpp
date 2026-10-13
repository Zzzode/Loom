#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
#include "test_ui_helpers.h"

import std;
import loom.types.types;
import loom.ui.messages.message_components;
import loom.ui.messages.message_row;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.collapsed_content_message;
import loom.ui.messages.api_error_message;
import loom.ui.messages.messages_list;
import loom.ui.foundation.theme_provider;

namespace {
namespace msg = loom::ui::messages;
std::string plain(ftxui::Element el, int width = 120, int height = 50) {
    return strip_ansi(render_to_plain_text(std::move(el), width, height));
}
std::string output_lines() {
    std::string text;
    for (int i = 0; i < 14; ++i) text += "output-line-" + std::to_string(i) + "\n";
    return text;
}
}

TEST(MessageInteractions, ClickActionsAndKeyHintsRemainDistinctAcrossThemes) {
    namespace theme = loom::ui::design::theme;
    const auto original = theme::current_theme();
    for (auto variant : {theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
            theme::ThemeVariant::Monochrome, theme::ThemeVariant::DarkDaltonized}) {
        theme::set_theme(variant);
        auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(70), ftxui::Dimension::Fixed(1));
        ftxui::Render(screen, ftxui::hbox({msg::message_action("Show more"),
                                        msg::message_key_hint("Ctrl+E", "Toggle tools")}));
        EXPECT_TRUE(screen.PixelAt(1, 0).underlined);
        EXPECT_TRUE(screen.PixelAt(1, 0).bold);
        EXPECT_FALSE(screen.PixelAt(1, 0).dim);
        EXPECT_EQ(screen.PixelAt(1, 0).foreground_color, theme::current_theme().palette->suggestion);
        EXPECT_TRUE(screen.PixelAt(13, 0).inverted);
        EXPECT_FALSE(screen.PixelAt(13, 0).underlined);
        EXPECT_FALSE(screen.PixelAt(13, 0).dim);
    }
    theme::set_theme(original);
}

TEST(MessageInteractions, ToolPreviewShortcutDependsOnTranscriptMode) {
    for (auto kind : {msg::ToolResultKind::Success, msg::ToolResultKind::Error}) {
        msg::ToolResultFaithfulData data;
        data.kind = kind;
        data.content = output_lines();
        auto collapsed = plain(msg::RenderToolResultMessageFaithful(data, false));
        EXPECT_NE(collapsed.find("[Show more]"), std::string::npos);
        EXPECT_NE(collapsed.find("[Ctrl+E] Toggle tools"), std::string::npos);
        EXPECT_EQ(collapsed.find("Ctrl+O"), std::string::npos);
        EXPECT_EQ(collapsed.find("output-line-13"), std::string::npos);
        data.is_transcript_mode = true;
        auto transcript = plain(msg::RenderToolResultMessageFaithful(data, false));
        EXPECT_NE(transcript.find("[Show more]"), std::string::npos);
        EXPECT_EQ(transcript.find("Ctrl+E"), std::string::npos);
        data.verbose = true;
        auto expanded = plain(msg::RenderToolResultMessageFaithful(data, false));
        EXPECT_NE(expanded.find("output-line-13"), std::string::npos);
        EXPECT_NE(expanded.find("[Show less]"), std::string::npos);
    }
}

TEST(MessageInteractions, ExpandedToolRestoresContentPastCharacterPreview) {
    msg::ToolResultFaithfulData data;
    data.content = std::string(3999, 'x') + "中" + "\nRESTORED-TAIL";
    auto collapsed = plain(msg::RenderToolResultMessageFaithful(data, false), 4200, 8);
    EXPECT_NE(collapsed.find("[Show more]"), std::string::npos);
    EXPECT_EQ(collapsed.find("RESTORED-TAIL"), std::string::npos);
    EXPECT_EQ(collapsed.find("�"), std::string::npos);
    data.verbose = true;
    auto expanded = plain(msg::RenderToolResultMessageFaithful(data, false), 4200, 8);
    EXPECT_NE(expanded.find("中"), std::string::npos);
    EXPECT_NE(expanded.find("RESTORED-TAIL"), std::string::npos);
}

TEST(MessageInteractions, ContentItemsRespectPreviewBudgetAndExpandAllItems) {
    msg::ToolResultFaithfulData data;
    data.content_items.emplace();
    loom::core::ToolResultContentItem first;
    first.type = "text";
    first.text = output_lines();
    loom::core::ToolResultContentItem image;
    image.type = "image";
    loom::core::ToolResultContentItem last;
    last.type = "text";
    last.text = "LAST-ITEM";
    data.content_items->push_back(first);
    data.content_items->push_back(image);
    data.content_items->push_back(last);
    auto collapsed = plain(msg::RenderToolResultMessageFaithful(data, false));
    EXPECT_EQ(collapsed.find("[Image]"), std::string::npos);
    EXPECT_EQ(collapsed.find("LAST-ITEM"), std::string::npos);
    EXPECT_NE(collapsed.find("+6 lines"), std::string::npos);
    data.verbose = true;
    auto expanded = plain(msg::RenderToolResultMessageFaithful(data, false));
    EXPECT_NE(expanded.find("[Image]"), std::string::npos);
    EXPECT_NE(expanded.find("LAST-ITEM"), std::string::npos);
}

TEST(MessageInteractions, UpstreamTruncationIsInformational) {
    msg::ToolResultFaithfulData data;
    data.content = "short output";
    data.is_truncated = true;
    auto text = plain(msg::RenderToolResultMessageFaithful(data, false));
    EXPECT_NE(text.find("(output truncated)"), std::string::npos);
    EXPECT_EQ(text.find("Show more"), std::string::npos);
}

TEST(MessageInteractions, ThinkingActionsOnlyAppearWhenToggleCanWork) {
    namespace thinking = msg::thinking_message;
    thinking::ThinkingMessageData data;
    data.raw_text = "summary\nfull thinking details";
    auto collapsed = plain(thinking::RenderThinkingMessageFaithful(data, false, false, false), 80, 5);
    EXPECT_NE(collapsed.find("[Show thinking]"), std::string::npos);
    EXPECT_NE(collapsed.find("[Ctrl+O] Transcript"), std::string::npos);
    auto expanded = plain(thinking::RenderThinkingMessageFaithful(data, false, true, false));
    EXPECT_NE(expanded.find("[Hide thinking]"), std::string::npos);
    auto forced = plain(thinking::RenderThinkingMessageFaithful(data, true, true, false));
    EXPECT_EQ(forced.find("Hide thinking"), std::string::npos);
    auto read_only = plain(thinking::RenderThinkingMessageFaithful(data, false, false, false, false));
    EXPECT_EQ(read_only.find("Show thinking"), std::string::npos);
}

TEST(MessageInteractions, AssistantPreviewAndCopyControlsHaveWorkingBindings) {
    msg::AssistantTextMessageData data;
    for (int i = 0; i < 24; ++i) data.content += "assistant-line-" + std::to_string(i) + "\n";
    std::string copied;
    int regenerated = 0;
    auto component = msg::MakeAssistantTextMessage(data,
        [&](std::string_view text) { copied = text; }, [&] { ++regenerated; });
    EXPECT_NE(plain(component->Render()).find("[Copy]"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('E')));
    auto collapsed = plain(component->Render());
    EXPECT_NE(collapsed.find("[E] Show more"), std::string::npos);
    EXPECT_EQ(collapsed.find("assistant-line-23"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('e')));
    EXPECT_NE(plain(component->Render()).find("assistant-line-23"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Special({3})));
    EXPECT_EQ(copied, data.content);
    copied.clear();
    component->TakeFocus();
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));
    EXPECT_EQ(copied, data.content);
    EXPECT_EQ(regenerated, 0);
}

TEST(MessageInteractions, StaticApiErrorDoesNotAdvertiseUnavailableKeyboardActions) {
    namespace api = msg::api_error_message;
    api::APIErrorOptions opts;
    for (int i = 0; i < 100; ++i) opts.error.message += "provider detail ";
    opts.error.message += "API-TAIL";
    auto static_text = plain(api::RenderAPIError(opts, false), 120, 30);
    EXPECT_NE(static_text.find("API-TAIL"), std::string::npos);
    EXPECT_EQ(static_text.find("[Enter]"), std::string::npos);
    auto component = api::APIErrorMessage(opts);
    EXPECT_NE(plain(component->Render()).find("[Enter] Show more"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));
    EXPECT_NE(plain(component->Render()).find("API-TAIL"), std::string::npos);
}

TEST(MessageInteractions, ExpandedGroupEnterPreviewsAndSpaceCollapses) {
    namespace collapsed = msg::collapsed_content;
    collapsed::CollapsedContentOptions opts;
    collapsed::ContentEntry entry;
    entry.path = "file.cpp";
    entry.preview = "group-preview";
    opts.entries.push_back(entry);
    int previews = 0;
    opts.on_toggle_entry = [&](std::size_t) { ++previews; };
    auto component = collapsed::CollapsedContentMessage(opts);
    EXPECT_NE(plain(component->Render()).find("[Enter/Space] Expand"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));
    auto expanded = plain(component->Render());
    EXPECT_NE(expanded.find("[Space] Collapse"), std::string::npos);
    EXPECT_NE(expanded.find("[Enter] Preview"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::ArrowDown));
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Return));
    EXPECT_EQ(previews, 1);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character(' ')));
    EXPECT_NE(plain(component->Render()).find("[Enter/Space] Expand"), std::string::npos);
}

TEST(MessageInteractions, NarrowToolHintsWrapWithoutLosingTheirControls) {
    msg::ToolResultFaithfulData data;
    data.content = output_lines();
    for (int width : {40, 60, 80}) {
        auto text = plain(msg::RenderToolResultMessageFaithful(data, false), width, 20);
        EXPECT_NE(text.find("[Show more]"), std::string::npos) << text;
        EXPECT_NE(text.find("[Ctrl+E] Toggle tools"), std::string::npos) << text;
    }
}

TEST(MessageInteractions, LongThinkingSummaryLeavesRoomForActions) {
    msg::thinking_message::ThinkingMessageData data;
    data.raw_text = std::string(1000, 'x');
    for (int width : {60, 80}) {
        auto text = plain(msg::thinking_message::RenderThinkingMessageFaithful(
            data, false, false, false), width, 5);
        EXPECT_NE(text.find("[Show thinking]"), std::string::npos) << text;
        EXPECT_NE(text.find("[Ctrl+O] Transcript"), std::string::npos) << text;
    }
}

TEST(MessageInteractions, TranscriptLimitShortcutSurvivesNarrowWidth) {
    for (int width : {40, 60, 80}) {
        auto text = plain(loom::ui::messages_list::detail::render_transcript_cap_divider(100), width, 5);
        EXPECT_NE(text.find("[Ctrl+E] Show all messages"), std::string::npos) << text;
    }
}

TEST(MessageInteractions, ReadOnlyCompactSummaryDoesNotAdvertiseKeyboardExpansion) {
    loom::ui::messages_list::VisibleRow row;
    row.kind = loom::ui::messages_list::VisibleRow::Kind::CompactGroup;
    row.group_count = 10;
    auto text = plain(loom::ui::messages_list::detail::render_compact_group_row(row, false));
    EXPECT_NE(text.find("10 messages collapsed"), std::string::npos);
    EXPECT_EQ(text.find("Enter"), std::string::npos);
    EXPECT_EQ(text.find("Space"), std::string::npos);
}

TEST(MessageInteractions, ArgsKeyCanExpandPastTheDefaultSizeThreshold) {
    namespace tool = msg::tool_use_message;
    tool::ToolUseRenderOptions opts;
    opts.call.tool_name = "Read";
    opts.call.parameters_language = "text";
    opts.call.raw_parameters = std::string(1100, 'x') + "\nARGS-TAIL";
    auto component = tool::ToolUseMessage(opts);
    auto before = plain(component->Render());
    EXPECT_NE(before.find("[A] Expand args"), std::string::npos);
    EXPECT_EQ(before.find("ARGS-TAIL"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('a')));
    auto after = plain(component->Render());
    EXPECT_NE(after.find("[A] Collapse args"), std::string::npos);
    EXPECT_NE(after.find("ARGS-TAIL"), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('a')));
    EXPECT_EQ(plain(component->Render()).find("ARGS-TAIL"), std::string::npos);
}

TEST(MessageInteractions, RedactedThinkingBridgeDoesNotExposeClickActions) {
    namespace list = loom::ui::messages_list;
    list::MessagesListInput input;
    msg::thinking_message::ThinkingMessageOptions opts;
    opts.data.raw_text = "redacted content";
    input.rows.push_back(opts);
    input.shapes.push_back(msg::MessageShape::AssistantRedactedThinking);
    input.uuids.push_back("redacted-row");
    input.streaming_tail_row = std::size_t(-1);
    list::RowClickTracker tracker;
    input.row_click_tracker = &tracker;
    tracker.begin_frame();
    auto text = plain(list::detail::render_payload_row(input, 0, false, 0, false));
    tracker.end_frame();
    EXPECT_EQ(text.find("Show thinking"), std::string::npos);
    EXPECT_FALSE(tracker.hit_test(1, 0).has_value());
}

TEST(MessageInteractions, ErrorMessageOnlyToolResultHasAWorkingClickTarget) {
    namespace list = loom::ui::messages_list;
    list::MessagesListInput input;
    msg::ToolResultOptions opts;
    opts.tool_name = "Read";
    opts.status = msg::ToolResultStatus::Error;
    opts.error_message = output_lines();
    input.rows.push_back(opts);
    input.shapes.push_back(msg::MessageShape::UserToolResult);
    input.uuids.push_back("error-result");
    input.streaming_tail_row = std::size_t(-1);
    list::RowClickTracker tracker;
    input.row_click_tracker = &tracker;
    tracker.begin_frame();
    auto text = plain(list::detail::render_payload_row(input, 0, false, 0, false));
    tracker.end_frame();
    EXPECT_NE(text.find("[Show more]"), std::string::npos);
    EXPECT_EQ(tracker.hit_test(1, 0), std::optional<std::string>("Read"));
}

TEST(MessageInteractions, ApiFailureIdentifiesRequestAndProviderWithoutNestedChrome) {
    namespace api = msg::api_error_message;
    api::APIErrorOptions opts;
    opts.error.http_status = 503;
    opts.error.provider = "Messages";
    opts.error.error_code = "overloaded_error";
    opts.error.message = "Provider is temporarily unavailable";
    opts.error.trace_id = "trace-123";
    opts.on_retry = [] {};
    const auto text = plain(api::RenderAPIError(opts, false));
    EXPECT_NE(text.find("Model request failed"), std::string::npos);
    EXPECT_NE(text.find("HTTP 503"), std::string::npos);
    EXPECT_NE(text.find("Messages"), std::string::npos);
    EXPECT_NE(text.find("overloaded_error"), std::string::npos);
    EXPECT_NE(text.find("Provider is temporarily unavailable"), std::string::npos);
    EXPECT_NE(text.find("trace-123"), std::string::npos);
    EXPECT_EQ(text.find("Retry"), std::string::npos);
    EXPECT_EQ(text.find("System"), std::string::npos);
    EXPECT_EQ(text.find("─"), std::string::npos);
    EXPECT_EQ(text.find("╭"), std::string::npos);
    EXPECT_NE(plain(api::APIErrorMessage(opts)->Render()).find("[R] Retry request"),
              std::string::npos);
}

TEST(MessageInteractions, AuthenticationFailureNeverAdvertisesOrInvokesRetry) {
    namespace api = msg::api_error_message;
    for (const auto status : {401, 403}) {
        api::APIErrorOptions opts;
        opts.error.http_status = status;
        opts.error.message = "Invalid credentials";
        int retries = 0;
        opts.on_retry = [&] { ++retries; };
        auto component = api::APIErrorMessage(opts);
        EXPECT_EQ(plain(component->Render()).find("Retry"), std::string::npos);
        EXPECT_FALSE(component->OnEvent(ftxui::Event::Character('R')));
        EXPECT_EQ(retries, 0);
        EXPECT_EQ(plain(component->Render()).find("Expand"), std::string::npos);
    }
}
