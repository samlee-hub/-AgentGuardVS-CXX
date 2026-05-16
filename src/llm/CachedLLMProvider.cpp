#include "llm/CachedLLMProvider.h"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/PathUtils.h"
#include "security/SecurityPolicy.h"

namespace agentguard
{
namespace
{
constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::string ToHex(const std::uint64_t value)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0') << std::setw(16) << value;
    return stream.str();
}

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to read LLM cache file.");
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content)
{
    EnsureDirectory(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Unable to write LLM cache file.");
    }
    output << content;
}

bool ContainsSensitiveText(const std::string& text)
{
    return RedactSensitiveText(text) != text;
}
} // namespace

std::string StableHashText(std::string_view text)
{
    std::uint64_t hash = kFnvOffset;
    for (const unsigned char ch : text)
    {
        hash ^= ch;
        hash *= kFnvPrime;
    }
    return ToHex(hash);
}

std::string ComputeLLMCacheKey(const std::vector<LLMCacheKeyPart>& parts)
{
    std::string material;
    for (const auto& part : parts)
    {
        material += part.name;
        material += '\0';
        material += part.value;
        material += '\0';
    }
    return StableHashText(material);
}

CachedLLMProvider::CachedLLMProvider(
    const ILLMProvider& inner_provider,
    std::filesystem::path cache_directory,
    std::string cache_key,
    const bool no_cache)
    : inner_provider_(inner_provider)
    , cache_directory_(std::move(cache_directory))
    , cache_key_(std::move(cache_key))
    , no_cache_(no_cache)
{
}

std::filesystem::path CachedLLMProvider::CachePath() const
{
    return cache_directory_ / (cache_key_ + ".json");
}

std::string CachedLLMProvider::GenerateText(std::string_view prompt) const
{
    cache_hit_ = false;
    if (!no_cache_ && !cache_key_.empty())
    {
        const auto path = CachePath();
        if (std::filesystem::exists(path))
        {
            cache_hit_ = true;
            return ReadTextFile(path);
        }
    }

    const std::string response = inner_provider_.GenerateText(prompt);
    if (!no_cache_ && !cache_key_.empty() && !ContainsSensitiveText(response))
    {
        WriteTextFile(CachePath(), response);
    }
    return response;
}

nlohmann::json CachedLLMProvider::GenerateJson(std::string_view prompt) const
{
    return nlohmann::json::parse(GenerateText(prompt));
}

bool CachedLLMProvider::cache_hit() const noexcept
{
    return cache_hit_;
}
} // namespace agentguard
