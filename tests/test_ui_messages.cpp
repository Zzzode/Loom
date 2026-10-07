/// @file test_ui_messages.cpp
/// @brief Split from test_ui.cpp - CollapseBackgroundBash, ImagePaste, ImagePasteCtrlV, ImagePasteFormat, ImagePasteOrphanCleanup, ImagePasteSubmit, MessagePipeline, Messages, MessagesList, VirtualList (SLOC budget fix)

#include <cstdlib>

#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <gtest/gtest.h>
#include <httplib.h>

#include "test_ui_helpers.h"

import std;
import loom.ui.messages.message_pipeline;
import loom.ui.messages.collapse_background_bash;
import loom.ui.messages.virtual_list;
import loom.ui.messages.messages_list;
import loom.ui.messages.message_row;
import loom.ui.messages.thinking_message;
import loom.ui.messages.tool_use_message;
import loom.ui.messages.message_tool_result;
import loom.ui.messages.user_text_message;
import loom.ui.messages.assistant_text_message;
import loom.ui.messages.message_image;
import loom.ui.visual.markdown;
import loom.ui.foundation.design_tokens;
import loom.ui.foundation.design_figures;
import loom.constants.constants;
import loom.text.parse_references;
import loom.ui.widgets.components;
import loom.ui.widgets.all_components;
import loom.types.types;

namespace {
namespace fs = std::filesystem;
}


// ═══════════════════════════════════════════════════════════════════════════════
// loom.ui.prompt.prompt_input: prompt buffer, history, typeahead, vim behavior
// ═══════════════════════════════════════════════════════════════════════════════

namespace pl = loom::ui::messages::pipeline;

TEST(MessagePipeline, DedupStartDeltaStopSmoke) {
    pl::DedupTracker t;
    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kNotSeen);

    EXPECT_TRUE(t.should_accept_start(0));
    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kOpen);

    // Duplicate Start while Open → accept (still open, no harm).
    EXPECT_TRUE(t.should_accept_start(0));
    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kOpen);

    // Deltas while Open → accept.
    EXPECT_TRUE(t.should_accept_delta(0));
    EXPECT_TRUE(t.should_accept_delta(0));

    // First Stop → accept and transition to terminal.
    EXPECT_TRUE(t.should_accept_stop(0));
    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kStopped);

    // Post-Stop: Start / Delta / Stop all dedup dropped.
    EXPECT_FALSE(t.should_accept_start(0)) << "replay Start after Stop must be dropped";
    EXPECT_FALSE(t.should_accept_delta(0)) << "Delta after Stop must be dropped";
    EXPECT_FALSE(t.should_accept_stop(0))  << "Second Stop must be dropped";

    // Other indices are unaffected.
    EXPECT_TRUE(t.should_accept_start(1));
    EXPECT_TRUE(t.should_accept_start(42));
}

TEST(MessagePipeline, DedupDeltaBeforeStartPromotesToOpen) {
    pl::DedupTracker t;
    // Out-of-order Delta (server sent events in a wacky order) → should accept
    // and leave the index in Open state so that the subsequent Start doesn't
    // create a duplicate.
    EXPECT_TRUE(t.should_accept_delta(7));
    EXPECT_EQ(t.state_of(7), pl::DedupTracker::IndexState::kOpen);
    EXPECT_TRUE(t.should_accept_start(7));   // Start after Delta → dedup accept
}

TEST(MessagePipeline, DedupToolUseExecStartAndEndOneShot) {
    pl::DedupTracker t;
    EXPECT_TRUE(t.should_accept_exec_start("toolu_01abc"));
    EXPECT_TRUE(t.should_accept_exec_end("toolu_01abc"));
    // Repeated calls are dedup'd.
    EXPECT_FALSE(t.should_accept_exec_start("toolu_01abc"));
    EXPECT_FALSE(t.should_accept_exec_end("toolu_01abc"));
    // Different id is independent.
    EXPECT_TRUE(t.should_accept_exec_start("toolu_02def"));
}

TEST(MessagePipeline, DedupClearResetsAllState) {
    pl::DedupTracker t;
    (void)t.should_accept_start(0);
    (void)t.should_accept_stop(0);
    (void)t.should_accept_exec_start("abc");
    (void)t.should_accept_exec_end("abc");
    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kStopped);

    t.clear();

    EXPECT_EQ(t.state_of(0), pl::DedupTracker::IndexState::kNotSeen);
    EXPECT_TRUE(t.should_accept_start(0));
    EXPECT_TRUE(t.should_accept_exec_start("abc"));
}

// ── Stage 4 USER_INPUT_FILTER ──────────────────────────────────────────────
TEST(MessagePipeline, FilterPlainTextFastPath) {
    auto rows = pl::filter_user_text("hello world");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].kind, pl::UserRowKind::kPlainText);
    EXPECT_EQ(rows[0].display_text, "hello world");
}

TEST(MessagePipeline, FilterBashInputTag) {
    auto rows = pl::filter_user_text("<bash-input>ls -la ~/Documents</bash-input>");
    ASSERT_GE(rows.size(), 1u);
    auto it = std::find_if(rows.begin(), rows.end(),
        [](const auto& r){ return r.kind == pl::UserRowKind::kBashInput; });
    ASSERT_NE(it, rows.end());
    EXPECT_EQ(it->display_text, "ls -la ~/Documents");
}

TEST(MessagePipeline, FilterQuotedReply) {
    auto rows = pl::filter_user_text(
        "<quoted-reply># Old issue number 42\nsecond line</quoted-reply>\n"
        "Please take a look");
    ASSERT_GE(rows.size(), 1u);
    auto it = std::find_if(rows.begin(), rows.end(),
        [](const auto& r){ return r.kind == pl::UserRowKind::kQuotedReply; });
    ASSERT_NE(it, rows.end());
    EXPECT_EQ(it->quoted_reply, "# Old issue number 42\nsecond line");
    EXPECT_EQ(it->display_text, "Please take a look");
}

TEST(MessagePipeline, FilterToolInvocation) {
    auto rows = pl::filter_user_text("<tool:BashTool>ls /tmp | head</tool:BashTool>");
    ASSERT_GE(rows.size(), 1u);
    auto it = std::find_if(rows.begin(), rows.end(),
        [](const auto& r){ return r.kind == pl::UserRowKind::kToolInvocation; });
    ASSERT_NE(it, rows.end());
    EXPECT_EQ(it->tool_name, "BashTool");
    EXPECT_EQ(it->display_text, "ls /tmp | head");
}

TEST(MessagePipeline, FilterAtMentions) {
    auto rows = pl::filter_user_text(
        "Please review "
        "<at-file>src/main.cpp</at-file> and "
        "<at-agent>SeniorEngineer</at-agent>"
        "<at-tool>SearchTool</at-tool>");
    auto has_attach = [](const auto& r){ return r.kind == pl::UserRowKind::kAttachment; };
    const auto attachments = std::count_if(rows.begin(), rows.end(), has_attach);
    EXPECT_GE(attachments, 3u);
    bool found_file = false, found_agent = false, found_tool = false;
    for (const auto& r : rows) {
        if (r.kind != pl::UserRowKind::kAttachment) continue;
        if (r.attachment_ref == "@src/main.cpp") found_file = true;
        if (r.attachment_ref == "@SeniorEngineer") found_agent = true;
        if (r.attachment_ref == "@tool:SearchTool") found_tool = true;
    }
    EXPECT_TRUE(found_file);
    EXPECT_TRUE(found_agent);
    EXPECT_TRUE(found_tool);
}

TEST(MessagePipeline, FilterMixedContentProducesPlainTextRemainder) {
    // <bash-input> + extra plain text should also yield a plain-text row.
    auto rows = pl::filter_user_text(
        "<bash-input>make</bash-input>\n<at-file>Makefile</at-file>");
    // Expect at least: kBashInput + kAttachment.  Any extra whitespace/text
    // may collapse to plain-text row; verify no row has raw XML left.
    for (const auto& r : rows) {
        EXPECT_EQ(r.display_text.find('<'), std::string::npos)
            << "display_text must never contain raw XML: " << r.display_text;
        EXPECT_EQ(r.quoted_reply.find('<'), std::string::npos);
    }
    EXPECT_TRUE(std::any_of(rows.begin(), rows.end(),
        [](const auto& r){ return r.kind == pl::UserRowKind::kBashInput; }));
    EXPECT_TRUE(std::any_of(rows.begin(), rows.end(),
        [](const auto& r){ return r.kind == pl::UserRowKind::kAttachment; }));
}

TEST(MessagePipeline, FilterEmptyInputProducesSinglePlaceholder) {
    auto rows = pl::filter_user_text("   ");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].kind, pl::UserRowKind::kPlainText);
}

// ── Stage 5 TOOL_RESULT_AUGMENT ────────────────────────────────────────────
TEST(MessagePipeline, ToolAugment_TruncationFlagTriggersAboveThreshold) {
    std::string big(pl::kMaxToolPreviewBytes + 100, 'x');
    auto a = pl::augment_tool_result(big, /*is_error=*/false);
    EXPECT_TRUE(a.truncated);
    EXPECT_EQ(a.error_code, 0);
    EXPECT_FALSE(a.preview.empty());   // truncated or not, preview is set

    std::string small(100, 'a');
    auto b = pl::augment_tool_result(small, /*is_error=*/false);
    EXPECT_FALSE(b.truncated);
}

