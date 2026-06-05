#pragma once

#include <string>
#include <vector>

#include "agents/ImplementerAgent.h"
#include "core/Models.h"
#include "core/SemanticModels.h"
#include "llm/ILLMProvider.h"

namespace agentguard
{
struct SemanticReviewInput
{
    std::string user_request;
    SemanticScopeResult semantic_scope;
    std::string diff_text;
    BuildResult build_result;
    std::vector<ParsedBuildError> parsed_errors;
    std::vector<RelevantFileContent> relevant_files;
};

struct SemanticReviewAgentResult
{
    bool success = false;
    SemanticReviewResult review;
    std::string prompt;
    std::string raw_response;
    std::string extracted_json_text;
    std::string error_message;
};

SemanticReviewAgentResult TryReviewSemanticChange(
    const SemanticReviewInput& input,
    const ILLMProvider& provider);
} // namespace agentguard
