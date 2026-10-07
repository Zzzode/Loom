/// =========================================================================
/// @file message_row.cppm
/// @brief Message row framework: enum of known message roles/types and
///        dispatch to per-type renderers.
///
/// PHASE 4 UI4 RESPONSIBILITY - "List framework + generic message types"
///   - Provide an enum of known message roles/types
///   - RenderMessageRowByType() dispatches by (role, type) → correct module
///
/// COMPLEX MESSAGE TYPES handled by UI5 Agent (NOT TOUCHED HERE):
///   - AssistantToolUse / Thinking / Attachments / RedactedThinking
///   - SystemAPIError / UserToolResult / StructuredDiff / ToolUseLoader
///
/// FULL MARKDOWN rendering + syntax highlighting → UI17 (NOT HERE).
/// =========================================================================
module;

#include <ctime>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

export module loom.ui.messages.message_row;

import std;

// --- Import per-type renderers ------------------------------------------
import loom.ui.messages.message_timestamp;
import loom.ui.messages.message_components;

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
import loom.ui.messages.message_tool_result;   // exports ToolResultOptions
import loom.ui.messages.user_message;
import loom.ui.messages.assistant_message;
import loom.ui.messages.error_message;

// =========================================================================
// SIX COMPLEX MESSAGE TYPES — UI5 Agent
// =========================================================================
// These modules are the UI5 deliverables for the complex interactivity
// bucket.  Dispatch below in RenderMessageRowByType routes each MessageShape
// through the dedicated stand-alone free function Render*().
//
// UI4-owned user/assistant/system wrappers keep outer chrome (avatar,
// role-badge, metadata row) while these branches forward complex content
// payloads directly to the UI5 renderers.
import loom.ui.messages.tool_use_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.attachment_message;
import loom.ui.messages.api_error_message;
import loom.ui.messages.collapsed_content_message;
import loom.ui.messages.local_command_output_message;

export namespace loom::ui::messages {

using namespace ftxui;

// =========================================================================
// 2) Dispatch enum (role × type)
// =========================================================================

/// Every known message shape.  A single enum flattens the (role, sub_type)
/// cross-product so dispatch logic reads cleanly.
enum class MessageShape {
    // --- User family ---
    UserText,
    UserCommand,
    UserBashInput,
    UserBashOutput,
    UserLocalCommandOutput,    // UI5: dedicated local_command_output_message
    UserLocalJsxOutput,
    UserTeammate,
    UserChannel,
    UserAgentNotification,
    UserMemoryInput,
    UserPlan,
    UserPrompt,
    UserResourceUpdate,
    UserImage,
    UserToolResult,
    UserAttachments,           // UI5: attachment_message (card grid)

    // --- Assistant family ---
    AssistantText,
    AssistantToolUse,          // UI5 responsibility (tool_use_message)
    AssistantThinking,         // UI5 responsibility (thinking_message plain)
    AssistantRedactedThinking, // UI5 responsibility (thinking_message redacted mode)
    AssistantGroupedTools,     // UI5 responsibility (tool_use_message::Grouped)

