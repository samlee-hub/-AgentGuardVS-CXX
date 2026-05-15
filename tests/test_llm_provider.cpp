#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "llm/FakeLLMProvider.h"
#include "llm/FileLLMProvider.h"
#include "llm/ClaudeProvider.h"
#include "llm/DeepSeekProvider.h"
#include "llm/LLMProviderConfig.h"
#include "llm/OpenAIProvider.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::FakeLLMProvider;
using agentguard::FileLLMProvider;
using agentguard::ClaudeProvider;
using agentguard::DeepSeekProvider;
using agentguard::LLMProviderConfig;
using agentguard::LoadLLMProviderConfigFromEnvironment;
using agentguard::OpenAIProvider;

fs::path MakeTempFile()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::temp_directory_path() / ("agentguard_llm_" + std::to_string(stamp) + ".json");
}

void SetEnvVar(const char* name, const char* value)
{
    _putenv_s(name, value);
}

void ClearProviderEnv()
{
    SetEnvVar("AGENTGUARD_LLM_PROVIDER", "");
    SetEnvVar("AGENTGUARD_OPENAI_MODEL", "");
    SetEnvVar("OPENAI_API_KEY", "");
    SetEnvVar("AGENTGUARD_OPENAI_BASE_URL", "");
    SetEnvVar("DEEPSEEK_API_KEY", "");
    SetEnvVar("AGENTGUARD_DEEPSEEK_MODEL", "");
    SetEnvVar("AGENTGUARD_DEEPSEEK_BASE_URL", "");
    SetEnvVar("ANTHROPIC_API_KEY", "");
    SetEnvVar("AGENTGUARD_CLAUDE_MODEL", "");
    SetEnvVar("AGENTGUARD_CLAUDE_BASE_URL", "");
    SetEnvVar("AGENTGUARD_CLAUDE_API_VERSION", "");
}

TEST(LLMProviderTest, FakeProviderReturnsConfiguredTextAndJson)
{
    FakeLLMProvider provider(
        "plain-text-response",
        nlohmann::json{{"task_id", "fake-task"}});

    EXPECT_EQ(provider.GenerateText("prompt"), "plain-text-response");
    EXPECT_EQ(provider.GenerateJson("prompt").at("task_id").get<std::string>(), "fake-task");
}

TEST(LLMProviderTest, FileProviderReadsJsonFromDisk)
{
    const fs::path file_path = MakeTempFile();
    {
        std::ofstream output(file_path);
        output << R"({"changes":[{"target_file":"src/Test.cpp"}]})";
    }

    FileLLMProvider provider(file_path);

    EXPECT_NE(provider.GenerateText("prompt").find("src/Test.cpp"), std::string::npos);
    EXPECT_EQ(
        provider.GenerateJson("prompt").at("changes").at(0).at("target_file").get<std::string>(),
        "src/Test.cpp");

    fs::remove(file_path);
}

TEST(LLMProviderConfigTest, DefaultsToFakeProviderAndResponsesBaseUrl)
{
    ClearProviderEnv();

    const auto config = LoadLLMProviderConfigFromEnvironment();

    EXPECT_EQ(config.provider, "fake");
    EXPECT_EQ(config.openai_base_url, "https://api.openai.com/v1/responses");
    EXPECT_TRUE(config.openai_api_key.empty());
    EXPECT_TRUE(config.openai_model.empty());
}

TEST(LLMProviderConfigTest, ReadsOpenAISettingsFromEnvironment)
{
    ClearProviderEnv();
    SetEnvVar("AGENTGUARD_LLM_PROVIDER", "openai");
    SetEnvVar("OPENAI_API_KEY", "test-api-key");
    SetEnvVar("AGENTGUARD_OPENAI_MODEL", "test-model");
    SetEnvVar("AGENTGUARD_OPENAI_BASE_URL", "https://example.test/v1/responses");

    const auto config = LoadLLMProviderConfigFromEnvironment();

    EXPECT_EQ(config.provider, "openai");
    EXPECT_EQ(config.openai_api_key, "test-api-key");
    EXPECT_EQ(config.openai_model, "test-model");
    EXPECT_EQ(config.openai_base_url, "https://example.test/v1/responses");
}

