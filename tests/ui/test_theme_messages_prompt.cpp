/// @file test_theme_messages_prompt.cpp
/// @brief Theme and content invariants for message and prompt presentation.
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.foundation.theme_provider;
import loom.ui.foundation.design_figures;
import loom.ui.messages.message_components;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.message_advisor;
import loom.ui.messages.message_rate_limit;
import loom.ui.messages.message_plan_approval;
import loom.ui.messages.message_shutdown;
import loom.ui.messages.message_task_assignment;
import loom.ui.messages.message_hook_progress;
import loom.ui.messages.user_text_message;
import loom.ui.messages.attachment_message;
import loom.ui.messages.message_image;
import loom.ui.prompt.combined_highlights;
import loom.ui.prompt.prompt_paste_handler;

namespace {
namespace theme = loom::ui::design::theme;
namespace msg = loom::ui::messages;
namespace figs = loom::ui::design::figures;
constexpr std::array kVariants = {
    theme::ThemeVariant::Light,
    theme::ThemeVariant::DarkDaltonized,
    theme::ThemeVariant::LightDaltonized,
    theme::ThemeVariant::Monochrome,
};
struct RestoreTheme {
    theme::Theme original = theme::current_theme();
    ~RestoreTheme() { theme::set_theme(original); }
};

ftxui::Screen RenderElement(ftxui::Element element) {
    auto screen = ftxui::Screen::Create(
        ftxui::Dimension::Fixed(100), ftxui::Dimension::Fixed(30));
    ftxui::Render(screen, std::move(element));
    return screen;
}

void ExpectWordColor(const ftxui::Screen& screen, std::string_view word,
                     ftxui::Color expected,
                     std::optional<ftxui::Color> expected_background = std::nullopt) {
    bool found = false;
    for (int y = 0; y < screen.dimy(); ++y) {
        for (int x = 0; x + static_cast<int>(word.size()) <= screen.dimx(); ++x) {
            bool match = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                if (screen.PixelAt(x + static_cast<int>(i), y).character !=
                    std::string(1, word[i])) { match = false; break; }
            }
            if (!match) continue;
            found = true;
            for (std::size_t i = 0; i < word.size(); ++i) {
                const auto& pixel = screen.PixelAt(x + static_cast<int>(i), y);
                EXPECT_EQ(pixel.foreground_color, expected) << word;
                if (expected_background)
                    EXPECT_EQ(pixel.background_color, *expected_background) << word;
            }
        }
    }
    EXPECT_TRUE(found) << "Missing rendered label: " << word;
}
}

TEST(MessagePromptTheme, HeldAdvisorActionFollowsCurrentSuggestionToken) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    msg::AdvisorMessage data;
    data.content = "Advice";
    data.severity = msg::AdvisorSeverity::Suggestion;
    data.action_label = "Resolve";
    auto component = msg::MakeAdvisorMessage(data, nullptr, [] {});
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        ExpectWordColor(RenderElement(component->Render()), "Resolve",
                        theme::current_theme().palette->suggestion);
    }
}

TEST(MessagePromptTheme, HeldRateLimitRetryFollowsCurrentTheme) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    msg::RateLimitInfo data;
    data.is_hard_limit = true;
    data.is_overloaded = true;
    auto component = msg::MakeRateLimitMessage(data, [] {});
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto screen = RenderElement(component->Render());
        ExpectWordColor(screen, "Retry", pal.success);
        ExpectWordColor(screen, "reached", pal.danger);
        EXPECT_EQ(strip_ansi(screen.ToString()).find('\xF0'), std::string::npos);
    }
}

TEST(MessagePromptTheme, HeldPlanActionsFollowCurrentThemeAndKeepCallbacks) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    msg::PlanApprovalOptions data;
    data.plan_title = "Plan";
    data.steps.push_back({"Run operation", std::nullopt, true});
    int approved = 0;
    auto component = msg::MakePlanApprovalMessage(data, [&] { ++approved; },
        [](std::vector<msg::PlanStep>) {}, [](std::string) {});
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto screen = RenderElement(component->Render());
        ExpectWordColor(screen, "Approve", pal.success);
        ExpectWordColor(screen, "Modify", pal.info);
        ExpectWordColor(screen, "Reject", pal.danger);
        EXPECT_EQ(strip_ansi(screen.ToString()).find('\xF0'), std::string::npos);
    }
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Character('y')));
    EXPECT_EQ(approved, 1);
}

TEST(MessagePromptTheme, HeldShutdownActionsFollowCurrentTheme) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    msg::shutdown::ShutdownMessageData data;
    auto component = msg::shutdown::MakeShutdownMessage(data, [] {}, [] {});
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto screen = RenderElement(component->Render());
        ExpectWordColor(screen, "Resume", pal.info);
        ExpectWordColor(screen, "session", pal.primary);
        EXPECT_EQ(strip_ansi(screen.ToString()).find('\xF0'), std::string::npos);
    }
}

TEST(MessagePromptTheme, TaskStatusesRemainReadableAcrossThemes) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        for (auto status : {msg::TaskAssignmentStatus::Completed,
                            msg::TaskAssignmentStatus::Failed}) {
            msg::TaskAssignmentOptions data;
            data.status = status;
            data.assignee = "worker";
            auto screen = RenderElement(msg::render_task_assignment(data));
            const bool success = status == msg::TaskAssignmentStatus::Completed;
            ExpectWordColor(screen, success ? "DONE" : "FAILED",
                            success ? pal.success : pal.danger);
        }
    }
}

