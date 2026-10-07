/// @file invariant_checker.hpp
/// @brief RFC 0003 Phase 3: scoped invariant checker for streaming replay.
///
/// InvariantChecker is constructed with a fixture's replay steps and wired
/// into StreamReplayHarness::play_with_invariants() as the per-step
/// callback. After every step it (1) updates internal state from the step
/// that just executed and (2) runs invariants INV-02 through INV-06 against
/// the normalized rendered screen (INV-01 — no crash — is implicit: if
/// check() is called, the screen rendered without throwing).
///
/// Each invariant is SELF-SCOPING: it returns an empty string (pass) when
/// its preconditions are unmet, so the same checker instance is safe to
/// run against every fixture without per-fixture configuration.
///
/// Layer 3 of the RFC 0003 architecture (assertion). Header-only: no
/// CMakeLists.txt change is needed because it is included by
/// test_streaming_replay.cpp, which is already registered.
///
/// Invariant catalog (RFC 0003 §8.2):
///   INV-01  No crash (implicit).
///   INV-02  Thinking not truncated in expanded view.
///   INV-03  No duplicate committed content after commit (while running).
///   INV-04  Streaming text cleared after __end_query__.
///   INV-05  No empty tool blocks after completion.
///   INV-06  Tool result line before subsequent assistant text line.
///   INV-07  Completed thinking shows collapsed label after grace expiry
///           (MANUAL — requires clock manipulation, see
///           check_inv07_grace_expiry()).
///
/// Screen-text grounding (from the committed golden snapshots under
/// tests/fixtures/streaming_snapshots/):
///   * The expanded thinking label is "∴ Thinking…" where … is U+2026
///     (NOT three ASCII dots). It is visible while a thinking row is the
///     streaming tail and for 3s after (the collapse grace,
///     was_recently_streaming); once the grace expires the row collapses
///     to the "∴ Thought for Xs <summary> (ctrl+o to expand)" label and
///     the expanded label is absent. Thinking rows are never hidden.
///   * The streaming assistant-text row carries a block cursor "▌" (U+25AC)
///     at its end; the committed row does not. The cursor BLINKS and is
///     non-deterministic — it is present in some golden snapshots and
///     absent in others — so it cannot be used to reliably distinguish the
///     streaming row from the committed row. Post-commit while the query
///     is running the same text legitimately appears twice (committed row
///     + streaming row) — this transient coexistence is expected and
///     cleared at __end_query__. INV-03 therefore accepts up to 2
///     occurrences (committed + streaming) and flags only 3+ (a genuine
///     duplicate); INV-04 counts all lines post-__end_query__ where the
///     streaming row must be gone.
///   * The committed tool-use row renders as "● <Name> (<input>)" — the
///     input is in header parens, not on a following line. A tool row with
///     no parens and no Input:/Output: section is the "empty tool block".
///   * tool_call_bash has no __commit__, so no tool line is visible at all
///     (the streaming tool row is skipped once exec_done) — INV-05/06 are
///     vacuous there.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "streaming_replay.hpp"

namespace loom::testing {

/// Runs the RFC 0003 §8.2 invariant catalog against a replayed fixture.
///
/// Usage (in play_fixture()):
///   auto steps = loom::testing::load_fixture("text_simple");
///   loom::testing::InvariantChecker checker(steps);
///   auto snapshots = harness.play_with_invariants(steps,
///       [&checker](std::string_view screen, std::size_t step_idx) {
///           checker.check(screen, step_idx);
///       });
///
/// All failures are reported via ADD_FAILURE() (non-fatal) so that every
/// invariant is checked even if an earlier one fails.
class InvariantChecker {
public:
    explicit InvariantChecker(std::span<const ReplayStep> steps)
        : steps_(steps) {}

