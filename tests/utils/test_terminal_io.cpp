/// @file test_terminal_io.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the terminal_io (ANSI/CSI/SGR parsing) suites.

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

// ===========================================================================
// loom.ui.chrome.terminal_io — pure ANSI/CSI/SGR parsing helpers.
// These parser entry points (parse_sgr, strip_ansi, parse_csi, tokenize_ansi)
// had no direct test coverage; the suite below pins their contract.
// ===========================================================================
namespace tio = loom::ui::termio;

TEST(TerminalIO, StripAnsiLeavesPlainText) {
    EXPECT_EQ(tio::strip_ansi("hello world"), "hello world");
}

TEST(TerminalIO, StripAnsiRemovesCsiSGR) {
    // \033[1;31m bold red, \033[0m reset
    std::string in = std::string("\033[1;31m") + "ERR" + "\033[0m";
    EXPECT_EQ(tio::strip_ansi(in), "ERR");
}

TEST(TerminalIO, StripAnsiRemovesCursorMoves) {
    // \033[2A cursor up 2, \033[10G column 10
    std::string in = std::string("a\033[2Ab\033[10Gc");
    EXPECT_EQ(tio::strip_ansi(in), "abc");
}

TEST(TerminalIO, StripAnsiRemovesOscTerminatedByBell) {
    // OSC sequence terminated by BEL (\a)
    std::string in = std::string("\033]0;title\a") + "body";
    EXPECT_EQ(tio::strip_ansi(in), "body");
}

TEST(TerminalIO, StripAnsiRemovesOscTerminatedByST) {
    // OSC sequence terminated by ST (ESC \)
    std::string in = std::string("\033]0;title\033\\") + "body";
    EXPECT_EQ(tio::strip_ansi(in), "body");
}

TEST(TerminalIO, ParseSGREmptyIsReset) {
    auto r = tio::parse_sgr("");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->bold);
    EXPECT_FALSE(r->underline);
    // fg/bg are default (monostate)
    EXPECT_TRUE(std::holds_alternative<std::monostate>(r->fg));
    EXPECT_TRUE(std::holds_alternative<std::monostate>(r->bg));
}

TEST(TerminalIO, ParseSGRBasicAttributes) {
    // 1=bold 3=italic 4=underline 7=inverse 9=strikethrough
    auto r = tio::parse_sgr("1;3;4;7;9");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->bold);
    EXPECT_TRUE(r->italic);
    EXPECT_TRUE(r->underline);
    EXPECT_TRUE(r->inverse);
    EXPECT_TRUE(r->strikethrough);
}

TEST(TerminalIO, ParseSGR16ColorForeground) {
    // 31 = red foreground (Color16 index 1)
    auto r = tio::parse_sgr("31");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(std::holds_alternative<tio::Color16>(r->fg));
    EXPECT_EQ(static_cast<int>(std::get<tio::Color16>(r->fg)), 1);
}

TEST(TerminalIO, ParseSGR256Color) {
    // 38;5;202 = 256-color foreground index 202
    auto r = tio::parse_sgr("38;5;202");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(std::holds_alternative<tio::Color256>(r->fg));
    EXPECT_EQ(std::get<tio::Color256>(r->fg).index, 202);
}

TEST(TerminalIO, ParseSGRTrueColor) {
    // 38;2;10;20;30 = truecolor foreground
    auto r = tio::parse_sgr("38;2;10;20;30");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(std::holds_alternative<tio::TrueColor>(r->fg));
    auto tc = std::get<tio::TrueColor>(r->fg);
    EXPECT_EQ(tc.r, 10);
    EXPECT_EQ(tc.g, 20);
    EXPECT_EQ(tc.b, 30);
}

TEST(TerminalIO, ParseSGRBrightForeground) {
    // 91 = bright red fg (Color16 index 9)
    auto r = tio::parse_sgr("91");
    ASSERT_TRUE(r.has_value());
    ASSERT_TRUE(std::holds_alternative<tio::Color16>(r->fg));
    EXPECT_EQ(static_cast<int>(std::get<tio::Color16>(r->fg)), 9);
}

TEST(TerminalIO, ParseSGRResetWithinSequence) {
    // 1;31;0 — the trailing 0 resets everything to defaults.
    auto r = tio::parse_sgr("1;31;0");
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->bold);
    EXPECT_TRUE(std::holds_alternative<std::monostate>(r->fg));
}

TEST(TerminalIO, ParseCSIBasicCursorMove) {
    // "2A" → params [2], final 'A' (cursor up)
    auto r = tio::parse_csi("2A");
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->params.size(), 1u);
    EXPECT_EQ(r->params[0], 2);
    EXPECT_EQ(r->final_byte, 'A');
    EXPECT_TRUE(r->intermediate.empty());
}

TEST(TerminalIO, ParseCSIDefaultParam) {
    // "H" with no parameter bytes → empty params list (the caller applies the
    // documented default for the final byte, e.g. cursor home).
    auto r = tio::parse_csi("H");
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->params.empty());
    EXPECT_EQ(r->final_byte, 'H');
}

TEST(TerminalIO, ParseCSIMultipleParams) {
    // "5;10H" → row 5, col 10
    auto r = tio::parse_csi("5;10H");
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->params.size(), 2u);
    EXPECT_EQ(r->params[0], 5);
    EXPECT_EQ(r->params[1], 10);
}

TEST(TerminalIO, ParseCSIMissingFinalByteFails) {
    auto r = tio::parse_csi("12;3");
    EXPECT_FALSE(r.has_value());
}

TEST(TerminalIO, ParseCSIEmptyFails) {
    auto r = tio::parse_csi("");
    EXPECT_FALSE(r.has_value());
}

TEST(TerminalIO, TokenizeAnsiPureText) {
    auto toks = tio::tokenize_ansi("plain text");
    ASSERT_EQ(toks.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<tio::TextToken>(toks[0]));
    EXPECT_EQ(std::get<tio::TextToken>(toks[0]).content, "plain text");
}

TEST(TerminalIO, TokenizeAnsiTextThenSGRThenText) {
    // "hi" + ESC[1m + "!"  → TextToken, SgrToken, TextToken
    std::string in = std::string("hi") + "\033[1m" + "!";
    auto toks = tio::tokenize_ansi(in);
    ASSERT_EQ(toks.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<tio::TextToken>(toks[0]));
    EXPECT_TRUE(std::holds_alternative<tio::SgrToken>(toks[1]));
    EXPECT_TRUE(std::holds_alternative<tio::TextToken>(toks[2]));
}

TEST(TerminalIO, GenerateCSIBuildsSequence) {
    // generate_csi(params, intermediate, final_byte) → ESC [ params intermediate final
    const int params[] = {2};
    auto seq = tio::generate_csi(params, {}, 'A');
    EXPECT_EQ(seq, std::string("\033[2A"));
}

TEST(TerminalIO, GenerateCSIMultipleParams) {
    const int params[] = {5, 10};
    auto seq = tio::generate_csi(params, {}, 'H');
    EXPECT_EQ(seq, std::string("\033[5;10H"));
}
