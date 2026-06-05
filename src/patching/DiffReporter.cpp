#include "patching/DiffReporter.h"

#include <filesystem>
#include <sstream>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace agentguard
{
namespace
{
namespace fs = std::filesystem;

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

ProcessRequest MakeGitRequest(
    const fs::path& repo_root,
    std::vector<std::wstring> arguments)
{
    ProcessRequest request;
    request.executable = ResolveGitExecutable();
    request.arguments = std::move(arguments);
    request.working_directory = repo_root;
    return request;
}

std::string TrimTrailingWhitespace(std::string value)
{
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t'))
    {
        value.pop_back();
    }
    return value;
}

std::string FailureText(const std::string& step, const ProcessResult& result)
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

bool EnsureBaseline(
    const fs::path& repo_root,
    const IProcessRunner& runner,
    std::vector<std::string>& warnings)
{
    const bool has_own_git = fs::exists(repo_root / ".git");
    if (has_own_git)
    {
        return true;
    }

    const ProcessResult init = runner.Run(MakeGitRequest(repo_root, {L"init"}));
    if (init.return_code != 0)
    {
        warnings.push_back(FailureText("git init", init));
        return false;
    }

    const ProcessResult add = runner.Run(MakeGitRequest(repo_root, {L"add", L"."}));
    if (add.return_code != 0)
    {
        warnings.push_back(FailureText("git add", add));
        return false;
    }

    const ProcessResult commit = runner.Run(
        MakeGitRequest(
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
            }));
    if (commit.return_code != 0)
    {
        warnings.push_back(FailureText("git commit", commit));
        return false;
    }

    return true;
}

bool ResolveAndValidateTopLevel(
    const fs::path& repo_root,
    const IProcessRunner& runner,
    DiffReport& report)
{
    const ProcessResult top_level = runner.Run(
        MakeGitRequest(repo_root, {L"rev-parse", L"--show-toplevel"}));
    if (top_level.return_code != 0)
    {
        report.warnings.push_back(FailureText("git rev-parse", top_level));
        return false;
    }

    report.git_top_level = fs::weakly_canonical(fs::path(TrimTrailingWhitespace(top_level.stdout_text)));
    const auto expected_top_level = fs::weakly_canonical(repo_root);
    if (report.git_top_level != expected_top_level)
    {
        std::ostringstream message;
        message << "Refusing to capture diff from parent Git repository. Expected top-level "
                << expected_top_level.string() << " but got " << report.git_top_level.string() << ".";
        report.warnings.push_back(message.str());
        return false;
    }

    return true;
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

    const fs::path normalized_repo_root = fs::weakly_canonical(repo_root);
    report.diff_base = "agentguard baseline";

    try
    {
        if (!EnsureBaseline(normalized_repo_root, process_runner, report.warnings))
        {
            return report;
        }
        if (!ResolveAndValidateTopLevel(normalized_repo_root, process_runner, report))
        {
            return report;
        }
    }
    catch (const std::exception& exception)
    {
        report.warnings.push_back(std::string("Git diff capture failed. ") + exception.what());
        return report;
    }

    const ProcessResult diff = process_runner.Run(
        MakeGitRequest(normalized_repo_root, {L"diff"}));
    const ProcessResult diff_stat = process_runner.Run(
        MakeGitRequest(normalized_repo_root, {L"diff", L"--stat"}));

    report.diff_text = diff.stdout_text;
    report.diff_stat = diff_stat.stdout_text;
    report.workspace_modified = !report.diff_text.empty() || !report.diff_stat.empty();
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
