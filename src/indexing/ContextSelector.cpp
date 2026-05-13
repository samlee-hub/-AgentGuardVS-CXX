#include "indexing/ContextSelector.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>

namespace agentguard
{
namespace
{
std::vector<std::string> Tokenize(const std::string& text)
{
    const std::regex token_pattern(R"([A-Za-z0-9_]+)");
    std::vector<std::string> tokens;

    for (auto it = std::sregex_iterator(text.begin(), text.end(), token_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        std::string token = (*it)[0].str();
        std::transform(token.begin(), token.end(), token.begin(), [](const unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        tokens.push_back(std::move(token));
    }

    return tokens;
}

bool ContainsToken(const std::string& haystack, const std::string& needle)
{
    std::string lowered = haystack;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered.find(needle) != std::string::npos;
}

void ScoreValues(
    ContextCandidate& candidate,
    const std::vector<std::string>& values,
    const std::vector<std::string>& request_tokens,
    const std::string& reason_prefix,
    const int weight)
{
    for (const auto& token : request_tokens)
    {
        for (const auto& value : values)
        {
            if (!ContainsToken(value, token))
            {
                continue;
            }

            candidate.score += weight;
            candidate.reasons.push_back(reason_prefix + ":" + token);
            break;
        }
    }
}
} // namespace

std::vector<ContextCandidate> SelectRelevantFiles(
    const std::string& user_request,
    const std::vector<SourceFileInfo>& files)
{
    const auto request_tokens = Tokenize(user_request);
    std::vector<ContextCandidate> candidates;

    for (const auto& file : files)
    {
        ContextCandidate candidate;
        candidate.file_path = file.file_path;

        for (const auto& token : request_tokens)
        {
            if (ContainsToken(file.file_path, token))
            {
                candidate.score += 5;
                candidate.reasons.push_back("filename:" + token);
            }
        }

        ScoreValues(candidate, file.classes, request_tokens, "class", 4);
        ScoreValues(candidate, file.functions, request_tokens, "function", 3);
        ScoreValues(candidate, file.includes, request_tokens, "include", 2);
        ScoreValues(candidate, file.keywords, request_tokens, "keyword", 1);

        std::sort(candidate.reasons.begin(), candidate.reasons.end());
        candidate.reasons.erase(
            std::unique(candidate.reasons.begin(), candidate.reasons.end()),
            candidate.reasons.end());

        if (candidate.score > 0)
        {
            candidates.push_back(std::move(candidate));
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const ContextCandidate& left, const ContextCandidate& right) {
        if (left.score != right.score)
        {
            return left.score > right.score;
        }
        return left.file_path < right.file_path;
    });

    return candidates;
}
} // namespace agentguard
