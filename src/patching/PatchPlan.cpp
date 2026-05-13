#include "patching/PatchPlan.h"

#include <nlohmann/json.hpp>

namespace agentguard
{
void to_json(nlohmann::json& json_value, const FileChange& change)
{
    json_value = nlohmann::json{
        {"target_file", change.target_file},
        {"change_type", change.change_type},
        {"original_snippet", change.original_snippet},
        {"replacement_snippet", change.replacement_snippet},
        {"explanation", change.explanation}
    };
}

void from_json(const nlohmann::json& json_value, FileChange& change)
{
    json_value.at("target_file").get_to(change.target_file);
    json_value.at("change_type").get_to(change.change_type);
    json_value.at("original_snippet").get_to(change.original_snippet);
    json_value.at("replacement_snippet").get_to(change.replacement_snippet);
    json_value.at("explanation").get_to(change.explanation);
}

void to_json(nlohmann::json& json_value, const PatchPlan& plan)
{
    json_value = nlohmann::json{
        {"changes", plan.changes}
    };
}

void from_json(const nlohmann::json& json_value, PatchPlan& plan)
{
    json_value.at("changes").get_to(plan.changes);
}
} // namespace agentguard
