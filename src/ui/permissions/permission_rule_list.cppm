/// @file permission_rule_list.cppm
/// @brief Three-column permission rule editor (groups | rules table w/ virtual
/// scroll + batch ops | editor + hit-test firewall).  JSON import/export diff.
/// All mutations flow through callbacks to loom.security.permissions_engine.
///
/// 60 function bodies live in the module implementation unit
/// (permission_rule_list.cpp) so the declarations-only BMI stays cheap
/// (fan-out = 1 on a body edit). The MiniPicker template stays inline
/// (templates must be defined in the interface).
module;

#include <cctype>
#include <cstdlib>
#include <cstddef>
#include <cstdint>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

export module loom.ui.permissions.rule_list;

import loom.ui.foundation.theme_provider;

import std;

import loom.security.permissions_engine;
import loom.ui.permissions.scope_editor;
import loom.ui.permissions.components;
import loom.ui.foundation.design_tokens;
import loom.ui.widgets.custom_select;
import loom.ui.visual.structured_diff;

export namespace loom::ui::permissions::rule_list {
using namespace ftxui;
namespace dt     = loom::ui::design::tokens;
namespace se     = loom::ui::permissions::scope_editor;
namespace pc     = loom::ui::permissions::components;
namespace eng    = loom::utils::permissions;
namespace cs     = loom::ui::custom_select;
namespace sd     = loom::ui::structured_diff;

using eng::MatchStrategy;
using eng::PermissionAction;
using eng::PermissionScope;
using eng::PermissionRule;

// --- Enums & Types (mirror PermissionRuleList + UI9 data contracts) ---

/// Batch operation dispatcher (for multi-selected rules)
enum class BatchOp : std::uint8_t {
    Enable,        // Set enabled = true
    Disable,       // Set enabled = false
    SetStrategy,   // Change MatchStrategy (payload in op_arg)
    SetScope,      // Change PermissionScope
    SetAction,     // Change PermissionAction
    Delete,        // Remove rule(s)
    ExportJson,    // Serialize selected rules to JSON string
};

/// A single logical group of rules.  Mirrors the "rule tabs" (recent /
/// allow / ask / deny / workspace / custom x3) — 8 canonical groups total.
struct RuleGroup {
    std::string id;        // e.g. "g1", "recent", "allow"
    std::string label;     // "Recently used", "Allowed"
    std::string hotkey;    // single-char shortcut: "1" / "2" … "8"
    std::string icon;      // 1-2 glyph prefix
    Color       accent;    // group accent color
};

/// A single rule visual row.  Field alignment with scope_editor::AllowlistRow
/// and engine::PermissionRule; additional UI-only flags: selected, edited,
/// newly_created, search_hits.
struct RuleEntry {
    std::string                  id;
    std::string                  tool_pattern;
    MatchStrategy                strategy  = MatchStrategy::Glob;
    PermissionAction             action    = PermissionAction::Ask;
    PermissionScope              scope     = PermissionScope::Session;
    std::optional<std::string>   path_pattern = std::nullopt;
    int                          priority  = 50;

