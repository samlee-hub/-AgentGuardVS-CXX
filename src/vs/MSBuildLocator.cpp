#include "vs/MSBuildLocator.h"

#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "core/ProcessRunner.h"

namespace agentguard
{
namespace
{
MSBuildLocationResult Found(const std::filesystem::path& path, const std::string& message)
{
    return MSBuildLocationResult{true, path, message};
}

std::filesystem::path ReadEnvironmentPath()
{
    const char* value = std::getenv("MSBUILD_PATH");
    if (value == nullptr || std::string(value).empty())
    {
        return {};
    }
    return std::filesystem::path(value);
}

std::filesystem::path VsWherePath()
{
    const std::filesystem::path candidate =
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    return std::filesystem::exists(candidate) ? candidate : std::filesystem::path{};
}

std::vector<std::filesystem::path> BuildVsWhereCandidates()
{
    const auto vswhere = VsWherePath();
    if (vswhere.empty())
    {
        return {};
    }

    ProcessRequest request;
    request.executable = vswhere.wstring();
    request.arguments = {
        L"-latest",
        L"-products",
        L"*",
        L"-property",
        L"installationPath"
    };

    ProcessResult result;
    try
    {
        result = ProcessRunner().Run(request);
    }
    catch (...)
    {
        return {};
    }

    if (result.return_code != 0)
    {
        return {};
    }

    std::istringstream lines(result.stdout_text);
    std::string installation_path;
    std::getline(lines, installation_path);
    if (installation_path.empty())
    {
        return {};
    }

    const std::filesystem::path root = installation_path;
    return {
        root / "MSBuild" / "Current" / "Bin" / "MSBuild.exe",
        root / "MSBuild" / "15.0" / "Bin" / "MSBuild.exe"
    };
}

std::vector<std::filesystem::path> CommonCandidates()
{
    return {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe"
    };
}

std::filesystem::path FindOnPath()
{
    wchar_t buffer[MAX_PATH];
    const DWORD result = SearchPathW(nullptr, L"MSBuild.exe", nullptr, MAX_PATH, buffer, nullptr);
    if (result == 0 || result >= MAX_PATH)
    {
        return {};
    }
    return std::filesystem::path(buffer);
}
} // namespace

MSBuildLocationResult LocateMSBuild(const MSBuildLocatorOptions& options)
{
    if (options.enable_environment)
    {
        const auto env_path = ReadEnvironmentPath();
        if (!env_path.empty() && std::filesystem::exists(env_path))
        {
            return Found(env_path, "MSBuild resolved from MSBUILD_PATH.");
        }
    }

    if (options.enable_vswhere)
    {
        for (const auto& candidate : BuildVsWhereCandidates())
        {
            if (std::filesystem::exists(candidate))
            {
                return Found(candidate, "MSBuild resolved from vswhere.");
            }
        }
    }

    if (options.enable_common_paths)
    {
        for (const auto& candidate : CommonCandidates())
        {
            if (std::filesystem::exists(candidate))
            {
                return Found(candidate, "MSBuild resolved from a common Visual Studio path.");
            }
        }
    }

    if (options.enable_path_lookup)
    {
        const auto path_candidate = FindOnPath();
        if (!path_candidate.empty())
        {
            return Found(path_candidate, "MSBuild resolved from PATH.");
        }
    }

    return MSBuildLocationResult{
        false,
        {},
        "MSBuild not found via MSBUILD_PATH, vswhere, common paths, or PATH."
    };
}
} // namespace agentguard
