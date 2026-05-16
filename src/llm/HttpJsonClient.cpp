#include "llm/HttpJsonClient.h"

#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace agentguard
{
namespace
{
struct WinHttpHandle
{
    HINTERNET value = nullptr;

    WinHttpHandle() = default;
    explicit WinHttpHandle(HINTERNET handle)
        : value(handle)
    {
    }

    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;

    WinHttpHandle(WinHttpHandle&& other) noexcept
        : value(std::exchange(other.value, nullptr))
    {
    }

    ~WinHttpHandle()
    {
        if (value != nullptr)
        {
            WinHttpCloseHandle(value);
        }
    }

    explicit operator bool() const
    {
        return value != nullptr;
    }
};

struct ParsedUrl
{
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

std::wstring ToWideAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string FormatWinHttpErrorMessage(const std::string& operation, const unsigned long error_code)
{
    std::ostringstream message;
    message << operation << " failed with Windows error " << error_code << ".";
    return message.str();
}

[[noreturn]] void ThrowLastWinHttpError(const std::string& operation)
{
    throw HttpTransportError(operation, GetLastError());
}

ParsedUrl ParseUrl(const std::string& url, const std::string& url_error_name)
{
    const std::wstring wide_url = ToWideAscii(url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wide_url.c_str(), static_cast<DWORD>(wide_url.size()), 0, &components))
    {
        ThrowLastWinHttpError("WinHttpCrackUrl");
    }

    ParsedUrl parsed;
    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0)
    {
        parsed.path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (parsed.path.empty())
    {
        parsed.path = L"/";
    }
    parsed.port = components.nPort;
    parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;

    if (parsed.host.empty() ||
        (components.nScheme != INTERNET_SCHEME_HTTP && components.nScheme != INTERNET_SCHEME_HTTPS))
    {
        throw std::runtime_error(url_error_name + " must be an absolute HTTP or HTTPS URL.");
    }

    return parsed;
}

std::wstring RenderHeaders(const std::vector<HttpHeader>& headers)
{
    std::wstring rendered = L"Content-Type: application/json\r\n";
    rendered += L"Accept: application/json\r\n";
    rendered += L"Connection: close\r\n";
    for (const auto& header : headers)
    {
        rendered += ToWideAscii(header.name);
        rendered += L": ";
        rendered += ToWideAscii(header.value);
        rendered += L"\r\n";
    }
    return rendered;
}

int ReadPositiveEnvironmentInt(const char* name, const int fallback_value)
{
    const char* raw_value = std::getenv(name);
    if (raw_value == nullptr || *raw_value == '\0')
    {
        return fallback_value;
    }

    try
    {
        const int value = std::stoi(raw_value);
        return value > 0 ? value : fallback_value;
    }
    catch (...)
    {
        return fallback_value;
    }
}

void SleepBeforeRetry(const int attempt)
{
    const int base_delay_ms = ReadPositiveEnvironmentInt("AGENTGUARD_HTTP_RETRY_DELAY_MS", 750);
    const int bounded_attempt = std::clamp(attempt, 1, 6);
    const int delay_ms = std::min(base_delay_ms * bounded_attempt, 5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}
} // namespace

HttpTransportError::HttpTransportError(std::string operation, const unsigned long error_code)
    : std::runtime_error(FormatWinHttpErrorMessage(operation, error_code))
    , operation_(std::move(operation))
    , error_code_(error_code)
{
}

HttpTransportError::HttpTransportError(
    std::string operation,
    const unsigned long error_code,
    std::string message)
    : std::runtime_error(std::move(message))
    , operation_(std::move(operation))
    , error_code_(error_code)
{
}

const std::string& HttpTransportError::operation() const noexcept
{
    return operation_;
}

unsigned long HttpTransportError::error_code() const noexcept
{
    return error_code_;
}

bool IsTransientWinHttpErrorCode(const unsigned long error_code)
{
    return error_code == ERROR_WINHTTP_TIMEOUT ||
           error_code == ERROR_WINHTTP_CONNECTION_ERROR ||
           error_code == ERROR_WINHTTP_INVALID_SERVER_RESPONSE ||
           error_code == ERROR_WINHTTP_OPERATION_CANCELLED ||
           error_code == ERROR_WINHTTP_RESEND_REQUEST;
}

int DefaultHttpJsonMaxAttempts()
{
    return ReadPositiveEnvironmentInt("AGENTGUARD_HTTP_MAX_ATTEMPTS", 5);
}

HttpJsonResponse ExecuteHttpJsonWithRetry(const HttpJsonRequest& request, const int max_attempts)
{
    if (!request)
    {
        throw std::invalid_argument("HTTP JSON request callback cannot be empty.");
    }
    if (max_attempts < 1)
    {
        throw std::invalid_argument("HTTP JSON max_attempts must be at least 1.");
    }

    for (int attempt = 1; attempt <= max_attempts; ++attempt)
    {
        try
        {
            return request();
        }
        catch (const HttpTransportError& error)
        {
            if (!IsTransientWinHttpErrorCode(error.error_code()) || attempt == max_attempts)
            {
                if (attempt > 1)
                {
                    std::ostringstream message;
                    message << error.what() << " Retried " << attempt
                            << " HTTP attempt(s) because the transport error was transient.";
                    throw HttpTransportError(error.operation(), error.error_code(), message.str());
                }
                throw;
            }
            SleepBeforeRetry(attempt);
        }
    }

    throw std::runtime_error("HTTP JSON retry loop exited unexpectedly.");
}

namespace
{
HttpJsonResponse PostJsonOnce(
    const ParsedUrl& parsed_url,
    const std::vector<HttpHeader>& headers,
    const std::string& body)
{
    WinHttpHandle session(WinHttpOpen(
        L"AgentGuardVS-CXX/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session)
    {
        ThrowLastWinHttpError("WinHttpOpen");
    }

    if (!WinHttpSetTimeouts(session.value, 30000, 30000, 120000, 120000))
    {
        ThrowLastWinHttpError("WinHttpSetTimeouts");
    }

    WinHttpHandle connection(WinHttpConnect(session.value, parsed_url.host.c_str(), parsed_url.port, 0));
    if (!connection)
    {
        ThrowLastWinHttpError("WinHttpConnect");
    }

    const DWORD request_flags = parsed_url.secure ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle request(WinHttpOpenRequest(
        connection.value,
        L"POST",
        parsed_url.path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        request_flags));
    if (!request)
    {
        ThrowLastWinHttpError("WinHttpOpenRequest");
    }

    const std::wstring rendered_headers = RenderHeaders(headers);
    const BOOL sent = WinHttpSendRequest(
        request.value,
        rendered_headers.c_str(),
        static_cast<DWORD>(rendered_headers.size()),
        reinterpret_cast<LPVOID>(const_cast<char*>(body.data())),
        static_cast<DWORD>(body.size()),
        static_cast<DWORD>(body.size()),
        0);
    if (!sent)
    {
        ThrowLastWinHttpError("WinHttpSendRequest");
    }

    if (!WinHttpReceiveResponse(request.value, nullptr))
    {
        ThrowLastWinHttpError("WinHttpReceiveResponse");
    }

    DWORD status_code = 0;
    DWORD status_code_size = sizeof(status_code);
    if (!WinHttpQueryHeaders(
            request.value,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status_code,
            &status_code_size,
            WINHTTP_NO_HEADER_INDEX))
    {
        ThrowLastWinHttpError("WinHttpQueryHeaders");
    }

    std::string response_body;
    char buffer[16384];
    for (;;)
    {
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request.value, buffer, sizeof(buffer), &bytes_read))
        {
            ThrowLastWinHttpError("WinHttpReadData");
        }
        if (bytes_read == 0)
        {
            break;
        }
        response_body.append(buffer, buffer + bytes_read);
    }

    return HttpJsonResponse{status_code, response_body};
}
} // namespace

HttpJsonResponse PostJson(
    const std::string& url,
    const std::vector<HttpHeader>& headers,
    const std::string& body,
    const std::string& url_error_name)
{
    const ParsedUrl parsed_url = ParseUrl(url, url_error_name);

    return ExecuteHttpJsonWithRetry(
        [&parsed_url, &headers, &body]() {
            return PostJsonOnce(parsed_url, headers, body);
        },
        DefaultHttpJsonMaxAttempts());
}
} // namespace agentguard
