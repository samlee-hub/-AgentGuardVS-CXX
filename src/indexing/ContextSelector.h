#pragma once

#include <string>
#include <vector>

#include "core/Models.h"

namespace agentguard
{
struct ContextCandidate
{
    std::string file_path;
    std::vector<std::string> reasons;
    int score = 0;
};

std::vector<ContextCandidate> SelectRelevantFiles(
    const std::string& user_request,
    const std::vector<SourceFileInfo>& files);
} // namespace agentguard
