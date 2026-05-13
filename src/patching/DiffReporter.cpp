#include "patching/DiffReporter.h"

#include <filesystem>
#include <string>

namespace agentguard
{
namespace
{
namespace fs = std::filesystem;

ProcessRequest MakeGitRequest(
    const fs::path& repo_root,
    std::vector<std::wstring> arguments)
{
    ProcessRequest request;
    request.executable = L"git";
    request.arguments = std::move(arguments);
    request.working_directory = repo_root;
    return request;
}

bool IsGitRepository(const fs::path& repo_root, const IProcessRunner& runner)
{
    const ProcessResult result = runner.Run(
        MakeGitRequest(repo_root, {L"rev-parse", L"--is-inside-work-tree"}));
    return result.return_code == 0;
}

bool RunGitCommand(
    const fs::path& repo_root,
    const IProcessRunner& runner,
    std::vector<std::wstring> arguments,
    std::vector<std::string>& warnings)
{
    const ProcessResult result = runner.Run(MakeGitRequest(repo_root, std::move(arguments)));
    if (result.return_code == 0)
    {
        return true;
    }

    if (!result.stderr_text.empty())
    {
        warnings.push_back(result.stderr_text);
    }
    else
    {
        warnings.push_back("Git command failed.");
    }
    return false;
}
} // namespace

DiffReport CaptureWorkspaceDiff(const fs::path& repo_root)
{
    const ProcessRunner runner;
    return CaptureWorkspaceDiff(repo_root, runner);
}

DiffReport CaptureWorkspaceDiff(
    const fs::path& repo_root,
    const IProcessRunner& process_runner)
{
    DiffReport report;

    if (!fs::exists(repo_root) || !fs::is_directory(repo_root))
    {
        report.warnings.push_back("Workspace repo directory does not exist.");
        return report;
    }

    if (!IsGitRepository(repo_root, process_runner))
    {
        const bool initialized = RunGitCommand(repo_root, process_runner, {L"init"}, report.warnings);
        const bool added = initialized
            && RunGitCommand(repo_root, process_runner, {L"add", L"."}, report.warnings);
        const bool committed = added
            && RunGitCommand(
                repo_root,
                process_runner,
                {
                    L"-c",
                    L"user.email=agentguard@example.invalid",
                    L"-c",
                    L"user.name=AgentGuardVS",
                    L"commit",
                    L"-m",
                    L"baseline"
                },
                report.warnings);

        if (!committed)
        {
            return report;
        }
    }

    const ProcessResult diff = process_runner.Run(
        MakeGitRequest(repo_root, {L"diff"}));
    const ProcessResult diff_stat = process_runner.Run(
        MakeGitRequest(repo_root, {L"diff", L"--stat"}));

    report.diff_text = diff.stdout_text;
    report.diff_stat = diff_stat.stdout_text;
    report.success = diff.return_code == 0 && diff_stat.return_code == 0;

    if (!report.success)
    {
        if (!diff.stderr_text.empty())
        {
            report.warnings.push_back(diff.stderr_text);
        }
        if (!diff_stat.stderr_text.empty())
        {
            report.warnings.push_back(diff_stat.stderr_text);
        }
    }

    return report;
}
} // namespace agentguard
