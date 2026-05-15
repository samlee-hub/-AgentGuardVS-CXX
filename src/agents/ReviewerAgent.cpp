#include "agents/ReviewerAgent.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace agentguard
{
namespace
{
constexpr std::size_t kMaxDiffChars = 6000;
constexpr std::size_t kMaxSnippetChars = 900;

std::string Truncate(const std::string& value, std::size_t max_chars, bool& truncated)
{
    if (value.size() <= max_chars)
    {
        truncated = false;
        return value;
    }

    truncated = true;
    return value.substr(0, max_chars);
}

std::string NormalizePathText(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.rfind("./", 0) == 0)
    {
        path.erase(0, 2);
    }
    return path;
}

std::unordered_set<std::string> PathSet(const std::vector<SemanticFileReference>& files)
{
    std::unordered_set<std::string> paths;
    for (const auto& file : files)
    {
        paths.insert(NormalizePathText(file.path));
    }
    return paths;
}

std::unordered_map<std::string, SemanticFileReference> FileMap(
    const std::vector<SemanticFileReference>& files)
{
    std::unordered_map<std::string, SemanticFileReference> by_path;
    for (const auto& file : files)
    {
        by_path.emplace(NormalizePathText(file.path), file);
    }
    return by_path;
}

bool ContainsPath(const std::unordered_set<std::string>& paths, const std::string& path)
{
    return paths.contains(NormalizePathText(path));
}

bool HasHighRisk(const SemanticReviewResult& review)
{
    for (const auto& risk : review.risks)
    {
        if (risk.severity == "high" || risk.severity == "critical")
        {
            return true;
        }
    }
    return false;
}

void AddNote(SemanticReviewResult& review, const std::string& note)
{
    if (std::find(review.notes.begin(), review.notes.end(), note) == review.notes.end())
    {
        review.notes.push_back(note);
    }
}

void AddUniqueSuggestion(
    SemanticReviewResult& review,
    const SemanticFileReference& suggestion)
{
    const auto normalized_path = NormalizePathText(suggestion.path);
    const auto found = std::find_if(
        review.suggested_scope_additions.begin(),
        review.suggested_scope_additions.end(),
        [&](const SemanticFileReference& existing) {
            return NormalizePathText(existing.path) == normalized_path;
        });
    if (found == review.suggested_scope_additions.end())
    {
        review.suggested_scope_additions.push_back(suggestion);
    }
}

std::string BuildPrompt(const SemanticReviewInput& input)
{
    std::ostringstream prompt;
    prompt << "You are doing semantic review for a Visual Studio C++ coding task.\n";
    prompt << "Return strict JSON only. Do not emit markdown or prose.\n";
    prompt << "Output must be a SemanticReviewResult with this exact JSON shape:\n";
    prompt << "{meets_requirement:bool, requires_scope_expansion:bool, ";
    prompt << "suggested_scope_additions:[{path,reason,confidence}], ";
    prompt << "risks:[{type,file,reason,severity}], confidence:number, ";
    prompt << "next_action:accept|repair|expand_scope|ask_user|stop, notes:string[]}.\n";
    prompt << "Do not return risks as a string. Do not return notes as a string. ";
    prompt << "Use [] when there are no risks, notes, or suggested scope additions.\n";
    prompt << "next_action must be one of accept, repair, expand_scope, ask_user, stop.\n";
    prompt << "Rules: protected files force stop; needs_approval files force ask_user; ";
    prompt << "build failures in suspected files should recommend expand_scope.\n";
    prompt << "User task: " << input.user_request << "\n";
    prompt << "Current semantic scope:\n" << nlohmann::json(input.semantic_scope).dump(2) << "\n";
    prompt << "Build success: " << (input.build_result.success ? "true" : "false") << "\n";
    prompt << "Build return code: " << input.build_result.return_code << "\n";
    prompt << "Parsed build errors:\n";
    for (const auto& error : input.parsed_errors)
    {
        prompt << "- " << error.file << "(" << error.line << "," << error.column << ") "
               << error.code << ": " << error.message << "\n";
    }

    bool diff_truncated = false;
    const auto diff_text = Truncate(input.diff_text, kMaxDiffChars, diff_truncated);
    prompt << "Diff truncated=" << (diff_truncated ? "true" : "false") << "\n";
    prompt << diff_text << "\n";

    prompt << "Relevant files:\n";
    for (const auto& file : input.relevant_files)
    {
        bool snippet_truncated = false;
        const auto snippet = Truncate(file.content, kMaxSnippetChars, snippet_truncated);
        prompt << "FILE " << file.relative_path
               << " truncated=" << (snippet_truncated ? "true" : "false") << "\n";
        prompt << snippet << "\n";
    }

    return prompt.str();
}

std::string ExtractJsonTextFromProviderResponse(const std::string& raw_response)
{
    const auto json_value = nlohmann::json::parse(raw_response);

    if (json_value.contains("next_action"))
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

nlohmann::json NormalizeReviewJson(nlohmann::json json_value)
{
    if (json_value.contains("risks") && json_value.at("risks").is_string())
    {
        const auto reason = json_value.at("risks").get<std::string>();
        json_value["risks"] = nlohmann::json::array({
            {
                {"type", "semantic"},
                {"file", ""},
                {"reason", reason},
                {"severity", "medium"}
            }
        });
    }

    if (json_value.contains("notes") && json_value.at("notes").is_string())
    {
        json_value["notes"] = nlohmann::json::array({json_value.at("notes").get<std::string>()});
    }

    if (json_value.contains("suggested_scope_additions") &&
        json_value.at("suggested_scope_additions").is_array())
    {
        nlohmann::json normalized_additions = nlohmann::json::array();
        for (const auto& addition : json_value.at("suggested_scope_additions"))
        {
            if (addition.is_object())
            {
                normalized_additions.push_back(addition);
                continue;
            }
            if (addition.is_string())
            {
                json_value["notes"].push_back(
                    "Unstructured suggested_scope_addition ignored: " +
                    addition.get<std::string>());
            }
        }
        json_value["suggested_scope_additions"] = std::move(normalized_additions);
    }

    return json_value;
}

void ApplyReviewSafetyRules(
    const SemanticReviewInput& input,
    SemanticReviewResult& review)
{
    const auto protected_paths = PathSet(input.semantic_scope.protected_files);
    const auto needs_approval_paths = PathSet(input.semantic_scope.needs_approval_files);
    const auto suspected_files = FileMap(input.semantic_scope.suspected_files);

    for (const auto& error : input.parsed_errors)
    {
        if (ContainsPath(protected_paths, error.file))
        {
            review.next_action = "stop";
            review.meets_requirement = false;
            review.requires_scope_expansion = false;
            AddNote(review, "Build error references a protected file.");
            return;
        }
        if (ContainsPath(needs_approval_paths, error.file))
        {
            review.next_action = "ask_user";
            review.meets_requirement = false;
            AddNote(review, "Build error references a file requiring user approval.");
            return;
        }
    }

    for (const auto& risk : review.risks)
    {
        if (ContainsPath(protected_paths, risk.file))
        {
            review.next_action = "stop";
            review.meets_requirement = false;
            review.requires_scope_expansion = false;
            AddNote(review, "Review risk references a protected file.");
            return;
        }
        if (ContainsPath(needs_approval_paths, risk.file))
        {
            review.next_action = "ask_user";
            review.meets_requirement = false;
            AddNote(review, "Review risk references a file requiring user approval.");
            return;
        }
    }

    for (const auto& suggestion : review.suggested_scope_additions)
    {
        if (ContainsPath(protected_paths, suggestion.path))
        {
            review.next_action = "stop";
            review.meets_requirement = false;
            review.requires_scope_expansion = false;
            AddNote(review, "Scope suggestion targets a protected file.");
            return;
        }
        if (ContainsPath(needs_approval_paths, suggestion.path))
        {
            review.next_action = "ask_user";
            review.meets_requirement = false;
            AddNote(review, "Scope suggestion targets a file requiring user approval.");
            return;
        }
    }

    if (!input.build_result.success && !input.parsed_errors.empty())
    {
        bool all_errors_are_suspected = true;
        for (const auto& error : input.parsed_errors)
        {
            if (!suspected_files.contains(NormalizePathText(error.file)))
            {
                all_errors_are_suspected = false;
                break;
            }
        }

        if (all_errors_are_suspected)
        {
            review.next_action = "expand_scope";
            review.requires_scope_expansion = true;
            review.meets_requirement = false;
            for (const auto& error : input.parsed_errors)
            {
                const auto found = suspected_files.find(NormalizePathText(error.file));
                if (found != suspected_files.end())
                {
                    AddUniqueSuggestion(review, found->second);
                }
            }
            AddNote(review, "Build errors are concentrated in suspected files.");
            return;
        }
    }

    if (input.build_result.success && review.next_action == "accept" && HasHighRisk(review))
    {
        review.next_action = "repair";
        review.meets_requirement = false;
        AddNote(review, "High semantic risk prevents direct accept.");
    }
}
} // namespace

SemanticReviewAgentResult TryReviewSemanticChange(
    const SemanticReviewInput& input,
    const ILLMProvider& provider)
{
    SemanticReviewAgentResult result;
    result.prompt = BuildPrompt(input);

    try
    {
        result.raw_response = provider.GenerateText(result.prompt);
        result.extracted_json_text = ExtractJsonTextFromProviderResponse(result.raw_response);
        result.review = NormalizeReviewJson(nlohmann::json::parse(result.extracted_json_text))
            .get<SemanticReviewResult>();
        ApplyReviewSafetyRules(input, result.review);
        result.success = true;
    }
    catch (const std::exception& exception)
    {
        result.error_message = std::string("Semantic review failed: ") + exception.what();
    }

    return result;
}
} // namespace agentguard
