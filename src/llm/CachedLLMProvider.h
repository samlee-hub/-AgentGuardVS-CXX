#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "llm/ILLMProvider.h"

namespace agentguard
{
struct LLMCacheKeyPart
{
    std::string name;
    std::string value;
};

std::string StableHashText(std::string_view text);
std::string ComputeLLMCacheKey(const std::vector<LLMCacheKeyPart>& parts);

class CachedLLMProvider final : public ILLMProvider
{
public:
    CachedLLMProvider(
        const ILLMProvider& inner_provider,
        std::filesystem::path cache_directory,
        std::string cache_key,
        bool no_cache);

    std::string GenerateText(std::string_view prompt) const override;
    nlohmann::json GenerateJson(std::string_view prompt) const override;

    bool cache_hit() const noexcept;

private:
    std::filesystem::path CachePath() const;

    const ILLMProvider& inner_provider_;
    std::filesystem::path cache_directory_;
    std::string cache_key_;
    bool no_cache_ = false;
    mutable bool cache_hit_ = false;
};
} // namespace agentguard
