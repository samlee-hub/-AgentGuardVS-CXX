#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

fs::path MakeRunsRoot(const std::string& prefix)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / (prefix + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

agentguard::ProcessResult RunCommand(std::vector<std::wstring> arguments)
{
    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = std::move(arguments);
    return ProcessRunner().Run(request);
}
} // namespace

TEST(CliAgentCommandsTest, HelpIsAvailableForAgentFacingCommands)
{
    for (const std::wstring command : {L"analyze", L"verify", L"review", L"run"})
    {
        const auto result = RunCommand({command, L"--help"});

        EXPECT_EQ(result.return_code, 0) << result.stderr_text;
        EXPECT_NE(result.stdout_text.find("--json"), std::string::npos);
    }
}

TEST(CliAgentCommandsTest, RunDryRunJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_run_json_");
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    const auto result = RunCommand({
        L"run",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--runs-root",
        runs_root.wstring(),
        L"--dry-run",
        L"--provider",
        L"fake",
        L"--force",
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("mode").get<std::string>(), "dry_run");
    EXPECT_TRUE(summary.contains("workspace"));
    EXPECT_EQ(summary.at("runs_root_source").get<std::string>(), "user_specified");

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, AnalyzeJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_analyze_json_");
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    const auto result = RunCommand({
        L"analyze",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"fake",
        L"--runs-root",
        runs_root.wstring(),
        L"--force",
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_TRUE(summary.contains("semantic_scope"));
    EXPECT_TRUE(summary.contains("report"));
    EXPECT_GE(summary.at("counts").at("allowed").get<int>(), 0);
    EXPECT_EQ(summary.at("runs_root_source").get<std::string>(), "user_specified");

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, VerifyJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_verify_json_");
    const fs::path workspace = runs_root / "repo";
    fs::create_directories(workspace);
    fs::copy(
        fs::path(MINI_VS_PROJECT),
        workspace,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing);

    const auto result = RunCommand({
        L"verify",
        L"--workspace",
        workspace.wstring(),
        L"--configuration",
        L"Debug",
        L"--platform",
        L"x64",
        L"--json"
    });

    EXPECT_TRUE(result.return_code == 0 || result.return_code == 1);
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.contains("ok"));
    EXPECT_TRUE(summary.contains("build"));
    EXPECT_TRUE(summary.at("artifacts").contains("log"));

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, AnalyzeJsonFailureUsesStableErrorEnvelope)
{
    const auto result = RunCommand({
        L"analyze",
        L"--solution",
        L"Z:\\does-not-exist\\Missing.sln",
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"fake",
        L"--json"
    });

    EXPECT_NE(result.return_code, 0);
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_FALSE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("error_code").get<std::string>(), "SOLUTION_PARSE_FAILED");
    EXPECT_TRUE(summary.contains("suggestion"));
}
