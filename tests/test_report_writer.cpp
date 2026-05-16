#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "report/ReportWriter.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::BuildResult;
using agentguard::DiffReport;
using agentguard::PatchApplyResult;
using agentguard::ReportWriteRequest;
using agentguard::TaskSpec;
using agentguard::WriteReport;

fs::path MakeReportsDir()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path directory = fs::temp_directory_path() / ("agentguard_report_" + std::to_string(stamp));
    fs::create_directories(directory);
    return directory;
}

std::string ReadText(const fs::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

TEST(ReportWriterTest, WritesMarkdownAndJsonWithAuditFields)
{
    const fs::path reports_dir = MakeReportsDir();

    TaskSpec spec;
    spec.task_id = "report-task";
    spec.user_request = "Add summon cooldown.";
    spec.target_solution = "Server.sln";
    spec.target_project = "Server";
    spec.allowed_files = {"src/Summon.cpp"};
    spec.forbidden_files = {"src/Auth.cpp"};
    spec.has_semantic_scope = true;
    spec.semantic_scope.task_summary = "Add summon cooldown.";
    spec.semantic_scope.risk_level = "medium";
    spec.semantic_scope.recommendation = "proceed";
    spec.semantic_scope.blast_radius = "medium";
    spec.semantic_scope.public_interface_changes = true;
    spec.semantic_scope.related_symbols = {"function:CanSummon@src/Summon.cpp"};
    spec.semantic_scope.dependency_reasons = {"src/Summon.h is included by src/Summon.cpp"};
    spec.semantic_scope.allowed_files = {
        agentguard::SemanticFileReference{"src/Summon.cpp", "Contains summon implementation.", 0.95}
    };
    spec.acceptance_criteria = {"Build succeeds"};
    spec.max_repair_rounds = 3;

    PatchApplyResult patch_result;
    patch_result.success = true;
    patch_result.applied_files = {"src/Summon.cpp"};

    BuildResult build_result;
    build_result.success = false;
    build_result.command = "msbuild Server.sln /m";
    build_result.return_code = 1;

    DiffReport diff_report;
    diff_report.success = true;
    diff_report.diff_stat = "1 file changed, 2 insertions(+)";
    diff_report.diff_text = "diff --git a/src/Summon.cpp b/src/Summon.cpp";
    diff_report.git_top_level = "C:/runs/task/repo";
    diff_report.diff_base = "agentguard baseline";
    diff_report.workspace_modified = true;

    ReportWriteRequest request;
    request.reports_directory = reports_dir;
    request.source_project = "C:/source/Server";
    request.workspace_repo = "C:/runs/task/repo";
    request.source_modified = "false";
    request.task_spec = spec;
    request.related_files = {"src/Summon.cpp"};
    request.patch_apply_result = patch_result;
    request.build_result = build_result;
    request.repair_rounds = 1;
    request.diff_report = diff_report;
    request.risk_warnings = {"Build is still failing."};
    request.metrics.analyze_duration_ms = 12;
    request.metrics.verify_duration_ms = 34;
    request.metrics.review_duration_ms = 56;
    request.metrics.modified_file_count = 1;
    request.metrics.allowed_file_count = 1;
    request.metrics.suspected_file_count = 0;
    request.metrics.protected_file_count = 0;
    request.metrics.build_error_count = 1;
    request.metrics.repair_rounds = 1;
    request.metrics.scope_expansions = 0;
    request.metrics.provider = "fake";
    request.metrics.model = "fake-model";
    request.artifacts = {
        {"report_md", (reports_dir / "report.md").generic_string()},
        {"report_json", (reports_dir / "report.json").generic_string()},
        {"build_log", "C:/runs/task/artifacts/build_logs/verify_build.log"}
    };

    const auto result = WriteReport(request);

    EXPECT_TRUE(fs::exists(result.markdown_path));
    EXPECT_TRUE(fs::exists(result.json_path));

    const std::string markdown = ReadText(result.markdown_path);
    EXPECT_NE(markdown.find("Add summon cooldown."), std::string::npos);
    EXPECT_NE(markdown.find("Server.sln"), std::string::npos);
    EXPECT_NE(markdown.find("src/Summon.cpp"), std::string::npos);
    EXPECT_NE(markdown.find("1 file changed, 2 insertions(+)"), std::string::npos);
    EXPECT_NE(markdown.find("Build is still failing."), std::string::npos);
    EXPECT_NE(markdown.find("agentguard baseline"), std::string::npos);
    EXPECT_NE(markdown.find("C:/runs/task/repo"), std::string::npos);
    EXPECT_NE(markdown.find("Blast radius: medium"), std::string::npos);
    EXPECT_NE(markdown.find("Public interface changes: true"), std::string::npos);

    {
        std::ifstream json_input(result.json_path);
        const nlohmann::json json_value = nlohmann::json::parse(json_input);
        EXPECT_EQ(json_value.at("schema_version").get<std::string>(), "1.0");
        EXPECT_EQ(json_value.at("task_spec").at("task_id").get<std::string>(), "report-task");
        EXPECT_EQ(json_value.at("patch_apply_result").at("success").get<bool>(), true);
        EXPECT_EQ(json_value.at("build_result").at("return_code").get<int>(), 1);
        EXPECT_EQ(json_value.at("repair_rounds").get<int>(), 1);
        EXPECT_EQ(json_value.at("git_diff").at("stat").get<std::string>(), "1 file changed, 2 insertions(+)");
        EXPECT_EQ(json_value.at("workspace_repo").get<std::string>(), "C:/runs/task/repo");
        EXPECT_EQ(json_value.at("git_top_level").get<std::string>(), "C:/runs/task/repo");
        EXPECT_EQ(json_value.at("diff_base").get<std::string>(), "agentguard baseline");
        EXPECT_EQ(json_value.at("source_modified").get<std::string>(), "false");
        EXPECT_EQ(json_value.at("workspace_modified").get<bool>(), true);
        EXPECT_EQ(json_value.at("semantic_scope").at("blast_radius").get<std::string>(), "medium");
        EXPECT_TRUE(json_value.at("semantic_scope").at("public_interface_changes").get<bool>());
        EXPECT_EQ(json_value.at("task").at("id").get<std::string>(), "report-task");
        EXPECT_EQ(json_value.at("source_project").at("path").get<std::string>(), "C:/source/Server");
        EXPECT_EQ(json_value.at("workspace").at("repo").get<std::string>(), "C:/runs/task/repo");
        EXPECT_EQ(json_value.at("build").at("return_code").get<int>(), 1);
        EXPECT_EQ(json_value.at("diff").at("base").get<std::string>(), "agentguard baseline");
        EXPECT_EQ(json_value.at("metrics").at("analyze_duration_ms").get<int>(), 12);
        EXPECT_EQ(json_value.at("metrics").at("verify_duration_ms").get<int>(), 34);
        EXPECT_EQ(json_value.at("metrics").at("review_duration_ms").get<int>(), 56);
        EXPECT_EQ(json_value.at("metrics").at("modified_file_count").get<int>(), 1);
        EXPECT_EQ(json_value.at("metrics").at("allowed_file_count").get<int>(), 1);
        EXPECT_EQ(json_value.at("metrics").at("build_error_count").get<int>(), 1);
        EXPECT_EQ(json_value.at("metrics").at("provider").get<std::string>(), "fake");
        EXPECT_EQ(json_value.at("metrics").at("model").get<std::string>(), "fake-model");
        EXPECT_EQ(json_value.at("artifacts").at("build_log").get<std::string>(),
            "C:/runs/task/artifacts/build_logs/verify_build.log");
    }

    fs::remove_all(reports_dir);
}

TEST(ReportWriterTest, RedactsSecretsFromReports)
{
    const fs::path reports_dir = MakeReportsDir();

    TaskSpec spec;
    spec.task_id = "secret-report";
    spec.user_request = "Build with API key.";
    spec.target_solution = "Server.sln";

    PatchApplyResult patch_result;

    BuildResult build_result;
    build_result.command = "msbuild Server.sln";
    build_result.stdout_text = "Authorization: Bearer token-redaction-sample-1234567890";
    build_result.stderr_text = "OPENAI_API_KEY=placeholder-secret-value";

    DiffReport diff_report;
    diff_report.diff_text = "diff --git a/src/App.cpp b/src/App.cpp";

    ReportWriteRequest request;
    request.reports_directory = reports_dir;
    request.task_spec = spec;
    request.patch_apply_result = patch_result;
    request.build_result = build_result;
    request.diff_report = diff_report;

    const auto result = WriteReport(request);
    const std::string json_text = ReadText(result.json_path);

    EXPECT_EQ(json_text.find("token-redaction-sample-1234567890"), std::string::npos);
    EXPECT_EQ(json_text.find("placeholder-secret-value"), std::string::npos);
    EXPECT_NE(json_text.find("[REDACTED_TOKEN]"), std::string::npos);

    fs::remove_all(reports_dir);
}

TEST(ReportWriterTest, MarkdownStartsWithOneScreenConclusion)
{
    const fs::path reports_dir = MakeReportsDir();

    TaskSpec spec;
    spec.task_id = "summary-report";
    spec.user_request = "Add summon cooldown.";
    spec.target_solution = "Server.sln";
    spec.allowed_files = {"src/Summon.cpp"};

    PatchApplyResult patch_result;
    patch_result.success = true;
    patch_result.applied_files = {"src/Summon.cpp"};

    BuildResult build_result;
    build_result.success = true;
    build_result.return_code = 0;

    DiffReport diff_report;
    diff_report.workspace_modified = true;

    ReportWriteRequest request;
    request.reports_directory = reports_dir;
    request.source_project = "C:/source/Server";
    request.workspace_repo = "C:/runs/task/repo";
    request.source_modified = "false";
    request.task_spec = spec;
    request.patch_apply_result = patch_result;
    request.build_result = build_result;
    request.diff_report = diff_report;
    request.review_next_action = "accept";
    request.artifacts = {
        {"report_md", (reports_dir / "report.md").generic_string()},
        {"report_json", (reports_dir / "report.json").generic_string()},
        {"build_log", "C:/runs/task/artifacts/build_logs/verify_build.log"}
    };

    const auto result = WriteReport(request);
    const std::string markdown = ReadText(result.markdown_path);

    const auto summary_position = markdown.find("## Summary");
    ASSERT_NE(summary_position, std::string::npos);
    EXPECT_LT(summary_position, markdown.find("## User Task"));
    EXPECT_NE(markdown.find("- Accepted: true"), std::string::npos);
    EXPECT_NE(markdown.find("- Source modified: false"), std::string::npos);
    EXPECT_NE(markdown.find("- Modified files:"), std::string::npos);
    EXPECT_NE(markdown.find("src/Summon.cpp"), std::string::npos);
    EXPECT_NE(markdown.find("- Report JSON: "), std::string::npos);
    EXPECT_NE(markdown.find("- Build log: C:/runs/task/artifacts/build_logs/verify_build.log"), std::string::npos);

    fs::remove_all(reports_dir);
}
} // namespace
