#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::ProcessRequest;
using agentguard::ProcessRunner;

fs::path MakeWorkspaceRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_review_" + std::to_string(stamp));
    fs::create_directories(root / "repo" / "src");
    fs::create_directories(root / "logs");
    return root;
}

void WriteFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << content;
}

nlohmann::json MakeScopeJson()
{
    return nlohmann::json{
        {"task_summary", "Add summon cooldown."},
        {"allowed_files", nlohmann::json::array({
            {{"path", "src/Summon.cpp"}, {"reason", "Owns summon behavior."}, {"confidence", 0.95}}
        })},
        {"context_files", nlohmann::json::array()},
        {"suspected_files", nlohmann::json::array()},
        {"protected_files", nlohmann::json::array()},
        {"needs_approval_files", nlohmann::json::array()},
        {"risk_level", "low"},
        {"recommendation", "proceed"},
        {"missing_context", nlohmann::json::array()},
        {"notes", nlohmann::json::array()}
    };
}

nlohmann::json MakeReviewJson()
{
    return nlohmann::json{
        {"meets_requirement", true},
        {"requires_scope_expansion", false},
        {"suggested_scope_additions", nlohmann::json::array()},
        {"risks", nlohmann::json::array()},
        {"confidence", 0.9},
        {"next_action", "accept"},
        {"notes", nlohmann::json::array()}
    };
}

fs::path WriteResponseFile(const nlohmann::json& response)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path path = fs::temp_directory_path() / ("agentguard_review_response_" + std::to_string(stamp) + ".json");
    WriteFile(path, response.dump(2));
    return path;
}
} // namespace

TEST(CliReviewTest, FileProviderWritesReviewArtifactsAndJsonSummary)
{
    const fs::path run_root = MakeWorkspaceRoot();
    const fs::path response_file = WriteResponseFile(MakeReviewJson());
    WriteFile(run_root / "semantic_scope.json", MakeScopeJson().dump(2));
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int cooldown = 3;\n");

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"review",
        L"--workspace",
        (run_root / "repo").wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"file",
        L"--response-file",
        response_file.wstring(),
        L"--json"
    };

    const auto result = ProcessRunner().Run(request);

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    const auto summary = nlohmann::json::parse(result.stdout_text);
    EXPECT_EQ(summary.at("next_action").get<std::string>(), "accept");
    EXPECT_TRUE(fs::exists(run_root / "semantic_review.json"));
    EXPECT_TRUE(fs::exists(run_root / "semantic_review_prompt.txt"));
    EXPECT_TRUE(fs::exists(run_root / "semantic_review_raw_response.json"));

    fs::remove(response_file);
    fs::remove_all(run_root);
}

TEST(CliReviewTest, InvalidProviderJsonFailsWithoutDeletingWorkspace)
{
    const fs::path run_root = MakeWorkspaceRoot();
    const fs::path response_file = WriteResponseFile(nlohmann::json{{"invalid", true}});
    WriteFile(run_root / "semantic_scope.json", MakeScopeJson().dump(2));
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int cooldown = 3;\n");

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"review",
        L"--workspace",
        (run_root / "repo").wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"file",
        L"--response-file",
        response_file.wstring()
    };

    const auto result = ProcessRunner().Run(request);

    EXPECT_NE(result.return_code, 0);
    EXPECT_NE(result.stderr_text.find("Semantic review failed"), std::string::npos);
    EXPECT_TRUE(fs::exists(run_root / "repo" / "src" / "Summon.cpp"));

    fs::remove(response_file);
    fs::remove_all(run_root);
}
