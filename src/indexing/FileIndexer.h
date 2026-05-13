#pragma once

#include <filesystem>
#include <vector>

#include "core/Models.h"

namespace agentguard
{
std::vector<SourceFileInfo> IndexSourceFiles(const std::filesystem::path& root);
} // namespace agentguard
