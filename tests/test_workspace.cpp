#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "core/PathUtils.h"
#include "core/Workspace.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::HasForbiddenPathTraversal;
using agentguard::RunWorkspace;
using agentguard::SafeRelativePath;
using agentguard::WorkspaceOptions;

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
    EXPECT_TRUE(fs::exists(paths.logs_root));
    EXPECT_TRUE(fs::exists(paths.reports_root));
    EXPECT_TRUE(fs::exists(paths.metadata_path));

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
    EXPECT_FALSE(fs::exists(paths.repo_root / ".git"));
    EXPECT_FALSE(fs::exists(paths.repo_root / ".vs"));
    EXPECT_FALSE(fs::exists(paths.repo_root / "build"));
    EXPECT_FALSE(fs::exists(paths.repo_root / "Saved"));

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
