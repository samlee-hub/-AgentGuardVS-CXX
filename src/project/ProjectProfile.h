#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agentguard
{
enum class ProjectType
{
    VisualStudioCxx,
    CMakeCpp,
    NodeWeb,
    Python,
    DotNet,
    Java,
    Unreal,
    Custom,
    Unknown
};

struct CommandStep
{
    std::string name;
    std::string executable;
    std::vector<std::string> arguments;
};

struct ProjectProfile
{
    std::filesystem::path root;
    ProjectType project_type = ProjectType::Unknown;
    std::string adapter = "unknown";
    std::vector<std::string> project_files;
    std::vector<std::string> source_roots;
    std::vector<std::string> test_roots;
    std::vector<std::string> protected_files;
    std::vector<CommandStep> verify_steps;
};

std::string ProjectTypeToString(ProjectType type);
ProjectType ProjectTypeFromString(const std::string& value);
ProjectProfile DetectProjectProfile(const std::filesystem::path& root);

void to_json(nlohmann::json& json_value, const CommandStep& step);
void from_json(const nlohmann::json& json_value, CommandStep& step);
void to_json(nlohmann::json& json_value, const ProjectProfile& profile);
} // namespace agentguard
