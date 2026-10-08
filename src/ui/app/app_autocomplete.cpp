// app_autocomplete.cpp — impl unit for AppAdapter::RefreshAutocompleteSuggestions
// and its helper token_around_cursor.  Split from the former monolithic
// app_autocomplete.cpp (P2-1e) to isolate the autocomplete closure from
// the render/event closure.
//
// Other impl units:
//   app_render_event.cpp   — ~AppAdapter, Render, OnEvent, ActiveChild
//   app_text_selection.cpp — SelectionHighlightNode, drag-to-select mouse
//                           handling, text extraction + clipboard copy
//   app_constructor.cpp    — constructor
//   app_handle_submit.cpp  — HandleSubmit, HandleCommand
//   app_agent_menu.cpp     — FormatAgentsMenuOutput, LoadAgentCardsForMenu,
//                           SyncState, ConsumePendingResult,
//                           DrainCompletedPastesAndSubmit, get_permission_callback,
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
#include <ftxui/dom/elements.hpp>
#include <cstdint>

module loom.ui.app.app;

import std;
import loom.commands.registry;
import loom.commands.command;

import loom.tools.agent_runtime;
import loom.ui.features.agents.agent_cards;
import loom.ui.prompt.autocomplete_sources;
import loom.ui.prompt.file_index;
import loom.ui.prompt.fuzzy_rank_nucleo;
import loom.ui.screens.repl_screen;
import loom.ui.screens.repl_state;
import loom.ui.screens.messages_store;
import loom.session.storage;
import loom.skills.support;
import loom.fs.path;

namespace loom::ui {
namespace agent_runtime = loom::tools::agent_runtime;
namespace agent_cards = loom::ui::agents::cards;
// Defined in app_extra_methods.cpp (same module); redeclared for module linkage.
agent_cards::AgentCardData project_agent_definition_card(
    const agent_runtime::AgentDefinition& agent);

namespace repl = loom::ui::repl_screen;
namespace acsrc = loom::ui::autocomplete_sources;
namespace frn = loom::ui::prompt::fuzzy_rank_nucleo;
namespace fidx = loom::ui::prompt::file_index;

namespace {

// Cache source data rather than filtered suggestions: cursor replacement ranges
// and fuzzy matches must always reflect the latest input. Each source retains
// at most eight contexts; thread-local ownership keeps UI instances on different
// event-loop threads independent without locking around filesystem reads.
template <typename T>
class CompletionSourceCache {
    using Clock = std::chrono::steady_clock;
    struct Entry {
        T value;
        Clock::time_point refreshed;
        Clock::time_point accessed;
    };
    std::unordered_map<std::string, Entry> entries_;

public:
    template <typename Loader>
    const T& get(std::string key, Loader&& load) {
        const auto now = Clock::now();
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            // Load before eviction so an unsuccessful loader preserves the cache.
            auto value = load();
            if (entries_.size() >= 8) {
                auto oldest = std::ranges::min_element(entries_, {},
                    [](const auto& item) { return item.second.accessed; });
                entries_.erase(oldest);
            }
            it = entries_.emplace(std::move(key), Entry{
                std::move(value), now, now}).first;
        } else {
            const auto age = now - it->second.refreshed;
            const auto quiet = now - it->second.accessed;
            // Debounce expired-source refreshes during rapid input. Refresh on
            // the first request after a pause, or after five seconds even when
            // typing continues. No background timer or stale filtered rows.
            if (age >= std::chrono::seconds(1) &&
                (quiet >= std::chrono::milliseconds(150) ||
                 age >= std::chrono::seconds(5))) {
                it->second.value = load();
                it->second.refreshed = now;
            }
            it->second.accessed = now;
        }
        return it->second.value;
    }
};

struct CompletionDirectoryEntry {
    std::string name;
    bool is_directory = false;
};

const std::vector<CompletionDirectoryEntry>& completion_directory_entries(
    const std::filesystem::path& cwd,
    const std::filesystem::path& directory) {
    static thread_local CompletionSourceCache<
        std::vector<CompletionDirectoryEntry>> cache;
    const auto parent = directory.lexically_normal();
    // Include cwd even for absolute paths: changing projects invalidates the
    // active context, while different leaf queries reuse the same source list.
    return cache.get(cwd.string() + "\n" + parent.string(), [&] {
        std::vector<CompletionDirectoryEntry> entries;
        std::error_code ec;
        for (auto it = std::filesystem::directory_iterator(parent, ec);
             !ec && it != std::filesystem::directory_iterator();
             it.increment(ec)) {
            const bool is_directory = it->is_directory(ec);
            if (ec) ec.clear();
            entries.push_back({it->path().filename().string(), is_directory});
        }
        return entries;
    });
}

// Cache the whole PATH source, rather than each PATH directory separately:
// long PATH lists must not churn the eight-context directory cache per keypress.
const std::vector<std::string>& completion_shell_commands(
    const std::filesystem::path& cwd,
    std::string_view path_text) {
    static thread_local CompletionSourceCache<std::vector<std::string>> cache;
    return cache.get(cwd.string() + "\n" + std::string(path_text), [&] {
        std::vector<std::string> names;
        std::unordered_set<std::string> seen;
        auto paths = path_text;
        while (!paths.empty()) {
            const auto sep = paths.find(':');
            const auto current = paths.substr(0, sep);
            if (sep == std::string_view::npos) paths = {};
            else paths.remove_prefix(sep + 1);
            if (current.empty()) continue;
            std::filesystem::path dir{std::string(current)};
            if (dir.is_relative()) dir = cwd / dir;
            for (const auto& entry : completion_directory_entries(cwd, dir)) {
                if (seen.insert(entry.name).second) names.push_back(entry.name);
            }
        }
        return names;
    });
}

struct CompletionTeammateEntry {
    std::string name;
    std::string status;
    std::string color;
};

std::string completion_agent_context(std::string_view cwd) {
    const char* home = std::getenv("HOME");
    const char* config = std::getenv("LOOM_CONFIG_DIR");
    return std::format("{}\n{}\n{}\n{}", cwd,
        agent_runtime::runtime_state_dir().string(),
        home ? home : "", config ? config : "");
}

const std::vector<CompletionTeammateEntry>& completion_native_agents(
    std::string_view cwd) {
    static thread_local CompletionSourceCache<
        std::vector<CompletionTeammateEntry>> cache;
    return cache.get(completion_agent_context(cwd), [] {
        std::vector<CompletionTeammateEntry> entries;
        for (const auto& record : agent_runtime::load_all_native_agent_records()) {
            entries.push_back({
                record.name.value_or(record.agent_id),
                std::string(agent_runtime::native_agent_status_name(record.status)),
                record.teammate_color.value_or("")});
        }
        return entries;
    });
}

const std::vector<acsrc::FormattedSuggestion>& completion_agent_suggestions(
    std::string_view cwd) {
    static thread_local CompletionSourceCache<
        std::vector<acsrc::FormattedSuggestion>> cache;
    return cache.get(completion_agent_context(cwd), [&] {
        return acsrc::build_agent_suggestions(cwd, "", 0, 0);
    });
}

struct CompletionSessionEntry {
    loom::session::SessionMetadata metadata;
    int message_count = 0;
    std::string fallback_title;
};

const std::vector<CompletionSessionEntry>& completion_session_entries(
    const std::filesystem::path& directory) {
    static thread_local CompletionSourceCache<
        std::vector<CompletionSessionEntry>> cache;
    return cache.get(directory.lexically_normal().string(), [&] {
        std::vector<CompletionSessionEntry> entries;
        for (auto metadata : loom::session::list_recent_sessions(directory, 50)) {
            const int count = count_session_messages(directory, metadata.session_id);
            std::string fallback;
            if (count > 0 &&
                (metadata.title.value_or("").empty() || metadata.title == "Session")) {
                fallback = first_user_message_as_title(directory, metadata.session_id);
            }
            entries.push_back({std::move(metadata), count, std::move(fallback)});
        }
        return entries;
    });
}

}  // namespace

