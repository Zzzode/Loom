// messages_list_filter.cpp - impl unit for loom.ui.messages.messages_list
// (RFC 0001 Phase C batch 7). Brief-mode filtering, drop-text mask, expand
// keys, the lowered/cached search-text accessor, shape categories and the
// O(N) build_visible_rows walk. No FTXUI types are named here.
//
// Phase A (#184957): std is TEXTUAL here on purpose — no `import std;`.
// With an empty GMF, this impl unit of a primary whose own GMF pulls libc++
// textually (via FTXUI) made clang 22's reduced-BMI writer emit a duplicate
// aligned operator new ("call to 'operator new' is ambiguous" in
// std::__libcpp_allocate). Impl units that textually include FTXUI
// themselves are unaffected; this one names no FTXUI type, so the
// app_extra_methods.cpp fallback applies.
module;

#include <cstddef>
#include <cstdint>

module loom.ui.messages.messages_list;

import std;

import loom.ui.foundation.clock;
import loom.serdes.json;
import loom.ui.messages.message_row;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.thinking_message;
import loom.ui.messages.tool_use_message;

namespace loom::ui::messages_list {

namespace brief_detail {

[[nodiscard]] bool is_brief_tool_name(std::string_view name) {
    for (auto tn : kBriefToolNames) {
        if (name == tn) return true;
    }
    return false;
}

/// Returns true if the tool name triggers dropTextInBriefTurns.
[[nodiscard]] bool is_drop_text_tool_name(std::string_view name) {
    for (auto tn : kDropTextToolNames) {
        if (name == tn) return true;
    }
    return false;
}

/// Extract tool_name from a MessageRowPayload if it is a tool_use or
/// tool_result variant.  Returns empty string otherwise.
[[nodiscard]] std::string_view extract_tool_name(const MessageRowPayload& p) {
    if (auto* opts = std::get_if<::loom::ui::messages::tool_use_message::ToolUseRenderOptions>(&p)) {
        return opts->call.tool_name;
    }
    if (auto* grp = std::get_if<::loom::ui::messages::tool_use_message::GroupedToolsOptions>(&p)) {
        if (!grp->calls.empty()) return grp->calls[0].tool_name;
    }
    if (auto* tro = std::get_if<::loom::ui::messages::ToolResultOptions>(&p)) {
        return tro->tool_name;
    }
    return {};
}

/// Failed calls and their results are actionable diagnostics, not background
/// activity. Keep them visible even when neighboring successful tools collapse.
[[nodiscard]] bool is_failed_tool_row(const MessageRowPayload& payload) {
    namespace tu = ::loom::ui::messages::tool_use_message;
    const auto failed_call = [](const tu::ToolUseCallData& call) {
        return call.status == tu::ToolStatus::Error ||
               call.status == tu::ToolStatus::Cancelled;
    };
    if (const auto* opts = std::get_if<tu::ToolUseRenderOptions>(&payload)) {
        return failed_call(opts->call);
    }
    if (const auto* group = std::get_if<tu::GroupedToolsOptions>(&payload)) {
        return std::any_of(group->calls.begin(), group->calls.end(), failed_call);
    }
    if (const auto* result = std::get_if<::loom::ui::messages::ToolResultOptions>(&payload)) {
        return result->status != ::loom::ui::messages::ToolResultStatus::Success;
    }
    return false;
}

}  // namespace brief_detail

/// Returns true if the row at index `i` should be VISIBLE in brief mode.
auto passes_brief_filter(
    MessageShape shape,
    const MessageRowPayload& payload) -> bool {
    using S = MessageShape;
    namespace bd = brief_detail;

    if (bd::is_failed_tool_row(payload)) return true;

    switch (shape) {
        // System rows: always visible (system messages must stay visible
        // for user feedback; api_metrics subtype dropped but not tagged in CPP)
        case S::SystemText:
        case S::SystemRateLimit:
        case S::SystemPlanApproval:
        case S::SystemHookProgress:
        case S::SystemShutdown:
        case S::SystemCompactBoundary:
        case S::SystemAdvisor:
        case S::SystemTaskAssignment:
        case S::SystemCollapsedContent:
        case S::SystemAPIError:
            return true;

        // Assistant rows: only brief tool uses are visible
        case S::AssistantToolUse:
        case S::AssistantGroupedTools:
            return bd::is_brief_tool_name(bd::extract_tool_name(payload));

        // Assistant text + thinking: hidden in brief mode
        case S::AssistantText:
        case S::AssistantThinking:
        case S::AssistantRedactedThinking:
            return false;

        // User input: always visible (real user text + command chips)
        case S::UserText:
        case S::UserPrompt:
        case S::UserCommand:
        case S::UserBashInput:
        case S::UserBashOutput:
        case S::UserLocalCommandOutput:
        case S::UserLocalJsxOutput:
        case S::UserImage:
        case S::UserTeammate:
        case S::UserChannel:
        case S::UserAgentNotification:
        case S::UserMemoryInput:
        case S::UserPlan:
        case S::UserResourceUpdate:
        case S::UserAttachments:
            return true;

        // Tool results: only brief-tool results visible
        case S::UserToolResult:
            return bd::is_brief_tool_name(bd::extract_tool_name(payload));

        default:
            return true;
    }
}

// ---------------------------------------------------------------------------
// dropTextInBriefTurns.
//
// In default mode (neither transcript nor brief-only), drops assistant TEXT
// rows in turns that called a Brief/SendUserMessage/SendUserFile tool.  The
// model's text output is redundant with the SendUserMessage content it wrote
// right after — dropping it keeps the transcript focused on tool output.
//
// Per-turn: only drops text in turns that actually called a Brief tool.  If
// the model forgets to call Brief, text still shows — otherwise the user
// would see nothing for that turn.
//
// Returns a vector<bool> mask (true = KEEP the row, false = DROP it).
// ---------------------------------------------------------------------------
[[nodiscard]] std::vector<bool> compute_drop_text_mask(
    const std::vector<MessageShape>& shapes,
    const std::vector<MessageRowPayload>& payloads)
{
    using S = MessageShape;
    namespace bd = brief_detail;

    const auto N = shapes.size();
    std::vector<bool> keep(N, true);   // default: keep everything

    // First pass: find which turns contain a Brief tool_use.
    std::set<std::size_t> turns_with_brief;
    // text_index_to_turn[i] = turn number for assistant text at index i.
    std::vector<std::optional<std::size_t>> text_index_to_turn(N, std::nullopt);

    std::size_t turn = 0;
    for (std::size_t i = 0; i < N; ++i) {
        const auto sh = shapes[i];
        // Real user message (non-tool_result) → advance turn counter.
        const bool is_real_user =
            (sh == S::UserText || sh == S::UserPrompt || sh == S::UserCommand ||
             sh == S::UserBashInput || sh == S::UserBashOutput ||
             sh == S::UserLocalCommandOutput || sh == S::UserLocalJsxOutput ||
             sh == S::UserImage || sh == S::UserPlan || sh == S::UserResourceUpdate ||
             sh == S::UserMemoryInput || sh == S::UserChannel ||
             sh == S::UserAgentNotification || sh == S::UserTeammate ||
             sh == S::UserAttachments);
        if (is_real_user) {
            ++turn;
            continue;
        }
        if (sh == S::AssistantText) {
            text_index_to_turn[i] = turn;
        } else if ((sh == S::AssistantToolUse || sh == S::AssistantGroupedTools) &&
                   i < payloads.size()) {
            if (bd::is_drop_text_tool_name(bd::extract_tool_name(payloads[i]))) {
                turns_with_brief.insert(turn);
            }
        }
    }

    if (turns_with_brief.empty()) return keep;   // no brief turns → keep all

    // Second pass: mark assistant text rows for dropping if their turn had Brief.
    for (std::size_t i = 0; i < N; ++i) {
        if (text_index_to_turn[i].has_value() &&
            turns_with_brief.count(*text_index_to_turn[i]) > 0) {
            keep[i] = false;
        }
    }
    return keep;
}

// ---------------------------------------------------------------------------
// Expand-key computation
//
// For tool_use and tool_result rows, returns the tool_name so a tool_use
// and its corresponding tool_result share the same key and expand together.
// For other rows, returns the uuid (or empty string if not available).
// ---------------------------------------------------------------------------
[[nodiscard]] std::string compute_expand_key(
    MessageShape shape,
    const MessageRowPayload& payload,
    std::string_view uuid) {
    using S = MessageShape;
    namespace bd = brief_detail;

    if (shape == S::AssistantToolUse || shape == S::AssistantGroupedTools ||
        shape == S::UserToolResult) {
        auto name = bd::extract_tool_name(payload);
        if (!name.empty()) return std::string(name);
    }
    // Fallback: use uuid (first 24 chars to match deriveUUID prefix)
    if (!uuid.empty()) return std::string(uuid.substr(0, 24));
    return {};
}

/// Returns true if the row supports click-to-expand: tool results that are
/// truncated, collapsed read/search groups, or advisor tool results.
[[nodiscard]] bool is_row_clickable(
    MessageShape shape,
    const MessageRowPayload& payload) {
    using S = MessageShape;
    namespace bd = brief_detail;

    // Collapsed content groups: always clickable
    if (shape == S::SystemCollapsedContent) return true;

    // Tool results: clickable if they have content to expand/collapse.
    // The UI truncates long output to 10 lines by default, so any tool
    // result with content is potentially expandable.
    if (shape == S::UserToolResult) {
        if (auto* opts = std::get_if<::loom::ui::messages::ToolResultOptions>(&payload)) {
            return opts->is_truncated ||
                   (opts->output && !opts->output->empty()) ||
                   (opts->content_items && !opts->content_items->empty());
        }
    }

    // Tool uses: clickable if they have a result preview (means they have content
    // that could be expanded)
    if (shape == S::AssistantToolUse) {
        if (auto* opts = std::get_if<::loom::ui::messages::tool_use_message::ToolUseRenderOptions>(&payload)) {
            return opts->call.result_preview.has_value() &&
                   !opts->call.result_preview->empty();
        }
    }

    // Thinking blocks: clickable if they have content to expand.
    // Redacted thinking is excluded — the placeholder has nothing to reveal.
    if (shape == S::AssistantThinking) {
        if (auto* opts = std::get_if<
                ::loom::ui::messages::thinking_message::ThinkingMessageOptions>(
                &payload)) {
            return !opts->data.raw_text.empty() ||
                   !opts->data.sections.empty();
        }
    }

    return false;
}

/// Returns true if the row at index `i` is currently "expanded" (user has
/// toggled it open via Enter/Space).  Expanded rows render with verbose=true
/// showing full content instead of truncated summaries.
[[nodiscard]] bool is_row_expanded(
    const MessagesListInput& input,
    std::size_t row_idx) {
    if (input.expanded_keys.empty()) return false;
    if (row_idx >= input.shapes.size() || row_idx >= input.rows.size()) return false;
    std::string_view uuid = (row_idx < input.uuids.size())
        ? std::string_view(input.uuids[row_idx]) : std::string_view{};
    auto key = compute_expand_key(input.shapes[row_idx], input.rows[row_idx], uuid);
    if (key.empty()) return false;
    return input.expanded_keys.count(key) > 0;
}

namespace detail {

/// Lower-case a UTF-8 string in place.  Only touches ASCII letters because
/// MessageRowPayload text fields are overwhelmingly English / path literals
/// (same strategy as renderableSearchText → toLowerCase).
auto lowered(std::string s) -> std::string {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    }
    return s;
}

// =========================================================================
// 2c)  Cached lowered search text accessor
// =========================================================================
//
// Returns the LOWERED rich searchable text for row_idx, using the per-row
// cache on MessagesListInput.  First call computes + caches; subsequent
// calls (build_visible_rows then visible_rows_to_virtual) hit the cache
// with zero alloc.
//
// Cache is keyed by row_idx (parallel to input.rows) — messages are
// append-only and immutable, so a cached entry is always valid.
[[nodiscard]] auto get_cached_lowered_search_text(
    const MessagesListInput& input,
    std::size_t row_idx,
    MessageShape shape) -> std::string
{
    if (row_idx >= input.rows.size()) return {};

    // Grow cache to match rows size on first access.
    if (input.lowered_search_cache.size() <= row_idx) {
        input.lowered_search_cache.resize(input.rows.size());
    }

    auto& slot = input.lowered_search_cache[row_idx];
    if (slot.has_value()) {
        return *slot;   // cache hit — zero alloc
    }

    // Cache miss: compute + store.  extract_search_text does the 2-tier
    // lookup (tool-owned extract_search_text preferred, payload_preview
    // fallback); we lower once here and cache the result.
    std::string lowered_text = detail::lowered(
        detail::extract_search_text(input.rows[row_idx], shape));
    slot = lowered_text;
    return lowered_text;
}

/// Returns true for message SHAPEs that belong to each filter category
/// (system / tool_use / tool_result / thinking / compacted).
auto shape_category(MessageShape s) -> std::string_view {
    switch (s) {
        // system family
        case MessageShape::SystemText:
        case MessageShape::SystemRateLimit:
        case MessageShape::SystemPlanApproval:
        case MessageShape::SystemHookProgress:
        case MessageShape::SystemShutdown:
        case MessageShape::SystemCompactBoundary:
        case MessageShape::SystemAdvisor:
        case MessageShape::SystemTaskAssignment:
        case MessageShape::SystemCollapsedContent:
        case MessageShape::SystemAPIError:
            return "system";
        // tool-in (assistant issuing a tool_use)
        case MessageShape::AssistantToolUse:
        case MessageShape::AssistantGroupedTools:
            return "tool_in";
        // tool-out (user returning tool_result, bash, local cmd)
        case MessageShape::UserToolResult:
        case MessageShape::UserBashInput:
        case MessageShape::UserBashOutput:
        case MessageShape::UserLocalCommandOutput:
            return "tool_out";
        // thinking
        case MessageShape::AssistantThinking:
        case MessageShape::AssistantRedactedThinking:
            return "thinking";
        default:
            return "other";
    }
}

auto passes_filters(MessageShape s, const Filters& f) -> bool {
    auto cat = shape_category(s);
    if (cat == "system"   && !f.show_system)   return false;
    if (cat == "tool_in"  && !f.show_tool_in)  return false;
    if (cat == "tool_out" && !f.show_tool_out) return false;
    if (cat == "thinking" && !f.show_thinking) return false;
    return true;
}

} // namespace detail

