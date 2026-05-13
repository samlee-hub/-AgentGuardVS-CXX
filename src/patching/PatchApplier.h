#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Models.h"
#include "patching/PatchPlan.h"

namespace agentguard
{
struct PatchApplyResult
{
    bool success = false;
    std::vector<std::string> applied_files;
    std::vector<std::string> errors;
};

PatchApplyResult ApplyPatchPlan(
    const PatchPlan& plan,
    const TaskSpec& spec,
    const std::filesystem::path& repo_root);
} // namespace agentguard
