#pragma once

#include <string>
#include <vector>

#include "core/Models.h"
#include "llm/ILLMProvider.h"
#include "patching/PatchPlan.h"

namespace agentguard
{
struct RelevantFileContent
{
    std::string relative_path;
    std::string content;
};

struct ImplementerInput
{
    TaskSpec task_spec;
    ProjectInfo project_info;
    std::vector<RelevantFileContent> relevant_files;
};

struct ImplementerResult
{
    bool success = false;
    PatchPlan patch_plan;
    std::string prompt;
    std::string error_message;
};

ImplementerResult TryCreatePatchPlan(
    const ImplementerInput& input,
    const ILLMProvider& provider);
} // namespace agentguard
