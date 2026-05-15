#pragma once

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

HttpJsonResponse PostJson(
    const std::string& url,
    const std::vector<HttpHeader>& headers,
    const std::string& body,
    const std::string& url_error_name);
} // namespace agentguard