    /// Called by the play_with_invariants callback after every step.
    /// First updates internal state from steps[step_idx] (the step that
    /// just executed), then runs all applicable invariants against the
    /// normalized screen text.
    void check(std::string_view screen, std::size_t step_idx) {
        // INV-01 (no crash) is implicit — reaching this line means the
        // screen rendered without throwing.

        if (step_idx < steps_.size()) {
            update_state(steps_[step_idx]);
        }

        if (auto msg = check_inv02(screen); !msg.empty())
            ADD_FAILURE() << "step " << step_idx << ": " << msg;
        if (auto msg = check_inv03(screen); !msg.empty())
            ADD_FAILURE() << "step " << step_idx << ": " << msg;
        if (auto msg = check_inv04(screen); !msg.empty())
            ADD_FAILURE() << "step " << step_idx << ": " << msg;
        if (auto msg = check_inv05(screen); !msg.empty())
            ADD_FAILURE() << "step " << step_idx << ": " << msg;
        if (auto msg = check_inv06(screen); !msg.empty())
            ADD_FAILURE() << "step " << step_idx << ": " << msg;
    }

    // ============================================================
    // INV-07: Completed thinking shows collapsed label after grace
    // expiry (RFC 0003 §8.3).
    //
    // MANUAL — not run by the automatic check() loop above, because it
    // requires clock manipulation (set_steady_now_for_testing) between
    // renders. A test calls this AFTER advancing the clock past the 3s
    // collapse grace and re-rendering:
    //
    //   loom::ui::clock::set_steady_now_for_testing(t0 + 4s);
    //   auto screen = harness.render_now();
    //   EXPECT_TRUE(
    //       loom::testing::InvariantChecker::check_inv07_grace_expiry(
    //           screen).empty());
    //
    // Returns an empty string on pass, a failure message otherwise.
    // ============================================================
    [[nodiscard]] static std::string check_inv07_grace_expiry(
        std::string_view screen) {
        // The collapsed label is marked by the " (ctrl+o to expand)" hint,
        // which RenderThinkingMessageCollapsed emits exclusively. Within
        // the 3s collapse grace the thinking row is still expanded, so the
        // hint is absent; after expiry it must appear.
        if (screen.find(kCollapsedThinkingHint) ==
            std::string_view::npos) {
            return "INV-07: collapsed thinking label (\" (ctrl+o to "
                   "expand)\") not found after grace expiry";
        }
        return {};
    }

    /// Count occurrences of a text block's first 40 chars on screen.
    /// Returns the count, or 0 if the text is too short. Used by
    /// targeted tests that need to assert exact occurrence counts
    /// (e.g. Bug 1 regression: streaming text cleared after tool
    /// completion — see test_streaming_replay.cpp).
    [[nodiscard]] static std::size_t count_text_on_screen(
        std::string_view screen, std::string_view text) {
        if (text.size() < 40) return 0;
        const std::string flat = flatten(screen);
        const std::string needle = flatten(text).substr(0, 40);
        if (needle.size() < 40) return 0;
        return count_occurrences(flat, needle);
    }

private:
    // ============================================================
    // Screen-text constants (exact UTF-8 bytes — see file header)
    // ============================================================

    /// "∴ Thinking…" — U+2234 (∴) + " Thinking" + U+2026 (…).
    static constexpr std::string_view kThinkingLabel =
        "\xE2\x88\xB4 Thinking\xE2\x80\xA6";

    /// " (ctrl+o to expand)" — the collapsed-label hint (kCtrlOHint in
    /// thinking_message.cppm). Emitted ONLY by
    /// RenderThinkingMessageCollapsed, so its presence marks a collapsed
    /// thinking row. Used by INV-07 (grace expiry). The collapsed label
    /// reads `∴ Thinking  <summary> (ctrl+o to expand)` for a committed
    /// block (duration=0), so the hint — not the glyph — is the marker.
    static constexpr std::string_view kCollapsedThinkingHint =
        " (ctrl+o to expand)";

    /// "● " — U+25CF (BLACK CIRCLE) + space. Prefix of both the committed
    /// assistant-text row and the committed tool-use row.
    static constexpr std::string_view kToolBullet = "\xE2\x97\x8F ";

    // ============================================================
    // State tracking
    // ============================================================

    std::span<const ReplayStep> steps_;

