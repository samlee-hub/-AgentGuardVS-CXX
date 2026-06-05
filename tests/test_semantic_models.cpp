#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/SemanticModels.h"

namespace
{
using agentguard::SemanticReviewResult;
using agentguard::SemanticScopeResult;

nlohmann::json MakeValidScopeJson()
{
    return nlohmann::json{
        {"task_summary", "Add summon cooldown."},
        {"allowed_files", nlohmann::json::array({
            {{"path", "src/Summon.cpp"}, {"reason", "Contains summon implementation."}, {"confidence", 0.95}}
        })},
        {"context_files", nlohmann::json::array({
            {{"path", "src/Summon.h"}, {"reason", "Declares summon API."}, {"confidence", 0.8}}
        })},
        {"suspected_files", nlohmann::json::array()},
        {"protected_files", nlohmann::json::array({
            {{"path", "src/Auth.cpp"}, {"reason", "Authentication is unrelated."}, {"confidence", 1.0}}
        })},
        {"needs_approval_files", nlohmann::json::array()},
        {"risk_level", "medium"},
        {"recommendation", "proceed"},
        {"related_symbols", nlohmann::json::array({"function:CanSummon@src/Summon.cpp"})},
        {"dependency_reasons", nlohmann::json::array({"src/Summon.h is included by src/Summon.cpp"})},
        {"blast_radius", "medium"},
        {"public_interface_changes", true},
        {"missing_context", nlohmann::json::array({"resource config"})},
        {"notes", nlohmann::json::array({"Review build output after changes."})}
    };
}

TEST(SemanticModelsTest, SemanticScopeResultRoundTripsValidJson)
{
    const SemanticScopeResult result = MakeValidScopeJson().get<SemanticScopeResult>();
    const nlohmann::json serialized = result;

    EXPECT_EQ(serialized.at("task_summary").get<std::string>(), "Add summon cooldown.");
    EXPECT_EQ(serialized.at("allowed_files").at(0).at("path").get<std::string>(), "src/Summon.cpp");
    EXPECT_EQ(serialized.at("risk_level").get<std::string>(), "medium");
    EXPECT_EQ(serialized.at("recommendation").get<std::string>(), "proceed");
    EXPECT_EQ(serialized.at("blast_radius").get<std::string>(), "medium");
    EXPECT_TRUE(serialized.at("public_interface_changes").get<bool>());
    EXPECT_EQ(serialized.at("related_symbols").at(0).get<std::string>(), "function:CanSummon@src/Summon.cpp");
}

TEST(SemanticModelsTest, SemanticScopeResultRejectsMissingRequiredField)
{
    auto json_value = MakeValidScopeJson();
    json_value.erase("task_summary");

    EXPECT_THROW((void)json_value.get<SemanticScopeResult>(), nlohmann::json::exception);
}

TEST(SemanticModelsTest, SemanticScopeResultRejectsInvalidConfidence)
{
    auto json_value = MakeValidScopeJson();
    json_value["allowed_files"][0]["confidence"] = 1.25;

    EXPECT_THROW((void)json_value.get<SemanticScopeResult>(), std::invalid_argument);
}

TEST(SemanticModelsTest, SemanticScopeResultRejectsProtectedAllowedConflict)
{
    auto json_value = MakeValidScopeJson();
    json_value["protected_files"].push_back({
        {"path", "src/Summon.cpp"},
        {"reason", "Conflict with allowed file."},
        {"confidence", 0.9}
    });

    EXPECT_THROW((void)json_value.get<SemanticScopeResult>(), std::invalid_argument);
}

TEST(SemanticModelsTest, SemanticScopeResultRejectsUnsupportedRecommendation)
{
    auto json_value = MakeValidScopeJson();
    json_value["recommendation"] = "continue_anyway";

    EXPECT_THROW((void)json_value.get<SemanticScopeResult>(), std::invalid_argument);
}

TEST(SemanticModelsTest, SemanticFileReferenceNormalizesBackslashPaths)
{
    const nlohmann::json json_value{
        {"path", "IndexingSample\\Game\\Summon.cpp"},
        {"reason", "Windows style path."},
        {"confidence", 0.9}
    };

    const auto file = json_value.get<agentguard::SemanticFileReference>();

    EXPECT_EQ(file.path, "IndexingSample/Game/Summon.cpp");
}

TEST(SemanticModelsTest, SemanticReviewResultRoundTripsValidJson)
{
    const nlohmann::json json_value{
        {"meets_requirement", false},
        {"requires_scope_expansion", true},
        {"suggested_scope_additions", nlohmann::json::array({
            {{"path", "src/Cooldown.cpp"}, {"reason", "Owns cooldown state."}, {"confidence", 0.7}}
        })},
        {"risks", nlohmann::json::array({
            {{"type", "scope"}, {"file", "src/Auth.cpp"}, {"reason", "Protected file referenced."}, {"severity", "high"}}
        })},
        {"confidence", 0.66},
        {"next_action", "expand_scope"},
        {"notes", nlohmann::json::array({"Need one additional file."})}
    };

    const SemanticReviewResult result = json_value.get<SemanticReviewResult>();
    const nlohmann::json serialized = result;

    EXPECT_TRUE(serialized.at("requires_scope_expansion").get<bool>());
    EXPECT_EQ(serialized.at("suggested_scope_additions").at(0).at("path").get<std::string>(), "src/Cooldown.cpp");
    EXPECT_EQ(serialized.at("next_action").get<std::string>(), "expand_scope");
}

TEST(SemanticModelsTest, SemanticReviewResultRejectsUnsupportedNextAction)
{
    nlohmann::json json_value{
        {"meets_requirement", false},
        {"requires_scope_expansion", false},
        {"suggested_scope_additions", nlohmann::json::array()},
        {"risks", nlohmann::json::array()},
        {"confidence", 0.5},
        {"next_action", "retry_forever"},
        {"notes", nlohmann::json::array()}
    };

    EXPECT_THROW((void)json_value.get<SemanticReviewResult>(), std::invalid_argument);
}
} // namespace
