/// @file permissions_components.cppm
/// @brief Shared reusable UI widgets for the Permissions subsystem.
///
/// Consolidates 47 small widgets (titles, icons, rows, list items,
/// confirmation bars, badges, etc.) into one reusable component library.
///
/// Provides: ToolIcon, ActionIcon, PathLabel (middle-ellipsis), RiskPill,
/// HeaderRow, ThinDivider, StatusDot, PathContextBadge, ToolNameBadge,
/// KeyboardHint, CheckboxToggle, and utility formatters.
///
/// Engine logic is 100% delegated to loom.security.permissions_engine — this
/// file only imports display types, never duplicates matching logic.
module;

#include <cstddef>
#include <cstdint>

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.permissions.components;

import loom.ui.foundation.theme_provider;

import std;
import loom.ui.foundation.design_figures;

import loom.security.permissions_engine;

export namespace loom::ui::permissions::components {
using namespace ftxui;
using Rule = loom::utils::permissions::PermissionRule;
using MatchStrategy = loom::utils::permissions::MatchStrategy;

// ============================================================
// ToolIcon — icon for a tool by canonical name
// ============================================================

/// Get a short unicode icon glyph for a tool name.
/// Falls back to a generic gear if the tool is not known.
[[nodiscard]] inline std::string_view ToolIconGlyph(std::string_view tool_name) {
    // File tools
    if (tool_name == "FileReadTool" || tool_name == "file_read" ||
        tool_name == "Read" || tool_name == "FileRead")
        return loom::ui::design::figures::kArrowDown;
    if (tool_name == "FileWriteTool" || tool_name == "file_write" ||
        tool_name == "Write" || tool_name == "FileWrite")
        return loom::ui::design::figures::kDiamond;
    if (tool_name == "FileEditTool" || tool_name == "file_edit" ||
        tool_name == "Edit" || tool_name == "FileEdit")
        return loom::ui::design::figures::kDiamond;
    if (tool_name == "GlobTool" || tool_name == "glob")     return loom::ui::design::figures::kInfo;
    if (tool_name == "GrepTool" || tool_name == "grep")     return loom::ui::design::figures::kInfo;
    // Shell tools
    if (tool_name == "BashTool" || tool_name == "bash")     return loom::ui::design::figures::kPlay;
    if (tool_name == "PowerShellTool" || tool_name == "powershell") return loom::ui::design::figures::kPlay;
    if (tool_name == "SedEditTool")                         return loom::ui::design::figures::kDiamond;
    // Network tools
    if (tool_name == "WebFetchTool" || tool_name == "web_fetch") return loom::ui::design::figures::kArrowRight;
    if (tool_name == "WebSearchTool" || tool_name == "web_search") return loom::ui::design::figures::kInfo;
    // Agent / planning
    if (tool_name == "AgentTool" || tool_name == "agent")   return loom::ui::design::figures::kDiamond;
    if (tool_name == "SkillTool" || tool_name == "skill")   return loom::ui::design::figures::kDiamond;
    if (tool_name == "EnterPlanModeTool" || tool_name == "EnterPlanMode") return loom::ui::design::figures::kReferenceMark;
    if (tool_name == "exit_plan_mode" || tool_name == "ExitPlanMode") return loom::ui::design::figures::kTick;
    if (tool_name == "TaskCreateTool" || tool_name == "task_create") return loom::ui::design::figures::kDiamond;
    if (tool_name == "TaskUpdateTool" || tool_name == "task_update") return loom::ui::design::figures::kDiamond;
    if (tool_name == "TeamCreateTool" || tool_name == "team_create") return loom::ui::design::figures::kCircleDouble;
    // MCP / Bridge
    if (tool_name == "MCPTool" || tool_name == "mcp")       return loom::ui::design::figures::kArrowRight;
    if (tool_name == "NotebookEditTool")                    return loom::ui::design::figures::kReferenceMark;
    // User interaction
    if (tool_name == "AskUserQuestionTool" || tool_name == "AskUserQuestion") return loom::ui::design::figures::kQuestionMarkPrefix;
    if (tool_name == "ComputerUseApproval" || tool_name == "ComputerUse")     return loom::ui::design::figures::kSquare;
    // Generic
    return loom::ui::design::figures::kDiamond;
}

/// Render a tool icon as an FTXUI Element (fixed 2-cell width)
[[nodiscard]] inline Element ToolIcon(std::string_view tool_name) {
    return text(std::string{ToolIconGlyph(tool_name)}) | center;
}

// ============================================================
// ActionIcon — icon for an action category (Read/Write/Execute/Network)
// ============================================================

enum class ActionKind : std::uint8_t {
    Read,       // File read, search, fetch
    Write,      // File write, edit, delete
    Execute,    // Bash, PowerShell, Skill, Agent
    Network,    // Web fetch, web search
    MCP,        // MCP tool call
    Environment,// Environment variable access
    Plan,       // Plan mode entry/exit
    Other,
};

[[nodiscard]] inline std::string_view ActionIconGlyph(ActionKind kind) {
    switch (kind) {
        case ActionKind::Read:        return loom::ui::design::figures::kArrowDown;
        case ActionKind::Write:       return loom::ui::design::figures::kDiamond;
        case ActionKind::Execute:     return loom::ui::design::figures::kPlay;
        case ActionKind::Network:     return loom::ui::design::figures::kArrowRight;
        case ActionKind::MCP:         return loom::ui::design::figures::kArrowRight;
        case ActionKind::Environment: return loom::ui::design::figures::kQuestionMarkPrefix;
        case ActionKind::Plan:        return loom::ui::design::figures::kReferenceMark;
        case ActionKind::Other:       return loom::ui::design::figures::kDiamond;
    }
    return loom::ui::design::figures::kDiamond;
}

[[nodiscard]] inline Color ActionColor(ActionKind kind) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (kind) {
        case ActionKind::Read:        return pal.info;
        case ActionKind::Write:       return pal.warning;
        case ActionKind::Execute:     return pal.primary;
        case ActionKind::Network:     return pal.info;
        case ActionKind::MCP:         return pal.primary;
        case ActionKind::Environment: return pal.permission;
        case ActionKind::Plan:        return pal.plan_mode;
        case ActionKind::Other:       return pal.muted;
    }
    return pal.text;
}

