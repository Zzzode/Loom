// app_message_projection.cpp — impl unit for the Engine-state →
// ReplScreenState message projection free functions, kept OUT of
// app.cppm to shrink the interface BMI (clang source-location / memory
// budget; a fat interface is instantiated into every importer).
//
// Contains: project_message, project_messages, RenderMessage.
module;

#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <cstdio>

module loom.ui.app.app;

import std;

import loom.types.types;
import loom.ui.screens.repl_screen;
import loom.ui.screens.messages_store;
import loom.ui.messages.xml_tags;
import loom.constants.xml;

namespace loom::ui {

namespace {

std::string_view notification_trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

// Unlike a tag search, consuming at the cursor rejects prose, duplicates and
// unknown fields. The engine's XML envelope remains unchanged in the transcript.
std::optional<std::string> take_notification_tag(std::string_view& remaining,
                                                 std::string_view tag) {
    remaining = notification_trim(remaining);
    const auto open = "<" + std::string(tag) + ">";
    if (!remaining.starts_with(open)) return std::nullopt;
    auto value = messages::xml_tags::extract_tag(remaining, tag);
    if (!value) return std::nullopt;
    remaining.remove_prefix(open.size() + value->size() + tag.size() + 3);
    return value;
}

std::string notification_label(std::string_view value, bool decode_entities) {
    // Only legacy XML names decode producer entities, exactly once. Typed
    // names are raw text, so entity-like substrings must remain literal.
    constexpr std::array<std::pair<std::string_view, char>, 5> entities{
        {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}}};
    std::string decoded;
    for (std::size_t i = 0; i < value.size();) {
        char character = value[i++];
        if (decode_entities && character == '&') {
            for (const auto& [entity, replacement] : entities) {
                if (value.substr(i - 1).starts_with(entity)) {
                    character = replacement;
                    i += entity.size() - 1;
                    break;
                }
            }
        }
        if (static_cast<unsigned char>(character) < 0x20 || character == 0x7f) character = ' ';
        if (character != ' ' || (!decoded.empty() && decoded.back() != ' ')) decoded += character;
    }
    if (!decoded.empty() && decoded.back() == ' ') decoded.pop_back();
    constexpr std::size_t max_bytes = 96;
    if (decoded.size() > max_bytes) {
        std::size_t end = max_bytes;
        while (end > 0 && (static_cast<unsigned char>(decoded[end]) & 0xC0) == 0x80)
            --end;
        decoded.resize(end);
        decoded += "…";
    }
    return decoded;
}

std::optional<repl::MessageDisplayEntry>
project_task_notification(const UserMessage& message,
                          std::chrono::system_clock::time_point timestamp) {
    if (const auto* event = std::get_if<TaskEvent>(&message.origin)) {
        std::string_view actor;
        switch (event->kind) {
        case TaskEventKind::Subagent:
            actor = "Subagent";
            break;
        case TaskEventKind::TeamAgent:
            actor = "Team Agent";
            break;
        }
        std::string_view state;
        switch (event->status) {
        case TaskEventStatus::Completed:
            state = "completed";
            break;
        case TaskEventStatus::Failed:
            state = "failed";
            break;
        case TaskEventStatus::Stopped:
            state = "stopped";
            break;
        }
        if (actor.empty() || state.empty()) return std::nullopt;
        auto name = notification_label(event->display_name, false);
        if (name.empty()) name = notification_label(event->task_id, false);
        repl::MessageDisplayEntry entry;
        entry.role = "system";
        entry.system_subtype = "background_task";
        entry.content_preview = std::format("{} {} · {}", actor, state, name);
        entry.timestamp = timestamp;
        return entry;
    }
    // Missing provenance is ambiguous old history, never evidence of a trusted
    // runtime event. New human text remains user text even if XML is identical.
    if (!std::holds_alternative<LegacyHistory>(message.origin)) return std::nullopt;
    // Display compatibility accepts only a complete single-text-block envelope.
    if (message.content.size() != 1) return std::nullopt;
    const auto* text = std::get_if<TextBlock>(&message.content.front());
    if (!text) return std::nullopt;
    namespace xml = loom::constants::xml;
    auto envelope = notification_trim(text->text);
    auto remaining = envelope;
    auto body = take_notification_tag(remaining, xml::TASK_NOTIFICATION_TAG);
    if (!body || !notification_trim(remaining).empty()) return std::nullopt;
    remaining = *body;
    const auto scalar = [](const auto& value) {
        return value && value->find_first_of("<>") == std::string::npos;
    };
    auto id = take_notification_tag(remaining, xml::TASK_ID_TAG);
    if (!scalar(id) || notification_trim(*id).empty()) return std::nullopt;
    if (notification_trim(remaining).starts_with("<tool_use_id>")) {
        if (!scalar(take_notification_tag(remaining, xml::TOOL_USE_ID_TAG))) return std::nullopt;
    }
    if (!scalar(take_notification_tag(remaining, xml::OUTPUT_FILE_TAG))) return std::nullopt;
    auto status = take_notification_tag(remaining, xml::STATUS_TAG);
    if (!scalar(status)) return std::nullopt;
    const auto state = notification_trim(*status);
    if (state != "completed" && state != "failed" && state != "stopped") return std::nullopt;
    auto summary = take_notification_tag(remaining, xml::SUMMARY_TAG);
    if (!scalar(summary)) return std::nullopt;
    if (notification_trim(remaining).starts_with("<result>")) {
        if (!scalar(take_notification_tag(remaining, xml::RESULT_TAG))) return std::nullopt;
    }
    if (notification_trim(remaining).starts_with("<worktree>")) {
        auto worktree = take_notification_tag(remaining, xml::WORKTREE_TAG);
        if (!worktree) return std::nullopt;
        auto fields = std::string_view(*worktree);
        if (!scalar(take_notification_tag(fields, xml::WORKTREE_PATH_TAG))) return std::nullopt;
        if (notification_trim(fields).starts_with("<worktree_branch>"))
            if (!scalar(take_notification_tag(fields, xml::WORKTREE_BRANCH_TAG)))
                return std::nullopt;
        if (!notification_trim(fields).empty()) return std::nullopt;
    } else {
        if (notification_trim(remaining).starts_with("<worktree_path>"))
            if (!scalar(take_notification_tag(remaining, xml::WORKTREE_PATH_TAG)))
                return std::nullopt;
        if (notification_trim(remaining).starts_with("<worktree_branch>"))
            if (!scalar(take_notification_tag(remaining, xml::WORKTREE_BRANCH_TAG)))
                return std::nullopt;
    }
    if (!notification_trim(remaining).empty()) return std::nullopt;

    auto label = notification_trim(*summary);
    const auto quote = label.starts_with("Agent &quot;") ? std::string_view{"&quot;"}
                       : label.starts_with("Agent \"")   ? std::string_view{"\""}
                                                         : std::string_view{};
    if (!quote.empty()) {
        const auto start = std::string_view{"Agent "}.size() + quote.size();
        if (state == "failed") {
            const auto marker = std::string(quote) + " failed:";
            const auto end = label.find(marker, start);
            // An escaped name and an error can both contain this marker.
            // Ambiguity yields the stable task ID, never failure/result text.
            if (end == std::string_view::npos ||
                label.find(marker, end + marker.size()) != std::string_view::npos)
                label = *id;
            else
                label = label.substr(start, end - start);
        } else {
            const auto suffix =
                std::string(quote) + (state == "completed" ? " completed" : " was stopped");
            if (!label.ends_with(suffix) || label.size() < start + suffix.size())
                return std::nullopt;
            label = label.substr(start, label.size() - start - suffix.size());
        }
    } else if (label.starts_with("Agent "))
        return std::nullopt;
    auto name = notification_label(label, true);
    if (name.empty()) name = notification_label(*id, true);
    if (name.empty()) name = "background task";
    repl::MessageDisplayEntry entry;
    entry.role = "system";
    entry.system_subtype = "background_task";
    entry.content_preview = std::format("Agent {} · {}", state, name);
    entry.timestamp = timestamp;
    return entry;
}

}  // namespace

