#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/ProcessRunner.h"
#include "project/ProjectProfile.h"

namespace agentguard
{
struct CommandStepResult
{
    std::string name;
    std::string command;
    std::vector<std::string> args;
    std::string working_directory;
    int timeout_seconds = 0;
    bool required = true;
    bool skipped = false;
    std::string status;
    int return_code = -1;
    std::string stdout_text;
    std::string stderr_text;
    std::int64_t duration_ms = 0;
    bool success = false;
};

struct CommandVerificationResult
{
    bool success = false;
    std::vector<CommandStepResult> steps;
};

CommandVerificationResult VerifyCommandProfile(
    const ProjectProfile& profile,
    const IProcessRunner& runner);

void to_json(nlohmann::json& json_value, const CommandStepResult& result);
void to_json(nlohmann::json& json_value, const CommandVerificationResult& result);
} // namespace agentguard