/// Render an action icon with its color as an FTXUI Element
[[nodiscard]] inline Element ActionIcon(ActionKind kind) {
    return text(std::string{ActionIconGlyph(kind)}) | color(ActionColor(kind));
}

// ============================================================
// PathLabel — display a path with middle-ellipsis and optional workspace indicator
// ============================================================

/// Produce a middle-ellipsis path string to fit within max_len.
/// e.g. /a/very/long/path/to/some/file.txt → /a/…/some/file.txt
[[nodiscard]] inline std::string MiddleEllipsisPath(std::string_view path,
                                                    std::size_t max_len = 50) {
    if (path.size() <= max_len) return std::string{path};
    // Preserve at least 10 chars for the tail (filename + 2 dirs preferred)
    constexpr std::size_t kMinTail = 10;
    constexpr std::size_t kMinHead = 5;
    const std::size_t ellipsis = 3; // "…"
    // If max_len is too small for head+ellipsis+tail, just head-ellipsis
    if (max_len < kMinHead + ellipsis + kMinTail) {
        if (max_len <= ellipsis) return "…";
        return std::string{path.substr(0, max_len - ellipsis)} + "…";
    }
    const std::size_t tail_len = std::min(
        std::max(kMinTail, max_len / 3),
        path.size() - kMinHead - ellipsis);
    const std::size_t head_len = max_len - tail_len - ellipsis;
    return std::string{path.substr(0, head_len)} + "…"
         + std::string{path.substr(path.size() - tail_len, tail_len)};
}

/// Returns true if the path is inside (or equal to) the workspace root.
[[nodiscard]] inline bool IsInsideWorkspace(std::string_view path,
                                            std::string_view workspace_root) {
    if (workspace_root.empty()) return false;
    if (path == workspace_root) return true;
    // Ensure trailing separator consistency
    std::string root{workspace_root};
    if (!root.empty() && root.back() != '/' && root.back() != '\\') {
        root += '/';
    }
    return path.substr(0, root.size()) == std::string_view{root};
}

