/// @file agent_wizard.cppm
/// @brief Agent creation/edit wizard. 4 agent-specific steps built on top of
/// the UI11 loom.ui.dialogs.wizard_dialog framework.
///
/// Replaces the old 6-step skeleton (same filename) and consolidates the
/// agent editor, color picker, model selector, and tool selector into a
/// single wizard flow.
///
/// 4 Steps (UI11 WizardComponent + 4 WizardStep entries):
///   Step 1: Basic         — name (required), short description,
///                           role tag picker (CustomSelect multi),
///                           avatar color
///   Step 2: Tools         — left: available tools (checkbox multi-select)
///                           right: selected tools (reorderable)
///                           + allowed/denied path scope editor
///   Step 3: Model+Prompt  — model dropdown (CustomSelect single)
///                           + temperature slider
///                           + system prompt (multiline code-highlighted)
///   Step 4: Perm+Summary  — default permission mode (Ask/Allow/Deny),
///                           confirm checkbox, full agent-definition summary
///
/// On wizard completion: fires on_save with the aggregated wizard state.
///
/// Reuses:
///   - loom.ui.foundation.feature_dialog_protocol (neutral wizard request +
///     ViewKind factory registry — RFC 0002 F2 row 6 inversion; this module
///     no longer imports loom.ui.dialogs.wizard_dialog)
///   - loom.ui.widgets.custom_select  (MakeSingleSelect / MakeMultiSelect for roles,
///                           tools, model)
///   - loom.ui.features.agents.agent_shared_widgets (AgentAvatar, RoleTags, RunStats)
///   - loom.tools.agent_color_manager (8-color palette, explicit set)
module;

#include <cctype>
#include <cstdint>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <cstddef>

export module loom.ui.features.agents.agent_wizard;

import loom.ui.foundation.theme_provider;

import std;

import loom.ui.foundation.design_figures;

import loom.ui.foundation.feature_dialog_protocol;
import loom.ui.widgets.custom_select;
import loom.ui.features.agents.agent_shared_widgets;
import loom.ui.features.agents.agent_cards;
import loom.tools.agent_color_manager;
import loom.teams.swarm.backends;

export namespace loom::ui::agents::wizard {
using namespace ftxui;

namespace fdp = loom::ui::feature_dialog_protocol;

using loom::ui::custom_select::MakeMultiSelect;
using loom::ui::custom_select::MakeSingleSelect;
using loom::ui::custom_select::MakeCustomSelect;
using loom::ui::custom_select::SelectOption;
using loom::ui::custom_select::SelectMode;
using loom::ui::custom_select::CustomSelectOptions;
using loom::ui::custom_select::CustomSelectHandle;

using cards::AgentCardData;
using shared::AgentAvatar;
using shared::AvatarOptions;
using shared::RoleTags;
using shared::status_color;
using shared::agent_color_to_ftxui;
using loom::utils::swarm_backends::AgentColor;
using loom::tools::agent_color_manager::parse_color_name;
using loom::tools::agent_color_manager::assign_cycle_color;

// ============================================================
// Wizard State — the mutable draft gathered across all 4 steps.
// ============================================================

/// Permission default mode (Step 4).
enum class PermissionMode : std::uint8_t {
    Ask,     // Always ask the user before invoking a tool.
    Allow,   // Auto-allow unless explicitly denied.
    Deny,    // Auto-deny unless explicitly allowed.
};

[[nodiscard]] inline std::string_view permission_label(PermissionMode m) {
    switch (m) {
        case PermissionMode::Ask:   return "Ask (default)";
        case PermissionMode::Allow: return "Auto-allow";
        case PermissionMode::Deny:  return "Auto-deny";
    }
    return "?";
}

/// Aggregated wizard draft. Step components write into this struct via
/// shared_ptr so that the final summary can render everything.
struct WizardDraft {
    // Step 1
    std::string name;
    std::string description;
    std::vector<std::string> role_tags;
    std::optional<AgentColor> avatar_color;

    // Step 2
    std::vector<std::string> available_tools;    // all known tools
    std::vector<std::string> selected_tools;     // user picks
    std::vector<std::string> path_allowlist;     // glob patterns
    std::vector<std::string> path_denylist;      // glob patterns

