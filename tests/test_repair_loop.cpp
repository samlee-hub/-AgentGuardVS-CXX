#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/RepairLoop.h"
#include "core/SemanticModels.h"
#include "llm/FakeLLMProvider.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::BuildResult;
using agentguard::FakeLLMProvider;
using agentguard::RepairLoopInput;
using agentguard::RunRepairLoop;
using agentguard::SemanticFileReference;
using agentguard::SemanticScopeResult;

fs::path MakeRunRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_repair_" + std::to_string(stamp));
    fs::create_directories(root / "repo" / "src");
    fs::create_directories(root / "logs");
    fs::create_directories(root / "reports");
    return root;
}

void WriteFile(const fs::path& path, const std::string& content)
{
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

SemanticScopeResult MakeSemanticScope()
{
    SemanticScopeResult scope;
    scope.task_summary = "Add summon cooldown.";
    scope.allowed_files = {
        SemanticFileReference{"src/Summon.cpp", "Owns summon behavior.", 0.95}
    };
    scope.suspected_files = {
        SemanticFileReference{"src/Cooldown.cpp", "Shared cooldown state.", 0.8}
    };
    scope.protected_files = {
        SemanticFileReference{"src/Auth.cpp", "Protected auth code.", 1.0}
    };
    scope.risk_level = "medium";
    scope.recommendation = "proceed";
    return scope;
}

nlohmann::json MakeReviewJson(
    const std::string& next_action,
    const std::string& suggested_path = {})
{
    nlohmann::json additions = nlohmann::json::array();
    if (!suggested_path.empty())
    {
        additions.push_back({
            {"path", suggested_path},
            {"reason", "Review requested scope change."},
            {"confidence", 0.9}
        });
    }

    return nlohmann::json{
        {"meets_requirement", next_action == "accept"},
        {"requires_scope_expansion", next_action == "expand_scope"},
        {"suggested_scope_additions", additions},
        {"risks", nlohmann::json::array()},
        {"confidence", 0.8},
        {"next_action", next_action},
        {"notes", nlohmann::json::array()}
    };
}

TEST(RepairLoopTest, AppliesInitialPatchThenBuildFixPatchUntilBuildSucceeds)
{
    const fs::path run_root = MakeRunRoot();
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int cooldown = 0;\n");

    FakeLLMProvider planner_provider(
        "",
        nlohmann::json{
            {"task_id", "repair-001"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Summon.cpp"})},
            {"forbidden_files", nlohmann::json::array({"src/Auth.cpp"})},
            {"acceptance_criteria", nlohmann::json::array({"Build succeeds."})},
            {"max_repair_rounds", 2}
        });

    FakeLLMProvider implementer_provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Summon.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int cooldown = 0;"},
                    {"replacement_snippet", "int cooldown = missingSymbol;"},
                    {"explanation", "Initial implementation."}
                }
            })}
        });

    FakeLLMProvider fixer_provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Summon.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int cooldown = missingSymbol;"},
                    {"replacement_snippet", "int cooldown = 3;"},
                    {"explanation", "Fix compile error."}
                }
            })}
        });

    int build_calls = 0;
    auto build_executor = [&build_calls](const agentguard::TaskSpec&, int) {
        ++build_calls;
        BuildResult result;
        result.command = "mock msbuild";
        result.duration_ms = 1;
        if (build_calls == 1)
        {
            result.success = false;
            result.return_code = 1;
            result.stdout_text = R"(src\Summon.cpp(1,16): error C2065: 'missingSymbol': undeclared identifier)";
        }
        else
        {
            result.success = true;
            result.return_code = 0;
            result.stdout_text = "Build succeeded.";
        }
        return result;
    };

    RepairLoopInput input;
    input.repo_root = run_root / "repo";
    input.logs_directory = run_root / "logs";
    input.reports_directory = run_root / "reports";
    input.planner_input.user_request = "Add summon cooldown.";
    input.planner_input.project_info.solution_path = "Server.sln";
    input.initial_relevant_files = {
        agentguard::RelevantFileContent{"src/Summon.cpp", "int cooldown = 0;\n"}
    };
    input.git_diff_summary = "diff summary";
    input.planner_provider = &planner_provider;
    input.implementer_provider = &implementer_provider;
    input.build_fixer_provider = &fixer_provider;
    input.build_executor = build_executor;

    const auto result = RunRepairLoop(input);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(build_calls, 2);
    EXPECT_EQ(result.report.repair_rounds, 1);
    EXPECT_EQ(ReadFile(run_root / "repo" / "src" / "Summon.cpp"), "int cooldown = 3;\n");
    EXPECT_TRUE(fs::exists(run_root / "logs" / "build_round_0.log"));
    EXPECT_TRUE(fs::exists(run_root / "logs" / "build_round_1.log"));
    EXPECT_TRUE(fs::exists(run_root / "reports" / "patch_round_0.json"));
    EXPECT_TRUE(fs::exists(run_root / "reports" / "patch_round_1.json"));

    fs::remove_all(run_root);
}