/// Render a path label with middle-ellipsis and optional workspace badge.
[[nodiscard]] inline Element PathLabel(std::string_view path,
                                       std::size_t max_len = 50,
                                       std::optional<std::string_view> workspace_root = {}) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto el = text(MiddleEllipsisPath(path, max_len));
    if (workspace_root && IsInsideWorkspace(path, *workspace_root)) {
        return hbox({
            el,
            text(" w") | dim | color(pal.success),
        });
    }
    return el;
}

/// Render a path with a danger highlight if it's on the high-risk list.
/// caller supplies a predicate — this file never hard-codes dangerous paths.
[[nodiscard]] inline Element PathLabelHighlighted(
    std::string_view path,
    bool is_dangerous,
    std::size_t max_len = 50)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    auto el = text(MiddleEllipsisPath(path, max_len));
    if (is_dangerous) {
        return el | bold | color(pal.danger);
    }
    return el;
}

// ============================================================
// RiskPill — colored risk-level indicator pill
// ============================================================

enum class RiskLevel : std::uint8_t {
    Low,        // Green  — read-only, trivial tools
    Medium,     // Yellow — writes inside workspace
    High,       // Red    — deletes, writes outside workspace
    Critical,   // Bold   red — rm -rf /, network uploads of secrets, etc.
};

[[nodiscard]] inline Color RiskPillBgColor(RiskLevel level) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (level) {
        case RiskLevel::Low:      return pal.success;
        case RiskLevel::Medium:   return pal.warning;
        case RiskLevel::High:     return pal.danger;
        case RiskLevel::Critical: return pal.danger;
    }
    return pal.muted;
}

[[nodiscard]] inline Color RiskPillFgColor(RiskLevel level) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    // Status fills remain readable when semantic colors collapse in monochrome.
    (void)level;
    return pal.inverse_text;
}

[[nodiscard]] inline std::string_view RiskLabel(RiskLevel level) {
    switch (level) {
        case RiskLevel::Low:      return "LOW";
        case RiskLevel::Medium:   return "MEDIUM";
        case RiskLevel::High:     return "HIGH";
        case RiskLevel::Critical: return "CRITICAL";
    }
    return "???";
}

/// Render a risk pill as a colored badge element: ` [LOW] `
[[nodiscard]] inline Element RiskPill(RiskLevel level) {
    auto fg = RiskPillFgColor(level);
    auto bg = RiskPillBgColor(level);
    return text(std::format(" {} ", RiskLabel(level))) | color(fg) | bgcolor(bg) | bold;
}

// ============================================================
// HeaderRow — title + optional subtitle used at the top of any permission UI
// ============================================================

/// Render a standard two-line header row with a bold title and dimmed subtitle.
[[nodiscard]] inline Element HeaderRow(std::string_view title,
                                       std::optional<std::string_view> subtitle = {}) {
    auto title_el = text(std::string{title}) | bold;
    if (!subtitle) return title_el;
    return vbox({
        title_el,
        text(std::string{*subtitle}) | dim,
    });
}

/// Header with a leading icon (tool or action) on the left and a right widget
/// (e.g. a RiskPill) on the right.
[[nodiscard]] inline Element HeaderRowWithIcon(
    std::string_view icon,
    std::string_view title,
    std::optional<std::string_view> subtitle = {},
    Element right_widget = text(""))
{
    auto left = hbox({
        text(std::string{icon}),
        text(" "),
        HeaderRow(title, subtitle),
    });
    return hbox({
        left | xflex,
        right_widget,
    });
}

// ============================================================
// ThinDivider — light separator line
// ============================================================

/// A thin horizontal separator (dimmer than ftxui::separator()).
/// Optional `col` override — when provided, paints the separator in that
/// color (faithful to the Divider(color) pattern).  Defaults to GrayDark
/// for the pre-existing uncoloured in-dialog section separators.
[[nodiscard]] inline Element ThinDivider(
    std::optional<Color> col = std::nullopt) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return separator() | color(col.value_or(pal.muted));
}

// ============================================================
// StatusDot — colored circular indicator for list items
// ============================================================

