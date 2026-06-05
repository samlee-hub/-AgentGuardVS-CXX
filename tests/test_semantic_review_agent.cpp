#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "agents/ReviewerAgent.h"
#include "llm/FakeLLMProvider.h"

namespace
{
using agentguard::BuildResult;
using agentguard::FakeLLMProvider;
using agentguard::ParsedBuildError;
using agentguard::RelevantFileContent;
using agentguard::SemanticFileReference;
using agentguard::SemanticReviewInput;
using agentguard::SemanticScopeResult;
using agentguard::TryReviewSemanticChange;

SemanticScopeResult MakeScope()
{
    SemanticScopeResult scope;
    scope.task_summary = "Add summon cooldown.";
    scope.allowed_files = {
        SemanticFileReference{"src/Summon.cpp", "Owns summon behavior.", 0.95}
    };
    scope.context_files = {
        SemanticFileReference{"src/Summon.h", "Declaration context.", 0.8}
    };
    scope.suspected_files = {
        SemanticFileReference{"src/Cooldown.cpp", "May own shared cooldown state.", 0.72}
    };
    scope.protected_files = {
        SemanticFileReference{"src/Auth.cpp", "Authentication is protected.", 1.0}
    };
    scope.needs_approval_files = {
        SemanticFileReference{"src/PublicApi.h", "Public API change needs approval.", 0.9}
    };
    scope.risk_level = "medium";
    scope.recommendation = "proceed";
    return scope;
}

SemanticReviewInput MakeInput()
{
    SemanticReviewInput input;
    input.user_request = "Add summon cooldown.";
    input.semantic_scope = MakeScope();
    input.diff_text = "diff --git a/src/Summon.cpp b/src/Summon.cpp";
    input.build_result.success = true;
    input.build_result.return_code = 0;
    input.build_result.stdout_text = "Build succeeded.";
    input.relevant_files = {
        RelevantFileContent{"src/Summon.cpp", "int cooldown = 3;\n"}
    };
    return input;
}

nlohmann::json MakeReviewJson(const std::string& next_action)
{
    return nlohmann::json{
        {"meets_requirement", next_action == "accept"},
        {"requires_scope_expansion", next_action == "expand_scope"},
        {"suggested_scope_additions", nlohmann::json::array()},
        {"risks", nlohmann::json::array()},
        {"confidence", 0.82},
        {"next_action", next_action},
        {"notes", nlohmann::json::array({"reviewed"})}
    };
}
} // namespace

TEST(SemanticReviewAgentTest, ParsesAcceptReviewFromFakeProvider)
{
    const auto review_json = MakeReviewJson("accept");
    const FakeLLMProvider provider(review_json.dump(), review_json);

    const auto result = TryReviewSemanticChange(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.review.next_action, "accept");
    EXPECT_TRUE(result.review.meets_requirement);
    EXPECT_NE(result.prompt.find("SemanticReviewResult"), std::string::npos);
    EXPECT_NE(result.prompt.find("risks:[{type,file,reason,severity}]"), std::string::npos);
    EXPECT_NE(result.prompt.find("notes:string[]"), std::string::npos);
    EXPECT_FALSE(result.raw_response.empty());
}

TEST(SemanticReviewAgentTest, SuggestsExpandScopeForSuspectedBuildError)
{
    auto input = MakeInput();
    input.build_result.success = false;
    input.parsed_errors = {
        ParsedBuildError{"src/Cooldown.cpp", 4, 1, "C2065", "missing symbol", agentguard::ErrorCategory::CompileError, ""}
    };

    auto review_json = MakeReviewJson("repair");
    const FakeLLMProvider provider(review_json.dump(), review_json);

    const auto result = TryReviewSemanticChange(input, provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.review.next_action, "expand_scope");
    ASSERT_FALSE(result.review.suggested_scope_additions.empty());
    EXPECT_EQ(result.review.suggested_scope_additions.front().path, "src/Cooldown.cpp");
}

TEST(SemanticReviewAgentTest, NeedsApprovalSuggestionForcesAskUser)
{
    auto review_json = MakeReviewJson("repair");
    review_json["suggested_scope_additions"].push_back({
        {"path", "src/PublicApi.h"},
        {"reason", "Required public API change."},
        {"confidence", 0.9}
    });
    const FakeLLMProvider provider(review_json.dump(), review_json);

    const auto result = TryReviewSemanticChange(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.review.next_action, "ask_user");
}

TEST(SemanticReviewAgentTest, ProtectedRiskForcesStop)
{
    auto review_json = MakeReviewJson("repair");
    review_json["risks"].push_back({
        {"type", "protected_file"},
        {"file", "src/Auth.cpp"},
        {"reason", "Patch would touch protected auth code."},
        {"severity", "high"}
    });
    const FakeLLMProvider provider(review_json.dump(), review_json);

    const auto result = TryReviewSemanticChange(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.review.next_action, "stop");
}

TEST(SemanticReviewAgentTest, InvalidJsonReturnsClearFailure)
{
    const FakeLLMProvider provider("not-json", nlohmann::json{});

    const auto result = TryReviewSemanticChange(MakeInput(), provider);

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("Semantic review failed"), std::string::npos);
}

TEST(SemanticReviewAgentTest, NormalizesCommonModelStringFieldsWithoutExpandingScope)
{
    const nlohmann::json provider_response{
        {"meets_requirement", false},
        {"requires_scope_expansion", true},
        {"suggested_scope_additions", nlohmann::json::array({
            "Engine/Battle.cc may need tick integration"
        })},
        {"risks", "Cooldown may diverge from engine tick timing."},
        {"confidence", 0.81},
        {"next_action", "ask_user"},
        {"notes", "User must approve tick integration before protected files are considered."}
    };
    const FakeLLMProvider provider(provider_response.dump(), provider_response);

    const auto result = TryReviewSemanticChange(MakeInput(), provider);

    ASSERT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.review.next_action, "ask_user");
    EXPECT_TRUE(result.review.suggested_scope_additions.empty());
    ASSERT_EQ(result.review.risks.size(), 1U);
    EXPECT_EQ(result.review.risks.front().type, "semantic");
    ASSERT_GE(result.review.notes.size(), 2U);
}