// Shapes that can participate in a compressed chain: thinking blocks,
// tool calls, tool results, grouped tools, bash I/O.  Everything else
// (user messages, assistant text, system rows, update tools) stops the chain.
[[nodiscard]] auto is_chain_compressible(MessageShape sh) -> bool {
    return sh == MessageShape::AssistantThinking ||
           sh == MessageShape::AssistantRedactedThinking ||
           sh == MessageShape::AssistantToolUse ||
           sh == MessageShape::AssistantGroupedTools ||
           sh == MessageShape::UserToolResult ||
           sh == MessageShape::UserBashInput ||
           sh == MessageShape::UserBashOutput;
}

// Returns true if the row is an update tool (Edit/Write) — either the
// tool_use itself or its tool_result.  Update tools are never compressed;
// they stop the chain and are shown individually.
[[nodiscard]] auto is_update_tool_row(
    MessageShape sh, const MessageRowPayload& payload) -> bool {
    if (sh == MessageShape::AssistantToolUse) {
        return tool_use_message::is_file_edit_tool(
            brief_detail::extract_tool_name(payload));
    }
    if (sh == MessageShape::AssistantGroupedTools) {
        if (auto* grp = std::get_if<
                tool_use_message::GroupedToolsOptions>(&payload)) {
            for (const auto& call : grp->calls) {
                if (tool_use_message::is_file_edit_tool(call.tool_name))
                    return true;
            }
        }
        return false;
    }
    if (sh == MessageShape::UserToolResult) {
        return tool_use_message::is_file_edit_tool(
            brief_detail::extract_tool_name(payload));
    }
    return false;
}

