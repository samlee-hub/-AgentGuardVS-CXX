#include "core/Models.h"

#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

namespace agentguard
{
namespace
{
std::string_view ToString(const ErrorCategory category)
{
    switch (category)
    {
    case ErrorCategory::CompileError:
        return "CompileError";
    case ErrorCategory::LinkError:
        return "LinkError";
    case ErrorCategory::IncludeError:
        return "IncludeError";
    case ErrorCategory::MSBuildError:
        return "MSBuildError";
    case ErrorCategory::ConfigurationError:
        return "ConfigurationError";
    case ErrorCategory::Unknown:
        return "Unknown";
    }

    return "Unknown";
}

ErrorCategory ErrorCategoryFromString(const std::string& value)
{
    if (value == "CompileError")
    {
        return ErrorCategory::CompileError;
    }
    if (value == "LinkError")
    {
        return ErrorCategory::LinkError;
    }
    if (value == "IncludeError")
    {
        return ErrorCategory::IncludeError;
    }
    if (value == "MSBuildError")
    {
        return ErrorCategory::MSBuildError;
    }
    if (value == "ConfigurationError")
    {
        return ErrorCategory::ConfigurationError;
    }
    if (value == "Unknown")
    {
        return ErrorCategory::Unknown;
    }

    throw std::invalid_argument("Unsupported ErrorCategory value: " + value);
}
} // namespace

void to_json(nlohmann::json& json_value, const ErrorCategory& category)
{
    json_value = ToString(category);
}

void from_json(const nlohmann::json& json_value, ErrorCategory& category)
{
    category = ErrorCategoryFromString(json_value.get<std::string>());
}

void to_json(nlohmann::json& json_value, const TaskSpec& spec)
{
    json_value = nlohmann::json{
        {"task_id", spec.task_id},
        {"user_request", spec.user_request},
        {"target_solution", spec.target_solution},
        {"target_project", spec.target_project},
        {"allowed_files", spec.allowed_files},
        {"forbidden_files", spec.forbidden_files},
        {"context_files", spec.context_files},
        {"suspected_files", spec.suspected_files},
        {"protected_files", spec.protected_files},
        {"needs_approval_files", spec.needs_approval_files},
        {"acceptance_criteria", spec.acceptance_criteria},
        {"max_repair_rounds", spec.max_repair_rounds}
    };
    if (spec.has_semantic_scope)
    {
        json_value["semantic_scope"] = spec.semantic_scope;
    }
}

void from_json(const nlohmann::json& json_value, TaskSpec& spec)
{
    json_value.at("task_id").get_to(spec.task_id);
    json_value.at("user_request").get_to(spec.user_request);
    json_value.at("target_solution").get_to(spec.target_solution);
    json_value.at("target_project").get_to(spec.target_project);
    json_value.at("allowed_files").get_to(spec.allowed_files);
    json_value.at("forbidden_files").get_to(spec.forbidden_files);
    spec.context_files = json_value.value("context_files", std::vector<std::string>{});
    spec.suspected_files = json_value.value("suspected_files", std::vector<std::string>{});
    spec.protected_files = json_value.value("protected_files", std::vector<std::string>{});
    spec.needs_approval_files = json_value.value("needs_approval_files", std::vector<std::string>{});
    if (json_value.contains("semantic_scope"))
    {
        spec.semantic_scope = json_value.at("semantic_scope").get<SemanticScopeResult>();
        spec.has_semantic_scope = true;
    }
    else
    {
        spec.has_semantic_scope = false;
    }
    json_value.at("acceptance_criteria").get_to(spec.acceptance_criteria);
    json_value.at("max_repair_rounds").get_to(spec.max_repair_rounds);
}

void to_json(nlohmann::json& json_value, const ParsedBuildError& error)
{
    json_value = nlohmann::json{
        {"file", error.file},
        {"line", error.line},
        {"column", error.column},
        {"code", error.code},
        {"message", error.message},
        {"category", error.category},
        {"raw_line", error.raw_line}
    };
}

void from_json(const nlohmann::json& json_value, ParsedBuildError& error)
{
    json_value.at("file").get_to(error.file);
    json_value.at("line").get_to(error.line);
    json_value.at("column").get_to(error.column);
    json_value.at("code").get_to(error.code);
    json_value.at("message").get_to(error.message);
    json_value.at("category").get_to(error.category);
    json_value.at("raw_line").get_to(error.raw_line);
}

void to_json(nlohmann::json& json_value, const BuildResult& result)
{
    json_value = nlohmann::json{
        {"success", result.success},
        {"command", result.command},
        {"return_code", result.return_code},
        {"stdout_text", result.stdout_text},
        {"stderr_text", result.stderr_text},
        {"duration_ms", result.duration_ms},
        {"parsed_errors", result.parsed_errors}
    };
}

void from_json(const nlohmann::json& json_value, BuildResult& result)
{
    json_value.at("success").get_to(result.success);
    json_value.at("command").get_to(result.command);
    json_value.at("return_code").get_to(result.return_code);
    json_value.at("stdout_text").get_to(result.stdout_text);
    json_value.at("stderr_text").get_to(result.stderr_text);
    json_value.at("duration_ms").get_to(result.duration_ms);
    json_value.at("parsed_errors").get_to(result.parsed_errors);
}

void to_json(nlohmann::json& json_value, const RunReport& report)
{
    json_value = nlohmann::json{
        {"success", report.success},
        {"task_spec", report.task_spec},
        {"build_result", report.build_result},
        {"repair_rounds", report.repair_rounds},
        {"related_files", report.related_files},
        {"risk_warnings", report.risk_warnings}
    };
}

void from_json(const nlohmann::json& json_value, RunReport& report)
{
    json_value.at("success").get_to(report.success);
    json_value.at("task_spec").get_to(report.task_spec);
    json_value.at("build_result").get_to(report.build_result);
    json_value.at("repair_rounds").get_to(report.repair_rounds);
    json_value.at("related_files").get_to(report.related_files);
    json_value.at("risk_warnings").get_to(report.risk_warnings);
}
} // namespace agentguard