    // Step 3
    std::string model;  // no default — user configures their model
    double temperature = 1.0;   // 0.0 .. 2.0
    std::string system_prompt;

    // Step 4
    PermissionMode default_permission = PermissionMode::Ask;
    bool confirmed = false;

    // Editing mode: if true, the wizard is editing an existing agent.
    bool is_edit_mode = false;
    std::string editing_agent_id;
};

// ============================================================
// Canonical tool list (mirrors ToolSelector's all-tools registry).
// Kept here so Step 2 can populate Available → Selected without pulling in
// the full tools/ registry header.
// ============================================================

inline std::vector<SelectOption> canonical_tool_options() {
    return {
        {.label = "Bash",        .value = "bash",        .description = "Execute shell commands", .group = "System", .icon = std::string(loom::ui::design::figures::kBashGlyph)},
        {.label = "FileRead",    .value = "file_read",   .description = "Read files from disk",   .group = "Files",  .icon = std::string(loom::ui::design::figures::kSquareSmall)},
        {.label = "FileWrite",   .value = "file_write",  .description = "Write/create files",     .group = "Files",  .icon = std::string(loom::ui::design::figures::kPointerSmall)},
        {.label = "FileEdit",    .value = "file_edit",   .description = "Edit existing files",    .group = "Files",  .icon = std::string(loom::ui::design::figures::kPointerSmall)},
        {.label = "Glob",        .value = "glob",        .description = "Pattern file search",    .group = "Search", .icon = std::string(loom::ui::design::figures::kInfo)},
        {.label = "Grep",        .value = "grep",        .description = "Regex content search",   .group = "Search", .icon = std::string(loom::ui::design::figures::kInfo)},
        {.label = "WebFetch",    .value = "web_fetch",   .description = "HTTP GET / scrape",      .group = "Web",    .icon = std::string(loom::ui::design::figures::kCircleDouble)},
        {.label = "WebSearch",   .value = "web_search",  .description = "Web search engine",      .group = "Web",    .icon = std::string(loom::ui::design::figures::kInfo)},
        {.label = "MCPTool",     .value = "mcp",         .description = "MCP server tools",       .group = "MCP",    .icon = std::string(loom::ui::design::figures::kCircleDouble)},
        {.label = "Agent",       .value = "agent",       .description = "Spawn sub-agents",       .group = "Agent",  .icon = std::string(loom::ui::design::figures::kDiamond)},
        {.label = "Skill",       .value = "skill",       .description = "Run skill workflows",    .group = "Agent",  .icon = std::string(loom::ui::design::figures::kLightningBolt)},
        {.label = "TaskCreate",  .value = "task_create", .description = "Create background task", .group = "Agent",  .icon = std::string(loom::ui::design::figures::kSquare)},
        {.label = "TeamCreate",  .value = "team_create", .description = "Spawn a team",           .group = "Agent",  .icon = std::string(loom::ui::design::figures::kDiamond)},
    };
}

inline std::vector<SelectOption> canonical_model_options() {
    // No built-in model presets — the harness is provider-agnostic.
    // Users type their model ID directly in the wizard.
    return {};
}

inline std::vector<SelectOption> canonical_role_options() {
    return {
        {.label = "Coder",      .value = "coder",      .description = "Write, edit, refactor code", .group = "Role", .icon = ""},
        {.label = "Reviewer",   .value = "reviewer",   .description = "Review PRs and diffs", .group = "Role", .icon = ""},
        {.label = "Researcher", .value = "researcher", .description = "Web research / fact-finding", .group = "Role", .icon = ""},
        {.label = "Planner",    .value = "planner",    .description = "Break tasks into plans", .group = "Role", .icon = ""},
        {.label = "Tester",     .value = "tester",     .description = "Write and run tests", .group = "Role", .icon = ""},
        {.label = "DevOps",     .value = "devops",     .description = "CI/CD, deployments, infra", .group = "Role", .icon = ""},
        {.label = "Writer",     .value = "writer",     .description = "Docs, changelogs, text", .group = "Role", .icon = ""},
        {.label = "Analyst",    .value = "analyst",    .description = "Data analysis, metrics", .group = "Role", .icon = ""},
    };
}

// ============================================================
// Step 1: Basic (name, description, role picker, avatar color)
// ============================================================

/// Build Step 1 content component.
[[nodiscard]] inline Component StepBasic(std::shared_ptr<WizardDraft> draft) {
    // Role multi-select handle.
    auto roles_opts = canonical_role_options();
    // Pre-select based on draft->role_tags.
    std::vector<std::string> defaults = draft->role_tags;
    CustomSelectOptions roles_cfg;
    roles_cfg.options = std::move(roles_opts);
    roles_cfg.mode = SelectMode::Multi;
    roles_cfg.default_values = defaults;
    roles_cfg.visible_count = 8;
    roles_cfg.enable_search = true;
    auto [roles_comp, roles_handle] = MakeCustomSelect(std::move(roles_cfg));

    // Track current color index (palette + 1 "auto" slot).
    static constexpr AgentColor kPalette[] = {
        AgentColor::Red, AgentColor::Blue, AgentColor::Green,  AgentColor::Yellow,
        AgentColor::Purple, AgentColor::Orange, AgentColor::Pink, AgentColor::Cyan,
    };
    constexpr int kColorCount = 8;
    auto color_idx = std::make_shared<int>(-1);   // -1 = auto
    if (draft->avatar_color) {
        for (int i = 0; i < kColorCount; ++i) {
            if (kPalette[i] == *draft->avatar_color) { *color_idx = i; break; }
        }
    }

    // Renderer for the whole step.
    auto name_comp = Input(&draft->name, "agent name (required)");
    auto desc_comp = Input(&draft->description, "short description — what this agent does");

    return Renderer(
        Container::Vertical({
            name_comp,
            desc_comp,
            roles_comp,
        }),
        [draft, roles_handle, color_idx, roles_comp] {
            const auto& pal = *loom::ui::design::theme::current_theme().palette;
            // Persist multi-select back into draft on every render.
            draft->role_tags = roles_handle->SelectedValues();

            // Persist color.
            if (*color_idx < 0) draft->avatar_color.reset();
            else                draft->avatar_color = kPalette[*color_idx];

            // --- Name row ---
            const auto field_background = loom::ui::design::theme::current_theme().variant
                    == loom::ui::design::theme::ThemeVariant::Monochrome
                ? pal.message_actions_background : pal.surface_tint_2;
            auto name_row = hbox({
                text(" Name:        ") | color(pal.info) | bold,
                hbox({
                    text(" " + draft->name + " ")
                        | bgcolor(draft->name.empty()
                                      ? pal.danger_tint_bg
                                      : field_background),
                }) | borderLight
                  | color(draft->name.empty() ? pal.danger : pal.info) | flex,
                draft->name.empty() ? text(" required!") | color(pal.danger) : text(""),
            });

            // --- Description row ---
            auto desc_row = hbox({
                text(" Description: ") | color(pal.info) | bold,
                text(" " + (draft->description.empty()
                                ? std::string("(press Tab then type)")
                                : draft->description) + " ")
                    | color(pal.muted) | borderLight | flex,
            });

            // --- Role tags preview ---
            auto tags_preview = hbox({
                text(" Roles:       ") | color(pal.info) | bold,
                RoleTags(draft->role_tags, 6),
                draft->role_tags.empty() ? text(" (select below)") | color(pal.muted) : text(""),
            });

            // --- Color picker (ASCII palette) ---
            Elements swatches;
            swatches.push_back(text(" Color:       ") | color(pal.info) | bold);
            // Auto swatch.
            {
                std::string glyph = (*color_idx < 0) ? "███" : "░░░";
                Color c = pal.muted;
                swatches.push_back(
                    hbox({
                        text("[") | dim,
                        text(glyph) | color(c),
                        text("]auto") | color(pal.muted),
                    })
                );
            }
            swatches.push_back(text("  "));
            for (int i = 0; i < kColorCount; ++i) {
                Color c = agent_color_to_ftxui(kPalette[i]);
                std::string glyph = (*color_idx == i) ? "███" : "▓▓▓";
                swatches.push_back(
                    hbox({
                        text(glyph) | color(c),
                        text(" "),
                    })
                );
            }
            auto color_row = hbox(std::move(swatches));

            // --- Avatar live preview ---
            AvatarOptions av;
            av.name = draft->name.empty() ? "?" : draft->name;
            av.agent_type = draft->name.empty() ? "custom" : draft->name;
            av.force_general = false;
            av.size_cells = 4;
            // Override color if user picked one.
            if (draft->avatar_color) {
                loom::tools::agent_color_manager::set_agent_color(
                    av.agent_type, *draft->avatar_color);
            }
            auto preview = hbox({
                text(" Preview:     ") | color(pal.info) | bold,
                AgentAvatar(av),
                text("  " + (draft->name.empty() ? std::string("NewAgent") : draft->name)),
            });

            return vbox({
                text(" Step 1 · Basic info") | bold | color(pal.info),
                separator() | color(pal.info),
                text(""),
                name_row,
                text(""),
                desc_row,
                text(""),
                tags_preview,
                text(""),
                color_row,
                text("  hint: press [←/→] to switch palette slot") | color(pal.muted),
                text(""),
                preview,
                separator() | dim,
                text(" Role picker (multi-select — Space toggles, / to search):")
                    | bold | color(pal.primary),
                roles_comp->Render() | borderLight | flex,
            }) | color(pal.text);
        }) | CatchEvent([color_idx](Event event) -> bool {
            // Color swatch navigation via Alt+Left/Right or just arrow keys
            // when focus is not in the text inputs (best-effort: handle h/l).
            if (event == Event::Character('h')) {
                *color_idx = (*color_idx - 1 + 9) % 9 - 1;
                if (*color_idx < -1) *color_idx = 7;
                return true;
            }
            if (event == Event::Character('l')) {
                *color_idx = (*color_idx + 1) % 9 - 1;
                if (*color_idx < -1) *color_idx = -1;
                return true;
            }
            return false;
        });
}

// ============================================================
// Step 2: Tools (Available → Selected + path scope editor)
// ============================================================

/// Build Step 2 content.
[[nodiscard]] inline Component StepTools(std::shared_ptr<WizardDraft> draft) {
    // Make sure available_tools is populated.
    if (draft->available_tools.empty()) {
        auto opts = canonical_tool_options();
        draft->available_tools.reserve(opts.size());
        for (auto& o : opts) draft->available_tools.push_back(o.value);
    }

    // Selected-tools multi-select with live write-back.
    auto tool_opts = canonical_tool_options();
    CustomSelectOptions tc;
    tc.options = std::move(tool_opts);
    tc.mode = SelectMode::Multi;
    tc.default_values = draft->selected_tools;
    tc.visible_count = 10;
    tc.enable_search = true;
    auto [tools_comp, tools_handle] = MakeCustomSelect(std::move(tc));

    // Path allow/deny lists are just textarea-like strings the user edits
    // via simple Input components (one line per pattern).
    std::string allow_joined;
    for (auto& p : draft->path_allowlist) { allow_joined += p; allow_joined += '\n'; }
    if (!allow_joined.empty()) allow_joined.pop_back();
    std::string deny_joined;
    for (auto& p : draft->path_denylist) { deny_joined += p; deny_joined += '\n'; }
    if (!deny_joined.empty()) deny_joined.pop_back();

    auto allow_store = std::make_shared<std::string>(std::move(allow_joined));
    auto deny_store  = std::make_shared<std::string>(std::move(deny_joined));
    auto allow_input = Input(allow_store.get(), "src/**  tests/**  (one per line)");
    auto deny_input  = Input(deny_store.get(),  "**/node_modules/**  **/.git/**");

    // Helpers to split/join.
    auto split_lines = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> out;
        std::istringstream iss(s);
        std::string ln;
        while (std::getline(iss, ln)) {
            // Trim whitespace.
            size_t a = ln.find_first_not_of(" \t");
            size_t b = ln.find_last_not_of(" \t");
            if (a == std::string::npos) continue;
            out.push_back(ln.substr(a, b - a + 1));
        }
        return out;
    };