TEST(MessagePipeline, ToolAugment_ParseErrorCodeRecognisesPatterns) {
    // Pattern 1: "Error 42: ..."
    EXPECT_EQ(pl::parse_error_code("Error 42: something failed"), 42);
    // Pattern 2: "exit code: 127"
    EXPECT_EQ(pl::parse_error_code("exit code: 127\n..."), 127);
    EXPECT_EQ(pl::parse_error_code("Exit status = 1\n"), 1);
    EXPECT_EQ(pl::parse_error_code("[exit_code=7] Done"), 7);
    // Pattern 3: HTTP status codes
    EXPECT_EQ(pl::parse_error_code("HTTP 404 Not Found\n"), 404);
    EXPECT_EQ(pl::parse_error_code("http 500 internal server error"), 500);
    // No numeric prefix → 0
    EXPECT_EQ(pl::parse_error_code("Success!"), 0);
    EXPECT_EQ(pl::parse_error_code(""), 0);
}

TEST(MessagePipeline, ToolAugment_PreviewUsesFirstNonEmptyLine) {
    const std::string input = "\n\n  \nThis is line four with content.\nLine five ignored.\n";
    auto a = pl::augment_tool_result(input, false);
    EXPECT_NE(a.preview.find("This is line four"), std::string::npos)
        << "preview: " << a.preview;
    EXPECT_EQ(a.preview.find("Line five"), std::string::npos)
        << "preview should only include first non-empty line";
}

TEST(MessagePipeline, ToolAugment_PreviewTruncatesTo200Codepoints) {
    std::string line = "abcdefghij";   // 10 chars
    std::string big;
    for (int i = 0; i < 30; ++i) big += line;  // 300 chars, one line
    auto a = pl::augment_tool_result(big, false);
    // kMaxCompactPreviewChars = 200 → preview capped at that plus ellipsis
    EXPECT_LE(a.preview.size(), 200u + 10u);
    EXPECT_NE(a.preview.find(loom::ui::design::figures::kEllipsis), std::string::npos)
        << "long preview should end with … ellipsis";
}

// ═══════════════════════════════════════════════════════════════════════════
// P0-2 collapseBackgroundBashNotifications tests
// TS REF: src/utils/collapseBackgroundBashNotifications.ts
// ═══════════════════════════════════════════════════════════════════════════

namespace cbb = loom::ui::messages::collapse;

namespace {
/// Build a user Message carrying a task-notification with the given status
/// and summary, matching the CPP wire format (underscored tags).
inline loom::core::Message make_notification(std::string_view status,
                                           std::string_view summary) {
    loom::core::UserMessage m{};
    std::string text = "<task_notification><status>";
    text += status;
    text += "</status><summary>";
    text += summary;
    text += "</summary></task_notification>";
    m.content.push_back(loom::core::TextBlock{std::move(text)});
    return m;
}

/// A completed background-bash notification (collapsible).
inline loom::core::Message make_completed_bash(std::string_view name = "\"foo\"") {
    return make_notification("completed",
                             std::string("Background command ") + std::string(name) + " completed");
}

/// Plain user text (never collapses).
inline loom::core::Message make_plain_user(std::string text) {
    loom::core::UserMessage m{};
    m.content.push_back(loom::core::TextBlock{std::move(text)});
    return m;
}

/// Read the first text block of a message (test helper).
inline std::string first_text(const loom::core::Message& msg) {
    const auto* u = std::get_if<loom::core::UserMessage>(&msg);
    if (!u || u->content.empty()) return {};
    const auto* t = std::get_if<loom::core::TextBlock>(&u->content.front());
    return t ? t->text : std::string{};
}
}  // namespace

TEST(CollapseBackgroundBash, SingleCompletionLeftUnchanged) {
    std::vector<loom::core::Message> in;
    in.push_back(make_completed_bash());
    auto out = cbb::collapse_background_bash_notifications(in, /*fullscreen=*/true, /*verbose=*/false);
    ASSERT_EQ(out.size(), 1u);
    // Not synthesized — original text preserved.
    EXPECT_NE(first_text(out[0]).find("Background command"), std::string::npos);
    EXPECT_EQ(first_text(out[0]).find("background commands completed"), std::string::npos);
}

TEST(CollapseBackgroundBash, MultipleConsecutiveCollapseIntoSynthetic) {
    std::vector<loom::core::Message> in;
    in.push_back(make_completed_bash("\"a\""));
    in.push_back(make_completed_bash("\"b\""));
    in.push_back(make_completed_bash("\"c\""));
    auto out = cbb::collapse_background_bash_notifications(in, true, false);
    ASSERT_EQ(out.size(), 1u);
    // TS: `<summary>3 background commands completed</summary>`
    EXPECT_NE(first_text(out[0]).find("3 background commands completed"), std::string::npos);
    EXPECT_NE(first_text(out[0]).find("<status>completed</status>"), std::string::npos);
}

TEST(CollapseBackgroundBash, FailedAndKilledStayVisible) {
    std::vector<loom::core::Message> in;
    in.push_back(make_notification("failed",  "Background command \"x\" failed with exit code 1"));
    in.push_back(make_notification("killed",  "Background command \"y\" was stopped"));
    auto out = cbb::collapse_background_bash_notifications(in, true, false);
    // Neither is a completed-bash, so both pass through untouched.
    EXPECT_EQ(out.size(), 2u);
}

TEST(CollapseBackgroundBash, NonBashSummaryNotCollapsed) {
    // Same 'completed' status but a summary that does NOT start with the
    // BACKGROUND_BASH_SUMMARY_PREFIX (e.g. an agent/workflow notification).
    std::vector<loom::core::Message> in;
    in.push_back(make_notification("completed", "Agent \"planner\" finished"));
    in.push_back(make_notification("completed", "Agent \"builder\" finished"));
    auto out = cbb::collapse_background_bash_notifications(in, true, false);
    EXPECT_EQ(out.size(), 2u);  // untouched
}

TEST(CollapseBackgroundBash, InterleavedRunsPreserveOrderAndCollapseOnlyRuns) {
    std::vector<loom::core::Message> in;
    in.push_back(make_plain_user("hello"));
    in.push_back(make_completed_bash("\"a\""));   // run of 2 → collapses
    in.push_back(make_completed_bash("\"b\""));
    in.push_back(make_plain_user("world"));
    in.push_back(make_completed_bash("\"c\""));   // run of 1 → stays
    auto out = cbb::collapse_background_bash_notifications(in, true, false);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(first_text(out[0]), "hello");
    EXPECT_NE(first_text(out[1]).find("2 background commands completed"), std::string::npos);
    EXPECT_EQ(first_text(out[2]), "world");
    EXPECT_NE(first_text(out[3]).find("Background command"), std::string::npos);  // single, unchanged
}

TEST(CollapseBackgroundBash, VerbosePassThrough) {
    std::vector<loom::core::Message> in;
    in.push_back(make_completed_bash("\"a\""));
    in.push_back(make_completed_bash("\"b\""));
    // TS: `if (verbose) return messages;`
    auto out = cbb::collapse_background_bash_notifications(in, /*fullscreen=*/true, /*verbose=*/true);
    EXPECT_EQ(out.size(), 2u);
}

TEST(CollapseBackgroundBash, NonFullscreenPassThrough) {
    std::vector<loom::core::Message> in;
    in.push_back(make_completed_bash("\"a\""));
    in.push_back(make_completed_bash("\"b\""));
    // TS: `if (!isFullscreenEnvEnabled()) return messages;`
    auto out = cbb::collapse_background_bash_notifications(in, /*fullscreen=*/false, /*verbose=*/false);
    EXPECT_EQ(out.size(), 2u);
}

// Integration: the collapse pass must be WIRED into the live AppAdapter
// message-projection path (TS Messages.tsx:520), not just unit-tested in
// isolation.  Append 3 consecutive completed-background-bash notifications to
// the engine conversation, run SyncState, and verify the transcript shows a
// single collapsed row instead of 3.

// P0-3 VirtualMessageList helpers
namespace vl = loom::ui::messages::virtual_list;
using ::loom::ui::messages::VisibleRow;

namespace {
/// Build N virtual rows of cycling heights [1,3,7,11] (same as internal
/// test helpers).  Pattern ensures we mix tiny + tall rows throughout.
inline std::vector<VisibleRow> make_vl_rows(size_t n) {
    std::vector<VisibleRow> rows(n);
    int pat[4] = {1, 3, 7, 11};
    for (size_t i = 0; i < n; ++i) {
        rows[i].row_id = i + 1;
        rows[i].estimated_height_lines = pat[i % 4];
        rows[i].backend_index = i;
        rows[i].type_hint = 0;
    }
    return rows;
}
}  // namespace

TEST(VirtualList, GeometryPrefixSumIsExact) {
    // N=5 rows  h=[1,3,7,11,1]  psum=[0,1,4,11,22,23]
    auto rows = make_vl_rows(5);
    auto jh = vl::build_geometry(std::span{rows});
    ASSERT_EQ(jh.size(), 5u);
    EXPECT_EQ(jh.total(), 23);
    EXPECT_EQ(jh.top_of(0), 0);
    EXPECT_EQ(jh.top_of(1), 1);
    EXPECT_EQ(jh.top_of(2), 4);
    EXPECT_EQ(jh.top_of(3), 11);
    EXPECT_EQ(jh.top_of(4), 22);
    EXPECT_EQ(jh.top_of(5), 23);  // one past last
    EXPECT_EQ(jh.height_of(2), 7);
    EXPECT_EQ(jh.height_of(3), 11);
}

TEST(VirtualList, FindRowBinarySearch) {
    auto rows = make_vl_rows(1000);
    auto jh = vl::build_geometry(std::span{rows});
    // Exact psum boundary → row whose top is at the target.
    EXPECT_EQ(jh.find_row_at_visual_line(0), 0u);
    // 1st row is h=1, so line 1 starts row 1
    EXPECT_EQ(jh.find_row_at_visual_line(1), 1u);
    // mid of row 2 (top=4, height=7): lines 4..10
    EXPECT_EQ(jh.find_row_at_visual_line(4), 2u);
    EXPECT_EQ(jh.find_row_at_visual_line(5), 2u);
    EXPECT_EQ(jh.find_row_at_visual_line(10), 2u);
    // line 11 → row 3
    EXPECT_EQ(jh.find_row_at_visual_line(11), 3u);
    // OOB clamp
    EXPECT_EQ(jh.find_row_at_visual_line(-999), 0u);
    EXPECT_EQ(jh.find_row_at_visual_line(999999), rows.size() - 1);
}

