/// @file repl_screen.cppm
/// @brief Main REPL screen skeleton: enums, state, layout orchestration,
///        dialog routing, and event binding.  Rendering sub-modules delegate
///        to dedicated UIx agents (see ownership matrix below).
///
/// =========================================================
/// PHASE 4 COMPONENT MATRIX — Sub-Component -> Agent Ownership
/// =========================================================
/// UI2  PromptInput full              UI11 Onboarding / wizards
/// UI3  Settings / model picker      UI12 Tasks panel UI
/// UI4  Assistant msg (markdown)     UI13 Agents panel / editor
/// UI5  User msg / attachments       UI14 Teams panel
/// UI6  CustomSelect / dropdowns     UI15 Install wizards
/// UI7  Structured diff viewer       UI16 Plugin dialogs / recs
/// UI8  Trust + sandbox dialogs      UI17 Code rendering (hl/md/term)
/// UI9  Permissions system           UI18 Feedback surveys
/// UI10 MCP dialogs (elicit/OAuth)   UI19 Spinner animations + tree
/// =========================================================
///
/// RENDERING DELEGATION (this file does NOT contain render bodies):
///   status bar   -> loom.ui.prompt.prompt_input_footer (UI1, user-configurable command output)
///   spinner      -> loom.ui.components.spinner_widget (UI19)
///   msg list     -> loom.ui.messages.messages (RenderMessages wrapper, UI4/5)
///   prompt input -> loom.ui.prompt.* (UI2)
///   dialogs      -> loom.ui.dialogs.* (DialogQueue 4-slot system, UI8-UI11/UI16)
///
/// RFC 0001 Phase C batch 9: every body lives in ten module implementation
/// units — repl_screen_messages.cpp (row projection), repl_screen_scroll.cpp
/// (unseen divider / scroll bounds), repl_screen_prompt_buffer.cpp (input
/// mutation + stash), repl_screen_welcome.cpp (status bar / spinner / welcome
/// header), repl_screen_prompt_render.cpp (prompt input + suggestions),
/// repl_screen_dialog_queue.cpp (the 4-slot queue renderer),
/// repl_screen_layout.cpp (RenderReplScreen + RouteDialog),
/// repl_screen_agents.cpp (agent wizard + agents menu),
/// repl_screen_dialog_panels.cpp (settings / trust / tool permission), and
/// repl_screen_events.cpp (both ReplScreen factories).  This primary keeps
/// only declarations, AgentMenuOptions, and the declaration-only
/// AgentMenuListBase class.
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.screens.repl_screen;

import std;

// RFC 0002 F3 Finalize: repl_state is imported (not re-exported) — call sites
// that name ReplScreenState / ReplMode / the store types import repl_state or
// the owning loom.ui.screens.*_store module directly.
import loom.ui.screens.repl_state;
// Store types named in this interface's declarations.
import loom.ui.screens.messages_store;   // MessageDisplayEntry
import loom.ui.screens.task_view_store;  // SpinnerMode
import loom.ui.screens.chrome_store;     // StatusBarData

// Core engine types (ImageBlock in the stash signatures; only named
// globally qualified as ::loom::core::ImageBlock).
import loom.types.types;  // arch-check: keep-import
// UnseenDivider used by RenderMessages / ComputeUnseenDivider.
import loom.ui.messages.messages_list;
// StreamingMarkdown pointer in surviving declarations (only named
// globally qualified as ::loom::ui::StreamingMarkdown*).
import loom.ui.visual.markdown;  // arch-check: keep-import
// AgentCardData is a member type of AgentMenuOptions.
import loom.ui.features.agents.agent_cards;

// Forward imports (implement bodies in owning agent modules):
//   loom.ui.dialogs.{permission_prompts,mcp_dialogs,trust_dialog,
//                  sandbox_dialog,settings_dialog,model_picker,
//                  ide_dialogs,plugin_dialog,
//                  feedback_survey,config_dialog,mcp_dialogs}
//   loom.ui.prompt.{autocomplete,vim_input}
//   loom.ui.messages.{assistant_message,user_message,structured_diff}
//   loom.ui.components.{custom_select,diff_view,spinner_widget,
//                     file_tree,text_input_widget,notification,
//                     cost_display,dev_bar,status_line}
//   loom.ui.{design.dialog,hooks.hooks_ui,permissions.permission_views,
//          agents.agent_editor,tasks.task_list_ui,markdown,terminal}

