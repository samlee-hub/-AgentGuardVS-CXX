#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "evaluation/BenchmarkTask.h"
#include "evaluation/Metrics.h"

namespace
{
namespace fs = std::filesystem;

using agentguard::BenchmarkTask;
using agentguard::Metrics;

TEST(EvaluationTest, BenchmarkTaskRoundTripsThroughJson)
{
    BenchmarkTask task;
    task.task_id = "server1-summon-cost";
    task.title = "Add summon resource cost";
    task.description = "Require summon actions to consume configured resources.";
    task.expected_files = {"Server1/Server/StrategyBattleMode.cpp"};
    task.forbidden_files = {"Server1/Server/Server.cpp"};
    task.acceptance_criteria = {"Build succeeds"};
    task.difficulty = "medium";
    task.tags = {"summon", "resource"};

    const nlohmann::json json_value = task;
    const auto parsed = json_value.get<BenchmarkTask>();

    EXPECT_EQ(parsed.task_id, task.task_id);
    EXPECT_EQ(parsed.expected_files.front(), "Server1/Server/StrategyBattleMode.cpp");
    EXPECT_EQ(parsed.tags.size(), 2U);
}

TEST(EvaluationTest, MetricsRoundTripsThroughJson)
{
    Metrics metrics;
    metrics.task_id = "server1-summon-cost";
    metrics.build_success = true;
    metrics.repair_rounds = 1;
    metrics.error_type_counts = {{"CompileError", 2}};
    metrics.modified_file_count = 2;

    const nlohmann::json json_value = metrics;
    const auto parsed = json_value.get<Metrics>();

    EXPECT_TRUE(parsed.build_success);
    EXPECT_EQ(parsed.repair_rounds, 1);
    EXPECT_EQ(parsed.error_type_counts.at("CompileError"), 2);
    EXPECT_EQ(parsed.modified_file_count, 2);
}

TEST(EvaluationTest, BenchmarkTaskSetContainsRequiredFields)
{
    const fs::path tasks_path = fs::path(__FILE__).parent_path().parent_path() / "benchmarks" / "tasks.jsonl";
    std::ifstream input(tasks_path);
    ASSERT_TRUE(input) << tasks_path.string();

    int task_count = 0;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
        {
            continue;
        }

        const auto task = nlohmann::json::parse(line).get<BenchmarkTask>();
        EXPECT_FALSE(task.task_id.empty());
        EXPECT_FALSE(task.title.empty());
        EXPECT_FALSE(task.description.empty());
        EXPECT_FALSE(task.expected_files.empty());
        EXPECT_FALSE(task.forbidden_files.empty());
        EXPECT_FALSE(task.acceptance_criteria.empty());
        EXPECT_FALSE(task.difficulty.empty());
        EXPECT_FALSE(task.tags.empty());
        ++task_count;
    }

    EXPECT_GE(task_count, 10);
}
} // namespace
