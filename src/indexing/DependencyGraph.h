#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Models.h"
#include "indexing/SymbolIndex.h"

namespace agentguard
{
struct DependencyGraph
{
    std::unordered_map<std::string, std::vector<std::string>> includes_by_file;
    std::unordered_map<std::string, std::vector<std::string>> reverse_includes_by_file;
    std::unordered_map<std::string, std::vector<std::string>> project_sources_by_project;
    std::vector<std::string> protected_files;
};

struct ImpactAnalysis
{
    std::vector<std::string> related_symbols;
    std::vector<std::string> direct_symbol_hits;
    std::vector<std::string> dependency_reasons;
    std::vector<std::string> reverse_include_dependents;
    std::vector<std::string> likely_tests;
    bool public_header_risk = false;
    std::string changed_header_blast_radius = "low";
    std::string blast_radius = "low";
    bool public_interface_changes = false;
};

DependencyGraph BuildDependencyGraph(
    const std::filesystem::path& root,
    const ProjectInfo& project_info,
    const std::vector<SourceFileInfo>& source_files);

std::vector<std::string> FindReverseIncludeDependents(
    const DependencyGraph& graph,
    const std::string& file_path);

ImpactAnalysis AnalyzeImpact(
    const std::string& user_request,
    const std::vector<SourceFileInfo>& source_files,
    const DependencyGraph& graph,
    const SymbolIndex& symbol_index);
} // namespace agentguard