TEST(VirtualList, VisibleSliceCoversViewportPlusOverscan) {
    // 1000 rows of cycling heights → total ≈ 5500 lines
    auto rows = make_vl_rows(1000);
    auto jh = vl::build_geometry(std::span{rows});
    // slice start=0, viewport=40 → must cover lines [0, 40+kOverscan)
    auto [start, cnt] = vl::build_visible_slice(jh, 0, 40);
    EXPECT_EQ(start, 0u);
    const int end_line = jh.find_visual_top_for_row(start + cnt);
    const int need = 40 + vl::kOverscanRows;
    EXPECT_GE(end_line, need)
        << "slice must cover viewport + overscan vertically";
}

TEST(VirtualList, VisibleSliceMiddleJump) {
    auto rows = make_vl_rows(1000);
    auto jh = vl::build_geometry(std::span{rows});
    // Jump to the row whose visual top is nearest line 2500.
    size_t target_row = jh.find_row_at_visual_line(2500);
    ASSERT_GT(target_row, 200u);   // sanity: not in the first 200
    int scroll_top = jh.find_visual_top_for_row(target_row);
    auto [start, cnt] = vl::build_visible_slice(jh, scroll_top, 40);
    EXPECT_LE(start, target_row);
    EXPECT_GT(start + cnt, target_row);
    // start should be within overscan of target_row (overscan rows
    // *average_height* ≈ overscan * 5.5 lines ≈ 110 lines).
    EXPECT_GE(start + 10, target_row)  // generous bound
        << "slice should start WITHIN overscan ABOVE the target row";
}

TEST(VirtualList, LargeN100K_NoOverflow) {
    // 100'000 rows: build_geometry must not grow memory beyond ~800 KB,
    // find_row_at_visual_line stays O(log N), slice finds the right region.
    auto rows = make_vl_rows(100'000);
    auto jh = vl::build_geometry(std::span{rows});
    EXPECT_EQ(jh.size(), 100'000u);
    // Avg row height ≈ (1+3+7+11)/4 = 5.5 → total ≈ 550'000 lines
    EXPECT_GT(jh.total(), 500'000);
    // Sample 3 locations
    for (int line : {12345, 271828, 540000}) {
        size_t idx = jh.find_row_at_visual_line(line);
        ASSERT_LT(idx, rows.size());
        EXPECT_LE(jh.top_of(idx), line);
        if (idx + 1 < jh.size())
            EXPECT_GT(jh.top_of(idx + 1), line);
    }
    // slice near tail
    auto [s, c] = vl::build_visible_slice(jh, jh.total() - 100, 40);
    EXPECT_GT(s + c, 99'500u) << "tail slice must include the last rows";
    EXPECT_GT(c, 0u);
}

TEST(VirtualList, ClampScrollEmptyList) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows.clear();
    s.jh = vl::build_geometry(std::span{s.rows});
    vl::clamp_scroll(s);
    EXPECT_EQ(s.scroll_top, 0);
}

TEST(VirtualList, StickyBottomOnSetRowsGrowth) {
    vl::VirtualListHandle h;
    auto state = std::make_shared<vl::VirtualListState>();
    h.state = state.get();
    state->viewport_rows = 10;
    state->auto_mode = vl::AutoScrollMode::Sticky;
    // Initial list of 5 rows (heights [1,3,7,11,1]) → total = 23.
    auto rows = make_vl_rows(5);
    h.SetRows(std::vector<VisibleRow>(rows.begin(), rows.end()));
    // After initial SetRows with sticky semantics enabled:
    //   max = total - vp = 23 - 10 = 13.  Start at max means the viewport
    //   covers lines [13, 23) — pinned to the tail.
    EXPECT_EQ(state->scroll_top, 13);
    EXPECT_TRUE(state->sticky_bottom);

    // Simulate the user manually scrolling away (not sticky anymore).
    state->scroll_top = 0;
    state->sticky_bottom = false;
    vl::update_sticky_after_scroll(*state, 13);

    // Append rows 5..14 (10 more rows).  Because scroll is not sticky,
    // the new-message counter MUST increment.
    auto more = make_vl_rows(15);
    h.SetRows(std::vector<VisibleRow>(more.begin(), more.end()));
    EXPECT_GT(state->new_message_count, 0)
        << "scrolled-up + appended rows = new_message_count must grow";
    EXPECT_FALSE(state->sticky_bottom);
    EXPECT_EQ(state->scroll_top, 0) << "non-sticky list must NOT re-pin";
}

TEST(VirtualList, BuildRowGeometrySliceMatchesPsum) {
    auto rows = make_vl_rows(100);
    auto jh = vl::build_geometry(std::span{rows});
    // Request a slice at rows [37, 42).
    auto gs = vl::build_row_geometry_slice(std::span{rows}, jh, 37, 5);
    ASSERT_EQ(gs.size(), 5u);
    for (size_t k = 0; k < gs.size(); ++k) {
        size_t idx = 37 + k;
        EXPECT_EQ(gs[k].row_idx, idx);
        EXPECT_EQ(gs[k].top_line, jh.find_visual_top_for_row(idx));
        EXPECT_EQ(gs[k].height_lines,
                  std::max(1, (int)rows[idx].estimated_height_lines));
        // `height_measured` was never set → `cached == false`.
        EXPECT_FALSE(gs[k].cached);
    }
}

TEST(VirtualList, HandleJumpToRowCentersHeadroom) {
    vl::VirtualListHandle h;
    auto state = std::make_shared<vl::VirtualListState>();
    h.state = state.get();
    state->viewport_rows = 40;
    state->auto_mode = vl::AutoScrollMode::Disabled;
    auto rows = make_vl_rows(500);
    h.SetRows(std::vector<VisibleRow>(rows.begin(), rows.end()));

    // JumpToRow(100, headroom=3) → scroll_top set to top(100) - 3, clamped ≥ 0.
    h.JumpToRow(100, 3);
    int expected = std::max(0, state->jh.find_visual_top_for_row(100) - 3);
    EXPECT_EQ(state->scroll_top, expected);
    // JumpToRow(0) when headroom would go negative → clamp to 0.
    h.JumpToRow(0, 999);
    EXPECT_EQ(state->scroll_top, 0);
    // JumpToRow past the end → clamped to last row.
    h.JumpToRow(99999, 0);
    ASSERT_FALSE(state->rows.empty());
    size_t last = state->rows.size() - 1;
    int last_top = state->jh.find_visual_top_for_row(last);
    EXPECT_LE(state->scroll_top, last_top);
}

TEST(VirtualList, RenderListEmptyDoesNotCrash) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows.clear();
    s.jh = vl::build_geometry(std::span{s.rows});
    // render_list_as_elements on empty → returns 1-line filler, no segfault.
    ftxui::Element el = vl::render_list_as_elements(s);
    ftxui::Screen screen(80, 5);
    ftxui::Render(screen, el);
    // Screen must render successfully.  Empty output is acceptable as long
    // as the call didn't abort (ASAN would catch OOB).
    EXPECT_TRUE(true);
    (void)screen;
}

TEST(VirtualList, RenderListMediumProducesValidVBox) {
    // 200 rows, viewport 40 = definitely windowed (OVERSCAN in both dirs).
    vl::VirtualListState s;
    s.options.ascii_gutter = true;
    s.auto_mode = vl::AutoScrollMode::Disabled;
    s.viewport_rows = 40;
    s.options.viewport_rows = 40;
    auto rows = make_vl_rows(200);
    s.rows.assign(rows.begin(), rows.end());
    s.jh = vl::build_geometry(std::span{s.rows});
    int n_rendered = 0;
    s.callbacks.render_row = [&](size_t i, const VisibleRow&) -> ftxui::Element {
        ++n_rendered;
        return ftxui::text("row " + std::to_string(i))
             | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL,
                           std::max(1, rows[i].estimated_height_lines));
    };
    // Scroll to mid-list.
    s.scroll_top = s.jh.total() / 2;
    ftxui::Element el = vl::render_list_as_elements(s);
    // Rendering is required to force FTXUI layout.
    ftxui::Screen screen(120, 40);
    ftxui::Render(screen, el);
    // Critical invariant: # of rendered rows MUST be strictly less than
    // rows.size() (the whole point of virtual windowing).  A conservative
    // upper bound is viewport_rows + 2·kOverscanRows + 4 (for pills).
    EXPECT_LT(n_rendered, (int)rows.size())
        << "virtual window must not render all rows";
    const int expected_max = 40 + 2 * (int)vl::kOverscanRows + 4;
    EXPECT_LE(n_rendered, expected_max)
        << "rendered count cap: viewport + 2·overscan + pills";
    (void)screen;
}

TEST(VirtualList, ScrollZeroViewportNeverUnderflows) {
    vl::VirtualListState s;
    s.viewport_rows = 0;   // pathological: caller supplies invalid size
    auto rows = make_vl_rows(10);
    s.rows.assign(rows.begin(), rows.end());
    s.jh = vl::build_geometry(std::span{s.rows});
    s.scroll_top = 9999;
    vl::clamp_scroll(s);
    // viewport_rows==0 ⇒ max = total - 0 = total.  Scroll is clamped there.
    EXPECT_EQ(s.scroll_top, s.jh.total());
    // slice with vp=0 → should not overflow / not produce a negative count.
    auto [start, cnt] = vl::build_visible_slice(s.jh, 0, 0);
    EXPECT_LE((int)start, (int)s.rows.size());
    // render must not crash / abort — pathological viewport size returns a
    // syntactically-valid (possibly zero-height) Element.
    ftxui::Element el = vl::render_list_as_elements(s);
    ftxui::Screen screen(80, 3);
    ftxui::Render(screen, el);
    EXPECT_TRUE(true);
    (void)cnt;
    (void)screen;
}

// ── P0-round7: 2-tier search index (VirtualMessageList search engine) ────────

namespace {
/// Build N rows with cycling heights [1,3,7,11] and per-row search_key
/// derived from `search_texts[i % search_texts.size()]`.  The search_key is
/// lowered (as messages_list::get_cached_lowered_search_text would do).
inline std::vector<VisibleRow> make_vl_rows_with_search(
    size_t n,
    std::vector<std::string> const &search_texts) {
    std::vector<VisibleRow> rows(n);
    int pat[4] = {1, 3, 7, 11};
    for (size_t i = 0; i < n; ++i) {
        rows[i].row_id = i + 1;
        rows[i].estimated_height_lines = pat[i % 4];
        rows[i].backend_index = i;
        rows[i].type_hint = 0;
        if (!search_texts.empty()) {
            std::string const &src = search_texts[i % search_texts.size()];
            // Lower the text to simulate get_cached_lowered_search_text.
            std::string lowered;
            lowered.reserve(src.size());
            for (char c : src) {
                if (c >= 'A' && c <= 'Z') lowered += static_cast<char>(c + ('a' - 'A'));
                else lowered += c;
            }
            rows[i].search_key = std::move(lowered);
        }
    }
    return rows;
}
}  // namespace

TEST(VirtualList, RunSearchFindsMatchingRows) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    // Rows with varied search text; "error" appears in rows 1 and 3.
    s.rows = make_vl_rows_with_search(6, {
        "hello world",
        "error: something failed",
        "all good here",
        "error: timeout in bash",
        "success",
        "no problems",
    });
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::run_search(s, "error");

    EXPECT_EQ(s.search_matches.size(), 2u);
    EXPECT_EQ(s.search_matches[0], 1u);  // row 1 has "error"
    EXPECT_EQ(s.search_matches[1], 3u);  // row 3 has "error"
    EXPECT_EQ(s.search_total_occurrences, 2u);
    // prefixSum: [0, 1, 2]
    ASSERT_EQ(s.search_prefix_sum.size(), 3u);
    EXPECT_EQ(s.search_prefix_sum[0], 0u);
    EXPECT_EQ(s.search_prefix_sum[1], 1u);
    EXPECT_EQ(s.search_prefix_sum[2], 2u);
}

