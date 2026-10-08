// repl_screen_messages.cpp - impl unit for loom.ui.screens.repl_screen
// (RFC 0001 Phase C batch 9). RenderMessages: the single message-row projection from MessageDisplayEntry to the messages_list view input.
//
// Phase A (#184957): textual FTXUI GMF headers + `import std;`
// (the textual FTXUI includes keep the reduced-BMI writer happy).
module;

#include <ftxui/dom/elements.hpp>

module loom.ui.screens.repl_screen;

import std;

import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.types.types;  // arch-check: keep-import (::loom::core::ImageBlockSource)
import loom.ui.messages.message_row;
import loom.ui.messages.message_image;
import loom.ui.messages.messages_list;
import loom.ui.visual.markdown;  // arch-check: keep-import (::loom::ui::StreamingMarkdown)
import loom.ui.messages.user_text_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.system_text_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.local_command_output_message;
import loom.ui.messages.api_error_message;

namespace loom::ui::repl_screen {
using namespace ftxui;

namespace {
// Exact comparisons keep in-place streaming/status changes visible without
// relying on IDs, lengths, hashes, or append-only source rows.
bool same_projected_source(const MessageDisplayEntry& a,
                           const MessageDisplayEntry& b) {
    const auto fields = [](const MessageDisplayEntry& m) {
        return std::tie(m.id, m.role, m.content_preview, m.full_content,
            m.is_streaming, m.is_thinking, m.is_tool_use, m.thinking_active,
            m.is_local_command_input, m.is_local_command_output,
            m.is_local_jsx_output, m.is_compact_boundary, m.is_error,
            m.is_image, m.image_display_id, m.tool_name, m.tool_status,
            m.tool_input_json, m.tool_result_preview, m.agent_display_name,
            m.agent_color_name, m.timestamp, m.thinking_duration,
            m.estimated_height_lines, m.system_subtype, m.retry_after_ms,
            m.retry_attempt, m.max_retries, m.session_expired);
    };
    if (fields(a) != fields(b)) return false;
    if (a.image_block.has_value() != b.image_block.has_value()) return false;
    if (a.image_block) {
        const auto image_fields = [](const ::loom::core::ImageBlock& image) {
            return std::tie(image.media_type, image.width,
                image.height, image.size_bytes, image.file_name,
                image.source_path, image.source);
        };
        if (image_fields(*a.image_block) != image_fields(*b.image_block))
            return false;
    }
    if (a.tool_result_content_items.has_value() !=
        b.tool_result_content_items.has_value()) return false;
    if (a.tool_result_content_items) {
        return std::ranges::equal(*a.tool_result_content_items,
            *b.tool_result_content_items, [](const auto& x, const auto& y) {
                return std::tie(x.type, x.text, x.media_type, x.data) ==
                       std::tie(y.type, y.text, y.media_type, y.data);
            });
    }
    return true;
}
}  // namespace

// UI4/UI5: message list.  Delegates to messages_list.cppm (UI21).
// `spinner_frame` drives the tool-use header spinner animation (fix #10).
// `unseen_divider` is the optional in-transcript "N new messages" anchor
// set by the unseen-divider tracker; nullopt when
// pinned to bottom or the session has no scroll-away yet.
[[nodiscard]] Element RenderMessages(
    const std::vector<MessageDisplayEntry>& entries,
    int sel, int vlines,
    int offs, bool pinned,
    int spinner_frame,
    std::optional<loom::ui::messages_list::UnseenDivider> unseen_divider,
    Elements leading_elements,
    bool is_brief_mode,
    const std::unordered_set<std::string>& expanded_keys,
    bool is_transcript_mode,
    bool show_all_in_transcript,
    bool disable_chain_compression,
    bool query_running,
    // GAP 3: msg-system-api-error-retry — callback for the Retry button on
    // SystemAPIError rich cards.  When set, the API error card renders a
    // clickable Retry pill that invokes this to re-send the last user message.
    std::function<void()> on_retry,
    // P2 gap api-error-retry: callback for "Clear session" button on
    // session-expired error cards.  onClearSession prop — invoked when
    // auth has expired and user chooses to clear the session to
    // re-authenticate.
    std::function<void()> on_clear_session,
    // StreamingMarkdown stable-prefix cache for the streaming-text tail
    // row.  When non-null,
    // RenderAssistantTextMessageFaithful uses update() instead of full
    // render_markdown() for is_streaming rows.
    ::loom::ui::StreamingMarkdown* streaming_md,
    // Optional mouse hit-testing tracker for click-to-expand rows.
    loom::ui::messages_list::RowClickTracker* row_click_tracker,
    // Optional store for virtual-list scroll bounds.
    MessagesStore* store) {
    // NOTE: We no longer early-return on empty entries.  The leading_element
    // (welcome/logo card) must always be rendered inside the yframe so it
    // scrolls with messages.  The messages_list handles empty rows gracefully
    // via its own visible.empty() path which prepends leading elements.

    namespace ml = loom::ui::messages_list;
    namespace image = loom::ui::messages::image;
    ml::MessagesListInput transient_input;
    auto& input = store ? store->render_input : transient_input;
    const bool transcript_changed = store &&
        store->render_sources_transcript_mode != is_transcript_mode;
    const auto cached_count = store ? store->render_sources.size() : 0;
    if (input.rows.size() > entries.size()) {
        input.rows.resize(entries.size());
        input.shapes.resize(entries.size());
        input.uuids.resize(entries.size());
        input.lowered_search_cache.resize(entries.size());
    }
    input.rows.reserve(entries.size());
    input.shapes.reserve(entries.size());
    input.uuids.reserve(entries.size());
    input.unseen_divider = std::move(unseen_divider);
    input.selected_row_idx.reset();
    input.virtual_jh_out.reset();
    if (store) store->render_sources.resize(entries.size());

    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& m = entries[index];
        if (store && index < cached_count && !transcript_changed &&
            same_projected_source(m, store->render_sources[index])) {
            // Callbacks can change independently of the source message.
            if (auto* error = std::get_if<messages::api_error_message::APIErrorOptions>(
                    &input.rows[index])) {
                error->on_retry = on_retry;
                error->on_clear_session = on_clear_session;
            }
            continue;
        }
        ml::MessagesListInput projected;
        // uuid → 24-char prefix anchor.
        // Populated parallel to rows/shapes; empty strings are harmless
        // (find_divider_before_visible_index skips them).
        projected.uuids.push_back(m.id);
        if (m.is_local_command_input) {
            projected.shapes.push_back(messages::MessageShape::UserCommand);
            projected.rows.push_back(messages::UserTextMessageData{
                .content = m.content_preview,
                .timestamp = m.timestamp,
                .quoted_reply = std::nullopt,
                .is_transcript_mode = is_transcript_mode,
                .command_name = std::nullopt});
        } else if (m.is_local_jsx_output) {
            projected.shapes.push_back(messages::MessageShape::UserLocalJsxOutput);
            projected.rows.push_back(messages::UserTextMessageData{
                .content = m.content_preview,
                .timestamp = m.timestamp,
                .quoted_reply = std::nullopt,
                .is_transcript_mode = is_transcript_mode,
                .command_name = std::nullopt});
        } else if (m.is_local_command_output) {
            projected.shapes.push_back(messages::MessageShape::UserLocalCommandOutput);
            messages::local_cmd::LocalCommandOptions opts;
            opts.show_line_numbers = false;
            opts.data.exit_code = m.is_error ? 1 : 0;

            std::size_t start = 0;
            while (start <= m.content_preview.size()) {
                auto nl = m.content_preview.find('\n', start);
                std::string line = nl == std::string::npos
                    ? m.content_preview.substr(start)
                    : m.content_preview.substr(start, nl - start);
                opts.data.lines.push_back(messages::local_cmd::OutputLine{
                    .kind = m.is_error
                        ? messages::local_cmd::StreamKind::Stderr
                        : messages::local_cmd::StreamKind::Stdout,
                    .text = std::move(line),
                });
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            projected.rows.push_back(std::move(opts));
        } else if (m.role == "user") {
            if (m.is_image && m.image_block) {
                // Each user-attached image is its own UserImage row.
                // The data flow is: project_messages() → is_image=true +
                // image_block; we translate the block metadata into
                // image::ImageMessageData which message_image.cppm already
                // knows how to render.
                using image::ImageMessageData;
                using image::ImageSource;
                ImageMessageData d;
                const auto& ib = *m.image_block;
                d.timestamp = m.timestamp;
                d.media_type = ib.media_type;
                // [Image #N] label — use the display id assigned
                // by project_messages from content-block order (matches
                // user's paste order shown in the input placeholder).
                if (m.image_display_id) {
                    d.image_id = std::to_string(*m.image_display_id);
                }
                if (ib.file_name)        d.file_name = *ib.file_name;
                if (ib.source_path)      d.source = *ib.source_path;
                if (ib.width)            d.width = *ib.width;
                if (ib.height)           d.height = *ib.height;
                if (ib.size_bytes)       d.file_size = *ib.size_bytes;
                switch (ib.source) {
                    using IS = ::loom::core::ImageBlockSource;
                    case IS::Clipboard: d.source_type = ImageSource::Clipboard; break;
                    case IS::File:      d.source_type = ImageSource::File; break;
                    case IS::Base64:    d.source_type = ImageSource::Base64; break;
                    case IS::Unknown:
                    default:
                        // Heuristic: empty source + non-empty data means a
                        // raw inline paste (no file path known).
                        d.source_type = d.source.empty()
                            ? ImageSource::Clipboard
                            : ImageSource::File;
                        break;
                }
                // NOTE: deliberately do NOT stuff ib.data into alt_text.
                // The base64 PNG prefix "iVBORw0KGgo..." rendered as "Alt: ..."
                // is worse than useless — wastes a line and confuses users.
                // Removed 2026-07-04 per spacing bug report.
                projected.shapes.push_back(messages::MessageShape::UserImage);
                projected.rows.push_back(std::move(d));
            } else {
                projected.shapes.push_back(messages::MessageShape::UserText);
                projected.rows.push_back(messages::UserTextMessageData{
                    .content = m.content_preview,
                    .timestamp = m.timestamp,
                    .quoted_reply = std::nullopt,
                    .is_transcript_mode = is_transcript_mode,
                    .command_name = std::nullopt});
            }
        } else if (m.role == "assistant") {
            if (m.is_thinking) {
                projected.shapes.push_back(messages::MessageShape::AssistantThinking);
                messages::thinking_message::ThinkingMessageOptions opts;
                opts.data.raw_text = m.full_content.empty()
                    ? m.content_preview : m.full_content;
                // Thread the thinking duration so the collapsed label shows
                // "∴ Thought for Xs" instead of a generic "Thinking".
                opts.data.duration = m.thinking_duration;
                // Thread the wall-clock start time so the chain compressor's
                // live elapsed timer ticks in real time during streaming.
                opts.data.thinking_start_time = m.thinking_start_time;
                // Static / unselected view renders the collapsed "Thinking"
                // label (collapsed state) — the row is never hidden. The
                // 3s collapse grace (was_recently_streaming) keeps a
                // just-finished thinking row expanded before it collapses.
                if (m.thinking_active) {
                    opts.data.state =
                        messages::thinking_message::ThinkingState::Active;
                }
                projected.rows.push_back(std::move(opts));
            } else if (m.is_tool_use) {
                projected.shapes.push_back(messages::MessageShape::AssistantToolUse);
                messages::tool_use_message::ToolUseRenderOptions opts;
                opts.call.tool_name = m.tool_name.value_or("tool");
                // Fix #8/#9: thread the real parsed tool input + status from
                // the shared projection contract into the renderer (previously
                // raw_parameters fell back to content_preview and status was
                // hard-coded Pending).
                opts.call.raw_parameters =
                    m.tool_input_json.value_or(m.content_preview);
                // M6 result_preview: thread live streaming result preview into
                // the call data so ToolUIRegistry.progress() can format a
                // dynamic progress line (e.g. last output line for Bash tools).
                if (m.tool_result_preview) {
                    opts.call.result_preview = *m.tool_result_preview;
                }
                opts.call.parameters_language = "json";
                opts.call.status = messages::tool_use_message::parse_tool_status(
                    m.tool_status.value_or("pending"));
                projected.rows.push_back(std::move(opts));
            } else {
                projected.shapes.push_back(messages::MessageShape::AssistantText);
                projected.rows.push_back(messages::AssistantTextMessageData{
                    .content = m.content_preview,
                    .timestamp = m.timestamp,
                    .model_name = std::nullopt,
                    .is_streaming = m.is_streaming});
            }
        } else if (m.role == "tool") {
            projected.shapes.push_back(messages::MessageShape::UserToolResult);
            projected.rows.push_back(messages::ToolResultOptions{
                .tool_name = m.tool_name.value_or("tool"),
                .status = m.is_error
                    ? messages::ToolResultStatus::Error
                    : messages::ToolResultStatus::Success,
                .output = m.content_preview,
                .error_message = std::nullopt,
                .duration_ms = std::nullopt,
                .is_truncated = false,
                .is_transcript_mode = is_transcript_mode,
                .content_items = m.tool_result_content_items});
        } else if (m.is_error) {
            // GAP 3: msg-system-api-error-retry — route system error messages
            // through the rich SystemAPIError card (severity borders + pills)
            // instead of the plain SystemText glyph.  The Retry button calls
            // on_retry to re-send the last user message.
            namespace aem = loom::ui::messages::api_error_message;
            aem::APIErrorData err_data;
            err_data.message = m.content_preview;
            err_data.provider = "API";
            err_data.severity = aem::ErrorSeverity::Error;
            // P2 gap api-error-retry: thread retry metadata from the entry.
            // retryInMs, retryAttempt,
            //   maxRetries, sessionExpired destructured from message prop.
            err_data.retry_after_ms   = m.retry_after_ms;
            err_data.current_attempt  = m.retry_attempt;
            err_data.max_attempts     = m.max_retries;
            err_data.session_expired  = m.session_expired;
            aem::APIErrorOptions err_opts;
            err_opts.error = std::move(err_data);
            err_opts.on_retry = on_retry;
            // P2 gap: clear-session callback for expired auth sessions.
            // Wired through ReplScreenCallbacks.on_clear_session below.
            err_opts.on_clear_session = on_clear_session;
            err_opts.show_buttons = true;
            projected.shapes.push_back(messages::MessageShape::SystemAPIError);
            projected.rows.push_back(std::move(err_opts));
        } else {
            projected.shapes.push_back(messages::MessageShape::SystemText);
            // Bridge the system-row subtype so the LIVE faithful renderer
            // (RenderSystemTextMessageFaithful dispatches per subtype) shows
            // the right glyph (※ away_summary / ✻ event / ⏺ generic).  When
            // the engine hasn't set system_subtype we derive a best-effort
            // subtype from the preview text — same labels SystemTextMessage
            // keys on (turn_duration / memory_saved / etc.).
            auto derive_subtype = [](const std::string& s,
                const std::optional<std::string>& hint) {
                using ST = messages::SystemMessageSubtype;
                if (hint) {
                    if (*hint == "away_summary")    return ST::AwaySummary;
                    if (*hint == "turn_duration")   return ST::TurnDuration;
                    if (*hint == "memory_saved")    return ST::MemorySaved;
                    if (*hint == "bridge_status")   return ST::BridgeStatus;
                    if (*hint == "thinking_summary")return ST::ThinkingSummary;
                    if (*hint == "hook_summary")    return ST::HookSummary;
                    if (*hint == "model_switch")    return ST::ModelSwitch;
                    if (*hint == "background_task") return ST::BackgroundTask;
                }
                // Heuristic fallback: scan preview text for keywords.
                if (s.find("away for") != std::string::npos ||
                    s.find("Welcome back") != std::string::npos)
                    return ST::AwaySummary;
                if (s.find("took") != std::string::npos ||
                    s.find("duration") != std::string::npos ||
                    s.find("seconds") != std::string::npos)
                    return ST::TurnDuration;
                if (s.find("memory") != std::string::npos ||
                    s.find("saved") != std::string::npos)
                    return ST::MemorySaved;
                if (s.find("bridge") != std::string::npos ||
                    s.find("IDE") != std::string::npos)
                    return ST::BridgeStatus;
                if (s.find("model") != std::string::npos &&
                    (s.find("switch") != std::string::npos ||
                     s.find("changed") != std::string::npos))
                    return ST::ModelSwitch;
                return ST::Plain;
            };
            projected.rows.push_back(messages::SystemTextMessageData{
                .subtype = derive_subtype(m.content_preview, m.system_subtype),
                .summary = m.content_preview,
                .detail = {},
                .timestamp = m.timestamp,
                .is_transcript_mode = is_transcript_mode});
        }

        if (index < input.rows.size()) {
            input.rows[index] = std::move(projected.rows.front());
            input.shapes[index] = projected.shapes.front();
            input.uuids[index] = std::move(projected.uuids.front());
        } else {
            input.rows.push_back(std::move(projected.rows.front()));
            input.shapes.push_back(projected.shapes.front());
            input.uuids.push_back(std::move(projected.uuids.front()));
        }
        if (index < input.lowered_search_cache.size())
            input.lowered_search_cache[index].reset();
        if (store) {
            auto& cached_source = store->render_sources[index];
            cached_source = m;
            if (cached_source.image_block) {
                // Image rows consume metadata only. Release the base64 buffer
                // rather than retaining another copy in the render cache.
                std::string{}.swap(cached_source.image_block->data);
            }
        }
    }
    if (store) store->render_sources_transcript_mode = is_transcript_mode;

    if (sel >= 0)
        input.selected_row_idx = static_cast<std::size_t>(sel);

    const auto N = entries.size();
    // Streaming text + streaming thinking
    // tails are rendered after all committed messages.  The tail row is
    // whichever comes last: a streaming text entry (is_streaming) or an
    // active thinking entry (thinking_active, set while streaming or
    // within 3s grace).  The tail row drives the "Running" status badge
    // and keeps thinking rows expanded while streaming.
    bool has_streaming = !entries.empty() &&
        (entries.back().is_streaming || entries.back().thinking_active);
    input.streaming_tail_row = has_streaming ? N - 1 : N;
    input.pin_to_bottom = pinned;
    input.scroll_offset = std::max(0, offs);
    input.viewport_rows = std::max(1, vlines);
    input.is_brief_mode = is_brief_mode;
    // is_transcript_mode + show_all_in_transcript.
    // In transcript mode the 3-tier filter shows all message types; cap at
    // 30 unless show_all_in_transcript lifts it.
    input.is_transcript_mode     = is_transcript_mode;
    input.show_all_in_transcript = show_all_in_transcript;
    input.disable_chain_compression = disable_chain_compression;
    input.query_running = query_running;
    // expanded_keys — user-expanded rows show
    // verbose full content.  Passed by copy (cheap for small sets).
    input.expanded_keys = expanded_keys;
    // GAP 3: thread on_retry through to the messages list so SystemAPIError
    // rows can render a working Retry button.
    input.on_retry = on_retry;
    // P2 gap api-error-retry: thread on_clear_session for session-expired
    // error cards.
    input.on_clear_session = on_clear_session;
    // Thread the shared StreamingMarkdown instance so the streaming-text
    // tail row uses
    // stable-prefix caching instead of full re-parse per token.
    input.streaming_md = streaming_md;
    input.row_click_tracker = row_click_tracker;
    namespace ml = loom::ui::messages_list;
    // A flex-grow filler at the bottom of the
    // message list absorbs remaining viewport space so short content stays
    // compact at the top (logo + messages adjacent, no blank gap between).
    // Without this filler, the yframe viewport is full-height but the inner
    // vbox is content-sized; when pin-to-bottom is off, FTXUI top-aligns the
    // inner vbox which is correct, but the filler ensures the scroll
    // indicator reflects "content fits" rather than "content is short".
    auto element = ml::render_messages_list_view(
        input,
        static_cast<std::size_t>(spinner_frame),
        ml::kMaxRenderedLastN,
        {filler()},  // trailing_elements — elastic spacer (flex-grow)
        true,        // wrap_in_yframe
        std::move(leading_elements));
    // Propagate virtual-list geometry to the store so ScrollTranscript uses
    // exact jh.total() instead of the EstimateTranscriptRows heuristic.
    // This eliminates the "first few wheel-up events from bottom have no
    // effect" dead-zone caused by heuristic overestimation.
    if (store) {
        if (input.virtual_jh_out) {
            store->virtual_list_active = true;
            store->virtual_jh = std::move(*input.virtual_jh_out);
        } else {
            store->virtual_list_active = false;
        }
    }
    return std::move(element) | flex;
}

}  // namespace loom::ui::repl_screen
