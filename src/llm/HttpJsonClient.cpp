#include "llm/HttpJsonClient.h"

#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>

#include <sstream>
#include <stdexcept>
#include <string>
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

std::string LastErrorMessage(const std::string& operation)
{
    std::ostringstream message;
    message << operation << " failed with Windows error " << GetLastError() << ".";
    return message.str();
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
        throw std::runtime_error(LastErrorMessage("WinHttpCrackUrl"));
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
    for (const auto& header : headers)
    {
        rendered += ToWideAscii(header.name);
        rendered += L": ";
        rendered += ToWideAscii(header.value);
        rendered += L"\r\n";
    }
    return rendered;
}
} // namespace

HttpJsonResponse PostJson(
    const std::string& url,
    const std::vector<HttpHeader>& headers,
    const std::string& body,
    const std::string& url_error_name)
{
    const ParsedUrl parsed_url = ParseUrl(url, url_error_name);

    WinHttpHandle session(WinHttpOpen(
        L"AgentGuardVS-CXX/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0));
    if (!session)
    {
        throw std::runtime_error(LastErrorMessage("WinHttpOpen"));
    }

    if (!WinHttpSetTimeouts(session.value, 30000, 30000, 120000, 120000))
    {
        throw std::runtime_error(LastErrorMessage("WinHttpSetTimeouts"));
    }

    WinHttpHandle connection(WinHttpConnect(session.value, parsed_url.host.c_str(), parsed_url.port, 0));
    if (!connection)
    {
        throw std::runtime_error(LastErrorMessage("WinHttpConnect"));
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
        throw std::runtime_error(LastErrorMessage("WinHttpOpenRequest"));
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
        throw std::runtime_error(LastErrorMessage("WinHttpSendRequest"));
    }

    if (!WinHttpReceiveResponse(request.value, nullptr))
    {
        throw std::runtime_error(LastErrorMessage("WinHttpReceiveResponse"));
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
        throw std::runtime_error(LastErrorMessage("WinHttpQueryHeaders"));
    }

    std::string response_body;
    for (;;)
    {
        DWORD bytes_available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &bytes_available))
        {
            throw std::runtime_error(LastErrorMessage("WinHttpQueryDataAvailable"));
        }
        if (bytes_available == 0)
        {
            break;
        }

        std::string chunk(bytes_available, '\0');
        DWORD bytes_read = 0;
        if (!WinHttpReadData(request.value, chunk.data(), bytes_available, &bytes_read))
        {
            throw std::runtime_error(LastErrorMessage("WinHttpReadData"));
        }
        chunk.resize(bytes_read);
        response_body += chunk;
    }

    return HttpJsonResponse{status_code, response_body};
}
} // namespace agentguard