// RFC 0001 Phase C batch 2: folded from app.cppm — sole caller is
// RefreshAutocompleteSuggestions below. Plain (non-inline) definition;
// the declaration stays exported in app.cppm.
[[nodiscard]] AutocompleteToken token_around_cursor(
    std::string_view input,
    std::size_t cursor) {
    if (cursor == std::string::npos || cursor > input.size()) {
        cursor = input.size();
    }

    std::size_t start = cursor;
    while (start > 0 && !ascii_isspace(input[start - 1])) --start;

    std::size_t end = cursor;
    while (end < input.size() && !ascii_isspace(input[end])) ++end;

    // Quoted @ mention detection.
    // If the token starts with @", extend end to include the full quoted content
    // (up to closing quote or end of input). This allows @"path with spaces"
    // to be treated as a single token for autocomplete.
    std::string text_before = std::string(input.substr(start, cursor - start));
    std::size_t token_end = cursor;
    if (text_before.starts_with("@\"")) {
        // Find the closing quote after cursor, or end of input.
        std::size_t close = input.find('"', cursor);
        if (close != std::string_view::npos) {
            token_end = close + 1;  // include the closing quote
        } else {
            token_end = input.size();  // unterminated quote — extend to end
        }
    }

    return AutocompleteToken{
        .start = start,
        .end = token_end,
        .text = std::string(input.substr(start, cursor - start)),
    };
}

