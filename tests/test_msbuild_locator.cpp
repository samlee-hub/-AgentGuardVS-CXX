#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "vs/MSBuildLocator.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::LocateMSBuild;
using agentguard::MSBuildLocatorOptions;

fs::path MakeTempRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_msbuild_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

TEST(MSBuildLocatorTest, EnvironmentVariableHasHighestPriority)
{
    const fs::path root = MakeTempRoot();
    const fs::path fake_msbuild = root / "MSBuild.exe";
    std::ofstream(fake_msbuild) << "stub";
    _putenv_s("MSBUILD_PATH", fake_msbuild.string().c_str());

    MSBuildLocatorOptions options;
    options.enable_environment = true;
    options.enable_vswhere = false;
    options.enable_common_paths = false;
    options.enable_path_lookup = false;

    const auto result = LocateMSBuild(options);

    EXPECT_TRUE(result.found);
    EXPECT_EQ(result.path, fake_msbuild);

    _putenv_s("MSBUILD_PATH", "");
    fs::remove_all(root);
}

TEST(MSBuildLocatorTest, MissingSearchProducesExplicitMessage)
{
    MSBuildLocatorOptions options;
    options.enable_environment = false;
    options.enable_vswhere = false;
    options.enable_common_paths = false;
    options.enable_path_lookup = false;

    const auto result = LocateMSBuild(options);

    EXPECT_FALSE(result.found);
    EXPECT_NE(result.message.find("not found"), std::string::npos);
}
} // namespace