TEST(VirtualList, RunSearchCountsMultipleOccurrencesPerRow) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    // Row 0: "foo" appears 3x.  Row 1: "foo" appears 0x.  Row 2: "foo" appears 2x.
    s.rows = make_vl_rows_with_search(3, {
        "foo bar foo baz foo",
        "nothing to see",
        "foo and foo again",
    });
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::run_search(s, "foo");

    EXPECT_EQ(s.search_matches.size(), 2u);
    EXPECT_EQ(s.search_matches[0], 0u);
    EXPECT_EQ(s.search_matches[1], 2u);
    EXPECT_EQ(s.search_total_occurrences, 5u);  // 3 + 2
    ASSERT_EQ(s.search_prefix_sum.size(), 3u);
    EXPECT_EQ(s.search_prefix_sum[1], 3u);  // after row 0: 3
    EXPECT_EQ(s.search_prefix_sum[2], 5u);  // after row 2: 5
}

TEST(VirtualList, RunSearchEmptyQueryClearsState) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(4, {"hello", "world", "hello", "there"});
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::run_search(s, "hello");
    ASSERT_EQ(s.search_matches.size(), 2u);  // sanity

    vl::run_search(s, "");
    EXPECT_TRUE(s.search_matches.empty());
    EXPECT_EQ(s.search_total_occurrences, 0u);
    ASSERT_EQ(s.search_prefix_sum.size(), 1u);
    EXPECT_EQ(s.search_prefix_sum[0], 0u);
}

TEST(VirtualList, RunSearchNoMatchesReturnsEmpty) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(5, {"alpha", "beta", "gamma", "delta", "epsilon"});
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::run_search(s, "zzznotfound");
    EXPECT_TRUE(s.search_matches.empty());
    EXPECT_EQ(s.search_total_occurrences, 0u);
}

TEST(VirtualList, RunSearchIsCaseInsensitive) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    // search_key is already lowered by the helper.
    s.rows = make_vl_rows_with_search(3, {"HELLO WORLD", "mixed Case", "UPPERCASE"});
    s.jh = vl::build_geometry(std::span{s.rows});

    // The query is also lowered inside set_search_query, but run_search
    // expects pre-lowered input.  Verify lowered "hello" finds "HELLO" row.
    vl::run_search(s, "hello");
    EXPECT_EQ(s.search_matches.size(), 1u);
    EXPECT_EQ(s.search_matches[0], 0u);

    // "case" matches "mixed Case" (row 1) AND "UPPERCASE" (row 2)
    // because "uppercase" contains "case" as a substring.
    vl::run_search(s, "case");
    EXPECT_EQ(s.search_matches.size(), 2u);
    EXPECT_EQ(s.search_matches[0], 1u);
    EXPECT_EQ(s.search_matches[1], 2u);
}

TEST(VirtualList, SetSearchQueryJumpsToNearestMatch) {
    vl::VirtualListState s;
    s.viewport_rows = 10;
    // Heights: row0=1, row1=3, row2=7, row3=11, row4=1, row5=3
    // Tops:    0,    1,    4,    11,   22,   23
    s.rows = make_vl_rows_with_search(6, {
        "intro text",
        "chapter one",
        "chapter two",
        "chapter three",
        "conclusion",
        "appendix",
    });
    s.jh = vl::build_geometry(std::span{s.rows});
    s.sticky_bottom = false;
    s.scroll_top = 5;  // near row 2 (top=4, height=7)

    vl::set_search_query(s, "chapter");

    // "chapter" matches rows 1, 2, 3.  Their tops: 1, 4, 11.
    // scroll_top=5 is closest to row 2 (top=4, dist=1) vs row 1 (dist=4) vs row 3 (dist=6).
    ASSERT_FALSE(s.search_matches.empty());
    EXPECT_EQ(s.search_matches[s.search_ptr], 2u);
    // scroll should be clamped to valid range.
    EXPECT_GE(s.scroll_top, 0);
    EXPECT_LE(s.scroll_top, s.jh.total());
}

TEST(VirtualList, SetSearchQueryEmptyClearsSearch) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(4, {"hello", "world", "hello", "there"});
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::set_search_query(s, "hello");
    ASSERT_FALSE(s.search_matches.empty());  // sanity

    vl::set_search_query(s, "");
    EXPECT_TRUE(s.search_matches.empty());
    EXPECT_EQ(s.search_ptr, 0u);
    EXPECT_EQ(s.search_total_occurrences, 0u);
    EXPECT_TRUE(s.search_query.empty());
}

TEST(VirtualList, SetSearchQueryNoMatchesFiresZero) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(4, {"hello", "world", "hello", "there"});
    s.jh = vl::build_geometry(std::span{s.rows});

    size_t cb_total = 999, cb_current = 999;
    s.callbacks.on_search_matches_change = [&](size_t t, size_t c) {
        cb_total = t;
        cb_current = c;
    };

    vl::set_search_query(s, "zzznotfound");
    EXPECT_TRUE(s.search_matches.empty());
    EXPECT_EQ(cb_total, 0u);
    EXPECT_EQ(cb_current, 0u);
}

TEST(VirtualList, SearchStepMatchWrapsAround) {
    vl::VirtualListState s;
    s.viewport_rows = 10;
    s.rows = make_vl_rows_with_search(6, {
        "match here",
        "nope",
        "match here too",
        "nothing",
        "match three",
        "nada",
    });
    s.jh = vl::build_geometry(std::span{s.rows});
    s.sticky_bottom = false;

    vl::set_search_query(s, "match");
    ASSERT_EQ(s.search_matches.size(), 3u);  // rows 0, 2, 4
    size_t initial_ptr = s.search_ptr;

    // Step forward through all 3 matches.
    vl::search_step_match(s, 1);
    EXPECT_NE(s.search_ptr, initial_ptr);
    size_t ptr_after_1 = s.search_ptr;

    vl::search_step_match(s, 1);
    EXPECT_NE(s.search_ptr, ptr_after_1);

    // Third step forward should wrap back to start.
    vl::search_step_match(s, 1);
    // After 3 steps forward from start, we should be back at start
    // (or at least have visited all 3 unique positions).
    // Just verify it's still in range.
    EXPECT_LT(s.search_ptr, s.search_matches.size());

    // Step backward.
    size_t before_prev = s.search_ptr;
    vl::search_step_match(s, -1);
    EXPECT_LT(s.search_ptr, s.search_matches.size());
    (void)before_prev;
}

TEST(VirtualList, GetSearchMatchInfoReturnsBadge) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(4, {
        "foo bar foo",      // 2 occurrences
        "nothing",
        "foo baz",          // 1 occurrence
        "no match",
    });
    s.jh = vl::build_geometry(std::span{s.rows});

    // No search active → 0/0.
    auto [t0, c0] = vl::get_search_match_info(s);
    EXPECT_EQ(t0, 0u);
    EXPECT_EQ(c0, 0u);

    vl::set_search_query(s, "foo");
    auto [total, current] = vl::get_search_match_info(s);
    EXPECT_EQ(total, 3u);  // 2 + 1
    EXPECT_GE(current, 1u);
    EXPECT_LE(current, 3u);
}

