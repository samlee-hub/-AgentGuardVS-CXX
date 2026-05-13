#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#define NOMINMAX
#include <Windows.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_size <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }

    std::string result(static_cast<std::size_t>(required_size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        required_size,
        nullptr,
        nullptr);
    return result;
}

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

TEST(CliRunTest, DryRunNoLlmPreservesUtf8TaskText)
{
    const fs::path runs_root = MakeRunsRoot();
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";
    const std::wstring task_text =
        L"\u4e3a\u53ec\u5524\u529f\u80fd\u589e\u52a0\u8d44\u6e90\u6d88\u8017"
        L"\u548c\u5168\u5c403\u79d2\u51b7\u5374";

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"run",
        L"--solution",
        solution.wstring(),
        L"--task",
        task_text,
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

    fs::path report_path;
    for (const auto& entry : fs::recursive_directory_iterator(runs_root))
    {
        if (entry.path().filename() == "report.json")
        {
            report_path = entry.path();
            break;
        }
    }
    ASSERT_FALSE(report_path.empty());

    {
        std::ifstream report_file(report_path);
        const auto report_json = nlohmann::json::parse(report_file);
        EXPECT_EQ(
            report_json.at("task_spec").at("user_request").get<std::string>(),
            WideToUtf8(task_text));
    }

    fs::remove_all(runs_root);
}
} // namespace
