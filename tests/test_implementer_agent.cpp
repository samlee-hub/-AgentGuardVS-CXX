#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/ImplementerAgent.h"
#include "llm/FakeLLMProvider.h"

namespace
{
using agentguard::FakeLLMProvider;
using agentguard::ImplementerInput;
using agentguard::RelevantFileContent;
using agentguard::TaskSpec;
using agentguard::TryCreatePatchPlan;

ImplementerInput MakeInput()
{
    TaskSpec spec;
    spec.task_id = "impl-001";
    spec.allowed_files = {"src/Summon.cpp"};
    spec.forbidden_files = {"src/Auth.cpp"};

    ImplementerInput input;
    input.task_spec = spec;
    input.project_info.solution_name = "Server";
    input.project_info.solution_path = "Server.sln";
    input.relevant_files = {
        RelevantFileContent{"src/Summon.cpp", "int cost = 1;\n"}
    };
    return input;
}

TEST(ImplementerAgentTest, ParsesValidatedPatchPlanFromStrictJson)
{
    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Summon.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int cost = 1;"},
                    {"replacement_snippet", "int cost = 2;"},
                    {"explanation", "Increase summon cost."}
                }
            })}
        });

    const auto result = TryCreatePatchPlan(MakeInput(), provider);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.patch_plan.changes.size(), 1U);
    EXPECT_EQ(result.patch_plan.changes.front().target_file, "src/Summon.cpp");
    EXPECT_NE(result.prompt.find("strict JSON"), std::string::npos);
    EXPECT_NE(result.prompt.find("PatchPlan"), std::string::npos);
}

TEST(ImplementerAgentTest, RejectsPatchTargetsThatEscapeWorkspace)
{
    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "../escape.cpp"},
                    {"change_type", "create"},
                    {"original_snippet", ""},
                    {"replacement_snippet", "int escaped = 1;"},
                    {"explanation", "Must reject."}
                }
            })}
        });

    const auto result = TryCreatePatchPlan(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("workspace-relative"), std::string::npos);
}
} // namespace
