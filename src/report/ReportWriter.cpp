#include "report/ReportWriter.h"

#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/PathUtils.h"
#include "security/SecurityPolicy.h"

namespace agentguard
{
namespace
{
std::map<std::string, int> SummarizeErrorCategories(const BuildResult& build_result)
{
    std::map<std::string, int> summary;
    for (const auto& error : build_result.parsed_errors)
    {
        nlohmann::json category_json = error.category;
        ++summary[category_json.get<std::string>()];
    }
    return summary;
}

void WriteList(std::ofstream& output, const std::vector<std::string>& values)
{
    if (values.empty())
    {
        output << "- None\n";
        return;
    }

    for (const auto& value : values)
    {
        output << "- " << value << "\n";
    }
}

void WriteSemanticFiles(std::ofstream& output, const std::vector<SemanticFileReference>& values)
{
    if (values.empty())
    {
        output << "- None\n";
        return;
    }

    for (const auto& value : values)
    {
        output << "- " << value.path << " confidence=" << value.confidence
               << " reason=" << value.reason << "\n";
    }
}

nlohmann::json ToJson(const PatchApplyResult& result)
{
    return nlohmann::json{
        {"success", result.success},
        {"applied_files", result.applied_files},
        {"errors", result.errors}
    };
}

nlohmann::json ToJson(const ReportMetrics& metrics)
{
    return nlohmann::json{
        {"analyze_duration_ms", metrics.analyze_duration_ms},
        {"verify_duration_ms", metrics.verify_duration_ms},
        {"review_duration_ms", metrics.review_duration_ms},
        {"modified_file_count", metrics.modified_file_count},
        {"allowed_file_count", metrics.allowed_file_count},
        {"suspected_file_count", metrics.suspected_file_count},
        {"protected_file_count", metrics.protected_file_count},
        {"build_error_count", metrics.build_error_count},
        {"repair_rounds", metrics.repair_rounds},
        {"scope_expansions", metrics.scope_expansions},
        {"provider", metrics.provider},
        {"model", metrics.model},
        {"cache_hit", metrics.cache_hit}
    };
}

BuildResult RedactBuildResult(BuildResult result)
{
    result.command = RedactSensitiveText(result.command);
    result.stdout_text = RedactSensitiveText(result.stdout_text);
    result.stderr_text = RedactSensitiveText(result.stderr_text);
    for (auto& error : result.parsed_errors)
    {
        error.raw_line = RedactSensitiveText(error.raw_line);
        error.message = RedactSensitiveText(error.message);
    }
    return result;
}

DiffReport RedactDiffReport(DiffReport result)
{
    result.diff_text = RedactSensitiveText(result.diff_text);
    result.diff_stat = RedactSensitiveText(result.diff_stat);
    for (auto& warning : result.warnings)
    {
        warning = RedactSensitiveText(warning);
    }
    return result;
}

std::string ArtifactPath(
    const std::map<std::string, std::string>& artifacts,
    const std::string& key)
{
    const auto found = artifacts.find(key);
    return found == artifacts.end() ? "" : found->second;
}

std::vector<std::string> ModifiedFiles(const ReportWriteRequest& request)
{
    if (!request.patch_apply_result.applied_files.empty())
    {
        return request.patch_apply_result.applied_files;
    }
    return request.related_files;
}

bool IsAccepted(const ReportWriteRequest& request, const BuildResult& build_result)
{
    if (!request.review_next_action.empty())
    {
        return request.review_next_action == "accept";
    }
    if (request.semantic_review.has_value())
    {
        return request.semantic_review->next_action == "accept";
    }
    return build_result.success && request.risk_warnings.empty();
}
} // namespace

ReportWriteResult WriteReport(const ReportWriteRequest& request)
{
    EnsureDirectory(request.reports_directory);

    ReportWriteResult result;
    result.markdown_path = request.reports_directory / "report.md";
    result.json_path = request.reports_directory / "report.json";

    const BuildResult redacted_build_result = RedactBuildResult(request.build_result);
    const DiffReport redacted_diff_report = RedactDiffReport(request.diff_report);
    const auto error_summary = SummarizeErrorCategories(redacted_build_result);

    {
        std::ofstream markdown(result.markdown_path, std::ios::trunc);
        if (!markdown)
        {
            throw std::runtime_error("Unable to create Markdown report.");
        }

        const auto modified_files = ModifiedFiles(request);
        markdown << "# AgentGuardVS-CXX Run Report\n\n";
        markdown << "## Summary\n";
        markdown << "- Accepted: " << (IsAccepted(request, redacted_build_result) ? "true" : "false") << "\n";
        markdown << "- Build success: " << (redacted_build_result.success ? "true" : "false") << "\n";
        markdown << "- Review next action: "
                 << (request.review_next_action.empty() ? "unknown" : request.review_next_action) << "\n";
        markdown << "- Source modified: " << request.source_modified << "\n";
        markdown << "- Workspace modified: "
                 << (redacted_diff_report.workspace_modified ? "true" : "false") << "\n";
        markdown << "- Modified files:\n";
        WriteList(markdown, modified_files);
        markdown << "- Risk count: " << request.risk_warnings.size() << "\n";
        markdown << "- Recommendation: "
                 << (IsAccepted(request, redacted_build_result) ? "accept" : "review required") << "\n";
        markdown << "- Report Markdown: " << ArtifactPath(request.artifacts, "report_md") << "\n";
        markdown << "- Report JSON: " << ArtifactPath(request.artifacts, "report_json") << "\n";
        markdown << "- Build log: " << ArtifactPath(request.artifacts, "build_log") << "\n\n";

        markdown << "## User Task\n" << request.task_spec.user_request << "\n\n";
        markdown << "## Target\n";
        markdown << "- Solution: " << request.task_spec.target_solution << "\n";
        markdown << "- Project: " << request.task_spec.target_project << "\n\n";
        markdown << "## Workspace Audit\n";
        markdown << "- Source project: " << request.source_project.generic_string() << "\n";
        markdown << "- Workspace repo: " << request.workspace_repo.generic_string() << "\n";
        markdown << "- Git top-level: " << redacted_diff_report.git_top_level.generic_string() << "\n";
        markdown << "- Diff base: " << redacted_diff_report.diff_base << "\n";
        markdown << "- Source modified: " << request.source_modified << "\n";
        markdown << "- Workspace modified: "
                 << (redacted_diff_report.workspace_modified ? "true" : "false") << "\n\n";

        markdown << "## Related Files\n";
        WriteList(markdown, request.related_files);
        markdown << "\n## Allowed Files\n";
        WriteList(markdown, request.task_spec.allowed_files);
        markdown << "\n## Forbidden Files\n";
        WriteList(markdown, request.task_spec.forbidden_files);

        if (request.task_spec.has_semantic_scope)
        {
            markdown << "\n## Semantic Scope\n";
            markdown << "- Risk level: " << request.task_spec.semantic_scope.risk_level << "\n";
            markdown << "- Recommendation: " << request.task_spec.semantic_scope.recommendation << "\n";
            markdown << "- Blast radius: " << request.task_spec.semantic_scope.blast_radius << "\n";
            markdown << "- Public interface changes: "
                     << (request.task_spec.semantic_scope.public_interface_changes ? "true" : "false")
                     << "\n";
            markdown << "- Related symbols:\n";
            WriteList(markdown, request.task_spec.semantic_scope.related_symbols);
            markdown << "- Dependency reasons:\n";
            WriteList(markdown, request.task_spec.semantic_scope.dependency_reasons);
            markdown << "\n### Semantic Allowed Files\n";
            WriteSemanticFiles(markdown, request.task_spec.semantic_scope.allowed_files);
            markdown << "\n### Semantic Context Files\n";
            WriteSemanticFiles(markdown, request.task_spec.semantic_scope.context_files);
            markdown << "\n### Semantic Suspected Files\n";
            WriteSemanticFiles(markdown, request.task_spec.semantic_scope.suspected_files);
            markdown << "\n### Semantic Protected Files\n";
            WriteSemanticFiles(markdown, request.task_spec.semantic_scope.protected_files);
            markdown << "\n### Semantic Needs Approval Files\n";
            WriteSemanticFiles(markdown, request.task_spec.semantic_scope.needs_approval_files);
        }

        markdown << "\n## Patch Apply Result\n";
        markdown << "- Success: " << (request.patch_apply_result.success ? "true" : "false") << "\n";
        markdown << "- Applied files:\n";
        WriteList(markdown, request.patch_apply_result.applied_files);
        markdown << "- Errors:\n";
        WriteList(markdown, request.patch_apply_result.errors);

        markdown << "\n## Build Result\n";
        markdown << "- Success: " << (redacted_build_result.success ? "true" : "false") << "\n";
        markdown << "- Return code: " << redacted_build_result.return_code << "\n";
        markdown << "- Command: " << redacted_build_result.command << "\n";

        markdown << "\n## Error Classification Summary\n";
        if (error_summary.empty())
        {
            markdown << "- None\n";
        }
        else
        {
            for (const auto& [category, count] : error_summary)
            {
                markdown << "- " << category << ": " << count << "\n";
            }
        }

        markdown << "\n## Repair Rounds\n";
        markdown << request.repair_rounds << "\n";

        markdown << "\n## Git Diff Summary\n";
        markdown << redacted_diff_report.diff_stat << "\n";

        markdown << "\n## Risk Warnings\n";
        WriteList(markdown, request.risk_warnings);
    }

    {
        const auto metrics_json = ToJson(request.metrics);
        const nlohmann::json review_json = request.semantic_review.has_value()
            ? nlohmann::json(*request.semantic_review)
            : nlohmann::json(nullptr);
        const auto modified_files = ModifiedFiles(request);
        const nlohmann::json json_value{
            {"schema_version", "1.0"},
            {"task", {
                {"id", request.task_spec.task_id},
                {"user_request", request.task_spec.user_request},
                {"target_solution", request.task_spec.target_solution},
                {"target_project", request.task_spec.target_project},
                {"acceptance_criteria", request.task_spec.acceptance_criteria}
            }},
            {"source_project", {
                {"path", request.source_project.generic_string()},
                {"modified", request.source_modified}
            }},
            {"workspace", {
                {"repo", request.workspace_repo.generic_string()},
                {"git_top_level", redacted_diff_report.git_top_level.generic_string()},
                {"modified", redacted_diff_report.workspace_modified}
            }},
            {"semantic_scope", request.task_spec.has_semantic_scope
                ? nlohmann::json(request.task_spec.semantic_scope)
                : nlohmann::json(nullptr)},
            {"build", redacted_build_result},
            {"review", review_json},
            {"diff", {
                {"success", redacted_diff_report.success},
                {"base", redacted_diff_report.diff_base},
                {"stat", redacted_diff_report.diff_stat},
                {"text", redacted_diff_report.diff_text},
                {"warnings", redacted_diff_report.warnings}
            }},
            {"risks", {
                {"warnings", request.risk_warnings},
                {"review_risks", request.semantic_review.has_value()
                    ? nlohmann::json(request.semantic_review->risks)
                    : nlohmann::json::array()}
            }},
            {"metrics", metrics_json},
            {"artifacts", request.artifacts},
            {"task_spec", request.task_spec},
            {"workspace_repo", request.workspace_repo.generic_string()},
            {"git_top_level", redacted_diff_report.git_top_level.generic_string()},
            {"diff_base", redacted_diff_report.diff_base},
            {"source_modified", request.source_modified},
            {"workspace_modified", redacted_diff_report.workspace_modified},
            {"related_files", request.related_files},
            {"allowed_files", request.task_spec.allowed_files},
            {"forbidden_files", request.task_spec.forbidden_files},
            {"context_files", request.task_spec.context_files},
            {"suspected_files", request.task_spec.suspected_files},
            {"protected_files", request.task_spec.protected_files},
            {"needs_approval_files", request.task_spec.needs_approval_files},
            {"semantic_scope", request.task_spec.has_semantic_scope
                ? nlohmann::json(request.task_spec.semantic_scope)
                : nlohmann::json(nullptr)},
            {"patch_apply_result", ToJson(request.patch_apply_result)},
            {"build_result", redacted_build_result},
            {"error_category_summary", error_summary},
            {"repair_rounds", request.repair_rounds},
            {"review_next_action", request.review_next_action},
            {"modified_files", modified_files},
            {"git_diff", {
                {"success", redacted_diff_report.success},
                {"stat", redacted_diff_report.diff_stat},
                {"text", redacted_diff_report.diff_text},
                {"warnings", redacted_diff_report.warnings}
            }},
            {"risk_warnings", request.risk_warnings}
        };

        std::ofstream json_output(result.json_path, std::ios::trunc);
        if (!json_output)
        {
            throw std::runtime_error("Unable to create JSON report.");
        }

        json_output << json_value.dump(2);
    }

    return result;
}
} // namespace agentguard
