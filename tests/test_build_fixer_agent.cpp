#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/BuildFixerAgent.h"
#include "llm/FakeLLMProvider.h"

namespace
{
using agentguard::BuildFixerInput;
using agentguard::ErrorCategory;
using agentguard::FakeLLMProvider;
using agentguard::ParsedBuildError;
using agentguard::RelevantFileContent;
using agentguard::TaskSpec;
using agentguard::TryCreateBuildFixPatchPlan;

ParsedBuildError MakeError(std::string code, ErrorCategory category)
{
    ParsedBuildError error;
    error.file = "src/Summon.cpp";
    error.line = 10;
    error.code = std::move(code);
    error.category = category;
    error.message = "sample error";
    return error;
}

BuildFixerInput MakeInput()
{
    TaskSpec spec;
    spec.task_id = "fix-001";
    spec.user_request = "Add summon cooldown.";
    spec.allowed_files = {"src/Summon.cpp"};
    spec.forbidden_files = {"src/Auth.cpp"};
    spec.max_repair_rounds = 3;

    BuildFixerInput input;
    input.task_spec = spec;
    input.build_result.success = false;
    input.build_result.return_code = 1;
    input.parsed_errors = {
        MakeError("C1083", ErrorCategory::IncludeError),
        MakeError("C2065", ErrorCategory::CompileError),
        MakeError("LNK2019", ErrorCategory::LinkError),
        MakeError("MSB3073", ErrorCategory::MSBuildError)
    };
    input.current_files = {
        RelevantFileContent{"src/Summon.cpp", "#include \"Summon.h\"\n"}
    };
    input.git_diff_summary = "1 file changed";
    return input;
}

TEST(BuildFixerAgentTest, CreatesPatchPlanAndIncludesCategorySpecificGuidance)
{
    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Summon.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int cooldown = 0;"},
                    {"replacement_snippet", "int cooldown = 3;"},
                    {"explanation", "Fix cooldown compile issue."}
                }
            })}
        });

    const auto result = TryCreateBuildFixPatchPlan(MakeInput(), provider);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.patch_plan.changes.size(), 1U);
    EXPECT_EQ(result.patch_plan.changes.front().target_file, "src/Summon.cpp");
    EXPECT_NE(result.prompt.find("include path"), std::string::npos);
    EXPECT_NE(result.prompt.find("variable declarations"), std::string::npos);
    EXPECT_NE(result.prompt.find("declaration and definition"), std::string::npos);
    EXPECT_NE(result.prompt.find("project configuration"), std::string::npos);
}

TEST(BuildFixerAgentTest, RejectsWorkspaceEscapingPatchTargets)
{
    FakeLLMProvider provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "..\\outside.cpp"},
                    {"change_type", "create"},
                    {"original_snippet", ""},
                    {"replacement_snippet", "int x = 0;"},
                    {"explanation", "Must reject."}
                }
            })}
        });

    const auto result = TryCreateBuildFixPatchPlan(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("workspace-relative"), std::string::npos);
}
} // namespace
