#include <stdexcept>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/PlannerAgent.h"
#include "llm/FakeLLMProvider.h"

namespace
{
using agentguard::ContextCandidate;
using agentguard::FakeLLMProvider;
using agentguard::ILLMProvider;
using agentguard::PlannerInput;
using agentguard::ProjectInfo;
using agentguard::SourceFileInfo;
using agentguard::TaskSpec;
using agentguard::TryPlanTask;

class ThrowingProvider final : public ILLMProvider
{
public:
    std::string GenerateText(std::string_view) const override
    {
        return {};
    }

    nlohmann::json GenerateJson(std::string_view) const override
    {
        throw std::runtime_error("invalid planner json");
    }
};

PlannerInput MakeInput()
{
    PlannerInput input;
    input.user_request = "Add summon cooldown.";
    input.project_info.solution_name = "Server";
    input.project_info.solution_path = "Server.sln";
    input.source_files = {
        SourceFileInfo{"src/Summon.cpp"}
    };
    input.context_candidates = {
        ContextCandidate{"src/Summon.cpp", {"filename:summon"}, 5}
    };
    return input;
}

TEST(PlannerAgentTest, ParsesAndValidatesTaskSpecFromProviderJson)
{
    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"task_id", "summon-001"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Summon.cpp"})},
            {"forbidden_files", nlohmann::json::array({"src/Auth.cpp"})},
            {"acceptance_criteria", nlohmann::json::array({"Build remains valid."})},
            {"max_repair_rounds", 3}
        });

    const auto result = TryPlanTask(MakeInput(), provider);

    ASSERT_TRUE(result.success);
    EXPECT_EQ(result.task_spec.task_id, "summon-001");
    EXPECT_EQ(result.task_spec.allowed_files.size(), 1U);
    EXPECT_NE(result.prompt.find("allowed_files"), std::string::npos);
    EXPECT_NE(result.prompt.find("PatchApplier"), std::string::npos);
    EXPECT_NE(result.prompt.find("original project"), std::string::npos);
}

TEST(PlannerAgentTest, ReportsProviderJsonFailuresClearly)
{
    ThrowingProvider provider;

    const auto result = TryPlanTask(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("invalid planner json"), std::string::npos);
}
} // namespace
