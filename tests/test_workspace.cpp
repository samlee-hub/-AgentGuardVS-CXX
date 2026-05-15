#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"
#include "core/PathUtils.h"
#include "core/Workspace.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::HasForbiddenPathTraversal;
using agentguard::ProcessRequest;
using agentguard::ProcessResult;
using agentguard::RunWorkspace;
using agentguard::SafeRelativePath;
using agentguard::WorkspaceOptions;

class MissingGitRunner final : public agentguard::IProcessRunner
{
public:
    ProcessResult Run(const ProcessRequest&) const override
    {
        ProcessResult result;
        result.return_code = 9009;
        result.stderr_text = "git executable was not found";
        return result;
    }
};

fs::path MakeUniqueTempRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_workspace_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

void WriteTextFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << text;
}

std::string ReadTextFile(const fs::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

TEST(WorkspaceTest, CreatesIsolatedWorkspaceAndMetadata)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";

    WriteTextFile(source_root / "Server.sln", "sample solution");
    WriteTextFile(source_root / "Game" / "Summon.cpp", "int summon() { return 1; }");

    const RunWorkspace workspace(WorkspaceOptions{
        source_root,
        runs_root,
        "task-create",
        false
    });

    const auto paths = workspace.Prepare();

    EXPECT_TRUE(fs::exists(paths.task_root));
    EXPECT_TRUE(fs::exists(paths.repo_root / "Server.sln"));
    EXPECT_TRUE(fs::exists(paths.repo_root / "Game" / "Summon.cpp"));
    EXPECT_EQ(ReadTextFile(source_root / "Game" / "Summon.cpp"), "int summon() { return 1; }");
    EXPECT_TRUE(fs::exists(paths.logs_root));
    EXPECT_TRUE(fs::exists(paths.reports_root));
    EXPECT_TRUE(fs::exists(paths.metadata_path));
    EXPECT_TRUE(fs::exists(paths.repo_root / ".git"));

    {
        std::ifstream metadata_file(paths.metadata_path);
        const auto metadata = nlohmann::json::parse(metadata_file);
        EXPECT_EQ(metadata.at("source_project_path").get<std::string>(), agentguard::NormalizePath(source_root).generic_string());
        EXPECT_EQ(metadata.at("repo_root").get<std::string>(), agentguard::NormalizePath(paths.repo_root).generic_string());
        EXPECT_EQ(metadata.at("diff_base").get<std::string>(), "agentguard baseline");
    }

    fs::remove_all(temp_root);
}

TEST(WorkspaceTest, ExcludesTransientProjectDirectoriesDuringCopy)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";

    WriteTextFile(source_root / "keep.cpp", "int keep = 1;");
    WriteTextFile(source_root / ".git" / "config", "git");
    WriteTextFile(source_root / ".vs" / "cache.bin", "vs");
    WriteTextFile(source_root / "build" / "output.obj", "obj");
    WriteTextFile(source_root / "Saved" / "cache.txt", "saved");

    const RunWorkspace workspace(WorkspaceOptions{
        source_root,
        runs_root,
        "task-excludes",
        false
    });

    const auto paths = workspace.Prepare();

    EXPECT_TRUE(fs::exists(paths.repo_root / "keep.cpp"));
    EXPECT_TRUE(fs::exists(paths.repo_root / ".git"));
    EXPECT_FALSE(fs::exists(paths.repo_root / ".vs"));
    EXPECT_FALSE(fs::exists(paths.repo_root / "build"));
    EXPECT_FALSE(fs::exists(paths.repo_root / "Saved"));

    fs::remove_all(temp_root);
}

TEST(WorkspaceTest, DoesNotCopySourceGitButCreatesIndependentBaseline)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";

    WriteTextFile(source_root / ".git" / "config", "source git config");
    WriteTextFile(source_root / "Library.sln", "sample solution");
    WriteTextFile(source_root / "LibraryApp" / "Book.cpp", "int book = 1;\n");

    const RunWorkspace workspace(WorkspaceOptions{
        source_root,
        runs_root,
        "task-baseline",
        false
    });

    const auto paths = workspace.Prepare();

    EXPECT_TRUE(fs::exists(paths.repo_root / ".git"));
    EXPECT_FALSE(fs::exists(paths.repo_root / ".git" / "config") &&
        ReadTextFile(paths.repo_root / ".git" / "config").find("source git config") != std::string::npos);

    fs::remove_all(temp_root);
}

TEST(WorkspaceTest, GitBaselineFailureReportsClearError)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";
    WriteTextFile(source_root / "Library.sln", "sample solution");
    MissingGitRunner missing_git;

    const RunWorkspace workspace(WorkspaceOptions{
        source_root,
        runs_root,
        "task-no-git",
        false,
        &missing_git
    });

    try
    {
        static_cast<void>(workspace.Prepare());
        FAIL() << "Expected Git baseline failure.";
    }
    catch (const std::runtime_error& exception)
    {
        const std::string message = exception.what();
        EXPECT_NE(message.find("Git baseline failed"), std::string::npos);
        EXPECT_NE(message.find("git executable was not found"), std::string::npos);
    }

    fs::remove_all(temp_root);
}

TEST(WorkspaceTest, RejectsOverwriteWhenForceIsFalse)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";
    WriteTextFile(source_root / "Server.sln", "sample solution");

    const RunWorkspace first(WorkspaceOptions{
        source_root,
        runs_root,
        "task-existing",
        false
    });
    first.Prepare();

    const RunWorkspace second(WorkspaceOptions{
        source_root,
        runs_root,
        "task-existing",
        false
    });

    EXPECT_THROW(second.Prepare(), std::runtime_error);

    fs::remove_all(temp_root);
}

TEST(WorkspaceTest, BlocksTraversalTaskIdsAndUnsafeRelativePaths)
{
    EXPECT_TRUE(HasForbiddenPathTraversal("../escape"));
    EXPECT_TRUE(HasForbiddenPathTraversal("..\\escape"));
    EXPECT_FALSE(HasForbiddenPathTraversal("safe/path/file.cpp"));
    EXPECT_THROW(SafeRelativePath("../escape"), std::invalid_argument);

    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path source_root = temp_root / "source";
    const fs::path runs_root = temp_root / "runs";
    WriteTextFile(source_root / "Server.sln", "sample solution");

    const RunWorkspace workspace(WorkspaceOptions{
        source_root,
        runs_root,
        "../escape",
        false
    });

    EXPECT_THROW(workspace.Prepare(), std::invalid_argument);

    fs::remove_all(temp_root);
}
} // namespace