    std::string                  group_id;       // e.g. "g3"
    std::vector<std::string>     tools = {};     // explicit tool list (ToolMulti)
    std::string                  description;    // human-readable rule memo
    std::string                  source;         // Bundled / User / CustomPath
    bool                         enabled     = true;
    bool                         is_default  = false;
    std::size_t                  enabled_count = 0; // cache for group badges
};

struct HitSample {
    std::string test_path;      // e.g. "/etc/shadow"
    std::string test_tool;      // e.g. "BashTool"
    std::string note;           // short sample label
};

struct FilterSet {
    std::string           group_id;        // empty = all
    PermissionAction      action_filter = PermissionAction::Ask;
    std::optional<bool>   enabled_only;
    bool                  use_action_filter = false;
};

struct RuleListInput {
    std::vector<RuleEntry>    rules;
    std::vector<RuleGroup>    groups;        // 8 canonical
    std::string               search_query;
    FilterSet                 filters;
    std::vector<HitSample>    hit_samples;   // up to 3 for preview
};

/// All mutations go through these callbacks to the engine layer.
struct RuleListCallbacks {
    std::function<void(RuleEntry rule)>                          on_add;
    std::function<void(std::string_view rule_id, RuleEntry upd)> on_update;
    std::function<void(std::string_view rule_id)>                on_delete;
    std::function<void(std::vector<std::string> ids,
                       BatchOp op, std::string arg)>             on_batch;
    std::function<void(std::string_view json_text)>              on_import;
    std::function<std::string()>                                 on_export;
    std::function<std::string(const HitSample&, bool matched)>   on_hit_test;
};

// --- Constants (8 groups + column layout + virtual-scroll) ---

namespace detail {

/// Default 8 rule groups.  Callers can override in RuleListInput::groups but
/// the factory seeds this list when the input groups vector is empty.
[[nodiscard]] std::array<RuleGroup, 8> DefaultGroups();

/// Strategy / scope / action names (reused in editor dropdowns + batch op UI)
inline constexpr std::array<std::string_view, 4> kStrategyNames = {
    "Exact", "Prefix", "Glob", "Regex"
};
inline constexpr std::array<std::string_view, 4> kScopeNames    = {
    "Global", "Project", "Session", "Command"
};
inline constexpr std::array<std::string_view, 4> kActionNames   = {
    "Allow", "Deny", "Ask", "Ask once"
};

/// 3 sample firewall hit-test paths used when caller omits hit_samples.
inline constexpr std::array<const char*, 3> kDefaultSamples = {
    "/home/user/project/src/**/*.ts",
    "~/.aws/credentials",
    "/tmp/loom-workdir/build/**/*",
};

/// Virtual-scroll geometry constants.
inline constexpr int kVScrollVisible = 18;     // visible rule rows
inline constexpr int kVScrollBuffer  = 8;      // off-screen buffer for smooth
inline constexpr int kLeftColWidth   = 22;     // left column fixed width
inline constexpr int kRightColWidth  = 36;     // right column fixed width
inline constexpr int kDescriptionRows = 3;     // rule description editor height
inline constexpr int kDescriptionCols = 80;    // soft column cap for wrapping

} // namespace detail

// --- State ---

enum class FocusZone : std::uint8_t {
    Groups,     // left column group list
    Rules,      // middle rule table
    Editor,     // right column editor
    Toolbar,    // bottom toolbar
    DiffModal,  // JSON-diff fullscreen modal
    ImportModal,// Import JSON paste dialog
};

/// RuleEditor state (right column, subset of fields editable)
struct EditorState {
    bool dirty = false;
    std::string pattern_buf;
    int         strategy_idx = 2;  // Glob default
    int         scope_idx    = 2;  // Session default
    int         action_idx   = 2;  // Ask default
    int         group_idx    = 0;
    std::string description_buf;  // 3-line description
    std::vector<std::string> selected_tools;
};

/// Batch-pending state
struct BatchState {
    bool pending = false;
    BatchOp op;
    std::string op_arg;
};

struct RuleListState {
    RuleListInput     input;
    RuleListCallbacks cbs;

    std::size_t       active_group_idx = 0;
    std::size_t       rule_cursor      = 0;
    std::vector<std::size_t> selected_indexes;
    std::optional<std::size_t> anchor_index; // for Shift-range

    std::string       search_buf;
    bool              search_focused = false;
    FilterSet         filters;

    FocusZone         focus = FocusZone::Rules;

    EditorState       editor;
    std::optional<std::size_t> editing_idx;

    std::string       import_buf;
    bool              show_diff   = false;
    std::string       diff_old;
    std::string       diff_new;

    BatchState        batch;

    std::size_t       vscroll_offset = 0;

    std::vector<std::size_t> filtered;

    std::vector<HitSample> hit_samples;
    std::string status_bar_msg;
    std::size_t status_expire = 0;

