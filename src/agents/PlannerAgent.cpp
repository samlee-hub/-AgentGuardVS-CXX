#include "agents/PlannerAgent.h"

#include <sstream>
#include <stdexcept>

namespace agentguard
{
namespace
{
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
