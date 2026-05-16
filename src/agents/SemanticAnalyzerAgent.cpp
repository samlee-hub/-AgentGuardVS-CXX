#include "agents/SemanticAnalyzerAgent.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "security/SecurityPolicy.h"

namespace agentguard
{
namespace
{
constexpr std::size_t kMaxCandidateCount = 12;
constexpr std::size_t kMaxSourceFileSummaryCount = 40;
constexpr std::size_t kMaxSnippetChars = 900;

std::string JoinStrings(const std::vector<std::string>& values)
{
    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index > 0)
        {
            output << ", ";
        }
        output << values[index];
    }
    return output.str();
}

std::string TruncateSnippet(const std::string& content, bool& truncated)
{
    if (content.size() <= kMaxSnippetChars)
    {
        truncated = false;
        return content;
    }

    truncated = true;
    return content.substr(0, kMaxSnippetChars);
}

std::string BuildPrompt(const SemanticAnalyzerInput& input)
{
    std::ostringstream prompt;
    prompt << "You are doing semantic scope analysis for a Visual Studio C++ coding task.\n";
    prompt << "Return strict JSON only. Do not emit markdown or prose.\n";
    prompt << "Do not propose code edits. Only classify file scope.\n";
    prompt << "JSON schema fields:\n";
    prompt << "{task_summary:string, allowed_files:[{path,reason,confidence}], ";
    prompt << "context_files:[{path,reason,confidence}], suspected_files:[{path,reason,confidence}], ";
    prompt << "protected_files:[{path,reason,confidence}], needs_approval_files:[{path,reason,confidence}], ";
    prompt << "risk_level:low|medium|high, recommendation:proceed|expand_scope|needs_approval|stop, ";
    prompt << "related_symbols:string[], dependency_reasons:string[], blast_radius:low|medium|high, ";
    prompt << "public_interface_changes:bool, missing_context:string[], notes:string[]}.\n";
    prompt << "Rules: protected_files must not appear in allowed_files; needs_approval_files must not appear ";
    prompt << "in allowed_files; confidence must be between 0 and 1.\n";
    prompt << "User task: " << input.user_request << "\n";
    prompt << "Solution: " << input.solution_path.string() << "\n";

    prompt << "Static allowed files:\n";
    for (const auto& path : input.static_allowed_files)
    {
        prompt << "- " << path << "\n";
    }

    prompt << "Static forbidden files:\n";
    for (const auto& path : input.static_forbidden_files)
    {
        prompt << "- " << path << "\n";
    }

    prompt << "Static context candidates:\n";
    for (std::size_t index = 0; index < input.context_candidates.size() && index < kMaxCandidateCount; ++index)
    {
        const auto& candidate = input.context_candidates[index];
        prompt << "- path=" << candidate.file_path
               << " score=" << candidate.score
               << " reasons=[" << JoinStrings(candidate.reasons) << "]\n";
    }

    prompt << "Source index summary:\n";
    for (std::size_t index = 0; index < input.source_files.size() && index < kMaxSourceFileSummaryCount; ++index)
    {
        const auto& file = input.source_files[index];
        prompt << "- path=" << file.file_path
               << " includes=[" << JoinStrings(file.includes) << "]"
               << " classes=[" << JoinStrings(file.classes) << "]"
               << " structs=[" << JoinStrings(file.structs) << "]"
               << " enums=[" << JoinStrings(file.enums) << "]"
               << " functions=[" << JoinStrings(file.functions) << "]"
               << " methods=[" << JoinStrings(file.methods) << "]"
               << " member_variables=[" << JoinStrings(file.member_variables) << "]"
               << " macros=[" << JoinStrings(file.macros) << "]"
               << " command_strings=[" << JoinStrings(file.command_strings) << "]"
               << " keywords=[" << JoinStrings(file.keywords) << "]\n";
    }

    prompt << "Hybrid impact analysis:\n";
    prompt << "- related_symbols=[" << JoinStrings(input.impact.related_symbols) << "]\n";
    prompt << "- direct_symbol_hits=[" << JoinStrings(input.impact.direct_symbol_hits) << "]\n";
    prompt << "- reverse_include_dependents=[" << JoinStrings(input.impact.reverse_include_dependents) << "]\n";
    prompt << "- likely_tests=[" << JoinStrings(input.impact.likely_tests) << "]\n";
    prompt << "- public_header_risk=" << (input.impact.public_header_risk ? "true" : "false") << "\n";
    prompt << "- changed_header_blast_radius=" << input.impact.changed_header_blast_radius << "\n";
    prompt << "- dependency_reasons=[" << JoinStrings(input.impact.dependency_reasons) << "]\n";
    prompt << "Decision hints: if public_header_risk=true, risk_level must be at least medium. ";
    prompt << "Files with many reverse_include_dependents should usually be context or suspected, ";
    prompt << "not automatically allowed. likely_tests should be allowed or suspected when relevant. ";
    prompt << "third_party, build, generated, and .git paths are protected.\n";

    prompt << "Relevant snippets:\n";
    for (const auto& file : input.relevant_files)
    {
        bool truncated = false;
        const auto snippet = TruncateSnippet(file.content, truncated);
        prompt << "FILE " << file.relative_path << " truncated=" << (truncated ? "true" : "false") << "\n";
        prompt << snippet << "\n";
    }

    return prompt.str();
}