    return Renderer(
        Container::Vertical({
            tools_comp,
            allow_input,
            deny_input,
        }),
        [draft, tools_handle, allow_store, deny_store, split_lines, tools_comp, allow_input, deny_input] {
            const auto& pal = *loom::ui::design::theme::current_theme().palette;
            // Sync selection back to draft.
            draft->selected_tools = tools_handle->SelectedValues();
            draft->path_allowlist = split_lines(*allow_store);
            draft->path_denylist  = split_lines(*deny_store);

            // --- Selected summary chip row ---
            Elements sel_parts;
            for (size_t i = 0; i < draft->selected_tools.size(); ++i) {
                sel_parts.push_back(
                    hbox({
                        text(" " + draft->selected_tools[i] + " ")
                            | color(pal.success) | borderLight,
                    })
                );
                sel_parts.push_back(text(" "));
            }
            if (sel_parts.empty()) sel_parts.push_back(text("(none selected)") | color(pal.muted));

            // --- Path summary ---
            auto path_col = [](const std::string& title, Color c,
                               const std::vector<std::string>& rows) -> Element {
                const auto& pal = *loom::ui::design::theme::current_theme().palette;
                Elements els = {
                    hbox({text(" " + title + " ") | bold | color(c)}),
                };
                if (rows.empty()) els.push_back(text("  (empty)") | color(pal.muted));
                else {
                    int shown = std::min(4, static_cast<int>(rows.size()));
                    for (int i = 0; i < shown; ++i)
                        els.push_back(text("  " + rows[i]) | color(c));
                    int rest = static_cast<int>(rows.size()) - shown;
                    if (rest > 0)
                        els.push_back(text(std::format("  ... +{} more", rest)) | color(pal.muted));
                }
                return vbox(std::move(els)) | borderLight | color(c);
            };

            return vbox({
                text(" Step 2 · Tools & path scope") | bold | color(pal.primary),
                separator() | color(pal.primary),
                text(""),
                hbox({
                    text(" Selected (") | bold,
                    text(std::format("{}", draft->selected_tools.size()))
                        | bold | color(pal.success),
                    text("):  ") | bold,
                    hbox(std::move(sel_parts)) | flex,
                }),
                text(""),
                text(" Available tools (Space toggles, / filters):")
                    | bold | color(pal.primary),
                tools_comp->Render() | borderLight | size(HEIGHT, LESS_THAN, 12) | flex,
                separator() | dim,
                text(" Path scope (one glob per line):") | bold | color(pal.info),
                text(""),
                hbox({
                    vbox({
                        text(" Allow list:") | color(pal.success),
                        allow_input->Render() | size(HEIGHT, LESS_THAN, 5) | flex | borderLight,
                    }) | flex,
                    text("  "),
                    vbox({
                        text(" Deny list:") | color(pal.danger),
                        deny_input->Render() | size(HEIGHT, LESS_THAN, 5) | flex | borderLight,
                    }) | flex,
                }),
                text(""),
                hbox({
                    path_col("Allow",  pal.success, draft->path_allowlist) | flex,
                    text(" "),
                    path_col("Deny",   pal.danger,   draft->path_denylist)  | flex,
                }),
            }) | color(pal.text);
        });
}