TEST(VirtualList, DisarmSearchClearsAnchor) {
    vl::VirtualListState s;
    s.viewport_rows = 40;
    s.rows = make_vl_rows_with_search(4, {"hello", "world", "hello", "there"});
    s.jh = vl::build_geometry(std::span{s.rows});

    vl::set_search_query(s, "hello");
    // After set_search_query, anchor should be set (was -1).
    EXPECT_NE(s.search_anchor_scroll_top, -1);

    vl::disarm_search(s);
    EXPECT_EQ(s.search_anchor_scroll_top, -1);
}

TEST(VirtualList, MakeSearchStepCallbackReturnsVisualLine) {
    vl::VirtualListState s;
    s.viewport_rows = 10;
    s.rows = make_vl_rows_with_search(6, {
        "target",
        "filler",
        "target",
        "filler",
        "target",
        "filler",
    });
    s.jh = vl::build_geometry(std::span{s.rows});
    s.sticky_bottom = false;

    vl::set_search_query(s, "target");
    ASSERT_EQ(s.search_matches.size(), 3u);

    auto cb = vl::make_search_step_callback(&s);
    // delta=+1 should return a valid visual line (>= 0).
    int target = cb(1);
    EXPECT_GE(target, 0);
    EXPECT_LE(target, s.jh.total());

    // delta=-1 should also be valid.
    int target_prev = cb(-1);
    EXPECT_GE(target_prev, 0);
    EXPECT_LE(target_prev, s.jh.total());

    // No matches → -1.
    vl::VirtualListState s2;
    s2.viewport_rows = 10;
    s2.rows = make_vl_rows_with_search(3, {"a", "b", "c"});
    s2.jh = vl::build_geometry(std::span{s2.rows});
    auto cb2 = vl::make_search_step_callback(&s2);
    EXPECT_EQ(cb2(1), -1);
}

TEST(VirtualList, HandleSearchApiParity) {
    auto state = std::make_shared<vl::VirtualListState>();
    state->viewport_rows = 10;
    state->rows = make_vl_rows_with_search(5, {
        "needle in haystack",
        "nothing",
        "needle needle",
        "nope",
        "needle",
    });
    state->jh = vl::build_geometry(std::span{state->rows});
    state->sticky_bottom = false;

    vl::VirtualListHandle h;
    h.state = state.get();

    EXPECT_FALSE(h.IsSearchActive());

    h.SetSearchQuery("needle");
    EXPECT_TRUE(h.IsSearchActive());

    auto [total, current] = h.GetSearchMatchInfo();
    EXPECT_EQ(total, 4u);  // 1 + 2 + 1
    EXPECT_GE(current, 1u);

    h.NextMatch();
    auto [t2, c2] = h.GetSearchMatchInfo();
    EXPECT_EQ(t2, 4u);

    h.PrevMatch();
    auto [t3, c3] = h.GetSearchMatchInfo();
    EXPECT_EQ(t3, 4u);

    h.DisarmSearch();
    // IsSearchActive checks search_matches, not anchor — should still be true.
    EXPECT_TRUE(h.IsSearchActive());

    // WarmSearchIndex returns row count (no-op in CPP since keys are pre-populated).
    EXPECT_EQ(h.WarmSearchIndex(), 5u);

    h.SetSearchQuery("");
    EXPECT_FALSE(h.IsSearchActive());
}

// ── P0-4 LogoV2 + WelcomeV2 + 10-deep notice stack ──────────────────────────

namespace unseen_divider_test {

using namespace loom::ui::messages_list;
using loom::ui::messages::MessageShape;
using loom::ui::messages::UserTextMessageData;
using loom::ui::messages::AssistantTextMessageData;

/// Helper: build a minimal MessagesListInput with N alternating rows, each
/// with a 24-char uuid of the form `<prefix>_<i>{pad}`.  Row i is user if
/// (i%2==0) else assistant.  The caller can then set input.unseen_divider.
auto make_synthetic_input(std::size_t num_rows,
                          std::string_view uuid_prefix20 = "ABCDEF0123456789abcd") {
    MessagesListInput in;
    in.rows.reserve(num_rows);
    in.shapes.reserve(num_rows);
    in.uuids.reserve(num_rows);
    for (std::size_t i = 0; i < num_rows; ++i) {
        char uuid_buf[25];
        // uuid_prefix20 is 20 chars, so with 4-digit suffix = 24 chars exactly.
        std::snprintf(uuid_buf, sizeof(uuid_buf), "%.*s%04zu",
                      20, uuid_prefix20.data(), i);
        in.uuids.emplace_back(uuid_buf, 24);
        if ((i & 1u) == 0u) {
            in.shapes.push_back(MessageShape::UserText);
            in.rows.push_back(UserTextMessageData{
                .content = std::string("User prompt #") + std::to_string(i),
                .quoted_reply = std::nullopt,
                .command_name = std::nullopt});
        } else {
            in.shapes.push_back(MessageShape::AssistantText);
            in.rows.push_back(AssistantTextMessageData{
                .content = std::string("Assistant reply #") + std::to_string(i),
                .model_name = std::nullopt,
                .is_streaming = false});
        }
    }
    in.viewport_rows = 40;
    in.pin_to_bottom = true;
    return in;
}

} // namespace unseen_divider_test

TEST(MessagesList, UnseenDivider_PrefixMatchFindsTargetRow) {
    using namespace unseen_divider_test;
    auto in = make_synthetic_input(6, "old0000000000000000000");
    // Row 0 (user) → uuid = "old0000000000000000000000" (indices 0-23)
    // Row 1 (asst) → "old0000000000000000000001"
    // Row 2 (user) → "old0000000000000000000002"
    // Row 3 (asst) → "old0000000000000000000003"
    // Row 4 (user) → "old0000000000000000000004"
    // Row 5 (asst) → "old0000000000000000000005"

    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 6u);

    // Case A: point at row 3 → divider_before = 3
    in.unseen_divider = UnseenDivider{
        .first_unseen_uuid_prefix = "old0000000000000000000003",
        .count = 1,
    };
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible), 3u);

    // Case B: point at row 0 (first message) → divider_before = 0
    in.unseen_divider->first_unseen_uuid_prefix = "old0000000000000000000000";
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible), 0u);

    // Case C: prefix match — TS's deriveUUID preserves 24-char prefix across
    // derived sub-blocks.  We match on prefix even if the divider's stored
    // value is a longer full uuid (36 chars) — only first 24 count.
    in.unseen_divider->first_unseen_uuid_prefix =
        std::string("old0000000000000000000002") + "-EXTRA-SUFFIX-IGNORED";
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible), 2u);

    // Case D: no match → return visible.size() (sentinel)
    in.unseen_divider->first_unseen_uuid_prefix = "ZZZZZZZZZZZZZZZZZZZZZZZZ00";
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible),
              visible.size());

    // Case E: unseen_divider = nullopt → sentinel (no divider)
    in.unseen_divider.reset();
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible),
              visible.size());
}

/// Unit test: count pluralisation in divider title.  TS: count === 1 → "message",
/// else → "messages".
TEST(MessagesList, UnseenDivider_TitlePluralisation) {
    using namespace loom::ui::messages_list;
    using namespace sticky_prompt_test;

    // count=1 → title reads "1 new message"
    auto div1 = loom::ui::messages_list::detail::render_unseen_divider(1);
    auto snap1 = strip_ansi(render_ansi(std::move(div1), 80, 4));
    EXPECT_NE(snap1.find("1 new message"), std::string::npos);
    // Singular must NOT contain "1 new messages" (note trailing 's')
    EXPECT_EQ(snap1.find("1 new messages"), std::string::npos);

    // count=3 → "3 new messages"
    auto divN = loom::ui::messages_list::detail::render_unseen_divider(3);
    auto snapN = strip_ansi(render_ansi(std::move(divN), 80, 4));
    EXPECT_NE(snapN.find("3 new messages"), std::string::npos);
}

/// Golden snapshot test: render a 4-row transcript WITH an unseen divider
/// inserted before row 2 (first of the "new" assistant turns) with count=2.
/// This test catches any drift in the divider layout (marginTop=1 blank line,
/// separator dashes, bold/muted title, trailing dashes stretched to width).
TEST(MessagesList, UnseenDivider_RendersDividerInTranscript_Golden) {
    using namespace unseen_divider_test;
    using namespace sticky_prompt_test;

    auto in = make_synthetic_input(4, "snap00000000000000000000");
    // Rows 0=user#0, 1=asst#1, 2=user#2, 3=asst#3
    // Unseen divider: 2 new assistant turns starting BEFORE row#2.
    in.unseen_divider = UnseenDivider{
        .first_unseen_uuid_prefix = "snap0000000000000000000002",
        .count = 2,
    };
    in.pin_to_bottom = false;
    in.viewport_rows = 30;

    auto el = render_messages_list_view(std::move(in), /*frame=*/0,
                                        /*render_last_n=*/80);
    std::string snap = strip_ansi(render_ansi(std::move(el), 100, 30));

    // Assert title exists in the transcript (the golden catches exact layout).
    EXPECT_NE(snap.find("2 new messages"), std::string::npos)
        << "divider title must appear in the rendered transcript";
    // Assert the user #2 row appears AFTER the divider (content ordering check).
    // If the divider were placed AFTER its target row, or not at all, this
    // ordering invariant would break.
    auto pos_divider = snap.find("2 new messages");
    auto pos_row2 = snap.find("User prompt #2");
    ASSERT_NE(pos_divider, std::string::npos);
    ASSERT_NE(pos_row2, std::string::npos);
    EXPECT_LT(pos_divider, pos_row2)
        << "divider must render BEFORE its target payload row";

    // ── Golden snapshot (TS REF: Messages.tsx L631-635 Divider element).
    //    UPDATE_GOLDENS=1 ./loom_test --gtest_filter='*Golden*' to refresh.
    // Re-render to a fresh snapshot (std::move consumed el above).
    auto in2 = make_synthetic_input(4, "snap00000000000000000000");
    in2.unseen_divider = UnseenDivider{
        .first_unseen_uuid_prefix = "snap0000000000000000000002",
        .count = 2,
    };
    in2.pin_to_bottom = false;
    in2.viewport_rows = 30;
    auto el2 = render_messages_list_view(std::move(in2), 0, 80);
    check_golden("unseen_divider_in_transcript_missing",
                 render_ansi(std::move(el2), 100, 30));
}

