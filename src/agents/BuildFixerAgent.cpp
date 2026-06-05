#include "agents/BuildFixerAgent.h"

#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
std::string CategoryGuidance(ErrorCategory category)
{
    switch (category)
    {
    case ErrorCategory::IncludeError:
        return "IncludeError: check include path, header filename, relative path, and vcxproj include directories.";
    case ErrorCategory::CompileError:
        return "CompileError: check variable declarations, scope, types, function signatures, and header/source synchronization.";
    case ErrorCategory::LinkError:
        return "LinkError: check declaration and definition consistency, whether cpp files are included in the project, and library paths.";
    case ErrorCategory::MSBuildError:
    case ErrorCategory::ConfigurationError:
        return "MSBuildError: check project configuration, platform, and build events.";
    case ErrorCategory::Unknown:
        return "Unknown: preserve raw error context and make the smallest targeted fix.";
    }

    return "Unknown: preserve raw error context and make the smallest targeted fix.";
}

std::string BuildPrompt(const BuildFixerInput& input)
{
    std::ostringstream prompt;
    prompt << "Create a repair PatchPlan for a failed Visual Studio C++ build.\n";
    prompt << "Return strict JSON only. Do not write files. Do not emit prose.\n";
    prompt << "PatchPlan schema: {\"changes\":[{\"target_file\",\"change_type\",\"original_snippet\",";
    prompt << "\"replacement_snippet\",\"explanation\"}]}.\n";
    prompt << "Every target_file must be workspace-relative and should stay within TaskSpec.allowed_files.\n";
    prompt << "Task: " << input.task_spec.user_request << "\n";
    prompt << "Git diff summary: " << input.git_diff_summary << "\n";

    std::set<ErrorCategory> categories;
    prompt << "Build errors:\n";
    for (const auto& error : input.parsed_errors)
    {
        categories.insert(error.category);
        prompt << "- " << error.file << "(" << error.line << "," << error.column << ") ";
        prompt << error.code << ": " << error.message << "\n";
    }

    prompt << "Repair guidance:\n";
    for (const auto category : categories)
    {
        prompt << "- " << CategoryGuidance(category) << "\n";
    }

    prompt << "Current related files:\n";
    for (const auto& file : input.current_files)
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

            change.target_file = SafeRelativePath(std::filesystem::path(change.target_file)).generic_string();
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

BuildFixerResult TryCreateBuildFixPatchPlan(
    const BuildFixerInput& input,
    const ILLMProvider& provider)
{
    BuildFixerResult result;
    result.prompt = BuildPrompt(input);

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
