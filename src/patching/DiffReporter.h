#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/ProcessRunner.h"

namespace agentguard
{
struct DiffReport
{
    bool success = false;
    std::string diff_text;
    std::string diff_stat;
    std::filesystem::path git_top_level;
    std::string diff_base;
    bool workspace_modified = false;
    std::vector<std::string> warnings;
};

DiffReport CaptureWorkspaceDiff(const std::filesystem::path& repo_root);
DiffReport CaptureWorkspaceDiff(
    const std::filesystem::path& repo_root,
    const IProcessRunner& process_runner);
} // namespace agentguard
