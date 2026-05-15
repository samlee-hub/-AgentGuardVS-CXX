#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "agents/ImplementerAgent.h"
#include "core/Models.h"
#include "core/SemanticModels.h"
#include "indexing/ContextSelector.h"
#include "indexing/DependencyGraph.h"
#include "llm/ILLMProvider.h"

namespace agentguard
{
struct SemanticAnalyzerInput
{
    std::string user_request;
    std::filesystem::path solution_path;
    ProjectInfo project_info;
    std::vector<SourceFileInfo> source_files;
    std::vector<ContextCandidate> context_candidates;
    std::vector<RelevantFileContent> relevant_files;
    std::vector<std::string> static_allowed_files;
    std::vector<std::string> static_forbidden_files;
    ImpactAnalysis impact;
};

struct SemanticAnalyzerResult
{
    bool success = false;
    SemanticScopeResult scope;
    std::string prompt;
    std::string raw_response;
    std::string extracted_json_text;
    std::string error_message;
};

SemanticAnalyzerResult TryAnalyzeSemanticScope(
    const SemanticAnalyzerInput& input,
    const ILLMProvider& provider);
} // namespace agentguard
