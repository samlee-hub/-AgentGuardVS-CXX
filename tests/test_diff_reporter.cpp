#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#define NOMINMAX
#include <Windows.h>

#include <gtest/gtest.h>

#include "core/ProcessRunner.h"
#include "patching/DiffReporter.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::CaptureWorkspaceDiff;
using agentguard::ProcessRequest;
using agentguard::ProcessResult;
using agentguard::ProcessRunner;

fs::path MakeUniqueTempRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_diff_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

void WriteTextFile(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::trunc);
    file << text;
}

ProcessResult RunGit(const fs::path& working_directory, std::vector<std::wstring> arguments)
{
    wchar_t buffer[MAX_PATH];
    const DWORD found = SearchPathW(nullptr, L"git.exe", nullptr, MAX_PATH, buffer, nullptr);
    ProcessRequest request;
    request.executable = found > 0 && found < MAX_PATH ? std::wstring(buffer) : L"git.exe";
    request.arguments = std::move(arguments);
    request.working_directory = working_directory;
    return ProcessRunner().Run(request);
}

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
} // namespace

TEST(DiffReporterTest, InitializesOwnBaselineInsideParentGitRepository)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path parent_repo = temp_root / "parent";
    const fs::path workspace_repo = parent_repo / "runs" / "task" / "repo";
    WriteTextFile(parent_repo / "README.md", "parent\n");
    ASSERT_EQ(RunGit(parent_repo, {L"init"}).return_code, 0);

    WriteTextFile(workspace_repo / "LibraryApp" / "Book.cpp", "int book = 1;\n");

    const auto initial = CaptureWorkspaceDiff(workspace_repo);

    EXPECT_TRUE(initial.success) << initial.warnings.front();
    EXPECT_EQ(fs::weakly_canonical(initial.git_top_level), fs::weakly_canonical(workspace_repo));
    EXPECT_EQ(initial.diff_base, "agentguard baseline");
    EXPECT_TRUE(initial.diff_text.empty());
    EXPECT_FALSE(initial.workspace_modified);

    fs::remove_all(temp_root);
}

TEST(DiffReporterTest, CapturesDiffAfterBaseline)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path workspace_repo = temp_root / "runs" / "task" / "repo";
    WriteTextFile(workspace_repo / "LibraryApp" / "Book.cpp", "int book = 1;\n");

    const auto initial = CaptureWorkspaceDiff(workspace_repo);
    ASSERT_TRUE(initial.success) << initial.warnings.front();

    WriteTextFile(workspace_repo / "LibraryApp" / "Book.cpp", "int book = 2;\n");
    const auto changed = CaptureWorkspaceDiff(workspace_repo);

    EXPECT_TRUE(changed.success) << changed.warnings.front();
    EXPECT_EQ(fs::weakly_canonical(changed.git_top_level), fs::weakly_canonical(workspace_repo));
    EXPECT_NE(changed.diff_text.find("-int book = 1;"), std::string::npos);
    EXPECT_NE(changed.diff_text.find("+int book = 2;"), std::string::npos);
    EXPECT_TRUE(changed.workspace_modified);

    fs::remove_all(temp_root);
}

TEST(DiffReporterTest, MissingGitReturnsClearWarning)
{
    const fs::path temp_root = MakeUniqueTempRoot();
    const fs::path workspace_repo = temp_root / "repo";
    WriteTextFile(workspace_repo / "LibraryApp" / "Book.cpp", "int book = 1;\n");
    MissingGitRunner missing_git;

    const auto report = CaptureWorkspaceDiff(workspace_repo, missing_git);

    EXPECT_FALSE(report.success);
    ASSERT_FALSE(report.warnings.empty());
    EXPECT_NE(report.warnings.front().find("Git baseline failed"), std::string::npos);
    EXPECT_NE(report.warnings.front().find("git executable was not found"), std::string::npos);

    fs::remove_all(temp_root);
}
