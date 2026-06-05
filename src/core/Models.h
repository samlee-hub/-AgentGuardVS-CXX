#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "core/SemanticModels.h"

namespace agentguard
{
enum class ErrorCategory
{
    CompileError,
    LinkError,
    IncludeError,
    MSBuildError,
    ConfigurationError,
    Unknown
};

struct VSProjectInfo
{
    std::string project_name;
    std::string project_path;
    std::string project_guid;
    std::string project_type_guid;
    std::vector<std::string> configurations;
    std::vector<std::string> platforms;
    std::vector<std::string> source_files;
    std::vector<std::string> header_files;
    std::vector<std::string> include_directories;
    std::vector<std::string> preprocessor_definitions;
};

struct ProjectInfo
{
    std::string solution_name;
    std::string solution_path;
    std::vector<VSProjectInfo> projects;
};

struct SourceFileInfo
{
    std::string file_path;
    std::vector<std::string> includes;
    std::vector<std::string> classes;
    std::vector<std::string> structs;
    std::vector<std::string> enums;
    std::vector<std::string> functions;
    std::vector<std::string> methods;
    std::vector<std::string> member_variables;
    std::vector<std::string> macros;
    std::vector<std::string> command_strings;
    std::vector<std::string> symbol_references;
    std::vector<std::string> keywords;
};

struct TaskSpec
{
    std::string task_id;
    std::string user_request;
    std::string target_solution;
    std::string target_project;
    std::vector<std::string> allowed_files;
    std::vector<std::string> forbidden_files;
    std::vector<std::string> context_files;
    std::vector<std::string> suspected_files;
    std::vector<std::string> protected_files;
    std::vector<std::string> needs_approval_files;
    bool has_semantic_scope = false;
    SemanticScopeResult semantic_scope;
    std::vector<std::string> acceptance_criteria;
    int max_repair_rounds = 0;
};

struct ModificationConstraint
{
    std::vector<std::string> allowed_files;
    std::vector<std::string> forbidden_files;
    std::string rationale;
};

struct BuildCommand
{
    std::string executable;
    std::vector<std::string> arguments;
    std::string working_directory;
    std::string rendered_command;
};

struct ParsedBuildError
{
    std::string file;
    int line = 0;
    int column = 0;
    std::string code;
    std::string message;
    ErrorCategory category = ErrorCategory::Unknown;
    std::string raw_line;
};

struct BuildResult
{
    bool success = false;
    std::string command;
    int return_code = 0;
    std::string stdout_text;
    std::string stderr_text;
    std::int64_t duration_ms = 0;
    std::vector<ParsedBuildError> parsed_errors;
};

struct AgentRunResult
{
    bool success = false;
    std::string agent_name;
    std::string summary;
    std::vector<std::string> warnings;
};

struct RunReport
{
    bool success = false;
    TaskSpec task_spec;
    BuildResult build_result;
    int repair_rounds = 0;
    std::vector<std::string> related_files;
    std::vector<std::string> risk_warnings;
};

void to_json(nlohmann::json& json_value, const ErrorCategory& category);
void from_json(const nlohmann::json& json_value, ErrorCategory& category);

void to_json(nlohmann::json& json_value, const TaskSpec& spec);
void from_json(const nlohmann::json& json_value, TaskSpec& spec);

void to_json(nlohmann::json& json_value, const ParsedBuildError& error);
void from_json(const nlohmann::json& json_value, ParsedBuildError& error);

void to_json(nlohmann::json& json_value, const BuildResult& result);
void from_json(const nlohmann::json& json_value, BuildResult& result);

void to_json(nlohmann::json& json_value, const RunReport& report);
void from_json(const nlohmann::json& json_value, RunReport& report);
} // namespace agentguard
