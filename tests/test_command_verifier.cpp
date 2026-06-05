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

agentguard::ProjectProfile MakeProfile(const fs::path& root)
{
    agentguard::ProjectProfile profile;
    profile.root = root;
    profile.kind = agentguard::ProjectKind::Custom;
    profile.project_type = profile.kind;
    profile.adapter = "custom";
    return profile;
}
} // namespace

TEST(CommandVerifierTest, RunsConfiguredCommandAndCapturesOutput)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_ok_");
    auto profile = MakeProfile(root);
    profile.verify_commands.push_back(agentguard::CommandStep{
        "unit",
        "C:\\Windows\\System32\\cmd.exe",
        {"/C", "echo profile-ok"},
        ".",
        30,
        true,
        false
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_EQ(result.steps.front().name, "unit");
    EXPECT_EQ(result.steps.front().status, "PASS");
    EXPECT_EQ(result.steps.front().return_code, 0);
    EXPECT_NE(result.steps.front().stdout_text.find("profile-ok"), std::string::npos);

    fs::remove_all(root);
}

TEST(CommandVerifierTest, FailingRequiredCommandMarksOverallVerificationFailed)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_fail_");
    auto profile = MakeProfile(root);
    profile.verify_commands.push_back(agentguard::CommandStep{
        "unit",
        "C:\\Windows\\System32\\cmd.exe",
        {"/C", "exit /b 7"},
        ".",
        30,
        true,
        false
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_FALSE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_EQ(result.steps.front().status, "FAIL");
    EXPECT_EQ(result.steps.front().return_code, 7);

    fs::remove_all(root);
}

TEST(CommandVerifierTest, MissingExecutableCanBeSkipped)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_skip_");
    auto profile = MakeProfile(root);
    profile.verify_commands.push_back(agentguard::CommandStep{
        "optional-tool",
        "definitely-missing-agentguard-tool.exe",
        {},
        ".",
        30,
        true,
        true
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_TRUE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_TRUE(result.steps.front().skipped);
    EXPECT_EQ(result.steps.front().status, "SKIP");

    fs::remove_all(root);
}

TEST(CommandVerifierTest, MissingRequiredExecutableWithoutSkipFails)
{
    const auto root = MakeTempRoot("agentguard_verify_profile_missing_");
    auto profile = MakeProfile(root);
    profile.verify_commands.push_back(agentguard::CommandStep{
        "required-tool",
        "definitely-missing-agentguard-tool.exe",
        {},
        ".",
        30,
        true,
        false
    });

    const auto result = agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

    EXPECT_FALSE(result.success);
    ASSERT_EQ(result.steps.size(), 1U);
    EXPECT_FALSE(result.steps.front().skipped);
    EXPECT_EQ(result.steps.front().status, "FAIL");

    fs::remove_all(root);
}
