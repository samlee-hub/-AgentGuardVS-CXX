#pragma once

#include <filesystem>

#include "llm/ILLMProvider.h"

namespace agentguard
{
class FileLLMProvider final : public ILLMProvider
{
public:
    explicit FileLLMProvider(std::filesystem::path response_file);

    std::string GenerateText(std::string_view prompt) const override;
    nlohmann::json GenerateJson(std::string_view prompt) const override;

private:
    std::string ReadFileContents() const;

    std::filesystem::path response_file_;
};
} // namespace agentguard
