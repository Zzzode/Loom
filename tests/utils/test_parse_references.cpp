/// @file test_parse_references.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the parse_references (image/pasted/truncated-text refs, paste truncation) suites.

#include <gtest/gtest.h>
#include <cstdlib>
#include <httplib.h>

import std;
import loom.text.string;
import loom.text.string_utils;
import loom.containers.array_utils;
import loom.serdes.json;
import loom.utils.error;
import loom.containers.circular_buffer;
import loom.model.token_budget;
import loom.config.settings_sources;
import loom.net.http.ssrf_guard;
import loom.plugins.plugin_identifier;
import loom.plugins.plugin_dependency_resolver;
import loom.config.settings_paths;
import loom.config.settings_merge;
import loom.plugins.plugin_marketplace_rules;
import loom.plugins.plugin_versioning;
import loom.plugins.plugin_loader;
import loom.parsing.cli.argument_substitution;
import loom.text.semantic_boolean;
import loom.text.semantic_number;
import loom.ui.chrome.terminal_io;
import loom.commands.review.review_remote;
import loom.security.query_guard;
import loom.agent.agent_id;
import loom.security.auto_mode_denials;
import loom.diagnostics.activity_manager;
import loom.platform.env.env_utils;
import loom.cache.cache_paths;
import loom.platform.binary_check;
import loom.skills.hints;
import loom.scm.git.commit_attribution;
import loom.crypto.hash;
import loom.types.tagged_id;
import loom.ui.messages.message_predicates;
import loom.types.wire.content_array;
import loom.containers.object_group_by;
import loom.process.timeouts;
import loom.parsing.cli.slash_command_parsing;
import loom.containers.set_utils;
import loom.text.words;
import loom.diagnostics.fps_tracker;
import loom.security.privacy_level;
import loom.tools.support.script_tool_enabled;
import loom.prompt.support.prompt_category;
import loom.teams.control_message_compat;
import loom.security.sanitization;
import loom.text.diff_utils;
import loom.process.shell.shell_providers;
import loom.config.settings;
import loom.scm.git.git_diff;
import loom.net.http.proxy_utils;
import loom.net.http.github_utils;
import loom.plugins.marketplace;
import loom.platform.clipboard;
import loom.text.parse_references;
import loom.memdir.memdir;

// ── parse_references (TS history.ts L62-75 parity) ──────────────────────────

TEST(ParseReferences, EmptyInput_ReturnsEmpty) {
    auto refs = loom::utils::parse_references("");
    EXPECT_TRUE(refs.empty());
}

TEST(ParseReferences, NoPlaceholders_ReturnsEmpty) {
    auto refs = loom::utils::parse_references("hello world this is a prompt");
    EXPECT_TRUE(refs.empty());
}

TEST(ParseReferences, SingleImageRef) {
    auto refs = loom::utils::parse_references("look at this [Image #1] please");
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].id, 1);
    EXPECT_EQ(refs[0].match, "[Image #1]");
    EXPECT_EQ(refs[0].index, 13u);  // byte offset of '['
}

TEST(ParseReferences, MultipleImageRefs) {
    auto refs = loom::utils::parse_references("[Image #1] and [Image #2] here");
    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0].id, 1);
    EXPECT_EQ(refs[0].index, 0u);
    EXPECT_EQ(refs[1].id, 2);
    EXPECT_EQ(refs[1].match, "[Image #2]");
}

TEST(ParseReferences, PastedTextRef) {
    auto refs = loom::utils::parse_references("see [Pasted text #5] for details");
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].id, 5);
    EXPECT_EQ(refs[0].match, "[Pasted text #5]");
}

TEST(ParseReferences, PastedTextRefWithLineCount) {
    auto refs = loom::utils::parse_references("[Pasted text #3 +10 lines]");
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].id, 3);
    EXPECT_EQ(refs[0].match, "[Pasted text #3 +10 lines]");
}

TEST(ParseReferences, TruncatedTextRef) {
    auto refs = loom::utils::parse_references("[...Truncated text #7]");
    ASSERT_EQ(refs.size(), 1u);
    EXPECT_EQ(refs[0].id, 7);
    EXPECT_EQ(refs[0].match, "[...Truncated text #7]");
}