    /// Block index → "thinking" / "text" / "tool_use" (from
    /// ContentBlockStart). Needed because ContentBlockDelta carries only
    /// index + delta_text, not the block type — the checker must know
    /// which deltas are thinking.
    std::map<std::uint32_t, std::string> block_types_;

    /// Block index → accumulated thinking text (from ContentBlockDelta on
    /// blocks whose ContentBlockStart declared type "thinking").
    std::map<std::uint32_t, std::string> thinking_deltas_;

    /// All committed thinking blocks (from __commit__ → ThinkingBlock).
    std::vector<std::string> committed_thinking_;

    struct CommittedText {
        std::string text;
        std::size_t commit = 0;  // commit sequence number (for ordering)
    };
    struct CommittedTool {
        std::string name;
        std::size_t commit = 0;
    };

    /// All committed text blocks (from __commit__ → TextBlock).
    std::vector<CommittedText> committed_texts_;

    /// All committed tool_use blocks (from __commit__ → ToolUseBlock).
    std::vector<CommittedTool> committed_tools_;

    struct ToolState {
        std::string name;
        bool exec_started = false;
        bool exec_done = false;
        std::string result_text;
    };
    /// tool_use_id → state (from ToolExecutionStart / ToolExecutionEnd).
    std::map<std::string, ToolState> tools_;

    bool query_running_ = false;   ///< true after first Event, false on EndQuery
    bool post_commit_ = false;     ///< true after any __commit__ while running
    bool post_end_query_ = false;  ///< true after __end_query__
    std::size_t commit_count_ = 0; ///< monotonic commit sequence number

    // ============================================================
    // State update
    // ============================================================

    /// Update internal state from the step that just executed.
    void update_state(const ReplayStep& step) {
        switch (step.kind) {
            case ReplayStepKind::Event:
                // The harness sets query_running_ before the first event;
                // mirror that so post-commit / post-end-query scoping is
                // accurate.
                if (!query_running_) query_running_ = true;
                if (step.event) {
                    std::visit([this](const auto& e) {
                        using T = std::decay_t<decltype(e)>;

                        if constexpr (std::is_same_v<T, loom::core::StreamStart>) {
                            // A new stream reuses block indices from the
                            // previous turn (both turns start at index 0).
                            // Clear per-stream accumulators so turn-2 deltas
                            // do not append to turn-1 entries.
                            thinking_deltas_.clear();
                            block_types_.clear();
                        } else if constexpr (std::is_same_v<
                                          T, loom::core::ContentBlockStart>) {
                            // Record the block type so later deltas can be
                            // classified as thinking vs text.
                            std::string type;
                            std::visit([&type](const auto& b) {
                                using B = std::decay_t<decltype(b)>;
                                if constexpr (std::is_same_v<
                                                  B, loom::core::ThinkingBlock>)
                                    type = "thinking";
                                else if constexpr (std::is_same_v<
                                                       B, loom::core::TextBlock>)
                                    type = "text";
                                else if constexpr (std::is_same_v<
                                                       B, loom::core::ToolUseBlock>)
                                    type = "tool_use";
                            }, e.block);
                            block_types_[e.index] = std::move(type);
                        } else if constexpr (std::is_same_v<
                                                 T, loom::core::ContentBlockDelta>) {
                            // Accumulate thinking deltas only.
                            auto it = block_types_.find(e.index);
                            if (it != block_types_.end() &&
                                it->second == "thinking") {
                                thinking_deltas_[e.index] += e.delta_text;
                            }
                        } else if constexpr (std::is_same_v<
                                                 T, loom::core::ToolExecutionStart>) {
                            auto& ts = tools_[e.tool_use_id];
                            ts.name = e.tool_name;
                            ts.exec_started = true;
                        } else if constexpr (std::is_same_v<
                                                 T, loom::core::ToolExecutionEnd>) {
                            auto it = tools_.find(e.tool_use_id);
                            if (it != tools_.end()) {
                                it->second.exec_done = true;
                                it->second.result_text = e.result;
                            }
                        }
                        // ContentBlockStop / ToolExecutionProgress /
                        // StreamEnd / StreamError need no tracked state.
                    }, *step.event);
                }
                break;

            case ReplayStepKind::Commit:
                if (step.message) {
                    // ReplayStep::message is a full Message variant; every
                    // alternative carries a content vector via MessageBase.
                    std::visit([this](const auto& m) {
                        for (const auto& block : m.content) {
                            std::visit([this](const auto& b) {
                                using B = std::decay_t<decltype(b)>;
                                if constexpr (std::is_same_v<
                                                  B, loom::core::TextBlock>) {
                                    committed_texts_.push_back(
                                        {.text = b.text, .commit = commit_count_});
                                } else if constexpr (std::is_same_v<
                                                         B, loom::core::ThinkingBlock>) {
                                    committed_thinking_.push_back(b.thinking);
                                } else if constexpr (std::is_same_v<
                                                         B, loom::core::ToolUseBlock>) {
                                    committed_tools_.push_back(
                                        {.name = b.name, .commit = commit_count_});
                                }
                            }, block);
                        }
                    }, *step.message);
                }
                if (query_running_) post_commit_ = true;
                ++commit_count_;
                break;

            case ReplayStepKind::EndQuery:
                query_running_ = false;
                post_end_query_ = true;
                break;

            case ReplayStepKind::Checkpoint:
                break;  // no state change
        }
    }

