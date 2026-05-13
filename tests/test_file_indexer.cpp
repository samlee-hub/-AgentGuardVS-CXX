#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "indexing/FileIndexer.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::IndexSourceFiles;

TEST(FileIndexerTest, ScansSupportedCppExtensionsAndSkipsExcludedDirectories)
{
    const fs::path root =
        fs::path(__FILE__).parent_path() /
        "fixtures" /
        "mini_vs_project" /
        "IndexingSample";

    const auto files = IndexSourceFiles(root);

    std::vector<std::string> paths;
    for (const auto& file : files)
    {
        paths.push_back(file.file_path);
    }

    EXPECT_EQ(paths, (std::vector<std::string>{
        "Engine\\Battle.cc",
        "Game\\Summon.cpp",
        "Game\\Summon.h",
        "Game\\Summon.hpp",
        "World\\Arena.cxx"
    }));
}
} // namespace
