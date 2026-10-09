/// @file test_insights.cpp
/// @brief Insights facet pipeline tests.

#include <gtest/gtest.h>
#include <cstdlib>
#include <atomic>
#include <unistd.h>

import std;
import loom.commands.command;
import loom.commands.registry;
import loom.query.query_engine;
import loom.tools.tool;
import loom.types.types;
import loom.commands.agents;
import loom.commands.clear;
import loom.commands.config;
import loom.config.config;
import loom.commands.help;
import loom.commands.hooks;
import loom.commands.insights;
import loom.commands.model;
import loom.commands.mcp_cmd;
import loom.commands.rewind;
import loom.commands.plugin_cmd;
import loom.commands.plugin_ui_data;
import loom.commands.plugin_parse_args;
import loom.commands.resume;
import loom.utils.error;
import loom.serdes.json;
import loom.commands.terminal_setup;
import loom.platform.hyperlink;
import loom.services.mcp.xaa_idp_login;

// ============================================================================
// Insights facet pipeline — deterministic tests for the portable core
// (label map, aggregation, cache round-trip, HTML report structure, seam).
// ============================================================================

namespace {

namespace insights = loom::commands;
namespace fs = std::filesystem;

// Scoped HOME override so facet cache tests never touch the real user store.
struct HomeGuard {
    std::string name = "HOME";
    std::optional<std::string> previous;
    fs::path tmp;
    explicit HomeGuard() {
        if (const char* h = std::getenv("HOME")) previous = h;
        auto base = fs::temp_directory_path();
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        static std::atomic<long> counter{0};
        tmp = base / ("cc-insights-test-" + std::to_string(stamp) + "-" +
                      std::to_string(counter.fetch_add(1)));
        fs::create_directories(tmp);
        setenv("HOME", tmp.c_str(), 1);
    }
    ~HomeGuard() {
        if (previous) {
            setenv("HOME", previous->c_str(), 1);
        } else {
            unsetenv("HOME");
        }
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
};

insights::SessionFacets make_facets(std::string id) {
    insights::SessionFacets f;
    f.session_id = std::move(id);
    f.underlying_goal = "Ship the feature";
    f.goal_categories = {{"implement_feature", 2}, {"fix_bug", 1}};
    f.outcome = "fully_achieved";
    f.user_satisfaction_counts = {{"satisfied", 2}, {"happy", 1}};
    f.loom_helpfulness = "very_helpful";
    f.session_type = "iterative_refinement";
    f.friction_counts = {{"buggy_code", 1}};
    f.friction_detail = "One bad edit";
    f.primary_success = "correct_code_edits";
    f.brief_summary = "User shipped the feature";
    return f;
}

} // namespace

TEST(Insights, HumaniseLabelMap) {
    EXPECT_EQ(insights::humanise("fix_bug"), "Fix Bug");
    EXPECT_EQ(insights::humanise("fully_achieved"), "Fully Achieved");
    EXPECT_EQ(insights::humanise("satisfied"), "Satisfied");
    // Unknown keys pass through verbatim (matches TS behaviour).
    EXPECT_EQ(insights::humanise("unknown_thing"), "unknown_thing");
}

TEST(Insights, AggregateFacetsIsDeterministic) {
    std::vector<insights::SessionFacets> facets = {
        make_facets("sess-a"),
        make_facets("sess-b"),
    };
    auto agg = insights::aggregate_facets(facets);
    EXPECT_EQ(agg.sessions_with_facets, 2u);
    EXPECT_EQ(agg.goal_categories.at("implement_feature"), 4u);
    EXPECT_EQ(agg.goal_categories.at("fix_bug"), 2u);
    EXPECT_EQ(agg.outcomes.at("fully_achieved"), 2u);
    EXPECT_EQ(agg.satisfaction.at("satisfied"), 4u);
    EXPECT_EQ(agg.satisfaction.at("happy"), 2u);
    EXPECT_EQ(agg.helpfulness.at("very_helpful"), 2u);
    EXPECT_EQ(agg.session_types.at("iterative_refinement"), 2u);
    EXPECT_EQ(agg.friction.at("buggy_code"), 2u);
    EXPECT_EQ(agg.success.at("correct_code_edits"), 2u);
    // 'none' primary_success is excluded.
    EXPECT_EQ(agg.success.count("none"), 0u);
}

TEST(Insights, AggregateFacetsSkipsNonPositiveCounts) {
    auto f = make_facets("sess-c");
    // Zero/negative counts must be filtered (matches TS `count > 0`).
    f.goal_categories["zero_goal"] = 0;
    auto agg = insights::aggregate_facets({f});
    EXPECT_EQ(agg.goal_categories.count("zero_goal"), 0u);
    EXPECT_EQ(agg.goal_categories.at("implement_feature"), 2u);
}

TEST(Insights, AggregateFacetsEmpty) {
    auto agg = insights::aggregate_facets({});
    EXPECT_EQ(agg.sessions_with_facets, 0u);
    EXPECT_TRUE(agg.goal_categories.empty());
}

TEST(Insights, FacetCacheRoundTrip) {
    HomeGuard guard;
    auto original = make_facets("round-trip-1");
    auto save_res = insights::save_facets(original);
    ASSERT_TRUE(save_res.has_value()) << save_res.error().message();

    auto loaded = insights::load_facets("round-trip-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->session_id, original.session_id);
    EXPECT_EQ(loaded->underlying_goal, original.underlying_goal);
    EXPECT_EQ(loaded->outcome, original.outcome);
    EXPECT_EQ(loaded->goal_categories, original.goal_categories);
    EXPECT_EQ(loaded->user_satisfaction_counts,
              original.user_satisfaction_counts);
    EXPECT_EQ(loaded->friction_counts, original.friction_counts);
    EXPECT_EQ(loaded->primary_success, original.primary_success);
    EXPECT_EQ(loaded->brief_summary, original.brief_summary);
}

TEST(Insights, FacetCacheMissForUnknownSession) {
    HomeGuard guard;
    auto loaded = insights::load_facets("does-not-exist-xyz");
    EXPECT_FALSE(loaded.has_value());
}

TEST(Insights, FacetCacheRejectsInvalidSchema) {
    HomeGuard guard;
    // Missing required string fields + missing required object fields.
    const std::string bad = R"({"session_id":"x","foo":1})";
    auto path = insights::detail::facets_dir();
    fs::create_directories(path);
    { std::ofstream o(path / "bad-1.json"); o << bad; }
    EXPECT_FALSE(insights::load_facets("bad-1").has_value());
}

TEST(Insights, FacetCacheAcceptsTSJsonShape) {
    HomeGuard guard;
    // A facets JSON shaped exactly like the TS cache format.
    const std::string good = R"({
      "session_id": "ts-1",
      "underlying_goal": "Goal",
      "goal_categories": {"fix_bug": 3, "implement_feature": 1},
      "outcome": "mostly_achieved",
      "user_satisfaction_counts": {"satisfied": 2},
      "loom_helpfulness": "essential",
      "session_type": "multi_task",
      "friction_counts": {"wrong_approach": 1},
      "friction_detail": "Detour",
      "primary_success": "multi_file_changes",
      "brief_summary": "Did the thing"
    })";
    auto path = insights::detail::facets_dir();
    fs::create_directories(path);
    { std::ofstream o(path / "ts-1.json"); o << good; }