TEST(ParseReferences, ZeroIdFilteredOut) {
    // TS L74: filter(match => match.id > 0)
    auto refs = loom::utils::parse_references("[Image #0]");
    EXPECT_TRUE(refs.empty());
}

TEST(ParseReferences, MixedRefTypes) {
    auto refs = loom::utils::parse_references(
        "[Image #1] then [Pasted text #2 +5 lines] and [...Truncated text #3]");
    ASSERT_EQ(refs.size(), 3u);
    EXPECT_EQ(refs[0].id, 1);
    EXPECT_EQ(refs[0].match, "[Image #1]");
    EXPECT_EQ(refs[1].id, 2);
    EXPECT_EQ(refs[1].match, "[Pasted text #2 +5 lines]");
    EXPECT_EQ(refs[2].id, 3);
    EXPECT_EQ(refs[2].match, "[...Truncated text #3]");
}

TEST(ParseReferences, FormatImageRef) {
    EXPECT_EQ(loom::utils::format_image_ref(1), "[Image #1]");
    EXPECT_EQ(loom::utils::format_image_ref(42), "[Image #42]");
}

TEST(ParseReferences, FormatPastedTextRef_NoLines) {
    EXPECT_EQ(loom::utils::format_pasted_text_ref(3, 0), "[Pasted text #3]");
}

TEST(ParseReferences, FormatPastedTextRef_WithLines) {
    EXPECT_EQ(loom::utils::format_pasted_text_ref(7, 12), "[Pasted text #7 +12 lines]");
}

TEST(ParseReferences, ExpandPastedTextRefs_ReplacesTextRefs) {
    // Simulate pasted_contents lookup: id=1 → "hello world", id=2 → no text (image)
    std::string input = "before [Pasted text #1] after [Image #2]";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input,
        [](int id) -> std::optional<std::string> {
            if (id == 1) return "hello world";
            return std::nullopt;  // image or unknown
        });
    // [Pasted text #1] replaced; [Image #2] left alone
    EXPECT_EQ(expanded, "before hello world after [Image #2]");
}

TEST(ParseReferences, ExpandPastedTextRefs_NoRefs) {
    std::string input = "just normal text";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input, [](int) -> std::optional<std::string> { return std::nullopt; });
    EXPECT_EQ(expanded, input);
}

TEST(ParseReferences, ExpandPastedTextRefs_MultipleRefsReverseOrder) {
    // Verify reverse-order splicing keeps offsets correct
    std::string input = "[Pasted text #1] middle [Pasted text #2]";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input,
        [](int id) -> std::optional<std::string> {
            if (id == 1) return "AAA";
            if (id == 2) return "BBB";
            return std::nullopt;
        });
    EXPECT_EQ(expanded, "AAA middle BBB");
}

// ── paste-truncation (TS inputPaste.ts parity) ────────────────────────────

TEST(ParseReferences, FormatTruncatedTextRef) {
    // TS REF: inputPaste.ts L57 formatTruncatedTextRef
    EXPECT_EQ(loom::utils::format_truncated_text_ref(1, 0),
              "[...Truncated text #1 +0 lines...]");
    EXPECT_EQ(loom::utils::format_truncated_text_ref(3, 42),
              "[...Truncated text #3 +42 lines...]");
    EXPECT_EQ(loom::utils::format_truncated_text_ref(7, 100),
              "[...Truncated text #7 +100 lines...]");
}

TEST(ParseReferences, GetPastedTextRefNumLines_NoNewlines) {
    // TS REF: history.ts L47 getPastedTextRefNumLines — "abc" → 0
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines(""), 0);
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("hello world"), 0);
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines(std::string(1000, 'x')), 0);
}

TEST(ParseReferences, GetPastedTextRefNumLines_LF) {
    // "a\nb\nc" → 2 (TS: newline match count, NOT line count)
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("a\nb\nc"), 2);
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("\n"), 1);
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("line1\nline2"), 1);
}

TEST(ParseReferences, GetPastedTextRefNumLines_CRLF) {
    // "a\r\nb" → 1 (CRLF counted as one break)
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("a\r\nb"), 1);
    // "a\r\nb\r\nc" → 2
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("a\r\nb\r\nc"), 2);
}

TEST(ParseReferences, GetPastedTextRefNumLines_CR) {
    // "a\rb" → 1 (standalone CR)
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("a\rb"), 1);
    // Mixed: "a\nb\rc\r\nd" → LF + CR + CRLF = 3
    EXPECT_EQ(loom::utils::get_pasted_text_ref_num_lines("a\nb\rc\r\nd"), 3);
}

