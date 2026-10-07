/// @file autocomplete_sources.cppm
/// @brief Thin data-source facade for prompt autocomplete.
module;

#include <cstddef>
#include <cstdint>

export module loom.ui.prompt.autocomplete_sources;

import std;

export namespace loom::ui::autocomplete_sources {

struct SkillSuggestionData {
    std::string name;
    std::string description;
    std::string source;
    std::string source_detail;
    std::string kind;                     // SL-08: skill kind (e.g. "workflow")
    std::size_t token_estimate = 0;
    bool enabled = true;
    bool user_invocable = true;           // SL-10: false => reject /name invocation
    std::string content;
};

struct PluginCommandSuggestionData {
    std::string command;
    std::string plugin_name;
};

struct McpResourceSuggestionData {
    std::string display;
    std::string description;
    std::string insert_text;
    std::string server_name;
    bool channel_like = false;
};

// DM/teammate + named agent autocomplete: typing @name shows teammate /
// subagent suggestions with status.  Agent definitions (agent_type +
// when_to_use + color) are merged with file/MCP results.  We combine both
// into a single flat list so the REPL autocomplete layer can rank them
// uniformly.
struct AgentSuggestionData {
    std::string name;                     // agent type or teammate name
    std::string description;              // short "when to use" or status text
    std::string source;                   // "agent" | "teammate"
    std::optional<std::string> color;     // agent color name (e.g. "blue")
    std::optional<std::string> status;    // teammate status (e.g. "running")
    bool is_subagent = false;             // true for native-agent records
};

// getHistory() yields HistoryEntry objects for the current project,
// newest-first, deduped by display text, capped at MAX_HISTORY_ITEMS (100).
// Ctrl+R search does a case-sensitive substring match via lastIndexOf.
struct HistorySuggestionData {
    std::string prompt_text;              // the matched prompt (first line)
    std::string full_text;                // full multi-line prompt text
    std::int64_t timestamp_ms = 0;        // when this prompt was submitted
    std::string session_id;               // originating session
    std::size_t message_count = 0;        // session message count (context)
};

[[nodiscard]] std::vector<SkillSuggestionData> collect_skill_suggestions(
    std::string_view cwd);

[[nodiscard]] std::optional<SkillSuggestionData> find_skill_suggestion(
    std::string_view cwd,
    std::string_view name);

[[nodiscard]] std::string skill_invocation_prompt(
    const SkillSuggestionData& skill,
    std::string_view user_text);

[[nodiscard]] std::vector<PluginCommandSuggestionData> collect_plugin_commands(
    std::string_view cwd);

[[nodiscard]] std::vector<McpResourceSuggestionData> collect_mcp_resource_suggestions();

// ── Agent / teammate autocomplete ────────────────────────────────────────
// Collects all available agent definitions + native agent (teammate) records
// into a single list.  `cwd` is used to resolve project-level agent defs.
[[nodiscard]] std::vector<AgentSuggestionData> collect_agent_suggestions(
    std::string_view cwd);

// ── History autocomplete ─────────────────────────────────────────────────
// Searches persisted prompt history for entries matching `query` (substring
// match, case-insensitive).  Returns up to `max_entries` results, sorted
// reverse-chronologically (newest first).  When `query` is empty, returns the
// most recent entries.
[[nodiscard]] std::vector<HistorySuggestionData> collect_history_suggestions(
    std::string_view query,
    std::size_t max_entries = 50);

// Append a prompt entry to the persisted history file.  Called on submit so
// that subsequent @history / Ctrl+R searches can find it.
void append_prompt_history(
    std::string_view prompt_text,
    std::string_view session_id,
    std::string_view project_path);

// Load recent prompt texts from the persisted history file for the given
// project, in chronological order (oldest first, newest last).  Used to
// populate the in-memory input_history on startup so the up-arrow recall
// works across sessions.  Dedups by display text.
[[nodiscard]] std::vector<std::string> load_recent_prompt_texts(
    std::string_view project_path,
    std::size_t max_entries = 100);

// ── Pre-formatted suggestions (to keep app.cppm under source-loc budget) ──
// A suggestion ready for add_suggestion(); mirrors AutocompleteSuggestion fields
// but lives in this module so the formatting logic can live outside app.cppm.
struct FormattedSuggestion {
    std::string display_text;
    std::string description;
    std::string insert_text;
    std::size_t replacement_start = 0;
    std::size_t replacement_end = 0;
    bool submit_on_return = false;
    std::string id;
    std::string icon;
    std::string color_name;  // agent color (e.g. "red", "blue") for colored dot
};

// Build @history suggestions with display truncation + relative-time formatting.
// `query` is the search term (empty = show recent).  `replacement_start`/`end`
// are passed through verbatim (caller knows cursor/token positions).
[[nodiscard]] std::vector<FormattedSuggestion> build_history_suggestions(
    std::string_view query,
    std::size_t replacement_start,
    std::size_t replacement_end,
    std::size_t max_entries = 50);

// Build agent/teammate suggestions, pre-filtered by fuzzy match on `query`.
// Returns only agents whose name matches the fuzzy scorer.
[[nodiscard]] std::vector<FormattedSuggestion> build_agent_suggestions(
    std::string_view cwd,
    std::string_view query,
    std::size_t token_start,
    std::size_t token_end);

} // namespace loom::ui::autocomplete_sources
