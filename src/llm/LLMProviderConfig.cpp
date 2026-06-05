#include "llm/LLMProviderConfig.h"

#include <cstdlib>
#include <string>

namespace agentguard
{
namespace
{
std::string ReadEnvironmentString(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr)
    {
        return {};
    }
    return value;
}

std::string ReadEnvironmentStringOrDefault(const char* name, std::string default_value)
{
    const auto value = ReadEnvironmentString(name);
    return value.empty() ? std::move(default_value) : value;
}
} // namespace

LLMProviderConfig LoadLLMProviderConfigFromEnvironment()
{
    LLMProviderConfig config;
    config.provider = ReadEnvironmentStringOrDefault("AGENTGUARD_LLM_PROVIDER", "fake");
    config.openai_api_key = ReadEnvironmentString("OPENAI_API_KEY");
    config.openai_model = ReadEnvironmentString("AGENTGUARD_OPENAI_MODEL");
    config.openai_base_url = ReadEnvironmentStringOrDefault(
        "AGENTGUARD_OPENAI_BASE_URL",
        "https://api.openai.com/v1/responses");
    config.deepseek_api_key = ReadEnvironmentString("DEEPSEEK_API_KEY");
    config.deepseek_model = ReadEnvironmentString("AGENTGUARD_DEEPSEEK_MODEL");
    config.deepseek_base_url = ReadEnvironmentStringOrDefault(
        "AGENTGUARD_DEEPSEEK_BASE_URL",
        "https://api.deepseek.com/chat/completions");
    config.claude_api_key = ReadEnvironmentString("ANTHROPIC_API_KEY");
    config.claude_model = ReadEnvironmentString("AGENTGUARD_CLAUDE_MODEL");
    config.claude_base_url = ReadEnvironmentStringOrDefault(
        "AGENTGUARD_CLAUDE_BASE_URL",
        "https://api.anthropic.com/v1/messages");
    config.claude_api_version = ReadEnvironmentStringOrDefault(
        "AGENTGUARD_CLAUDE_API_VERSION",
        "2023-06-01");
    config.file_response_path = ReadEnvironmentString("AGENTGUARD_LLM_RESPONSE_FILE");
    return config;
}
} // namespace agentguard