// ============================================================
// Step 3: Model + Prompt (dropdown + temperature slider + system prompt)
// ============================================================

/// Build Step 3 content.
[[nodiscard]] inline Component StepModelPrompt(std::shared_ptr<WizardDraft> draft) {
    // Model single-select.
    auto model_opts = canonical_model_options();
    CustomSelectOptions mc;
    mc.options = std::move(model_opts);
    mc.mode = SelectMode::Single;
    mc.default_value = draft->model;
    mc.visible_count = 6;
    mc.enable_search = false;
    auto [model_comp, model_handle] = MakeCustomSelect(std::move(mc));

    // Temperature slider: rendered as a manual gauge; + / - keys adjust.
    auto temp = std::make_shared<double>(draft->temperature);

    // System prompt input.
    auto prompt_store = std::make_shared<std::string>(draft->system_prompt);
    auto prompt_input = Input(prompt_store.get(),
        "# System prompt — instructions this agent always follows\n"
        "# Example:\n"
        "You are a senior C++ engineer. Prefer range-based for loops.\n"
        "Always run clang-format before committing.");

    return Renderer(
        Container::Vertical({
            model_comp,
            prompt_input,
        }),
        [draft, model_handle, temp, prompt_store, model_comp, prompt_input] {
            const auto& pal = *loom::ui::design::theme::current_theme().palette;
            // Sync back.
            auto mv = model_handle->SelectedValues();
            if (!mv.empty()) draft->model = mv[0];
            draft->temperature = std::clamp(*temp, 0.0, 2.0);
            draft->system_prompt = *prompt_store;

            // Model picker row.
            auto current = draft->model;
            auto model_row = vbox({
                text(" Model: ") | bold | color(pal.warning),
                text(""),
                model_comp->Render() | borderLight | size(HEIGHT, LESS_THAN, 8) | flex,
                text(""),
                hbox({
                    text("  Selected: ") | color(pal.muted),
                    text(current) | color(pal.warning) | bold,
                }),
            });

            // Temperature gauge.
            int bar_w = 30;
            int filled = static_cast<int>((draft->temperature / 2.0) * bar_w);
            const std::string_view kFill = "█";
            const std::string_view kEmpty = "░";
            std::string bar;
            bar.reserve(bar_w * 3);
            for (int i = 0; i < filled; ++i) bar.append(kFill);
            for (int i = 0; i < bar_w - filled; ++i) bar.append(kEmpty);
            // Color transition: blue (cold) -> green -> red (hot).
            Color tcolor = pal.info;
            if (draft->temperature > 0.5) tcolor = pal.success;
            if (draft->temperature > 1.2) tcolor = pal.warning;
            if (draft->temperature > 1.7) tcolor = pal.danger;

            auto temp_row = vbox({
                hbox({
                    text(" Temperature: ") | bold | color(tcolor),
                    text(bar) | color(tcolor),
                    text(std::format(" {:.2f} ", draft->temperature)) | bold,
                }),
                text("  hint: [+] / [-] to adjust (0.0 = deterministic, 2.0 = wild)") | color(pal.muted),
            });

            // System prompt block (code-style).
            auto prompt_view = vbox({
                text(" System prompt:") | bold | color(pal.info),
                text(""),
                hbox({
                    text(" │") | dim | color(pal.info),
                    prompt_input->Render()
                        | bgcolor(pal.surface_tint_3)
                        | size(HEIGHT, LESS_THAN, 10) | flex | borderLight,
                }),
                text(std::format("  {} chars", draft->system_prompt.size())) | color(pal.muted),
            });

            return vbox({
                text(" Step 3 · Model + system prompt") | bold | color(pal.warning),
                separator() | color(pal.warning),
                text(""),
                model_row | borderLight,
                text(""),
                temp_row,
                text(""),
                prompt_view,
            }) | color(pal.text);
        }) | CatchEvent([temp](Event event) -> bool {
            if (event == Event::Character('+') || event == Event::Character('=')) {
                *temp = std::min(2.0, *temp + 0.1);
                return true;
            }
            if (event == Event::Character('-') || event == Event::Character('_')) {
                *temp = std::max(0.0, *temp - 0.1);
                return true;
            }
            if (event == Event::Character('0')) { *temp = 0.0; return true; }
            if (event == Event::Character('1')) { *temp = 1.0; return true; }
            if (event == Event::Character('2')) { *temp = 2.0; return true; }
            return false;
        });
}

