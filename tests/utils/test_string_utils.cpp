/// @file test_string_utils.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the string/text utilities (trim/split/join, words, sanitization, diff, hashing, CLI parsing) suites.

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

TEST(StringUtils, TrimRemovesWhitespace) {
    EXPECT_EQ(loom::utils::string::trim("  hello  "), "hello");
    EXPECT_EQ(loom::utils::string::trim("\t\n text \r\n"), "text");
    EXPECT_EQ(loom::utils::string::trim("no_change"), "no_change");
    EXPECT_EQ(loom::utils::string::trim(""), "");
}

TEST(StringUtils, SplitByDelimiter) {
    auto parts = loom::utils::string::split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");


    auto empty = loom::utils::string::split("", ',');
    ASSERT_EQ(empty.size(), 1u);
    EXPECT_EQ(empty[0], "");
}

TEST(StringUtils, SplitHandlesConsecutiveDelimiters) {
    auto parts = loom::utils::string::split("a,,b", ',');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_EQ(parts[1], "");
}

TEST(StringUtils, JoinCombinesStrings) {
    std::vector<std::string> items = {"hello", "world", "test"};
    EXPECT_EQ(loom::utils::string::join(items, " "), "hello world test");
    EXPECT_EQ(loom::utils::string::join(items, ", "), "hello, world, test");


    std::vector<std::string> empty_vec;
    EXPECT_EQ(loom::utils::string::join(empty_vec, ","), "");
}

TEST(StringUtils, CaseConversion) {
    EXPECT_EQ(loom::utils::string::to_lower("Hello World"), "hello world");
    EXPECT_EQ(loom::utils::string::to_upper("Hello World"), "HELLO WORLD");
    EXPECT_EQ(loom::utils::string::to_lower("ALREADY"), "already");
    EXPECT_EQ(loom::utils::string::to_upper("already"), "ALREADY");
}

TEST(StringUtils, StartsWithAndEndsWith) {
    EXPECT_TRUE(loom::utils::string::starts_with("hello world", "hello"));
    EXPECT_FALSE(loom::utils::string::starts_with("hello world", "world"));
    EXPECT_TRUE(loom::utils::string::ends_with("hello world", "world"));
    EXPECT_FALSE(loom::utils::string::ends_with("hello world", "hello"));


    EXPECT_TRUE(loom::utils::string::starts_with("anything", ""));
    EXPECT_TRUE(loom::utils::string::ends_with("anything", ""));
}

TEST(StringUtils, ReplaceAllTreatsEmptyNeedleAsNoop) {
    EXPECT_EQ(loom::utils::string::replace_all("abc", "", "x"), "abc");
    EXPECT_EQ(loom::utils::string::replace_all("a-b-a", "a", "z"), "z-b-z");
}

TEST(StringUtils, TruncateDoesNotUnderflowWhenEllipsisIsLongerThanLimit) {
    EXPECT_EQ(loom::utils::string::truncate("abcdef", 2, "..."), "..");
    EXPECT_EQ(loom::utils::string::truncate("abcdef", 5, "..."), "ab...");
}

TEST(StringUtilsCompat, EscapeRegexEscapesSpecialCharacters) {
    EXPECT_EQ(loom::utils::escape_regex(R"(a.b*c+$^?{}()|[]\)"), R"(a\.b\*c\+\$\^\?\{\}\(\)\|\[\]\\)");
    EXPECT_EQ(loom::utils::escape_regex("plain"), "plain");
}

TEST(StringUtilsCompat, CapitalizePreservesRestOfString) {
    EXPECT_EQ(loom::utils::capitalize("fooBar"), "FooBar");
    EXPECT_EQ(loom::utils::capitalize("hELLO"), "HELLO");
    EXPECT_EQ(loom::utils::capitalize(""), "");
}