    auto loaded = insights::load_facets("ts-1");
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->goal_categories.at("fix_bug"), 3u);
    EXPECT_EQ(loaded->outcome, "mostly_achieved");
    EXPECT_EQ(loaded->loom_helpfulness, "essential");
}

TEST(Insights, HtmlReportHasExpectedStructure) {
    std::vector<insights::SessionFacets> facets = {make_facets("h1")};
    auto agg = insights::aggregate_facets(facets);
    auto html = insights::render_html_report(
        5, 100, "2026-01-01", "2026-06-01", agg);

    EXPECT_TRUE(html.starts_with("<!DOCTYPE html>"));
    EXPECT_NE(html.find("<title>Loom Insights</title>"), std::string::npos);
    // Sections present.
    EXPECT_NE(html.find("id=\"goals\""), std::string::npos);
    EXPECT_NE(html.find("id=\"outcomes\""), std::string::npos);
    EXPECT_NE(html.find("id=\"satisfaction\""), std::string::npos);
    EXPECT_NE(html.find("id=\"friction\""), std::string::npos);
    // Humanised label rendered (Fix Bug), not the raw key.
    EXPECT_NE(html.find("Fix Bug"), std::string::npos);
    EXPECT_EQ(html.find("fix_bug"), std::string::npos);
    // Closing tags.
    EXPECT_NE(html.find("</html>"), std::string::npos);
}