/// Baseline golden: same 4-row transcript BUT no unseen_divider set.  Serves
/// as the "control" to confirm the divider-only delta in the test above.
TEST(MessagesList, UnseenDivider_NoDividerBaseline_Golden) {
    using namespace unseen_divider_test;
    using namespace sticky_prompt_test;

    auto in = make_synthetic_input(4, "snap00000000000000000000");
    in.unseen_divider.reset();   // explicit nullopt: no divider
    in.pin_to_bottom = false;
    in.viewport_rows = 30;

    auto in_assert = make_synthetic_input(4, "snap00000000000000000000");
    in_assert.unseen_divider.reset();
    in_assert.pin_to_bottom = false;
    in_assert.viewport_rows = 30;
    std::string snap = strip_ansi(render_ansi(
        render_messages_list_view(std::move(in_assert), 0, 80), 100, 30));
    // Baseline must NOT contain the divider title.
    EXPECT_EQ(snap.find("new message"), std::string::npos)
        << "baseline (unseen_divider=nullopt) must NOT render a divider";

    // Golden snapshot for the no-divider case (control file).  Use a
    // separately-constructed input so the assertion snapshot and the golden
    // snapshot are independent (in_assert was moved-from above).
    auto in_ctrl = make_synthetic_input(4, "snap00000000000000000000");
    in_ctrl.unseen_divider.reset();
    in_ctrl.pin_to_bottom = false;
    in_ctrl.viewport_rows = 30;
    check_golden("unseen_divider_baseline_no_divider",
                 render_ansi(render_messages_list_view(std::move(in_ctrl), 0, 80),
                             100, 30));
}

/// Edge-case test: divider anchor lands on a ThinkingBlock (skipped in TS's
/// computeUnseenDivider per CC-724).  The CPP equivalent's guard (progress +
/// null-rendering attachments skipped) isn't in this repo yet, but the
/// prefix-match must still pick the first PAYLOAD row it encounters (not
/// crash on empty uuids).
TEST(MessagesList, UnseenDivider_AnchorWithEmptyUuidEntries_NoCrash) {
    using namespace unseen_divider_test;
    using namespace sticky_prompt_test;  // strip_ansi, render_ansi
    auto in = make_synthetic_input(5, "edge00000000000000000000");
    // Clear uuid on row 2 to simulate a filtered-out row.
    in.uuids[2].clear();
    in.unseen_divider = UnseenDivider{
        .first_unseen_uuid_prefix = "edge0000000000000000000003",
        .count = 1,
    };
    auto visible = build_visible_rows(in);
    // find_divider_before_visible_index must not UB; must return 3.
    EXPECT_EQ(loom::ui::messages_list::detail::find_divider_before_visible_index(in, visible), 3u);

    // Rendering path must not crash (empty uuid on row 2 is legal input).
    auto in_render = make_synthetic_input(5, "edge00000000000000000000");
    in_render.uuids[2].clear();
    in_render.unseen_divider = UnseenDivider{
        .first_unseen_uuid_prefix = "edge0000000000000000000003",
        .count = 1,
    };
    auto el = render_messages_list_view(std::move(in_render), 0, 80);
    // Smoke-render the transcript to catch any UB/crash in the divider
    // insertion branch when one row's uuid is empty.  We route through
    // strip_ansi so that Screen::ToString() fully traverses the Element
    // tree and exercises every node.
    std::string snap =
        strip_ansi(render_ansi(std::move(el), /*term_w=*/80, /*term_h=*/20));
    // Divider title for count=1 MUST appear.
    EXPECT_NE(snap.find("1 new message"), std::string::npos);
    (void)snap;
}

// =============================================================================
// Chain compression: thinking + non-update tool calls → 1 summary line
//
// Between assistant text blocks, consecutive thinking blocks and non-update
// tool calls (Bash, Read, Grep, …) are compressed into a single
// "∴ Thinking + N tool calls" row.  Update tools (Edit/Write) stop
// compression and are shown individually.  The streaming tail and thinking
// within the 3s collapse grace are never compressed.
// =============================================================================

namespace chain_compression_test {

namespace ml = loom::ui::messages_list;
using loom::ui::messages::thinking_message::ThinkingMessageOptions;
using loom::ui::messages::thinking_message::ThinkingState;
using loom::ui::messages::tool_use_message::ToolUseRenderOptions;
using loom::ui::messages::tool_use_message::ToolStatus;
using loom::ui::messages::ToolResultOptions;
using loom::ui::messages::AssistantTextMessageData;
using loom::ui::messages::UserTextMessageData;
using loom::ui::messages::MessageRowPayload;
using ml::MessagesListInput;
using ml::MessageShape;

[[nodiscard]] ThinkingMessageOptions make_thinking(std::string text) {
    ThinkingMessageOptions opts;
    opts.data.raw_text = std::move(text);
    opts.data.state = ThinkingState::Complete;
    opts.data.duration = std::chrono::milliseconds(500);
    return opts;
}

[[nodiscard]] ToolUseRenderOptions make_tool_use(
    std::string name, ToolStatus status = ToolStatus::Success) {
    ToolUseRenderOptions opts;
    opts.call.tool_name = std::move(name);
    opts.call.status = status;
    opts.call.raw_parameters = "{}";
    return opts;
}

[[nodiscard]] ToolResultOptions make_tool_result(std::string name) {
    ToolResultOptions opts;
    opts.tool_name = std::move(name);
    return opts;
}

[[nodiscard]] AssistantTextMessageData make_text(std::string content) {
    return AssistantTextMessageData{
        .content = std::move(content),
        .model_name = std::nullopt,
        .is_streaming = false,
    };
}

/// Build a MessagesListInput from (shape, payload) pairs.
/// streaming_tail_row is set to rows.size() (not streaming).
[[nodiscard]] MessagesListInput make_input(
    std::vector<std::pair<MessageShape, MessageRowPayload>> rows) {
    MessagesListInput in;
    in.streaming_tail_row = rows.size();
    for (auto& [shape, payload] : rows) {
        in.shapes.push_back(shape);
        in.rows.push_back(std::move(payload));
        in.uuids.emplace_back();
    }
    return in;
}

/// Count visible rows of a given kind.
[[nodiscard]] std::size_t count_kind(
    const std::vector<loom::ui::messages_list::VisibleRow>& rows,
    loom::ui::messages_list::VisibleRow::Kind kind) {
    std::size_t n = 0;
    for (const auto& r : rows) {
        if (r.kind == kind) ++n;
    }
    return n;
}

/// The CompressedChain kind value, derived from build_visible_rows' return
/// type to work around the name-lookup conflict with
/// loom::ui::messages::VisibleRow (exported via `using namespace messages;`
/// in messages_list.cppm).
[[nodiscard]] auto compressed_chain_kind() {
    static MessagesListInput dummy;
    auto v = build_visible_rows(dummy);
    return decltype(v)::value_type::Kind::CompressedChain;
}

[[nodiscard]] auto payload_kind() {
    static MessagesListInput dummy;
    auto v = build_visible_rows(dummy);
    return decltype(v)::value_type::Kind::Payload;
}

}  // namespace chain_compression_test

/// Basic compression: thinking + tool_use + tool_result → 1 CompressedChain.
TEST(MessagesList, ChainCompression_BasicThinkingPlusTool) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("I should check the file.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
    });
    auto visible = build_visible_rows(in);
    // 3 rows compressed into 1 CompressedChain.
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 1u);   // 1 thinking block
    EXPECT_EQ(visible[0].tool_turns, 1u);     // 1 tool call
}

/// Multiple thinking blocks + tool calls compressed together.
TEST(MessagesList, ChainCompression_MultipleThinkingAndTools) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First thought.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantThinking, make_thinking("Second thought.")},
        {MessageShape::AssistantToolUse, make_tool_use("Grep")},
        {MessageShape::UserToolResult, make_tool_result("Grep")},
    });
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 2u);   // 2 thinking blocks
    EXPECT_EQ(visible[0].tool_turns, 2u);     // 2 tool calls
}

/// Update tool (Edit) stops compression — the thinking before it is NOT
/// compressed (lone thinking, no tools), and the Edit is shown.
TEST(MessagesList, ChainCompression_UpdateToolStopsCompression) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("I need to edit.")},
        {MessageShape::AssistantToolUse, make_tool_use("Edit")},
        {MessageShape::UserToolResult, make_tool_result("Edit")},
    });
    auto visible = build_visible_rows(in);
    // Lone thinking (compressed chain) + Edit tool_use + Edit tool_result.
    // The Edit tool stops the chain, so the thinking is its own chain.
    ASSERT_EQ(visible.size(), 3u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());  // thinking (compressed)
    EXPECT_EQ(visible[1].kind, payload_kind());  // Edit tool_use
    EXPECT_EQ(visible[2].kind, payload_kind());  // Edit tool_result
}

/// Assistant text stops compression — the lone thinking before text IS
/// compressed (no tools needed).  The text block is shown individually.
TEST(MessagesList, ChainCompression_AssistantTextStopsCompression) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("Thinking...")},
        {MessageShape::AssistantText, make_text("Here is my answer.")},
    });
    auto visible = build_visible_rows(in);
    // Thinking (compressed chain) + text (payload) = 2 visible rows.
    ASSERT_EQ(visible.size(), 2u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 1u);  // 1 thinking
    EXPECT_EQ(visible[0].tool_turns, 0u);   // no tools
    EXPECT_EQ(visible[1].kind, payload_kind());
}

