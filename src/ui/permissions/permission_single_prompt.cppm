/// @file permission_single_prompt.cppm
/// @brief Single-request permission prompt dialog.
///
/// Covers ~80% of permission scenarios: one tool + one action needs approval.
///
/// Layout:
///   Header : tool icon + tool name + action kind + RiskPill (right)
///   Body   : one-line description ("BashTool wants to execute `rm -rf *.log`")
///          : optional destructive-warning banner (yellow)
///          : optional affected paths (with danger highlighting + depth tag)
///   Footer : compact decisions and sandbox toggle, followed by full scope
///            checkboxes and atomic key/action hints that wrap at narrow widths.
///
/// Keyboard shortcuts: y=Allow, n=Deny, a=Always allow, d=Always deny,
///                     s=Toggle sandbox, Esc=Abort, Enter=focused decision,
///                     PageUp/PageDown=scroll request details
///
/// Engine logic is 100% delegated to loom.security.permissions_engine.
/// This file only formats the UI and routes user input.
module;

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

#include <cstdint>

export module loom.ui.permissions.single_prompt;

import std;
import loom.ui.foundation.design_figures;
import loom.ui.foundation.theme_provider;

import loom.security.permissions_engine;
import loom.ui.permissions.components;

export namespace loom::ui::permissions::single_prompt {
using namespace ftxui;
namespace pc = loom::ui::permissions::components;
namespace eng = loom::utils::permissions;

// ============================================================
// Types
// ============================================================

/// Final decision returned by the prompt.
enum class Decision : std::uint8_t {
    AllowOnce,    // Allow only this invocation
    AlwaysAllow,  // Persist an allow rule for this scope/tool
    Deny,         // Deny this invocation only
    AlwaysDeny,   // Persist a deny rule
    Abort,        // Escape / abort the whole operation
};

/// Action category mirrors the shared component enum for convenience.
using ActionKind = pc::ActionKind;
using RiskLevel = pc::RiskLevel;

/// Tool-specific detail payload (rendered inside the body).
struct DetailBash {
    std::string command;
    std::optional<std::string> working_dir = std::nullopt;
    bool is_destructive = false;
    std::string destructive_reason = {};  // empty if is_destructive=false
};

struct DetailFileEdit {
    std::string file_path;
    std::string old_snippet;  // first 80 chars of old_string
    std::string new_snippet;  // first 80 chars of new_string
    bool creates_file = false;
};

struct DetailFileWrite {
    std::string file_path;
    std::size_t content_bytes = 0;
    std::size_t content_lines = 0;
    bool creates_file = false;
};

struct DetailFileRead {
    std::string file_path;
    std::optional<std::size_t> line_count;
};

struct DetailNetwork {
    std::string url_or_domain;
    bool is_upload = false;  // POST / PUT with a body
};

struct DetailMCP {
    std::string server_id;
    std::string tool_name;
    std::string description;
};

struct DetailSkill {
    std::string skill_name;
    std::string description;
};

struct DetailPlan {
    bool is_entry = true;  // true = Enter, false = Exit
};

struct DetailAskUser {
    std::string question;
    std::vector<std::string> options;
};

struct DetailGeneric {
    std::string description;  // free-form body
};

/// Computer-use (screen/mouse/keyboard) action. Rendered with an explicit
/// warning banner because approving it hands the model real input control.
struct DetailComputerUse {
    std::string action_label;  // e.g. "Click on screen"
    std::optional<std::string> target_app;
    std::optional<std::string> coordinates;
    std::optional<std::string> text_to_type;
    bool first_use_in_session = false;
};

using ToolDetail = std::variant<DetailBash, DetailFileEdit, DetailFileWrite, DetailFileRead,
                                DetailNetwork, DetailMCP, DetailSkill, DetailPlan, DetailAskUser,
                                DetailComputerUse, DetailGeneric>;

/// Props for creating a single-prompt dialog.
struct SinglePromptProps {
    std::string tool_name;    // e.g. "BashTool"
    ActionKind action_kind;   // Read / Write / Execute / Network ...
    RiskLevel risk_level;     // Risk classification (from engine)
    std::string description;  // One-liner, e.g. "BashTool wants to execute…"
    std::vector<std::string> affected_paths;
    std::optional<std::string> workspace_root;  // for "inside workspace" badges
    ToolDetail detail;                          // Tool-specific rendering payload
    std::string rule_match_explanation;         // from PermissionResult::explanation (may be "")
    // Callbacks
    std::function<void(Decision, bool sandbox_requested)> on_decide;
    std::function<void()> on_abort;  // optional
    // Initial state (optional)
    bool initial_sandbox_toggle = false;
};

// ============================================================
// Internal rendering helpers
// ============================================================

namespace detail {

/// Render the title bar: icon, tool name, action kind, risk pill.
[[nodiscard]] inline Element RenderTitleBar(const SinglePromptProps& p) {
    auto action_label = [&] {
        switch (p.action_kind) {
        case ActionKind::Read:
            return "read";
        case ActionKind::Write:
            return "write";
        case ActionKind::Execute:
            return "execute";
        case ActionKind::Network:
            return "network";
        case ActionKind::MCP:
            return "mcp";
        case ActionKind::Environment:
            return "env";
        case ActionKind::Plan:
            return "plan";
        case ActionKind::Other:
            return "use";
        }
        return "use";
    }();
    return hbox({
        text(std::string{pc::ToolIconGlyph(p.tool_name)}) | bold,
        text(" "),
        text(p.tool_name) | bold | color(pc::ActionColor(p.action_kind)),
        text(" wants to "),
        text(action_label),
        filler(),
        pc::RiskPill(p.risk_level),
    });
}

/// Render tool-specific detail content.
[[nodiscard]] inline Element RenderToolDetail(const ToolDetail& det) {
    return std::visit(
        [](const auto& d) -> Element {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, DetailBash>) {
                Elements els = {pc::CommandPreview(d.command, 80)};
                if (d.working_dir) {
                    els.push_back(hbox({
                        text(" in: "),
                        pc::PathLabel(*d.working_dir, 60),
                    }));
                }
                if (d.is_destructive) {
                    els.insert(els.begin(),
                               pc::DestructiveWarningBanner(
                                   d.destructive_reason.empty()
                                       ? "This command may permanently delete or overwrite files."
                                       : d.destructive_reason));
                }
                return vbox(els);
            } else if constexpr (std::is_same_v<T, DetailFileEdit>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                return vbox({
                    hbox({text("File: "), pc::PathLabel(d.file_path, 70)}),
                    text(""),
                    hbox({text("- ") | color(pal.danger),
                          text(pc::HeadEllipsisCommand(d.old_snippet, 78)) | color(pal.danger)}),
                    hbox({text("+ ") | color(pal.success),
                          text(pc::HeadEllipsisCommand(d.new_snippet, 78)) | color(pal.success)}),
                });
            } else if constexpr (std::is_same_v<T, DetailFileWrite>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                return vbox({
                    hbox({text("File: "), pc::PathLabel(d.file_path, 70)}),
                    hbox({text("Size: "),
                          text(std::format("{} bytes (~{} line{})", d.content_bytes,
                                           d.content_lines, d.content_lines == 1 ? "" : "s"))}),
                    (d.creates_file ? hbox({text("new file") | color(pal.warning)}) : text("")),
                });
            } else if constexpr (std::is_same_v<T, DetailFileRead>) {
                auto line_el = d.line_count ? text(std::format(" ({})", *d.line_count)) : text("");
                return hbox({
                    text("File: "),
                    pc::PathLabel(d.file_path, 70),
                    line_el,
                });
            } else if constexpr (std::is_same_v<T, DetailNetwork>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                auto tag =
                    d.is_upload
                        ? hbox({text(" UPLOAD ") | color(pal.inverse_text) | bgcolor(pal.danger) |
                                bold})
                        : hbox({text(" fetch ") | color(pal.inverse_text) | bgcolor(pal.info)});
                return hbox({tag, text("  "), pc::PathLabel(d.url_or_domain, 70)});
            } else if constexpr (std::is_same_v<T, DetailMCP>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                return vbox({
                    hbox({text("MCP server: "), text(d.server_id) | color(pal.primary)}),
                    hbox({text("Tool: "), text(d.tool_name) | bold}),
                    (d.description.empty() ? text("") : paragraph(d.description)),
                });
            } else if constexpr (std::is_same_v<T, DetailSkill>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                return vbox({
                    hbox({text("Skill: "), text(d.skill_name) | color(pal.primary) | bold}),
                    (d.description.empty() ? text("") : paragraph(d.description)),
                });
            } else if constexpr (std::is_same_v<T, DetailPlan>) {
                return text(d.is_entry ? "Enter plan mode (output-only, pauses execution)"
                                       : "Exit plan mode and resume execution");
            } else if constexpr (std::is_same_v<T, DetailAskUser>) {
                Elements els = {paragraph(d.question)};
                for (const auto& opt : d.options) {
                    els.push_back(hbox({text("  • "), text(opt)}));
                }
                return vbox(els);
            } else if constexpr (std::is_same_v<T, DetailComputerUse>) {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                Elements els;
                els.push_back(hbox({
                    text(" COMPUTER USE ") | color(pal.inverse_text) | bgcolor(pal.primary) | bold,
                    text("  "),
                    text(d.action_label) | bold,
                }));
                if (d.target_app) {
                    els.push_back(hbox({text("  App: "), text(*d.target_app)}));
                }
                if (d.coordinates) {
                    els.push_back(hbox({text("  Position: "), text(*d.coordinates)}));
                }
                if (d.text_to_type) {
                    els.push_back(hbox({
                        text("  Text: "),
                        text(pc::HeadEllipsisCommand(*d.text_to_type, 60)),
                    }));
                }
                if (d.first_use_in_session) {
                    els.push_back(hbox({
                        text("  First computer use in this session") | color(pal.warning),
                    }));
                }
                return vbox(els);
            } else if constexpr (std::is_same_v<T, DetailGeneric>) {
                return paragraph(d.description);
            }
        },
        det);
}

/// Render affected paths section.
[[nodiscard]] inline Element RenderAffectedPaths(const std::vector<std::string>& paths,
                                                 std::optional<std::string_view>) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    if (paths.empty()) return text("");
    Elements header = {
        hbox({text(" Affected paths") | bold, filler()}),
    };
    Elements list = pc::SummarisePathList(paths, 6, 60);
    // Prepend 2-space indent
    for (auto& el : list) {
        el = hbox({text("  "), el});
    }
    Elements all = header;
    all.insert(all.end(), list.begin(), list.end());
    return vbox(all) | color(pal.muted);
}

