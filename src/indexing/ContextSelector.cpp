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

void AddTokenIfContains(
    const std::string& text,
    const std::string& utf8_phrase,
    std::vector<std::string>& tokens,
    std::initializer_list<const char*> hints)
{
    if (text.find(utf8_phrase) == std::string::npos)
    {
        return;
    }

    for (const char* hint : hints)
    {
        tokens.emplace_back(hint);
    }
}

std::vector<std::string> BuildRequestTokens(const std::string& text)
{
    std::vector<std::string> tokens = Tokenize(text);

    AddTokenIfContains(text, "\xE5\x8F\xAC\xE5\x94\xA4", tokens, {"summon"});
    AddTokenIfContains(text, "\xE8\xB5\x84\xE6\xBA\x90", tokens, {"resource", "cost"});
    AddTokenIfContains(text, "\xE6\xB6\x88\xE8\x80\x97", tokens, {"cost", "consume"});
    AddTokenIfContains(text, "\xE5\x86\xB7\xE5\x8D\xB4", tokens, {"cooldown"});
    AddTokenIfContains(text, "\xE5\x8C\xB9\xE9\x85\x8D", tokens, {"match", "matchmaking"});
    AddTokenIfContains(text, "\xE6\x88\x98\xE6\x96\x97", tokens, {"battle"});
    AddTokenIfContains(text, "\xE6\x88\xBF\xE9\x97\xB4", tokens, {"room"});
    AddTokenIfContains(text, "\xE7\x8A\xB6\xE6\x80\x81", tokens, {"status", "state"});
    AddTokenIfContains(text, "\xE7\x99\xBB\xE5\xBD\x95", tokens, {"login", "auth"});
    AddTokenIfContains(text, "\xE9\x85\x8D\xE7\xBD\xAE", tokens, {"config"});
    AddTokenIfContains(text, "\xE8\xAE\xB0\xE5\xBD\x95", tokens, {"record", "log"});

    std::sort(tokens.begin(), tokens.end());
    tokens.erase(std::unique(tokens.begin(), tokens.end()), tokens.end());
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
    const auto request_tokens = BuildRequestTokens(user_request);
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
