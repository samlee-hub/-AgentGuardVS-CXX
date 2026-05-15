#pragma once

#include <string>
#include <vector>

#include "core/Models.h"
#include "core/SemanticModels.h"
#include "indexing/ContextSelector.h"
#include "llm/ILLMProvider.h"

namespace agentguard
{
struct PlannerInput
{
    std::string user_request;
    ProjectInfo project_info;
    std::vector<SourceFileInfo> source_files;
    std::vector<ContextCandidate> context_candidates;
    bool has_semantic_scope = false;
    SemanticScopeResult semantic_scope;
    double semantic_allowed_confidence_threshold = 0.7;
};

struct PlannerResult
{
    bool success = false;
    TaskSpec task_spec;
    std::string prompt;
    std::string error_message;
};

PlannerResult TryPlanTask(
    const PlannerInput& input,
    const ILLMProvider& provider);
} // namespace agentguard