TEST(StringUtilsCompat, PluralSupportsRegularAndIrregularForms) {
    EXPECT_EQ(loom::utils::plural(1, "file"), "file");
    EXPECT_EQ(loom::utils::plural(2, "file"), "files");
    EXPECT_EQ(loom::utils::plural(2, "entry", "entries"), "entries");
}

TEST(StringUtilsCompat, FirstLineAndCountCharMatchTypeScriptHelpers) {
    EXPECT_EQ(loom::utils::first_line_of("#!/bin/sh\necho ok"), "#!/bin/sh");
    EXPECT_EQ(loom::utils::first_line_of("single"), "single");
    EXPECT_EQ(loom::utils::count_char_in_string("a,b,c,d", ',', 2), 2u);
}

TEST(StringUtilsCompat, NormalizesFullWidthDigitsAndSpace) {
    EXPECT_EQ(loom::utils::normalize_full_width_digits("０１２３ abc ９"), "0123 abc 9");
    EXPECT_EQ(loom::utils::normalize_full_width_space("foo　bar　baz"), "foo bar baz");
}

TEST(StringUtilsCompat, SafeJoinLinesTruncatesLikeTypeScriptHelper) {
    std::vector<std::string> lines = {"abc", "def", "ghi"};
    EXPECT_EQ(loom::utils::safe_join_lines(lines, ",", 10), "abc,def...[truncated]");
    EXPECT_EQ(loom::utils::safe_join_lines(lines, ",", 20), "abc,def,ghi");
}

TEST(WordsSlug, ExposesTypeScriptWordTablesAndDeterministicSlugAssembly) {
    EXPECT_TRUE(loom::utils::words::is_adjective("abundant"));
    EXPECT_TRUE(loom::utils::words::is_adjective("virtual"));
    EXPECT_TRUE(loom::utils::words::is_noun("aurora"));
    EXPECT_TRUE(loom::utils::words::is_noun("yao"));
    EXPECT_TRUE(loom::utils::words::is_verb("baking"));
    EXPECT_TRUE(loom::utils::words::is_verb("zooming"));
    EXPECT_FALSE(loom::utils::words::is_noun("not-in-table"));

    EXPECT_EQ(loom::utils::words::word_slug_from_indices(0, 4, 0), "abundant-brewing-aurora");
    EXPECT_EQ(loom::utils::words::short_word_slug_from_indices(0, 0), "abundant-aurora");
}

TEST(WordsSlug, GeneratesRandomSlugsWithExpectedShapeAndKnownWords) {
    const auto slug = loom::utils::words::generate_word_slug();
    const auto parts = loom::utils::string::split(slug, '-');
    ASSERT_EQ(parts.size(), 3u);
    EXPECT_TRUE(loom::utils::words::is_adjective(parts[0]));
    EXPECT_TRUE(loom::utils::words::is_verb(parts[1]));
    EXPECT_TRUE(loom::utils::words::is_noun(parts[2]));

    const auto short_slug = loom::utils::words::generate_short_word_slug();
    const auto short_parts = loom::utils::string::split(short_slug, '-');
    ASSERT_EQ(short_parts.size(), 2u);
    EXPECT_TRUE(loom::utils::words::is_adjective(short_parts[0]));
    EXPECT_TRUE(loom::utils::words::is_noun(short_parts[1]));
}

TEST(Sanitization, RemovesDangerousHiddenUnicodeRanges) {
    EXPECT_EQ(
        loom::utils::sanitization::partially_sanitize_unicode("safe\xE2\x80\x8Bhidden\xEF\xBB\xBFtext"),
        "safehiddentext");
    EXPECT_EQ(
        loom::utils::sanitization::partially_sanitize_unicode("left\xE2\x80\xAEright"),
        "leftright");
    EXPECT_EQ(
        loom::utils::sanitization::partially_sanitize_unicode("private\xEE\x80\x80use"),
        "privateuse");
}