// ============================================================
// Projection: Engine state -> ReplScreenState
// ============================================================

[[nodiscard]] repl::MessageDisplayEntry project_message(const Message& msg) {
    return std::visit(
        [](const auto& m) -> repl::MessageDisplayEntry {
            using T = std::decay_t<decltype(m)>;
            repl::MessageDisplayEntry e;
            e.timestamp = (m.timestamp == std::chrono::system_clock::time_point{})
                              ? std::chrono::system_clock::now()
                              : m.timestamp;

            if constexpr (std::is_same_v<T, UserMessage>) {
                if (auto notification = project_task_notification(m, e.timestamp))
                    return *notification;
                e.role = "user";
                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block)) {
                        e.content_preview += tb->text;
                    } else if (const auto* ib = std::get_if<ImageBlock>(&block)) {
                        // Single-entry fallback (user only attached images, no
                        // text).  When project_messages runs below with the
                        // full multi-row split, each ImageBlock becomes its own
                        // entry with is_image=true; this path just ensures the
                        // legacy / one-entry case doesn't render a completely
                        // blank card.
                        if (e.content_preview.empty() && m.content.size() == 1) {
                            e.is_image = true;
                            e.image_block = *ib;
                            e.image_display_id = 1;  // single image = #1
                        }
                        e.content_preview += e.content_preview.empty() ? "" : "\n";
                        e.content_preview += "[Image";
                        if (ib->width && ib->height) {
                            char buf[48];
                            std::snprintf(buf, sizeof(buf), " %zux%zu", *ib->width, *ib->height);
                            e.content_preview += buf;
                        }
                        e.content_preview += "]";
                    } else if (const auto* trb = std::get_if<ToolResultBlock>(&block)) {
                        // Single-entry fallback for ToolResultBlock in user
                        // message.  project_messages handles this properly
                        // (emits role="tool" entry); this ensures the
                        // legacy one-entry path doesn't render blank.
                        //
                        // is_tool_use must be false for tool results.  The
                        // repl_screen dispatcher checks is_tool_use FIRST
                        // (line 858), so a tool_result with is_tool_use=true
                        // gets routed to AssistantToolUse rendering instead of
                        // UserToolResult — the committed result card never
                        // appears as a separate block.
                        if (e.content_preview.empty() && m.content.size() == 1) {
                            e.role = "tool";
                            e.is_tool_use = false;
                            e.tool_status = trb->is_error ? "error" : "success";
                            e.is_error = trb->is_error;
                            e.tool_use_id = trb->tool_use_id.value;
                        }
                        e.content_preview += e.content_preview.empty() ? "" : "\n";
                        e.content_preview += tool_result_content_text(*trb);
                    }
                }
            } else if constexpr (std::is_same_v<T, AssistantMessage>) {
                e.role = "assistant";
                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block)) {
                        e.content_preview += tb->text;
                    } else if (const auto* thk = std::get_if<ThinkingBlock>(&block)) {
                        e.is_thinking = true;
                        if (e.content_preview.empty())
                            e.content_preview = thk->thinking.substr(0, 200);
                        e.full_content = thk->thinking;
                    } else if (const auto* tool = std::get_if<ToolUseBlock>(&block)) {
                        e.is_tool_use = true;
                        e.tool_name = tool->name;
                        e.tool_use_id = tool->id.value;
                        e.tool_input_json = tool->input_json;
                    }
                }
            } else if constexpr (std::is_same_v<T, SystemMessage>) {
                e.role = "system";
                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block))
                        e.content_preview += tb->text;
                }
            } else if constexpr (std::is_same_v<T, ToolResultMessage>) {
                e.role = "tool";
                // is_tool_use=false for tool results.  ToolResultMessage is the
                // committed result (role="tool"), NOT the tool_use request
                // (role="assistant").  Setting this true routes the entry to
                // AssistantToolUse rendering in repl_screen, so the result never
                // appears as its own transcript card.
                e.is_tool_use = false;
                e.tool_status = m.is_error ? "error" : "success";
                e.is_error = m.is_error;
                e.tool_use_id = m.tool_use_id.value;
                // Propagate the tool name from the result message so the
                // renderer can show "Bash" / "Edit" instead of generic "tool".
                // The old code left tool_name unset → BuildMessagesList fell back
                // to "tool", making every tool result look anonymous.
                if (!m.tool_name.empty()) {
                    e.tool_name = m.tool_name;
                }
                // Collect structured content items so the faithful renderer can
                // iterate them (text + image separately).
                std::vector<loom::core::ToolResultContentItem> content_items;
                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block)) {
                        e.content_preview += tb->text;
                        content_items.push_back(loom::core::ToolResultContentItem{
                            .type = "text", .text = tb->text, .media_type = {}, .data = {}});
                    } else if (const auto* ib = std::get_if<ImageBlock>(&block)) {
                        // Tool results may return images (e.g. analyze_image).
                        // Append a [Image] marker so the row isn't empty.
                        if (!e.content_preview.empty()) e.content_preview += '\n';
                        e.content_preview += "[Image]";
                        e.is_image = true;
                        e.image_block = *ib;
                        content_items.push_back(
                            loom::core::ToolResultContentItem{.type = "image",
                                                              .text = {},
                                                              .media_type = ib->media_type,
                                                              .data = ib->data});
                    } else if (const auto* db = std::get_if<DocumentBlock>(&block)) {
                        if (!e.content_preview.empty()) e.content_preview += '\n';
                        e.content_preview += "[Document]";
                    }
                }
                if (!content_items.empty()) {
                    e.tool_result_content_items = std::move(content_items);
                }
            }
            // NOTE: content_preview IS the rendered message body, not a
            // "preview".  Do NOT truncate here — VirtualMessageList handles
            // display clipping.  Truncating caused "text vanishes after
            // streaming": the live stream showed the tail (last 500 chars)
            // but the committed view showed only the head (first 500 chars),
            // so for >500-char responses the content the user was reading
            // disappeared on completion.
            return e;
        },
        msg);
}

