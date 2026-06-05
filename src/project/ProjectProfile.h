#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agentguard
{
enum class ProjectKind
{
    VisualStudioCxx,
    CMakeCpp,
    Python,
    Node,
    DotNet,
    Java,
    Unreal,
    Custom,
    Unknown
};

using ProjectType = ProjectKind;

enum class LanguageKind
{
    Cpp,
    Python,
    JavaScript,
    TypeScript,
    CSharp,
    FSharp,
    Java,
    Unreal,
    Custom,
    Unknown
};

struct CommandStep
{
    std::string name;
    std::string command;
    std::vector<std::string> args;
    std::string working_directory = ".";
    int timeout_seconds = 0;
    bool required = true;
    bool skip_if_missing_executable = false;
};

struct SourceFilePolicy
{
    std::vector<std::string> include;
    std::vector<std::string> exclude;
    std::vector<std::string> source_extensions;
    std::vector<std::string> config_file_patterns;
    std::vector<std::string> exclude_dirs;
    std::uintmax_t max_file_size = 1024 * 1024;
    bool skip_binary_files = true;
    bool skip_generated_files = true;
};

struct ProjectProfile
{
    std::filesystem::path root;
    ProjectKind kind = ProjectKind::Unknown;
    ProjectType project_type = ProjectType::Unknown;
    std::string adapter = "unknown";
    std::vector<LanguageKind> languages;
    SourceFilePolicy source_policy;
    std::vector<std::string> source_extensions;
    std::vector<std::string> exclude_dirs;
    std::vector<std::string> project_files;
    std::vector<std::string> source_roots;
    std::vector<std::string> test_roots;
    std::vector<std::string> protected_files;
    std::vector<CommandStep> verify_commands;
    std::vector<CommandStep> verify_steps;
    double confidence = 0.0;
    std::vector<std::string> reasons;
    std::string profile_source = "auto";
};

std::string ProjectKindToString(ProjectKind kind);
ProjectKind ProjectKindFromString(const std::string& value);
std::string ProjectTypeToString(ProjectType type);
ProjectType ProjectTypeFromString(const std::string& value);
std::string LanguageKindToString(LanguageKind language);
LanguageKind LanguageKindFromString(const std::string& value);
ProjectProfile DetectProjectProfile(const std::filesystem::path& root);
bool IsPathAllowedBySourcePolicy(
    const std::filesystem::path& root,
    const SourceFilePolicy& policy,
    const std::filesystem::path& relative_path);

void to_json(nlohmann::json& json_value, const CommandStep& step);
void from_json(const nlohmann::json& json_value, CommandStep& step);
void to_json(nlohmann::json& json_value, const SourceFilePolicy& policy);
void to_json(nlohmann::json& json_value, const ProjectProfile& profile);
} // namespace agentguard