export namespace loom::ui::repl_screen {
using namespace ftxui;

// =========================================================
// Status bar / spinner / welcome primitives
// (bodies in repl_screen_welcome.cpp)
// =========================================================

// UI1: status bar — delegates to loom.ui.components.status_line.
[[nodiscard]] Element RenderStatusBar(const StatusBarData& d);

// UI19: spinner line shell.  Single loom-gold theme, cycling
// TEARDROP_ASTERISK glyph, random playful verb, 3-dot blink cadence.
[[nodiscard]] Element RenderSpinner(
    SpinnerMode m, const std::optional<std::string>& verb,
    const std::optional<std::string>& tip,
    int frame = 0);

// Column-width truncation helper; also used by the prompt-render unit.
[[nodiscard]] std::string truncate_columns(std::string text, int max_cols);

[[nodiscard]] std::string repeat_welcome_segment(std::string_view text, int count);

// =========================================================
// Unseen divider + transcript projection / scroll
// (bodies in repl_screen_scroll.cpp)
// =========================================================

namespace unseen_detail {

/// Whether an assistant entry has visible text content.
[[nodiscard]] bool assistant_has_visible_text(
    const MessageDisplayEntry& e);

/// Count assistant turns in entries[start_idx..end).
[[nodiscard]] std::size_t count_unseen_assistant_turns(
    const std::vector<MessageDisplayEntry>& entries,
    std::size_t start_idx);

}  // namespace unseen_detail

/// Compute the UnseenDivider from state.messages_store.divider_index + messages.
[[nodiscard]] std::optional<::loom::ui::messages_list::UnseenDivider>
ComputeUnseenDivider(const ReplScreenState& s);

/// Borrow store rows; append active local-jsx rows in store-owned scratch storage.
[[nodiscard]] const std::vector<MessageDisplayEntry>& BuildVisibleMessages(
    const ReplScreenState& s);

// UI4/UI5: message list.  Delegates to messages_list.cppm (UI21).
// `spinner_frame` drives the tool-use header spinner; `unseen_divider` is
// the in-transcript "N new messages" anchor.
[[nodiscard]] Element RenderMessages(
    const std::vector<MessageDisplayEntry>& entries,
    int sel = -1, int vlines = 40,
    int offs = 0, bool pinned = true,
    int spinner_frame = 0,
    std::optional<loom::ui::messages_list::UnseenDivider> unseen_divider =
        std::nullopt,
    Elements leading_elements = {},
    bool is_brief_mode = false,
    const std::unordered_set<std::string>& expanded_keys = {},
    bool is_transcript_mode = false,
    bool show_all_in_transcript = false,
    // Test-only: skip chain compression when true.
    bool disable_chain_compression = false,
    // Production: true while a query is actively streaming.
    bool query_running = false,
    // Retry callback for SystemAPIError rich cards.
    std::function<void()> on_retry = nullptr,
    // Clear-session callback for session-expired error cards.
    std::function<void()> on_clear_session = nullptr,
    // StreamingMarkdown stable-prefix cache for the streaming-text tail row.
    ::loom::ui::StreamingMarkdown* streaming_md = nullptr,
    // Optional mouse hit-testing tracker for click-to-expand rows.
    // When set, the renderer populates it with screen-space boxes of
    // clickable rows (thinking blocks) each frame.
    loom::ui::messages_list::RowClickTracker* row_click_tracker = nullptr,
    // Optional store for virtual-list scroll bounds.  When set and the
    // virtual render path is used, the renderer populates virtual_jh
    // (exact geometry) and virtual_list_active so ScrollTranscript uses
    // precise bounds instead of the EstimateTranscriptRows heuristic.
    MessagesStore* store = nullptr);

/// Count '\n'-separated lines in text (minimum 1).
[[nodiscard]] int CountTextLines(std::string_view text);

/// Estimate rendered transcript height for the non-virtual scroll path.
/// `term_cols` enables wrapping-aware line counting so long paragraphs
/// contribute their visual wrapped height instead of just '\n' count.
[[nodiscard]] int EstimateTranscriptRows(
    const std::vector<MessageDisplayEntry>& entries,
    int term_cols = 80);

/// Wheel / PageUp / PageDown scroll against the virtual JumpHandle or the
/// crude row estimator; maintains the unseen-divider snapshot.
bool ScrollTranscript(const std::shared_ptr<ReplScreenState>& state,
                      int delta);

// =========================================================
// Legacy ASCII-art welcome helpers (preserved, no longer called by
// RenderWelcomeHeader; bodies in repl_screen_welcome.cpp).
// =========================================================
[[nodiscard]] Element RenderWelcomeFeed(std::string title,
                                        std::string message,
                                        int width,
                                        Color accent,
                                        Color muted);
[[nodiscard]] Element RenderWelcomeFeedColumn(int width,
                                              Color accent,
                                              Color muted);
[[nodiscard]] Element RenderWelcomeWindow(Element title,
                                          Element body,
                                          int width,
                                          int title_width,
                                          Color accent);

// UI0: welcome header — LogoV2 3-mode dispatch (condensed / compact /
// horizontal) on a fresh idle session.
[[nodiscard]] Element RenderWelcomeHeader(const ReplScreenState& s,
                                          int spinner_frame = 0,
                                          int term_cols = 80,
                                          bool force_full_logo = false);

// =========================================================
// Prompt input buffer mutation
// (bodies in repl_screen_prompt_buffer.cpp)
// =========================================================

[[nodiscard]] bool is_utf8_continuation_byte(unsigned char c);
[[nodiscard]] std::size_t clamp_input_cursor(
    const std::string& text,
    std::size_t pos);
[[nodiscard]] std::size_t input_cursor_or_end(
    const ReplScreenState& s);
[[nodiscard]] std::size_t previous_utf8_boundary(
    const std::string& text,
    std::size_t pos);
[[nodiscard]] std::size_t next_utf8_boundary(
    const std::string& text,
    std::size_t pos);

void set_prompt_input_text(
    const std::shared_ptr<ReplScreenState>& state,
    std::string value,
    std::size_t cursor);
void insert_prompt_text(
    const std::shared_ptr<ReplScreenState>& state,
    std::string_view value);

// AT-09: apply inbound IDE at_mentioned tokens drained from the AppImpl
// staging queue into mcp_status_store. MUST be called on the render thread
// (it mutates input_text/cursor). The staging mutex lives in the app
// composition layer (AppAdapter::DrainPendingAtMentionInserts).
std::size_t ApplyPendingAtMentionInserts(
    const std::shared_ptr<ReplScreenState>& state);

/// Stash the current input text and cursor position (GAP 2).
bool StashCurrentPrompt(
    const std::shared_ptr<ReplScreenState>& state,
    std::unordered_map<int, ::loom::core::ImageBlock> pasted_images = {},
    std::unordered_map<int, std::string> pasted_texts = {});

/// Restore the stashed prompt; returns pasted maps via out-params.
bool RestoreStashedPrompt(
    const std::shared_ptr<ReplScreenState>& state,
    std::unordered_map<int, ::loom::core::ImageBlock>* out_images = nullptr,
    std::unordered_map<int, std::string>* out_texts = nullptr);

/// True when a stashed prompt exists (for UI notice rendering).
bool HasStashedPrompt(const std::shared_ptr<ReplScreenState>& state);

bool backspace_prompt_text(const std::shared_ptr<ReplScreenState>& state);
// At-caret-0 Backspace/Escape/Delete/Ctrl-U exits a special input mode.
bool exit_input_mode_if_at_start(const std::shared_ptr<ReplScreenState>& state);
bool delete_prompt_text(const std::shared_ptr<ReplScreenState>& state);
void move_prompt_cursor_left(const std::shared_ptr<ReplScreenState>& state);
void move_prompt_cursor_right(const std::shared_ptr<ReplScreenState>& state);

// Text-derived bash mode (text-prefixed-with-! detection).
[[nodiscard]] bool effective_is_bash(const ReplScreenState& s);

// =========================================================
// Prompt input rendering
// (bodies in repl_screen_prompt_render.cpp)
// =========================================================

// 4-tier contextual placeholder (adapter onto placeholder_cascade).
[[nodiscard]] std::optional<std::string> ComputePlaceholder(
    const ReplScreenState& s);

// The pure-function prompt-input renderer: TextInputImpl synced from the
// projection, vim badge, stash notice, declared caret.
[[nodiscard]] Element RenderPromptInput(const ReplScreenState& s,
                                        int term_cols);

[[nodiscard]] std::string pad_to_columns(std::string text, int width);

// Autocomplete suggestion list (inline footer or fullscreen overlay).
[[nodiscard]] Element RenderPromptSuggestions(const ReplScreenState& s,
                                              int term_cols,
                                              bool is_overlay = false,
                                              int term_rows = 24);

// Accept the currently-selected autocomplete suggestion into the buffer.
[[nodiscard]] std::optional<std::string> accept_selected_prompt_suggestion(
    const std::shared_ptr<ReplScreenState>& state);

// =========================================================
// M7: Queue-based dialog rendering
// (bodies in repl_screen_dialog_queue.cpp)
//
// Four slots in priority order: Standalone > Modal > Overlay > Bottom.
// =========================================================
namespace dialog_queue_render {

/// Build the render-time width/height context for the registry.
[[nodiscard]] DialogRenderContext MakeContext(
    int term_w = 120, int term_h = 40, bool is_modal = false,
    const void* repl_state = nullptr);

/// Standalone dialog: full-takeover render (no chrome).
[[nodiscard]] Element RenderStandaloneDialog(ReplScreenState& s,
                                             int w = 120, int h = 40);

/// Modal dialog (stack top): rendered full-width dbox above the rest.
[[nodiscard]] Element RenderModalDialog(ReplScreenState& s,
                                        int w = 120, int h = 40);

/// Overlay dialog (ToolPermission, Band3): centered floating dbox.
[[nodiscard]] Element RenderOverlayDialog(
    ReplScreenState& s,
    bool is_prompt_input_active,
    bool allow_dialogs_with_animation = true,
    int w = 120, int h = 40);

/// Bottom slot: banner-style dialogs affixed above the prompt.
[[nodiscard]] Element RenderBottomDialog(
    ReplScreenState& s,
    bool is_prompt_input_active,
    bool allow_dialogs_with_animation,
    int w = 120, int h = 40);

// Event dispatch in priority Standalone > Modal > Overlay > Bottom.
bool DispatchDialogQueueEvents(ReplScreenState& s,
                               const ftxui::Event& ev,
                               bool is_prompt_input_active,
                               bool allow_dialogs_with_animation);

/// Combine Overlay + Modal + Bottom into one dbox over the base chrome.
[[nodiscard]] Element LayerAllDialogs(
    Element base_chrome,
    ReplScreenState& s,
    bool is_prompt_input_active,
    bool allow_dialogs_with_animation,
    int w = 120, int h = 40);

}  // namespace dialog_queue_render

/// Top-level dialog router: ReplMode -> overlay Element.  Legacy routing is
/// retired (M7); always returns nullopt today.
[[nodiscard]] std::optional<Element> RouteDialog(
    ReplMode m, const ReplScreenState& s);

// =========================================================
// Full layout composition
// (body in repl_screen_layout.cpp)
// =========================================================

/// Compose the REPL screen via the FullscreenLayout slot system and layer
/// the dialog queue on top.
[[nodiscard]] Element RenderReplScreen(ReplScreenState& s,
    std::function<void()> on_retry = nullptr,
    std::function<void()> on_clear_session = nullptr,
    ::loom::ui::StreamingMarkdown* streaming_md = nullptr);

// =========================================================
// Dialog routing: agent wizard / agents menu / settings / trust /
// tool-permission panels (bodies in repl_screen_agents.cpp and
// repl_screen_dialog_panels.cpp).
// =========================================================
namespace dialog_router {

// -------------------------------------------------------------------
// Agent wizard helpers
// -------------------------------------------------------------------

/// Lazily create (or re-create) the agent wizard component.
[[nodiscard]] std::shared_ptr<Component> get_agent_wizard(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Forward an event to the agent wizard component.
bool forward_agent(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev);

/// Render the agent wizard content as an Element.
[[nodiscard]] Element render_agent_wizard(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Reset (destroy) the agent wizard so the next entry starts fresh.
void reset_agent_wizard(const std::shared_ptr<ReplScreenState>& s);

// -------------------------------------------------------------------
// Agents menu helpers (UI13)
// -------------------------------------------------------------------

namespace agents_menu {

struct AgentMenuOptions {
    std::vector<cards::AgentCardData> agents;
    std::function<void()> on_create_new;
    std::function<void(const std::string& id)> on_select;
    std::function<void()> on_cancel;
};

[[nodiscard]] bool is_built_in(const cards::AgentCardData& agent);

[[nodiscard]] std::string resolved_model_label(
    const cards::AgentCardData& agent);

[[nodiscard]] std::vector<std::size_t> selectable_indices(
    const std::vector<cards::AgentCardData>& agents);

class AgentMenuListBase : public ComponentBase {
public:
    explicit AgentMenuListBase(AgentMenuOptions opts);

    Element Render() override;
    bool OnEvent(Event ev) override;

private:
    [[nodiscard]] static int selected_position_for_index(
        const std::vector<std::size_t>& selectable,
        std::size_t index);

    [[nodiscard]] static Element render_create_row(bool selected, Color suggestion);

    [[nodiscard]] static Element render_agent_row(
        const cards::AgentCardData& agent,
        bool selected,
        bool selectable,
        Color suggestion,
        Color muted);

    AgentMenuOptions opts_;
    int selected_position_ = 0;  // 0 is "Create new agent".
};

[[nodiscard]] Component AgentMenuList(AgentMenuOptions opts);

}  // namespace agents_menu

void close_agents_menu(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

[[nodiscard]] std::shared_ptr<Component> get_agents_component(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

[[nodiscard]] Element render_agents_menu(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

bool forward_agents_menu(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev);

// -------------------------------------------------------------------
// Settings dialog helpers (UI3)
// -------------------------------------------------------------------

/// Lazily create (or re-create) the settings dialog component.
[[nodiscard]] std::shared_ptr<Component> get_settings_component(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Reset (destroy) the settings component so the next entry starts fresh.
void reset_settings_component(const std::shared_ptr<ReplScreenState>& s);

/// Render the settings dialog content as an Element.
[[nodiscard]] Element render_settings(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Forward an event to the settings dialog component.
bool forward_settings(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev);

// -------------------------------------------------------------------
// Trust dialog helpers (UI8)
// -------------------------------------------------------------------

/// Lazily create (or re-create) the trust dialog component.
[[nodiscard]] std::shared_ptr<Component> get_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Render the trust dialog content as an Element.
[[nodiscard]] Element render_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb);

/// Forward an event to the trust dialog component.
bool forward_trust_dialog(
    const std::shared_ptr<ReplScreenState>& s,
    const std::shared_ptr<ReplScreenCallbacks>& cb,
    Event ev);

}  // namespace dialog_router

// =========================================================
// FTXUI Component factory (bodies in repl_screen_events.cpp)
// =========================================================

/// Build the REPL screen as an FTXUI Component.
/// Engine updates the externally-held state between frames.
[[nodiscard]] Component ReplScreen(
    std::shared_ptr<ReplScreenState> state,
    ReplScreenCallbacks cbs);

/// Convenience: self-owned state (demos/tests only).
/// Production: use externally-held shared_ptr<ReplScreenState> overload.
[[nodiscard]] Component ReplScreen(ReplScreenCallbacks cbs);

}  // namespace loom::ui::repl_screen
