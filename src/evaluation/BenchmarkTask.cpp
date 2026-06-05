#include "evaluation/BenchmarkTask.h"

#include <nlohmann/json.hpp>

namespace agentguard
{
void to_json(nlohmann::json& json_value, const BenchmarkTask& task)
{
    json_value = nlohmann::json{
        {"task_id", task.task_id},
        {"id", task.task_id},
        {"title", task.title},
        {"name", task.title},
        {"solution", task.solution},
        {"task", task.task_file},
        {"description", task.description},
        {"expected_files", task.expected_files},
        {"forbidden_files", task.forbidden_files},
        {"protected_files", task.protected_files.empty() ? task.forbidden_files : task.protected_files},
        {"acceptance_criteria", task.acceptance_criteria},
        {"validation_command", task.validation_command},
        {"difficulty", task.difficulty},
        {"tags", task.tags}
    };
}

void from_json(const nlohmann::json& json_value, BenchmarkTask& task)
{
    task.task_id = json_value.value("task_id", json_value.value("id", std::string{}));
    task.title = json_value.value("title", json_value.value("name", std::string{}));
    task.solution = json_value.value("solution", std::string{});
    task.task_file = json_value.value("task", std::string{});
    task.description = json_value.value("description", task.task_file);
    json_value.at("expected_files").get_to(task.expected_files);
    task.forbidden_files = json_value.value("forbidden_files", std::vector<std::string>{});
    task.protected_files = json_value.value("protected_files", std::vector<std::string>{});
    if (task.forbidden_files.empty())
    {
        task.forbidden_files = task.protected_files;
    }
    if (task.protected_files.empty())
    {
        task.protected_files = task.forbidden_files;
    }
    task.acceptance_criteria = json_value.value("acceptance_criteria", std::vector<std::string>{});
    task.validation_command = json_value.value("validation_command", std::string{});
    task.difficulty = json_value.value("difficulty", std::string{});
    task.tags = json_value.value("tags", std::vector<std::string>{});
}
} // namespace agentguard
