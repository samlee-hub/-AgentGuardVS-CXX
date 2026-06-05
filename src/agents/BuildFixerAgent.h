#pragma once

#include <string>
#include <vector>

#include "agents/ImplementerAgent.h"
#include "core/Models.h"
#include "llm/ILLMProvider.h"
#include "patching/PatchPlan.h"

namespace agentguard
{
struct BuildFixerInput
{
    TaskSpec task_spec;
    BuildResult build_result;
    std::vector<ParsedBuildError> parsed_errors;
    std::vector<RelevantFileContent> current_files;
    std::string git_diff_summary;
};

struct BuildFixerResult
{
    bool success = false;
    PatchPlan patch_plan;
    std::string prompt;
    std::string error_message;
};

BuildFixerResult TryCreateBuildFixPatchPlan(
    const BuildFixerInput& input,
    const ILLMProvider& provider);
} // namespace agentguard
