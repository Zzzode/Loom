// app_render_event.cpp — impl unit for AppAdapter virtual/override methods
// and the render/event loop.  Split from app_autocomplete.cpp (P2-1e) to
// isolate the autocomplete closure from the render/event closure.
//
// Contains: ~AppAdapter, is_streaming_thinking_visible,
//           DrainPendingAtMentionInserts, Render, OnEvent, ActiveChild.
//
// Other impl units:
//   app_autocomplete.cpp   — RefreshAutocompleteSuggestions
//   app_text_selection.cpp — SelectionHighlightNode, drag-to-select mouse
//                           handling, text extraction + clipboard copy
//   app_constructor.cpp    — constructor
//   app_handle_submit.cpp  — HandleSubmit, HandleCommand
//   app_agent_menu.cpp     — FormatAgentsMenuOutput, LoadAgentCardsForMenu,
//                           SyncState, ConsumePendingResult,
//                           WaitForInFlightPastes, get_permission_callback,
//                           trigger_orphan_cleanup_for_testing
//   app_extra_methods.cpp  — RunLocalBashCommand, ProjectRuntimeMetadataToScreenState,
//                           ApplyMessageCollapsePipeline, SpawnPasteWorker,
//                           ProcessCompletedPastes, project_agent_definition_card
module;

#include <cstring>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <cstdint>

module loom.ui.app.app;

import std;
import loom.query.query_engine;
import loom.tools.agent_runtime;
import loom.ui.features.agents.agent_cards;
import loom.ui.foundation.clock;
import loom.ui.foundation.declared_cursor;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.diagnostics.debug;
import loom.platform.hyperlink;
import loom.text.parse_references;
import loom.teams.swarm.pane_observer;

