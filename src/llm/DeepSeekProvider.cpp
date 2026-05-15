#include "llm/DeepSeekProvider.h"

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
    if (config.deepseek_api_key.empty())
    {
        throw std::runtime_error("DEEPSEEK_API_KEY is required when AGENTGUARD_LLM_PROVIDER=deepseek.");
    }
    if (config.deepseek_model.empty())
    {
        throw std::runtime_error("AGENTGUARD_DEEPSEEK_MODEL is required when AGENTGUARD_LLM_PROVIDER=deepseek.");
    }
    if (config.deepseek_base_url.empty())
    {
        throw std::runtime_error("AGENTGUARD_DEEPSEEK_BASE_URL cannot be empty.");
    }
}
} // namespace

DeepSeekProvider::DeepSeekProvider(LLMProviderConfig config)
    : config_(std::move(config))
{
}

std::string DeepSeekProvider::GenerateText(std::string_view prompt) const
{
    ValidateConfigForRequest(config_);

    const nlohmann::json request_body{
        {"model", config_.deepseek_model},
        {"messages", nlohmann::json::array({
            {
                {"role", "user"},
                {"content", std::string(prompt)}
            }
        })}
    };

    const auto response = PostJson(
        config_.deepseek_base_url,
        {HttpHeader{"Authorization", "Bearer " + config_.deepseek_api_key}},
        request_body.dump(),
        "AGENTGUARD_DEEPSEEK_BASE_URL");
    if (response.status_code < 200 || response.status_code >= 300)
    {
        std::ostringstream error;
        error << "DeepSeek API request failed with HTTP status "
              << response.status_code << ": " << response.body;
        throw std::runtime_error(error.str());
    }

    return response.body;
}

nlohmann::json DeepSeekProvider::GenerateJson(std::string_view prompt) const
{
    return nlohmann::json::parse(GenerateText(prompt));
}
} // namespace agentguard
