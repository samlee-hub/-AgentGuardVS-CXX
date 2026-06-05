#include "llm/OpenAIProvider.h"

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
    if (config.openai_api_key.empty())
    {
        throw std::runtime_error("OPENAI_API_KEY is required when AGENTGUARD_LLM_PROVIDER=openai.");
    }
    if (config.openai_model.empty())
    {
        throw std::runtime_error("AGENTGUARD_OPENAI_MODEL is required when AGENTGUARD_LLM_PROVIDER=openai.");
    }
    if (config.openai_base_url.empty())
    {
        throw std::runtime_error("AGENTGUARD_OPENAI_BASE_URL cannot be empty.");
    }
}
} // namespace

OpenAIProvider::OpenAIProvider(LLMProviderConfig config)
    : config_(std::move(config))
{
}

std::string OpenAIProvider::GenerateText(std::string_view prompt) const
{
    ValidateConfigForRequest(config_);

    const nlohmann::json request_body{
        {"model", config_.openai_model},
        {"input", std::string(prompt)}
    };

    const auto response = PostJson(
        config_.openai_base_url,
        {HttpHeader{"Authorization", "Bearer " + config_.openai_api_key}},
        request_body.dump(),
        "AGENTGUARD_OPENAI_BASE_URL");
    if (response.status_code < 200 || response.status_code >= 300)
    {
        std::ostringstream error;
        error << "OpenAI Responses API request failed with HTTP status "
              << response.status_code << ": " << response.body;
        throw std::runtime_error(error.str());
    }

    return response.body;
}

nlohmann::json OpenAIProvider::GenerateJson(std::string_view prompt) const
{
    return nlohmann::json::parse(GenerateText(prompt));
}
} // namespace agentguard
