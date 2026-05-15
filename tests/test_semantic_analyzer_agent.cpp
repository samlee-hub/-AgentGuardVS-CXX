#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/SemanticAnalyzerAgent.h"
#include "llm/FakeLLMProvider.h"

namespace
{
using agentguard::ContextCandidate;
using agentguard::FakeLLMProvider;
using agentguard::RelevantFileContent;
using agentguard::SemanticAnalyzerInput;
using agentguard::SourceFileInfo;
using agentguard::TryAnalyzeSemanticScope;

nlohmann::json MakeScopeJson()
{
    return nlohmann::json{
        {"task_summary", "Add summon cooldown."},
        {"allowed_files", nlohmann::json::array({
            {{"path", "Game/Summon.cpp"}, {"reason", "Owns summon logic."}, {"confidence", 0.95}}
        })},
        {"context_files", nlohmann::json::array({
            {{"path", "Game/Summon.h"}, {"reason", "Declares summon API."}, {"confidence", 0.8}}
        })},
        {"suspected_files", nlohmann::json::array()},
        {"protected_files", nlohmann::json::array({
            {{"path", "Auth/Login.cpp"}, {"reason", "Unrelated auth path."}, {"confidence", 1.0}}
        })},
        {"needs_approval_files", nlohmann::json::array()},
        {"risk_level", "low"},
        {"recommendation", "proceed"},
        {"related_symbols", nlohmann::json::array({"function:CanSummon@Game/Summon.cpp"})},
        {"dependency_reasons", nlohmann::json::array({"Game/Summon.h included by Game/Summon.cpp"})},
        {"blast_radius", "medium"},
        {"public_interface_changes", true},
        {"missing_context", nlohmann::json::array()},
        {"notes", nlohmann::json::array({"Use build verification later."})}
    };
}

SemanticAnalyzerInput MakeInput()
{
    SemanticAnalyzerInput input;
    input.user_request = "Add summon cooldown.";
    input.solution_path = "Mini.sln";
    input.source_files = {
        SourceFileInfo{
            "Game/Summon.cpp",
            {"Summon.h"},
            {"SummonService"},
            {},
            {},
            {"CanSummon", "ApplyCooldown"},
            {"summon", "cooldown"}
        }
    };
    input.context_candidates = {
        ContextCandidate{"Game/Summon.cpp", {"filename:summon", "keyword:cooldown"}, 9}
    };
    input.static_allowed_files = {"Game/Summon.cpp"};
    input.static_forbidden_files = {"Auth/Login.cpp"};
    input.impact.related_symbols = {"function:CanSummon@Game/Summon.cpp"};
    input.impact.reverse_include_dependents = {"Game/Summon.cpp"};
    input.impact.likely_tests = {"Game/SummonTests.cpp"};
    input.impact.public_header_risk = true;
    input.impact.changed_header_blast_radius = "medium";
    input.impact.blast_radius = "medium";
    input.impact.public_interface_changes = true;
    input.impact.dependency_reasons = {"Game/Summon.h included by Game/Summon.cpp"};
    input.relevant_files = {
        RelevantFileContent{"Game/Summon.cpp", std::string(1500, 'x')}
    };
    return input;
}

TEST(SemanticAnalyzerAgentTest, ParsesFixedScopeFromFakeProvider)
{
    const FakeLLMProvider provider(MakeScopeJson().dump(), MakeScopeJson());

    const auto result = TryAnalyzeSemanticScope(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.scope.allowed_files.at(0).path, "Game/Summon.cpp");
    EXPECT_NE(result.prompt.find("Return strict JSON only"), std::string::npos);
    EXPECT_NE(result.prompt.find("truncated=true"), std::string::npos);
    EXPECT_NE(result.prompt.find("related_symbols"), std::string::npos);
    EXPECT_NE(result.prompt.find("reverse_include_dependents"), std::string::npos);
    EXPECT_NE(result.prompt.find("public_header_risk=true"), std::string::npos);
    EXPECT_EQ(result.scope.blast_radius, "medium");
    EXPECT_TRUE(result.scope.public_interface_changes);
    EXPECT_FALSE(result.raw_response.empty());
}

TEST(SemanticAnalyzerAgentTest, ExtractsDeepSeekStyleMessageContent)
{
    const nlohmann::json provider_response{
        {"choices", nlohmann::json::array({
            {
                {"message", {
                    {"role", "assistant"},
                    {"content", MakeScopeJson().dump()}
                }}
            }
        })}
    };
    const FakeLLMProvider provider(provider_response.dump(), provider_response);

    const auto result = TryAnalyzeSemanticScope(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.scope.risk_level, "medium");
}

TEST(SemanticAnalyzerAgentTest, InvalidJsonReturnsClearFailure)
{
    const FakeLLMProvider provider("not-json", nlohmann::json{});

    const auto result = TryAnalyzeSemanticScope(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("Semantic analysis failed"), std::string::npos);
}
} // namespace