/// Render matched-rule explanation footnote.
[[nodiscard]] inline Element RenderRuleExplanation(std::string_view explanation) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    if (explanation.empty()) return text("");
    return hbox({
        text(" rule: "),
        text(std::string{explanation}) | color(pal.info),
    });
}

}  // namespace detail

// ============================================================
// Full element renderer
// ============================================================

/// Internal state shared between Renderer and CatchEvent.
struct PromptState {
    SinglePromptProps props;
    bool always_deny_checkbox = false;
    bool always_allow_checkbox = false;
    bool sandbox_toggle = false;
    int focused_button = 0;  // 0=Allow once  1=Deny  2=Always allow  3=Always deny
    int detail_line = 0;
    Element detail_content;  // Last laid-out document; its requirement is not stencil-clipped.
    Box detail_viewport_box;
    // One-shot guard: contract fires EXACTLY ONE terminal callback per prompt.
    // Priority 1) on_abort (if present), 2) else on_decide(...). NEVER both.
    bool callback_fired = false;
};

/// Scroll only the request details; decision focus and callbacks stay unchanged.
inline bool HandleDetailScroll(PromptState& state, const Event& event) {
    if (event != Event::PageUp && event != Event::PageDown) return false;
    const int last =
        state.detail_content ? std::max(0, state.detail_content->requirement().min_y - 1) : 0;
    const int step = std::max(1, state.detail_viewport_box.y_max - state.detail_viewport_box.y_min);
    state.detail_line = std::clamp(state.detail_line, 0, last);
    state.detail_line = event == Event::PageDown ? std::min(last, state.detail_line + step)
                                                 : std::max(0, state.detail_line - step);
    return true;
}

