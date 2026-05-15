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
        EXPECT_EQ(json_value.at("task_spec").at("task_id").get<std::string>(), "report-task");
        EXPECT_EQ(json_value.at("patch_apply_result").at("success").get<bool>(), true);
        EXPECT_EQ(json_value.at("build_result").at("return_code").get<int>(), 1);
        EXPECT_EQ(json_value.at("repair_rounds").get<int>(), 1);
        EXPECT_EQ(json_value.at("git_diff").at("stat").get<std::string>(), "1 file changed, 2 insertions(+)");
        EXPECT_EQ(json_value.at("source_project").get<std::string>(), "C:/source/Server");
        EXPECT_EQ(json_value.at("workspace_repo").get<std::string>(), "C:/runs/task/repo");
        EXPECT_EQ(json_value.at("git_top_level").get<std::string>(), "C:/runs/task/repo");
        EXPECT_EQ(json_value.at("diff_base").get<std::string>(), "agentguard baseline");
        EXPECT_EQ(json_value.at("source_modified").get<std::string>(), "false");
        EXPECT_EQ(json_value.at("workspace_modified").get<bool>(), true);
        EXPECT_EQ(json_value.at("semantic_scope").at("blast_radius").get<std::string>(), "medium");
        EXPECT_TRUE(json_value.at("semantic_scope").at("public_interface_changes").get<bool>());
    }

    fs::remove_all(reports_dir);
}
} // namespace
