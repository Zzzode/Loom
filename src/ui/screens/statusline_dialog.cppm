/// @file statusline_dialog.cppm
/// @brief Interactive toggle dialog for the built-in status bar segments.
///
/// MODULE:   loom.ui.screens.statusline_dialog
/// LICENCE:  Exported.  Imported by the app composition root to register
///           the statusline dialog renderer alongside the default set.
///
/// Lives on the screens side (rank 10) because the dialog needs live
/// preview values from ReplScreenState (cwd, git branch, model, tokens,
/// cost).  dialogs (rank 9) cannot import screens (rank 10) — that would
/// be a back edge under the UI9 rank table.  screens -> dialogs is
/// downward-legal, so the registration lives here, following the
/// doctor_dialog_registration.cppm precedent.
///
/// The dialog reads/writes the `statusLine.segments` object in
/// settings.json via ConfigManager.  On save it also updates
/// ReplScreenState::status_bar_segments directly so the footer
/// re-renders with the new mask on the next frame (no restart needed).
module;

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.screens.statusline_dialog;

import std;

import loom.config.config;
import loom.ui.dialogs.system;
import loom.ui.screens.repl_state;
import loom.ui.prompt.prompt_input_footer;

export namespace loom::ui::screens::statusline_dialog {

using namespace ftxui;
using loom::core::ConfigManager;
using loom::ui::prompt::footer::StatusBarSegments;
using loom::ui::repl_screen::ReplScreenState;

namespace dsys = loom::ui::dialogs::system;
namespace pif = loom::ui::prompt::footer;

// ============================================================
// Segment metadata (internal — not exported)
// ============================================================

namespace detail {

struct SegmentRow {
    std::string label;
    std::string icon;
    bool (*get)(const StatusBarSegments&);
    void (*set)(StatusBarSegments&, bool);
};

inline constexpr std::array<SegmentRow, 7> kSegments = {{
    {"Folder",  "📁",
     [](const StatusBarSegments& s) { return s.cwd; },
     [](StatusBarSegments& s, bool v) { s.cwd = v; }},
    {"Git",     "🌿",
     [](const StatusBarSegments& s) { return s.git; },
     [](StatusBarSegments& s, bool v) { s.git = v; }},
    {"Model",   "🤖",
     [](const StatusBarSegments& s) { return s.model; },
     [](StatusBarSegments& s, bool v) { s.model = v; }},
    {"Tokens",  "▮",
     [](const StatusBarSegments& s) { return s.tokens; },
     [](StatusBarSegments& s, bool v) { s.tokens = v; }},
    {"Cost",    "$",
     [](const StatusBarSegments& s) { return s.cost; },
     [](StatusBarSegments& s, bool v) { s.cost = v; }},
    {"Tasks",   "⚙",
     [](const StatusBarSegments& s) { return s.tasks; },
     [](StatusBarSegments& s, bool v) { s.tasks = v; }},
    {"Agent",   "👤",
     [](const StatusBarSegments& s) { return s.agent; },
     [](StatusBarSegments& s, bool v) { s.agent = v; }},
}};

} // namespace detail

// ============================================================
// Preview value extraction
// ============================================================

[[nodiscard]] inline std::string preview_for(
    std::size_t idx, const ReplScreenState& s)
{
    switch (idx) {
        case 0:  // cwd
            return pif::GetLastPathComponents(s.cwd, 2);
        case 1:  // git
            return s.chrome_store.git_branch.empty()
                ? "(not a repo)" : s.chrome_store.git_branch;
        case 2:  // model
            return !s.chrome_store.model_display_name.empty()
                ? s.chrome_store.model_display_name
                : s.chrome_store.status_bar.model_name;
        case 3: {  // tokens
            const auto& sb = s.chrome_store.status_bar;
            return std::format("{}/{}",
                pif::FormatTokensK(sb.context_token_count),
                pif::FormatTokensK(200000));
        }
        case 4: {  // cost
            const auto& sb = s.chrome_store.status_bar;
            if (!sb.cost_usd || *sb.cost_usd <= 0.0) return "(no cost yet)";
            char buf[32];
            std::snprintf(buf, sizeof(buf), "$%.4f", *sb.cost_usd);
            return buf;
        }
        case 5:  // tasks
            return std::format("{} teammate{}",
                s.task_view_store.teammate_count,
                s.task_view_store.teammate_count == 1 ? "" : "s");
        case 6:  // agent
            return s.chrome_store.status_bar.agent_name
                .value_or(s.settings_agent_name);
    }
    return "";
}

// ============================================================
// Dialog state
// ============================================================

struct DialogState {
    ConfigManager* cfg = nullptr;
    ReplScreenState* screen = nullptr;  // mutable for post-save update
    StatusBarSegments working;
    StatusBarSegments saved;
    bool master_enabled = true;
    bool master_saved = true;
    int focus_row = 0;  // 0 = master, 1..7 = segments
    bool dirty = false;
    std::string toast;
    std::chrono::steady_clock::time_point toast_until;
    std::function<void()> on_close;

