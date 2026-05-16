#include <chrono>
#include <filesystem>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "llm/CachedLLMProvider.h"

namespace
{
namespace fs = std::filesystem;

class CountingProvider final : public agentguard::ILLMProvider
{
public:
    explicit CountingProvider(std::string response)
        : response_(std::move(response))
    {
    }

    std::string GenerateText(std::string_view) const override
    {
        ++calls;
        return response_;
    }

    nlohmann::json GenerateJson(std::string_view prompt) const override
    {
        return nlohmann::json::parse(GenerateText(prompt));
    }

    mutable int calls = 0;

private:
    std::string response_;
};

fs::path MakeCacheDir()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path directory = fs::temp_directory_path() / ("agentguard_llm_cache_" + std::to_string(stamp));
    fs::create_directories(directory);
    return directory;
}
} // namespace

TEST(LLMCacheTest, CacheHitDoesNotCallProviderSecondTime)
{
    const fs::path cache_dir = MakeCacheDir();
    CountingProvider provider(R"({"ok":true})");
    const std::string key = agentguard::ComputeLLMCacheKey({
        {"kind", "analyze"},
        {"task", "Add summon cooldown"},
        {"project_index_hash", "project-hash"},
        {"relevant_file_hash", "file-hash"},
        {"provider", "fake"},
        {"model", "fake-model"}
    });

    agentguard::CachedLLMProvider first(provider, cache_dir, key, false);
    EXPECT_EQ(first.GenerateText("prompt"), R"({"ok":true})");
    EXPECT_EQ(provider.calls, 1);

    agentguard::CachedLLMProvider second(provider, cache_dir, key, false);
    EXPECT_EQ(second.GenerateText("prompt"), R"({"ok":true})");
    EXPECT_EQ(provider.calls, 1);
    EXPECT_TRUE(second.cache_hit());

    fs::remove_all(cache_dir);
}

TEST(LLMCacheTest, NoCacheForcesProviderCall)
{
    const fs::path cache_dir = MakeCacheDir();
    CountingProvider provider(R"({"ok":true})");
    const std::string key = agentguard::ComputeLLMCacheKey({
        {"kind", "review"},
        {"task", "Review summon cooldown"},
        {"project_index_hash", "project-hash"},
        {"relevant_file_hash", "file-hash"},
        {"diff_hash", "diff-hash"},
        {"build_log_hash", "build-log-hash"},
        {"provider", "fake"},
        {"model", "fake-model"}
    });

    agentguard::CachedLLMProvider first(provider, cache_dir, key, false);
    EXPECT_EQ(first.GenerateText("prompt"), R"({"ok":true})");
    EXPECT_EQ(provider.calls, 1);

    agentguard::CachedLLMProvider second(provider, cache_dir, key, true);
    EXPECT_EQ(second.GenerateText("prompt"), R"({"ok":true})");
    EXPECT_EQ(provider.calls, 2);
    EXPECT_FALSE(second.cache_hit());

    fs::remove_all(cache_dir);
}

TEST(LLMCacheTest, DoesNotCacheResponsesContainingSecrets)
{
    const fs::path cache_dir = MakeCacheDir();
    CountingProvider provider("OPENAI_API_KEY=placeholder-secret-value");
    const std::string key = agentguard::ComputeLLMCacheKey({
        {"kind", "analyze"},
        {"task", "Secret smoke"},
        {"provider", "fake"},
        {"model", "fake-model"}
    });

    agentguard::CachedLLMProvider first(provider, cache_dir, key, false);
    (void)first.GenerateText("prompt");
    agentguard::CachedLLMProvider second(provider, cache_dir, key, false);
    (void)second.GenerateText("prompt");

    EXPECT_EQ(provider.calls, 2);
    EXPECT_FALSE(second.cache_hit());

    fs::remove_all(cache_dir);
}
