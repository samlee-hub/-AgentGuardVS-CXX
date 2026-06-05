#include "core/SemanticModels.h"

#include <stdexcept>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
bool ContainsString(std::initializer_list<std::string_view> values, const std::string& candidate)
{
    for (const auto value : values)
    {
        if (candidate == value)
        {
            return true;
        }
    }
    return false;
}

void ValidateConfidence(double confidence, const std::string& field_name)
{
    if (confidence < 0.0 || confidence > 1.0)
    {
        throw std::invalid_argument(field_name + " confidence must be between 0 and 1.");
    }
}

void ValidateFileReference(const SemanticFileReference& file)
{
    if (file.path.empty())
    {
        throw std::invalid_argument("Semantic file path cannot be empty.");
    }
    ValidateConfidence(file.confidence, file.path);
}

std::string NormalizeSemanticPath(const std::string& path)
{
    return SafeRelativePath(std::filesystem::path(path)).generic_string();
}

void ValidateFileReferences(const std::vector<SemanticFileReference>& files)
{
    for (const auto& file : files)
    {
        ValidateFileReference(file);
    }
}

void ValidateScopeResult(const SemanticScopeResult& result)
{
    ValidateFileReferences(result.allowed_files);
    ValidateFileReferences(result.context_files);
    ValidateFileReferences(result.suspected_files);
    ValidateFileReferences(result.protected_files);
    ValidateFileReferences(result.needs_approval_files);

    if (!ContainsString({"low", "medium", "high"}, result.risk_level))
    {
        throw std::invalid_argument("SemanticScopeResult.risk_level is unsupported.");
    }
    if (!ContainsString({"proceed", "expand_scope", "needs_approval", "stop"}, result.recommendation))
    {
        throw std::invalid_argument("SemanticScopeResult.recommendation is unsupported.");
    }
    if (!ContainsString({"low", "medium", "high"}, result.blast_radius))
    {
        throw std::invalid_argument("SemanticScopeResult.blast_radius is unsupported.");
    }

    std::unordered_set<std::string> allowed_paths;
    for (const auto& file : result.allowed_files)
    {
        allowed_paths.insert(file.path);
    }
    for (const auto& file : result.protected_files)
    {
        if (allowed_paths.contains(file.path))
        {
            throw std::invalid_argument("SemanticScopeResult protected_files cannot overlap allowed_files.");
        }
    }
}

void ValidateReviewResult(const SemanticReviewResult& result)
{
    ValidateFileReferences(result.suggested_scope_additions);
    ValidateConfidence(result.confidence, "SemanticReviewResult");

    if (!ContainsString({"accept", "repair", "expand_scope", "ask_user", "stop"}, result.next_action))
    {
        throw std::invalid_argument("SemanticReviewResult.next_action is unsupported.");
    }
}
} // namespace

void to_json(nlohmann::json& json_value, const SemanticFileReference& file)
{
    json_value = nlohmann::json{
        {"path", file.path},
        {"reason", file.reason},
        {"confidence", file.confidence}
    };
}

void from_json(const nlohmann::json& json_value, SemanticFileReference& file)
{
    json_value.at("path").get_to(file.path);
    json_value.at("reason").get_to(file.reason);
    json_value.at("confidence").get_to(file.confidence);
    file.path = NormalizeSemanticPath(file.path);
    ValidateFileReference(file);
}

void to_json(nlohmann::json& json_value, const SemanticRisk& risk)
{
    json_value = nlohmann::json{
        {"type", risk.type},
        {"file", risk.file},
        {"reason", risk.reason},
        {"severity", risk.severity}
    };
}

void from_json(const nlohmann::json& json_value, SemanticRisk& risk)
{
    json_value.at("type").get_to(risk.type);
    json_value.at("file").get_to(risk.file);
    json_value.at("reason").get_to(risk.reason);
    json_value.at("severity").get_to(risk.severity);
}

void to_json(nlohmann::json& json_value, const SemanticScopeResult& result)
{
    json_value = nlohmann::json{
        {"task_summary", result.task_summary},
        {"allowed_files", result.allowed_files},
        {"context_files", result.context_files},
        {"suspected_files", result.suspected_files},
        {"protected_files", result.protected_files},
        {"needs_approval_files", result.needs_approval_files},
        {"risk_level", result.risk_level},
        {"recommendation", result.recommendation},
        {"related_symbols", result.related_symbols},
        {"dependency_reasons", result.dependency_reasons},
        {"blast_radius", result.blast_radius},
        {"public_interface_changes", result.public_interface_changes},
        {"missing_context", result.missing_context},
        {"notes", result.notes}
    };
}

void from_json(const nlohmann::json& json_value, SemanticScopeResult& result)
{
    json_value.at("task_summary").get_to(result.task_summary);
    json_value.at("allowed_files").get_to(result.allowed_files);
    json_value.at("context_files").get_to(result.context_files);
    json_value.at("suspected_files").get_to(result.suspected_files);
    json_value.at("protected_files").get_to(result.protected_files);
    json_value.at("needs_approval_files").get_to(result.needs_approval_files);
    json_value.at("risk_level").get_to(result.risk_level);
    json_value.at("recommendation").get_to(result.recommendation);
    result.related_symbols = json_value.value("related_symbols", std::vector<std::string>{});
    result.dependency_reasons = json_value.value("dependency_reasons", std::vector<std::string>{});
    result.blast_radius = json_value.value("blast_radius", std::string("low"));
    result.public_interface_changes = json_value.value("public_interface_changes", false);
    json_value.at("missing_context").get_to(result.missing_context);
    json_value.at("notes").get_to(result.notes);
    ValidateScopeResult(result);
}

void to_json(nlohmann::json& json_value, const SemanticReviewResult& result)
{
    json_value = nlohmann::json{
        {"meets_requirement", result.meets_requirement},
        {"requires_scope_expansion", result.requires_scope_expansion},
        {"suggested_scope_additions", result.suggested_scope_additions},
        {"risks", result.risks},
        {"confidence", result.confidence},
        {"next_action", result.next_action},
        {"notes", result.notes}
    };
}

void from_json(const nlohmann::json& json_value, SemanticReviewResult& result)
{
    json_value.at("meets_requirement").get_to(result.meets_requirement);
    json_value.at("requires_scope_expansion").get_to(result.requires_scope_expansion);
    json_value.at("suggested_scope_additions").get_to(result.suggested_scope_additions);
    json_value.at("risks").get_to(result.risks);
    json_value.at("confidence").get_to(result.confidence);
    json_value.at("next_action").get_to(result.next_action);
    json_value.at("notes").get_to(result.notes);
    ValidateReviewResult(result);
}
} // namespace agentguard
