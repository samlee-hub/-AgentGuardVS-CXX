#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/ExtraValidators.hpp>
#include <CLI/Formatter.hpp>
#include <nlohmann/json.hpp>

#include "agents/RepairLoop.h"
#include "core/PathUtils.h"
#include "core/Workspace.h"
#include "indexing/ContextSelector.h"
#include "indexing/CppSymbolIndexer.h"
#include "indexing/FileIndexer.h"
#include "llm/FakeLLMProvider.h"
#include "report/ReportWriter.h"
#include "vs/MSBuildRunner.h"
#include "vs/SlnParser.h"
#include "vs/VcxprojParser.h"

namespace
{
namespace fs = std::filesystem;

struct RunOptions
{
    fs::path solution;
    std::string task;
    std::string configuration = "Debug";
    std::string platform = "x64";
    fs::path runs_root = "runs";
    bool dry_run = false;
    bool no_llm = false;
    bool force = false;
};

#ifdef _WIN32
std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_size <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }

    std::string result(static_cast<std::size_t>(required_size), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        required_size,
        nullptr,
        nullptr);
    if (converted != required_size)
    {
        throw std::runtime_error("WideCharToMultiByte produced an unexpected byte count.");
    }

    return result;
}
#endif

std::string SanitizeTaskIdPart(std::string value)
{
    for (char& ch : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_')
        {
            ch = '-';
        }
    }
    value.erase(std::unique(value.begin(), value.end(), [](char left, char right) {
        return left == '-' && right == '-';
    }), value.end());
    if (value.empty())
    {
        return "task";
    }
    return value.substr(0, 32);
}

std::string MakeTaskId(const fs::path& solution, const std::string& task)
{
    const auto hash_value = std::hash<std::string>{}(solution.string() + "|" + task);
    std::ostringstream stream;
    stream << SanitizeTaskIdPart(solution.stem().string()) << "-" << std::hex << (hash_value & 0xfffffff);
    return stream.str();
}

