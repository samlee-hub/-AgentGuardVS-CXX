#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

bool HasLanguage(const agentguard::ProjectProfile& profile, agentguard::LanguageKind language)
{
    return std::find(profile.languages.begin(), profile.languages.end(), language) != profile.languages.end();
}
} // namespace

TEST(ProjectProfileTest, DetectsVisualStudioCxxProjectFromVcxproj)
{
    const auto root = MakeTempRoot("agentguard_profile_vs_");
    WriteText(root / "App" / "App.vcxproj", "<Project></Project>");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.kind, agentguard::ProjectKind::VisualStudioCxx);
    EXPECT_EQ(profile.project_type, agentguard::ProjectKind::VisualStudioCxx);
    EXPECT_EQ(profile.adapter, "visualstudio-cxx");
    EXPECT_TRUE(HasLanguage(profile, agentguard::LanguageKind::Cpp));
    ASSERT_EQ(profile.project_files.size(), 1U);
    EXPECT_EQ(profile.project_files.front(), "App/App.vcxproj");
    EXPECT_NE(std::find(profile.source_extensions.begin(), profile.source_extensions.end(), ".cpp"),
        profile.source_extensions.end());

    fs::remove_all(root);
}

TEST(ProjectProfileTest, DetectsCMakeCppProject)
{
    const auto root = MakeTempRoot("agentguard_profile_cmake_");
    WriteText(root / "CMakeLists.txt", "cmake_minimum_required(VERSION 3.25)");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.kind, agentguard::ProjectKind::CMakeCpp);
    EXPECT_TRUE(HasLanguage(profile, agentguard::LanguageKind::Cpp));
    ASSERT_GE(profile.verify_commands.size(), 2U);
    EXPECT_EQ(profile.verify_commands[0].command, "cmake");

    fs::remove_all(root);
}

TEST(ProjectProfileTest, DetectsPythonNodeJavaDotNetAndUnrealMarkers)
{
    const auto python = MakeTempRoot("agentguard_profile_python_");
    WriteText(python / "pyproject.toml", "[project]\nname='demo'");
    EXPECT_EQ(agentguard::DetectProjectProfile(python).kind, agentguard::ProjectKind::Python);

    const auto node = MakeTempRoot("agentguard_profile_node_");
    WriteText(node / "package.json", R"({"scripts":{"build":"vite build","test":"vitest"}})");
    const auto node_profile = agentguard::DetectProjectProfile(node);
    EXPECT_EQ(node_profile.kind, agentguard::ProjectKind::Node);
    EXPECT_TRUE(HasLanguage(node_profile, agentguard::LanguageKind::TypeScript));

    const auto java = MakeTempRoot("agentguard_profile_java_");
    WriteText(java / "pom.xml", "<project></project>");
    EXPECT_EQ(agentguard::DetectProjectProfile(java).kind, agentguard::ProjectKind::Java);

    const auto dotnet = MakeTempRoot("agentguard_profile_dotnet_");
    WriteText(dotnet / "App" / "App.csproj", "<Project></Project>");
    EXPECT_EQ(agentguard::DetectProjectProfile(dotnet).kind, agentguard::ProjectKind::DotNet);

    const auto unreal = MakeTempRoot("agentguard_profile_unreal_");
    WriteText(unreal / "Game.uproject", "{}");
    EXPECT_EQ(agentguard::DetectProjectProfile(unreal).kind, agentguard::ProjectKind::Unreal);

    fs::remove_all(python);
    fs::remove_all(node);
    fs::remove_all(java);
    fs::remove_all(dotnet);
    fs::remove_all(unreal);
}

TEST(ProjectProfileTest, AgentGuardJsonOverridesSourceExtensionsAndVerifyCommands)
{
    const auto root = MakeTempRoot("agentguard_profile_config_");
    WriteText(root / "package.json", R"({"scripts":{"build":"vite build"}})");
    WriteText(
        root / "agentguard.json",
        R"({
  "project_kind": "custom",
  "languages": ["cpp", "python", "typescript"],
  "include": ["src/**", "tests/**"],
  "exclude": ["build/**", "node_modules/**", "runs/**"],
  "source_extensions": [".cpp", ".h", ".py", ".ts"],
  "verify_commands": [
    {
      "name": "unit",
      "command": "C:\\Windows\\System32\\cmd.exe",
      "args": ["/C", "echo ok"],
      "working_directory": ".",
      "timeout_seconds": 120,
      "required": true,
      "skip_if_missing_executable": true
    }
  ]
})");

    const auto profile = agentguard::DetectProjectProfile(root);

    EXPECT_EQ(profile.kind, agentguard::ProjectKind::Custom);
    EXPECT_EQ(profile.profile_source, "custom");
    EXPECT_TRUE(HasLanguage(profile, agentguard::LanguageKind::Cpp));
    EXPECT_TRUE(HasLanguage(profile, agentguard::LanguageKind::Python));
    ASSERT_EQ(profile.source_policy.include.size(), 2U);
    ASSERT_EQ(profile.source_policy.exclude.size(), 3U);
    ASSERT_EQ(profile.verify_commands.size(), 1U);
    EXPECT_EQ(profile.verify_commands.front().name, "unit");
    EXPECT_EQ(profile.verify_commands.front().command, "C:\\Windows\\System32\\cmd.exe");

    fs::remove_all(root);
}

TEST(ProjectProfileTest, RejectsAgentGuardJsonPathTraversal)
{
    const auto root = MakeTempRoot("agentguard_profile_traversal_");
    WriteText(
        root / "agentguard.json",
        R"({
  "project_kind": "custom",
  "include": ["../outside/**"]
})");

    EXPECT_THROW(agentguard::DetectProjectProfile(root), std::exception);

    fs::remove_all(root);
}