// UI-only stage timing follows raw material boundaries, independently of filters.
[[nodiscard]] bool is_chain_row(MessageShape shape, const MessageRowPayload& row) {
    return is_chain_compressible(shape) && !is_update_tool_row(shape, row) &&
           !brief_detail::is_failed_tool_row(row);
}

[[nodiscard]] auto earliest_row_start(const MessageRowPayload& row)
    -> std::chrono::steady_clock::time_point {
    using namespace tool_use_message;
    if (const auto* thinking = std::get_if<thinking_message::ThinkingMessageOptions>(&row))
        return thinking->data.thinking_start_time;
    if (const auto* tool = std::get_if<ToolUseRenderOptions>(&row))
        return tool->call.start_time;
    std::chrono::steady_clock::time_point earliest{};
    if (const auto* group = std::get_if<GroupedToolsOptions>(&row)) {
        for (const auto& call : group->calls) {
            if (call.start_time != decltype(earliest){} &&
                (earliest == decltype(earliest){} || call.start_time < earliest))
                earliest = call.start_time;
        }
    }
    return earliest;
}

[[nodiscard]] auto update_chain_timings(const MessagesListInput& input)
    -> std::vector<const ChainTiming*> {
    const auto now = clock::steady_now();
    const auto count = std::min(input.rows.size(), input.shapes.size());
    std::vector<const ChainTiming*> timing_for_row(input.rows.size(), nullptr);
    auto previous = std::move(input.chain_timings);
    input.chain_timings.clear();
    input.chain_timings.reserve(previous.size());
    // Reserve all old IDs before assigning any new ones: replacement must not
    // steal a surviving stage's disclosure key later in this same projection.
    std::unordered_set<std::string> reserved;
    for (const auto& [key, timing] : previous) {
        if (!timing.disclosure_id.empty()) reserved.insert(timing.disclosure_id);
    }
    std::unordered_map<std::string, std::size_t> occurrences;
    std::string boundary = "initial";
    for (std::size_t i = 0; i < count;) {
        if (!is_chain_row(input.shapes[i], input.rows[i])) {
            const auto text = detail::payload_preview(input.rows[i]);
            const auto base = std::to_string(static_cast<int>(input.shapes[i])) +
                ":" + std::to_string(text.size()) + ":" + text;
            boundary = base + ":" + std::to_string(occurrences[base]++);
            ++i;
            continue;
        }
        const auto begin = i;
        auto earliest = std::chrono::steady_clock::time_point{};
        while (i < count && is_chain_row(input.shapes[i], input.rows[i])) {
            const auto start = earliest_row_start(input.rows[i]);
            if (start != decltype(start){} && start <= now &&
                (earliest == decltype(earliest){} || start < earliest))
                earliest = start;
            ++i;
        }
        auto& timing = input.chain_timings[boundary];
        if (const auto found = previous.find(boundary); found != previous.end())
            timing = std::move(found->second);
        if (timing.disclosure_id.empty() && begin < input.uuids.size() &&
            !input.uuids[begin].empty()) {
            const auto& anchor = input.uuids[begin];
            auto candidate = anchor;
            std::size_t suffix = 0;
            while (reserved.contains(candidate))
                candidate = anchor + ":stage:" + std::to_string(++suffix);
            timing.disclosure_id = candidate;
            reserved.insert(std::move(candidate));
        }
        const bool live = input.query_running && i == count;
        if (!timing.finished && (live || timing.elapsed.has_value())) {
            if (!timing.elapsed) {
                timing.started_at = earliest == decltype(earliest){} ? now : earliest;
                timing.elapsed = std::chrono::milliseconds::zero();
            } else if (earliest != decltype(earliest){} && earliest < timing.started_at) {
                timing.started_at = earliest;
            }
            timing.elapsed = std::max(*timing.elapsed,
                std::chrono::duration_cast<std::chrono::milliseconds>(now - timing.started_at));
            if (!live) timing.finished = true;
        }
        for (auto r = begin; r < i; ++r) timing_for_row[r] = &timing;
    }
    return timing_for_row;
}