std::string ReadTextIfPresent(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::string> SelectAllowedFiles(
    const std::vector<agentguard::ContextCandidate>& candidates,
    const std::vector<agentguard::SourceFileInfo>& files)
{
    std::vector<std::string> allowed;
    for (const auto& candidate : candidates)
    {
        allowed.push_back(fs::path(candidate.file_path).generic_string());
        if (allowed.size() >= 5)
        {
            break;
        }
    }

    if (allowed.empty() && !files.empty())
    {
        allowed.push_back(fs::path(files.front().file_path).generic_string());
    }

    return allowed;
}

std::vector<std::string> BuildForbiddenFiles(
    const std::vector<agentguard::SourceFileInfo>& files,
    const std::vector<std::string>& allowed_files)
{
    std::unordered_set<std::string> allowed_set(allowed_files.begin(), allowed_files.end());
    std::vector<std::string> forbidden;
    for (const auto& file : files)
    {
        const auto normalized = fs::path(file.file_path).generic_string();
        if (!allowed_set.contains(normalized))
        {
            forbidden.push_back(normalized);
        }
        if (forbidden.size() >= 20)
        {
            break;
        }
    }
    return forbidden;
}

std::vector<agentguard::RelevantFileContent> LoadRelevantFileContents(
    const fs::path& repo_root,
    const std::vector<std::string>& relative_paths)
{
    std::vector<agentguard::RelevantFileContent> contents;
    for (const auto& relative_path : relative_paths)
    {
        const auto safe_path = agentguard::SafeRelativePath(fs::path(relative_path));
        contents.push_back(agentguard::RelevantFileContent{
            safe_path.generic_string(),
            ReadTextIfPresent(repo_root / safe_path)
        });
    }
    return contents;
}

nlohmann::json MakePlannerJson(
    const std::string& task_id,
    const RunOptions& options,
    const std::string& target_project,
    const std::vector<std::string>& allowed_files,
    const std::vector<std::string>& forbidden_files)
{
    return nlohmann::json{
        {"task_id", task_id},
        {"user_request", options.task},
        {"target_solution", options.solution.string()},
        {"target_project", target_project},
        {"allowed_files", allowed_files},
        {"forbidden_files", forbidden_files},
        {"acceptance_criteria", nlohmann::json::array({
            "All edits stay inside the isolated workspace.",
            "Build verification is used before completion.",
            "PatchApplier remains the only file mutation path."
        })},
        {"max_repair_rounds", 3}
    };
}

nlohmann::json MakeNoOpPatchPlanJson(const std::vector<agentguard::RelevantFileContent>& files)
{
    if (files.empty())
    {
        return nlohmann::json{{"changes", nlohmann::json::array()}};
    }

    const auto& file = files.front();
    return nlohmann::json{
        {"changes", nlohmann::json::array({
            {
                {"target_file", file.relative_path},
                {"change_type", "modify"},
                {"original_snippet", file.content},
                {"replacement_snippet", file.content},
                {"explanation", "No-op patch generated by --no-llm mode."}
            }
        })}
    };
}

void WriteDryRunReport(
    const fs::path& path,
    const agentguard::TaskSpec& task_spec,
    const std::vector<agentguard::ContextCandidate>& candidates,
    const std::vector<agentguard::SourceFileInfo>& source_files,
    const std::vector<agentguard::VSProjectInfo>& projects)
{
    std::ofstream output(path, std::ios::trunc);
    output << "# Dry Run Report\n\n";
    output << "## TaskSpec\n\n";
    output << nlohmann::json(task_spec).dump(2) << "\n\n";
    output << "## Projects\n\n";
    for (const auto& project : projects)
    {
        output << "- " << project.project_name << ": " << project.project_path << "\n";
    }
    output << "\n## Source Files\n\n";
    output << "- Count: " << source_files.size() << "\n";
    output << "\n## Related Files\n\n";
    if (candidates.empty())
    {
        output << "- None\n";
    }
    for (const auto& candidate : candidates)
    {
        output << "- " << candidate.file_path << " score=" << candidate.score << "\n";
    }
}

int RunCommand(const RunOptions& options)
{
    if (!options.no_llm)
    {
        std::cerr << "Only --no-llm mode is implemented in this stage.\n";
        return 2;
    }
    if (!fs::exists(options.solution) || options.solution.extension() != ".sln")
    {
        std::cerr << "--solution must point to an existing .sln file.\n";
        return 2;
    }

    const fs::path source_root = fs::absolute(options.solution).parent_path();
    const std::string task_id = MakeTaskId(options.solution, options.task);

    const agentguard::WorkspacePaths workspace = agentguard::RunWorkspace(agentguard::WorkspaceOptions{
        source_root,
        options.runs_root,
        task_id,
        options.force
    }).Prepare();

    const fs::path workspace_solution = workspace.repo_root / options.solution.filename();
    const agentguard::SlnParseResult sln_result = agentguard::ParseSln(workspace_solution);

    agentguard::ProjectInfo project_info;
    project_info.solution_name = workspace_solution.stem().string();
    project_info.solution_path = workspace_solution.string();

    for (const auto& sln_project : sln_result.projects)
    {
        auto parsed_project = agentguard::ParseVcxproj(workspace.repo_root / fs::path(sln_project.project_path));
        parsed_project.project_guid = sln_project.project_guid;
        parsed_project.project_type_guid = sln_project.project_type_guid;
        project_info.projects.push_back(std::move(parsed_project));
    }

    const auto source_files = agentguard::IndexSourceFiles(workspace.repo_root);
    std::vector<agentguard::SourceFileInfo> symbol_files;
    symbol_files.reserve(source_files.size());
    for (const auto& source_file : source_files)
    {
        symbol_files.push_back(agentguard::IndexCppSymbols(workspace.repo_root / source_file.file_path, workspace.repo_root));
    }

    const auto candidates = agentguard::SelectRelevantFiles(options.task, symbol_files);
    const auto allowed_files = SelectAllowedFiles(candidates, symbol_files);
    const auto forbidden_files = BuildForbiddenFiles(symbol_files, allowed_files);
    const std::string target_project = project_info.projects.empty() ? "" : project_info.projects.front().project_name;

    agentguard::FakeLLMProvider planner_provider(
        "",
        MakePlannerJson(task_id, options, target_project, allowed_files, forbidden_files));

    agentguard::PlannerInput planner_input;
    planner_input.user_request = options.task;
    planner_input.project_info = project_info;
    planner_input.source_files = symbol_files;
    planner_input.context_candidates = candidates;

    const auto planner_result = agentguard::TryPlanTask(planner_input, planner_provider);
    if (!planner_result.success)
    {
        std::cerr << planner_result.error_message << "\n";
        return 1;
    }

    const auto relevant_contents = LoadRelevantFileContents(workspace.repo_root, planner_result.task_spec.allowed_files);

    if (options.dry_run)
    {
        const fs::path dry_run_report = workspace.reports_root / "dry_run_report.md";
        WriteDryRunReport(dry_run_report, planner_result.task_spec, candidates, symbol_files, project_info.projects);

        agentguard::ReportWriteRequest report_request;
        report_request.reports_directory = workspace.reports_root;
        report_request.task_spec = planner_result.task_spec;
        report_request.related_files = planner_result.task_spec.allowed_files;
        report_request.risk_warnings = {"Dry-run only: no patch was applied and MSBuild was not invoked."};
        agentguard::WriteReport(report_request);

        std::cout << "Dry run completed\n";
        std::cout << "Workspace: " << workspace.task_root.string() << "\n";
        std::cout << "Report: " << dry_run_report.string() << "\n";
        std::cout << "TaskSpec:\n" << nlohmann::json(planner_result.task_spec).dump(2) << "\n";
        std::cout << "Related files:\n";
        for (const auto& file : planner_result.task_spec.allowed_files)
        {
            std::cout << "- " << file << "\n";
        }
        return 0;
    }

    agentguard::FakeLLMProvider implementer_provider("", MakeNoOpPatchPlanJson(relevant_contents));
    agentguard::FakeLLMProvider fixer_provider("", MakeNoOpPatchPlanJson(relevant_contents));

    agentguard::RepairLoopInput repair_input;
    repair_input.repo_root = workspace.repo_root;
    repair_input.logs_directory = workspace.logs_root;
    repair_input.reports_directory = workspace.reports_root;
    repair_input.planner_input = planner_input;
    repair_input.initial_relevant_files = relevant_contents;
    repair_input.git_diff_summary = "";
    repair_input.planner_provider = &planner_provider;
    repair_input.implementer_provider = &implementer_provider;
    repair_input.build_fixer_provider = &fixer_provider;
    repair_input.build_executor = [&](const agentguard::TaskSpec&, int) {
        return agentguard::BuildSolution(workspace_solution, options.configuration, options.platform);
    };

    const auto repair_result = agentguard::RunRepairLoop(repair_input);

    agentguard::ReportWriteRequest report_request;
    report_request.reports_directory = workspace.reports_root;
    report_request.task_spec = repair_result.report.task_spec;
    report_request.related_files = repair_result.report.related_files;
    report_request.build_result = repair_result.report.build_result;
    report_request.repair_rounds = repair_result.report.repair_rounds;
    report_request.risk_warnings = repair_result.success
        ? std::vector<std::string>{}
        : std::vector<std::string>{repair_result.error_message};
    const auto report = agentguard::WriteReport(report_request);

    std::cout << "Run completed\n";
    std::cout << "Workspace: " << workspace.task_root.string() << "\n";
    std::cout << "Report: " << report.markdown_path.string() << "\n";
    return repair_result.success ? 0 : 1;
}
} // namespace