TEST(Insights, HtmlReportEscapesAndHumanises) {
    // HTML-escape coverage: render with a value known to contain HTML. The
    // date-range subtitle passes straight into the document, so inject markup
    // there and assert it is escaped (no raw tag survives).
    insights::AggregatedFacets agg;
    agg.sessions_with_facets = 1;
    agg.goal_categories = {{"fix_bug", 1}};
    const std::string evil_date = "2026<script>boom</script>";
    auto html = insights::render_html_report(1, 1, evil_date, "2026-01-02", agg);
    // Raw angle brackets from the date value must be escaped.
    EXPECT_EQ(html.find("<script>boom"), std::string::npos);
    EXPECT_NE(html.find("&lt;script&gt;boom&lt;/script&gt;"), std::string::npos);
}

TEST(Insights, ExtractFacetsSeamParsesValidJson) {
    // The seam receives a fully-formed prompt+transcript (the live caller
    // prepends facet_extraction_prompt()). Assert it is forwarded intact.
    const std::string transcript =
        loom::commands::facet_extraction_prompt() + "user: please fix the bug";
    loom::commands::LlmExtractFn stub = [&transcript](
                                          const std::string& received) {
        EXPECT_EQ(received, transcript);
        return std::optional<std::string>(
            std::string("Here you go: {") +
            R"("underlying_goal":"g","goal_categories":{"fix_bug":2},)"
            R"("outcome":"fully_achieved","user_satisfaction_counts":{"happy":1},)"
            R"("loom_helpfulness":"very_helpful","session_type":"single_task",)"
            R"("friction_counts":{"buggy_code":1},"friction_detail":"",)"
            R"("primary_success":"none","brief_summary":"b"}) done.)");
    };
    auto f = loom::commands::extract_facets_with_seam(stub, transcript, "sid");
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->session_id, "sid");
    EXPECT_EQ(f->underlying_goal, "g");
    EXPECT_EQ(f->goal_categories.at("fix_bug"), 2u);
    EXPECT_EQ(f->outcome, "fully_achieved");
}

TEST(Insights, ExtractFacetsSeamRejectsInvalidSchema) {
    loom::commands::LlmExtractFn stub = [](const std::string&) {
        // Object present but missing required fields.
        return std::optional<std::string>(std::string(R"({"foo":1})"));
    };
    auto f = loom::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamRejectsNoJson) {
    loom::commands::LlmExtractFn stub = [](const std::string&) {
        return std::optional<std::string>(std::string("no json here at all"));
    };
    auto f = loom::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamHandlesNullFn) {
    loom::commands::LlmExtractFn stub;  // empty
    auto f = loom::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, ExtractFacetsSeamHandlesEmptyResponse) {
    loom::commands::LlmExtractFn stub = [](const std::string&) {
        return std::optional<std::string>(std::string(""));
    };
    auto f = loom::commands::extract_facets_with_seam(stub, "t", "sid");
    EXPECT_FALSE(f.has_value());
}

TEST(Insights, FacetExtractionPromptContainsGuidelines) {
    const auto& p = loom::commands::facet_extraction_prompt();
    // The prompt must mention the key contract elements from the TS source.
    EXPECT_NE(p.find("goal_categories"), std::string::npos);
    EXPECT_NE(p.find("user_satisfaction_counts"), std::string::npos);
    EXPECT_NE(p.find("friction_counts"), std::string::npos);
    EXPECT_NE(p.find("warmup_minimal"), std::string::npos);
    EXPECT_NE(p.find("SESSION:"), std::string::npos);
}

TEST(Insights, CommandDefinitionAndModel) {
    auto def = loom::commands::InsightsCommand::definition();
    EXPECT_EQ(def.name, "insights");
    EXPECT_FALSE(def.hidden);
}

TEST(Insights, CommandExecuteWithNoSessionsIsGraceful) {
    HomeGuard guard;
    // Point HOME at an empty dir with no sessions subdir -> list_sessions empty.
    loom::commands::InsightsCommand cmd;
    loom::core::CommandContext cctx{};
    auto res = cmd.execute(cctx);
    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(res->ok);
    EXPECT_NE(res->message.find("No sessions found"), std::string::npos);
}