enum class ItemStatus : std::uint8_t {
    Pending,   // Yellow — awaiting decision
    Approved,  // Green  — allowed
    Denied,    // Red    — denied
    Always,    // Cyan   — always rule applied
};

[[nodiscard]] inline Color StatusDotColor(ItemStatus s) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (s) {
        case ItemStatus::Pending:  return pal.warning;
        case ItemStatus::Approved: return pal.success;
        case ItemStatus::Denied:   return pal.danger;
        case ItemStatus::Always:   return pal.info;
    }
    return pal.muted;
}

/// Render a 2-cell status marker; shape retains the status in monochrome.
[[nodiscard]] inline Element StatusDot(ItemStatus s) {
    namespace figs = loom::ui::design::figures;
    const auto glyph = [&] {
        switch (s) {
            case ItemStatus::Pending:  return figs::kCircle;
            case ItemStatus::Approved: return figs::kTick;
            case ItemStatus::Denied:   return figs::kCross;
            case ItemStatus::Always:   return figs::kCircleDouble;
        }
        return figs::kCircle;
    }();
    return text(std::string{glyph} + " ") | color(StatusDotColor(s));
}

// ============================================================
// PathContextBadge — small badge showing scope (Read/Write/Exec)
// ============================================================

enum class PathScope : std::uint8_t {
    Read,
    Write,
    Execute,
    Network,
    All,
};

[[nodiscard]] inline std::string_view PathScopeLabel(PathScope s) {
    switch (s) {
        case PathScope::Read:    return "R";
        case PathScope::Write:   return "W";
        case PathScope::Execute: return "X";
        case PathScope::Network: return "N";
        case PathScope::All:     return "RWX";
    }
    return "?";
}

[[nodiscard]] inline Color PathScopeColor(PathScope s) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    switch (s) {
        case PathScope::Read:    return pal.info;
        case PathScope::Write:   return pal.warning;
        case PathScope::Execute: return pal.primary;
        case PathScope::Network: return pal.info;
        case PathScope::All:     return pal.success;
    }
    return pal.text;
}

/// Render a small scope badge, e.g. `[R]` or `[W]`.
[[nodiscard]] inline Element PathScopeBadge(PathScope scope) {
    return text(std::format("[{}]", PathScopeLabel(scope)))
           | color(PathScopeColor(scope)) | dim;
}

// ============================================================
// ToolNameBadge — tool name with color based on risk category
// ============================================================

/// Render a tool name badge with a dim border.
[[nodiscard]] inline Element ToolNameBadge(std::string_view tool_name,
                                           RiskLevel level = RiskLevel::Low) {
    return text(std::format(" {} ", tool_name))
           | color(ActionColor([&] {
               // heuristic coloring based on risk
               if (level >= RiskLevel::High) return ActionKind::Execute;
               if (level == RiskLevel::Medium) return ActionKind::Write;
               return ActionKind::Read;
           }()))
           | borderLight | size(WIDTH, EQUAL, static_cast<int>(tool_name.size()) + 4);
}

// ============================================================
// KeyboardHint — bottom-of-dialog keybinding hint row
// ============================================================

struct KeyHint {
    std::string key;        // e.g. "y", "Esc", "Enter"
    std::string action;     // e.g. "Allow once"
};

/// Render a row of keyboard hints, dimmed: `[y] Allow  [n] Deny  [Esc] Cancel`
[[nodiscard]] inline Element KeyboardHintRow(
    const std::vector<KeyHint>& hints)
{
    Elements parts;
    for (std::size_t i = 0; i < hints.size(); ++i) {
        const auto& pal = *loom::ui::design::theme::current_theme().palette;
        const auto& h = hints[i];
        parts.push_back(text("[") | dim);
        parts.push_back(text(h.key) | color(pal.info) | bold | dim);
        parts.push_back(text("] ") | dim);
        parts.push_back(text(h.action) | dim);
        if (i + 1 < hints.size()) parts.push_back(text("  ") | dim);
    }
    return hbox(parts);
}

// ============================================================
// CheckboxToggle — interactive [ ] / [x] toggle used for "always" options
// ============================================================