TEST(ParseReferences, MaybeTruncatePaste_ShortText) {
    // TS REF: inputPaste.ts L25 — text <= 10000 chars → returned as-is
    auto result = loom::utils::maybe_truncate_paste("hello world", 1);
    EXPECT_EQ(result.truncated_text, "hello world");
    EXPECT_TRUE(result.placeholder_content.empty());
}

TEST(ParseReferences, MaybeTruncatePaste_AtThreshold) {
    // Exactly 10000 chars → no truncation
    std::string text(10000, 'a');
    auto result = loom::utils::maybe_truncate_paste(text, 5);
    EXPECT_EQ(result.truncated_text.size(), 10000u);
    EXPECT_TRUE(result.placeholder_content.empty());
}

TEST(ParseReferences, MaybeTruncatePaste_OverThreshold_SingleLine) {
    // >10000 chars single-line → head(500) + ref + tail(500)
    std::string text(25000, 'x');
    auto result = loom::utils::maybe_truncate_paste(text, 3);
    // Should be much shorter than original
    EXPECT_LT(result.truncated_text.size(), 1200u);
    // Should contain the truncated text ref with paste id
    EXPECT_NE(result.truncated_text.find("[...Truncated text #3 +0 lines...]"),
              std::string::npos);
    // Head should be first 500 chars
    EXPECT_EQ(result.truncated_text.substr(0, 500), std::string(500, 'x'));
    // Tail should be last 500 chars
    EXPECT_EQ(result.truncated_text.substr(result.truncated_text.size() - 500),
              std::string(500, 'x'));
    // Placeholder content = middle = text.substr(500, 24000)
    EXPECT_EQ(result.placeholder_content.size(), 24000u);
    EXPECT_EQ(result.placeholder_content, std::string(24000, 'x'));
}

TEST(ParseReferences, MaybeTruncatePaste_OverThreshold_MultiLine) {
    // >10000 chars multi-line → elided line count > 0
    std::string big;
    for (int i = 0; i < 700; ++i) big += std::string(19, 'y') + "\n";
    ASSERT_GT(big.size(), 10000u);
    auto result = loom::utils::maybe_truncate_paste(big, 1);
    EXPECT_NE(result.truncated_text.find("[...Truncated text #1 +"),
              std::string::npos);
    // Should NOT report "+0 lines..." (multi-line paste has many newlines)
    EXPECT_EQ(result.truncated_text.find("+0 lines...]"), std::string::npos);
    // Placeholder content should be non-empty (the truncated middle)
    EXPECT_FALSE(result.placeholder_content.empty());
    // Verify head + ref + tail structure
    EXPECT_TRUE(result.truncated_text.starts_with(std::string(19, 'y') + "\n"));
}

TEST(ParseReferences, ExpandPastedTextRefs_TruncatedTextRefs) {
    // [...Truncated text #N] refs should also be expanded
    std::string input = "head [...Truncated text #1 +42 lines...] tail";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input,
        [](int id) -> std::optional<std::string> {
            if (id == 1) return "MIDDLE_CONTENT";
            return std::nullopt;
        });
    EXPECT_EQ(expanded, "head MIDDLE_CONTENT tail");
}

TEST(ParseReferences, ExpandPastedTextRefs_MixedPastedAndTruncated) {
    // Mix of [Pasted text #N] and [...Truncated text #N] refs
    std::string input = "[Pasted text #1] and [...Truncated text #2 +5 lines...]";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input,
        [](int id) -> std::optional<std::string> {
            if (id == 1) return "FIRST";
            if (id == 2) return "SECOND";
            return std::nullopt;
        });
    EXPECT_EQ(expanded, "FIRST and SECOND");
}

TEST(ParseReferences, ExpandPastedTextRefs_ImageLeftAlone) {
    // [Image #N] refs should NOT be expanded
    std::string input = "[Image #1] [...Truncated text #2 +3 lines...]";
    auto expanded = loom::utils::expand_pasted_text_refs(
        input,
        [](int id) -> std::optional<std::string> {
            if (id == 2) return "EXPANDED";
            return std::nullopt;
        });
    EXPECT_EQ(expanded, "[Image #1] EXPANDED");
}