namespace loom::ui {
namespace agent_runtime = loom::tools::agent_runtime;
namespace agent_cards = loom::ui::agents::cards;
// Defined in app_extra_methods.cpp (same module); redeclared for module linkage.
agent_cards::AgentCardData project_agent_definition_card(
    const agent_runtime::AgentDefinition& agent);

namespace repl = loom::ui::repl_screen;

// ── Virtual/override methods (moved out of class body to fix vtable crash) ──
// These were previously defined inline in AppAdapter's class body.  Moving
// them out makes Render() the key function (first non-inline virtual), so
// the vtable is emitted in this impl TU rather than in every TU that imports
// loom.ui.app.  This avoids a clang crash in DefineUsedVTables during app.cppm
// compilation (exit code 139 in NamespaceDecl::getMostRecentDeclImpl).

AppAdapter::~AppAdapter() {
    // Unsubscribe from the pane observer before other teardown so a late
    // capture-pass callback cannot flag/post on a torn-down adapter.
    if (pane_observer_token_ != 0) {
        loom::utils::pane_observer::unsubscribe_changed(pane_observer_token_);
        pane_observer_token_ = 0;
    }
    if (leader_inbox_thread_.joinable()) {
        leader_inbox_thread_.request_stop();
    }
    if (query_running_.load() && static_cast<loom::core::QueryEngine*>(engine_raw())) {
        static_cast<loom::core::QueryEngine*>(engine_raw())->abort();
    }
    if (query_thread_.joinable()) query_thread_.request_stop();
    if (spinner_thread_.joinable()) spinner_thread_.request_stop();
    statusline_.Stop();  // P3-1c: coordinator owns the jthread
    if (bash_thread_.joinable()) bash_thread_.request_stop();
    // P4-1d: unblock all pending permission/elicitation/ask-user waits.
    permission_.abort_all();

    // Unsubscribe from dynamic skill discovery callbacks.
    if (skills_changed_unsubscribe_) {
        try { skills_changed_unsubscribe_(); } catch (...) {}
    }
}

// RFC 0001 Phase C batch 2: folded from app.cppm — sole callers are the
// Render() streaming-thinking path below.
// The grace period is 3s, matching kThinkingCollapseGrace in
// messages_list_payload_row.cpp — after this the in-flight projection
// path is deactivated and the committed row takes over (collapsed).
bool AppAdapter::is_streaming_thinking_visible() const {
    auto now = clock::steady_now();
    for (const auto& [idx, stp] : streaming_thinking_) {
        if (!stp.complete) return true;
        if (stp.streaming_ended_at &&
            std::chrono::duration_cast<std::chrono::seconds>(
                now - *stp.streaming_ended_at).count() < 3)
            return true;
    }
    return false;
}

// AT-09: drain inbound IDE at_mentioned tokens staged by the MCP receive
// thread into screen_state_->mcp_status_store, then apply them to the
// prompt. MUST be called on the render thread (the apply mutates
// input_text/cursor). The staging queue and its mutex live here in the
// composition layer (RFC 0002 F3: stores hold no locks); the store holds
// only drained data. Empty-under-the-lock fast path keeps per-frame cost
// negligible.
std::size_t AppAdapter::DrainPendingAtMentionInserts() {
    {
        std::lock_guard lk(at_mention_mutex_);
        if (pending_at_mention_inserts_.empty()) return 0;
        screen_state_->mcp_status_store.pending_at_mention_inserts.swap(
            pending_at_mention_inserts_);
    }
    return repl::ApplyPendingAtMentionInserts(screen_state_);
}

Element AppAdapter::Render() {
    this->ProjectRuntimeMetadataToScreenState();
    ConsumePendingResult();
    // AT-09: apply any inbound IDE at_mentioned tokens that landed since
    // the last frame (drained from the AppImpl staging queue on the render
    // thread for input_text safety).
    DrainPendingAtMentionInserts();

    const bool qr = query_running_.load();
    loom::utils::debug("app.render",
        "Render: query_running={}, messages={}, spinner_mode={}",
        qr, screen_state_->messages_store.messages.size(),
        static_cast<int>(screen_state_->task_view_store.spinner_mode));

    if (qr) {
        std::lock_guard lk(result_mutex_);

        const auto now = std::chrono::system_clock::now();
        auto messages = static_cast<loom::core::QueryEngine*>(engine_raw())->get_conversation();
        // Collapse chain for background-bash and similar collapsible rows.
        messages = ApplyMessageCollapsePipeline(std::move(messages));
        // Build the new message list in a local vector, then swap at the
        // end.  This prevents a one-frame blank screen: the store is
        // never observed in a cleared-but-not-yet-rebuilt state.
        std::vector<repl::MessageDisplayEntry> new_messages;
        new_messages.reserve(
            messages.size() + streaming_tools_.size() +
            streaming_thinking_.size() + 1);

        // Same uuid-assignment pattern as SyncState above — per-source
        // Message 24-char prefix shared by all derived sub-rows.
        auto make_uuid24 = [](std::uint64_t msg_idx, const std::string& seed) {
            std::uint64_t h = 1469598103934665603ULL;
            for (char c : seed) { h ^= static_cast<std::uint64_t>(c); h *= 1099511628211ULL; }
            char buf[32];
            std::snprintf(buf, sizeof(buf), "msg_%012llx%08llx",
                          (unsigned long long)msg_idx,
                          (unsigned long long)(h & 0xFFFFFFFFULL));
            return std::string(buf, 24);
        };
        std::uint64_t tick_msg_idx = 0;

        // Project completed messages (skip system prompt, split mixed assistant).
        for (const auto& msg : messages) {
            if (std::holds_alternative<SystemMessage>(msg)) continue;
            auto projected = project_messages(msg);
            std::string seed_preview;
            std::visit([&](const auto& m) {
                if constexpr (requires{ m.content; }) {
                    for (const auto& blk : m.content) {
                        if (const auto* tb = std::get_if<TextBlock>(&blk)) {
                            seed_preview += tb->text.substr(0, 64);
                            break;
                        }
                    }
                }
            }, msg);
            const std::string u24 = make_uuid24(tick_msg_idx, seed_preview);
            for (auto& e : projected) {
                e.id = u24;
                // Enrich committed thinking entries with cached duration.
                // ThinkingBlock in the core types has no timing info, so
                // the duration was cached when the streaming block completed.
                if (e.is_thinking && e.thinking_duration.count() == 0 &&
                    !e.full_content.empty()) {
                    auto it = thinking_duration_cache_.find(e.full_content);
                    if (it != thinking_duration_cache_.end()) {
                        e.thinking_duration = it->second;
                    }
                }
                new_messages.push_back(std::move(e));
            }
            ++tick_msg_idx;
        }
        // Append local-command messages (same logic as
        // AppendLocalMessagesToScreenState, but into the local vector).
        {
            static std::uint64_t s_local_seq = 0;
            for (auto it = local_command_messages_.begin();
                 it != local_command_messages_.end(); ++it) {
                if (it->id.empty()) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "loc_%016llx",
                                  (unsigned long long)s_local_seq++);
                    it->id = std::string(buf, 24);
                }
            }
            new_messages.insert(new_messages.end(),
                                local_command_messages_.begin(),
                                local_command_messages_.end());
        }
        // Restore chronological order (see SyncState). Applied BEFORE the
        // in-flight streaming projection so streaming rows stay last.
        std::stable_sort(
            new_messages.begin(),
            new_messages.end(),
            [](const repl::MessageDisplayEntry& a,
               const repl::MessageDisplayEntry& b) {
                return a.timestamp < b.timestamp;
            });