    // ============================================================
    // Invariant implementations
    //
    // Each returns an empty string on pass (or when out of scope) and a
    // failure message otherwise.
    // ============================================================

    /// INV-02: Thinking not truncated in expanded view.
    ///
    /// Scope: the screen contains the expanded label "∴ Thinking…".
    ///
    /// When the label is present, at least one tracked thinking block's
    /// leading content must be visible. Among the visible blocks, if the
    /// longest exceeds 200 chars, content beyond the 200-char truncation
    /// boundary must also be visible (guards the substr(0,200) truncation
    /// bug).
    std::string check_inv02(std::string_view screen) const {
        if (screen.find(kThinkingLabel) == std::string_view::npos)
            return {};  // no expanded thinking on screen — vacuous

        const std::string flat_screen = flatten(screen);

        // Find the longest thinking block whose leading content is visible.
        // Only the streaming-tail thinking row is shown (the filter hides
        // non-tail thinking rows during the grace period), so we require at
        // least ONE block to be found rather than every block.
        std::string longest_visible;
        bool found_any = false;

        auto consider = [&](std::string_view content) {
            const std::string flat = flatten(content);
            if (flat.empty()) return;
            // General probe: first 50 chars must appear on screen.
            if (flat_screen.find(flat.substr(0, 50)) !=
                std::string_view::npos) {
                found_any = true;
                if (flat.size() > longest_visible.size())
                    longest_visible = flat;
            }
        };

        for (const auto& [idx, text] : thinking_deltas_) consider(text);
        for (const auto& text : committed_thinking_) consider(text);

        if (!found_any) {
            return "thinking label present but no tracked thinking content "
                   "found on screen";
        }

        // Truncation probe: the historical bug fed a 200-char
        // content_preview as the thinking body. If the longest visible
        // block exceeds 200 chars, verify content past char 200 is shown.
        if (longest_visible.size() > 200) {
            constexpr std::size_t kTruncBoundary = 200;
            const std::size_t off = kTruncBoundary + 10;  // 210
            if (off < longest_visible.size()) {
                const std::string probe = longest_visible.substr(off, 50);
                if (!probe.empty() &&
                    flat_screen.find(probe) == std::string_view::npos) {
                    return "thinking content beyond char 200 not found on "
                           "screen — truncated?";
                }
            }
        }
        return {};
    }

