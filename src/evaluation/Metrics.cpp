#include "evaluation/Metrics.h"

#include <nlohmann/json.hpp>

namespace agentguard
{
void to_json(nlohmann::json& json_value, const Metrics& metrics)
{
    json_value = nlohmann::json{
        {"task_id", metrics.task_id},
        {"build_success", metrics.build_success},
        {"repair_rounds", metrics.repair_rounds},
        {"error_type_counts", metrics.error_type_counts},
        {"modified_file_count", metrics.modified_file_count}
    };
}

void from_json(const nlohmann::json& json_value, Metrics& metrics)
{
    json_value.at("task_id").get_to(metrics.task_id);
    json_value.at("build_success").get_to(metrics.build_success);
    json_value.at("repair_rounds").get_to(metrics.repair_rounds);
    json_value.at("error_type_counts").get_to(metrics.error_type_counts);
    json_value.at("modified_file_count").get_to(metrics.modified_file_count);
}
} // namespace agentguard