        // ── Faithful live-path: project in-flight content blocks ──
        // Each content block renders as a separate message row as it
        // streams in (thinking → text → tool-use, in block-index order).
        // We follow the same pattern: collect all active streaming blocks
        // by their index, then project each into a MessageDisplayEntry
        // that flows through RenderMessages → render_payload_row → the
        // matching faithful Element renderer (thinking / tool-use / text).
        // This way tool-use rows and thinking rows appear progressively
        // during streaming, not just after the full message completes.
        bool has_in_flight = !streaming_text_.empty() ||
                             !streaming_tools_.empty() ||
                             !streaming_thinking_.empty();

        // If any streaming tool has complete=true (ContentBlockStop received),
        // the AssistantMessage for this turn was already committed to the
        // conversation (append_message fires after all blocks finish streaming).
        // All streaming blocks (text + thinking + tool-use) from this turn
        // are now duplicates of what the committed path already projected.

        // Prune thinking entries whose 3-second grace period has expired.
        // Done before has_in_flight so stale entries don't keep the
        // projection path alive.
        {
            auto now = clock::steady_now();
            std::erase_if(streaming_thinking_,
                [&now](const auto& p) {
                    return p.second.complete &&
                           p.second.streaming_ended_at &&
                           std::chrono::duration_cast<std::chrono::seconds>(
                               now - *p.second.streaming_ended_at).count() >= 3;
                });
        }

        if (has_in_flight) {
            bool all_tools_complete = !streaming_tools_.empty() &&
                std::ranges::all_of(streaming_tools_, [](const auto& p) {
                    return p.second.complete;
                });
            // The streaming thinking tail stays visible for 3s after
            // completion (matching the collapse grace).  Keep projecting
            // in-flight blocks when thinking is still within the grace
            // period, even if all tools have completed.
            bool thinking_visible = is_streaming_thinking_visible();
            if (all_tools_complete && !thinking_visible) has_in_flight = false;
            // Once all tool blocks have stopped (ContentBlockStop received),
            // the AssistantMessage was committed to the conversation before
            // tool execution began (query_engine_loop.cpp: append_message
            // runs before execute_pending_tools).  The streaming text is
            // now a duplicate of the committed row — clear it so the
            // thinking grace period doesn't keep the text row alive.
            if (all_tools_complete) {
                streaming_text_.clear();
                streaming_text_index_.reset();
            }
        }

        loom::utils::debug("app.render",
            "  streaming-path: committed_msgs={}, in_flight={} "
            "(text_len={}, tools={}, thinking={})",
            screen_state_->messages_store.messages.size(), has_in_flight,
            streaming_text_.size(), streaming_tools_.size(),
            streaming_thinking_.size());

