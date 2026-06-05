#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

#include "indexing/FileIndexer.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::IndexSourceFiles;

void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

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
        paths.push_back(fs::path(file.file_path).generic_string());
    }

    EXPECT_EQ(paths, (std::vector<std::string>{
        "Engine/Battle.cc",
        "Game/Summon.cpp",
        "Game/Summon.h",
        "Game/Summon.hpp",
        "World/Arena.cxx"
    }));
}

TEST(FileIndexerTest, ScansProfilePolicySourcesAndConfigFiles)
{
    const fs::path root = fs::temp_directory_path() / "agentguard_file_indexer_policy";
    fs::remove_all(root);
    WriteText(root / "src" / "app.py", "print('ok')");
    WriteText(root / "src" / "ui.ts", "export const x = 1;");
    WriteText(root / "src" / "main.java", "class Main {}");
    WriteText(root / "src" / "Program.cs", "class Program {}");
    WriteText(root / "package.json", R"({"scripts":{}})");
    WriteText(root / "pyproject.toml", "[project]");
    WriteText(root / "node_modules" / "ignored.ts", "ignored");
    WriteText(root / "runs" / "ignored.py", "ignored");
    WriteText(root / "build" / "ignored.java", "ignored");

    agentguard::SourceFilePolicy policy;
    policy.source_extensions = {".py", ".ts", ".java", ".cs"};
    policy.config_file_patterns = {"package.json", "pyproject.toml"};
    policy.exclude_dirs = {
        ".git", "build", "runs", "node_modules", "__pycache__", ".venv", "venv"
    };

    const auto result = agentguard::IndexSourceFilesWithPolicy(root, policy);
    std::vector<std::string> paths;
    for (const auto& file : result.files)
    {
        paths.push_back(fs::path(file.file_path).generic_string());
    }

    EXPECT_EQ(paths, (std::vector<std::string>{
        "package.json",
        "pyproject.toml",
        "src/Program.cs",
        "src/app.py",
        "src/main.java",
        "src/ui.ts"
    }));
    EXPECT_EQ(result.skipped_files, 0U);

    fs::remove_all(root);
}

TEST(FileIndexerTest, IncludeExcludeRulesRestrictIndexedFiles)
{
    const fs::path root = fs::temp_directory_path() / "agentguard_file_indexer_include";
    fs::remove_all(root);
    WriteText(root / "src" / "keep.py", "print('ok')");
    WriteText(root / "tests" / "keep_test.py", "print('ok')");
    WriteText(root / "docs" / "skip.py", "print('no')");
    WriteText(root / "src" / "generated" / "skip.py", "print('no')");

    agentguard::SourceFilePolicy policy;
    policy.include = {"src/**", "tests/**"};
    policy.exclude = {"src/generated/**"};
    policy.source_extensions = {".py"};
    policy.exclude_dirs = {};

    const auto result = agentguard::IndexSourceFilesWithPolicy(root, policy);
    std::vector<std::string> paths;
    for (const auto& file : result.files)
    {
        paths.push_back(fs::path(file.file_path).generic_string());
    }

    EXPECT_EQ(paths, (std::vector<std::string>{
        "src/keep.py",
        "tests/keep_test.py"
    }));
    EXPECT_GE(result.skipped_files, 2U);

    fs::remove_all(root);
}
} // namespace