// ============================================================
// Step 4: Permissions + Summary
// ============================================================

/// Build Step 4 content.
[[nodiscard]] inline Component StepPermissions(std::shared_ptr<WizardDraft> draft) {
    auto perm = std::make_shared<int>(static_cast<int>(draft->default_permission));
    auto confirmed = std::make_shared<bool>(draft->confirmed);

    return Renderer([draft, perm, confirmed] {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        draft->default_permission = static_cast<PermissionMode>(
            std::clamp(*perm, 0, 2));
        draft->confirmed = *confirmed;

        // --- Permission radio row ---
        auto pill = [&](PermissionMode m, Color c) {
            bool on = draft->default_permission == m;
            std::string mark = on ? std::string(loom::ui::design::figures::kBullet) + " " : std::string(loom::ui::design::figures::kEffortLow) + " ";
            auto t = hbox({
                text(mark) | color(c),
                text(std::string(permission_label(m))) | color(on ? c : pal.muted)
                    | (on ? bold : nothing),
            });
            if (on) t = t | borderLight | color(c);
            return t;
        };

        auto perm_row = vbox({
            text(" Default permission mode:") | bold | color(pal.info),
            text(""),
            hbox({
                pill(PermissionMode::Ask,   pal.warning) | flex,
                text("  "),
                pill(PermissionMode::Allow, pal.success)  | flex,
                text("  "),
                pill(PermissionMode::Deny,  pal.danger)    | flex,
            }),
            text("  hint: [1] Ask  [2] Allow  [3] Deny") | color(pal.muted),
        });

        // --- Confirm checkbox ---
        auto confirm_row = hbox({
            text(*confirmed ? "[✓] " : "[ ] ") | color(pal.success) | bold,
            text("I have reviewed this agent configuration and I am ready to ")
                | color(pal.muted),
            text(draft->is_edit_mode ? "save changes." : "create this agent.")
                | (*confirmed ? color(pal.success) | bold : color(pal.muted)),
        });

        // --- Summary block ---
        Elements summary_rows = {
            hbox({text(" Name:       ") | color(pal.muted),
                  text(draft->name.empty() ? "(missing!)" : draft->name) | bold
                      | color(draft->name.empty() ? pal.danger : pal.text)}),
            hbox({text(" Description:") | color(pal.muted),
                  text(draft->description.empty() ? std::string("(none)") : draft->description)}),
            hbox({text(" Roles:      ") | color(pal.muted),
                  RoleTags(draft->role_tags, 5)}),
            hbox({text(" Tools:      ") | color(pal.muted),
                  text(std::format("{} selected", draft->selected_tools.size()))
                      | color(pal.success)}),
            hbox({text(" Model:      ") | color(pal.muted),
                  text(draft->model) | color(pal.warning)}),
            hbox({text(" Temp:       ") | color(pal.muted),
                  text(std::format("{:.2f}", draft->temperature))}),
            hbox({text(" Permission: ") | color(pal.muted),
                  text(std::string(permission_label(draft->default_permission)))
                      | color(pal.info)}),
            hbox({text(" Paths:      ") | color(pal.muted),
                  text(std::format("{} allow · {} deny",
                                   draft->path_allowlist.size(),
                                   draft->path_denylist.size()))}),
            hbox({text(" Prompt:     ") | color(pal.muted),
                  text(std::format("{} chars", draft->system_prompt.size())) | color(pal.muted)}),
        };
        auto summary = vbox({
            text(" Configuration summary") | bold | color(pal.info),
            separator() | color(pal.info),
            vbox(std::move(summary_rows)),
        }) | borderLight;

        // Named ANSI status indices may equal their tint backgrounds.
        const auto variant = loom::ui::design::theme::current_theme().variant;
        const bool ansi = variant == loom::ui::design::theme::ThemeVariant::LightAnsi
                       || variant == loom::ui::design::theme::ThemeVariant::DarkAnsi;

        // --- "Ready?" banner ---
        bool ready = !draft->name.empty() && draft->confirmed;
        Element banner;
        if (ready) {
            banner = hbox({
                text(" " + std::string(loom::ui::design::figures::kTick) + " ") | color(pal.success) | bold,
                text(draft->is_edit_mode
                         ? "Ready to save. Press Enter → update agent registry."
                         : "Ready to create. Press Enter → write to agent registry.")
                    | color(pal.success) | bold,
            }) | bgcolor(ansi ? pal.background : pal.success_tint_bg) | borderLight | color(pal.success);
        } else {
            banner = hbox({
                text(" ! ") | color(pal.warning) | bold,
                text(std::string(draft->name.empty()
                          ? "Name is required. "
                          : "") +
                     std::string(!draft->confirmed
                          ? "Tick the confirmation box to proceed."
                          : ""))
                    | color(pal.warning),
            }) | bgcolor(ansi ? pal.background : pal.warning_tint_bg) | borderLight | color(pal.warning);
        }

        return vbox({
            text(" Step 4 · Permissions & summary") | bold | color(pal.info),
            separator() | color(pal.info),
            text(""),
            perm_row,
            separator() | dim,
            summary,
            text(""),
            confirm_row,
            text(""),
            banner,
            text(""),
            text(" hint: [Space] / [c] toggle confirm  [1..3] permission mode") | color(pal.muted),
        }) | color(pal.text);
    }) | CatchEvent([perm, confirmed](Event event) -> bool {
        if (event == Event::Character('1')) { *perm = 0; return true; }
        if (event == Event::Character('2')) { *perm = 1; return true; }
        if (event == Event::Character('3')) { *perm = 2; return true; }
        if (event == Event::Character(' ') || event == Event::Character('c')) {
            *confirmed = !*confirmed;
            return true;
        }
        if (event == Event::ArrowRight || event == Event::ArrowDown) {
            *perm = (*perm + 1) % 3;
            return true;
        }
        if (event == Event::ArrowLeft || event == Event::ArrowUp) {
            *perm = (*perm + 2) % 3;
            return true;
        }
        return false;
    });
}

