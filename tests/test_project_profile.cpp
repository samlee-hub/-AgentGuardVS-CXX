#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "project/ProjectProfile.h"

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

void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}
} // namespace

TEST(ProjectProfileTest, DetectsVisualStudioCxxProjectFromVcxproj)
{
    const auto root = MakeTempRoot("agentguard_profile_vs_");
    WriteText(root / "App" / "App.vcxproj", "<Project></Project>");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.project_type, agentguard::ProjectType::VisualStudioCxx);
    EXPECT_EQ(profile.adapter, "visualstudio-cxx");
    ASSERT_EQ(profile.project_files.size(), 1U);
    EXPECT_EQ(profile.project_files.front(), "App/App.vcxproj");

    fs::remove_all(root);
}

TEST(ProjectProfileTest, DetectsNodeWebProjectFromPackageJson)
{
    const auto root = MakeTempRoot("agentguard_profile_node_");
    WriteText(root / "package.json", R"({"scripts":{"build":"vite build","test":"vitest"}})");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.project_type, agentguard::ProjectType::NodeWeb);
    EXPECT_EQ(profile.adapter, "node-web");
    ASSERT_EQ(profile.verify_steps.size(), 2U);
    EXPECT_EQ(profile.verify_steps[0].name, "build");
    EXPECT_EQ(profile.verify_steps[1].name, "test");

    fs::remove_all(root);
}

TEST(ProjectProfileTest, AgentGuardJsonOverridesDetectedProfileAndAddsCommands)
{
    const auto root = MakeTempRoot("agentguard_profile_config_");
    WriteText(root / "package.json", R"({"scripts":{"build":"vite build"}})");
    WriteText(
        root / "agentguard.json",
        R"({
  "project_type": "custom",
  "adapter": "command-profile",
  "source_roots": ["src"],
  "test_roots": ["tests"],
  "protected_files": [".env"],
  "verify_steps": [
    {
      "name": "unit",
      "executable": "C:\\Windows\\System32\\cmd.exe",
      "arguments": ["/C", "echo ok"]
    }
  ]
})");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.project_type, agentguard::ProjectType::Custom);
    EXPECT_EQ(profile.adapter, "command-profile");
    ASSERT_EQ(profile.source_roots.size(), 1U);
    EXPECT_EQ(profile.source_roots.front(), "src");
    ASSERT_EQ(profile.test_roots.size(), 1U);
    EXPECT_EQ(profile.test_roots.front(), "tests");
    ASSERT_EQ(profile.protected_files.size(), 1U);
    EXPECT_EQ(profile.protected_files.front(), ".env");
    ASSERT_EQ(profile.verify_steps.size(), 1U);
    EXPECT_EQ(profile.verify_steps.front().name, "unit");

    fs::remove_all(root);
}