/// Render a checkbox state (static label — for embedding in a row).
[[nodiscard]] inline Element CheckboxBox(bool checked) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return checked
        ? hbox({text("[") | dim, text("x") | color(pal.success) | bold, text("]") | dim})
        : hbox({text("[") | dim, text(" "),                        text("]") | dim});
}

/// State for a checkbox component.
struct CheckboxState {
    bool checked = false;
    std::string label;
};

/// Create an interactive checkbox with keyboard support (Space toggles).
[[nodiscard]] inline Component CheckboxToggle(
    std::shared_ptr<CheckboxState> state,
    std::function<void(bool)> on_change = {})
{
    return Renderer([state] {
        return hbox({
            CheckboxBox(state->checked),
            text(" "),
            text(state->label),
        });
    }) | CatchEvent([state, on_change](Event event) -> bool {
        if (event == Event::Character(' ') || event == Event::Return) {
            state->checked = !state->checked;
            if (on_change) on_change(state->checked);
            return true;
        }
        return false;
    });
}

// ============================================================
// MatchStrategy labels — delegate to engine enums without duplication
// ============================================================

[[nodiscard]] inline std::string_view MatchStrategyLabel(MatchStrategy s) {
    using loom::utils::permissions::MatchStrategy;
    switch (s) {
        case MatchStrategy::Exact:  return "exact";
        case MatchStrategy::Prefix: return "prefix";
        case MatchStrategy::Glob:   return "glob";
        case MatchStrategy::Regex:  return "regex";
    }
    return "???";
}

// ============================================================
// Helpers for path depth and list summarisation
// ============================================================

/// Count path separators (rough depth count).
[[nodiscard]] inline int PathDepth(std::string_view path) {
    int depth = 0;
    for (char c : path) if (c == '/' || c == '\\') ++depth;
    return depth;
}

/// Summarise N paths: show up to `max_shown` with middle-ellipsis,
/// add "+N more" footer. Returns an Elements vector.
[[nodiscard]] inline Elements SummarisePathList(
    const std::vector<std::string>& paths,
    int max_shown = 5,
    std::size_t max_path_len = 50)
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    Elements els;
    int shown = 0;
    for (const auto& p : paths) {
        if (shown >= max_shown) break;
        els.push_back(hbox({
            text("• ") | dim,
            PathLabel(p, max_path_len),
        }));
        ++shown;
    }
    if (paths.size() > static_cast<std::size_t>(max_shown)) {
        auto remaining = paths.size() - static_cast<std::size_t>(max_shown);
        els.push_back(text(std::format("… and {} more", remaining)) | dim | color(pal.muted));
    }
    return els;
}

// ============================================================
// Ellipsised command preview
// ============================================================

/// Trim a shell command preview to fit `max_len` using head-ellipsis
/// (preserves the end of the command, which is usually the interesting part).
[[nodiscard]] inline std::string HeadEllipsisCommand(std::string_view cmd,
                                                     std::size_t max_len = 80) {
    if (cmd.size() <= max_len) return std::string{cmd};
    if (max_len < 4) return "…";
    const std::size_t keep = max_len - 1;
    return "…" + std::string{cmd.substr(cmd.size() - keep, keep)};
}

/// Render a shell command with a "$ " prefix and optional truncation.
[[nodiscard]] inline Element CommandPreview(std::string_view cmd,
                                            std::size_t max_len = 80) {
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return hbox({
        text("$ ") | color(pal.warning) | bold,
        text(HeadEllipsisCommand(cmd, max_len)) | color(pal.warning),
    });
}

// ============================================================
// Destructive warning banner (reused from trust_utils visual style)
// ============================================================

/// Render a yellow warning banner for destructive commands.
/// Caller determines destructiveness — we only draw the banner.
[[nodiscard]] inline Element DestructiveWarningBanner(
    std::string_view reason = "This command may permanently delete or overwrite files.")
{
    const auto& pal = *loom::ui::design::theme::current_theme().palette;
    return vbox({
        hbox({
            text("⚠ ") | color(pal.warning) | bold,
            text(std::string{reason}) | color(pal.warning) | bold,
        }),
    }) | borderStyled(pal.warning) | bgcolor(pal.warning_tint_bg);
}

} // namespace loom::ui::permissions::components
