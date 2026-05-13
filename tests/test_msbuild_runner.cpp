#include <filesystem>

#include <gtest/gtest.h>

#include "vs/MSBuildRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::BuildSolution;
using agentguard::IProcessRunner;
using agentguard::ProcessRequest;
using agentguard::ProcessResult;

class FakeProcessRunner final : public IProcessRunner
{
public:
    mutable ProcessRequest last_request;
    ProcessResult next_result;

    ProcessResult Run(const ProcessRequest& request) const override
    {
        last_request = request;
        return next_result;
    }
};

TEST(MSBuildRunnerTest, BuildsExpectedCommandAndMapsProcessResult)
{
    FakeProcessRunner runner;
    runner.next_result.return_code = 0;
    runner.next_result.stdout_text = "build ok";
    runner.next_result.stderr_text = "";
    runner.next_result.duration_ms = 55;

    const auto result = BuildSolution(
        fs::path("C:\\workspace\\Server.sln"),
        "Debug",
        "x64",
        fs::path("C:\\Tools\\MSBuild.exe"),
        runner);

    ASSERT_EQ(runner.last_request.arguments.size(), 5U);
    EXPECT_EQ(runner.last_request.executable, L"C:\\Tools\\MSBuild.exe");
    EXPECT_EQ(runner.last_request.arguments[0], L"C:\\workspace\\Server.sln");
    EXPECT_EQ(runner.last_request.arguments[1], L"/m");
    EXPECT_EQ(runner.last_request.arguments[2], L"/p:Configuration=Debug");
    EXPECT_EQ(runner.last_request.arguments[3], L"/p:Platform=x64");
    EXPECT_EQ(runner.last_request.arguments[4], L"/v:minimal");
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.return_code, 0);
    EXPECT_EQ(result.stdout_text, "build ok");
    EXPECT_EQ(result.duration_ms, 55);
    EXPECT_NE(result.command.find("/p:Configuration=Debug"), std::string::npos);
}
} // namespace
