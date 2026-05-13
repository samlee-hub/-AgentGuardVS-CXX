#include "core/ProcessRunner.h"

#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <stdexcept>
#include <thread>

namespace agentguard
{
namespace
{
std::wstring QuoteArgument(const std::wstring& argument)
{
    if (argument.find_first_of(L" \t\"") == std::wstring::npos)
    {
        return argument;
    }

    std::wstring quoted = L"\"";
    for (const wchar_t ch : argument)
    {
        if (ch == L'"')
        {
            quoted += L"\\\"";
        }
        else
        {
            quoted.push_back(ch);
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring BuildCommandLine(const ProcessRequest& request)
{
    std::wstring command_line = QuoteArgument(request.executable);
    for (const auto& argument : request.arguments)
    {
        command_line += L" ";
        command_line += QuoteArgument(argument);
    }
    return command_line;
}

std::string ReadPipe(HANDLE read_handle)
{
    std::string output;
    char buffer[4096];
    DWORD bytes_read = 0;

    while (ReadFile(read_handle, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0)
    {
        output.append(buffer, buffer + bytes_read);
    }

    return output;
}
} // namespace

ProcessResult ProcessRunner::Run(const ProcessRequest& request) const
{
    if (request.executable.empty())
    {
        throw std::invalid_argument("Process executable cannot be empty.");
    }

    SECURITY_ATTRIBUTES security_attributes{};
    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    HANDLE stderr_read = nullptr;
    HANDLE stderr_write = nullptr;

    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0) ||
        !CreatePipe(&stderr_read, &stderr_write, &security_attributes, 0))
    {
        throw std::runtime_error("Unable to create child-process pipes.");
    }

    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(STARTUPINFOW);
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = stdout_write;
    startup_info.hStdError = stderr_write;

    PROCESS_INFORMATION process_info{};
    std::wstring command_line = BuildCommandLine(request);
    std::wstring working_directory = request.working_directory.empty()
        ? std::wstring{}
        : request.working_directory.wstring();

    const auto started = std::chrono::steady_clock::now();
    const BOOL created = CreateProcessW(
        request.executable.c_str(),
        command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        working_directory.empty() ? nullptr : working_directory.c_str(),
        &startup_info,
        &process_info);

    CloseHandle(stdout_write);
    CloseHandle(stderr_write);

    if (!created)
    {
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);
        throw std::runtime_error("CreateProcessW failed.");
    }

    std::string stdout_text;
    std::string stderr_text;
    std::thread stdout_reader([&]() { stdout_text = ReadPipe(stdout_read); });
    std::thread stderr_reader([&]() { stderr_text = ReadPipe(stderr_read); });

    WaitForSingleObject(process_info.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(process_info.hProcess, &exit_code);

    stdout_reader.join();
    stderr_reader.join();

    CloseHandle(stdout_read);
    CloseHandle(stderr_read);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);

    const auto finished = std::chrono::steady_clock::now();

    ProcessResult result;
    result.return_code = static_cast<int>(exit_code);
    result.stdout_text = std::move(stdout_text);
    result.stderr_text = std::move(stderr_text);
    result.duration_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();
    return result;
}
} // namespace agentguard