        if (has_in_flight) {
            // Gather block indices (keys of all streaming maps) to sort.
            std::vector<std::uint32_t> block_indices;
            for (const auto& [i, _] : streaming_thinking_)
                block_indices.push_back(i);
            for (const auto& [i, _] : streaming_tools_)
                block_indices.push_back(i);
            // Text block: use the actual block index tracked from
            // ContentBlockStart.  The model can emit text before tool_use
            // (thinking → text → tool_use), so assuming text has the highest
            // index would place it after the tools, causing the chain
            // compression to briefly include tools in the wrong chain.
            std::uint32_t text_idx = 0;
            if (!streaming_text_.empty()) {
                if (streaming_text_index_) {
                    text_idx = *streaming_text_index_;
                } else {
                    // Fallback: max of all other indices + 1, or 0.
                    for (std::uint32_t i : block_indices)
                        if (i > text_idx) text_idx = i;
                    if (!block_indices.empty()) text_idx += 1;
                }
                block_indices.push_back(text_idx);
            }
            std::sort(block_indices.begin(), block_indices.end());

            // Project each block in index order.
            // All in-flight streaming blocks belong to the same in-progress
            // source AssistantMessage → share a single 24-char uuid prefix so
            // the UnseenDivider still lands even when e.g. the first
            // block is a ThinkingBlock (CC-724).
            const std::string streaming_uuid24 =
                make_uuid24(tick_msg_idx, "streaming");
            for (std::uint32_t idx : block_indices) {
                // Thinking block
                auto thk = streaming_thinking_.find(idx);
                if (thk != streaming_thinking_.end()) {
                    repl::MessageDisplayEntry e;
                    e.role = "assistant";
                    e.is_thinking = true;
                    // Active while streaming (not complete) or within the
                    // 3s grace period after completion (entries are pruned
                    // at 3s above, so this is always true for live entries).
                    // thinking_active=true makes the faithful renderer show
                    // the expanded body rather than the collapsed label, and
                    // marks this row as the streaming tail.
                    {
                        auto now = clock::steady_now();
                        bool within_grace = thk->second.streaming_ended_at &&
                            std::chrono::duration_cast<std::chrono::seconds>(
                                now - *thk->second.streaming_ended_at).count() < 3;
                        e.thinking_active = !thk->second.complete || within_grace;
                        // Compute thinking duration for the collapsed label.
                        if (thk->second.streaming_started_at) {
                            auto end = thk->second.streaming_ended_at.value_or(now);
                            e.thinking_duration = std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                    end - *thk->second.streaming_started_at);
                        }
                    }
                    e.content_preview = thk->second.text.substr(0, 200);
                    e.full_content = thk->second.text;
                    e.timestamp = now;
                    e.id = streaming_uuid24;
                    new_messages.push_back(std::move(e));
                    continue;
                }
                // Tool-use block
                auto tlu = streaming_tools_.find(idx);
                if (tlu != streaming_tools_.end()) {
                    // Once exec_done, the committed conversation already
                    // contains this tool_use (AssistantMessage was appended
                    // before execute_pending_tools ran).  Skip it here to
                    // avoid a duplicate green-dot row in the transcript.
                    if (tlu->second.exec_done) continue;

                    repl::MessageDisplayEntry e;
                    e.role = "assistant";
                    e.is_tool_use = true;
                    e.tool_name = tlu->second.tool_name;
                    e.tool_input_json = tlu->second.input_json;
                    // Status: if tool execution has completed
                    // (ToolExecutionEnd received), show resolved status
                    // so the faithful renderer paints a green/red dot
                    // instead of the animated spinner.  The actual result
                    // lives in the separate UserToolResult card (committed
                    // ToolResultMessage), NOT in the tool_use card's
                    // Output section.
                    //
                    // We check exec_done (set by ToolExecutionEnd), NOT
                    // complete (set by ContentBlockStop when input_json
                    // finishes streaming — happens before execution).
                    if (tlu->second.exec_done) {
                        e.tool_status = tlu->second.is_error
                            ? "error" : "success";
                    } else {
                        e.tool_status = "running";
                    }
                    e.content_preview = tlu->second.input_json;
                    // result_preview: ONLY forward while the tool is
                    // still executing (progress output).  Once exec_done,
                    // suppress it — the committed ✓ card shows the final
                    // result, and showing it here too would duplicate
                    // the output and defeat the "separate result card"
                    // UX (AssistantToolUseMessage +
                    // UserToolSuccessMessage).
                    if (!tlu->second.exec_done &&
                        !tlu->second.result_preview.empty())
                        e.tool_result_preview = tlu->second.result_preview;
                    e.is_error = tlu->second.is_error;
                    e.timestamp = now;
                    e.id = streaming_uuid24;
                    new_messages.push_back(std::move(e));
                    continue;
                }
                // Text block (streaming)
                if (!streaming_text_.empty() && idx == text_idx) {
                    repl::MessageDisplayEntry e;
                    e.role = "assistant";
                    e.content_preview = streaming_text_;
                    e.is_streaming = true;
                    e.timestamp = now;
                    e.id = streaming_uuid24;
                    new_messages.push_back(std::move(e));
                    continue;
                }
            }

