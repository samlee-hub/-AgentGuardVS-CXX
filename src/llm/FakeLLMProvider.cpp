#include "llm/FakeLLMProvider.h"

#include <utility>

namespace agentguard
{
FakeLLMProvider::FakeLLMProvider(std::string text_response, nlohmann::json json_response)
    : text_response_(std::move(text_response))
    , json_response_(std::move(json_response))
{
}

std::string FakeLLMProvider::GenerateText(std::string_view) const
{
    return text_response_;
}

nlohmann::json FakeLLMProvider::GenerateJson(std::string_view) const
{
    return json_response_;
}
} // namespace agentguard