TEST(LLMProviderConfigTest, ReadsDeepSeekSettingsFromEnvironment)
{
    ClearProviderEnv();
    SetEnvVar("AGENTGUARD_LLM_PROVIDER", "deepseek");
    SetEnvVar("DEEPSEEK_API_KEY", "deepseek-test-key");
    SetEnvVar("AGENTGUARD_DEEPSEEK_MODEL", "deepseek-chat");
    SetEnvVar("AGENTGUARD_DEEPSEEK_BASE_URL", "https://example.test/chat/completions");

    const auto config = LoadLLMProviderConfigFromEnvironment();

    EXPECT_EQ(config.provider, "deepseek");
    EXPECT_EQ(config.deepseek_api_key, "deepseek-test-key");
    EXPECT_EQ(config.deepseek_model, "deepseek-chat");
    EXPECT_EQ(config.deepseek_base_url, "https://example.test/chat/completions");
}

TEST(LLMProviderConfigTest, ReadsClaudeSettingsFromEnvironment)
{
    ClearProviderEnv();
    SetEnvVar("AGENTGUARD_LLM_PROVIDER", "claude");
    SetEnvVar("ANTHROPIC_API_KEY", "anthropic-test-key");
    SetEnvVar("AGENTGUARD_CLAUDE_MODEL", "claude-test-model");
    SetEnvVar("AGENTGUARD_CLAUDE_BASE_URL", "https://example.test/v1/messages");
    SetEnvVar("AGENTGUARD_CLAUDE_API_VERSION", "2023-06-01");

    const auto config = LoadLLMProviderConfigFromEnvironment();

    EXPECT_EQ(config.provider, "claude");
    EXPECT_EQ(config.claude_api_key, "anthropic-test-key");
    EXPECT_EQ(config.claude_model, "claude-test-model");
    EXPECT_EQ(config.claude_base_url, "https://example.test/v1/messages");
    EXPECT_EQ(config.claude_api_version, "2023-06-01");
}

TEST(OpenAIProviderTest, MissingApiKeyReturnsClearErrorBeforeNetworkCall)
{
    LLMProviderConfig config;
    config.provider = "openai";
    config.openai_model = "test-model";
    config.openai_base_url = "https://api.openai.com/v1/responses";

    const OpenAIProvider provider(config);

    try
    {
        (void)provider.GenerateText("Return OK.");
        FAIL() << "Expected missing API key error.";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("OPENAI_API_KEY"), std::string::npos);
    }
}

TEST(DeepSeekProviderTest, MissingApiKeyReturnsClearErrorBeforeNetworkCall)
{
    LLMProviderConfig config;
    config.provider = "deepseek";
    config.deepseek_model = "deepseek-chat";
    config.deepseek_base_url = "https://api.deepseek.com/chat/completions";

    const DeepSeekProvider provider(config);

    try
    {
        (void)provider.GenerateText("Return OK.");
        FAIL() << "Expected missing API key error.";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("DEEPSEEK_API_KEY"), std::string::npos);
    }
}

TEST(ClaudeProviderTest, MissingApiKeyReturnsClearErrorBeforeNetworkCall)
{
    LLMProviderConfig config;
    config.provider = "claude";
    config.claude_model = "claude-test-model";
    config.claude_base_url = "https://api.anthropic.com/v1/messages";
    config.claude_api_version = "2023-06-01";

    const ClaudeProvider provider(config);

    try
    {
        (void)provider.GenerateText("Return OK.");
        FAIL() << "Expected missing API key error.";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("ANTHROPIC_API_KEY"), std::string::npos);
    }
}
} // namespace