TEST(MessagePromptTheme, UserEmojiRemainsInRenderedAndCopiedPayload) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Light);
    msg::UserTextMessageData data;
    data.content = "User payload 🎁 🔑 🤖";
    std::string copied;
    auto component = msg::MakeUserTextMessage(data,
        [&](std::string_view value) { copied = value; });
    const auto rendered = strip_ansi(RenderElement(component->Render()).ToString());
    EXPECT_NE(rendered.find(data.content), std::string::npos);
    EXPECT_TRUE(component->OnEvent(ftxui::Event::Special({3})));
    EXPECT_EQ(copied, data.content);
}

TEST(MessagePromptTheme, HookSpinnerUsesWholeCanonicalGlyph) {
    auto component = msg::MakeHookProgressMessage({{"hook", msg::HookState::Running,
        std::nullopt, std::nullopt}});
    const auto rendered = strip_ansi(RenderElement(component->Render()).ToString());
    EXPECT_NE(rendered.find(figs::spinner_frame_glyph(0)), std::string::npos);
}

TEST(MessagePromptTheme, AttachmentAndImageChromeKeepsReadableTypeLabels) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        msg::attachment_message::AttachmentItem item;
        item.kind = msg::attachment_message::AttachmentKind::PDF;
        item.name = "manual.pdf";
        auto card = RenderElement(msg::attachment_message::RenderAttachmentCard(item, false, 0));
        const auto rendered = strip_ansi(card.ToString());
        EXPECT_NE(rendered.find("PDF"), std::string::npos);
        EXPECT_EQ(rendered.find('\xF0'), std::string::npos);
        ExpectWordColor(card, "manual.pdf", theme::current_theme().palette->icon_default);
        EXPECT_STREQ(msg::image::source_icon(msg::image::ImageSource::Clipboard), "clipboard");
        EXPECT_STREQ(msg::image::source_icon(msg::image::ImageSource::URL), "url");
    }
}

TEST(MessagePromptTheme, PromptHighlightUsesThemeAndPreservesExplicitMemberColor) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        loom::ui::prompt::CombinedHighlightContext ctx;
        ctx.text = "/help";
        const auto highlights = loom::ui::prompt::build_combined_highlights(ctx);
        ASSERT_FALSE(highlights.empty());
        EXPECT_EQ(highlights.front().color, pal.primary);
        const auto explicit_color = ftxui::Color::RGB(31, 63, 95);
        ctx.text = "@worker";
        ctx.agent_swarms_enabled = true;
        ctx.team_members = {{"worker", explicit_color}};
        const auto member_highlights = loom::ui::prompt::build_combined_highlights(ctx);
        ASSERT_FALSE(member_highlights.empty());
        EXPECT_EQ(member_highlights.front().color, explicit_color);
        auto default_chip = RenderElement(loom::ui::prompt::render_image_chip("[Image #1]"));
        ExpectWordColor(default_chip, "Image", pal.text);
        auto custom_chip = RenderElement(loom::ui::prompt::render_image_chip(
            "[Image #1]", false, explicit_color));
        ExpectWordColor(custom_chip, "Image", explicit_color);
    }
}

TEST(MessagePromptTheme, ToolTimeoutUsesReadableLabelAndWarningToken) {
    RestoreTheme restore;
    for (auto variant : kVariants) {
        theme::set_theme(variant);
        msg::ToolResultOptions options;
        options.tool_name = "Read";
        options.status = msg::ToolResultStatus::Timeout;
        const auto screen = RenderElement(msg::render_tool_result(options));
        ExpectWordColor(screen, "Timeout", theme::current_theme().palette->warning);
        EXPECT_EQ(strip_ansi(screen.ToString()).find("⏱"), std::string::npos);
    }
}

TEST(MessagePromptTheme, ActualUserAndImageHoverKeepsReadableThemeSurface) {
    RestoreTheme restore;
    theme::set_theme(theme::ThemeVariant::Dark);
    int user_clicks = 0;
    int image_clicks = 0;
    msg::UserTextMessageData user_data;
    user_data.content = "HoverBody 🎁";
    auto user = msg::MakeUserTextMessage(user_data, nullptr, [&] { ++user_clicks; });
    msg::image::ImageMessageData image_data;
    image_data.source_type = msg::image::ImageSource::Clipboard;
    image_data.source = "/tmp/capture.png";
    image_data.file_name = "capture.png";
    auto image = msg::image::MakeImageMessage(image_data, [&] { ++image_clicks; });
    ftxui::Mouse mouse;
    mouse.button = ftxui::Mouse::Left;
    mouse.motion = ftxui::Mouse::Released;
    const auto click = ftxui::Event::Mouse("", mouse);
    EXPECT_TRUE(user->OnEvent(click));
    EXPECT_TRUE(image->OnEvent(click));
    EXPECT_EQ(user_clicks, 1);
    EXPECT_EQ(image_clicks, 1);
    for (auto variant : {theme::ThemeVariant::Dark, theme::ThemeVariant::Light,
            theme::ThemeVariant::DarkDaltonized, theme::ThemeVariant::LightDaltonized,
            theme::ThemeVariant::Monochrome, theme::ThemeVariant::DarkAnsi,
            theme::ThemeVariant::LightAnsi}) {
        theme::set_theme(variant);
        const auto& pal = *theme::current_theme().palette;
        auto user_screen = RenderElement(user->Render());
        ExpectWordColor(user_screen, "HoverBody", pal.text, pal.surface_selected);
        EXPECT_NE(strip_ansi(user_screen.ToString()).find(user_data.content), std::string::npos);
        auto image_screen = RenderElement(image->Render());
        ExpectWordColor(image_screen, "capture.png", pal.text_link, pal.surface_selected);
    }
    EXPECT_TRUE(user->OnEvent(click));
    EXPECT_TRUE(image->OnEvent(click));
    EXPECT_EQ(user_clicks, 2);
    EXPECT_EQ(image_clicks, 2);
}
