#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "agents/BuildFixerAgent.h"
#include "agents/ImplementerAgent.h"
#include "agents/PlannerAgent.h"
#include "agents/ReviewerAgent.h"
#include "core/Models.h"

namespace agentguard
{
using RepairBuildExecutor = std::function<BuildResult(const TaskSpec& task_spec, int round_index)>;

struct RepairLoopInput
{
    std::filesystem::path repo_root;
    std::filesystem::path logs_directory;
    std::filesystem::path reports_directory;
    PlannerInput planner_input;
    std::vector<RelevantFileContent> initial_relevant_files;
    std::string git_diff_summary;
    const ILLMProvider* planner_provider = nullptr;
    const ILLMProvider* implementer_provider = nullptr;
    const ILLMProvider* build_fixer_provider = nullptr;
    const ILLMProvider* semantic_review_provider = nullptr;
    int max_scope_expansions = 1;
    RepairBuildExecutor build_executor;
};

struct RepairLoopResult
{
    bool success = false;
    RunReport report;
    std::string error_message;
};

RepairLoopResult RunRepairLoop(const RepairLoopInput& input);
} // namespace agentguard