std::string ExtractJsonTextFromProviderResponse(const std::string& raw_response)
{
    const auto json_value = nlohmann::json::parse(raw_response);

    if (json_value.contains("task_summary"))
    {
        return json_value.dump();
    }

    if (json_value.contains("choices"))
    {
        return json_value.at("choices").at(0).at("message").at("content").get<std::string>();
    }

    if (json_value.contains("output"))
    {
        for (const auto& output_item : json_value.at("output"))
        {
            if (!output_item.contains("content"))
            {
                continue;
            }
            for (const auto& content_item : output_item.at("content"))
            {
                if (content_item.value("type", "") == "output_text")
                {
                    return content_item.at("text").get<std::string>();
                }
            }
        }
    }

    if (json_value.contains("content"))
    {
        for (const auto& content_item : json_value.at("content"))
        {
            if (content_item.value("type", "") == "text")
            {
                return content_item.at("text").get<std::string>();
            }
        }
    }

    return json_value.dump();
}

bool SameSemanticPath(const SemanticFileReference& file, const std::string& path)
{
    return file.path == path;
}

void AddUniqueSemanticFile(
    std::vector<SemanticFileReference>& files,
    const SemanticFileReference& file)
{
    const auto it = std::find_if(files.begin(), files.end(), [&](const auto& existing) {
        return SameSemanticPath(existing, file.path);
    });
    if (it == files.end())
    {
        files.push_back(file);
    }
}

void RemoveSecurityProtectedFiles(
    std::vector<SemanticFileReference>& files,
    SemanticScopeResult& scope,
    const SecurityPolicy& policy)
{
    auto it = files.begin();
    while (it != files.end())
    {
        if (!IsProtectedPath(policy, it->path) && !IsSecretPath(policy, it->path))
        {
            ++it;
            continue;
        }

        SemanticFileReference protected_file = *it;
        protected_file.reason = "SecurityPolicy protected path. Original reason: " + protected_file.reason;
        protected_file.confidence = 1.0;
        AddUniqueSemanticFile(scope.protected_files, protected_file);
        it = files.erase(it);
    }
}

void EnforceSecurityPolicy(
    SemanticScopeResult& scope,
    const SemanticAnalyzerInput& input)
{
    const auto policy = DefaultSecurityPolicy();
    RemoveSecurityProtectedFiles(scope.allowed_files, scope, policy);
    RemoveSecurityProtectedFiles(scope.context_files, scope, policy);
    RemoveSecurityProtectedFiles(scope.suspected_files, scope, policy);
    RemoveSecurityProtectedFiles(scope.needs_approval_files, scope, policy);

    for (const auto& file : input.relevant_files)
    {
        for (const auto& finding : DetectPromptInjectionText(policy, file.relative_path, file.content))
        {
            const std::string note =
                finding.type + ":" + finding.file + ":" + finding.reason;
            if (std::find(scope.notes.begin(), scope.notes.end(), note) == scope.notes.end())
            {
                scope.notes.push_back(note);
            }
            if (scope.risk_level == "low")
            {
                scope.risk_level = "medium";
            }
        }
    }
}
} // namespace

SemanticAnalyzerResult TryAnalyzeSemanticScope(
    const SemanticAnalyzerInput& input,
    const ILLMProvider& provider)
{
    SemanticAnalyzerResult result;
    result.prompt = BuildPrompt(input);

    try
    {
        result.raw_response = provider.GenerateText(result.prompt);
        result.extracted_json_text = ExtractJsonTextFromProviderResponse(result.raw_response);
        result.scope = nlohmann::json::parse(result.extracted_json_text).get<SemanticScopeResult>();
        EnforceSecurityPolicy(result.scope, input);
        if (result.scope.related_symbols.empty())
        {
            result.scope.related_symbols = input.impact.related_symbols;
        }
        if (result.scope.dependency_reasons.empty())
        {
            result.scope.dependency_reasons = input.impact.dependency_reasons;
        }
        if (result.scope.blast_radius == "low" && input.impact.blast_radius != "low")
        {
            result.scope.blast_radius = input.impact.blast_radius;
        }
        if (input.impact.public_interface_changes)
        {
            result.scope.public_interface_changes = true;
        }
        if (input.impact.public_header_risk && result.scope.risk_level == "low")
        {
            result.scope.risk_level = "medium";
        }
        result.success = true;
    }
    catch (const std::exception& exception)
    {
        result.error_message = std::string("Semantic analysis failed: ") + exception.what();
    }

    return result;
}
} // namespace agentguard