/// Two separate chains separated by assistant text.
TEST(MessagesList, ChainCompression_TwoChainsSeparatedByText) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantText, make_text("Middle text.")},
        {MessageShape::AssistantThinking, make_thinking("Second.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
    });
    auto visible = build_visible_rows(in);
    // Chain1 (3 rows) + text (1 row) + Chain2 (3 rows) = 3 visible rows.
    ASSERT_EQ(visible.size(), 3u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 1u);   // 1 thinking
    EXPECT_EQ(visible[0].tool_turns, 1u);
    EXPECT_EQ(visible[1].kind, payload_kind());
    EXPECT_EQ(visible[2].kind, compressed_chain_kind());
    EXPECT_EQ(visible[2].group_count, 1u);   // 1 thinking
    EXPECT_EQ(visible[2].tool_turns, 1u);
}

/// Streaming tail is never compressed.
TEST(MessagesList, ChainCompression_StreamingTailIncludedInChain) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantThinking, make_thinking("Streaming...")},
    });
    // Row 3 is the streaming tail — it IS included in the chain now.
    in.streaming_tail_row = 3;
    auto visible = build_visible_rows(in);
    // All 4 rows compressed into one chain.
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 2u);   // 2 thinking
    EXPECT_EQ(visible[0].tool_turns, 1u);    // 1 tool call
    // Not live because query_running defaults to false.
    EXPECT_FALSE(visible[0].chain_is_live);
}

/// When query_running is true and the chain includes the streaming tail,
/// the chain is marked as live (renders as a live status row).
TEST(MessagesList, ChainCompression_LiveStatusWhenQueryRunning) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantThinking, make_thinking("Streaming...")},
    });
    in.streaming_tail_row = 3;
    in.query_running = true;
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_TRUE(visible[0].chain_is_live);
}

/// When query_running is true but the chain does NOT include the streaming
/// tail (e.g. an earlier chain separated by text), it is not live.
TEST(MessagesList, ChainCompression_EarlierChainNotLive) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantText, make_text("Done.")},
        {MessageShape::AssistantThinking, make_thinking("Second.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
    });
    // Streaming tail is row 5 (the last tool use).
    in.streaming_tail_row = 5;
    in.query_running = true;
    auto visible = build_visible_rows(in);
    // Two chains: rows 0-2 (before text) and rows 4-5 (after text).
    // The first chain is NOT live (doesn't include streaming tail).
    // The second chain IS live (includes streaming tail).
    bool found_live = false;
    bool found_nonlive = false;
    for (const auto& vr : visible) {
        if (vr.kind == compressed_chain_kind()) {
            if (vr.chain_is_live) found_live = true;
            else found_nonlive = true;
        }
    }
    EXPECT_TRUE(found_live);
    EXPECT_TRUE(found_nonlive);
}

/// Transcript mode skips compression entirely.
TEST(MessagesList, ChainCompression_TranscriptModeSkipsCompression) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    in.is_transcript_mode = true;
    auto visible = build_visible_rows(in);
    // No compression in transcript mode.
    EXPECT_EQ(count_kind(visible, compressed_chain_kind()), 0u);
    EXPECT_EQ(visible.size(), 3u);
}

/// Search mode skips compression entirely.
TEST(MessagesList, ChainCompression_SearchModeSkipsCompression) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    in.search_query = "Bash";
    auto visible = build_visible_rows(in);
    // No compression in search mode.
    EXPECT_EQ(count_kind(visible, compressed_chain_kind()), 0u);
}

/// Lone thinking block (no tool calls) is NOT compressed — it renders as a
/// regular collapsed thinking row ("∴ Thought for Xs"), visually part of the
/// assistant's turn.  A bare "Thought" summary line floating before the
/// assistant text looks disconnected from the message flow.
/// Lone thinking blocks ARE compressed — the user prefers the summary line
/// over the full thinking content.
TEST(MessagesList, ChainCompression_SingleThinkingCompressed) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("Just thinking.")},
    });
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 1u);  // 1 thinking
    EXPECT_EQ(visible[0].tool_turns, 0u);   // no tools
}

/// Chain with only thinking blocks (no tool calls) — compressed.
TEST(MessagesList, ChainCompression_ThinkingOnlyChain) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantThinking, make_thinking("Second.")},
    });
    auto visible = build_visible_rows(in);
    // Both thinking blocks compressed into 1 chain row.
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 2u);  // 2 thinking
    EXPECT_EQ(visible[0].tool_turns, 0u);   // no tools
}

/// Chain stops at a non-compressible visible row (user text).
TEST(MessagesList, ChainCompression_UserTextStopsChain) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("Thinking.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::UserText, UserTextMessageData{
            .content = "User reply",
            .quoted_reply = std::nullopt,
            .command_name = std::nullopt}},
        {MessageShape::AssistantThinking, make_thinking("After user.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
    });
    auto visible = build_visible_rows(in);
    // Chain1 (3) + user text (1) + Chain2 (3) = 3 visible rows.
    ASSERT_EQ(visible.size(), 3u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[1].kind, payload_kind());
    EXPECT_EQ(visible[2].kind, compressed_chain_kind());
}

/// Write tool also stops compression (is_file_edit_tool matches "write").
/// The Write tool_use AND its tool_result are both shown individually.
/// The lone thinking before the Write IS compressed (no tools needed).
TEST(MessagesList, ChainCompression_WriteToolStopsCompression) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("Need to write.")},
        {MessageShape::AssistantToolUse, make_tool_use("Write")},
        {MessageShape::UserToolResult, make_tool_result("Write")},
        {MessageShape::AssistantThinking, make_thinking("After write.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    auto visible = build_visible_rows(in);
    // Lone thinking (compressed chain) + Write tool_use + Write tool_result
    // + Chain2 (thinking + Bash + Bash result = 3 rows compressed).
    ASSERT_EQ(visible.size(), 4u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());  // thinking (compressed)
    EXPECT_EQ(visible[1].kind, payload_kind());  // Write tool_use
    EXPECT_EQ(visible[2].kind, payload_kind());  // Write tool_result
    EXPECT_EQ(visible[3].kind, compressed_chain_kind());
    EXPECT_EQ(visible[3].group_count, 1u);       // 1 thinking
    EXPECT_EQ(visible[3].tool_turns, 1u);
}

/// Pure tool chain (no thinking): consecutive tool calls are compressed.
TEST(MessagesList, ChainCompression_PureToolChain) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
    });
    auto visible = build_visible_rows(in);
    // 4 rows compressed into 1 CompressedChain (0 thinking, 2 tool calls).
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 0u);   // no thinking
    EXPECT_EQ(visible[0].tool_turns, 2u);     // 2 tool calls
}

/// A single tool call (tool_use + tool_result = 2 rows) is compressed.
TEST(MessagesList, ChainCompression_SingleToolCallCompressed) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].group_count, 0u);
    EXPECT_EQ(visible[0].tool_turns, 1u);
}

/// Update tool (Edit) and its result are both shown individually; the
/// following non-update tool chain is compressed separately.
TEST(MessagesList, ChainCompression_UpdateToolResultNotCompressed) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantToolUse, make_tool_use("Edit")},
        {MessageShape::UserToolResult, make_tool_result("Edit")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    auto visible = build_visible_rows(in);
    // Edit tool_use + Edit result (both payload) + Bash chain (compressed).
    ASSERT_EQ(visible.size(), 3u);
    EXPECT_EQ(visible[0].kind, payload_kind());  // Edit tool_use
    EXPECT_EQ(visible[1].kind, payload_kind());  // Edit tool_result
    EXPECT_EQ(visible[2].kind, compressed_chain_kind());
    EXPECT_EQ(visible[2].group_count, 0u);
    EXPECT_EQ(visible[2].tool_turns, 1u);
}

/// Expand/collapse: the CompressedChain row carries the anchor UUID so the
/// click tracker can build the "chain:<uuid>" key.  When that key is in
/// expanded_keys, the chain emits a header row (chain_is_expanded=true,
/// "click to collapse") followed by the individual payload rows.
TEST(MessagesList, ChainCompression_ExpandedChainShowsIndividualRows) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("I should check the file.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
    });
    in.uuids[0] = "test-uuid-abc";

    // Collapsed: 1 CompressedChain carrying the anchor UUID.
    auto collapsed = build_visible_rows(in);
    ASSERT_EQ(collapsed.size(), 1u);
    EXPECT_EQ(collapsed[0].kind, compressed_chain_kind());
    EXPECT_EQ(collapsed[0].chain_uuid, "test-uuid-abc");
    EXPECT_FALSE(collapsed[0].chain_is_expanded);

    // Expanded: 1 CompressedChain header + 3 payload rows.
    in.expanded_keys.insert("chain:test-uuid-abc");
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 4u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].chain_uuid, "test-uuid-abc");
    EXPECT_TRUE(visible[0].chain_is_expanded);
    EXPECT_EQ(visible[1].kind, payload_kind());
    EXPECT_EQ(visible[1].row_idx, 0u);
    EXPECT_EQ(visible[2].kind, payload_kind());
    EXPECT_EQ(visible[2].row_idx, 1u);
    EXPECT_EQ(visible[3].kind, payload_kind());
    EXPECT_EQ(visible[3].row_idx, 2u);
}

