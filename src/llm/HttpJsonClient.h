#pragma once

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace agentguard
{
struct HttpHeader
{
    std::string name;
    std::string value;
};

struct HttpJsonResponse
{
    unsigned long status_code = 0;
    std::string body;
};

class HttpTransportError : public std::runtime_error
{
public:
    HttpTransportError(std::string operation, unsigned long error_code);
    HttpTransportError(std::string operation, unsigned long error_code, std::string message);

    const std::string& operation() const noexcept;
    unsigned long error_code() const noexcept;

private:
    std::string operation_;
    unsigned long error_code_ = 0;
};

using HttpJsonRequest = std::function<HttpJsonResponse()>;

bool IsTransientWinHttpErrorCode(unsigned long error_code);
int DefaultHttpJsonMaxAttempts();
HttpJsonResponse ExecuteHttpJsonWithRetry(const HttpJsonRequest& request, int max_attempts);

HttpJsonResponse PostJson(
    const std::string& url,
    const std::vector<HttpHeader>& headers,
    const std::string& body,
    const std::string& url_error_name);
} // namespace agentguard
