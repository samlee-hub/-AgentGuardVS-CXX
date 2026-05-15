#include "report/ReportWriter.h"

#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "core/PathUtils.h"

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
} // namespace

ReportWriteResult WriteReport(const ReportWriteRequest& request)
{
    EnsureDirectory(request.reports_directory);

    ReportWriteResult result;
    result.markdown_path = request.reports_directory / "report.md";
    result.json_path = request.reports_directory / "report.json";

    const auto error_summary = SummarizeErrorCategories(request.build_result);

    {
        std::ofstream markdown(result.markdown_path, std::ios::trunc);
        if (!markdown)
        {
            throw std::runtime_error("Unable to create Markdown report.");
        }

        markdown << "# AgentGuardVS-CXX Run Report\n\n";
        markdown << "## User Task\n" << request.task_spec.user_request << "\n\n";
        markdown << "## Target\n";
        markdown << "- Solution: " << request.task_spec.target_solution << "\n";
        markdown << "- Project: " << request.task_spec.target_project << "\n\n";
        markdown << "## Workspace Audit\n";
        markdown << "- Source project: " << request.source_project.generic_string() << "\n";
        markdown << "- Workspace repo: " << request.workspace_repo.generic_string() << "\n";
        markdown << "- Git top-level: " << request.diff_report.git_top_level.generic_string() << "\n";
        markdown << "- Diff base: " << request.diff_report.diff_base << "\n";
        markdown << "- Source modified: " << request.source_modified << "\n";
        markdown << "- Workspace modified: "
                 << (request.diff_report.workspace_modified ? "true" : "false") << "\n\n";

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
        markdown << "- Success: " << (request.build_result.success ? "true" : "false") << "\n";
        markdown << "- Return code: " << request.build_result.return_code << "\n";
        markdown << "- Command: " << request.build_result.command << "\n";

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
        markdown << request.diff_report.diff_stat << "\n";

        markdown << "\n## Risk Warnings\n";
        WriteList(markdown, request.risk_warnings);
    }

    {
        const nlohmann::json json_value{
            {"task_spec", request.task_spec},
            {"source_project", request.source_project.generic_string()},
            {"workspace_repo", request.workspace_repo.generic_string()},
            {"git_top_level", request.diff_report.git_top_level.generic_string()},
            {"diff_base", request.diff_report.diff_base},
            {"source_modified", request.source_modified},
            {"workspace_modified", request.diff_report.workspace_modified},
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
            {"build_result", request.build_result},
            {"error_category_summary", error_summary},
            {"repair_rounds", request.repair_rounds},
            {"git_diff", {
                {"success", request.diff_report.success},
                {"stat", request.diff_report.diff_stat},
                {"text", request.diff_report.diff_text},
                {"warnings", request.diff_report.warnings}
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
