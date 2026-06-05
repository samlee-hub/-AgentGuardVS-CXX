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

fs::path MakeRunsRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_analyze_" + std::to_string(stamp));
    fs::create_directories(root);
    return root;
}

fs::path WriteResponseFile(const nlohmann::json& response)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path path = fs::temp_directory_path() / ("agentguard_semantic_response_" + std::to_string(stamp) + ".json");
    std::ofstream output(path, std::ios::trunc);
    output << response.dump(2);
    return path;
}

nlohmann::json MakeScopeJson()
{
    return nlohmann::json{
        {"task_summary", "Add summon cooldown."},
        {"allowed_files", nlohmann::json::array({
            {{"path", "IndexingSample/Game/Summon.cpp"}, {"reason", "Owns summon behavior."}, {"confidence", 0.95}}
        })},
        {"context_files", nlohmann::json::array()},
        {"suspected_files", nlohmann::json::array()},
        {"protected_files", nlohmann::json::array({
            {{"path", "IndexingSample/Engine/Battle.cc"}, {"reason", "Battle logic is protected for this task."}, {"confidence", 0.9}}
        })},
        {"needs_approval_files", nlohmann::json::array()},
        {"risk_level", "low"},
        {"recommendation", "proceed"},
        {"missing_context", nlohmann::json::array()},
        {"notes", nlohmann::json::array()}
    };
}

fs::path FindFile(const fs::path& root, const std::string& filename)
{
    for (const auto& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.path().filename() == filename)
        {
            return entry.path();
        }
    }
    return {};
}

TEST(CliAnalyzeTest, FileProviderGeneratesSemanticScopeArtifacts)
{
    const fs::path runs_root = MakeRunsRoot();
    const fs::path response_file = WriteResponseFile(MakeScopeJson());
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"analyze",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"file",
        L"--response-file",
        response_file.wstring(),
        L"--runs-root",
        runs_root.wstring(),
        L"--force"
    };

    const auto result = ProcessRunner().Run(request);

    EXPECT_EQ(result.return_code, 0) << result.stderr_text;
    EXPECT_NE(result.stdout_text.find("allowed=1"), std::string::npos);
    EXPECT_NE(result.stdout_text.find("protected=1"), std::string::npos);

    const auto semantic_scope = FindFile(runs_root, "semantic_scope.json");
    const auto prompt = FindFile(runs_root, "semantic_analyze_prompt.txt");
    const auto raw_response = FindFile(runs_root, "semantic_analyze_raw_response.json");
    ASSERT_FALSE(semantic_scope.empty());
    EXPECT_FALSE(prompt.empty());
    EXPECT_FALSE(raw_response.empty());

    {
        std::ifstream scope_file(semantic_scope);
        const auto scope_json = nlohmann::json::parse(scope_file);
        EXPECT_EQ(scope_json.at("allowed_files").size(), 1U);
        EXPECT_EQ(scope_json.at("protected_files").size(), 1U);
    }

    fs::remove(response_file);
    fs::remove_all(runs_root);
}

TEST(CliAnalyzeTest, InvalidProviderJsonFailsClearly)
{
    const fs::path runs_root = MakeRunsRoot();
    const fs::path response_file = WriteResponseFile(nlohmann::json{{"invalid", true}});
    const fs::path solution = fs::path(MINI_VS_PROJECT) / "Mini.sln";

    ProcessRequest request;
    request.executable = fs::path(AGENTGUARD_EXE).wstring();
    request.arguments = {
        L"analyze",
        L"--solution",
        solution.wstring(),
        L"--task",
        L"Add summon cooldown",
        L"--provider",
        L"file",
        L"--response-file",
        response_file.wstring(),
        L"--runs-root",
        runs_root.wstring(),
        L"--force"
    };

    const auto result = ProcessRunner().Run(request);

    EXPECT_NE(result.return_code, 0);
    EXPECT_NE(result.stderr_text.find("Semantic analysis failed"), std::string::npos);

    fs::remove(response_file);
    fs::remove_all(runs_root);
}
} // namespace