            // Do NOT unconditionally reset scroll_pinned_to_bottom here.
            // It is managed by HandleSubmit (true on new query) and
            // ScrollTranscript (false when the user scrolls up).  Resetting
            // it on every streaming render overrides the user's manual
            // scroll position, causing the view to jump back to the latest
            // content mid-stream.
            if (screen_state_->messages_store.scroll_pinned_to_bottom) {
                // Clear unseen divider while pinned (no unseen messages at bottom).
                screen_state_->messages_store.divider_index.reset();
                screen_state_->messages_store.unseen_divider.reset();
                screen_state_->messages_store.unseen_message_count = 0;
                screen_state_->messages_store.pill_visible = false;
            }
        }

        // Atomically replace the store contents — never leaves the store
        // in a cleared-but-empty state that would flash a blank screen.
        screen_state_->messages_store.messages.swap(new_messages);
    }

    // Reset cursor to hidden each frame.
    // Any active declared_cursor decorator on descendant elements can
    // override this with a physical cursor anchor at the declared position.
    // This enables IME preedit text to appear inline at the insertion
    // point and lets screen readers / magnifiers follow the input.
    namespace dc = loom::ui::common::declared_cursor;
    // Drain background paste results on every render frame so the user
    // doesn't need to press another key to see the image data filled in.
    this->ProcessCompletedPastes();
    auto el = repl_component_->Render() | dc::cursor_reset();
    // Drag-to-select: paint the selection rectangle's background after
    // the normal render pass.  SelectionHighlightNode (defined in
    // app_text_selection.cpp) follows FTXUI's own `inverted` decorator
    // pattern (render children, then modify pixels in-place).
    el = ApplySelectionHighlight(std::move(el));
    return el;
}

