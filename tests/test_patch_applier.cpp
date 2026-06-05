#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "patching/PatchApplier.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ApplyPatchPlan;
using agentguard::FileChange;
using agentguard::PatchPlan;
using agentguard::TaskSpec;

fs::path MakeRepo()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path repo = fs::temp_directory_path() / ("agentguard_patch_" + std::to_string(stamp)) / "repo";
    fs::create_directories(repo);
    return repo;
}

void WriteFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

TaskSpec MakeSpec()
{
    TaskSpec spec;
    spec.task_id = "patch-task";
    spec.allowed_files = {"src\\Game.cpp", "src\\Created.cpp"};
    spec.forbidden_files = {"src\\Secrets.cpp"};
    return spec;
}

TEST(PatchApplierTest, AppliesModifyAndCreatesBackup)
{
    const fs::path repo = MakeRepo();
    WriteFile(repo / "src" / "Game.cpp", "int cost = 1;\n");

    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "src\\Game.cpp",
        "modify",
        "int cost = 1;",
        "int cost = 2;",
        "raise resource cost"
    });

    const auto result = ApplyPatchPlan(plan, MakeSpec(), repo);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(ReadFile(repo / "src" / "Game.cpp"), "int cost = 2;\n");
    EXPECT_TRUE(fs::exists(repo / "src" / "Game.cpp.bak"));
    fs::remove_all(repo.parent_path());
}

TEST(PatchApplierTest, RejectsForbiddenFilesWithoutChangingThem)
{
    const fs::path repo = MakeRepo();
    WriteFile(repo / "src" / "Secrets.cpp", "int secret = 1;\n");

    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "src\\Secrets.cpp",
        "modify",
        "int secret = 1;",
        "int secret = 2;",
        "must reject"
    });

    const auto result = ApplyPatchPlan(plan, MakeSpec(), repo);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(ReadFile(repo / "src" / "Secrets.cpp"), "int secret = 1;\n");
    fs::remove_all(repo.parent_path());
}

TEST(PatchApplierTest, RejectsTraversalTargets)
{
    const fs::path repo = MakeRepo();
    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "..\\escape.cpp",
        "create",
        "",
        "int escaped = 1;",
        "must reject"
    });

    const auto result = ApplyPatchPlan(plan, MakeSpec(), repo);

    EXPECT_FALSE(result.success);
    fs::remove_all(repo.parent_path());
}

TEST(PatchApplierTest, RejectsProtectedPathsEvenWhenAllowed)
{
    const fs::path repo = MakeRepo();
    fs::create_directories(repo / "build");

    TaskSpec spec = MakeSpec();
    spec.allowed_files.push_back("build\\Generated.cpp");

    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "build\\Generated.cpp",
        "create",
        "",
        "int generated = 1;",
        "must reject protected path"
    });

    const auto result = ApplyPatchPlan(plan, spec, repo);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(fs::exists(repo / "build" / "Generated.cpp"));
    fs::remove_all(repo.parent_path());
}

TEST(PatchApplierTest, RejectsMissingOriginalSnippetWithoutPartialWrite)
{
    const fs::path repo = MakeRepo();
    WriteFile(repo / "src" / "Game.cpp", "int cost = 1;\n");

    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "src\\Game.cpp",
        "modify",
        "int cost = 999;",
        "int cost = 2;",
        "must reject"
    });

    const auto result = ApplyPatchPlan(plan, MakeSpec(), repo);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(ReadFile(repo / "src" / "Game.cpp"), "int cost = 1;\n");
    fs::remove_all(repo.parent_path());
}

TEST(PatchApplierTest, CreatesAllowedFiles)
{
    const fs::path repo = MakeRepo();
    PatchPlan plan;
    plan.changes.push_back(FileChange{
        "src\\Created.cpp",
        "create",
        "",
        "int created = 1;\n",
        "new file"
    });

    const auto result = ApplyPatchPlan(plan, MakeSpec(), repo);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(ReadFile(repo / "src" / "Created.cpp"), "int created = 1;\n");
    fs::remove_all(repo.parent_path());
}
} // namespace