    static constexpr int kRowCount = 8;  // master + 7 segments

    void show_toast(std::string msg) {
        toast = std::move(msg);
        toast_until = std::chrono::steady_clock::now()
                      + std::chrono::seconds(3);
    }

    [[nodiscard]] bool toast_active() const {
        return !toast.empty() &&
               std::chrono::steady_clock::now() < toast_until;
    }

    void mark_dirty() {
        dirty = true;
    }
};

// ============================================================
// Component factory
// ============================================================

[[nodiscard]] Component MakeStatusLineDialog(
    ConfigManager& cfg,
    ReplScreenState& screen,
    std::function<void()> on_close = {})
{
    auto state = std::make_shared<DialogState>();
    state->cfg = &cfg;
    state->screen = &screen;
    state->working = screen.status_bar_segments;
    state->saved = state->working;
    state->master_enabled = screen.status_bar_enabled;
    state->master_saved = state->master_enabled;
    state->on_close = std::move(on_close);

    auto save = [state]() {
        // Persist to ConfigManager.
        state->cfg->settings_mut().status_line.segments.clear();
        state->cfg->settings_mut().status_line.segments["cwd"]    = state->working.cwd;
        state->cfg->settings_mut().status_line.segments["git"]    = state->working.git;
        state->cfg->settings_mut().status_line.segments["model"]  = state->working.model;
        state->cfg->settings_mut().status_line.segments["tokens"] = state->working.tokens;
        state->cfg->settings_mut().status_line.segments["cost"]   = state->working.cost;
        state->cfg->settings_mut().status_line.segments["tasks"]  = state->working.tasks;
        state->cfg->settings_mut().status_line.segments["agent"]  = state->working.agent;
        state->cfg->settings_mut().status_line.enabled = state->master_enabled;

        auto res = state->cfg->save_user();
        if (res) {
            // Directly update the screen state so the footer re-renders
            // with the new mask on the next frame.
            state->screen->status_bar_segments = state->working;
            state->screen->status_bar_enabled = state->master_enabled;
            state->saved = state->working;
            state->master_saved = state->master_enabled;
            state->dirty = false;
            state->show_toast("✓ Saved to ~/.loom/settings.json");
        } else {
            state->show_toast("✗ Save failed: " + res.error().message);
        }
    };

    return Renderer([state] {
        Elements rows;

        // Title
        rows.push_back(hbox({
            text(" Status Bar Segments ") | bold | color(Color::Magenta),
            filler(),
        }));
        rows.push_back(separator());

        // Master toggle (row 0)
        {
            bool focused = (state->focus_row == 0);
            auto toggle = text(state->master_enabled ? " [ON]  " : " [OFF] ")
                        | color(state->master_enabled ? Color::Green : Color::GrayDark)
                        | (focused ? inverted : nothing);
            auto label = text(" Status Bar")
                       | size(WIDTH, EQUAL, 14)
                       | (focused ? bold : nothing);
            auto hint = text(state->master_enabled ? "visible" : "hidden")
                       | dim | color(Color::GrayLight);
            rows.push_back(hbox({toggle, label, text("  "), hint}));
        }

        rows.push_back(separator());

        // Segment toggles (rows 1..7)
        for (std::size_t i = 0; i < detail::kSegments.size(); ++i) {
            const auto& seg = detail::kSegments[i];
            bool on = seg.get(state->working);
            bool focused = (state->focus_row == static_cast<int>(i + 1));
            bool dimmed = !state->master_enabled;

            auto toggle = text(on ? " [x]  " : " [ ] ")
                        | color(on ? Color::Green : Color::GrayDark)
                        | (focused ? inverted : nothing)
                        | (dimmed ? dim : nothing);
            auto label = text(" " + seg.label)
                       | size(WIDTH, EQUAL, 14)
                       | (focused ? bold : nothing)
                       | (dimmed ? dim : nothing);
            auto preview = text(preview_for(i, *state->screen))
                          | dim | color(Color::GrayLight);

            rows.push_back(hbox({toggle, label, text("  "), preview}));
        }

        rows.push_back(separator());

        // Footer
        auto footer = hbox({
            text(" Space ") | color(Color::Green),
            text("toggle  "),
            text(" Ctrl+S ") | color(Color::Green),
            text("save  "),
            text(" Esc ") | color(Color::Red),
            text("close"),
            filler(),
            state->dirty ? text(" * unsaved") | color(Color::Yellow) | dim
                         : text(""),
        });
        rows.push_back(footer);

        if (state->toast_active()) {
            rows.push_back(text(" " + state->toast) | color(Color::Green));
        }

        return window(text(" Status Line ") | bold | color(Color::Magenta),
                      vbox(std::move(rows)))
             | color(Color::Magenta);
    }) | CatchEvent([state, save](Event event) -> bool {
        // Close
        if (event == Event::Escape) {
            if (state->on_close) state->on_close();
            return true;
        }
        // Save
        if (event == Event::Character('\x13')) {  // Ctrl+S
            save();
            return true;
        }
        // Navigation
        if (event == Event::ArrowUp || event == Event::Character('k')) {
            state->focus_row = (state->focus_row - 1 + DialogState::kRowCount)
                               % DialogState::kRowCount;
            return true;
        }
        if (event == Event::ArrowDown || event == Event::Character('j')) {
            state->focus_row = (state->focus_row + 1) % DialogState::kRowCount;
            return true;
        }
        // Toggle
        if (event == Event::Character(' ') || event == Event::Return) {
            if (state->focus_row == 0) {
                state->master_enabled = !state->master_enabled;
            } else {
                auto& seg = detail::kSegments[state->focus_row - 1];
                bool cur = seg.get(state->working);
                seg.set(state->working, !cur);
            }
            state->mark_dirty();
            return true;
        }
        return false;
    });
}

// ============================================================
// Holder component
// ============================================================

namespace detail {

/// Wraps the statusline dialog component so the dialog system can
/// render it and route events to its CatchEvent handler.
/// ComponentBase::OnEvent is protected, so this subclass exposes a
/// public InjectEvent() surface (same pattern as DoctorDialogHolder).
struct StatuslineDialogHolder : public ComponentBase {
    Component inner;

