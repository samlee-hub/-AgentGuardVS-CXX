#include "project/CommandVerifier.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <stdexcept>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
std::wstring WidenUtf8(const std::string& value)
{
#ifdef _WIN32
    if (value.empty())
    {
        return {};
    }
    const int required_size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (required_size <= 0)
    {
        throw std::runtime_error("MultiByteToWideChar failed.");
    }
    std::wstring result(static_cast<std::size_t>(required_size), L'\0');
    const int converted = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        required_size);
    if (converted != required_size)
    {
        throw std::runtime_error("MultiByteToWideChar returned an unexpected size.");
    }
    return result;
#else
    return std::wstring(value.begin(), value.end());
#endif
}

std::wstring ResolveExecutablePath(const std::string& command)
{
    const std::filesystem::path command_path(command);
    if (command_path.has_parent_path())
    {
        return std::filesystem::exists(command_path) ? WidenUtf8(command) : std::wstring{};
    }
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    if (SearchPathW(nullptr, WidenUtf8(command).c_str(), nullptr, MAX_PATH, buffer, nullptr) > 0)
    {
        return buffer;
    }
    if (std::filesystem::path(command).extension().empty())
    {
        const std::string with_exe = command + ".exe";
        if (SearchPathW(nullptr, WidenUtf8(with_exe).c_str(), nullptr, MAX_PATH, buffer, nullptr) > 0)
        {
            return buffer;
        }
    }
    return {};
#else
    return WidenUtf8(command);
#endif
}

ProcessRequest MakeProcessRequest(const ProjectProfile& profile, const CommandStep& step)
{
    const auto relative_working_directory = SafeRelativePath(std::filesystem::path(step.working_directory.empty()
        ? "."
        : step.working_directory));
    const auto working_directory = NormalizePath(profile.root / relative_working_directory);
    if (!IsSubPath(working_directory, NormalizePath(profile.root)))
    {
        throw std::runtime_error("Verify command working_directory escapes project root.");
    }

    ProcessRequest request;
    auto resolved_executable = ResolveExecutablePath(step.command);
    request.executable = resolved_executable.empty() ? WidenUtf8(step.command) : std::move(resolved_executable);
    for (const auto& argument : step.args)
    {
        request.arguments.push_back(WidenUtf8(argument));
    }
    request.working_directory = working_directory;
    request.timeout_seconds = step.timeout_seconds;
    return request;
}
} // namespace

CommandVerificationResult VerifyCommandProfile(
    const ProjectProfile& profile,
    const IProcessRunner& runner)
{
    CommandVerificationResult verification;
    verification.success = true;

    for (const auto& step : profile.verify_commands)
    {
        CommandStepResult step_result;
        step_result.name = step.name;
        step_result.command = step.command;
        step_result.args = step.args;
        step_result.working_directory = step.working_directory.empty() ? "." : step.working_directory;
        step_result.timeout_seconds = step.timeout_seconds;
        step_result.required = step.required;

        try
        {
            if (ResolveExecutablePath(step.command).empty())
            {
                step_result.return_code = -1;
                step_result.stderr_text = "Executable not found: " + step.command;
                if (step.skip_if_missing_executable)
                {
                    step_result.skipped = true;
                    step_result.status = "SKIP";
                    step_result.success = true;
                }
                else
                {
                    step_result.status = "FAIL";
                    step_result.success = false;
                }
            }
            else
            {
                const auto process_result = runner.Run(MakeProcessRequest(profile, step));
                step_result.return_code = process_result.return_code;
                step_result.stdout_text = process_result.stdout_text;
                step_result.stderr_text = process_result.stderr_text;
                step_result.duration_ms = process_result.duration_ms;
                step_result.success = process_result.return_code == 0;
                step_result.status = step_result.success ? "PASS" : "FAIL";
            }
        }
        catch (const std::exception& exception)
        {
            step_result.return_code = -1;
            step_result.stderr_text = exception.what();
            step_result.success = false;
            step_result.status = "FAIL";
        }

        if (!step_result.success && step.required)
        {
            verification.success = false;
        }
        verification.steps.push_back(std::move(step_result));
    }

    return verification;
}

void to_json(nlohmann::json& json_value, const CommandStepResult& result)
{
    json_value = nlohmann::json{
        {"name", result.name},
        {"command", result.command},
        {"args", result.args},
        {"working_directory", result.working_directory},
        {"timeout_seconds", result.timeout_seconds},
        {"required", result.required},
        {"skipped", result.skipped},
        {"status", result.status},
        {"return_code", result.return_code},
        {"stdout", result.stdout_text},
        {"stderr", result.stderr_text},
        {"duration_ms", result.duration_ms},
        {"success", result.success}
    };
}

void to_json(nlohmann::json& json_value, const CommandVerificationResult& result)
{
    json_value = nlohmann::json{
        {"success", result.success},
        {"steps", result.steps}
    };
}
} // namespace agentguard