void AppAdapter::RefreshAutocompleteSuggestions() {
    const auto previous_suggestions = screen_state_->autocomplete_suggestions;
    const int previous_index = screen_state_->autocomplete_index;
    screen_state_->autocomplete_suggestions.clear();
    screen_state_->autocomplete_index = -1;
    screen_state_->autocomplete_stable_name_width = 0;  // INF-03: slash branch sets it

    const std::string& input = screen_state_->input_text;
    const std::size_t cursor =
        screen_state_->input_cursor == std::string::npos ||
            screen_state_->input_cursor > input.size()
        ? input.size()
        : screen_state_->input_cursor;
    const auto token = token_around_cursor(input, cursor);

    auto add_suggestion = [&](std::string display,
                              std::string description,
                              std::string insert,
                              std::size_t start,
                              std::size_t end,
                              bool submit_on_return = false,
                              std::string id = "",
                              std::string icon = "",
                              std::string color_name = "") {
        if (id.empty()) id = display;  // INF-02: stable-id fallback
        screen_state_->autocomplete_suggestions.push_back(
            repl::ReplScreenState::AutocompleteSuggestion{
                .display_text = std::move(display),
                .description = std::move(description),
                .insert_text = std::move(insert),
                .replacement_start = start,
                .replacement_end = end,
                .submit_on_return = submit_on_return,
                .icon = std::move(icon),
                .id = std::move(id),
                .color_name = std::move(color_name),
            });
    };

    auto restore_index = [&] {
        if (screen_state_->autocomplete_suggestions.empty()) return;
        int preserved_index = 0;
        if (previous_index >= 0 &&
            previous_index < static_cast<int>(previous_suggestions.size())) {
            const auto& previous =
                previous_suggestions[static_cast<std::size_t>(previous_index)];
            // INF-02: match by stable id (falls back to display_text via
            // add_suggestion's default), so same-display items from
            // different sources don't collide.
            auto it = std::ranges::find(
                screen_state_->autocomplete_suggestions,
                previous.id,
                &repl::ReplScreenState::AutocompleteSuggestion::id);
            if (it != screen_state_->autocomplete_suggestions.end()) {
                preserved_index = static_cast<int>(
                    std::distance(
                        screen_state_->autocomplete_suggestions.begin(),
                        it));
            }
        }
        screen_state_->autocomplete_index = preserved_index;
    };

    if (input.empty()) {
        // SL-11: the next-action suggestion is shown as an inline placeholder
        // (via placeholder_cascade L0), NOT as an autocomplete popup entry.
        // Tab accepts it (see repl_screen_events.cpp).
        return;
    }
    // SL-11: user typed something — retire the next-action suggestion.
    screen_state_->next_action_suggestion.reset();

    // INF-05: honor a previous Esc dismissal — if the user closed the
    // popup for this exact input, don't reopen until the input changes.
    // (Suggestions were already cleared at the top of this function.)
    if (input == screen_state_->dismissed_autocomplete_for_input) {
        return;
    }
    screen_state_->dismissed_autocomplete_for_input.clear();

    // SL-03: derive inline argument hint for "/cmd ..." inputs (shown by
    // TextInputImpl after the prompt).
    screen_state_->pending_argument_hint.clear();
    if (input.starts_with('/') && static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())) {
        const auto sp = input.find(' ');
        if (sp != std::string::npos && sp > 1) {
            const std::string cmd_name = input.substr(1, sp - 1);
            if (const auto* def = static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->find_definition(cmd_name)) {
                if (!def->argument_hint.empty()) {
                    screen_state_->pending_argument_hint = def->argument_hint;
                }
            }
        }
    }

    // SL-05: mid-input slash ghost text — a "/prefix" token appearing
    // mid-input (input doesn't start with '/') completes inline to the
    // shortest matching command name.
    screen_state_->pending_ghost_text.clear();
    if (!input.starts_with('/') && token.text.starts_with('/') &&
        static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())) {
        const std::string partial = token.text.substr(1);
        if (!(partial.empty() || partial.find(' ') != std::string::npos)) {
            const CommandDefinition* best = nullptr;
            for (const auto* def : static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->visible_commands()) {
                if (def->name.size() >= partial.size() &&
                    def->name.compare(0, partial.size(), partial) == 0) {
                    if (!best || def->name.size() < best->name.size()) best = def;
                }
            }
            if (best && best->name.size() > partial.size()) {
                screen_state_->pending_ghost_text = best->name.substr(partial.size());
            }
        }
    }

    auto add_directory_suggestions = [&](std::string_view partial) {
        std::filesystem::path base = screen_state_->cwd.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(screen_state_->cwd);
        std::filesystem::path raw{std::string(partial)};
        std::filesystem::path parent = raw.has_parent_path()
            ? base / raw.parent_path()
            : base;
        const auto prefix = raw.has_parent_path()
            ? raw.parent_path().string() + "/"
            : std::string{};
        const auto leaf = raw.filename().string();

        struct DirCandidate { std::string display; std::string insert; int rank; };
        std::vector<DirCandidate> dirs;
        for (const auto& entry : completion_directory_entries(base, parent)) {
            if (!entry.is_directory) continue;
            const auto& name = entry.name;
            if (!frn::fuzzy_match_nucleo(name, leaf)) continue;
            auto insert = prefix + name + "/";
            dirs.push_back(DirCandidate{
                .display = insert,
                .insert = insert,
                .rank = frn::fuzzy_rank_nucleo(name, leaf),
            });
            if (dirs.size() >= 50) break;
        }
        std::ranges::sort(dirs, [](const auto& a, const auto& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.display < b.display;
        });
        for (auto& dir : dirs) {
            add_suggestion(
                std::move(dir.display),
                "Directory",
                std::move(dir.insert),
                token.start,
                token.end,
                false);
        }
    };

    auto add_session_suggestions = [&](std::string_view partial) {
        for (const auto& entry : completion_session_entries(sessions_dir())) {
            const auto& m = entry.metadata;
            const auto& id = m.session_id;
            const auto& title = m.title.value_or("");
            // Skip empty sessions — they are startup shells with no messages.
            // Count from the file on disk: metadata.message_count is 0 for
            // sessions that use messages.jsonl (the engine appends without
            // updating the metadata count).
            if (entry.message_count == 0) continue;
            if (!frn::fuzzy_match_nucleo(id, partial) &&
                !frn::fuzzy_match_nucleo(title, partial)) {
                continue;
            }
            // Short ID: UUID → first 8 chars; session_<ts>_<hex> → hex suffix.
            std::string short_id_str;
            if (id.starts_with("session_")) {
                auto pos = id.rfind('_');
                short_id_str = (pos != std::string::npos && pos + 1 < id.size())
                    ? id.substr(pos + 1, 8) : id.substr(0, 8);
            } else {
                short_id_str = id.substr(0, std::min<std::size_t>(id.size(), 8));
            }
            // Use the first user message as description when title is generic.
            std::string desc = title.empty() ? "Session" : title;
            if (desc.empty() || desc == "Session") {
                desc = entry.fallback_title;
            }
            add_suggestion(
                std::move(short_id_str),
                std::move(desc),
                id,
                token.start,
                token.end,
                true);
        }
    };

    const auto before_cursor = input.substr(0, cursor);
    if (before_cursor.starts_with("/add-dir ") ||
        before_cursor.starts_with("/add-dir\t")) {
        add_directory_suggestions(token.text);
        restore_index();
        return;
    }
    if (before_cursor.starts_with("/resume ") ||
        before_cursor.starts_with("/resume\t") ||
        before_cursor.starts_with("/r ") ||
        before_cursor.starts_with("/r\t")) {
        add_session_suggestions(token.text);
        restore_index();
        return;
    }

    // @history with-space trigger: user typed "@history <query>"
    if (before_cursor.starts_with("@history ") ||
        before_cursor.starts_with("@history\t")) {
        // Use everything after "@history " as the search query, not just
        // the token around cursor — multi-word history search should work.
        // Trim leading whitespace so "@history  " (extra spaces) is treated
        // the same as "@history " (single space) — both yield an empty query.
        std::string_view history_query(before_cursor);
        history_query.remove_prefix(9);  // len("@history ") = 9
        while (!history_query.empty() &&
               (history_query.front() == ' ' || history_query.front() == '\t')) {
            history_query.remove_prefix(1);
        }
        for (const auto& sug : acsrc::build_history_suggestions(
                 std::string(history_query), 0, cursor, 50)) {
            add_suggestion(sug.display_text, sug.description,
                sug.insert_text, sug.replacement_start, sug.replacement_end,
                sug.submit_on_return, sug.id, sug.icon, sug.color_name);
        }
        restore_index();
        return;
    }

    if (input.starts_with('/') &&
        cursor <= input.size() &&
        before_cursor.find_first_of(" \t\n") != std::string::npos &&
        static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())) {
        auto completions = static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->complete(before_cursor);
        for (auto& completion : completions) {
            const bool whole_command = completion.starts_with('/');
            add_suggestion(
                completion,
                "Command argument",
                whole_command ? completion + " " : completion,
                whole_command ? 0 : token.start,
                whole_command ? cursor : token.end,
                true);
        }
        if (!screen_state_->autocomplete_suggestions.empty()) {
            restore_index();
            return;
        }
    }

    if (token.text.starts_with('/')) {
        const auto query = std::string_view(token.text).substr(1);
        struct SlashCandidate {
            std::string display;
            std::string description;
            std::string insert;
            int rank = 0;
            bool submit = true;
            std::string id;  // INF-02: source-aware stable id
        };
        std::vector<SlashCandidate> candidates;

        if (static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())) {
            for (const auto* def : static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->visible_commands()) {
                if (!def) continue;
                // SL-02: multi-key match — name (exact/prefix/substring/subseq)
                // outranks a description-word match, so commands are still
                // surfaced when the user types a description term (Fuse
                // weights descriptionKey×0.5; cpp previously matched name only).
                int cmd_rank = -1;
                if (frn::fuzzy_match_nucleo(def->name, query)) {
                    cmd_rank = frn::fuzzy_rank_nucleo(def->name, query);
                } else {
                    const auto d = lowercase_ascii(def->description);
                    const auto q = lowercase_ascii(query);
                    if (!q.empty() && d.find(q) != std::string::npos) {
                        cmd_rank = 10;  // description match, lower priority
                    }
                }
                if (cmd_rank >= 0) {
                    // SL-06: only auto-execute (submit on Enter) commands that
                    // don't require arguments — commands with required args
                    // expand to "/cmd " so the user can type them. The
                    // shouldExecute gate is on argNames.length.
                    bool needs_args = false;
                    for (const auto& a : def->args) {
                        if (a.required) { needs_args = true; break; }
                    }
                    // SL-07: show the matched alias as a parenthetical on
                    // the CANONICAL row.
                    std::string matched_alias;
                    if (!query.empty() && !def->aliases.empty()) {
                        const auto q = lowercase_ascii(query);
                        for (const auto& alias : def->aliases) {
                            if (lowercase_ascii(alias).starts_with(q)) {
                                matched_alias = alias;
                                break;
                            }
                        }
                    }
                    std::string display = "/" + def->name;
                    if (!matched_alias.empty()) display += " (" + matched_alias + ")";
                    candidates.push_back(SlashCandidate{
                        .display = std::move(display),
                        .description = def->description,
                        .insert = "/" + def->name + " ",
                        .rank = cmd_rank,
                        .submit = !needs_args,
                        .id = "cmd:" + def->name,
                    });
                }
                for (const auto& alias : def->aliases) {
                    if (!frn::fuzzy_match_nucleo(alias, query)) continue;
                    candidates.push_back(SlashCandidate{
                        .display = "/" + alias,
                        .description = "Alias for /" + def->name,
                        .insert = "/" + alias + " ",
                        .rank = frn::fuzzy_rank_nucleo(alias, query) + 1,
                        .submit = true,
                        .id = "alias:" + alias,
                    });
                }
            }
        }

        for (const auto& skill : cached_skills_) {
            // SL-02: name match outranks a description-word match (Fuse
            // keeps descriptionKey at lower weight; cpp matched name only).
            int skill_rank = -1;
            if (frn::fuzzy_match_nucleo(skill.name, query)) {
                skill_rank = frn::fuzzy_rank_nucleo(skill.name, query) + 4;
            } else {
                const auto d = lowercase_ascii(skill.description);
                const auto q = lowercase_ascii(query);
                if (!q.empty() && d.find(q) != std::string::npos) skill_rank = 14;
            }
            if (skill_rank < 0) continue;
            // SL-04: recency boost — recently-used skills rank higher
            // (getSkillUsageScore). Bounded so fuzzy relevance still wins
            // on non-empty queries; on empty '/' all skills tie on fuzzy rank
            // so recency dominates, surfacing recent skills first.
            const int recency_bonus = static_cast<int>(
                std::min(loom::utils::skill_usage::get_skill_usage_score(skill.name), 3.0));
            candidates.push_back(SlashCandidate{
                .display = "/" + skill.name,
                .description = (skill.kind == "workflow")
                    ? std::format("[workflow] {} skill · {}", skill.source, skill.description)
                    : std::format("{} skill · {}", skill.source, skill.description),
                .insert = "/" + skill.name + " ",
                .rank = skill_rank - recency_bonus,
                .submit = true,
                .id = "skill:" + skill.name + ":" + skill.source,
            });
        }

        for (const auto& plugin_command : cached_plugin_commands_) {
            int plugin_rank = -1;
            if (frn::fuzzy_match_nucleo(plugin_command.command, query)) {
                plugin_rank = frn::fuzzy_rank_nucleo(plugin_command.command, query) + 6;
            } else {
                const auto d = lowercase_ascii(plugin_command.plugin_name);
                const auto q = lowercase_ascii(query);
                if (!q.empty() && d.find(q) != std::string::npos) plugin_rank = 16;
            }
            if (plugin_rank < 0) continue;
            candidates.push_back(SlashCandidate{
                .display = "/" + plugin_command.command,
                .description = "Plugin command · " + plugin_command.plugin_name,
                .insert = "/" + plugin_command.command + " ",
                .rank = plugin_rank,
                .submit = false,
                .id = "plugin:" + plugin_command.command + ":" + plugin_command.plugin_name,
            });
        }

        // SL-01: hidden-command exact-name escape hatch — if the user typed
        // the full name of a hidden command, surface it at the top.
        // visible_commands() otherwise hides them entirely.
        if (auto* hidden = static_cast<loom::commands::AppCommandRegistry*>(cmd_registry_raw())->hidden_command_if_exact(query)) {
            candidates.push_back(SlashCandidate{
                .display = "/" + hidden->name,
                .description = hidden->description,
                .insert = "/" + hidden->name + " ",
                .rank = -1000,  // force top (rank ascending = smaller first)
                .submit = true,
                .id = "hidden-cmd:" + hidden->name,
            });
        }

        std::ranges::sort(candidates, [](const auto& a, const auto& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.display < b.display;
        });
        // INF-03: precompute stable name-column width over ALL candidates
        // (not just the visible window) so the description column doesn't
        // jitter as the user filters. Slash names are ASCII, so size() is a
        // faithful width measure here.
        {
            int widest = 0;
            for (const auto& c : candidates) {
                widest = std::max(widest, static_cast<int>(c.display.size()));
            }
            screen_state_->autocomplete_stable_name_width = widest;
        }
        const std::size_t limit = std::min<std::size_t>(candidates.size(), 80);
        for (std::size_t i = 0; i < limit; ++i) {
            add_suggestion(
                std::move(candidates[i].display),
                std::move(candidates[i].description),
                std::move(candidates[i].insert),
                token.start,
                token.end,
                candidates[i].submit,
                std::move(candidates[i].id));
        }
        restore_index();
        return;
    }

    // INF-01/AT-08: @ mentions are suppressed in bash mode (the @DM/@file
    // branches are gated on non-bash mode);
    // in bash mode we fall through to the $PATH shell-command scan below.
    if (token.text.starts_with('@') &&
        !repl::effective_is_bash(*screen_state_)) {
        // Strip @" prefix and trailing " for quoted paths.
        std::string raw_query_text(token.text.substr(1));
        const bool is_quoted = !raw_query_text.empty() && raw_query_text.front() == '"';
        if (is_quoted) {
            raw_query_text.erase(raw_query_text.begin());  // strip leading "
            if (!raw_query_text.empty() && raw_query_text.back() == '"') {
                raw_query_text.pop_back();  // strip trailing "
            }
        }
        const std::string_view query(raw_query_text);

        // ── @history trigger: prompt history search ──────────────────────
        // When user types @history<query>, surface persisted prompt history.
        if (raw_query_text.starts_with("history")) {
            auto after = std::string_view(raw_query_text).substr(7);
            while (!after.empty() && (after.front() == ' ' || after.front() == '\t'))
                after.remove_prefix(1);
            for (const auto& sug : acsrc::build_history_suggestions(
                     std::string(after), token.start, token.end, 50)) {
                add_suggestion(sug.display_text, sug.description,
                    sug.insert_text, sug.replacement_start, sug.replacement_end,
                    sug.submit_on_return, sug.id, sug.icon, sug.color_name);
            }
            restore_index();
            return;
        }

        std::filesystem::path base = screen_state_->cwd.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(screen_state_->cwd);

        // Expand ~ before resolving the directory to scan.
        // The display prefix keeps the original unexpanded form so the user
        // sees @~/Documents not @/home/user/Documents.
        std::filesystem::path raw_path(raw_query_text);
        std::filesystem::path expanded_path = loom::utils::path::expand_tilde(raw_path);

        // Compute the display prefix (unexpanded, e.g. "~/src/")
        const auto prefix = raw_path.has_parent_path()
            ? raw_path.parent_path().string() + "/"
            : std::string{};

        // Compute the actual directory to scan (expanded, e.g. /home/user/src)
        std::filesystem::path parent;
        std::string leaf;
        if (expanded_path.has_parent_path()) {
            parent = expanded_path.parent_path();
            leaf = expanded_path.filename().string();
        } else {
            parent = expanded_path;  // e.g. "~" alone → home dir
            leaf.clear();
        }
        // If parent is still relative (e.g. "src" without ./), resolve against base
        if (!parent.is_absolute()) {
            parent = base / parent;
        }

        // AT-01: bare @-queries (no path separator) fuzzy-match the whole
        // repo via the file index, so "@readme" finds src/readme.md. The
        // directory_iterator block below still handles explicit path
        // browsing (@src/...).
        // ~/ ./ ../ / all trigger path completion.
        const bool has_separator = query.find('/') != std::string_view::npos ||
                                   query.find('\\') != std::string_view::npos;
        const bool is_path_like = has_separator ||
                                  query == "~" || query == "." || query == ".." ||
                                  query.starts_with("~/") || query.starts_with("./") ||
                                  query.starts_with("../");
        const bool bare_query = !is_path_like;

        // AT-05: bare-@ teammate DM precedence — when the query has no path
        // separator, teammate (native-agent) DM matches are shown EXCLUSIVELY
        // before file/agent/mcp (startsWith on lowercased name). With a
        // match we return immediately so the popup is DM-only.
        if (bare_query) {
            auto starts_with_ci = [](std::string_view name, std::string_view q) {
                if (name.size() < q.size()) return false;
                for (std::size_t i = 0; i < q.size(); ++i) {
                    if (std::tolower(static_cast<unsigned char>(name[i])) !=
                        std::tolower(static_cast<unsigned char>(q[i]))) {
                        return false;
                    }
                }
                return true;
            };
            struct DmCandidate { std::string display; std::string insert; std::string desc; std::string color; };
            std::vector<DmCandidate> dms;
            for (const auto& record : completion_native_agents(screen_state_->cwd)) {
                const auto& name = record.name;
                if (!starts_with_ci(name, query)) continue;
                dms.push_back(DmCandidate{
                    .display = "@" + std::string(name),
                    .insert = "@" + std::string(name) + " ",
                    .desc = "Teammate · " + record.status,
                    .color = record.color,
                });
            }
            if (!dms.empty()) {
                for (auto& d : dms) {
                    add_suggestion(std::move(d.display), std::move(d.desc),
                                   std::move(d.insert), token.start, token.end,
                                   false, "", "👤", std::move(d.color));
                }
                restore_index();
                return;  // exclusive: bare-@ with teammate match shows DMs only
            }
        }

        // AT-06: skip the file index when the query is empty (bare "@") —
        // On an empty @ query, show teammates/MCP/agents, not every file
        // in the repo. With a non-empty query the index fuzzy-matches.
        if (bare_query && !query.empty()) {
            struct RepoCandidate { std::string display; std::string insert; std::string desc; int rank; std::string icon; };
            std::vector<RepoCandidate> repos;
            for (const auto& rel : fidx::collect_repo_files(screen_state_->cwd)) {
                const std::size_t slash = rel.find_last_of("/\\");
                const std::string base = (slash == std::string_view::npos)
                    ? rel : rel.substr(slash + 1);
                const bool match_base = frn::fuzzy_match_nucleo(base, query);
                const bool match_path = !match_base && frn::fuzzy_match_nucleo(rel, query);
                if (!match_base && !match_path) continue;
                // Files get a file icon. Directories (trailing /) get a
                // directory icon.
                const bool is_dir = !rel.empty() && rel.back() == '/';
                repos.push_back(RepoCandidate{
                    .display = "@" + rel,
                    .insert = "@" + rel + " ",
                    .desc = is_dir ? "Directory" : "File",
                    .rank = frn::fuzzy_rank_nucleo(base, query) + (match_base ? 0 : 2),
                    .icon = is_dir ? "📁" : "📄",
                });
                if (repos.size() >= 50) break;  // cap at 50 for terminal
            }
            std::ranges::sort(repos, [](const auto& a, const auto& b) {
                if (a.rank != b.rank) return a.rank < b.rank;
                return a.display < b.display;
            });
            for (auto& r : repos) {
                add_suggestion(std::move(r.display), std::move(r.desc),
                               std::move(r.insert), token.start, token.end,
                               false, "", std::move(r.icon));
            }
        } else {
            struct FileCandidate { std::string display; std::string insert; std::string desc; int rank; std::string icon; };
            std::vector<FileCandidate> files;
            for (const auto& entry : completion_directory_entries(base, parent)) {
                const auto& name = entry.name;
                if (name.starts_with(".")) continue;
                if (!frn::fuzzy_match_nucleo(name, leaf)) continue;
                const bool is_dir = entry.is_directory;
                auto rel = prefix + name + (is_dir ? "/" : "");
                // Directories get "/" suffix so user can continue browsing.
                // Icons: 📄 for files, 📁 for directories.
                files.push_back(FileCandidate{
                    .display = "@" + rel,
                    .insert = is_quoted
                        ? std::format("@\"{}\"", rel)  // re-wrap in quotes
                        : "@" + rel,
                    .desc = is_dir ? "Directory" : "File",
                    .rank = frn::fuzzy_rank_nucleo(name, leaf),
                    .icon = is_dir ? "📁" : "📄",
                });
                if (files.size() >= 50) break;  // cap at 50 results
            }
            std::ranges::sort(files, [](const auto& a, const auto& b) {
                if (a.rank != b.rank) return a.rank < b.rank;
                return a.display < b.display;
            });
            for (auto& file : files) {
                add_suggestion(
                    std::move(file.display),
                    std::move(file.desc),
                    std::move(file.insert),
                    token.start,
                    token.end,
                    false,
                    "",
                    std::move(file.icon));
            }
        }

        // ── Agent / teammate autocomplete ─────────────────────────────
        // Agent defs with color + truncated when_to_use; teammate DMs with
        // status, prefix-matched on lowercased name.
        // Cache source formatting; filter names and apply the current token
        // replacement range on every input event.
        for (const auto& sug : completion_agent_suggestions(screen_state_->cwd)) {
            if (!frn::fuzzy_match_nucleo(
                    std::string_view(sug.display_text).substr(1), query)) continue;
            add_suggestion(sug.display_text, sug.description,
                sug.insert_text, token.start, token.end,
                sug.submit_on_return, sug.id, sug.icon, sug.color_name);
        }
        // MCP server resources (list_native_mcp_resources) are collected
        // and passed to the unified @-mention autocomplete dropdown
        // alongside files, agents, skills, and history.  The display name
        // is shown in the picker; insert_text is what gets injected into
        // the prompt buffer on accept.
        for (const auto& resource : acsrc::collect_mcp_resource_suggestions()) {
            if (!frn::fuzzy_match_nucleo(resource.display, query) &&
                !frn::fuzzy_match_nucleo(resource.insert_text, query)) {
                continue;
            }
            add_suggestion(
                "@" + resource.display,
                resource.description,
                "@" + resource.insert_text + " ",
                token.start,
                token.end,
                false,
                "",
                "🔌");
        }

        // AT-10: @-history session autocomplete — search past sessions by
        // title/ID, exposed as an @-mention source alongside
        // files/agents/MCP. Sessions are sorted by recency (newest first)
        // per loom::session::list_recent_sessions.
        {
            const auto& entries = completion_session_entries(sessions_dir());
            const auto limit = std::min<std::size_t>(entries.size(), 30);
            for (std::size_t i = 0; i < limit; ++i) {
                const auto& entry = entries[i];
                const auto& m = entry.metadata;
                const auto& id = m.session_id;
                const auto& title = m.title.value_or("");
                const std::string short_id =
                    id.substr(0, std::min<std::size_t>(id.size(), 8));
                const std::string display_label =
                    title.empty() ? short_id : title;

                if (!frn::fuzzy_match_nucleo(display_label, query) &&
                    !frn::fuzzy_match_nucleo(id, query) &&
                    !frn::fuzzy_match_nucleo(short_id, query)) {
                        continue;
                    }

                // Build description: "Session" + short-ID hint + msg count.
                // Count from the file on disk: metadata.message_count is 0
                // for sessions that use messages.jsonl.
                const int msg_count = entry.message_count;
                std::string desc = "Session";
                if (!title.empty() && title != short_id) {
                    desc += " · " + short_id;
                }
                if (msg_count > 0) {
                    desc += std::format(" ({} msgs)", msg_count);
                }

                add_suggestion(
                    "@" + display_label,
                    std::move(desc),
                    "@" + id,
                    token.start,
                    token.end,
                    /*submit_on_return=*/false,
                    /*id=*/"session:" + id,
                    /*icon=*/"🕘");
            }
        }

        // AT-11: @-history prompt autocomplete — show recent prompt history
        // entries alongside sessions.  Yields HistoryEntry objects for the
        // current project, newest-first, deduped by display text.
        // The @history with-space trigger (above) shows prompts too, but
        // the bare-@ section surfaces them inline so users can @-mention
        // a recent prompt without typing @history first.
        {
            auto prompt_hist = acsrc::collect_history_suggestions(query, 20);
            for (const auto& entry : prompt_hist) {
                std::string display = entry.prompt_text;
                if (display.size() > 80) display = display.substr(0, 77) + "...";

                // Build description with relative time + session hint
                std::string desc = "Prompt";
                if (entry.timestamp_ms > 0) {
                    using namespace std::chrono;
                    auto now = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch()).count();
                    auto delta_sec = (now - entry.timestamp_ms) / 1000;
                    if (delta_sec < 60) desc += " · just now";
                    else if (delta_sec < 3600) desc += std::format(" · {}m ago", delta_sec / 60);
                    else if (delta_sec < 86400) desc += std::format(" · {}h ago", delta_sec / 3600);
                    else desc += std::format(" · {}d ago", delta_sec / 86400);
                }
                if (!entry.session_id.empty()) {
                    desc += std::format(" · {}", entry.session_id.substr(0, 8));
                }

                // Insert: the full prompt text so accepting it pastes the
                // complete previous prompt into the input.
                add_suggestion(
                    "@" + display,
                    std::move(desc),
                    entry.full_text,
                    token.start,
                    token.end,
                    /*submit_on_return=*/false,
                    /*id=*/"prompt-history:" + std::to_string(entry.timestamp_ms),
                    /*icon=*/"📝");
            }
        }

        restore_index();
        return;
    }

    // INF-01/AT-08: # channels are suppressed in bash mode (the #-channel
    // branch is gated on prompt mode); in bash mode we fall through to the
    // $PATH shell-command scan below.
    if (token.text.starts_with('#') &&
        !repl::effective_is_bash(*screen_state_)) {
        const auto query = std::string_view(token.text).substr(1);
        // Channel-like MCP resources (name starts with '#' or uri contains
        // "channel"/"slack") are passed to the #-channel autocomplete
        // dropdown.  This is how MCP server channels surface in the
        // #-mention picker alongside native Slack channels.
        for (const auto& resource : acsrc::collect_mcp_resource_suggestions()) {
            if (!resource.channel_like) continue;
            const auto display = resource.display.starts_with("#")
                ? resource.display
                : "#" + resource.display;
            if (!frn::fuzzy_match_nucleo(display, token.text) &&
                !frn::fuzzy_match_nucleo(resource.insert_text, query)) {
                    continue;
                }
            add_suggestion(
                display,
                resource.description,
                display + " ",
                token.start,
                token.end,
                false,
                "",
                "💬");
        }
        restore_index();
        return;
    }

    if (repl::effective_is_bash(*screen_state_) && !token.text.empty()) {
        if (const char* path_env = std::getenv("PATH")) {
            const auto base = std::filesystem::current_path();
            for (const auto& name : completion_shell_commands(base, path_env)) {
                if (!frn::fuzzy_match_nucleo(name, token.text)) continue;
                add_suggestion(
                    name,
                    "Shell command",
                    name + " ",
                    token.start,
                    token.end,
                    false,
                    "",
                    "▶_");
                if (screen_state_->autocomplete_suggestions.size() >= 50) break;
            }
        }
        restore_index();
    }
}

}  // namespace loom::ui
