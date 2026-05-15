#include "indexing/DependencyGraph.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <regex>
#include <set>
#include <unordered_set>

namespace agentguard
{
namespace
{
std::string NormalizePath(const std::string& path)
{
    return std::filesystem::path(path).generic_string();
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void AddUnique(std::vector<std::string>& values, const std::string& value)
{
    if (value.empty())
    {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

std::vector<std::string> Tokenize(const std::string& text)
{
    const std::regex token_pattern(R"([A-Za-z0-9_]+)");
    std::vector<std::string> tokens;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), token_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        std::string token = ToLower((*it)[0].str());
        if (token.size() >= 3)
        {
            tokens.push_back(std::move(token));
        }
    }
    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
    return tokens;
}

bool ContainsAnyToken(const std::string& text, const std::vector<std::string>& tokens)
{
    const std::string lowered = ToLower(text);
    for (const auto& token : tokens)
    {
        if (lowered.find(token) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

bool IsHeader(const std::string& path)
{
    const std::string lowered = ToLower(path);
    return lowered.ends_with(".h") || lowered.ends_with(".hpp");
}

bool IsCppFile(const std::string& path)
{
    const std::string lowered = ToLower(path);
    return lowered.ends_with(".cpp") || lowered.ends_with(".cc") || lowered.ends_with(".cxx");
}

bool IsLikelyTest(const std::string& path)
{
    const std::string lowered = ToLower(path);
    return lowered.find("test") != std::string::npos || lowered.find("tests/") != std::string::npos ||
           lowered.find("_spec") != std::string::npos;
}

bool IsProtectedPath(const std::string& path)
{
    const std::string lowered = ToLower(path);
    return lowered.find(".git/") != std::string::npos ||
           lowered.find("/third_party/") != std::string::npos ||
           lowered.find("/external/") != std::string::npos ||
           lowered.find("/build/") != std::string::npos ||
           lowered.find("/generated/") != std::string::npos ||
           lowered.starts_with("build/") ||
           lowered.starts_with("generated/");
}

std::string ResolveInclude(
    const std::string& including_file,
    const std::string& include_value,
    const std::unordered_set<std::string>& known_files)
{
    if (include_value.empty() || include_value.front() == '<')
    {
        return {};
    }

    const auto including_path = std::filesystem::path(including_file);
    const std::string relative_candidate =
        (including_path.parent_path() / include_value).lexically_normal().generic_string();
    if (known_files.contains(relative_candidate))
    {
        return relative_candidate;
    }

    const std::string normalized_include =
        std::filesystem::path(include_value).lexically_normal().generic_string();
    if (known_files.contains(normalized_include))
    {
        return normalized_include;
    }

    std::vector<std::string> suffix_matches;
    for (const auto& file : known_files)
    {
        if (file.ends_with("/" + normalized_include) || file == normalized_include)
        {
            suffix_matches.push_back(file);
        }
    }
    std::sort(suffix_matches.begin(), suffix_matches.end());
    return suffix_matches.empty() ? std::string{} : suffix_matches.front();
}

std::string BlastRadiusForDependents(std::size_t dependent_count, bool public_header_risk)
{
    if (dependent_count >= 5)
    {
        return "high";
    }
    if (public_header_risk || dependent_count > 0)
    {
        return "medium";
    }
    return "low";
}
} // namespace

DependencyGraph BuildDependencyGraph(
    const std::filesystem::path&,
    const ProjectInfo& project_info,
    const std::vector<SourceFileInfo>& source_files)
{
    DependencyGraph graph;
    std::unordered_set<std::string> known_files;
    for (const auto& file : source_files)
    {
        const std::string normalized = NormalizePath(file.file_path);
        known_files.insert(normalized);
        if (IsProtectedPath(normalized))
        {
            graph.protected_files.push_back(normalized);
        }
    }

    for (const auto& file : source_files)
    {
        const std::string including_file = NormalizePath(file.file_path);
        for (const auto& include_value : file.includes)
        {
            const std::string resolved = ResolveInclude(including_file, include_value, known_files);
            if (resolved.empty())
            {
                continue;
            }
            AddUnique(graph.includes_by_file[including_file], resolved);
            AddUnique(graph.reverse_includes_by_file[resolved], including_file);
        }
    }

    for (const auto& project : project_info.projects)
    {
        auto& project_sources = graph.project_sources_by_project[project.project_name];
        for (const auto& source : project.source_files)
        {
            AddUnique(project_sources, NormalizePath(source));
        }
        for (const auto& header : project.header_files)
        {
            AddUnique(project_sources, NormalizePath(header));
        }
        std::sort(project_sources.begin(), project_sources.end());
    }

    for (auto& [_, values] : graph.includes_by_file)
    {
        std::sort(values.begin(), values.end());
    }
    for (auto& [_, values] : graph.reverse_includes_by_file)
    {
        std::sort(values.begin(), values.end());
    }
    std::sort(graph.protected_files.begin(), graph.protected_files.end());
    graph.protected_files.erase(
        std::unique(graph.protected_files.begin(), graph.protected_files.end()),
        graph.protected_files.end());
    return graph;
}

std::vector<std::string> FindReverseIncludeDependents(
    const DependencyGraph& graph,
    const std::string& file_path)
{
    const std::string normalized = NormalizePath(file_path);
    const auto it = graph.reverse_includes_by_file.find(normalized);
    if (it == graph.reverse_includes_by_file.end())
    {
        return {};
    }
    return it->second;
}

ImpactAnalysis AnalyzeImpact(
    const std::string& user_request,
    const std::vector<SourceFileInfo>& source_files,
    const DependencyGraph& graph,
    const SymbolIndex& symbol_index)
{
    ImpactAnalysis impact;
    const auto tokens = Tokenize(user_request);
    std::set<std::string> matched_files;

    for (const auto& entry : symbol_index.entries)
    {
        if (!ContainsAnyToken(entry.name, tokens) && !ContainsAnyToken(user_request, {ToLower(entry.name)}))
        {
            continue;
        }
        AddUnique(
            impact.related_symbols,
            entry.kind + ":" + entry.name + "@" + entry.file_path);
        matched_files.insert(entry.file_path);
    }

    for (const auto& file : source_files)
    {
        const std::string path = NormalizePath(file.file_path);
        if (ContainsAnyToken(path, tokens))
        {
            matched_files.insert(path);
        }
        if (IsLikelyTest(path) && (ContainsAnyToken(path, tokens) || !impact.related_symbols.empty()))
        {
            AddUnique(impact.likely_tests, path);
        }
    }

    std::size_t max_dependents = 0;
    for (const auto& file : matched_files)
    {
        if (!IsHeader(file))
        {
            continue;
        }
        const auto dependents = FindReverseIncludeDependents(graph, file);
        max_dependents = std::max(max_dependents, dependents.size());
        if (!dependents.empty())
        {
            impact.public_header_risk = true;
            impact.public_interface_changes = true;
        }
        for (const auto& dependent : dependents)
        {
            AddUnique(impact.reverse_include_dependents, dependent);
            AddUnique(impact.dependency_reasons, file + " included by " + dependent);
        }
    }

    for (const auto& [header, dependents] : graph.reverse_includes_by_file)
    {
        if (!ContainsAnyToken(header, tokens))
        {
            continue;
        }
        max_dependents = std::max(max_dependents, dependents.size());
        if (IsHeader(header) && !dependents.empty())
        {
            impact.public_header_risk = true;
            impact.public_interface_changes = true;
        }
    }

    impact.changed_header_blast_radius =
        BlastRadiusForDependents(max_dependents, impact.public_header_risk);
    impact.blast_radius = impact.changed_header_blast_radius;

    std::sort(impact.related_symbols.begin(), impact.related_symbols.end());
    impact.related_symbols.erase(
        std::unique(impact.related_symbols.begin(), impact.related_symbols.end()),
        impact.related_symbols.end());
    std::sort(impact.dependency_reasons.begin(), impact.dependency_reasons.end());
    std::sort(impact.reverse_include_dependents.begin(), impact.reverse_include_dependents.end());
    std::sort(impact.likely_tests.begin(), impact.likely_tests.end());
    return impact;
}
} // namespace agentguard
