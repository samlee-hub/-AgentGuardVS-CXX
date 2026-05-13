#include "agents/ImplementerAgent.h"

#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
std::string BuildImplementerPrompt(const ImplementerInput& input)
{
    std::ostringstream prompt;
    prompt << "Create a PatchPlan for a constrained Visual Studio C++ task.\n";
    prompt << "Return strict JSON only. Do not emit prose.\n";
    prompt << "PatchPlan schema: {\"changes\":[{\"target_file\",\"change_type\",\"original_snippet\",";
    prompt << "\"replacement_snippet\",\"explanation\"}]}.\n";
    prompt << "Every target_file must be relative to the isolated workspace repo.\n";
    prompt << "Task: " << input.task_spec.user_request << "\n";
    prompt << "Allowed files:\n";
    for (const auto& file : input.task_spec.allowed_files)
    {
        prompt << "- " << file << "\n";
    }
    prompt << "Relevant file contents:\n";
    for (const auto& file : input.relevant_files)
    {
        prompt << "FILE " << file.relative_path << "\n";
        prompt << file.content << "\n";
    }
    return prompt.str();
}

bool IsSupportedChangeType(const std::string& change_type)
{
    return change_type == "modify" || change_type == "create" || change_type == "delete";
}

bool ValidatePatchPlan(PatchPlan& plan, std::string& error_message)
{
    if (plan.changes.empty())
    {
        error_message = "PatchPlan must include at least one change.";
        return false;
    }

    try
    {
        for (auto& change : plan.changes)
        {
            if (change.target_file.empty())
            {
                error_message = "PatchPlan.target_file cannot be empty.";
                return false;
            }
            if (!IsSupportedChangeType(change.change_type))
            {
                error_message = "PatchPlan.change_type is unsupported.";
                return false;
            }

            const auto safe_path = SafeRelativePath(std::filesystem::path(change.target_file));
            change.target_file = safe_path.generic_string();
        }
    }
    catch (const std::exception&)
    {
        error_message = "PatchPlan target_file must be workspace-relative.";
        return false;
    }

    return true;
}
} // namespace

ImplementerResult TryCreatePatchPlan(
    const ImplementerInput& input,
    const ILLMProvider& provider)
{
    ImplementerResult result;
    result.prompt = BuildImplementerPrompt(input);

    try
    {
        const nlohmann::json json_value = provider.GenerateJson(result.prompt);
        result.patch_plan = json_value.get<PatchPlan>();
        if (!ValidatePatchPlan(result.patch_plan, result.error_message))
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