    int toolbar_cursor = 0;
};

// --- Helpers: filter + badges + formatting ---

namespace detail {

[[nodiscard]] std::string_view StrategyName(MatchStrategy s);
[[nodiscard]] std::string_view ScopeName(PermissionScope s);
[[nodiscard]] std::string_view ActionName(PermissionAction a);
[[nodiscard]] Color ActionColor(PermissionAction a);

/// Apply search + filters; list of indices into state.input.rules.
void RefreshFiltered(RuleListState& st);

/// Per-group enabled/total badge numbers.
[[nodiscard]] auto GroupCounts(const RuleListState& st);

/// Selected filtered-indices -> matching rule ids.
[[nodiscard]] inline std::vector<std::string>
SelectedRuleIds(const RuleListState& st) {
    std::vector<std::string> ids;
    ids.reserve(st.selected_indexes.size());
    for (auto si : st.selected_indexes) {
        if (si >= st.filtered.size()) continue;
        ids.push_back(st.input.rules[st.filtered[si]].id);
    }
    return ids;
}

} // namespace detail

// --- Rendering helpers ---

namespace render {

[[nodiscard]] Element GroupsColumn(RuleListState& st);

Element RulesHeader();

[[nodiscard]] Element RuleRow(const RuleEntry& r, std::size_t row_num,
                                     bool hovered, bool selected, bool editing);

[[nodiscard]] Element RulesColumn(RuleListState& st);

} // namespace render

namespace render {

/// Generic mini segmented picker rendered from a string-view list.
template<std::size_t N>
[[nodiscard]] inline Element MiniPicker(int sel, Color c,
    const std::array<std::string_view, N>& names)
{
    Elements els; els.reserve(2 * N);
    for (std::size_t i = 0; i < N; ++i) {
        auto e = text(std::format(" {}", names[i]))
               | (std::size_t(sel) == i ? (bold | color(c) | inverted) : dim);
        els.push_back(std::move(e));
        if (i + 1 < N) els.push_back(text(" "));
    }
    return hbox(std::move(els));
}

[[nodiscard]] Element MiniStrategyPicker(const EditorState& ed);
[[nodiscard]] Element MiniScopePicker(const EditorState& ed);
[[nodiscard]] Element MiniActionPicker(const EditorState& ed);

/// Very simple word-wrap for the 3-line description buffer.
[[nodiscard]] inline std::vector<std::string>
SoftWrap(std::string_view text, std::size_t col) {
    std::vector<std::string> out;
    std::string cur; cur.reserve(col);
    std::istringstream iss(std::string{text});
    std::string word;
    while (iss >> word) {
        if (cur.size() + 1 + word.size() > col) {
            out.push_back(cur); cur = word;
            if (out.size() >= detail::kDescriptionRows - 1) break;
        } else {
            if (!cur.empty()) cur += ' ';
            cur += word;
        }
    }
    if (!cur.empty() && out.size() < detail::kDescriptionRows)
        out.push_back(std::move(cur));
    while (out.size() < detail::kDescriptionRows) out.emplace_back();
    if (out.empty()) out.emplace_back();
    return out;
}

[[nodiscard]] Element EffectivePreview(const RuleEntry* r,
                                              const std::vector<HitSample>& samples,
                                              RuleListState& st);

[[nodiscard]] Element EditorColumn(RuleListState& st);

} // namespace render

namespace render {

[[nodiscard]] Element Toolbar(RuleListState& st);

[[nodiscard]] Element ImportOverlay(RuleListState& st);

[[nodiscard]] Element DiffOverlay(RuleListState& st);

} // namespace render

// --- Main composition ---

[[nodiscard]] Element RenderRuleList(std::shared_ptr<RuleListState> st);

// --- Event handling helpers ---

namespace ev {

/// Cycle focus zones: Groups → Rules → Editor → Toolbar → Groups …
void CycleFocusForward(RuleListState& st);
void CycleFocusBack(RuleListState& st);

/// Toggle a single filtered-index into the selection set.
void ToggleIndex(RuleListState& st, std::size_t fi);

/// Select the index range [min(fi, anchor), max(fi, anchor)] inclusive.
void SelectRange(RuleListState& st, std::size_t fi);

void SelectAll(RuleListState& st);

/// Apply the current EditorState buffer back to the edited rule entry.
void CommitEditorChanges(RuleListState& st);

void LoadEditorForSelected(RuleListState& st, std::size_t fi);

void StartNewRule(RuleListState& st);

void DeleteAtCursor(RuleListState& st);

void FireBatch(RuleListState& st, BatchOp op, std::string arg = "");

} // namespace ev

// --- Event handlers per focus zone ---

namespace ev {

bool HandleGroups(RuleListState& st, Event e);

bool HandleSearch(RuleListState& st, Event e);

bool HandleRules(RuleListState& st, Event e);

bool HandleEditor(RuleListState& st, Event e);

bool HandleToolbar(RuleListState& st, Event e);

bool HandleImportModal(RuleListState& st, Event e);

bool HandleDiffModal(RuleListState& st, Event e);

} // namespace ev

// --- Public factory ---

[[nodiscard]] Component MakePermissionRuleList(
    RuleListInput input, RuleListCallbacks cbs);

} // namespace loom::ui::permissions::rule_list

