#include <filesystem>

#include <gtest/gtest.h>

#include "vs/SlnParser.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ParseSln;

TEST(SlnParserTest, ParsesCppProjectsAndEmitsWarningsForIgnoredEntries)
{
    const fs::path fixture_path =
        fs::path(__FILE__).parent_path() /
        "fixtures" /
        "mini_vs_project" /
        "Mini.sln";

    const auto result = ParseSln(fixture_path);

    ASSERT_EQ(result.projects.size(), 1U);
    EXPECT_EQ(result.solution_path.filename(), "Mini.sln");
    EXPECT_EQ(result.projects[0].project_name, "MiniGame");
    EXPECT_EQ(result.projects[0].project_path, "MiniGame\\MiniGame.vcxproj");
    EXPECT_EQ(result.projects[0].project_guid, "{11111111-2222-3333-4444-555555555555}");
    EXPECT_EQ(result.projects[0].project_type_guid, "{BC8A1FFA-BEE3-4634-8014-F334798102B3}");
    ASSERT_EQ(result.warnings.size(), 1U);
    EXPECT_NE(result.warnings[0].find("Solution Items"), std::string::npos);
}
} // namespace