/// Render the complete single-prompt dialog body.
[[nodiscard]] inline Element RenderSinglePrompt(std::shared_ptr<PromptState> st) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    const auto& p = st->props;

    auto title_bar = detail::RenderTitleBar(p);
    auto detail_el = detail::RenderToolDetail(p.detail);
    auto desc_el = paragraph(p.description) | color(pal.text);
    auto paths_el = detail::RenderAffectedPaths(
        p.affected_paths,
        p.workspace_root ? std::optional<std::string_view>{*p.workspace_root} : std::nullopt);
    auto rule_el = detail::RenderRuleExplanation(p.rule_match_explanation);

    // Keep each control and each key/action pair atomic. Native flexbox wraps
    // between them rather than shrinking a scope label inside a rigid column.
    const auto flow = FlexboxConfig().SetGap(2, 0);
    auto decision_button = [&](std::string label, Color ink, int index) {
        auto el = text(std::move(label)) | color(ink);
        if (st->focused_button == index) el = el | inverted | bold;
        return el;
    };
    auto scope_toggle = [&](bool checked, std::string label, int index) {
        auto el = hbox({pc::CheckboxBox(checked), text(" " + label)});
        if (st->focused_button == index) el = el | inverted | bold;
        return el;
    };
    Elements hints;
    for (const auto& hint : std::vector<pc::KeyHint>{{"y", "Allow"},
                                                     {"n", "Deny"},
                                                     {"a", "Always allow"},
                                                     {"d", "Always deny"},
                                                     {"s", "Toggle sandbox"},
                                                     {"Esc", "Cancel"},
                                                     {"PgUp/PgDn", "Details"}}) {
        hints.push_back(pc::KeyboardHintRow({hint}));
    }
    auto footer = vbox({
        pc::ThinDivider(),
        flexbox(
            {
                decision_button("× Deny", pal.danger, 1),
                decision_button("✓ Allow once", pal.success, 0),
                hbox({text("Sandbox "),
                      st->sandbox_toggle ? text("[ON]") | color(pal.info) | bold : text("[OFF]")}),
            },
            flow),
        flexbox(
            {
                scope_toggle(st->always_deny_checkbox, "Always deny this tool", 3),
                scope_toggle(st->always_allow_checkbox, "Always allow this scope", 2),
            },
            flow),
        flexbox(std::move(hints), flow),
    });

    // The tool target precedes supplementary prose. Under the overlay's
    // height budget, shrink only this detail region, never the decision footer.
    Elements details{detail_el, desc_el};
    if (!p.affected_paths.empty()) details.push_back(paths_el);
    if (!p.rule_match_explanation.empty()) details.push_back(rule_el);
    st->detail_content = vbox(std::move(details));
    auto body = vbox({
                    title_bar,
                    st->detail_content | focusPosition(0, st->detail_line) | vscroll_indicator |
                        yframe | reflect(st->detail_viewport_box) | yflex_shrink,
                    footer,
                }) |
                color(pal.text) | xflex;

    auto border_col = [&] {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        switch (p.risk_level) {
        case RiskLevel::Low:
            return pal.info;
        case RiskLevel::Medium:
            return pal.warning;
        case RiskLevel::High:
            return pal.danger;
        case RiskLevel::Critical:
            return pal.danger;
        }
        return pal.muted;
    }();

    return window(text((" " + std::string{loom::ui::design::figures::kQuestionMarkPrefix} +
                        " Permission ")) |
                      bold | color(pal.warning),
                  body | xflex) |
           clear_under | color(border_col) | size(WIDTH, LESS_THAN, 90);
}

