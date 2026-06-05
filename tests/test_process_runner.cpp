#include <filesystem>

#include <gtest/gtest.h>

#include "core/ProcessRunner.h"

namespace
{
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

TEST(ProcessRunnerTest, CapturesStdoutStderrReturnCodeAndDuration)
{
    ProcessRequest request;
    request.executable = L"C:\\Windows\\System32\\cmd.exe";
    request.arguments = {
        L"/C",
        L"echo stdout-text && echo stderr-text 1>&2 && exit /b 7"
    };

    const auto result = ProcessRunner().Run(request);

    EXPECT_EQ(result.return_code, 7);
    EXPECT_NE(result.stdout_text.find("stdout-text"), std::string::npos);
    EXPECT_NE(result.stderr_text.find("stderr-text"), std::string::npos);
    EXPECT_GE(result.duration_ms, 0);
}
} // namespace
