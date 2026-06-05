#pragma once

#include <string>
#include <vector>

#include "core/Models.h"

namespace agentguard
{
std::vector<ParsedBuildError> ParseBuildErrors(const std::string& build_log);
std::vector<ParsedBuildError> ParseBuildErrors(const BuildResult& build_result);
} // namespace agentguard
