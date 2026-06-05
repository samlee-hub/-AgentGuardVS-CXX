#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace agentguard
{
struct BenchmarkTask
{
    std::string task_id;
    std::string title;
    std::string solution;
    std::string task_file;
    std::string description;
    std::vector<std::string> expected_files;
    std::vector<std::string> forbidden_files;
    std::vector<std::string> protected_files;
    std::vector<std::string> acceptance_criteria;
    std::string validation_command;
    std::string difficulty;
    std::vector<std::string> tags;
};

void to_json(nlohmann::json& json_value, const BenchmarkTask& task);
void from_json(const nlohmann::json& json_value, BenchmarkTask& task);
} // namespace agentguard
