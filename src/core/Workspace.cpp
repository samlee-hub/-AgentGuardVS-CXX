#include "core/Workspace.h"

#include <array>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <nlohmann/json.hpp>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
constexpr std::array<std::string_view, 10> kExcludedDirectories{
    ".git",
    ".vs",
    "x64",
    "Debug",
    "Release",
    "build",
    "out",
    "Binaries",
    "Intermediate",
    "Saved"
};

constexpr std::string_view kDiffBase = "agentguard baseline";

std::wstring ResolveGitExecutable()
{
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    const DWORD result = SearchPathW(nullptr, L"git.exe", nullptr, MAX_PATH, buffer, nullptr);
    if (result > 0 && result < MAX_PATH)
    {
        return buffer;
    }
#endif
    return L"git.exe";
}

bool IsExcludedDirectory(const std::filesystem::path& path)
{
    const auto directory_name = path.filename().string();
    for (const auto excluded : kExcludedDirectories)
    {
        if (directory_name == excluded)
        {
            return true;
        }
    }

    return false;
}

void CopyProjectTree(const std::filesystem::path& source_root, const std::filesystem::path& repo_root)
{
    for (std::filesystem::recursive_directory_iterator it(source_root), end; it != end; ++it)
    {
        const auto& source_path = it->path();

        if (it->is_directory() && IsExcludedDirectory(source_path))
        {
            it.disable_recursion_pending();
            continue;
        }

        const auto relative_path = std::filesystem::relative(source_path, source_root);
        const auto destination_path = repo_root / relative_path;

        if (it->is_directory())
        {
            EnsureDirectory(destination_path);
            continue;
        }

        if (it->is_regular_file())
        {
            EnsureDirectory(destination_path.parent_path());
            std::filesystem::copy_file(
                source_path,
                destination_path,
                std::filesystem::copy_options::overwrite_existing);
        }
    }
}

ProcessResult RunGitCommand(
    const IProcessRunner& runner,
    const std::filesystem::path& repo_root,
    std::vector<std::wstring> arguments)
{
    ProcessRequest request;
    request.executable = ResolveGitExecutable();
    request.arguments = std::move(arguments);
    request.working_directory = repo_root;
    return runner.Run(request);
}

std::string GitFailureMessage(const std::string& step, const ProcessResult& result)
{
    std::string message = "Git baseline failed during " + step + ".";
    if (!result.stderr_text.empty())
    {
        message += " " + result.stderr_text;
    }
    else if (!result.stdout_text.empty())
    {
        message += " " + result.stdout_text;
    }
    else
    {
        message += " git returned code " + std::to_string(result.return_code) + ".";
    }
    return message;
}

void EnsureGitBaseline(
    const std::filesystem::path& repo_root,
    const IProcessRunner& runner)
{
    try
    {
        const auto init = RunGitCommand(runner, repo_root, {L"init"});
        if (init.return_code != 0)
        {
            throw std::runtime_error(GitFailureMessage("git init", init));
        }

        const auto add = RunGitCommand(runner, repo_root, {L"add", L"."});
        if (add.return_code != 0)
        {
            throw std::runtime_error(GitFailureMessage("git add", add));
        }

        const auto commit = RunGitCommand(
            runner,
            repo_root,
            {
                L"-c",
                L"user.email=agentguard@example.invalid",
                L"-c",
                L"user.name=AgentGuardVS",
                L"commit",
                L"--allow-empty",
                L"-m",
                L"agentguard baseline"
            });
        if (commit.return_code != 0)
        {
            throw std::runtime_error(GitFailureMessage("git commit", commit));
        }
    }
    catch (const std::runtime_error&)
    {
        throw;
    }
    catch (const std::exception& exception)
    {
        throw std::runtime_error(std::string("Git baseline failed. ") + exception.what());
    }
}
} // namespace

RunWorkspace::RunWorkspace(WorkspaceOptions options)
    : options_(std::move(options))
{
}

WorkspacePaths RunWorkspace::Prepare() const
{
    if (!std::filesystem::exists(options_.source_project_path) ||
        !std::filesystem::is_directory(options_.source_project_path))
    {
        throw std::invalid_argument("Source project path must be an existing directory.");
    }

    const auto safe_task_id = SafeRelativePath(options_.task_id);
    if (safe_task_id.has_parent_path())
    {
        throw std::invalid_argument("Task id must resolve to a single workspace directory.");
    }

    const auto runs_root = NormalizePath(options_.runs_root);
    EnsureDirectory(runs_root);

    const auto task_root = NormalizePath(runs_root / safe_task_id);
    if (!IsSubPath(task_root, runs_root))
    {
        throw std::invalid_argument("Task root escaped the configured runs directory.");
    }

    if (std::filesystem::exists(task_root))
    {
        if (!options_.force)
        {
            throw std::runtime_error("Workspace already exists and force is false.");
        }

        if (!IsSubPath(task_root, runs_root))
        {
            throw std::runtime_error("Refusing to remove a workspace outside runs_root.");
        }

        std::filesystem::remove_all(task_root);
    }

    WorkspacePaths paths;
    paths.task_root = task_root;
    paths.repo_root = task_root / "repo";
    paths.logs_root = task_root / "logs";
    paths.reports_root = task_root / "reports";
    paths.metadata_path = task_root / "metadata.json";
    paths.source_project_path = NormalizePath(options_.source_project_path);
    paths.diff_base = std::string(kDiffBase);

    EnsureDirectory(paths.repo_root);
    EnsureDirectory(paths.logs_root);
    EnsureDirectory(paths.reports_root);

    CopyProjectTree(NormalizePath(options_.source_project_path), paths.repo_root);
    const ProcessRunner default_runner;
    const IProcessRunner& git_runner = options_.git_runner == nullptr ? default_runner : *options_.git_runner;
    EnsureGitBaseline(paths.repo_root, git_runner);

    const nlohmann::json metadata{
        {"task_id", safe_task_id.generic_string()},
        {"source_project_path", NormalizePath(options_.source_project_path).generic_string()},
        {"runs_root", runs_root.generic_string()},
        {"repo_root", NormalizePath(paths.repo_root).generic_string()},
        {"logs_root", NormalizePath(paths.logs_root).generic_string()},
        {"reports_root", NormalizePath(paths.reports_root).generic_string()},
        {"diff_base", std::string(kDiffBase)}
    };

    std::ofstream metadata_file(paths.metadata_path);
    metadata_file << metadata.dump(2);

    return paths;
}
} // namespace agentguard