// ============================================================
// project_messages — projection that splits a single
// AssistantMessage into MULTIPLE display rows when it mixes a ThinkingBlock
// with a TextBlock / ToolUseBlock.  These render as separate sibling
// messages (a collapsed `∴ Thinking` row followed by the visible answer /
// tool-use row); the legacy single-entry projection collapsed them into one
// thinking row, which hid the visible answer once M4 routed thinking rows
// through RenderThinkingMessageFaithful (collapsed → raw text hidden).
//
// Non-assistant messages and assistant messages with a single block kind
// still project to exactly one entry (identical to project_message).
// ============================================================
[[nodiscard]] std::vector<repl::MessageDisplayEntry> project_messages(const Message& msg) {
    std::vector<repl::MessageDisplayEntry> out;
    // Use the message's own timestamp (set when the engine appended it) so
    // chronological sorting against local-command rows is correct.  Fallback
    // to now() for messages with epoch-zero timestamps (shouldn't happen but
    // guards against uninitialized fields).
    const auto msg_ts = std::visit([](const auto& m) { return m.timestamp; }, msg);
    const auto now = (msg_ts == std::chrono::system_clock::time_point{})
                         ? std::chrono::system_clock::now()
                         : msg_ts;

    std::visit(
        [&](const auto& m) {
            using T = std::decay_t<decltype(m)>;

            if constexpr (std::is_same_v<T, AssistantMessage>) {
                // Iterate content blocks in ORIGINAL ORDER instead of grouping
                // by kind — each block becomes a separate sibling row.
                //
                // The old code grouped all text into one accumulator and
                // all tools into a separate vector, then emitted [thinking,
                // merged_text, tool1, tool2, ...].  For [text1, tool_use,
                // text2] this produced [text1+text2, tool_use] — text was
                // merged and the order was wrong, so the model's final
                // response text (text2) appeared glued to the pre-tool
                // announcement (text1) instead of being a separate block.
                //
                // Consecutive text blocks ARE merged (adjacent text nodes
                // render inline), but any non-text block (thinking, tool_use)
                // flushes the text accumulator and emits its own row.
                std::string text_acc;

                auto flush_text = [&] {
                    if (text_acc.empty()) return;
                    repl::MessageDisplayEntry a;
                    a.role = "assistant";
                    a.content_preview = text_acc;
                    a.timestamp = now;
                    out.push_back(std::move(a));
                    text_acc.clear();
                };

                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block)) {
                        // Consecutive text blocks merge (adjacent text nodes
                        // render inline without separation).
                        if (!text_acc.empty() && !tb->text.empty() && tb->text.front() != '\n') {
                            text_acc += '\n';
                        }
                        text_acc += tb->text;
                    } else if (const auto* thk = std::get_if<ThinkingBlock>(&block)) {
                        flush_text();
                        repl::MessageDisplayEntry t;
                        t.role = "assistant";
                        t.is_thinking = true;
                        t.content_preview = thk->thinking.substr(0, 200);
                        t.full_content = thk->thinking;
                        t.timestamp = now;
                        out.push_back(std::move(t));
                    } else if (const auto* tool = std::get_if<ToolUseBlock>(&block)) {
                        flush_text();
                        repl::MessageDisplayEntry tu;
                        tu.role = "assistant";
                        tu.is_tool_use = true;
                        tu.tool_name = tool->name;
                        tu.tool_use_id = tool->id.value;
                        tu.tool_input_json = tool->input_json;
                        // The AssistantMessage commits before tool execution
                        // (query_engine_loop.cpp: append_message runs before
                        // execute_pending_tools), so the tool may still be
                        // running.  The streaming path shows the live status;
                        // this committed row is the fallback for when the
                        // streaming path is not active.
                        tu.tool_status = "success";
                        tu.timestamp = now;
                        out.push_back(std::move(tu));
                    }
                    // ToolResultBlock in assistant messages: shouldn't
                    // happen (API invariant), but if it does we skip it
                    // — tool results are projected from ToolResultMessage
                    // or UserMessage with ToolResultBlock below.
                }
                flush_text();  // emit any trailing text
            } else if constexpr (std::is_same_v<T, UserMessage>) {
                if (auto notification = project_task_notification(m, now)) {
                    out.push_back(std::move(*notification));
                    return;
                }
                // ── Each ImageBlock in the user message becomes its own
                //    transcript row, interleaved with text rows in the same
                //    order as m.content.
                std::string text_acc;
                int img_id_counter = 0;  // imageIds assigned per content block order
                auto flush_text = [&] {
                    if (text_acc.empty()) return;
                    repl::MessageDisplayEntry u;
                    u.role = "user";
                    u.content_preview = text_acc;
                    // Do NOT truncate — user needs to see their full message
                    u.timestamp = now;
                    out.push_back(std::move(u));
                    text_acc.clear();
                };
                for (const auto& block : m.content) {
                    if (const auto* tb = std::get_if<TextBlock>(&block)) {
                        if (!text_acc.empty() && !tb->text.empty() && tb->text.front() != '\n')
                            text_acc += '\n';
                        text_acc += tb->text;
                    } else if (const auto* ib = std::get_if<ImageBlock>(&block)) {
                        flush_text();
                        repl::MessageDisplayEntry img;
                        img.role = "user";
                        img.is_image = true;
                        img.image_block = *ib;
                        img.image_display_id = ++img_id_counter;  // imageIds from paste order
                        img.timestamp = now;
                        // Human-readable preview for list views / debugger tools.
                        std::string preview = "[Image";
                        if (ib->width && ib->height) {
                            char buf[48];
                            std::snprintf(buf, sizeof(buf), " %zux%zu", *ib->width, *ib->height);
                            preview += buf;
                        }
                        if (ib->file_name) {
                            preview += " ";
                            preview += *ib->file_name;
                        }
                        preview += "]";
                        img.content_preview = std::move(preview);
                        img.estimated_height_lines = 2;  // compact card: label + optional source
                        out.push_back(std::move(img));
                    } else if (const auto* trb = std::get_if<ToolResultBlock>(&block)) {
                        // Handle ToolResultBlock in user messages.  The API
                        // returns tool results as role=user messages with
                        // tool_result content blocks.  The old code ignored
                        // these, so committed tool results vanished from the
                        // transcript after streaming ended (streaming_tools_
                        // was cleared but the committed UserMessage with
                        // ToolResultBlock was never projected).
                        flush_text();
                        repl::MessageDisplayEntry tr;
                        tr.role = "tool";
                        // is_tool_use=false.  This is a committed tool result
                        // (role="tool"), not the assistant's tool_use request.
                        // repl_screen checks is_tool_use before role, so true
                        // here would swallow the result into the tool_use
                        // card's Output section instead of rendering it as a
                        // separate card.
                        tr.is_tool_use = false;
                        tr.tool_status = trb->is_error ? "error" : "success";
                        tr.is_error = trb->is_error;
                        tr.tool_use_id = trb->tool_use_id.value;
                        // tool_name not available from ToolResultBlock (it
                        // only has tool_use_id); renderer falls back to
                        // "tool" via m.tool_name.value_or("tool").
                        //
                        // Content may be string or array of content items.
                        if (std::holds_alternative<std::string>(trb->content)) {
                            tr.content_preview = std::get<std::string>(trb->content);
                        } else {
                            const auto& items =
                                std::get<std::vector<loom::core::ToolResultContentItem>>(
                                    trb->content);
                            std::vector<loom::core::ToolResultContentItem> ci_copy;
                            for (const auto& item : items) {
                                if (item.type == "text") {
                                    if (!tr.content_preview.empty()) tr.content_preview += '\n';
                                    tr.content_preview += item.text;
                                } else if (item.type == "image") {
                                    if (!tr.content_preview.empty()) tr.content_preview += '\n';
                                    tr.content_preview += "[Image]";
                                    tr.is_image = true;
                                }
                                ci_copy.push_back(item);
                            }
                            tr.tool_result_content_items = std::move(ci_copy);
                        }
                        tr.timestamp = now;
                        out.push_back(std::move(tr));
                    }
                    // ThinkingBlock/ToolUseBlock in a user message are API
                    // invariants; ignore if present (project_message doesn't
                    // render them either).
                }
                flush_text();
                if (out.empty()) {
                    // Degenerate case: user message with zero renderable blocks.
                    // Fall back to the one-entry project_message so we never
                    // emit an empty list.
                    out.push_back(project_message(msg));
                }
            } else {
                // Non-assistant → identical to the single-entry projection.
                out.push_back(project_message(msg));
            }
        },
        msg);

    if (out.empty()) out.push_back(project_message(msg));
    return out;
}

