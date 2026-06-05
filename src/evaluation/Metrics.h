#pragma once

#include <map>
#include <string>

#include <nlohmann/json_fwd.hpp>

namespace agentguard
{
struct Metrics
{
    std::string task_id;
    bool build_success = false;
    int repair_rounds = 0;
    std::map<std::string, int> error_type_counts;
    int modified_file_count = 0;
};

void to_json(nlohmann::json& json_value, const Metrics& metrics);
void from_json(const nlohmann::json& json_value, Metrics& metrics);
} // namespace agentguard
