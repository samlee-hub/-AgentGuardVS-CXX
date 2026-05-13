#pragma once

#include <filesystem>
#include <vector>

#include "core/Models.h"
#include "patching/DiffReporter.h"
#include "patching/PatchApplier.h"

namespace agentguard
{
struct ReportWriteRequest
{
    std::filesystem::path reports_directory;
    TaskSpec task_spec;
    std::vector<std::string> related_files;
    PatchApplyResult patch_apply_result;
    BuildResult build_result;
    int repair_rounds = 0;
    DiffReport diff_report;
    std::vector<std::string> risk_warnings;
};

struct ReportWriteResult
{
    std::filesystem::path markdown_path;
    std::filesystem::path json_path;
};

ReportWriteResult WriteReport(const ReportWriteRequest& request);
} // namespace agentguard
