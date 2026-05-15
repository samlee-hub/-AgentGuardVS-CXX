#pragma once

#include <filesystem>

#include "core/ProcessRunner.h"

namespace agentguard
{
struct WorkspaceOptions
{
    std::filesystem::path source_project_path;
    std::filesystem::path runs_root;
    std::filesystem::path task_id;
    bool force = false;
    const IProcessRunner* git_runner = nullptr;
};

struct WorkspacePaths
{
    std::filesystem::path task_root;
    std::filesystem::path repo_root;
    std::filesystem::path logs_root;
    std::filesystem::path reports_root;
    std::filesystem::path metadata_path;
    std::filesystem::path source_project_path;
    std::string diff_base;
};

class RunWorkspace
{
public:
    explicit RunWorkspace(WorkspaceOptions options);

    WorkspacePaths Prepare() const;

private:
    WorkspaceOptions options_;
};
} // namespace agentguard
