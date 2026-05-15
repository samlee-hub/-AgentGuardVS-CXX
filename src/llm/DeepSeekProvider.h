#pragma once

#include "llm/ILLMProvider.h"
#include "llm/LLMProviderConfig.h"

namespace agentguard
{
class DeepSeekProvider final : public ILLMProvider
{
public:
    explicit DeepSeekProvider(LLMProviderConfig config);

    std::string GenerateText(std::string_view prompt) const override;
    nlohmann::json GenerateJson(std::string_view prompt) const override;

private:
    LLMProviderConfig config_;
};
} // namespace agentguard
