#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>

#include "core/ProcessRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

fs::path MakeRunsRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_cli_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

TEST(CliRunTest, DryRunNoLlmCreatesIsolatedWorkspaceAndPrintsTaskSpec)
{
    const fs::path runs_root = MakeRunsRoot();
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"run",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--runs-root",
        runs_root.wstring(),
        L"--dry-run",
        L"--no-llm",
        L"--force"
    };

    const ProcessRunner runner;
    const auto result = runner.Run(request);

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    EXPECT_NE(result.stdout_text.find("Dry run completed"), std::string::npos);
    EXPECT_NE(result.stdout_text.find("TaskSpec"), std::string::npos);
    EXPECT_NE(result.stdout_text.find("Related files"), std::string::npos);
    EXPECT_TRUE(fs::exists(runs_root));
    EXPECT_FALSE(fs::is_empty(runs_root));

    fs::remove_all(runs_root);
}
} // namespace