// ============================================================
// Public factory: make the interactive component
// ============================================================

/// Construct a full interactive single-permission-prompt component.
[[nodiscard]] inline Component MakeSinglePromptDialog(SinglePromptProps props) {
    constexpr int kButtonCount = 4;
    auto state = std::make_shared<PromptState>();
    state->props = std::move(props);
    state->sandbox_toggle = state->props.initial_sandbox_toggle;

    auto emit = [state](Decision d) {
        if (state->callback_fired) return;
        state->callback_fired = true;
        // contract: one-shot. For Abort, prefer on_abort when present; else
        // fall back to on_decide(Abort). For all other decisions, use on_decide.
        if (d == Decision::Abort && state->props.on_abort) {
            state->props.on_abort();
            return;
        }
        if (state->props.on_decide) {
            state->props.on_decide(d, state->sandbox_toggle);
        }
    };

    return Renderer([state] { return RenderSinglePrompt(state); }) |
           CatchEvent([state, emit](Event event) -> bool {
               if (HandleDetailScroll(*state, event)) return true;
               // ----- Movement between buttons -----
               if (event == Event::ArrowLeft || event == Event::Character('h')) {
                   state->focused_button =
                       (state->focused_button - 1 + kButtonCount) % kButtonCount;
                   return true;
               }
               if (event == Event::ArrowRight || event == Event::Character('l')) {
                   state->focused_button = (state->focused_button + 1) % kButtonCount;
                   return true;
               }
               if (event == Event::Tab) {
                   state->focused_button = (state->focused_button + 1) % kButtonCount;
                   return true;
               }
               if (event == Event::TabReverse) {
                   state->focused_button =
                       (state->focused_button - 1 + kButtonCount) % kButtonCount;
                   return true;
               }

               // ----- Direct shortcuts -----
               if (event == Event::Character('y')) {
                   emit(state->always_allow_checkbox ? Decision::AlwaysAllow : Decision::AllowOnce);
                   return true;
               }
               if (event == Event::Character('n')) {
                   emit(state->always_deny_checkbox ? Decision::AlwaysDeny : Decision::Deny);
                   return true;
               }
               if (event == Event::Character('a')) {
                   emit(Decision::AlwaysAllow);
                   return true;
               }
               if (event == Event::Character('d')) {
                   emit(Decision::AlwaysDeny);
                   return true;
               }
               if (event == Event::Character('s')) {
                   state->sandbox_toggle = !state->sandbox_toggle;
                   return true;
               }

               // ----- Checkbox toggles -----
               if (event == Event::Character('1')) {
                   state->always_deny_checkbox = !state->always_deny_checkbox;
                   return true;
               }
               if (event == Event::Character('2')) {
                   state->always_allow_checkbox = !state->always_allow_checkbox;
                   return true;
               }

               // ----- Enter activates focused button -----
               if (event == Event::Return) {
                   switch (state->focused_button) {
                   case 0:  // Allow once
                       emit(state->always_allow_checkbox ? Decision::AlwaysAllow
                                                         : Decision::AllowOnce);
                       return true;
                   case 1:  // Deny
                       emit(state->always_deny_checkbox ? Decision::AlwaysDeny : Decision::Deny);
                       return true;
                   case 2:  // Always allow
                       emit(Decision::AlwaysAllow);
                       return true;
                   case 3:  // Always deny
                       emit(Decision::AlwaysDeny);
                       return true;
                   }
                   return true;
               }

               // ----- Escape: abort with one-shot priority ----
               // Priority: (1) on_abort if set, (2) else on_decide(Abort).
               // Never both — emit() enforces the oneshot via callback_fired.
               if (event == Event::Escape) {
                   emit(Decision::Abort);
                   return true;
               }

               // Space toggles the closest checkbox
               if (event == Event::Character(' ')) {
                   if (state->focused_button == 1 || state->focused_button == 3)
                       state->always_deny_checkbox = !state->always_deny_checkbox;
                   else
                       state->always_allow_checkbox = !state->always_allow_checkbox;
                   return true;
               }

               return false;
           });
}