// ============================================================
// Convenience: render a single core Message to an Element.
// Used by tests and callers that want a quick rendering of one message.
// ============================================================

[[nodiscard]] Element RenderMessage(const Message& msg) {
    // Single-message render: disable chain compression.  Chain compression
    // is a list-level feature that collapses consecutive thinking+tool
    // rows; a single message has no chain to compress, and callers expect
    // to see the message's own rendering (e.g. a thinking block's collapsed
    // label) rather than a chain summary.
    return repl::RenderMessages(project_messages(msg), -1, 40, 0, true, 0, std::nullopt, {}, false,
                                {}, false, false, true);
}

}  // namespace loom::ui

namespace loom::ui {
// Both retained rendering and explicit SyncState use the same committed result
// status. Tool names cannot distinguish retries or parallel calls of one tool.
void reconcile_tool_result_statuses(std::vector<repl::MessageDisplayEntry>& rows) {
    std::unordered_map<std::string, std::size_t> calls;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].is_tool_use && rows[i].tool_use_id && !rows[i].tool_use_id->empty())
            calls[*rows[i].tool_use_id] = i;
    }
    for (const auto& result : rows) {
        if (result.role != "tool" || !result.tool_use_id || result.tool_use_id->empty()) continue;
        const auto found = calls.find(*result.tool_use_id);
        if (found == calls.end()) continue;
        rows[found->second].tool_status = result.is_error ? "error" : "success";
    }
}
}  // namespace loom::ui
