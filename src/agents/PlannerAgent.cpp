#include "agents/PlannerAgent.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace agentguard
{
namespace
{
void AddUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

std::unordered_set<std::string> PathSet(const std::vector<SemanticFileReference>& files)
{
    std::unordered_set<std::string> paths;
    for (const auto& file : files)
    {
        paths.insert(file.path);
    }
    return paths;
}

std::vector<std::string> PathsFrom(const std::vector<SemanticFileReference>& files)
{
    std::vector<std::string> paths;
    paths.reserve(files.size());
    for (const auto& file : files)
    {
        AddUnique(paths, file.path);
    }
    return paths;
}

std::string BuildPlannerPrompt(const PlannerInput& input)
{
    std::ostringstream prompt;
    prompt << "You are planning a constrained Visual Studio C++ coding task.\n";
    prompt << "User request: " << input.user_request << "\n";
    prompt << "Target solution: " << input.project_info.solution_path << "\n";
    prompt << "Only suggest files related to the task.\n";
    prompt << "Return strict JSON with task_id, user_request, target_solution, target_project, ";
    prompt << "allowed_files, forbidden_files, acceptance_criteria, max_repair_rounds.\n";
    prompt << "The original project must never be modified directly.\n";
    prompt << "Do not bypass PatchApplier; it is the only write authority.\n";
    if (input.has_semantic_scope)
    {
        prompt << "Semantic scope is available. Use semantic allowed_files as direct write scope, ";
        prompt << "map protected_files to forbidden_files, keep context/suspected/needs_approval separate.\n";
    }
    prompt << "Context candidates:\n";
    for (const auto& candidate : input.context_candidates)
    {
        prompt << "- " << candidate.file_path << " score=" << candidate.score << "\n";
    }
    return prompt.str();
}

bool IsTaskSpecValid(const TaskSpec& task_spec, std::string& error_message)
{
    if (task_spec.task_id.empty())
    {
        error_message = "TaskSpec.task_id cannot be empty.";
        return false;
    }
    if (task_spec.user_request.empty())
    {
        error_message = "TaskSpec.user_request cannot be empty.";
        return false;
    }
    if (task_spec.target_solution.empty())
    {
        error_message = "TaskSpec.target_solution cannot be empty.";
        return false;
    }
    if (task_spec.max_repair_rounds < 0)
    {
        error_message = "TaskSpec.max_repair_rounds cannot be negative.";
        return false;
    }

    return true;
}

void ApplySemanticScope(TaskSpec& task_spec, const PlannerInput& input)
{
    if (!input.has_semantic_scope)
    {
        return;
    }

    const auto protected_paths = PathSet(input.semantic_scope.protected_files);
    const auto needs_approval_paths = PathSet(input.semantic_scope.needs_approval_files);

    task_spec.allowed_files.clear();
    task_spec.suspected_files.clear();

    for (const auto& file : input.semantic_scope.allowed_files)
    {
        if (protected_paths.contains(file.path) || needs_approval_paths.contains(file.path))
        {
            continue;
        }
        if (file.confidence < input.semantic_allowed_confidence_threshold)
        {
            AddUnique(task_spec.suspected_files, file.path);
            continue;
        }
        AddUnique(task_spec.allowed_files, file.path);
    }

    for (const auto& file : input.semantic_scope.suspected_files)
    {
        if (!protected_paths.contains(file.path) && !needs_approval_paths.contains(file.path))
        {
            AddUnique(task_spec.suspected_files, file.path);
        }
    }

    task_spec.context_files = PathsFrom(input.semantic_scope.context_files);
    task_spec.protected_files = PathsFrom(input.semantic_scope.protected_files);
    task_spec.needs_approval_files = PathsFrom(input.semantic_scope.needs_approval_files);

    for (const auto& path : task_spec.protected_files)
    {
        AddUnique(task_spec.forbidden_files, path);
    }

    task_spec.has_semantic_scope = true;
    task_spec.semantic_scope = input.semantic_scope;
}
} // namespace

PlannerResult TryPlanTask(
    const PlannerInput& input,
    const ILLMProvider& provider)
{
    PlannerResult result;
    result.prompt = BuildPlannerPrompt(input);

    try
    {
        const nlohmann::json json_value = provider.GenerateJson(result.prompt);
        result.task_spec = json_value.get<TaskSpec>();
        if (!IsTaskSpecValid(result.task_spec, result.error_message))
        {
            return result;
        }
        ApplySemanticScope(result.task_spec, input);
    }
    catch (const std::exception& exception)
    {
        result.error_message = exception.what();
        return result;
    }

    result.success = true;
    return result;
}
} // namespace agentguard