    // --- System family ---
    SystemText,
    SystemAPIError,            // UI5: api_error_message (severity borders + pills)
    SystemCollapsedContent,    // UI5: collapsed_content_message (Read/Search aggregate)
    SystemRateLimit,
    SystemPlanApproval,
    SystemHookProgress,
    SystemShutdown,
    SystemCompactBoundary,
    SystemAdvisor,
    SystemTaskAssignment,
};

inline auto MessageShapeToString(MessageShape s) -> std::string_view {
    switch (s) {
        case MessageShape::UserText:                 return "user.text";
        case MessageShape::UserCommand:              return "user.command";
        case MessageShape::UserBashInput:            return "user.bash_input";
        case MessageShape::UserBashOutput:           return "user.bash_output";
        case MessageShape::UserLocalCommandOutput:   return "user.local_command_output";
        case MessageShape::UserLocalJsxOutput:       return "user.local_jsx_output";
        case MessageShape::UserTeammate:             return "user.teammate";
        case MessageShape::UserChannel:              return "user.channel";
        case MessageShape::UserAgentNotification:    return "user.agent_notification";
        case MessageShape::UserMemoryInput:          return "user.memory_input";
        case MessageShape::UserPlan:                 return "user.plan";
        case MessageShape::UserPrompt:               return "user.prompt";
        case MessageShape::UserResourceUpdate:       return "user.resource_update";
        case MessageShape::UserImage:                return "user.image";
        case MessageShape::UserToolResult:           return "user.tool_result";
        case MessageShape::UserAttachments:          return "user.attachments";
        case MessageShape::AssistantText:            return "assistant.text";
        case MessageShape::AssistantToolUse:         return "assistant.tool_use";
        case MessageShape::AssistantGroupedTools:    return "assistant.grouped_tools";
        case MessageShape::AssistantThinking:        return "assistant.thinking";
        case MessageShape::AssistantRedactedThinking: return "assistant.redacted_thinking";
        case MessageShape::SystemText:               return "system.text";
        case MessageShape::SystemAPIError:           return "system.api_error";
        case MessageShape::SystemCollapsedContent:   return "system.collapsed_content";
        case MessageShape::SystemRateLimit:          return "system.rate_limit";
        case MessageShape::SystemPlanApproval:       return "system.plan_approval";
        case MessageShape::SystemHookProgress:       return "system.hook_progress";
        case MessageShape::SystemShutdown:           return "system.shutdown";
        case MessageShape::SystemCompactBoundary:    return "system.compact_boundary";
        case MessageShape::SystemAdvisor:            return "system.advisor";
        case MessageShape::SystemTaskAssignment:     return "system.task_assignment";
    }
    return "unknown";
}

// =========================================================================
// 3) Generic variant payload — pure struct inputs, NO AppState globals.
// =========================================================================

/// Empty tag for sub-types whose payloads are UI5-owned.
struct UI5HandledTag {};

/// MessageRowPayload = std::variant over per-message-type structs.
/// Modules not owned by UI4 use `UI5HandledTag` so dispatch still compiles
/// and can return a graceful diagnostic for a mismatched payload.
using MessageRowPayload = std::variant<
    // User
    UserTextMessageData,
    user_command::UserCommandData,
    BashIOEntry,                          // bash input / output
    UserMessageData,                      // teammate / prompt / plan / etc.
    image::ImageMessageData,                     // image attachment
    ToolResultOptions,                    // user-facing tool result
    local_cmd::LocalCommandOptions,       // UI5: UserLocalCommandOutput
    attachment_message::AttachmentGridOptions, // UI5: UserAttachments

    // Assistant
    AssistantTextMessageData,
    tool_use_message::ToolUseRenderOptions,   // UI5: AssistantToolUse
    tool_use_message::GroupedToolsOptions,    // UI5: AssistantGroupedTools
    thinking_message::ThinkingMessageOptions, // UI5: AssistantThinking (+ Redacted)

    // System
    SystemTextMessageData,
    ErrorMessageData,                     // API error (basic struct)
    api_error_message::APIErrorOptions,   // UI5: SystemAPIError (rich card)
    collapsed_content::CollapsedContentOptions, // UI5: SystemCollapsedContent
    RateLimitInfo,
    PlanApprovalOptions,
    std::vector<HookProgressEntry>,       // hook progress list
    shutdown::ShutdownMessageData,
    AdvisorMessage
>;

// =========================================================================
// 4) Dispatch: RenderMessageRowByType
// =========================================================================

/// Callbacks the engine injects (all optional).  UI4 types hook into these;
/// UI5-owned sub-types pass callbacks through via a secondary dispatch.
struct MessageRowCallbacks {
    std::function<void(std::string_view text)>       on_copy;
    std::function<void()>                            on_click;
    std::function<void()>                            on_retry;          // RL
    std::function<void()>                            on_clear_session;  // P2 api-error-retry
    std::function<void()>                            on_rate_limit_opts; // RL
    std::function<void()>                            on_regenerate;     // Asst
    std::function<void(bool expanded)>               on_toggle;         // System
    std::function<void()>                            on_resume;         // Shutdown
    std::function<void()>                            on_new_session;    // Shutdown
    std::function<void()>                            on_approve;        // Plan
    std::function<void(std::vector<PlanStep>)>       on_modify;         // Plan
    std::function<void(std::string_view reason)>     on_reject;         // Plan
    std::function<void()>                            on_dismiss;        // Advisor / Hook
    std::function<void()>                            on_action;         // Advisor
};

/// Result of dispatch: a Component tree ready for insertion into the screen.
/// Always returns a valid Component (never null).  UI5-owned sub-types
/// return a greyed-out placeholder reading "[UI5: foo_message]".
///
/// P2-2b: the 228-line body lives in message_row_dispatch.cpp (impl unit)
/// so the BMI no longer carries the dispatch logic.
[[nodiscard]] Component RenderMessageRowByType(
    MessageShape shape,
    MessageRowPayload payload,
    MessageRowCallbacks callbacks = {});

} // namespace loom::ui::messages