TEST(Sanitization, AppliesCompatibilityNormalizationBeforeFiltering) {
    EXPECT_EQ(
        loom::utils::sanitization::partially_sanitize_unicode("\xEF\xBC\xA8\xEF\xBD\x85\xEF\xBD\x8C\xEF\xBD\x8C\xEF\xBD\x8F\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93"),
        "Hello123");
    EXPECT_EQ(loom::utils::sanitization::partially_sanitize_unicode(std::string("o") + "\xEF\xAC\x83" + "ce"), "office");
    EXPECT_EQ(loom::utils::sanitization::partially_sanitize_unicode("\xE2\x91\xA0\xE2\x85\xA0\xC2\xB2\xE3\x8E\x8F"), "1I2kg");
}

TEST(Sanitization, RecursivelySanitizesStringsArraysObjectsAndKeys) {
    using loom::utils::sanitization::SanitizedValue;

    SanitizedValue input = SanitizedValue::object({
        {"safe\xE2\x80\x8Bkey", SanitizedValue("value\xEF\xBB\xBFtext")},
        {"items", SanitizedValue::array({
            SanitizedValue("\xEF\xBC\xA8\xEF\xBD\x89"),
            SanitizedValue::object({{"nested\xE2\x80\xAEkey", SanitizedValue("ok")}}),
        })},
    });

    auto sanitized = loom::utils::sanitization::recursively_sanitize_unicode(input);
    const auto& obj = sanitized.as_object();
    ASSERT_TRUE(obj.contains("safekey"));
    EXPECT_EQ(obj.at("safekey").as_string(), "valuetext");
    const auto& items = obj.at("items").as_array();
    ASSERT_EQ(items.size(), 2u);
    EXPECT_EQ(items[0].as_string(), "Hi");
    EXPECT_TRUE(items[1].as_object().contains("nestedkey"));
}

TEST(Sanitization, RecursivelySanitizedObjectKeysUseLastWriteWinsOnCollisions) {
    using loom::utils::sanitization::SanitizedValue;

    SanitizedValue input = SanitizedValue::object({
        {"safekey", SanitizedValue("plain")},
        {"safe\xE2\x80\x8Bkey", SanitizedValue("hidden")},
    });

    auto sanitized = loom::utils::sanitization::recursively_sanitize_unicode(input);
    const auto& obj = sanitized.as_object();
    ASSERT_TRUE(obj.contains("safekey"));
    EXPECT_EQ(obj.at("safekey").as_string(), "hidden");
}

TEST(ArgumentSubstitution, ParsesNamesHintsAndSubstitutesPlaceholders) {
    using namespace loom::utils::argument_substitution;

    EXPECT_TRUE(parse_arguments("  \t ").empty());
    EXPECT_EQ(parse_arguments(R"(foo "hello world" 'again there' $FOO)"), (std::vector<std::string>{"foo", "hello world", "again there", "$FOO"}));
    EXPECT_EQ(parse_arguments(R"(foo "unterminated)"), (std::vector<std::string>{"foo", "\"unterminated"}));

    EXPECT_EQ(parse_argument_names(std::optional<std::string>{"foo bar 123  baz"}), (std::vector<std::string>{"foo", "bar", "baz"}));
    EXPECT_EQ(parse_argument_names(std::vector<std::string>{"foo", "", "42", "bar"}), (std::vector<std::string>{"foo", "bar"}));
    EXPECT_EQ(generate_progressive_argument_hint({"arg1", "arg2", "arg3"}, {"value1"}), "[arg2] [arg3]");
    EXPECT_FALSE(generate_progressive_argument_hint({"arg1"}, {"value1"}).has_value());

    EXPECT_EQ(
        substitute_arguments("all=$ARGUMENTS first=$ARGUMENTS[0] second=$1 named=$foo missing=$2", "alpha beta", true, {"foo"}),
        "all=alpha beta first=alpha second=beta named=alpha missing="
    );
    EXPECT_EQ(substitute_arguments("unchanged", std::nullopt), "unchanged");
    EXPECT_EQ(substitute_arguments("prefix", "x y", true), "prefix\n\nARGUMENTS: x y");
    EXPECT_EQ(substitute_arguments("$foo $foobar $foo[0]", "one two", true, {"foo"}), "one $foobar $foo[0]");
}