// ═══════════════════════════════════════════════════════════════════════════
// NEW CONTENT FOR P2-04: Permissions rules UI tabs
// 7 additional functions + loom::utils::permissions_engine singleton state.
// ═══════════════════════════════════════════════════════════════════════════

// =========================================================================
// loom::utils::permissions_engine – NEW namespace: denial + workspace state
// =========================================================================
// This namespace is declared locally because the existing loom.security.permissions_engine
// module exports into loom::utils::permissions.  We follow the task spec and
// use a distinct namespace so that callers can write
//   loom::utils::permissions_engine::recent_denials(50)
// per the P2-04 contract.
// =========================================================================

export namespace loom::utils::permissions_engine {

using namespace ftxui;
namespace fs = std::filesystem;

// --- Denial tracking -----------------------------------------------------

/// A single auto-mode / denied tool invocation.
struct DenialEntry {
    int64_t     ts_ms = 0;         // epoch ms (we use system_clock)
    std::string tool_name;         // e.g. "Bash", "FileWrite"
    std::string action;            // short human summary
    std::string path;              // affected path / target
    std::string deny_reason;       // why it was denied
    bool        resolved = false;
    std::string rule_that_would_allow = {};
};

// --- Workspace directory -------------------------------------------------

struct WorkspaceEntry {
    fs::path path;
    enum class Policy { Allow, Deny, Default } policy = Policy::Default;
    bool is_default = false;
};

// --- Singleton storage (GlobalStateSlot pattern) ------------------------
// Uses an unnamed namespace with a mutex + vector, matching how
// loom::utils::permissions::PermissionEngine stores rules in permissions_engine.cppm.

namespace rl_anon_0 {

struct GlobalStateSlot {
    mutable std::mutex               mx;
    std::vector<DenialEntry>         denials;
    std::vector<WorkspaceEntry>      workspaces;
};

GlobalStateSlot& g_state() noexcept {
    static GlobalStateSlot s;
    return s;
}

} // namespace rl_anon_0
using namespace rl_anon_0; // unnamed namespace

// --- Denial public API ---------------------------------------------------

/// Return up to `limit` most recent denials, sorted newest first.
[[nodiscard]] auto recent_denials(int limit = 50)
    -> std::vector<DenialEntry>;

/// Mark the entry at filtered position `idx` (0 = newest) as acknowledged.
/// `idx` is interpreted relative to recent_denials() order.
auto acknowledge(int idx) -> void;

/// Internal helper: push a fresh denial.  Used by unit tests and by hooks
/// that feed denials into the UI.
auto push_denial(DenialEntry d) -> void;

/// Reset for unit tests only.
auto __test_reset_denials() -> void;

// --- Workspace public API ------------------------------------------------

[[nodiscard]] auto workspace_directories()
    -> std::vector<WorkspaceEntry>;

[[nodiscard]] auto add_workspace_dir(fs::path p,
                                            WorkspaceEntry::Policy policy)
    -> std::expected<void, std::string>;

[[nodiscard]] auto remove_workspace_dir(const fs::path& p) -> bool;

/// Seed a default workspace entry (used during startup to reflect the
/// initial project directory).  The `is_default = true` flag makes the
/// remove button a no-op.
auto seed_default_workspace(fs::path p) -> void;

/// Reset for unit tests only.
auto __test_reset_workspaces() -> void;

} // namespace loom::utils::permissions_engine

// =========================================================================
// Now extend loom::ui::permissions::rule_list with the 7 builder functions.
// =========================================================================

