// messages_list_payload_row.cpp - impl unit for loom.ui.messages.messages_list
// (RFC 0001 Phase C batch 7). The single detail::render_payload_row body in
// its own TU to cap peak source-location/PSS: it fans out to every faithful
// per-type message renderer plus the tool UI registry.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

module loom.ui.messages.messages_list;

import std;

import loom.ui.foundation.clock;
import loom.ui.messages.message_row;
import loom.ui.messages.user_text_message;
import loom.ui.messages.local_command_output_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.thinking_message;
import loom.ui.messages.system_text_message;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.message_image;
import loom.ui.tools.registry;
import loom.ui.tools.generic;

namespace loom::ui::messages_list {

namespace detail {

// Per-row "grace period" after streaming ends: the thinking block stays
// expanded for this long before collapsing, so the user has time to read
// the reasoning instead of it vanishing instantly.
//
// Maps row_idx → steady_clock timestamp of the last frame where the row
// was the streaming tail.  Entries older than the grace period are pruned
// on access.
inline constexpr auto kThinkingCollapseGrace = std::chrono::seconds(3);
inline std::unordered_map<std::size_t, std::chrono::steady_clock::time_point>
    thinking_stream_last_seen;

/// Return true if the thinking row at `row_idx` was the streaming tail
/// within the grace period — i.e. it should stay expanded even though
/// `is_streaming_tail` is now false.  Declared in messages_list.cppm so
/// estimate_row_height (messages_list_geometry.cpp) can size expanded
/// thinking rows correctly in the virtual-list geometry.
[[nodiscard]] bool was_recently_streaming(std::size_t row_idx) {
    auto it = thinking_stream_last_seen.find(row_idx);
    if (it == thinking_stream_last_seen.end()) return false;
    auto elapsed = clock::steady_now() - it->second;
    if (elapsed >= kThinkingCollapseGrace) {
        thinking_stream_last_seen.erase(it);
        return false;
    }
    return true;
}

/// Record that the thinking row at `row_idx` is currently the streaming
/// tail (called every frame while streaming).
inline void mark_streaming(std::size_t row_idx) {
    thinking_stream_last_seen[row_idx] = clock::steady_now();
}

}  // namespace detail

// RFC 0003: test seam — clear the process-global grace map so replay
// fixtures don't leak streaming state across tests. Declared in
// messages_list.cppm; the map lives in detail above.
void clear_thinking_stream_last_seen_for_testing() {
    detail::thinking_stream_last_seen.clear();
}

namespace detail {

auto render_payload_row(const MessagesListInput& input,
                               std::size_t row_idx,
                               bool is_selected,
                               std::size_t frame_count,
                               bool add_margin) -> Element
{
    if (row_idx >= input.shapes.size() || row_idx >= input.rows.size()) {
        return text("⚠ bad row_idx") | color(Color::Yellow);
    }
    MessageShape shape   = input.shapes[row_idx];
    const auto& payload  = input.rows[row_idx];

    // Helper: returns true for shapes that belong to the same assistant
    // "turn group" (i.e. multiple content blocks inside one assistant
    // message: thinking, text, tool_use, redacted_thinking, grouped_tools).
    // Used by the caller to decide whether add_margin should be true (first
    // block of a turn) or false (same-turn siblings).
    (void)add_margin;

    // ── LIVE-PATH FAITHFUL RENDER (M4 + M6) ───────────────────────────────
    // The five core message types (user/assistant/thinking/system/tool-use)
    // are routed THROUGH THE FAITHFUL Element renderers
    // (RenderUserPromptMessage, RenderAssistantTextMessageFaithful,
    // RenderThinkingMessageFaithful, RenderSystemTextMessageFaithful,
    // RenderFaithfulToolUseMessage).  These emit the exact
    // components/messages/* shapes the running user sees:
    //   * user     → `❯ <text>` full-width, userMessageBackground tint
    //   * assistant→ `[dot?] <markdown body>` flex-start, no header chrome
    //   * thinking → `∴ Thinking (ctrl+o to expand)` collapsed / indented body
    //   * system   → `※ / ✻ / ⏺ <content>` flat one-line event row
    //   * tool-use → `[●] <BoldName> (summary)` + progress/queued line below
    // The divergent RenderMessageRowByType / render_message_envelope path
    // (avatar column + role pill + top accent border) is NOT faithful —
    // it added invented chrome.  We therefore BYPASS it for these core types
    // and emit the faithful Element directly.  Sub-types not yet ported still
    // flow through the divergent envelope+dispatch path below.
    using S = MessageShape;
    const bool is_streaming_tail =
        (input.streaming_tail_row != std::size_t(-1) &&
         row_idx == input.streaming_tail_row);

    if (shape == S::UserText || shape == S::UserPrompt || shape == S::UserCommand) {
        auto* d = std::get_if<UserTextMessageData>(&payload);
        if (d) {
            // Bridge the row-data variant to the faithful fn's args.  The
            // streaming/selection state isn't part of the bubble (the
            // spinner is rendered separately as the streaming-tail cursor
            // below the row), so we render the canonical shape.  When a
            // command_name chip is set, route through the slash-command shape.
            // add_margin is threaded for faithfulness (top margin = add_margin ? 1 : 0).
            const UserTextMessageData fd = *d;
            Element el = (shape == S::UserCommand || fd.command_name)
                ? RenderUserCommandMessage(fd, /*is_selected=*/is_selected, /*add_margin=*/add_margin)
                : RenderUserPromptMessage(fd, /*is_selected=*/is_selected, /*add_margin=*/add_margin);
            (void)frame_count; (void)is_streaming_tail;
            return el;
        }
    }
    else if (shape == S::UserLocalCommandOutput) {
        auto* opts = std::get_if<local_cmd::LocalCommandOptions>(&payload);
        if (opts) {
            Element el = local_cmd::RenderLocalCommandOutputFaithful(*opts);
            (void)frame_count; (void)is_streaming_tail;
            return el;
        }
    }
    else if (shape == S::UserLocalJsxOutput) {
        auto* d = std::get_if<UserTextMessageData>(&payload);
        if (d) {
            Elements lines;
            std::size_t start = 0;
            while (start <= d->content.size()) {
                const auto nl = d->content.find('\n', start);
                std::string line = nl == std::string::npos
                    ? d->content.substr(start)
                    : d->content.substr(start, nl - start);

                Element row = text(line.empty() ? " " : line);
                if (line == "Skills" || line == "Agents") {
                    row = std::move(row) | bold | color(Color::BlueLight);
                } else if (line == "Esc to close" ||
                           line.starts_with("Press ")) {
                    row = std::move(row) | dim;
                } else if (line.starts_with("› ")) {
                    row = std::move(row) | bold | color(Color::Cyan);
                } else if (line.find("─") != std::string::npos) {
                    row = std::move(row) | dim;
                } else if (line.ends_with("skills") ||
                           line.find("skills (") != std::string::npos ||
                           line == "MCP skills" ||
                           line == "Workflow commands" ||
                           line.ends_with("agents") ||
                           line.find("agents (") != std::string::npos ||
                           line == "Built-in (always available):") {
                    row = std::move(row) | bold | dim;
                } else if (const auto marker = line.find(" · ");
                           marker != std::string::npos) {
                    row = hbox({
                        text(line.substr(0, marker)),
                        text(line.substr(marker)) | dim,
                    });
                } else {
                    row = std::move(row) | color(Color::GrayLight);
                }
                lines.push_back(std::move(row));

                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            if (lines.empty()) lines.push_back(text(" "));
            (void)frame_count; (void)is_streaming_tail;
            return vbox(std::move(lines));
        }
    }
    else if (shape == S::AssistantText) {
        auto* d = std::get_if<AssistantTextMessageData>(&payload);
        if (d) {
            // show_dot is ALWAYS true for every AssistantTextMessage
            // block within a turn (B4), regardless of streaming state. The
            // streaming-tail-only override below is REMOVED — the payload's
            // own show_dot field (defaulted true in AssistantTextMessageData)
            // now governs. Selection recolors the dot via is_selected below.
            const AssistantTextMessageData& fd = *d;
            // Streaming text row uses StreamingMarkdown (stable-prefix cache).
            // Thread the shared instance from the input so
            // RenderAssistantTextMessageFaithful can call streaming_md->update()
            // instead of full render_markdown.
            Element el = RenderAssistantTextMessageFaithful(
                fd, add_margin, /*is_selected=*/is_selected,
                /*streaming_md=*/input.streaming_md);
            (void)frame_count;
            return el;
        }
    }
    else if (shape == S::AssistantThinking || shape == S::AssistantRedactedThinking) {
        auto* o = std::get_if<thinking_message::ThinkingMessageOptions>(&payload);
        if (o) {
            // Completed thinking rows render as a collapsed "∴ Thought for
            // Xs" summary (RenderThinkingMessageCollapsed) that can be
            // expanded on click.
            //
            // NOTE: `is_selected` (row navigation highlight) does NOT mean
            // "expanded".  Expansion requires an explicit
            // user gesture (Ctrl+O / Enter) via the interactive Component
            // path.  `is_transcript_mode` (full thinking content) is
            // driven by the user's Ctrl+O transcript toggle.
            //
            // Streaming thinking tail is ALWAYS expanded (transcript mode).
            // When this row is the streaming tail OR was recently streaming
            // (within the 3s collapse grace), force transcript mode so the
            // full body is shown rather than the collapsed
            // "∴ Thinking (ctrl+o to expand)" label.
            const bool thinking_force_expanded = is_streaming_tail ||
                was_recently_streaming(row_idx);
            if (is_streaming_tail) mark_streaming(row_idx);
            Element el = thinking_message::RenderThinkingMessageFaithful(
                o->data,
                /*is_transcript_mode=*/input.is_transcript_mode || thinking_force_expanded,
                /*verbose=*/is_row_expanded(input, row_idx),
                /*add_margin=*/add_margin);
            // Track clickable (non-redacted, non-empty) thinking blocks for
            // mouse hit-testing.  The repl screen's event handler uses the
            // tracker to map a click → expand-key toggle.
            if (shape == S::AssistantThinking && input.row_click_tracker &&
                (!o->data.raw_text.empty() || !o->data.sections.empty())) {
                std::string_view uuid = (row_idx < input.uuids.size())
                    ? std::string_view(input.uuids[row_idx])
                    : std::string_view{};
                auto key = compute_expand_key(shape, payload, uuid);
                if (!key.empty()) {
                    Box& box = input.row_click_tracker->track_row(std::move(key));
                    el = std::move(el) | reflect(box);
                }
            }
            (void)frame_count;
            return el;
        }
    }
    else if (shape == S::SystemText) {
        auto* d = std::get_if<SystemTextMessageData>(&payload);
        if (d) {
            Element el = RenderSystemTextMessageFaithful(*d, /*add_margin=*/add_margin);
            (void)frame_count; (void)is_streaming_tail;
            return el;
        }
    }
    else if (shape == S::UserToolResult) {
        auto* opts = std::get_if<ToolResultOptions>(&payload);
        if (opts) {
            // Bridge ToolResultOptions (divergent model) → ToolResultFaithfulData
            // (faithful model).  The divergent struct carries both
            // an `output` field (main content, possibly ANSI) and a separate
            // `error_message` field; the faithful renderer uses a single
            // `content` field plus a `kind` enum that drives dispatch.
            ToolResultFaithfulData fd;
            fd.tool_name = opts->tool_name;
            fd.duration_ms = opts->duration_ms;
            fd.is_truncated = opts->is_truncated;
            fd.verbose = is_row_expanded(input, row_idx);

            using DS = ToolResultStatus;   // divergent status
            using FK = ToolResultKind;     // faithful kind

            switch (opts->status) {
                case DS::Success:
                    fd.kind = FK::Success;
                    fd.content = opts->output;
                    fd.content_items = opts->content_items;
                    break;
                case DS::Error:
                    fd.kind = FK::Error;
                    // Prefer error_message if set; fall back to output field.
                    if (opts->error_message && !opts->error_message->empty()) {
                        fd.content = opts->error_message;
                    } else {
                        fd.content = opts->output;
                    }
                    break;
                case DS::Timeout:
                    fd.kind = FK::Error;
                    fd.content = opts->output
                        ? opts->output
                        : std::optional<std::string>("Timed out");
                    break;
                case DS::Cancelled:
                    fd.kind = FK::Canceled;
                    fd.content = opts->output;
                    break;
            }

            Element el = RenderToolResultMessageFaithful(fd, add_margin);
            // Track clickable tool result rows for mouse hit-testing.
            // The event handler toggles the expand key on click, which
            // expands/collapses the truncated output.
            if (input.row_click_tracker &&
                ((opts->output && !opts->output->empty()) ||
                 (opts->content_items && !opts->content_items->empty()))) {
                std::string_view uuid = (row_idx < input.uuids.size())
                    ? std::string_view(input.uuids[row_idx])
                    : std::string_view{};
                auto key = compute_expand_key(shape, payload, uuid);
                if (!key.empty()) {
                    Box& box = input.row_click_tracker->track_row(std::move(key));
                    el = std::move(el) | reflect(box);
                }
            }
            (void)frame_count; (void)is_streaming_tail;
            return el;
        }
    }
    else if (shape == S::AssistantToolUse) {
        auto* opts = std::get_if<tool_use_message::ToolUseRenderOptions>(&payload);
        if (opts) {
            // Bridge ToolUseRenderOptions → FaithfulToolUseData via
            // the tool UI registry.  Each registered tool provides its
            // own user_facing_name / message / tag / progress / queued
            // functions (matching tool-class UI methods).
            //
            // Falls back to the generic renderer for unregistered tools.
            using namespace loom::ui::tools;
            const bool is_registered_builtin =
                global_tool_ui_registry().find(opts->call.tool_name) != nullptr;
            const ToolUIFunctions& ui =
                get_tool_ui_or_generic(opts->call.tool_name);

            tool_use_message::FaithfulToolUseData fd;
            fd.user_facing_name =
                ui.user_facing_name
                    ? ui.user_facing_name(opts->call.raw_parameters)
                    : std::string{opts->call.tool_name};
            fd.message =
                ui.message
                    ? ui.message(opts->call.raw_parameters)
                    : std::string{};
            if (ui.tag) {
                auto tag = ui.tag(opts->call.raw_parameters);
                if (tag) fd.tag = *tag;
            }

            // Status mapping: ToolStatus → FaithfulToolStatus
            using TS = tool_use_message::ToolStatus;
            using FTS = tool_use_message::FaithfulToolStatus;
            switch (opts->call.status) {
                case TS::Pending:   fd.status = FTS::Queued;  break;
                case TS::Running:   fd.status = FTS::Running; break;
                case TS::Success:   fd.status = FTS::Success; break;
                case TS::Error:     fd.status = FTS::Error;   break;
                case TS::Cancelled: fd.status = FTS::Error;   break;
            }

            // Progress / queued text from tool UI functions
            if (ui.progress) {
                std::string_view preview = {};
                if (opts->call.result_preview) {
                    preview = *opts->call.result_preview;
                }
                fd.progress_text = ui.progress(
                    opts->call.raw_parameters, preview);
            } else {
                fd.progress_text = "Running…";
            }
            if (ui.queued) {
                fd.queued_text = ui.queued(opts->call.raw_parameters);
            } else {
                fd.queued_text = "Waiting…";
            }

            fd.is_transparent_wrapper = ui.is_transparent_wrapper;
            fd.should_show_dot = true;
            fd.add_margin = add_margin;
            fd.spinner_frame = static_cast<int>(frame_count);
            fd.should_animate = (opts->call.status == TS::Running);

            // ── MCP-only Input/Output sections ──
            // Built-in tools (Bash, Read, Write, Edit, Glob, Grep) NEVER
            // show Input:/Output: sections — the command is in the header
            // parens and the result appears as a separate tool_result row.
            // Only MCP/unregistered tools show raw JSON parameters inline.
            fd.is_mcp_tool = !is_registered_builtin;
            if (!is_registered_builtin) {
                fd.input_json = opts->call.raw_parameters;
                if (opts->call.result_preview && !opts->call.result_preview->empty()) {
                    fd.output_text = *opts->call.result_preview;
                }
            }

            Element el = tool_use_message::RenderFaithfulToolUseMessage(fd);
            return el;
        }
    }
    else if (shape == S::UserImage) {
        // Faithful render: each user-attached image is its own transcript
        // row.  Render directly via message_image::render (stateless Element)
        // wrapped in user-message chrome for visual consistency with
        // RenderUserPromptMessage: full-width, userMessageBackground tint,
        // top margin = (add_margin ? 1 : 0).
        //
        // The [Image #N] label lives inside the user message bubble; we
        // project images as separate rows (one per attachment) so each gets
        // its own user-styled card.
        auto* d = std::get_if<image::ImageMessageData>(&payload);
        if (d) {
            // userMessageBackground = rgb(55, 55, 55)
            // (matches RenderUserPromptMessage kUserBg).
            const Color kUserBg = Color::RGB(55, 55, 55);

            Element body = image::render(*d);

            if (add_margin) {
                // First user block in turn: wrap in user-message chrome with
                // full-width bg tint (matches RenderUserPromptMessage).
                Element content = hbox({
                    text(" ") | bgcolor(kUserBg),
                    std::move(body) | bgcolor(kUserBg) | flex,
                    text(" ") | bgcolor(kUserBg),
                });
                return vbox({text(""), std::move(content)});
            }
            // When add_margin is false (image is a continuation within the
            // same user turn), wrap in a MessageResponse which prepends
            // "  ⎿  " (U+23BF connector).  NO background tint — only the
            // primary user text bubble gets userMessageBackground;
            // continuation blocks are plain.
            return hbox({
                text("  \xe2\x8e\xbf  ") | dim,
                std::move(body),
            });
        }
    }

    // ── DIVERGENT PATH (sub-types not yet ported to faithful) ───────────
    // GAP 3: thread on_retry from MessagesListInput so SystemAPIError rows
    // can render a working Retry pill that re-sends the last user message.
    MessageRowCallbacks cb;
    cb.on_retry = input.on_retry;
    // P2 gap api-error-retry: thread on_clear_session for session-expired
    // error cards.
    cb.on_clear_session = input.on_clear_session;
    Component inner = RenderMessageRowByType(shape, payload, std::move(cb));

    RenderEnvelopeOptions env_opts{
        .shape          = shape,
        .timestamp      = std::chrono::system_clock::now(),
        .status         = derive_status_badge(shape, row_idx, input.streaming_tail_row),
        .show_dismiss   = (shape == MessageShape::SystemAdvisor ||
                           shape == MessageShape::SystemHookProgress),
        .is_selected    = is_selected,
        .frame_count    = frame_count,
    };
    return render_message_envelope(env_opts, inner->Render());
}

} // namespace detail

} // namespace loom::ui::messages_list
