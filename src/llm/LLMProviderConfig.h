#pragma once

#include <filesystem>
#include <string>

namespace agentguard
{
struct LLMProviderConfig
{
    std::string provider = "fake";
    std::string openai_api_key;
    std::string openai_model;
    std::string openai_base_url = "https://api.openai.com/v1/responses";
    std::string deepseek_api_key;
    std::string deepseek_model;
    std::string deepseek_base_url = "https://api.deepseek.com/chat/completions";
    std::string claude_api_key;
    std::string claude_model;
    std::string claude_base_url = "https://api.anthropic.com/v1/messages";
    std::string claude_api_version = "2023-06-01";
    std::filesystem::path file_response_path;
};

LLMProviderConfig LoadLLMProviderConfigFromEnvironment();
} // namespace agentguard