int AppMain(int argc, char** argv)
{
    CLI::App app{"AgentGuardVS-CXX"};
    app.set_version_flag("--version", "AgentGuardVS-CXX 0.1.0");

    RunOptions run_options;
    auto* run = app.add_subcommand("run", "Run an isolated AgentGuardVS-CXX task.");
    run->add_option("--solution", run_options.solution, "Path to the target .sln file.")->required();
    run->add_option("--task", run_options.task, "User task description.")->required();
    run->add_option("--configuration", run_options.configuration, "MSBuild configuration.")->default_val("Debug");
    run->add_option("--platform", run_options.platform, "MSBuild platform.")->default_val("x64");
    run->add_option("--runs-root", run_options.runs_root, "Root directory for isolated runs.")->default_val("runs");
    run->add_flag("--dry-run", run_options.dry_run, "Generate TaskSpec and context without applying patches.");
    run->add_flag("--no-llm", run_options.no_llm, "Use deterministic FakeLLMProvider output.");
    run->add_flag("--force", run_options.force, "Overwrite an existing task workspace.");

    CLI11_PARSE(app, argc, argv);

    try
    {
        if (*run)
        {
            return RunCommand(run_options);
        }

        std::cout << app.help();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    try
    {
        std::vector<std::string> utf8_args;
        utf8_args.reserve(static_cast<std::size_t>(argc));
        for (int index = 0; index < argc; ++index)
        {
            utf8_args.push_back(WideToUtf8(argv[index]));
        }

        std::vector<char*> utf8_argv;
        utf8_argv.reserve(utf8_args.size());
        for (auto& arg : utf8_args)
        {
            utf8_argv.push_back(arg.data());
        }

        return AppMain(static_cast<int>(utf8_argv.size()), utf8_argv.data());
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
#else
int main(int argc, char** argv)
{
    return AppMain(argc, argv);
}
#endif