TEST(RepairLoopTest, ExpandsScopeWhenSemanticReviewRequestsSuspectedFile)
{
    const fs::path run_root = MakeRunRoot();
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int summon = 0;\n");
    WriteFile(run_root / "repo" / "src" / "Cooldown.cpp", "int cooldown = 0;\n");

    FakeLLMProvider planner_provider(
        "",
        nlohmann::json{
            {"task_id", "repair-expand-001"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Legacy.cpp"})},
            {"forbidden_files", nlohmann::json::array()},
            {"acceptance_criteria", nlohmann::json::array({"Build succeeds."})},
            {"max_repair_rounds", 2}
        });

    FakeLLMProvider implementer_provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Summon.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int summon = 0;"},
                    {"replacement_snippet", "int summon = missingSymbol;"},
                    {"explanation", "Initial implementation."}
                }
            })}
        });

    FakeLLMProvider fixer_provider(
        "",
        nlohmann::json{
            {"changes", nlohmann::json::array({
                {
                    {"target_file", "src/Cooldown.cpp"},
                    {"change_type", "modify"},
                    {"original_snippet", "int cooldown = 0;"},
                    {"replacement_snippet", "int cooldown = 3;"},
                    {"explanation", "Repair in expanded scope."}
                }
            })}
        });

    const auto expand_review_json = MakeReviewJson("expand_scope", "src/Cooldown.cpp");
    FakeLLMProvider review_provider(expand_review_json.dump(), expand_review_json);

    int build_calls = 0;
    auto build_executor = [&build_calls](const agentguard::TaskSpec&, int) {
        ++build_calls;
        BuildResult result;
        result.command = "mock msbuild";
        result.duration_ms = 1;
        if (build_calls == 1)
        {
            result.success = false;
            result.return_code = 1;
            result.stdout_text = R"(src\Cooldown.cpp(1,5): error C2065: 'missingSymbol': undeclared identifier)";
        }
        else
        {
            result.success = true;
            result.return_code = 0;
            result.stdout_text = "Build succeeded.";
        }
        return result;
    };

    RepairLoopInput input;
    input.repo_root = run_root / "repo";
    input.logs_directory = run_root / "logs";
    input.reports_directory = run_root / "reports";
    input.planner_input.user_request = "Add summon cooldown.";
    input.planner_input.project_info.solution_path = "Server.sln";
    input.planner_input.has_semantic_scope = true;
    input.planner_input.semantic_scope = MakeSemanticScope();
    input.initial_relevant_files = {
        agentguard::RelevantFileContent{"src/Summon.cpp", "int summon = 0;\n"},
        agentguard::RelevantFileContent{"src/Cooldown.cpp", "int cooldown = 0;\n"}
    };
    input.planner_provider = &planner_provider;
    input.implementer_provider = &implementer_provider;
    input.build_fixer_provider = &fixer_provider;
    input.semantic_review_provider = &review_provider;
    input.max_scope_expansions = 1;
    input.build_executor = build_executor;

    const auto result = RunRepairLoop(input);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(build_calls, 2);
    EXPECT_EQ(ReadFile(run_root / "repo" / "src" / "Cooldown.cpp"), "int cooldown = 3;\n");
    EXPECT_TRUE(fs::exists(run_root / "reports" / "scope_change_round_0.json"));
    EXPECT_TRUE(fs::exists(run_root / "reports" / "patch_round_0.json"));
    EXPECT_TRUE(fs::exists(run_root / "reports" / "patch_round_1.json"));

    fs::remove_all(run_root);
}

