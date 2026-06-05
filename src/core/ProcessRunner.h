#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace agentguard
{
struct ProcessRequest
{
    std::wstring executable;
    std::vector<std::wstring> arguments;
    std::filesystem::path working_directory;
    int timeout_seconds = 0;
};

struct ProcessResult
{
    int return_code = -1;
    std::string stdout_text;
    std::string stderr_text;
    std::int64_t duration_ms = 0;
};

class IProcessRunner
{
public:
    virtual ~IProcessRunner() = default;
    virtual ProcessResult Run(const ProcessRequest& request) const = 0;
};

class ProcessRunner final : public IProcessRunner
{
public:
    ProcessResult Run(const ProcessRequest& request) const override;
};
} // namespace agentguard
