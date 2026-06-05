#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "core/Models.h"

namespace
{
using agentguard::BuildResult;
using agentguard::ErrorCategory;
using agentguard::ParsedBuildError;
using agentguard::TaskSpec;
using nlohmann::json;

TEST(ModelsJsonTest, TaskSpecRoundTripsThroughJson)
{
    TaskSpec spec;
    spec.task_id = "summon-cooldown-001";
    spec.user_request = "Add resource cost and a global summon cooldown.";
    spec.target_solution = "Server.sln";
    spec.target_project = "Server";
    spec.allowed_files = {
        "Server/Game/SummonService.cpp",
        "Server/Game/SummonService.h"
    };
    spec.forbidden_files = {
        "Server/Auth/LoginService.cpp"
    };
    spec.acceptance_criteria = {
        "Only isolated workspace files may change.",
        "The task must remain within the repair round limit."
    };
    spec.max_repair_rounds = 3;

    const json serialized = spec;
    const TaskSpec restored = serialized.get<TaskSpec>();

    EXPECT_EQ(restored.task_id, spec.task_id);
    EXPECT_EQ(restored.user_request, spec.user_request);
    EXPECT_EQ(restored.target_solution, spec.target_solution);
    EXPECT_EQ(restored.target_project, spec.target_project);
    EXPECT_EQ(restored.allowed_files, spec.allowed_files);
    EXPECT_EQ(restored.forbidden_files, spec.forbidden_files);
    EXPECT_EQ(restored.acceptance_criteria, spec.acceptance_criteria);
    EXPECT_EQ(restored.max_repair_rounds, spec.max_repair_rounds);
}

TEST(ModelsJsonTest, BuildResultRoundTripsWithParsedErrors)
{
    ParsedBuildError error;
    error.file = "Server/Game/SummonService.cpp";
    error.line = 42;
    error.column = 13;
    error.code = "C2065";
    error.message = "identifier not found";
    error.category = ErrorCategory::CompileError;
    error.raw_line = "SummonService.cpp(42,13): error C2065: identifier not found";

    BuildResult result;
    result.success = false;
    result.command = "msbuild Server.sln /m /p:Configuration=Debug /p:Platform=x64 /v:minimal";
    result.return_code = 1;
    result.stdout_text = "build output";
    result.stderr_text = "build error";
    result.duration_ms = 1250;
    result.parsed_errors = {error};

    const json serialized = result;
    const BuildResult restored = serialized.get<BuildResult>();

    ASSERT_EQ(restored.parsed_errors.size(), 1U);
    EXPECT_FALSE(restored.success);
    EXPECT_EQ(restored.command, result.command);
    EXPECT_EQ(restored.return_code, result.return_code);
    EXPECT_EQ(restored.stdout_text, result.stdout_text);
    EXPECT_EQ(restored.stderr_text, result.stderr_text);
    EXPECT_EQ(restored.duration_ms, result.duration_ms);
    EXPECT_EQ(restored.parsed_errors[0].file, error.file);
    EXPECT_EQ(restored.parsed_errors[0].line, error.line);
    EXPECT_EQ(restored.parsed_errors[0].column, error.column);
    EXPECT_EQ(restored.parsed_errors[0].code, error.code);
    EXPECT_EQ(restored.parsed_errors[0].message, error.message);
    EXPECT_EQ(restored.parsed_errors[0].category, error.category);
    EXPECT_EQ(restored.parsed_errors[0].raw_line, error.raw_line);
}
} // namespace
