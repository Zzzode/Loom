// message_row_dispatch.cpp — impl unit for loom.ui.messages.message_row
//
// RFC 0002 P2-2b: the 228-line RenderMessageRowByType body is moved out of
// the .cppm BMI into this impl unit.  The .cppm still imports all 24
// renderers (the MessageRowPayload variant needs their types), but the BMI
// no longer carries the dispatch function body — BMI rebuilds are faster
// and the dispatch logic can be edited without touching the interface.

module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

module loom.ui.messages.message_row;

import std;

import loom.ui.messages.message_components;
import loom.ui.messages.message_timestamp;

// Nine UI4-owned generic types:
import loom.ui.messages.user_text_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.system_text_message;
import loom.ui.messages.message_user_command;
import loom.ui.messages.message_rate_limit;
import loom.ui.messages.message_shutdown;
import loom.ui.messages.message_plan_approval;
import loom.ui.messages.message_hook_progress;
import loom.ui.messages.message_advisor;

// Flavours + shared modules:
import loom.ui.messages.message_bash_io;
import loom.ui.messages.message_channel;
import loom.ui.messages.message_image;
import loom.ui.messages.message_task_assignment;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.user_message;
import loom.ui.messages.assistant_message;
import loom.ui.messages.error_message;

// UI5 complex types:
import loom.ui.messages.tool_use_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.attachment_message;
import loom.ui.messages.api_error_message;
import loom.ui.messages.collapsed_content_message;
import loom.ui.messages.local_command_output_message;

