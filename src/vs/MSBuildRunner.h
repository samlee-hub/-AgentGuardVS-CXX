#pragma once

#include <filesystem>
#include <string>

#include "core/Models.h"
#include "core/ProcessRunner.h"

namespace agentguard
{
BuildResult BuildSolution(
    const std::filesystem::path& solution_path,
    const std::string& configuration = "Debug",
    const std::string& platform = "x64",
    const std::filesystem::path& msbuild_path = {},
    const IProcessRunner& runner = ProcessRunner{});
} // namespace agentguard
