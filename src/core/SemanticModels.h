#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace agentguard
{
struct SemanticFileReference
{
    std::string path;
    std::string reason;
    double confidence = 0.0;
};

struct SemanticRisk
{
    std::string type;
    std::string file;
    std::string reason;
    std::string severity;
};

struct SemanticScopeResult
{
    std::string task_summary;
    std::vector<SemanticFileReference> allowed_files;
    std::vector<SemanticFileReference> context_files;
    std::vector<SemanticFileReference> suspected_files;
    std::vector<SemanticFileReference> protected_files;
    std::vector<SemanticFileReference> needs_approval_files;
    std::string risk_level;
    std::string recommendation;
    std::vector<std::string> related_symbols;
    std::vector<std::string> dependency_reasons;
    std::string blast_radius = "low";
    bool public_interface_changes = false;
    std::vector<std::string> missing_context;
    std::vector<std::string> notes;
};

struct SemanticReviewResult
{
    bool meets_requirement = false;
    bool requires_scope_expansion = false;
    std::vector<SemanticFileReference> suggested_scope_additions;
    std::vector<SemanticRisk> risks;
    double confidence = 0.0;
    std::string next_action;
    std::vector<std::string> notes;
};

void to_json(nlohmann::json& json_value, const SemanticFileReference& file);
void from_json(const nlohmann::json& json_value, SemanticFileReference& file);

void to_json(nlohmann::json& json_value, const SemanticRisk& risk);
void from_json(const nlohmann::json& json_value, SemanticRisk& risk);

void to_json(nlohmann::json& json_value, const SemanticScopeResult& result);
void from_json(const nlohmann::json& json_value, SemanticScopeResult& result);

void to_json(nlohmann::json& json_value, const SemanticReviewResult& result);
void from_json(const nlohmann::json& json_value, SemanticReviewResult& result);
} // namespace agentguard
