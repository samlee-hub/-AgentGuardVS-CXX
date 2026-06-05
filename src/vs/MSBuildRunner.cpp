#include "vs/MSBuildRunner.h"

#include <sstream>

#include "vs/MSBuildLocator.h"

namespace agentguard
{
namespace
{
std::wstring ToWide(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string RenderCommand(
    const std::filesystem::path& executable,
    const std::filesystem::path& solution_path,
    const std::string& configuration,
    const std::string& platform)
{
    std::ostringstream command;
    command << executable.string()
            << " " << solution_path.string()
            << " /m"
            << " /p:Configuration=" << configuration
            << " /p:Platform=" << platform
            << " /v:minimal";
    return command.str();
}
} // namespace

BuildResult BuildSolution(
    const std::filesystem::path& solution_path,
    const std::string& configuration,
    const std::string& platform,
    const std::filesystem::path& msbuild_path,
    const IProcessRunner& runner)
{
    std::filesystem::path resolved_msbuild = msbuild_path;
    if (resolved_msbuild.empty())
    {
        const auto located = LocateMSBuild();
        if (!located.found)
        {
            BuildResult missing;
            missing.success = false;
            missing.return_code = -1;
            missing.stderr_text = located.message;
            return missing;
        }
        resolved_msbuild = located.path;
    }

    ProcessRequest request;
    request.executable = resolved_msbuild.wstring();
    request.arguments = {
        solution_path.wstring(),
        L"/m",
        ToWide("/p:Configuration=" + configuration),
        ToWide("/p:Platform=" + platform),
        L"/v:minimal"
    };

    const auto process_result = runner.Run(request);

    BuildResult build_result;
    build_result.success = process_result.return_code == 0;
    build_result.command = RenderCommand(resolved_msbuild, solution_path, configuration, platform);
    build_result.return_code = process_result.return_code;
    build_result.stdout_text = process_result.stdout_text;
    build_result.stderr_text = process_result.stderr_text;
    build_result.duration_ms = process_result.duration_ms;
    return build_result;
}
} // namespace agentguard
