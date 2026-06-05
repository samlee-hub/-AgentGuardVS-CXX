#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace agentguard
{
struct FileChange
{
    std::string target_file;
    std::string change_type;
    std::string original_snippet;
    std::string replacement_snippet;
    std::string explanation;
};

struct PatchPlan
{
    std::vector<FileChange> changes;
};

void to_json(nlohmann::json& json_value, const FileChange& change);
void from_json(const nlohmann::json& json_value, FileChange& change);

void to_json(nlohmann::json& json_value, const PatchPlan& plan);
void from_json(const nlohmann::json& json_value, PatchPlan& plan);
} // namespace agentguard
