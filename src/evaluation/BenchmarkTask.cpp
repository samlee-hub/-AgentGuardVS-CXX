#include "evaluation/BenchmarkTask.h"

#include <nlohmann/json.hpp>

namespace agentguard
{
void to_json(nlohmann::json& json_value, const BenchmarkTask& task)
{
    json_value = nlohmann::json{
        {"task_id", task.task_id},
        {"title", task.title},
        {"description", task.description},
        {"expected_files", task.expected_files},
        {"forbidden_files", task.forbidden_files},
        {"acceptance_criteria", task.acceptance_criteria},
        {"difficulty", task.difficulty},
        {"tags", task.tags}
    };
}

void from_json(const nlohmann::json& json_value, BenchmarkTask& task)
{
    json_value.at("task_id").get_to(task.task_id);
    json_value.at("title").get_to(task.title);
    json_value.at("description").get_to(task.description);
    json_value.at("expected_files").get_to(task.expected_files);
    json_value.at("forbidden_files").get_to(task.forbidden_files);
    json_value.at("acceptance_criteria").get_to(task.acceptance_criteria);
    json_value.at("difficulty").get_to(task.difficulty);
    json_value.at("tags").get_to(task.tags);
}
} // namespace agentguard
