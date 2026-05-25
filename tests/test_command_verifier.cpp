#include <chrono>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "project/CommandVerifier.h"

namespace
{
namespace fs = std::filesystem;

fs::path MakeTempRoot(const std::string& prefix)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / (prefix + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}
} // namespace

TEST(CommandVerifierTest, RunsConfiguredCommandAndCapturesOutput)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_ok_");
    agentguard::ProjectProfile profile;
    profile.root = root;
    profile.project_type = agentguard::ProjectType::Custom;
    profile.adapter = "command-profile";
    profile.verify_steps.push_back(agentguard::CommandStep{
        "unit",
        "C:\\Windows\\System32\\cmd.exe",
        {"/C", "echo profile-ok"}
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_EQ(result.steps.front().name, "unit");
    EXPECT_EQ(result.steps.front().return_code, 0);
    EXPECT_NE(result.steps.front().stdout_text.find("profile-ok"), std::string::npos);

    fs::remove_all(root);
}

TEST(CommandVerifierTest, FailingCommandMarksOverallVerificationFailed)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_fail_");
    agentguard::ProjectProfile profile;
    profile.root = root;
    profile.project_type = agentguard::ProjectType::Custom;
    profile.adapter = "command-profile";
    profile.verify_steps.push_back(agentguard::CommandStep{
        "unit",
        "C:\\Windows\\System32\\cmd.exe",
        {"/C", "exit /b 7"}
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_FALSE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_EQ(result.steps.front().return_code, 7);

    fs::remove_all(root);
}
