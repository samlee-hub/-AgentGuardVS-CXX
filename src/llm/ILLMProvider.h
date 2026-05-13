#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace agentguard
{
class ILLMProvider
{
public:
    virtual ~ILLMProvider() = default;

    virtual std::string GenerateText(std::string_view prompt) const = 0;
    virtual nlohmann::json GenerateJson(std::string_view prompt) const = 0;
};
} // namespace agentguard