namespace loom::ui::messages {

using namespace ftxui;

[[nodiscard]] Component RenderMessageRowByType(
    MessageShape shape,
    MessageRowPayload payload,
    MessageRowCallbacks callbacks)
{
    using S = MessageShape;

    // ---------------------------------------------------------------------
    // USER family
    // ---------------------------------------------------------------------
    if (shape == S::UserText) {
        auto data = std::get_if<UserTextMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for UserText"); });
        return MakeUserTextMessage(*data,
            std::move(callbacks.on_copy),
            std::move(callbacks.on_click));
    }

    if (shape == S::UserCommand) {
        auto data = std::get_if<user_command::UserCommandData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for UserCommand"); });
        return user_command::MakeUserCommandMessage(*data,
            std::move(callbacks.on_copy));
    }

    if (shape == S::UserBashInput || shape == S::UserBashOutput) {
        auto data = std::get_if<BashIOEntry>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for BashIO"); });
        return ftxui::Renderer([d = *data] { return render_bash_io(d); });
    }

    // Faithful: UserLocalCommandOutput — ⎿ prefix + ANSI passthrough, no line numbers, no borders.
    if (shape == S::UserLocalCommandOutput) {
        auto data = std::get_if<local_cmd::LocalCommandOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for LocalCmd"); });
        auto d = *data;
        return ftxui::Renderer([d] {
            return local_cmd::RenderLocalCommandOutputFaithful(d);
        });
    }

    // UI5: UserAttachments — card grid with pagination + ASCII thumbnails
    if (shape == S::UserAttachments) {
        auto data = std::get_if<attachment_message::AttachmentGridOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for Attachments"); });
        return attachment_message::AttachmentGrid(*data);
    }

    if (shape == S::UserTeammate || shape == S::UserPrompt ||
        shape == S::UserPlan     || shape == S::UserAgentNotification ||
        shape == S::UserMemoryInput || shape == S::UserResourceUpdate) {
        // All share the user_message base module.  Add a flavoured prefix.
        auto data = std::get_if<UserMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for user-flavour"); });
        std::string tag;
        switch (shape) {
            case S::UserTeammate:          tag = "[teammate] "; break;
            case S::UserPrompt:            tag = "[prompt] ";   break;
            case S::UserPlan:              tag = "[plan] ";     break;
            case S::UserAgentNotification: tag = "[agent-notify] "; break;
            case S::UserMemoryInput:       tag = "[memory] ";   break;
            case S::UserResourceUpdate:    tag = "[resource] "; break;
            default: break;
        }
        return ftxui::Renderer([tag, d = *data] {
            return vbox({
                text(tag + render_user_message(d)),
            });
        });
    }

    if (shape == S::UserToolResult) {
        // UI5 owns the *rich* tool-result rendering (code blocks, diffs).
        // The basic status + name + duration view is provided here as a
        // reasonable fallback using message_tool_result.cppm.
        auto data = std::get_if<ToolResultOptions>(&payload);
        if (!data) {
            return ftxui::Renderer([] {
                return vbox({
                    text("⚡ Tool result") | color(Color::Cyan),
                    text("(detailed view → UI5 message_tool_result)") | dim,
                });
            });
        }
        return ftxui::Renderer([d = *data] { return render_tool_result(d); });
    }

    // ---------------------------------------------------------------------
    // ASSISTANT family
    // ---------------------------------------------------------------------
    if (shape == S::AssistantText) {
        auto data = std::get_if<AssistantTextMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for AsstText"); });
        return MakeAssistantTextMessage(*data,
            std::move(callbacks.on_copy),
            std::move(callbacks.on_regenerate),
            std::move(callbacks.on_rate_limit_opts));
    }

    // ---------------------------------------------------------------------
    // ASSISTANT family — UI5-owned complex types (real dispatch, no placeholder)
    // ---------------------------------------------------------------------
    if (shape == S::AssistantToolUse) {
        auto data = std::get_if<tool_use_message::ToolUseRenderOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for AsstToolUse"); });
        return tool_use_message::ToolUseMessage(*data);
    }
    if (shape == S::AssistantGroupedTools) {
        auto data = std::get_if<tool_use_message::GroupedToolsOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for AsstGroupedTools"); });
        return tool_use_message::GroupedToolsComponent(*data);
    }
    if (shape == S::AssistantThinking) {
        auto data = std::get_if<thinking_message::ThinkingMessageOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for AsstThinking"); });
        return thinking_message::ThinkingMessage(*data);
    }
    if (shape == S::AssistantRedactedThinking) {
        auto data = std::get_if<thinking_message::ThinkingMessageOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for RedactedThinking"); });
        return thinking_message::ThinkingMessage(*data);
    }

    // ---------------------------------------------------------------------
    // SYSTEM family
    // ---------------------------------------------------------------------
    if (shape == S::SystemText) {
        auto data = std::get_if<SystemTextMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for SystemText"); });
        return MakeSystemTextMessage(*data, std::move(callbacks.on_toggle));
    }

    if (shape == S::SystemRateLimit) {
        auto data = std::get_if<RateLimitInfo>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for RateLimit"); });
        return MakeRateLimitMessage(*data,
            std::move(callbacks.on_retry),
            std::move(callbacks.on_rate_limit_opts));
    }

    if (shape == S::SystemPlanApproval) {
        auto data = std::get_if<PlanApprovalOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for PlanApproval"); });
        return MakePlanApprovalMessage(*data,
            std::move(callbacks.on_approve),
            std::move(callbacks.on_modify),
            std::move(callbacks.on_reject));
    }

    if (shape == S::SystemHookProgress) {
        auto data = std::get_if<std::vector<HookProgressEntry>>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for HookProgress"); });
        return MakeHookProgressMessage(*data, std::move(callbacks.on_dismiss));
    }

    if (shape == S::SystemShutdown) {
        auto data = std::get_if<shutdown::ShutdownMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for Shutdown"); });
        return shutdown::MakeShutdownMessage(*data,
            std::move(callbacks.on_resume),
            std::move(callbacks.on_new_session));
    }

    if (shape == S::SystemCompactBoundary) {
        // message_compact_boundary.cppm — stateless; just forward
        return ftxui::Renderer([] {
            return vbox({
                text("· · · compacted turn · · ·") | center | dim,
                separator() | dim,
            });
        });
    }

    if (shape == S::SystemAdvisor) {
        auto data = std::get_if<AdvisorMessage>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for Advisor"); });
        return MakeAdvisorMessage(*data,
            std::move(callbacks.on_dismiss),
            std::move(callbacks.on_action));
    }

    if (shape == S::SystemTaskAssignment) {
        return ftxui::Renderer([] {
            return vbox({
                text("📋 task assignment") | color(Color::Cyan),
                text("(rendered by message_task_assignment.cppm)") | dim,
            });
        });
    }

    if (shape == S::SystemAPIError) {
        auto data = std::get_if<api_error_message::APIErrorOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for API error"); });
        return api_error_message::APIErrorMessage(*data);
    }

    if (shape == S::SystemCollapsedContent) {
        auto data = std::get_if<collapsed_content::CollapsedContentOptions>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for CollapsedContent"); });
        return collapsed_content::CollapsedContentMessage(*data);
    }

    if (shape == S::UserChannel) {
        return ftxui::Renderer([] {
            return vbox({
                text("📡 channel message") | color(Color::Magenta),
                text("(rendered by message_channel.cppm)") | dim,
            });
        });
    }

    if (shape == S::UserImage) {
        auto data = std::get_if<image::ImageMessageData>(&payload);
        if (!data) return ftxui::Renderer([=] { return text("⚠ message_row: bad payload for UserImage"); });
        return image::MakeImageMessage(*data,
            std::move(callbacks.on_click),
            std::move(callbacks.on_copy));
    }

    // Unknown / future shape
    return ftxui::Renderer([shape] {
        return vbox({
            text("? " + std::string(MessageShapeToString(shape))) | color(Color::Yellow),
            text("(no renderer registered — message_row.cppm needs update)") | dim,
        });
    });
}

} // namespace loom::ui::messages
