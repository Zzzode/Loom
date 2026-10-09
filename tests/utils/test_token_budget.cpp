/// @file test_token_budget.cpp
///
/// Split out of the former test_utils.cpp kitchen-sink (169 tests, 53 suites).
/// This file covers the token budget and timeout parsing suites.

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

TEST(TokenBudget, BasicAllocation) {
    loom::utils::token_budget::TokenBudget config{
        .max_context = 1000,
        .max_output = 100,
        .reserved_for_tools = 100,
        .reserved_for_system = 100,
    };
    loom::utils::token_budget::BudgetManager budget(config);

    auto remaining_in_category = budget.allocate(
        loom::utils::token_budget::BudgetCategory::SystemPrompt, 80);
    EXPECT_EQ(remaining_in_category, 20u);
    EXPECT_TRUE(budget.can_fit(200));
    EXPECT_EQ(budget.remaining(), 920u);
}

TEST(TokenBudget, AvailableForMessagesAccountsForReservedTokens) {
    loom::utils::token_budget::TokenBudget budget{
        .max_context = 1000,
        .max_output = 100,
        .reserved_for_tools = 200,
        .reserved_for_system = 100,
    };

    EXPECT_EQ(budget.available_for_messages(), 600u);
}

TEST(TokenBudget, OverflowHandling) {
    loom::utils::token_budget::TokenBudget config{
        .max_context = 100,
        .max_output = 0,
        .reserved_for_tools = 0,
        .reserved_for_system = 100,
    };
    loom::utils::token_budget::BudgetManager budget(config);

    EXPECT_EQ(budget.allocate(loom::utils::token_budget::BudgetCategory::SystemPrompt, 80), 20u);


    EXPECT_EQ(budget.allocate(loom::utils::token_budget::BudgetCategory::SystemPrompt, 50), 0u);
    EXPECT_FALSE(budget.can_fit(1));
    EXPECT_EQ(budget.remaining(), 0u);
    EXPECT_TRUE(budget.should_compact());
}

TEST(TokenBudget, ResetBudget) {
    loom::utils::token_budget::TokenBudget config{
        .max_context = 500,
        .max_output = 50,
        .reserved_for_tools = 50,
        .reserved_for_system = 50,
    };
    loom::utils::token_budget::BudgetManager budget(config);
    (void)budget.allocate(loom::utils::token_budget::BudgetCategory::UserMessages, 300);
    budget.reset();
    EXPECT_EQ(budget.remaining(), 500u);
}

TEST(TokenBudget, EstimatesTextTokens) {
    loom::utils::token_budget::TokenEstimator estimator;
    EXPECT_GT(estimator.estimate_tokens("hello world"), 0u);
    EXPECT_GT(estimator.estimate_message_tokens("user", "hello world"),
              estimator.estimate_tokens("hello world"));
}

TEST(TokenBudget, ParsesOriginalTokenBudgetSyntax) {
    EXPECT_EQ(loom::utils::token_budget::parse_token_budget("+500k"), 500000u);
    EXPECT_EQ(loom::utils::token_budget::parse_token_budget("please use 2M tokens"), 2000000u);
    EXPECT_EQ(loom::utils::token_budget::parse_token_budget("finish with +1.5m."), 1500000u);
    EXPECT_FALSE(loom::utils::token_budget::parse_token_budget("budget 500k in the middle").has_value());
}

TEST(TokenBudget, FindsBudgetPositionsAndFormatsContinuationMessage) {
    auto positions = loom::utils::token_budget::find_token_budget_positions(" +1.5m and use 2k tokens");

    ASSERT_EQ(positions.size(), 2u);
    EXPECT_EQ(positions[0].start, 1u);
    EXPECT_EQ(positions[0].end, 6u);
    EXPECT_EQ(positions[1].start, 11u);
    EXPECT_EQ(positions[1].end, 24u);

    EXPECT_EQ(
        loom::utils::token_budget::get_budget_continuation_message(80, 1200, 1500),
        "Stopped at 80% of token target (1,200 / 1,500). Keep working — do not summarize.");
}

TEST(Timeouts, ParsesDefaultAndMaxBashTimeoutsLikeTypeScript) {
    using namespace loom::utils::timeouts;

    EXPECT_EQ(get_default_bash_timeout_ms({}), 120000);
    EXPECT_EQ(get_default_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "3000"}}), 3000);
    EXPECT_EQ(get_default_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "0"}}), 120000);
    EXPECT_EQ(get_default_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "abc"}}), 120000);
    EXPECT_EQ(get_default_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "42abc"}}), 42);

    EXPECT_EQ(get_max_bash_timeout_ms({}), 600000);
    EXPECT_EQ(get_max_bash_timeout_ms({{"BASH_MAX_TIMEOUT_MS", "5000"}}), 120000);
    EXPECT_EQ(get_max_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "200000"}, {"BASH_MAX_TIMEOUT_MS", "5000"}}), 200000);
    EXPECT_EQ(get_max_bash_timeout_ms({{"BASH_MAX_TIMEOUT_MS", "900000"}}), 900000);
    EXPECT_EQ(get_max_bash_timeout_ms({{"BASH_DEFAULT_TIMEOUT_MS", "700000"}}), 700000);
}
