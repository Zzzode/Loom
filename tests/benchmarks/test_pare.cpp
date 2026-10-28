#include <gtest/gtest.h>

import std;
import loom.benchmarks.pare.schema;
import loom.benchmarks.pare.run;
import loom.benchmarks.pare.cli;
import loom.benchmarks.pare.workspace;
import loom.serdes.json;

namespace {
namespace pare = loom::benchmarks::pare;
namespace fs = std::filesystem;

class PareBenchmark : public testing::Test {
protected:
    fs::path original_cwd_ = fs::current_path();
    fs::path root_;

    void SetUp() override {
        // Each discovered test runs in its own process; a unique directory also
        // isolates concurrent presets and keeps artifacts out of the checkout.
        root_ = fs::temp_directory_path() /
                ("loom-pare-test-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(root_);
        fs::current_path(root_);
        Write("cases.json", R"({"version":"2.0","cases":[
            {"id":"first","name":"First","prompt":"Reply ok","assertion":{"type":"exact","expected":"ok"}},
            {"id":"second","name":"Second","prompt":"Reply ok again","assertion":{"type":"exact","expected":"ok"}}
        ]})");
        Write("baseline.sh",
              "printf '%s\\n' "
              "'{\"result\":\"ok\",\"usage\":{\"input_tokens\":180,\"output_tokens\":20}}'\n");
        Write("candidate.sh",
              "printf '%s\\n' "
              "'{\"result\":\"ok\",\"usage\":{\"input_tokens\":80,\"output_tokens\":20}}'\n");
    }

    void TearDown() override {
        fs::current_path(original_cwd_);
        fs::remove_all(root_);
    }

    void Write(std::string_view name, std::string_view contents) {
        std::ofstream out(root_ / name);
        ASSERT_TRUE(out.good());
        out << contents;
    }

    pare::RunBenchmarkParams Params() const {
        return {
            .cases_path = (root_ / "cases.json").string(),
            .model = std::nullopt,
            .permission_mode = "bypassPermissions",
            .max_cases = std::nullopt,
            .runs_per_case = 2,
            .baseline = {"baseline", "/bin/sh", {(root_ / "baseline.sh").string()}},
            .candidate = {"candidate", "/bin/sh", {(root_ / "candidate.sh").string()}},
            .fail_on_regression = false,
            .workspace_mode = pare::WorkspaceMode::Current,
        };
    }
};

TEST_F(PareBenchmark, EvaluationSummaryPreservesCountsAndRepeatedRuns) {
    const auto result = pare::run_benchmark(Params());
    ASSERT_TRUE(result);
    const auto& comparison = result->comparison;
    ASSERT_TRUE(comparison.aggregate.evaluation);
    const auto& summary = *comparison.aggregate.evaluation;
    EXPECT_EQ(summary.runs_per_case, 2);
    EXPECT_EQ(summary.evaluated_cases, 2);
    EXPECT_EQ(summary.skipped_cases, 0);
    EXPECT_EQ(summary.unavailable_cases, 0);
    EXPECT_DOUBLE_EQ(summary.weighted_token_reduction_pct, 0.5);
    ASSERT_EQ(comparison.per_case.size(), 2u);
    for (const auto& item : comparison.per_case) {
        EXPECT_TRUE(item.baseline.pass);
        EXPECT_TRUE(item.candidate.pass);
        EXPECT_EQ(item.baseline.runs.size(), 2u);
        EXPECT_EQ(item.candidate.runs.size(), 2u);
        EXPECT_EQ(item.baseline.usage.total_tokens, 200);
        EXPECT_EQ(item.candidate.usage.total_tokens, 100);
    }
    // Persisted format versions remain an external contract.
    EXPECT_EQ(comparison.version, "pare-benchmark-v1");
    std::ifstream file(fs::path(result->artifact_dir) / "comparison.json");
    ASSERT_TRUE(file.good());
    const std::string contents((std::istreambuf_iterator<char>(file)), {});
    const auto doc = loom::utils::json::parse(contents);
    ASSERT_TRUE(doc);
    EXPECT_EQ(doc->root().get("version").as_str(), "pare-benchmark-v1");
    EXPECT_EQ(doc->root().size(), 2u);
}

TEST_F(PareBenchmark, EvaluationSummaryCountsUnavailableCandidates) {
    Write("candidate.sh", "printf '%s\\n' '{\"result\":\"ok\"}'\n");
    const auto result = pare::run_benchmark(Params());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->comparison.aggregate.evaluation);
    const auto& summary = *result->comparison.aggregate.evaluation;
    EXPECT_EQ(summary.evaluated_cases, 0);
    EXPECT_EQ(summary.unavailable_cases, 2);
    EXPECT_DOUBLE_EQ(summary.weighted_token_reduction_pct, 0.0);
}

TEST_F(PareBenchmark, CliPrintsUnversionedEvaluationMetrics) {
    Write("config.json", R"({"cases":"cases.json","runs_per_case":2,"workspace_mode":"current",
        "baseline":{"command":"/bin/sh","args":["baseline.sh"]},
        "candidate":{"command":"/bin/sh","args":["candidate.sh"]}})");

    struct CaptureOutput {
        std::ostringstream output;
        std::streambuf* saved = std::cout.rdbuf(output.rdbuf());

        ~CaptureOutput() { std::cout.rdbuf(saved); }
    } capture;

    const char* argv[] = {"pare-benchmark", "--config", "config.json"};
    EXPECT_EQ(pare::cli_main(3, argv), 0);
    const auto output = capture.output.str();
    EXPECT_NE(
        output.find("Evaluation metrics: runs_per_case=2 evaluated=2 skipped=0 unavailable=0"),
        std::string::npos);
    EXPECT_NE(output.find("weighted_token_reduction=50%"), std::string::npos);
    EXPECT_EQ(output.find("V2 metrics:"), std::string::npos);
}
}  // namespace