// ============================================================
// Top-level AgentWizard factory
// ============================================================

/// Factory options.
struct AgentWizardOptions {
    /// If set, the wizard opens in EDIT mode pre-populated with this agent's
    /// data; otherwise it starts blank (CREATE mode).
    std::optional<cards::AgentCardData> edit_agent;

    std::function<void(const WizardDraft& draft)> on_save;
    std::function<void()> on_cancel;
};

/// Build the full 4-step agent wizard component. Builds a neutral
/// FeatureWizardRequest and resolves the ViewKind::AgentWizard factory
/// (registered by the composition root — RFC 0002 F2 row 6). The factory
/// builds the concrete wizard_dialog component; this module never imports
/// loom.ui.dialogs.wizard_dialog.
[[nodiscard]] inline Component AgentWizard(AgentWizardOptions opts) {
    auto draft = std::make_shared<WizardDraft>();

    // Pre-populate if editing.
    if (opts.edit_agent) {
        const auto& e = *opts.edit_agent;
        draft->is_edit_mode = true;
        draft->editing_agent_id = e.id;
        draft->name = e.name;
        draft->description = e.description_long.empty()
                                 ? e.description
                                 : e.description_long;
        draft->role_tags = e.role_tags;
        draft->selected_tools = e.tools;
        if (e.model_override) draft->model = *e.model_override;
        // Permission mode parsing.
        if (e.permission_mode) {
            std::string p = *e.permission_mode;
            if (p == "Allow")       draft->default_permission = PermissionMode::Allow;
            else if (p == "Deny")   draft->default_permission = PermissionMode::Deny;
            else                    draft->default_permission = PermissionMode::Ask;
        }
        if (!e.agent_type.empty()) {
            auto c = loom::tools::agent_color_manager::get_agent_color(e.agent_type);
            if (c) draft->avatar_color = c;
        }
    }

    fdp::FeatureWizardRequest request;
    request.title = opts.edit_agent ? "Edit Agent" : "Create New Agent";
    request.show_step_counter = true;
    request.on_cancel = std::move(opts.on_cancel);
    request.on_complete = [draft, cb = std::move(opts.on_save)] {
        if (cb) cb(*draft);
    };

    // Wire 4 steps.
    request.steps.push_back({
        .id = "basic",
        .title = "Basic",
        .description = "Name, description, roles, avatar color",
        .create_content = [draft] { return StepBasic(draft); },
    });
    request.steps.push_back({
        .id = "tools",
        .title = "Tools",
        .description = "Pick enabled tools + path scope",
        .create_content = [draft] { return StepTools(draft); },
    });
    request.steps.push_back({
        .id = "model",
        .title = "Model & Prompt",
        .description = "Model, temperature, system prompt",
        .create_content = [draft] { return StepModelPrompt(draft); },
    });
    request.steps.push_back({
        .id = "summary",
        .title = "Permissions & Summary",
        .description = "Default permission mode, confirm, review",
        .create_content = [draft] { return StepPermissions(draft); },
    });

    auto factory = fdp::resolve_dialog_factory(fdp::ViewKind::AgentWizard);
    return factory
               ? factory(std::make_shared<fdp::FeatureWizardRequest>(
                     std::move(request)))
               : Component();
}

} // namespace loom::ui::agents::wizard