// Keep only public display arguments; unknown tools never spill arbitrary JSON.
[[nodiscard]] std::string current_tool_description(
    const tool_use_message::ToolUseCallData& call) {
    const auto document = loom::utils::json::parse(call.raw_parameters);
    const auto field = [&](std::string_view name) {
        if (!document) return std::string{};
        const auto value = document->root().get(name);
        return value.is_str() ? std::string(value.as_str()) : std::string{};
    };
    std::string target;
    if (call.tool_name == "Bash") target = field("command");
    else if (call.tool_name == "Read") {
        target = field("file_path");
        if (target.empty()) target = field("path");
    } else if (call.tool_name == "Grep" || call.tool_name == "Glob") {
        target = field("pattern");
        const auto path = field("path");
        if (!path.empty()) target += " in " + path;
    } else if (call.tool_name == "WebSearch") target = field("query");
    else if (call.tool_name == "WebFetch") target = field("url");
    return call.tool_name + (target.empty() ? "" : " " + target);
}

[[nodiscard]] std::string current_chain_activity(
    const MessagesListInput& input, std::size_t begin, std::size_t end,
    const std::vector<bool>& row_passes, const std::vector<bool>& consumed_by_group) {
    using namespace tool_use_message;
    std::vector<const ToolUseCallData*> running, pending;
    const auto add = [&](const ToolUseCallData& call) {
        if (call.status == ToolStatus::Running) running.push_back(&call);
        else if (call.status == ToolStatus::Pending) pending.push_back(&call);
    };
    for (auto i = begin; i < end; ++i) {
        if (!row_passes[i] || consumed_by_group[i]) continue;
        if (const auto* tool = std::get_if<ToolUseRenderOptions>(&input.rows[i])) add(tool->call);
        else if (const auto* group = std::get_if<GroupedToolsOptions>(&input.rows[i]))
            for (const auto& call : group->calls) add(call);
    }
    const auto& active = running.empty() ? pending : running;
    if (active.empty()) return "Waiting for model";
    std::string result = running.empty() ? "Preparing " : "Running ";
    for (std::size_t i = 0; i < std::min<std::size_t>(2, active.size()); ++i) {
        if (i) result += "; ";
        result += current_tool_description(*active[i]);
    }
    if (active.size() > 2) result += "; +" + std::to_string(active.size() - 2) + " more";
    return result;
}