// ============================================================
// Convenience overloads (most common variants)
// ============================================================

/// Build a bash-command permission prompt (the most common case).
[[nodiscard]] inline Component
MakeBashPrompt(std::string command, std::optional<std::string> working_dir, RiskLevel risk,
               bool is_destructive, std::string destructive_reason,
               std::vector<std::string> affected_paths,
               std::function<void(Decision, bool sandbox)> on_decide) {
    SinglePromptProps p;
    p.tool_name = "BashTool";
    p.action_kind = ActionKind::Execute;
    p.risk_level = risk;
    p.description = "BashTool wants to execute this command.";
    p.affected_paths = std::move(affected_paths);
    p.detail = DetailBash{
        .command = std::move(command),
        .working_dir = std::move(working_dir),
        .is_destructive = is_destructive,
        .destructive_reason = std::move(destructive_reason),
    };
    p.on_decide = std::move(on_decide);
    return MakeSinglePromptDialog(std::move(p));
}

/// Build a file-edit permission prompt.
[[nodiscard]] inline Component
MakeFileEditPrompt(std::string file_path, std::string old_snippet, std::string new_snippet,
                   bool creates_file, RiskLevel risk,
                   std::optional<std::string_view> workspace_root,
                   std::function<void(Decision, bool sandbox)> on_decide) {
    SinglePromptProps p;
    p.tool_name = "FileEditTool";
    p.action_kind = ActionKind::Write;
    p.risk_level = risk;
    p.description = creates_file ? "FileEditTool wants to create and edit a file."
                                 : "FileEditTool wants to modify a file.";
    p.affected_paths = {std::string{file_path}};
    if (workspace_root) p.workspace_root = std::string{*workspace_root};
    p.detail = DetailFileEdit{
        .file_path = std::move(file_path),
        .old_snippet = std::move(old_snippet),
        .new_snippet = std::move(new_snippet),
        .creates_file = creates_file,
    };
    p.on_decide = std::move(on_decide);
    return MakeSinglePromptDialog(std::move(p));
}

/// Build a network / web-fetch permission prompt.
[[nodiscard]] inline Component
MakeNetworkPrompt(std::string url_or_domain, bool is_upload, RiskLevel risk,
                  std::function<void(Decision, bool sandbox)> on_decide) {
    SinglePromptProps p;
    p.tool_name = is_upload ? "WebFetchTool (POST)" : "WebFetchTool";
    p.action_kind = ActionKind::Network;
    p.risk_level = risk;
    p.description = is_upload ? "WebFetchTool wants to send data over the network."
                              : "WebFetchTool wants to fetch content from the network.";
    p.detail = DetailNetwork{
        .url_or_domain = std::move(url_or_domain),
        .is_upload = is_upload,
    };
    p.on_decide = std::move(on_decide);
    return MakeSinglePromptDialog(std::move(p));
}

}  // namespace loom::ui::permissions::single_prompt
