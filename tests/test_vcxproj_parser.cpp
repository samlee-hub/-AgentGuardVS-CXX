#include <filesystem>

#include <gtest/gtest.h>

#include "vs/VcxprojParser.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ParseVcxproj;

TEST(VcxprojParserTest, ParsesCoreProjectFieldsFromNamespacedXml)
{
    const fs::path fixture_path =
        fs::path(__FILE__).parent_path() /
        "fixtures" /
        "mini_vs_project" /
        "MiniGame" /
        "MiniGame.vcxproj";

    const auto project = ParseVcxproj(fixture_path);

    EXPECT_EQ(project.project_name, "MiniGame");
    EXPECT_EQ(fs::path(project.project_path).filename(), "MiniGame.vcxproj");
    EXPECT_EQ(project.configurations, (std::vector<std::string>{"Debug", "Release"}));
    EXPECT_EQ(project.platforms, (std::vector<std::string>{"x64"}));
    EXPECT_EQ(project.source_files, (std::vector<std::string>{"main.cpp", "Game\\Summon.cpp"}));
    EXPECT_EQ(project.header_files, (std::vector<std::string>{"Game\\Summon.h"}));
    EXPECT_EQ(project.include_directories, (std::vector<std::string>{"include", "$(ProjectDir)Game"}));
    EXPECT_EQ(project.preprocessor_definitions, (std::vector<std::string>{"WIN32", "_DEBUG"}));
}
} // namespace
