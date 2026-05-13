#pragma once

#include <string>

#include "llm/ILLMProvider.h"

namespace agentguard
{
class FakeLLMProvider final : public ILLMProvider
{
public:
    FakeLLMProvider(std::string text_response, nlohmann::json json_response);

    std::string GenerateText(std::string_view prompt) const override;
    nlohmann::json GenerateJson(std::string_view prompt) const override;

private:
    std::string text_response_;
    nlohmann::json json_response_;
};
} // namespace agentguard
