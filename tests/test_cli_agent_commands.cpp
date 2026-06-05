#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

fs::path MakeRunsRoot(const std::string& prefix)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / (prefix + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

agentguard::ProcessResult RunCommand(std::vector<std::wstring> arguments)
{
    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = std::move(arguments);
    return ProcessRunner().Run(request);
}

void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}
} // namespace

TEST(CliAgentCommandsTest, HelpIsAvailableForAgentFacingCommands)
{
    for (const std::wstring command : {L"analyze", L"verify", L"review", L"run", L"detect", L"verify-profile", L"doctor"})
    {
        const auto result = RunCommand({command, L"--help"});

        EXPECT_EQ(result.return_code, 0) << result.stderr_text;
        EXPECT_NE(result.stdout_text.find("--json"), std::string::npos);
    }
}

TEST(CliAgentCommandsTest, RunDryRunJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_run_json_");
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    const auto result = RunCommand({
        L"run",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--runs-root",
        runs_root.wstring(),
        L"--dry-run",
        L"--provider",
        L"fake",
        L"--force",
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("mode").get<std::string>(), "dry_run");
    EXPECT_TRUE(summary.contains("workspace"));
    EXPECT_EQ(summary.at("runs_root_source").get<std::string>(), "user_specified");

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, AnalyzeJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_analyze_json_");
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    const auto result = RunCommand({
        L"analyze",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"fake",
        L"--runs-root",
        runs_root.wstring(),
        L"--force",
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_TRUE(summary.contains("semantic_scope"));
    EXPECT_TRUE(summary.contains("report"));
    EXPECT_GE(summary.at("counts").at("allowed").get<int>(), 0);
    EXPECT_EQ(summary.at("runs_root_source").get<std::string>(), "user_specified");

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, VerifyJsonOutputsMachineReadableSummary)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_verify_json_");
    const fs::path workspace = runs_root / "repo";
    fs::create_directories(workspace);
    fs::copy(
        fs::path(MINI_VS_PROJECT),
        workspace,
        fs::copy_options::recursive | fs::copy_options::overwrite_existing);

    const auto result = RunCommand({
        L"verify",
        L"--workspace",
        workspace.wstring(),
        L"--configuration",
        L"Debug",
        L"--platform",
        L"x64",
        L"--json"
    });

    EXPECT_TRUE(result.return_code == 0 || result.return_code == 1);
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.contains("ok"));
    EXPECT_TRUE(summary.contains("build"));
    EXPECT_TRUE(summary.at("artifacts").contains("log"));

    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, AnalyzeJsonFailureUsesStableErrorEnvelope)
{
    const auto result = RunCommand({
        L"analyze",
        L"--solution",
        L"Z:\\does-not-exist\\Missing.sln",
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"fake",
        L"--json"
    });

    EXPECT_NE(result.return_code, 0);
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_FALSE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("error_code").get<std::string>(), "SOLUTION_PARSE_FAILED");
    EXPECT_TRUE(summary.contains("suggestion"));
}

TEST(CliAgentCommandsTest, DetectJsonOutputsGenericProjectProfile)
{
    const fs::path project = MakeRunsRoot("agentguard_detect_node_");
    WriteText(project / "package.json", R"({"scripts":{"build":"vite build","test":"vitest"}})");

    const auto result = RunCommand({
        L"detect",
        L"--project",
        project.wstring(),
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("command").get<std::string>(), "detect");
    EXPECT_EQ(summary.at("profile").at("project_kind").get<std::string>(), "node");
    EXPECT_EQ(summary.at("profile").at("adapter").get<std::string>(), "node");

    fs::remove_all(project);
}

TEST(CliAgentCommandsTest, VerifyProfileJsonRunsConfiguredCommands)
{
    const fs::path project = MakeRunsRoot("agentguard_verify_profile_cli_");
    WriteText(
        project / "agentguard.json",
        R"({
  "project_kind": "custom",
  "languages": ["python"],
  "verify_commands": [
    {
      "name": "unit",
      "command": "C:\\Windows\\System32\\cmd.exe",
      "args": ["/C", "echo cli-profile-ok"],
      "working_directory": ".",
      "required": true,
      "skip_if_missing_executable": false
    }
  ]
})");

    const auto result = RunCommand({
        L"verify-profile",
        L"--project",
        project.wstring(),
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("command").get<std::string>(), "verify-profile");
    EXPECT_EQ(summary.at("profile").at("project_kind").get<std::string>(), "custom");
    ASSERT_EQ(summary.at("verification").at("steps").size(), 1U);
    EXPECT_TRUE(summary.at("verification").at("steps").at(0).at("success").get<bool>());
    EXPECT_EQ(summary.at("audit").at("profile_source").get<std::string>(), "custom");

    fs::remove_all(project);
}

TEST(CliAgentCommandsTest, AnalyzeProjectJsonOutputsProfileAudit)
{
    const fs::path runs_root = MakeRunsRoot("agentguard_analyze_project_json_");
    const fs::path project = MakeRunsRoot("agentguard_python_project_");
    WriteText(project / "pyproject.toml", "[project]\nname='demo'");
    WriteText(project / "src" / "service.py", "def add(a, b):\n    return a + b\n");

    const auto result = RunCommand({
        L"analyze",
        L"--project",
        project.wstring(),
        L"--task",
        L"Change service add behavior",
        L"--provider",
        L"fake",
        L"--runs-root",
        runs_root.wstring(),
        L"--force",
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("profile").at("project_kind").get<std::string>(), "python");
    EXPECT_GE(summary.at("audit").at("indexed_files_count").get<int>(), 2);

    fs::remove_all(project);
    fs::remove_all(runs_root);
}

TEST(CliAgentCommandsTest, VerifyProjectJsonUsesCommandVerifier)
{
    const fs::path project = MakeRunsRoot("agentguard_verify_project_cli_");
    WriteText(
        project / "agentguard.json",
        R"({
  "project_kind": "custom",
  "verify_commands": [
    {
      "name": "missing-optional",
      "command": "definitely-missing-agentguard-tool.exe",
      "args": [],
      "working_directory": ".",
      "required": true,
      "skip_if_missing_executable": true
    }
  ]
})");

    const auto result = RunCommand({
        L"verify",
        L"--project",
        project.wstring(),
        L"--json"
    });

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_TRUE(summary.at("ok").get<bool>());
    EXPECT_EQ(summary.at("mode").get<std::string>(), "project-profile");
    EXPECT_EQ(summary.at("verification").at("steps").at(0).at("status").get<std::string>(), "SKIP");

    fs::remove_all(project);
}
