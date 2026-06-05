#include "llm/FileLLMProvider.h"

#include <fstream>
#include <stdexcept>
#include <utility>

namespace agentguard
{
FileLLMProvider::FileLLMProvider(std::filesystem::path response_file)
    : response_file_(std::move(response_file))
{
}

std::string FileLLMProvider::GenerateText(std::string_view) const
{
    return ReadFileContents();
}

nlohmann::json FileLLMProvider::GenerateJson(std::string_view) const
{
    return nlohmann::json::parse(ReadFileContents());
}

std::string FileLLMProvider::ReadFileContents() const
{
    std::ifstream input(response_file_, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to open LLM response file: " + response_file_.string());
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}
} // namespace agentguard
