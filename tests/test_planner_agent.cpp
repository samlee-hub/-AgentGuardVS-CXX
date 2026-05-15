#include <algorithm>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/PlannerAgent.h"
#include "core/SemanticModels.h"
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
using agentguard::SemanticFileReference;
using agentguard::SemanticScopeResult;

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

SemanticScopeResult MakeSemanticScope()
{
    SemanticScopeResult scope;
    scope.task_summary = "Add summon cooldown.";
    scope.allowed_files = {
        SemanticFileReference{"src/Summon.cpp", "Owns summon behavior.", 0.95},
        SemanticFileReference{"src/Maybe.cpp", "Low confidence candidate.", 0.35},
        SemanticFileReference{"src/Approval.cpp", "Needs user approval.", 0.9},
        SemanticFileReference{"src/Protected.cpp", "Conflicting protected file.", 0.9}
    };
    scope.context_files = {
        SemanticFileReference{"src/Summon.h", "Declaration context.", 0.8}
    };
    scope.protected_files = {
        SemanticFileReference{"src/Protected.cpp", "Do not touch generated code.", 1.0}
    };
    scope.needs_approval_files = {
        SemanticFileReference{"src/Approval.cpp", "Public API change.", 0.9}
    };
    scope.risk_level = "medium";
    scope.recommendation = "proceed";
    return scope;
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

TEST(PlannerAgentTest, HybridSemanticScopeKeepsProtectedAndNeedsApprovalOutOfAllowed)
{
    auto input = MakeInput();
    input.has_semantic_scope = true;
    input.semantic_scope = MakeSemanticScope();
    input.semantic_allowed_confidence_threshold = 0.7;

    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"task_id", "summon-001"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Legacy.cpp"})},
            {"forbidden_files", nlohmann::json::array({"src/Auth.cpp"})},
            {"acceptance_criteria", nlohmann::json::array({"Build remains valid."})},
            {"max_repair_rounds", 3}
        });

    const auto result = TryPlanTask(input, provider);

    ASSERT_TRUE(result.success) << result.error_message;
    const std::vector<std::string> expected_allowed{"src/Summon.cpp"};
    const std::vector<std::string> expected_context{"src/Summon.h"};
    const std::vector<std::string> expected_suspected{"src/Maybe.cpp"};
    const std::vector<std::string> expected_needs_approval{"src/Approval.cpp"};
    const std::vector<std::string> expected_protected{"src/Protected.cpp"};
    EXPECT_EQ(result.task_spec.allowed_files, expected_allowed);
    EXPECT_EQ(result.task_spec.context_files, expected_context);
    EXPECT_EQ(result.task_spec.suspected_files, expected_suspected);
    EXPECT_EQ(result.task_spec.needs_approval_files, expected_needs_approval);
    EXPECT_EQ(result.task_spec.protected_files, expected_protected);
    EXPECT_NE(
        std::find(
            result.task_spec.forbidden_files.begin(),
            result.task_spec.forbidden_files.end(),
            "src/Protected.cpp"),
        result.task_spec.forbidden_files.end());
    EXPECT_TRUE(result.task_spec.has_semantic_scope);
}

TEST(PlannerAgentTest, ReportsProviderJsonFailuresClearly)
{
    ThrowingProvider provider;

    const auto result = TryPlanTask(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("invalid planner json"), std::string::npos);
}
} // namespace