export namespace loom::ui::permissions::rule_list {

using namespace ftxui;
namespace fs = std::filesystem;
namespace peng = loom::utils::permissions_engine;
namespace pc   = loom::ui::permissions::components;
namespace dt   = loom::ui::design::tokens;
namespace eng  = loom::utils::permissions;

// =========================================================================
// Shared helpers used by multiple builders
// =========================================================================

namespace ui_tabs_detail {

/// Convert a millisecond timestamp delta to a compact "5s ago" / "3m ago"
/// / "2h ago" / "1d ago" string.
[[nodiscard]] std::string TimeAgo(int64_t ts_ms);

/// Apply a substring filter on the text fields of a denial row.
[[nodiscard]] bool DenialMatches(const peng::DenialEntry& d,
                                        std::string_view filter);

/// Simple "text + [ok/cancel]" dialog wrapper rendered on top of dbox, used
/// by BuildAddWorkspaceDirectoryModal and BuildRemoveWorkspaceDirectoryConfirm.
[[nodiscard]] Element DialogFrame(std::string_view title,
                                         Color accent,
                                         Element body,
                                         Element footer);

/// Render a decision badge (colored pill) for Allow/Deny/Abort actions.
[[nodiscard]] Element DecisionBadge(eng::PermissionAction a);

[[nodiscard]] Element WorkspacePolicyBadge(peng::WorkspaceEntry::Policy p,
                                                  bool is_default);

} // namespace ui_tabs_detail

// =========================================================================
// Forward declarations of modal builders (defined later in this file).
// =========================================================================

[[nodiscard]] Component BuildAddWorkspaceDirectoryModal(
    bool& open,
    std::shared_ptr<std::expected<void, std::string>> result_ref,
    std::function<void(fs::path, peng::WorkspaceEntry::Policy)> on_ok,
    std::function<void()> on_cancel);

[[nodiscard]] Component BuildRemoveWorkspaceDirectoryConfirm(
    bool& open,
    const peng::WorkspaceEntry& entry,
    std::function<void()> on_yes,
    std::function<void()> on_no);

/// Resolve a Component to its rendered Element.  FTXUI's Button() returns a
/// Component; many hbox/vbox layouts below compose buttons, so we render them
/// to Elements here.  Decorators applied via `|` (color, bold, size, ...) on
/// a Component are honoured because `Component | Decorator` returns a new
/// Component whose Render() applies the decoration.
namespace compel_detail {
/// Node wrapper that keeps a Component alive for the Element's lifetime.
///
/// FTXUI Button::Render() emits an Element containing `reflect(&box_)` (Button
/// tracks its box for focus/mouse).  When CompEl flattens a Button Component to
/// an Element and drops the Component, a later ftxui::Render() walks the tree
/// and Reflect::SetBox writes through the dangling box_ — heap-use-after-free.
///
/// This node OWNS the Component as a member, so the Button (and its box_) live
/// as long as the Element tree.  Rendering/box-delegation mirrors FTXUI's own
/// NodeDecorator: pass through to the single child unchanged.
class ComponentHolderNode : public Node {
 public:
    Component held_;
    ComponentHolderNode(Component c, Element el)
        : Node(Elements{std::move(el)}), held_(std::move(c)) {}
    void ComputeRequirement() override {
        Node::ComputeRequirement();
        requirement_ = children_[0]->requirement();
    }
    void SetBox(Box box) override {
        Node::SetBox(box);
        children_[0]->SetBox(box);
    }
};
}  // namespace compel_detail

[[nodiscard]] Element CompEl(Component c);

// =========================================================================
// New public state types (declared here so the header-reading callers have
// a stable definition; they are the input structs for each builder below).
// =========================================================================

/// View-model for the Recent Denials tab.  If `denials` is empty the builder
/// falls back to loom::utils::permissions_engine::recent_denials().
struct RecentDenialsState {
    std::optional<std::vector<peng::DenialEntry>> denials;
};

/// View-model for the Workspaces tab.  Falls back to
/// loom::utils::permissions_engine::workspace_directories() when empty.
struct WorkspaceState {
    std::optional<std::vector<peng::WorkspaceEntry>> entries;
};

/// Application-facing model for the 4-tab permissions panel.  Mirrors the
/// `loom::ui::permissions::PermissionsPanelModel` declared in
/// `permission_rules_ui.cppm` (same field layout so the wrapper there can
/// forward a copy without touching its own struct definition).
struct PermissionsPanelModel {
    std::optional<RuleListInput>      rule_list;
    std::optional<RecentDenialsState> denials;
    std::optional<WorkspaceState>     workspaces;
};

/// Callback bundle for the 4-tab permissions panel.  Same shape as
/// `loom::ui::permissions::PermissionsPanelCallbacks`.
struct PermissionsPanelCallbacks {
    std::function<void(const RuleEntry&)>                 on_add_rule;
    std::function<void(std::string_view, const RuleEntry&)> on_update_rule;
    std::function<void(std::string_view)>                 on_delete_rule;
    std::function<void(int idx)>                          on_allow_once;
    std::function<void(int idx)>                          on_always_allow;
    std::function<void(fs::path)>                         on_add_workspace;
    std::function<void(fs::path)>                         on_remove_workspace;
};

/// Aggregate model + callbacks container, used by BuildPermissionsTabs.
struct PermissionsTabsState {
    PermissionsPanelModel      model;
    PermissionsPanelCallbacks  callbacks;
};

/// Active-tab selector for the 4-tab permissions panel.  Values must match
/// `loom::ui::permissions::PermTab` declared in `permission_rules_ui.cppm`
/// (used by the wrapper that owns the tab bar UI).  Keeping the enum here lets
/// BuildPermissionsTabs route renders/events without cross-module imports.
enum class PermTab : std::uint8_t {
    AllRules   = 0,
    Denials    = 1,
    Workspaces = 2,
    CreateRule = 3,
};

// =========================================================================
// (a) BuildRecentDenialsTab
// =========================================================================

[[nodiscard]] Component BuildRecentDenialsTab(
    RecentDenialsState state,
    std::function<void(int idx)> on_allow_once,
    std::function<void(int idx)> on_always_allow);

// =========================================================================
// (b) BuildWorkspaceTab
// =========================================================================

[[nodiscard]] Component BuildWorkspaceTab(
    WorkspaceState state,
    std::function<void(const fs::path&, peng::WorkspaceEntry::Policy)> on_add,
    std::function<void(const fs::path&)> on_remove);

// =========================================================================
// (c) BuildAddWorkspaceDirectoryModal
// =========================================================================

[[nodiscard]] Component BuildAddWorkspaceDirectoryModal(
    bool& open,
    std::shared_ptr<std::expected<void, std::string>> result_ref,
    std::function<void(fs::path, peng::WorkspaceEntry::Policy)> on_ok,
    std::function<void()> on_cancel);

// =========================================================================
// (d) BuildRemoveWorkspaceDirectoryConfirm
// =========================================================================

[[nodiscard]] Component BuildRemoveWorkspaceDirectoryConfirm(
    bool& open,
    const peng::WorkspaceEntry& entry,
    std::function<void()> on_yes,
    std::function<void()> on_no);

// =========================================================================
// (e) BuildPermissionRuleInputForm
// =========================================================================

namespace rl_anon_1 {
struct RuleFormFieldErrors {
    std::string tool_pattern;
    std::string path_pattern;
    std::string description;
    std::string submit; // generic top-level error
};
} // namespace rl_anon_1
using namespace rl_anon_1; // unnamed

using RuleFormErrorsRef = std::shared_ptr<RuleFormFieldErrors>;

/// Decision enum for the form's radio group – slightly richer than the
/// engine's PermissionAction because we allow one-shot decisions.
enum class FormDecision {
    AllowOnce = 0,
    AlwaysAllow,
    Deny,
    AlwaysDeny,
    Abort,
};

namespace rl_anon_2 {
inline constexpr std::array<const char*, 5> kDecisionLabels = {
    "Allow once", "Always allow", "Deny", "Always deny", "Abort",
};
} // namespace rl_anon_2
using namespace rl_anon_2; // unnamed

[[nodiscard]] Component BuildPermissionRuleInputForm(
    RuleEntry rule,
    RuleFormErrorsRef errors,
    std::function<void(const RuleEntry&, FormDecision)> on_submit,
    std::function<void()> on_cancel);

// =========================================================================
// (f) BuildPermissionRuleDescriptionCard
// =========================================================================

[[nodiscard]] Component BuildPermissionRuleDescriptionCard(
    const RuleEntry& rule,
    std::function<void()> on_edit,
    std::function<void()> on_delete,
    std::function<void()> on_duplicate);

// =========================================================================
// (g) BuildPermissionsTabs – the single entry-point router
// =========================================================================

[[nodiscard]] Component BuildPermissionsTabs(
    PermissionsTabsState state,
    std::shared_ptr<PermTab> active_tab);

} // namespace loom::ui::permissions::rule_list

// Expose namespace alias so callers can spell
// `peng::recent_denials()` against either the import module's namespace OR
// the namespace declared above.
namespace loom::utils::permissions {
    namespace engine_engine_alias = loom::utils::permissions_engine;
}
