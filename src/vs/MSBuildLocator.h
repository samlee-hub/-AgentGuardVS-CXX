#pragma once

#include <filesystem>
#include <string>

namespace agentguard
{
struct MSBuildLocatorOptions
{
    bool enable_environment = true;
    bool enable_vswhere = true;
    bool enable_common_paths = true;
    bool enable_path_lookup = true;
};

struct MSBuildLocationResult
{
    bool found = false;
    std::filesystem::path path;
    std::string message;
};

MSBuildLocationResult LocateMSBuild(const MSBuildLocatorOptions& options = {});
} // namespace agentguard