TEST(SemanticInputCoercion, CoercesOnlyExplicitBooleanAndDecimalStringLiterals) {
    using loom::utils::semantic_boolean::coerce_semantic_boolean;
    using loom::utils::semantic_number::coerce_semantic_number;

    EXPECT_EQ(coerce_semantic_boolean("true"), true);
    EXPECT_EQ(coerce_semantic_boolean("false"), false);
    EXPECT_FALSE(coerce_semantic_boolean("False").has_value());
    EXPECT_FALSE(coerce_semantic_boolean("0").has_value());

    ASSERT_TRUE(coerce_semantic_number("30").has_value());
    EXPECT_DOUBLE_EQ(*coerce_semantic_number("30"), 30.0);
    EXPECT_DOUBLE_EQ(*coerce_semantic_number("-5"), -5.0);
    EXPECT_DOUBLE_EQ(*coerce_semantic_number("3.14"), 3.14);
    EXPECT_FALSE(coerce_semantic_number("").has_value());
    EXPECT_FALSE(coerce_semantic_number("1.").has_value());
    EXPECT_FALSE(coerce_semantic_number(".5").has_value());
    EXPECT_FALSE(coerce_semantic_number("1e3").has_value());
}

TEST(HashUtils, MatchesTypeScriptDjb2Sha256AndPairHashing) {
    using namespace loom::utils::hash;

    EXPECT_EQ(djb2_hash(""), 0);
    EXPECT_EQ(djb2_hash("hello"), 99162322);
    EXPECT_EQ(djb2_hash("LOOM"), 2342561);
    EXPECT_EQ(djb2_hash("\xF0\x9F\x98\x80"), 1772899);

    EXPECT_EQ(hash_content("hello"), "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824");
    EXPECT_EQ(hash_pair("ts", "code"), "8aea308fb5b062d3ab47d67952ee9d76beab21da1610bf7fbcc97e728e84c226");
    EXPECT_EQ(hash_pair("tsc", "ode"), "2dd84b1bcba070e82fd5d6278dcc35424d1d6d3cc8d87fd3b12486e8dcfe5749");
    EXPECT_NE(hash_pair("ts", "code"), hash_pair("tsc", "ode"));
}

TEST(TaggedId, EncodesUuidAsApiCompatibleBase58TaggedId) {
    using namespace loom::utils::tagged_id;

    auto zero = to_tagged_id("user", "00000000-0000-0000-0000-000000000000");
    ASSERT_TRUE(zero.has_value()) << zero.error();
    EXPECT_EQ(*zero, "user_011111111111111111111111");

    auto max = to_tagged_id("user", "ffffffff-ffff-ffff-ffff-ffffffffffff");
    ASSERT_TRUE(max.has_value()) << max.error();
    EXPECT_EQ(*max, "user_01YcVfxkQb6JRzqk5kF2tNLv");

    auto with_hyphens = to_tagged_id("org", "123e4567-e89b-12d3-a456-426614174000");
    auto without_hyphens = to_tagged_id("org", "123e4567e89b12d3a456426614174000");
    ASSERT_TRUE(with_hyphens.has_value()) << with_hyphens.error();
    ASSERT_TRUE(without_hyphens.has_value()) << without_hyphens.error();
    EXPECT_EQ(*with_hyphens, "org_013FfGK34vwMvVFDedyb2nkf");
    EXPECT_EQ(*without_hyphens, *with_hyphens);

    EXPECT_FALSE(to_tagged_id("user", "not-a-uuid").has_value());
    EXPECT_FALSE(to_tagged_id("user", "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz").has_value());
}

