#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Models.h"

namespace agentguard
{
struct SlnParseResult
{
    std::filesystem::path solution_path;
    std::vector<VSProjectInfo> projects;
    std::vector<std::string> warnings;
};

SlnParseResult ParseSln(const std::filesystem::path& sln_path);
} // namespace agentguard
