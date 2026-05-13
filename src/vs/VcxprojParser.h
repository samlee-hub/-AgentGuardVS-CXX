#pragma once

#include <filesystem>

#include "core/Models.h"

namespace agentguard
{
VSProjectInfo ParseVcxproj(const std::filesystem::path& vcxproj_path);
} // namespace agentguard