    /// INV-03: No duplicate committed content after commit (while running).
    ///
    /// Scope: post-__commit__ while query_running_ is still true, and a
    /// committed text block of 40+ chars exists.
    ///
    /// Post-commit while the query is running, the same text legitimately
    /// appears in BOTH the committed row and the live streaming row. That
    /// transient coexistence (exactly 2 occurrences) is expected and is
    /// cleared at __end_query__. A genuine duplicate (e.g. double-commit)
    /// would produce 3+ occurrences. The streaming cursor "▌" blinks and
    /// is non-deterministic, so it cannot be used to exclude the streaming
    /// row — instead we accept the 2-occurrence coexistence and flag only
    /// 3+.
    std::string check_inv03(std::string_view screen) const {
        if (!post_commit_ || !query_running_) return {};

        const std::string flat = flatten(screen);

        for (const auto& ct : committed_texts_) {
            if (ct.text.size() < 40) continue;
            const std::string needle = flatten(ct.text).substr(0, 40);
            if (needle.size() < 40)
                continue;  // flatten shrank it below the threshold
            const auto count = count_occurrences(flat, needle);
            if (count > 2) {
                return "committed text appears " + std::to_string(count) +
                       " times on screen (expected at most 2: committed + "
                       "streaming)";
            }
        }
        return {};
    }

    /// INV-04: Streaming text cleared after query end.
    ///
    /// Scope: post-__end_query__.
    ///
    /// After the query ends, the idle path (SyncState) clears
    /// streaming_text_, so each committed text block must appear exactly
    /// once (the committed row only). More than one occurrence means the
    /// streaming row was not cleared.
    std::string check_inv04(std::string_view screen) const {
        if (!post_end_query_) return {};

        const std::string flat = flatten(screen);

        for (const auto& ct : committed_texts_) {
            if (ct.text.size() < 40) continue;
            const std::string needle = flatten(ct.text).substr(0, 40);
            if (needle.size() < 40) continue;
            const auto count = count_occurrences(flat, needle);
            if (count > 1) {
                return "committed text appears " + std::to_string(count) +
                       " times after __end_query__ — streaming text not "
                       "cleared?";
            }
        }
        return {};
    }

    /// INV-05: No empty tool blocks after completion.
    ///
    /// Scope: at least one tool has exec_done=true AND a "● <Name>" line
    /// for it is visible on screen.
    ///
    /// The committed tool-use row renders as "● <Name> (<input>)" — the
    /// input is in header parens. MCP tools may instead show an "Input:" /
    /// "Output:" section on the following line. A tool row with neither is
    /// the "empty tool block" defect. If the tool line is not visible at
    /// all (e.g. tool_call_bash, which has no __commit__), the invariant
    /// is vacuous.
    std::string check_inv05(std::string_view screen) const {
        for (const auto& [id, ts] : tools_) {
            if (!ts.exec_done) continue;

            const std::string needle = std::string(kToolBullet) + ts.name;
            const auto line_idx = find_line(screen, needle);
            if (!line_idx) continue;  // tool line not visible — vacuous

            const std::string_view line = get_line(screen, *line_idx);

            // Input is rendered in header parens: "● Bash (git status …)".
            if (line.find('(') != std::string_view::npos) continue;

            // Otherwise look for an Input:/Output: section on the next
            // non-empty line (MCP-style tools).
            if (const auto next = next_non_empty_line(screen, *line_idx)) {
                const std::string_view next_line =
                    get_line(screen, *next);
                if (next_line.find("Input:") != std::string_view::npos ||
                    next_line.find("Output:") != std::string_view::npos) {
                    continue;
                }
            }

            return "tool line '" + std::string(kToolBullet) + ts.name +
                   "' not followed by input or result content";
        }
        return {};
    }

