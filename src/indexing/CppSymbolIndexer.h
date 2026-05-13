#pragma once

#include <filesystem>

#include "core/Models.h"

namespace agentguard
{
SourceFileInfo IndexCppSymbols(
    const std::filesystem::path& file_path,
    const std::filesystem::path& root = {});
} // namespace agentguard