// Add one tool call to the breakdown vector (increment count or append).
void add_tool_to_breakdown(
    std::vector<std::pair<std::string, std::size_t>>& breakdown,
    std::string_view name) {
    if (name.empty()) return;
    for (auto& [n, c] : breakdown) {
        if (n == name) { ++c; return; }
    }
    breakdown.emplace_back(std::string(name), 1);
}

// Count tool calls in a row and add them to the breakdown.
void count_row_tools(
    MessageShape sh, const MessageRowPayload& payload,
    std::vector<std::pair<std::string, std::size_t>>& breakdown) {
    if (sh == MessageShape::AssistantToolUse) {
        add_tool_to_breakdown(breakdown,
            brief_detail::extract_tool_name(payload));
    } else if (sh == MessageShape::AssistantGroupedTools) {
        if (auto* grp = std::get_if<
                tool_use_message::GroupedToolsOptions>(&payload)) {
            for (const auto& call : grp->calls) {
                add_tool_to_breakdown(breakdown, call.tool_name);
            }
        }
    }
}

// =========================================================================
// 4)  build_visible_rows — linear stage tracking and display filtering
// =========================================================================
/// Step 1 : per-row filter + search (Filters + search_query)
/// Step 2 : if show_compact, collapse compact_boundary_groups into a
///          single "CompactGroup" visible row each.
/// Step 2.5: compress thinking + non-update-tool chains into a single
///          "CompressedChain" summary row (skipped in transcript/search mode).
///
/// Linear passes over rows plus material-boundary text and compact groups.
/// Stage state is input-owned; rendering never mutates source payloads.
auto build_visible_rows(const MessagesListInput& input) -> std::vector<VisibleRow> {
    const auto N = input.rows.size();
    input.chain_update_due.reset();
    const auto chain_timings = update_chain_timings(input);
    // ---- Step 0 : pre-compute lowered search needle (empty = skip search)
    const std::string needle = detail::lowered(input.search_query);
    const bool do_search     = !needle.empty();

    // ---- Step 0b : pre-compute dropTextInBriefTurns mask for default mode.
    //              Only needed when NOT in transcript mode AND NOT in brief mode.
    std::vector<bool> drop_text_keep_mask;
    const bool apply_drop_text = !input.is_transcript_mode && !input.is_brief_mode;
    if (apply_drop_text && N > 0) {
        drop_text_keep_mask = compute_drop_text_mask(input.shapes, input.rows);
    }

    // ---- Step 1 : mark rows that pass (filters AND search AND 3-tier filter).
    //              Separately build a "row visible" bitmask so Step 2 can use it.
    //
    // 3-tier filter:
    //   Tier 1 (transcript mode): show ALL message types — bypass brief/dropText.
    //   Tier 2 (brief-only):     only brief tool chain + user input + system.
    //   Tier 3 (default):        drop assistant text in turns that called Brief
    //                            (dropTextInBriefTurns), keep everything else.
    std::vector<bool> row_passes(N, false);
    for (std::size_t i = 0; i < N; ++i) {
        if (i >= input.shapes.size()) break;   // malformed input → safe stop
        if (!detail::passes_filters(input.shapes[i], input.filters)) continue;

        // Tier 1: transcript mode — skip brief/dropText filters entirely.
        if (!input.is_transcript_mode) {
            if (input.is_brief_mode) {
                // Tier 2: brief-only — filterForBriefTool.
                if (i < input.rows.size() &&
                    !passes_brief_filter(input.shapes[i], input.rows[i])) {
                    continue;
                }
            } else if (apply_drop_text && i < drop_text_keep_mask.size()) {
                // Tier 3: default — dropTextInBriefTurns.
                if (!drop_text_keep_mask[i]) continue;
            }
        }

        if (do_search) {
            // 2-tier search text extraction with cache.  Use cached accessor:
            // first call computes + caches lowered rich text; subsequent calls
            // (visible_rows_to_virtual scroll_search) hit the cache with zero
            // alloc.
            const std::string hay = detail::get_cached_lowered_search_text(
                input, i, input.shapes[i]);
            if (hay.find(needle) == std::string::npos) continue;
        }
        row_passes[i] = true;
    }

    // ---- Step 2 : compact-group collapsing.
    // Mark which rows are fully covered by an ACTIVE group (show_compact
    // AND every row in the range is visible).  A group whose rows were all
    // filtered out is dropped (produces no visible row at all).
    std::vector<bool> consumed_by_group(N, false);
    struct GroupInfo { std::size_t gidx, start, end, tool_turns, add, del, visible_rows; };
    std::vector<GroupInfo> active_groups;
    active_groups.reserve(input.compact_boundary_groups.size());

    if (input.filters.show_compact) {
        for (std::size_t gi = 0; gi < input.compact_boundary_groups.size(); ++gi) {
            auto [s, e] = input.compact_boundary_groups[gi];
            if (s > N) s = N;
            if (e > N) e = N;
            if (s >= e) continue;

            std::size_t vcount = 0, tool_turns = 0, add = 0, del = 0;
            for (std::size_t r = s; r < e; ++r) {
                if (!row_passes[r]) continue;
                ++vcount;
                // Tool-turn heuristics (same as collapsed_content_message):
                //   AssistantToolUse / AssistantGroupedTools each count as one
                //   "tool turn" inside the collapsed window.
                if (r < input.shapes.size()) {
                    auto sh = input.shapes[r];
                    if (sh == MessageShape::AssistantToolUse ||
                        sh == MessageShape::AssistantGroupedTools) {
                        ++tool_turns;
                    }
                    // Add / delete counts come from diff payloads.  We don't
                    // want to pull the structured_diff module here, so the
                    // engine can write precomputed stats alongside compact
                    // boundary groups. Until that richer input shape lands,
                    // approximate from UserToolResult preview_text
                    // occurrences of "+++" / "---" markers.
                    if (sh == MessageShape::UserToolResult && r < input.rows.size()) {
                        const std::string t = detail::payload_preview(input.rows[r]);
                        // rough counts
                        auto count_needle = [&](std::string_view pat) -> std::size_t {
                            std::size_t c = 0, pos = 0;
                            while ((pos = t.find(pat, pos)) != std::string::npos) {
                                ++c; pos += pat.size();
                            }
                            return c;
                        };
                        add += count_needle("+++");
                        del += count_needle("---");
                    }
                }
            }
            if (vcount == 0) continue;   // whole group filtered → drop it

            active_groups.push_back({ gi, s, e, tool_turns, add, del, vcount });
            for (std::size_t r = s; r < e; ++r) consumed_by_group[r] = true;
        }
    }

    // ---- Step 2.5: raw stage boundaries survive filtering and disclosure.
    std::vector<bool> consumed_by_chain(N, false);
    std::vector<std::optional<VisibleRow>> start_to_chain(N);
    if (!input.is_transcript_mode && !do_search && !input.disable_chain_compression) {
        for (std::size_t i = 0; i < N;) {
            const auto* timing = chain_timings[i];
            if (!timing || !row_passes[i] || consumed_by_group[i]) { ++i; continue; }
            const auto begin = i;
            auto end = i + 1;
            while (end < N && chain_timings[end] == timing) ++end;
            VisibleRow chain;
            chain.kind = VisibleRow::Kind::CompressedChain;
            chain.chain_uuid = timing->disclosure_id;
            chain.chain_elapsed = timing->elapsed;
            chain.chain_emphasize_work = input.emphasize_work;
            chain.chain_is_live = input.query_running && end == N && !timing->finished;
            chain.chain_is_expanded = !chain.chain_uuid.empty() &&
                input.expanded_keys.contains("chain:" + chain.chain_uuid);
            for (auto r = begin; r < end; ++r) {
                if (!row_passes[r] || consumed_by_group[r]) continue;
                const auto shape = input.shapes[r];
                if (shape == MessageShape::AssistantThinking ||
                    shape == MessageShape::AssistantRedactedThinking) ++chain.group_count;
                else chain.chain_has_tool_activity = true;
                if (shape == MessageShape::AssistantToolUse ||
                    shape == MessageShape::AssistantGroupedTools) {
                    ++chain.tool_turns;
                    count_row_tools(shape, input.rows[r], chain.chain_tool_breakdown);
                }
                consumed_by_chain[r] = !chain.chain_is_expanded;
            }
            if (chain.chain_is_live && chain.chain_has_tool_activity)
                chain.chain_current_activity = current_chain_activity(
                    input, begin, end, row_passes, consumed_by_group);
            if (chain.chain_is_live && chain.chain_elapsed) {
                const auto next_second = std::chrono::duration_cast<std::chrono::seconds>(
                    *chain.chain_elapsed) + std::chrono::seconds(1);
                input.chain_update_due = timing->started_at + next_second;
            }
            start_to_chain[begin] = std::move(chain);
            i = end;
        }
    }

    // ---- Step 3 : linear merge walk.
    // Groups may arrive in any order but *typically* are sorted; we walk
    // rows in ascending order and insert a group row exactly when we cross
    // its start boundary.  Result is always well-ordered regardless.
    std::vector<VisibleRow> out;
    out.reserve(N);

    // For O(1) group lookup by start index: build map
    std::vector<std::optional<GroupInfo>> start_to_group(N);
    for (const auto& g : active_groups) {
        if (g.start < N) start_to_group[g.start] = g;
    }

    for (std::size_t i = 0; i < N; ++i) {
        // 1) emit group row BEFORE the first consumed row of its range
        if (start_to_group[i].has_value()) {
            const auto& g = *start_to_group[i];
            out.push_back(VisibleRow{
                .kind        = VisibleRow::Kind::CompactGroup,
                .group_idx   = g.gidx,
                .group_count = g.visible_rows,
                .tool_turns  = g.tool_turns,
                .additions   = g.add,
                .deletions   = g.del,
            });
            continue;
        }
        // 1b) emit compressed-chain header at the chain start boundary.
        // For collapsed chains the rows below are consumed_by_chain and
        // skipped by check (2); for expanded chains the header is followed
        // by the individual payload rows.
        if (start_to_chain[i].has_value()) {
            out.push_back(*start_to_chain[i]);
            if (!start_to_chain[i]->chain_is_expanded) continue;
        }
        // 2) rows inside an active group or chain are skipped
        if (consumed_by_group[i]) continue;
        if (consumed_by_chain[i]) continue;
        // 3) regular payload row
        if (!row_passes[i]) continue;
        out.push_back(VisibleRow{
            .kind    = VisibleRow::Kind::Payload,
            .row_idx = i,
        });
    }

    // ---- Step 4 : transcript-mode cap.
    //              When is_transcript_mode and NOT show_all_in_transcript, cap
    //              visible rows to last kMaxMessagesInTranscriptMode (30).
    //              Prepend a TranscriptCapDivider showing how many were hidden.
    if (input.is_transcript_mode && !input.show_all_in_transcript &&
        out.size() > kMaxMessagesInTranscriptMode)
    {
        const std::size_t hidden = out.size() - kMaxMessagesInTranscriptMode;
        std::vector<VisibleRow> capped;
        capped.reserve(kMaxMessagesInTranscriptMode + 1);
        capped.push_back(VisibleRow{
            .kind         = VisibleRow::Kind::TranscriptCapDivider,
            .hidden_count = hidden,
            .chain_uuid   = {},
        });
        // Copy last kMax rows from the full output.
        const auto start = out.end() - static_cast<std::ptrdiff_t>(kMaxMessagesInTranscriptMode);
        capped.insert(capped.end(), start, out.end());
        out = std::move(capped);
    }

    return out;
}

} // namespace loom::ui::messages_list