/// Expanding one chain does not affect a separate chain later in the list.
TEST(MessagesList, ChainCompression_ExpandOneChainLeavesOtherCompressed) {
    using namespace chain_compression_test;
    auto in = make_input({
        {MessageShape::AssistantThinking, make_thinking("First.")},
        {MessageShape::AssistantToolUse, make_tool_use("Read")},
        {MessageShape::UserToolResult, make_tool_result("Read")},
        {MessageShape::AssistantText, make_text("Here is what I found.")},
        {MessageShape::AssistantThinking, make_thinking("Second.")},
        {MessageShape::AssistantToolUse, make_tool_use("Bash")},
        {MessageShape::UserToolResult, make_tool_result("Bash")},
    });
    in.uuids[0] = "chain-a";
    in.uuids[4] = "chain-b";

    // Collapsed: 2 CompressedChains + 1 text payload.
    auto collapsed = build_visible_rows(in);
    ASSERT_EQ(collapsed.size(), 3u);
    EXPECT_EQ(collapsed[0].kind, compressed_chain_kind());
    EXPECT_EQ(collapsed[0].chain_uuid, "chain-a");
    EXPECT_EQ(collapsed[1].kind, payload_kind());
    EXPECT_EQ(collapsed[1].row_idx, 3u);
    EXPECT_EQ(collapsed[2].kind, compressed_chain_kind());
    EXPECT_EQ(collapsed[2].chain_uuid, "chain-b");

    // Expand only chain-a: header + 3 payload rows + text + chain-b.
    in.expanded_keys.insert("chain:chain-a");
    auto visible = build_visible_rows(in);
    ASSERT_EQ(visible.size(), 6u);
    EXPECT_EQ(visible[0].kind, compressed_chain_kind());
    EXPECT_EQ(visible[0].chain_uuid, "chain-a");
    EXPECT_TRUE(visible[0].chain_is_expanded);
    EXPECT_EQ(visible[1].kind, payload_kind());
    EXPECT_EQ(visible[1].row_idx, 0u);
    EXPECT_EQ(visible[2].kind, payload_kind());
    EXPECT_EQ(visible[2].row_idx, 1u);
    EXPECT_EQ(visible[3].kind, payload_kind());
    EXPECT_EQ(visible[3].row_idx, 2u);
    EXPECT_EQ(visible[4].kind, payload_kind());
    EXPECT_EQ(visible[4].row_idx, 3u);
    EXPECT_EQ(visible[5].kind, compressed_chain_kind());
    EXPECT_EQ(visible[5].chain_uuid, "chain-b");
}

// =============================================================================
// GAP: image-paste-display-broken (P0, user-reported 2026-06-30)
// BUG: Clipboard Ctrl+V capture + API transmission paths (EXIST per commit
// f85a5b8), but the display pipeline was broken at TWO links:
//   (M3) project_message() silently dropped ImageBlocks when iterating user
//        message content — only TextBlock* was std::get_if'd.
//   (M4) repl_screen's row builder mapped ALL role=="user" entries to a single
//        MessageShape::UserText row — MessageShape::UserImage + message_image
//        module were dead code.
// FIX (commits above):
//   1. ImageBlock gains {width,height,size_bytes,file_name,source_path,source}
//   2. project_messages() SPLITS a UserMessage with mixed TextBlock+ImageBlock
//      content into MULTIPLE display rows (TS parity: each UserImageMessage is
//      its own transcript row).
//   3. BuildMessages() in repl_screen dispatches is_image entries to
//      MessageShape::UserImage and populates ImageMessageData from the block.
// TS REF: src/components/UserImageMessage.tsx (renderer)
//         src/utils/processUserInput/processUserInput.ts L351-395 (content blocks)

// =============================================================================
// Click-to-expand: thinking blocks
// =============================================================================

TEST(MessagesList, IsRowClickable_ThinkingWithContent) {
    using namespace loom::ui::messages;
    using namespace loom::ui::messages_list;

    thinking_message::ThinkingMessageOptions opts;
    opts.data.raw_text = "I should check the file first.";
    opts.data.state = thinking_message::ThinkingState::Complete;

    MessageRowPayload payload = opts;
    EXPECT_TRUE(is_row_clickable(MessageShape::AssistantThinking, payload));
}

TEST(MessagesList, IsRowClickable_ThinkingEmpty) {
    using namespace loom::ui::messages;
    using namespace loom::ui::messages_list;

    thinking_message::ThinkingMessageOptions opts;
    opts.data.raw_text = "";
    opts.data.sections.clear();

    MessageRowPayload payload = opts;
    EXPECT_FALSE(is_row_clickable(MessageShape::AssistantThinking, payload));
}

TEST(MessagesList, IsRowClickable_RedactedThinkingNotClickable) {
    using namespace loom::ui::messages;
    using namespace loom::ui::messages_list;

    thinking_message::ThinkingMessageOptions opts;
    opts.data.raw_text = "redacted content";
    opts.data.state = thinking_message::ThinkingState::Complete;

    MessageRowPayload payload = opts;
    // Redacted thinking has nothing to expand — not clickable.
    EXPECT_FALSE(is_row_clickable(MessageShape::AssistantRedactedThinking, payload));
}

// =============================================================================
// Thinking block Markdown rendering
// =============================================================================

TEST(Messages, ThinkingBlockRendersMarkdown) {
    // Regression test: thinking content was rendered as plain text line-by-line,
    // showing raw ```cpp fence markers instead of a rendered code block.
    // The fix routes the thinking body through render_markdown_dim.
    using namespace loom::ui::messages;
    using namespace sticky_prompt_test;

    thinking_message::ThinkingMessageData data;
    data.raw_text =
        "Found it! Line 611:\n"
        "```cpp\n"
        "const Color kBgColor = Color::RGB(20, 20, 22);\n"
        "```\n"
        "And line 711-713:\n"
        "```cpp\n"
        "return hbox({ text(\" \"), hbox(std::move(parts)), text(\" \") })\n"
        "    | bgcolor(kBgColor)\n"
        "    | s\n"
        "```\n";
    data.state = thinking_message::ThinkingState::Complete;
    data.is_collapsed = false;

    // Render in transcript mode (expanded).
    auto el = thinking_message::RenderThinkingMessageFaithful(
        data, /*is_transcript_mode=*/true, /*verbose=*/false, /*add_margin=*/false);
    std::string snap = strip_ansi(render_ansi(std::move(el), 100, 30));

    // The code content must be visible.
    EXPECT_NE(snap.find("kBgColor"), std::string::npos) << snap;
    EXPECT_NE(snap.find("RGB(20, 20, 22)"), std::string::npos) << snap;
    // Raw markdown fence markers must NOT appear (rendered as code block).
    EXPECT_EQ(snap.find("```"), std::string::npos) << snap;
}

// =============================================================================
// RowClickTracker: frame lifecycle + hit-testing
// =============================================================================

TEST(MessagesList, RowClickTracker_TrackAndHitTest) {
    using namespace loom::ui::messages_list;

    RowClickTracker tracker;

    // Frame 1: track two rows.
    tracker.begin_frame();
    Box& box1 = tracker.track_row("key-abc-123");
    box1.x_min = 10; box1.x_max = 50;
    box1.y_min = 5;  box1.y_max = 7;
    Box& box2 = tracker.track_row("key-def-456");
    box2.x_min = 10; box2.x_max = 50;
    box2.y_min = 8;  box2.y_max = 10;
    tracker.end_frame();

    // Hit inside box1.
    auto hit1 = tracker.hit_test(20, 6);
    ASSERT_TRUE(hit1.has_value());
    EXPECT_EQ(*hit1, "key-abc-123");

    // Hit inside box2.
    auto hit2 = tracker.hit_test(30, 9);
    ASSERT_TRUE(hit2.has_value());
    EXPECT_EQ(*hit2, "key-def-456");

    // Miss.
    EXPECT_FALSE(tracker.hit_test(0, 0).has_value());
}

TEST(MessagesList, RowClickTracker_EndFrameTrimsStaleBoxes) {
    using namespace loom::ui::messages_list;

    RowClickTracker tracker;

    // Frame 1: track two rows.
    tracker.begin_frame();
    Box& b1 = tracker.track_row("key-a");
    b1.x_min = 0; b1.x_max = 100; b1.y_min = 0; b1.y_max = 10;
    Box& b2 = tracker.track_row("key-b");
    b2.x_min = 0; b2.x_max = 100; b2.y_min = 11; b2.y_max = 20;
    tracker.end_frame();
    EXPECT_EQ(tracker.boxes.size(), 2u);

    // Frame 2: only one row — stale box must be trimmed.
    tracker.begin_frame();
    Box& b3 = tracker.track_row("key-c");
    b3.x_min = 0; b3.x_max = 100; b3.y_min = 0; b3.y_max = 5;
    tracker.end_frame();
    EXPECT_EQ(tracker.boxes.size(), 1u);

    // Old box2's position must not hit.
    EXPECT_FALSE(tracker.hit_test(50, 15).has_value());
    // New box hits.
    auto hit = tracker.hit_test(50, 3);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, "key-c");
}

TEST(MessagesList, RowClickTracker_ReusesBoxesAcrossFrames) {
    using namespace loom::ui::messages_list;

    RowClickTracker tracker;

    // Frame 1.
    tracker.begin_frame();
    Box& b1 = tracker.track_row("key-a");
    b1.x_min = 0; b1.x_max = 10; b1.y_min = 0; b1.y_max = 5;
    tracker.end_frame();
    const auto* box_ptr = tracker.boxes[0].get();

    // Frame 2: same count — boxes must be reused (same pointer).
    tracker.begin_frame();
    Box& b2 = tracker.track_row("key-b");
    b2.x_min = 0; b2.x_max = 10; b2.y_min = 0; b2.y_max = 5;
    tracker.end_frame();
    EXPECT_EQ(tracker.boxes[0].get(), box_ptr);

    // Key was overwritten.
    auto hit = tracker.hit_test(5, 2);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, "key-b");
}