TEST(RepairLoopTest, StopsWhenScopeExpansionLimitIsExceeded)
{
    const fs::path run_root = MakeRunRoot();
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int summon = 0;\n");
    WriteFile(run_root / "repo" / "src" / "Cooldown.cpp", "int cooldown = 0;\n");

    FakeLLMProvider planner_provider(
        "",
        nlohmann::json{
            {"task_id", "repair-expand-limit"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Summon.cpp"})},
            {"forbidden_files", nlohmann::json::array()},
            {"acceptance_criteria", nlohmann::json::array({"Build succeeds."})},
            {"max_repair_rounds", 2}
        });
    FakeLLMProvider implementer_provider(
        "",
        nlohmann::json{{"changes", nlohmann::json::array({
            {
                {"target_file", "src/Summon.cpp"},
                {"change_type", "modify"},
                {"original_snippet", "int summon = 0;"},
                {"replacement_snippet", "int summon = missingSymbol;"},
                {"explanation", "Initial implementation."}
            }
        })}});
    FakeLLMProvider fixer_provider("", nlohmann::json{{"changes", nlohmann::json::array()}});
    const auto expand_review_json = MakeReviewJson("expand_scope", "src/Cooldown.cpp");
    FakeLLMProvider review_provider(expand_review_json.dump(), expand_review_json);

    RepairLoopInput input;
    input.repo_root = run_root / "repo";
    input.logs_directory = run_root / "logs";
    input.reports_directory = run_root / "reports";
    input.planner_input.user_request = "Add summon cooldown.";
    input.planner_input.project_info.solution_path = "Server.sln";
    input.planner_provider = &planner_provider;
    input.implementer_provider = &implementer_provider;
    input.build_fixer_provider = &fixer_provider;
    input.semantic_review_provider = &review_provider;
    input.max_scope_expansions = 0;
    input.initial_relevant_files = {
        agentguard::RelevantFileContent{"src/Summon.cpp", "int summon = 0;\n"}
    };
    input.build_executor = [](const agentguard::TaskSpec&, int) {
        BuildResult result;
        result.success = false;
        result.return_code = 1;
        result.stdout_text = R"(src\Cooldown.cpp(1,5): error C2065: 'missingSymbol': undeclared identifier)";
        return result;
    };

    const auto result = RunRepairLoop(input);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("max_scope_expansions"), std::string::npos);

    fs::remove_all(run_root);
}

TEST(RepairLoopTest, StopsWhenSemanticReviewRequestsUserApproval)
{
    const fs::path run_root = MakeRunRoot();
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int summon = 0;\n");

    FakeLLMProvider planner_provider(
        "",
        nlohmann::json{
            {"task_id", "repair-approval"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Summon.cpp"})},
            {"forbidden_files", nlohmann::json::array()},
            {"acceptance_criteria", nlohmann::json::array({"Build succeeds."})},
            {"max_repair_rounds", 2}
        });
    FakeLLMProvider implementer_provider(
        "",
        nlohmann::json{{"changes", nlohmann::json::array({
            {
                {"target_file", "src/Summon.cpp"},
                {"change_type", "modify"},
                {"original_snippet", "int summon = 0;"},
                {"replacement_snippet", "int summon = missingSymbol;"},
                {"explanation", "Initial implementation."}
            }
        })}});
    FakeLLMProvider fixer_provider("", nlohmann::json{{"changes", nlohmann::json::array()}});
    const auto approval_review_json = MakeReviewJson("ask_user", "src/PublicApi.h");
    FakeLLMProvider review_provider(approval_review_json.dump(), approval_review_json);

    RepairLoopInput input;
    input.repo_root = run_root / "repo";
    input.logs_directory = run_root / "logs";
    input.reports_directory = run_root / "reports";
    input.planner_input.user_request = "Add summon cooldown.";
    input.planner_input.project_info.solution_path = "Server.sln";
    input.planner_provider = &planner_provider;
    input.implementer_provider = &implementer_provider;
    input.build_fixer_provider = &fixer_provider;
    input.semantic_review_provider = &review_provider;
    input.initial_relevant_files = {
        agentguard::RelevantFileContent{"src/Summon.cpp", "int summon = 0;\n"}
    };
    input.build_executor = [](const agentguard::TaskSpec&, int) {
        BuildResult result;
        result.success = false;
        result.return_code = 1;
        result.stdout_text = R"(src\PublicApi.h(1,5): error C2065: 'missingSymbol': undeclared identifier)";
        return result;
    };

    const auto result = RunRepairLoop(input);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("requires user approval"), std::string::npos);
    EXPECT_FALSE(fs::exists(run_root / "reports" / "patch_round_1.json"));

    fs::remove_all(run_root);
}