    /// INV-06: Tool result line before subsequent assistant text.
    ///
    /// Scope: fixtures with a committed tool_use block AND a committed
    /// text block that follows it (in commit order).
    ///
    /// Verifies the "● <Name>" tool line appears above the subsequent
    /// assistant text on screen. If either is not visible (off-screen or
    /// suppressed), the invariant is vacuous for that pair.
    std::string check_inv06(std::string_view screen) const {
        for (const auto& tool : committed_tools_) {
            for (const auto& ct : committed_texts_) {
                if (ct.commit <= tool.commit)
                    continue;  // text was not committed after the tool
                if (ct.text.size() < 40) continue;

                const std::string tool_needle =
                    std::string(kToolBullet) + tool.name;
                const auto tool_line = find_line(screen, tool_needle);
                if (!tool_line) continue;  // tool not visible

                // The first 40 chars of the text sit on its first line
                // (40 < 118-col wrap width), so a line-based substring
                // search locates the text block's first row.
                const std::string text_needle = ct.text.substr(0, 40);
                const auto text_line = find_line(screen, text_needle);
                if (!text_line) continue;  // text not visible

                if (*tool_line >= *text_line) {
                    return "tool line appears after assistant text — "
                           "ordering violated";
                }
            }
        }
        return {};
    }

    // ============================================================
    // Helpers
    // ============================================================

    /// Collapse every run of whitespace (spaces, tabs, newlines, CR) to a
    /// single space and trim leading/trailing whitespace. Makes substring
    /// searches wrap-safe: the thinking/text body is indented and wrapped
    /// at ~118 cols, so a needle that straddles a wrap boundary would not
    /// match a raw (newline-preserving) search.
    [[nodiscard]] static std::string flatten(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        bool in_ws = false;
        for (char c : s) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                if (!in_ws && !out.empty()) {
                    out.push_back(' ');
                    in_ws = true;
                }
            } else {
                out.push_back(c);
                in_ws = false;
            }
        }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        return out;
    }

    /// Count non-overlapping occurrences of `needle` in `hay`.
    [[nodiscard]] static std::size_t count_occurrences(
        std::string_view hay, std::string_view needle) {
        if (needle.empty()) return 0;
        std::size_t count = 0;
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string_view::npos) {
            ++count;
            pos += needle.size();
        }
        return count;
    }

    /// Find the index of the first line that contains `text`.
    /// Lines are split on '\n'. Returns std::nullopt if not found.
    [[nodiscard]] static std::optional<std::size_t> find_line(
        std::string_view screen, std::string_view text) {
        std::size_t line_idx = 0;
        for (std::size_t start = 0; start < screen.size();) {
            const auto end = screen.find('\n', start);
            const std::string_view line = (end == std::string_view::npos)
                                              ? screen.substr(start)
                                              : screen.substr(start, end - start);
            if (line.find(text) != std::string_view::npos) return line_idx;
            if (end == std::string_view::npos) break;
            start = end + 1;
            ++line_idx;
        }
        return std::nullopt;
    }

    /// Get the line at `line_idx` (0-based, split on '\n'). Returns an
    /// empty view if the index is out of range.
    [[nodiscard]] static std::string_view get_line(
        std::string_view screen, std::size_t line_idx) {
        std::size_t idx = 0;
        for (std::size_t start = 0; start < screen.size();) {
            const auto end = screen.find('\n', start);
            const std::string_view line = (end == std::string_view::npos)
                                              ? screen.substr(start)
                                              : screen.substr(start, end - start);
            if (idx == line_idx) return line;
            if (end == std::string_view::npos) break;
            start = end + 1;
            ++idx;
        }
        return {};
    }

    /// Find the index of the first non-empty (whitespace-only counts as
    /// empty) line strictly after `after_line`. Returns std::nullopt if
    /// there is no such line.
    [[nodiscard]] static std::optional<std::size_t> next_non_empty_line(
        std::string_view screen, std::size_t after_line) {
        std::size_t idx = 0;
        for (std::size_t start = 0; start < screen.size();) {
            const auto end = screen.find('\n', start);
            const std::string_view line = (end == std::string_view::npos)
                                              ? screen.substr(start)
                                              : screen.substr(start, end - start);
            if (idx > after_line) {
                bool non_empty = false;
                for (char c : line) {
                    if (c != ' ' && c != '\t' && c != '\r') {
                        non_empty = true;
                        break;
                    }
                }
                if (non_empty) return idx;
            }
            if (end == std::string_view::npos) break;
            start = end + 1;
            ++idx;
        }
        return std::nullopt;
    }
};

}  // namespace loom::testing
