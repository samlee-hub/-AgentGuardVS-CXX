#include "llm/ClaudeProvider.h"

#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "llm/HttpJsonClient.h"

namespace agentguard
{
namespace
{
void ValidateConfigForRequest(const LLMProviderConfig& config)
{
    if (config.claude_api_key.empty())
    {
        throw std::runtime_error("ANTHROPIC_API_KEY is required when AGENTGUARD_LLM_PROVIDER=claude.");
    }
    if (config.claude_model.empty())
    {
        throw std::runtime_error("AGENTGUARD_CLAUDE_MODEL is required when AGENTGUARD_LLM_PROVIDER=claude.");
    }
    if (config.claude_base_url.empty())
    {
        throw std::runtime_error("AGENTGUARD_CLAUDE_BASE_URL cannot be empty.");
    }
    if (config.claude_api_version.empty())
    {
        throw std::runtime_error("AGENTGUARD_CLAUDE_API_VERSION cannot be empty.");
    }
}
} // namespace

ClaudeProvider::ClaudeProvider(LLMProviderConfig config)
    : config_(std::move(config))
{
}

std::string ClaudeProvider::GenerateText(std::string_view prompt) const
{
    ValidateConfigForRequest(config_);

    const nlohmann::json request_body{
        {"model", config_.claude_model},
        {"max_tokens", 1024},
        {"messages", nlohmann::json::array({
            {
                {"role", "user"},
                {"content", std::string(prompt)}
            }
        })}
    };

    const auto response = PostJson(
        config_.claude_base_url,
        {
            HttpHeader{"x-api-key", config_.claude_api_key},
            HttpHeader{"anthropic-version", config_.claude_api_version}
        },
        request_body.dump(),
        "AGENTGUARD_CLAUDE_BASE_URL");
    if (response.status_code < 200 || response.status_code >= 300)
    {
        std::ostringstream error;
        error << "Claude Messages API request failed with HTTP status "
              << response.status_code << ": " << response.body;
        throw std::runtime_error(error.str());
    }

    return response.body;
}

nlohmann::json ClaudeProvider::GenerateJson(std::string_view prompt) const
{
    return nlohmann::json::parse(GenerateText(prompt));
}
} // namespace agentguard