TEST(RepairLoopTest, StopsWhenSemanticReviewSuggestsProtectedFile)
{
    const fs::path run_root = MakeRunRoot();
    WriteFile(run_root / "repo" / "src" / "Summon.cpp", "int summon = 0;\n");
    WriteFile(run_root / "repo" / "src" / "Auth.cpp", "int auth = 0;\n");

    FakeLLMProvider planner_provider(
        "",
        nlohmann::json{
            {"task_id", "repair-protected"},
            {"user_request", "Add summon cooldown."},
            {"target_solution", "Server.sln"},
            {"target_project", "Server"},
            {"allowed_files", nlohmann::json::array({"src/Summon.cpp"})},
            {"forbidden_files", nlohmann::json::array()},
            {"acceptance_criteria", nlohmann::json::array({"Build succeeds."})},
            {"max_repair_rounds", 2}
        });
    FakeLLMProvider implementer_provider(
        "",
        nlohmann::json{{"changes", nlohmann::json::array({
            {
                {"target_file", "src/Summon.cpp"},
                {"change_type", "modify"},
                {"original_snippet", "int summon = 0;"},
                {"replacement_snippet", "int summon = missingSymbol;"},
                {"explanation", "Initial implementation."}
            }
        })}});
    FakeLLMProvider fixer_provider("", nlohmann::json{{"changes", nlohmann::json::array()}});
    const auto protected_review_json = MakeReviewJson("expand_scope", "src/Auth.cpp");
    FakeLLMProvider review_provider(protected_review_json.dump(), protected_review_json);

    RepairLoopInput input;
    input.repo_root = run_root / "repo";
    input.logs_directory = run_root / "logs";
    input.reports_directory = run_root / "reports";
    input.planner_input.user_request = "Add summon cooldown.";
    input.planner_input.project_info.solution_path = "Server.sln";
    input.planner_input.has_semantic_scope = true;
    input.planner_input.semantic_scope = MakeSemanticScope();
    input.planner_provider = &planner_provider;
    input.implementer_provider = &implementer_provider;
    input.build_fixer_provider = &fixer_provider;
    input.semantic_review_provider = &review_provider;
    input.initial_relevant_files = {
        agentguard::RelevantFileContent{"src/Summon.cpp", "int summon = 0;\n"},
        agentguard::RelevantFileContent{"src/Auth.cpp", "int auth = 0;\n"}
    };
    input.build_executor = [](const agentguard::TaskSpec&, int) {
        BuildResult result;
        result.success = false;
        result.return_code = 1;
        result.stdout_text = R"(src\Auth.cpp(1,5): error C2065: 'missingSymbol': undeclared identifier)";
        return result;
    };

    const auto result = RunRepairLoop(input);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("stopped repair"), std::string::npos);
    EXPECT_FALSE(fs::exists(run_root / "reports" / "scope_change_round_0.json"));
    EXPECT_EQ(ReadFile(run_root / "repo" / "src" / "Auth.cpp"), "int auth = 0;\n");

    fs::remove_all(run_root);
}
} // namespace