    explicit StatuslineDialogHolder(Component c) : inner(std::move(c)) {
        ComponentBase::Add(inner);
    }

    Element Render() override { return inner->Render(); }

    bool InjectEvent(const Event& event) {
        return ComponentBase::OnEvent(event);
    }
};

} // namespace detail

// ============================================================
// Registration
// ============================================================

/// Register the StatuslineDialog renderer into a registry.
/// Call this once at app startup, alongside register_default_renderers().
inline void register_statusline_dialog_renderer(
    dsys::DialogRendererRegistry& registry)
{
    registry.register_dialog(
        dsys::DialogType::StatuslineDialog,
        /*renderer=*/
        [](dsys::DialogPayloadVariant& payload,
           const dsys::DialogRenderContext& ctx) -> Element {
            auto* p = std::get_if<dsys::StatuslineDialogPayload>(&payload);
            if (!p) return text("");

            // Lazily create the dialog component on first render.
            if (!p->component) {
                // repl_state is const void* but the underlying
                // ReplScreenState is mutable (passed as non-const ref to
                // RenderModalDialog).  const_cast is safe here; the
                // dialog updates status_bar_segments on save.
                auto* screen = const_cast<ReplScreenState*>(
                    static_cast<const ReplScreenState*>(ctx.repl_state));
                if (!screen) return text("(no screen state)");

                // Thread-local fallback ConfigManager, reloaded on each
                // dialog open (same pattern as the settings dialog).
                static thread_local ConfigManager fallback_cfg;
                (void)fallback_cfg.load(
                    loom::core::LoadOptions{.quiet = true});

                auto comp = MakeStatusLineDialog(
                    fallback_cfg, *screen, p->on_close);
                auto holder = std::make_shared<detail::StatuslineDialogHolder>(
                    std::move(comp));
                p->component = holder;
            }

            auto holder = std::static_pointer_cast<detail::StatuslineDialogHolder>(
                p->component);
            return holder ? holder->Render() : text("");
        },
        /*event_handler=*/
        [](dsys::DialogPayloadVariant& payload, const Event& event) -> bool {
            auto* p = std::get_if<dsys::StatuslineDialogPayload>(&payload);
            if (!p || !p->component) return false;

            auto holder = std::static_pointer_cast<detail::StatuslineDialogHolder>(
                p->component);
            if (!holder) return false;
            return holder->InjectEvent(event);
        }
    );
}

} // namespace loom::ui::screens::statusline_dialog