bool AppAdapter::OnEvent(Event event) {
    // Drain any background-thread paste results that completed since the
    // last event.  Must happen on the render thread (we mutate
    // paste_.pasted_contents() and possibly input_text).
    this->ProcessCompletedPastes();

    // Pane-teammate inbox delivery: the inbox worker posts Custom events when
    // tasks arrive. While idle, submit one queued teammate prompt here on the
    // UI thread. Ignore keyboard/other events and never block a running query.
    if (event == Event::Custom) {
        // Event-driven pane-observer wake: the background capture pass flags
        // this atomic (never touches screen state off-thread). Gated so the
        // 50ms PostRenderEvent animation traffic does no settings.json I/O.
        if (pane_snapshot_dirty_.exchange(false,
                                          std::memory_order_acq_rel)) {
            ProjectLiveTeammatesToScreenState();
        }
        // Leader-side teammate permission requests reuse the existing
        // ToolPermission dialog (one per Custom event, overlay stays empty).
        if (drain_one_teammate_permission()) return true;
        if (running_as_pane_teammate()) {
            if (drain_one_teammate_prompt()) return true;
        }
    }

    // Clipboard image paste (ctrl+v / cmd+v).
    //
    // NOTE: We detect Ctrl+V via `Event::Character('\x16')` — the same
    // pattern used by text_input.cppm L586.  `event.input() == "\x16"`
    // is unreliable because FTXUI normalizes control-character input()
    // to empty string in some code paths.
    //
    // PERFORMANCE NOTE: osascript clipboard reads are offloaded to a
    // background thread because std::system() + osascript fork + PNG
    // encoding can take 600ms+ and would block the FTXUI event loop
    // if done synchronously.  The "[Image #N]" placeholder is inserted
    // IMMEDIATELY so the user sees instant feedback; the actual image
    // data fills in asynchronously via ProcessCompletedPastes() (drained
    // on every OnEvent AND every Render() frame).
    //
    if (event == Event::Character('\x16') && !query_running_.load()) {
        const int id = paste_.allocate_paste_id();

        // Insert " [Image #N]" at cursor IMMEDIATELY (instant feedback).
        const std::string placeholder = loom::utils::format_image_ref(id);
        const auto cursor = repl::input_cursor_or_end(*screen_state_);
        std::string to_insert;
        if (cursor > 0 && cursor <= screen_state_->input_text.size() &&
            screen_state_->input_text[cursor - 1] != ' ') {
            to_insert = " " + placeholder;
        } else {
            to_insert = placeholder;
        }
        repl::insert_prompt_text(screen_state_, to_insert);

        // Offload the actual clipboard read to a background thread.
        this->SpawnPasteWorker(id);
        return true;
    }

    // ── Ctrl+S: manual stash/unstash ─────────────────
    //   If input is empty AND stash exists → pop stash (restore text).
    //   If input is non-empty → stash input and clear input.
    //   This lets users temporarily put aside a long prompt to run a
    //   quick command, then restore it.
    //
    // Suppress this global shortcut while any focus-taking dialog is open:
    // the dialog's own CatchEvent owns Ctrl+S there (e.g. the /statusline
    // segments dialog uses it for Save).  Without this guard the handler
    // returns before delegating to repl_component_ (below), so the dialog
    // never sees the keystroke.
    const bool any_dialog_open =
        screen_state_->dialog_store.dialog_queue.has_standalone()
        || screen_state_->dialog_store.dialog_queue.has_modal()
        || screen_state_->dialog_store.dialog_queue.has_overlay();
    if (event == Event::Character('\x13') && !query_running_.load()
        && !any_dialog_open) {
        namespace repl = loom::ui::repl_screen;
        const auto& input = screen_state_->input_text;

        // Trim check: `input.trim() === ''` decides pop-vs-push.
        // An input of only whitespace is treated as "empty" for stash pop.
        const bool input_has_content = [&] {
            for (char c : input) {
                if (!std::isspace(static_cast<unsigned char>(c))) return true;
            }
            return false;
        }();

        if (!input_has_content && repl::HasStashedPrompt(screen_state_)) {
            // Pop stash: restore stashed text + pasted contents.
            std::unordered_map<int, ::loom::core::ImageBlock> restored_images;
            std::unordered_map<int, std::string> restored_texts;
            repl::RestoreStashedPrompt(screen_state_, &restored_images, &restored_texts);
            // Merge restored pasted contents back into engine maps.
            for (auto& [id, img] : restored_images) {
                paste_.pasted_contents()[id] = std::move(img);
            }
            for (auto& [id, txt] : restored_texts) {
                paste_.pasted_text_contents()[id] = std::move(txt);
            }
            PostRenderEvent();
            return true;
        }
        if (input_has_content) {
            // Push stash: save current input + referenced pasted contents,
            // then clear input.
            const auto refs = loom::utils::parse_references(input);
            std::unordered_map<int, ::loom::core::ImageBlock> ref_images;
            std::unordered_map<int, std::string> ref_texts;
            for (const auto& r : refs) {
                if (auto it = paste_.pasted_contents().find(r.id);
                    it != paste_.pasted_contents().end()) {
                    ref_images[r.id] = it->second;
                }
                if (auto it = paste_.pasted_text_contents().find(r.id);
                    it != paste_.pasted_text_contents().end()) {
                    ref_texts[r.id] = it->second;
                }
            }
            repl::StashCurrentPrompt(screen_state_,
                std::move(ref_images), std::move(ref_texts));
            // Clear input.
            repl::set_prompt_input_text(screen_state_, {}, 0);
            // Clear pasted contents that were only referenced by the
            // stashed text (orphan cleanup will handle this naturally).
            PostRenderEvent();
            return true;
        }
        // Empty input with no stash → nothing to do.
        return false;
    }

    // ── Drag-to-select text in the transcript ─────────────────────────
    // Delegated to app_text_selection.cpp.  Returns true if the event
    // was consumed (drag active or completed).  Existing click handlers
    // (hyperlink, click-to-expand) use Released events, so press/motion
    // interception doesn't break them — a click without drag passes through.
    if (HandleTextSelectionMouse(event)) return true;

    // ── Markdown hyperlink click-to-open ─────────────────────────────
    // In fullscreen / alternate-screen mode, mouse tracking intercepts
    // all clicks before the terminal can natively open OSC 8 hyperlinks.
    // We compensate by detecting left-button releases at pixels that
    // carry a hyperlink ID (set by FTXUI's `hyperlink(url)` decorator
    // in markdown.cppm render_inlines → InlineTokenKind::Link).
    //
    // The Screen stores hyperlink URLs in hyperlinks_[], indexed by
    // the uint8_t ID written to Pixel.hyperlink during Render().  We
    // look up the URL via screen.Hyperlink(id) and route it through
    // loom::utils::try_open_hyperlink() (file: → open_file_path,
    // http(s): → open_browser).
    if (event.is_mouse()) {
        const auto& mouse = event.mouse();
        // Left-button release = "click".  Using release (not press)
        // lets the user cancel by dragging the cursor off the link
        // before releasing — standard web/desktop link behavior.
        if (mouse.button == Mouse::Left &&
            mouse.motion == Mouse::Released) {
            if (auto* screen = screen_.load(std::memory_order_acquire)) {
                const int x = mouse.x;
                const int y = mouse.y;
                // Bounds check — screen dims may change between render
                // and event delivery (terminal resize race).
                if (x >= 0 && x < screen->dimx() &&
                    y >= 0 && y < screen->dimy()) {
                    const uint8_t link_id = screen->PixelAt(x, y).hyperlink;
                    if (link_id != 0) {
                        // id 0 = no hyperlink (Screen default).
                        const std::string& url = screen->Hyperlink(link_id);
                        if (!url.empty()) {
                            // Fire-and-forget: open the link.  The
                            // system call (open / xdg-open) returns
                            // quickly; we don't block on browser
                            // launch completion.
                            (void)loom::utils::try_open_hyperlink(url);
                            return true;  // event consumed
                        }
                    }
                }
            }
        }
    }

    const bool handled = repl_component_->OnEvent(event);
    if (handled) {
        // SL-11: any accepted keystroke that fills input retires the
        // next-action suggestion for this turn (RefreshAutocompleteSuggestions
        // also clears on non-empty input, but this is the unambiguous
        // reset for the accept-on-Return path).
        if (!screen_state_->input_text.empty()) {
            screen_state_->next_action_suggestion.reset();
        }
        RefreshAutocompleteSuggestions();

        // Orphan cleanup.
        // Prune paste_.pasted_contents() entries whose [Image #N] placeholder is
        // no longer in the input text (covers backspace-over-pill, Ctrl+U,
        // char-by-char deletion — any edit that drops the ref).
        // Also prune paste_.pasted_text_contents() entries whose [...Truncated text #N]
        // ref is no longer in the input (same orphan scenarios for text pastes).
        if (!paste_.pasted_contents().empty() || !paste_.pasted_text_contents().empty()) {
            const auto refs = loom::utils::parse_references(screen_state_->input_text);
            std::unordered_set<int> referenced_ids;
            for (const auto& r : refs) referenced_ids.insert(r.id);
            for (auto it = paste_.pasted_contents().begin(); it != paste_.pasted_contents().end(); ) {
                if (!referenced_ids.contains(it->first)) {
                    it = paste_.pasted_contents().erase(it);
                } else {
                    ++it;
                }
            }
            for (auto it = paste_.pasted_text_contents().begin(); it != paste_.pasted_text_contents().end(); ) {
                if (!referenced_ids.contains(it->first)) {
                    it = paste_.pasted_text_contents().erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    return handled;
}

Component AppAdapter::ActiveChild() {
    return repl_component_;
}

}  // namespace loom::ui
