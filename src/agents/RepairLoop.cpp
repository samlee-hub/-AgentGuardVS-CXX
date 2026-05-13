#include "agents/RepairLoop.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "core/PathUtils.h"
#include "diagnostics/ErrorParser.h"
#include "patching/PatchApplier.h"

namespace agentguard
{
namespace
{
void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    EnsureDirectory(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Unable to write file: " + path.string());
    }
    output << content;
}

std::string ReadTextFileIfPresent(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void SavePatchPlan(
    const PatchPlan& patch_plan,
    const std::filesystem::path& reports_directory,
    int round_index)
{
    const nlohmann::json json_value = patch_plan;
    WriteTextFile(
        reports_directory / ("patch_round_" + std::to_string(round_index) + ".json"),
        json_value.dump(2));
}

void SaveBuildLog(
    const BuildResult& build_result,
    const std::filesystem::path& logs_directory,
    int round_index)
{
    std::string log;
    log += "Command: " + build_result.command + "\n";
    log += "Return code: " + std::to_string(build_result.return_code) + "\n";
    log += "Stdout:\n" + build_result.stdout_text + "\n";
    log += "Stderr:\n" + build_result.stderr_text + "\n";
    WriteTextFile(logs_directory / ("build_round_" + std::to_string(round_index) + ".log"), log);
}

std::vector<RelevantFileContent> RefreshRelevantFiles(
    const std::filesystem::path& repo_root,
    const std::vector<RelevantFileContent>& known_files)
{
    std::vector<RelevantFileContent> refreshed;
    refreshed.reserve(known_files.size());
    for (const auto& file : known_files)
    {
        const auto relative_path = SafeRelativePath(std::filesystem::path(file.relative_path));
        refreshed.push_back(RelevantFileContent{
            relative_path.generic_string(),
            ReadTextFileIfPresent(repo_root / relative_path)
        });
    }
    return refreshed;
}

std::vector<std::string> RelatedFilePaths(const std::vector<RelevantFileContent>& files)
{
    std::vector<std::string> paths;
    paths.reserve(files.size());
    for (const auto& file : files)
    {
        paths.push_back(file.relative_path);
    }
    return paths;
}

RepairLoopResult Fail(const TaskSpec& spec, std::string message)
{
    RepairLoopResult result;
    result.error_message = std::move(message);
    result.report.task_spec = spec;
    result.report.success = false;
    return result;
}
} // namespace

RepairLoopResult RunRepairLoop(const RepairLoopInput& input)
{
    if (input.planner_provider == nullptr || input.implementer_provider == nullptr
        || input.build_fixer_provider == nullptr)
    {
        return Fail(TaskSpec{}, "RepairLoop requires planner, implementer, and build fixer providers.");
    }
    if (!input.build_executor)
    {
        return Fail(TaskSpec{}, "RepairLoop requires a build executor.");
    }

    try
    {
        EnsureDirectory(input.logs_directory);
        EnsureDirectory(input.reports_directory);
    }
    catch (const std::exception& exception)
    {
        return Fail(TaskSpec{}, exception.what());
    }

    const PlannerResult planner_result = TryPlanTask(input.planner_input, *input.planner_provider);
    if (!planner_result.success)
    {
        return Fail(TaskSpec{}, planner_result.error_message);
    }

    const TaskSpec task_spec = planner_result.task_spec;

    ImplementerInput implementer_input;
    implementer_input.task_spec = task_spec;
    implementer_input.project_info = input.planner_input.project_info;
    implementer_input.relevant_files = input.initial_relevant_files;

    const ImplementerResult implementer_result =
        TryCreatePatchPlan(implementer_input, *input.implementer_provider);
    if (!implementer_result.success)
    {
        return Fail(task_spec, implementer_result.error_message);
    }

    PatchPlan current_patch_plan = implementer_result.patch_plan;
    BuildResult last_build_result;
    int completed_repair_rounds = 0;

    for (int round_index = 0; round_index <= task_spec.max_repair_rounds; ++round_index)
    {
        SavePatchPlan(current_patch_plan, input.reports_directory, round_index);

        const PatchApplyResult patch_apply_result =
            ApplyPatchPlan(current_patch_plan, task_spec, input.repo_root);
        if (!patch_apply_result.success)
        {
            std::string message = "Patch application failed.";
            if (!patch_apply_result.errors.empty())
            {
                message += " " + patch_apply_result.errors.front();
            }
            return Fail(task_spec, message);
        }

        last_build_result = input.build_executor(task_spec, round_index);
        last_build_result.parsed_errors = ParseBuildErrors(last_build_result);
        SaveBuildLog(last_build_result, input.logs_directory, round_index);

        if (last_build_result.success)
        {
            RepairLoopResult result;
            result.success = true;
            result.report.success = true;
            result.report.task_spec = task_spec;
            result.report.build_result = last_build_result;
            result.report.repair_rounds = completed_repair_rounds;
            result.report.related_files = RelatedFilePaths(input.initial_relevant_files);
            return result;
        }

        if (round_index >= task_spec.max_repair_rounds)
        {
            break;
        }

        BuildFixerInput fixer_input;
        fixer_input.task_spec = task_spec;
        fixer_input.build_result = last_build_result;
        fixer_input.parsed_errors = last_build_result.parsed_errors;
        fixer_input.current_files = RefreshRelevantFiles(input.repo_root, input.initial_relevant_files);
        fixer_input.git_diff_summary = input.git_diff_summary;

        const BuildFixerResult fixer_result =
            TryCreateBuildFixPatchPlan(fixer_input, *input.build_fixer_provider);
        if (!fixer_result.success)
        {
            return Fail(task_spec, fixer_result.error_message);
        }

        current_patch_plan = fixer_result.patch_plan;
        ++completed_repair_rounds;
    }

    RepairLoopResult result;
    result.error_message = "Build did not succeed before max_repair_rounds was exhausted.";
    result.report.success = false;
    result.report.task_spec = task_spec;
    result.report.build_result = last_build_result;
    result.report.repair_rounds = completed_repair_rounds;
    result.report.related_files = RelatedFilePaths(input.initial_relevant_files);
    return result;
}
} // namespace agentguard
