#include "project/CommandVerifier.h"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <stdexcept>

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

ProcessRequest MakeProcessRequest(const ProjectProfile& profile, const CommandStep& step)
{
    ProcessRequest request;
    request.executable = WidenUtf8(step.executable);
    for (const auto& argument : step.arguments)
    {
        request.arguments.push_back(WidenUtf8(argument));
    }
    request.working_directory = profile.root;
    return request;
}
} // namespace

CommandVerificationResult VerifyCommandProfile(
    const ProjectProfile& profile,
    const IProcessRunner& runner)
{
    CommandVerificationResult verification;
    verification.success = true;

    for (const auto& step : profile.verify_steps)
    {
        CommandStepResult step_result;
        step_result.name = step.name;
        step_result.executable = step.executable;
        step_result.arguments = step.arguments;

        try
        {
            const auto process_result = runner.Run(MakeProcessRequest(profile, step));
            step_result.return_code = process_result.return_code;
            step_result.stdout_text = process_result.stdout_text;
            step_result.stderr_text = process_result.stderr_text;
            step_result.duration_ms = process_result.duration_ms;
            step_result.success = process_result.return_code == 0;
        }
        catch (const std::exception& exception)
        {
            step_result.return_code = -1;
            step_result.stderr_text = exception.what();
            step_result.success = false;
        }

        if (!step_result.success)
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
        {"executable", result.executable},
        {"arguments", result.arguments},
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