TEST(DiffUtils, ApplyPatchHandlesUnifiedDiffHunks) {
    const std::string content = "one\ntwo\nthree\nfour\n";
    const std::string patch = R"PATCH(--- a/file.txt
+++ b/file.txt
@@ -1,4 +1,5 @@
 one
-two
+TWO
 three
+added
 four
)PATCH";

    auto result = loom::utils::apply_patch(content, patch);

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(*result, "one\nTWO\nthree\nadded\nfour\n");
}

TEST(DiffUtils, ApplyPatchHandlesMultipleHunks) {
    const std::string content = "a\nb\nc\nd\ne\n";
    const std::string patch = R"PATCH(--- a/file.txt
+++ b/file.txt
@@ -2,2 +2,2 @@
-b
+B
 c
@@ -5,1 +5,2 @@
 e
+f
)PATCH";

    auto result = loom::utils::apply_patch(content, patch);

    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(*result, "a\nB\nc\nd\ne\nf\n");
}

TEST(DiffUtils, ApplyPatchRejectsMismatchedContext) {
    const std::string content = "alpha\nbeta\n";
    const std::string patch = R"PATCH(--- a/file.txt
+++ b/file.txt
@@ -1,2 +1,2 @@
 alpha
-gamma
+delta
)PATCH";

    auto result = loom::utils::apply_patch(content, patch);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Patch context mismatch"), std::string::npos);
}

TEST(GitDiff, GenerateUnifiedDiffPreservesContextAndRoundTripsToStats) {
    auto patch = loom::utils::generate_unified_diff(
        "alpha\nbeta\ngamma\n",
        "alpha\ndelta\ngamma\n",
        "notes.txt");

    EXPECT_NE(patch.find("diff --git a/notes.txt b/notes.txt"), std::string::npos);
    EXPECT_NE(patch.find(" alpha\n"), std::string::npos);
    EXPECT_NE(patch.find("-beta\n"), std::string::npos);
    EXPECT_NE(patch.find("+delta\n"), std::string::npos);
    EXPECT_NE(patch.find(" gamma\n"), std::string::npos);

    auto parsed = loom::utils::parse_unified_diff(patch);
    ASSERT_EQ(parsed.size(), 1u);
    ASSERT_EQ(parsed[0].hunks.size(), 1u);

    auto stats = loom::utils::get_diff_stats(parsed);
    EXPECT_EQ(stats.files_changed, 1);
    EXPECT_EQ(stats.additions, 1);
    EXPECT_EQ(stats.deletions, 1);
}

TEST(GitDiff, GenerateUnifiedDiffReturnsEmptyStringForIdenticalContent) {
    EXPECT_EQ(
        loom::utils::generate_unified_diff("same\ncontent\n", "same\ncontent\n", "same.txt"),
        "");
}

TEST(SlashCommandParsing, ParsesRegularAndMcpSlashCommands) {
    using namespace loom::utils::slash_command_parsing;

    auto regular = parse_slash_command("  /search foo bar  ");
    ASSERT_TRUE(regular.has_value());
    EXPECT_EQ(regular->command_name, "search");
    EXPECT_EQ(regular->args, "foo bar");
    EXPECT_FALSE(regular->is_mcp);

    auto mcp = parse_slash_command("/mcp:tool (MCP) arg1  arg2");
    ASSERT_TRUE(mcp.has_value());
    EXPECT_EQ(mcp->command_name, "mcp:tool (MCP)");
    EXPECT_EQ(mcp->args, "arg1  arg2");
    EXPECT_TRUE(mcp->is_mcp);

    EXPECT_FALSE(parse_slash_command("search foo").has_value());
    EXPECT_FALSE(parse_slash_command("/").has_value());
    auto spaced = parse_slash_command("/cmd  two-spaces");
    ASSERT_TRUE(spaced.has_value());
    EXPECT_EQ(spaced->args, " two-spaces");
}
