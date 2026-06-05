#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/Models.h"
#include "patching/DiffReporter.h"
#include "patching/PatchApplier.h"

namespace agentguard
{
struct ReportMetrics
{
    std::int64_t analyze_duration_ms = 0;
    std::int64_t verify_duration_ms = 0;
    std::int64_t review_duration_ms = 0;
    std::size_t modified_file_count = 0;
    std::size_t allowed_file_count = 0;
    std::size_t suspected_file_count = 0;
    std::size_t protected_file_count = 0;
    std::size_t build_error_count = 0;
    int repair_rounds = 0;
    int scope_expansions = 0;
    std::string provider;
    std::string model;
    bool cache_hit = false;
};

struct ReportWriteRequest
{
    std::filesystem::path reports_directory;
    std::filesystem::path source_project;
    std::filesystem::path workspace_repo;
    std::string source_modified = "unknown";
    TaskSpec task_spec;
    std::vector<std::string> related_files;
    PatchApplyResult patch_apply_result;
    BuildResult build_result;
    int repair_rounds = 0;
    DiffReport diff_report;
    std::vector<std::string> risk_warnings;
    std::optional<SemanticReviewResult> semantic_review;
    std::string review_next_action;
    ReportMetrics metrics;
    std::map<std::string, std::string> artifacts;
    nlohmann::json audit = nlohmann::json::object();
};

struct ReportWriteResult
{
    std::filesystem::path markdown_path;
    std::filesystem::path json_path;
};

ReportWriteResult WriteReport(const ReportWriteRequest& request);
} // namespace agentguard
