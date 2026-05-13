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

    ReportWriteRequest request;
    request.reports_directory = reports_dir;
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

    {
        std::ifstream json_input(result.json_path);
        const nlohmann::json json_value = nlohmann::json::parse(json_input);
        EXPECT_EQ(json_value.at("task_spec").at("task_id").get<std::string>(), "report-task");
        EXPECT_EQ(json_value.at("patch_apply_result").at("success").get<bool>(), true);
        EXPECT_EQ(json_value.at("build_result").at("return_code").get<int>(), 1);
        EXPECT_EQ(json_value.at("repair_rounds").get<int>(), 1);
        EXPECT_EQ(json_value.at("git_diff").at("stat").get<std::string>(), "1 file changed, 2 insertions(+)");
    }

    fs::remove_all(reports_dir);
}
} // namespace
