#pragma once

#include <string>

#include "llm/ILLMProvider.h"
#include "llm/LLMProviderConfig.h"

namespace agentguard
{
class OpenAIProvider final : public ILLMProvider
{
public:
    explicit OpenAIProvider(LLMProviderConfig config);

    std::string GenerateText(std::string_view prompt) const override;
    nlohmann::json GenerateJson(std::string_view prompt) const override;

private:
    LLMProviderConfig config_;
};
} // namespace agentguard
