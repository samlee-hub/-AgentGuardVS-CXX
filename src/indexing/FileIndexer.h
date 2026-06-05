#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/Models.h"
#include "project/ProjectProfile.h"

namespace agentguard
{
struct SourceIndexResult
{
    std::vector<SourceFileInfo> files;
    std::size_t skipped_files = 0;
    std::vector<std::string> skipped_reasons;
};

std::vector<SourceFileInfo> IndexSourceFiles(const std::filesystem::path& root);
std::vector<SourceFileInfo> IndexSourceFiles(
    const std::filesystem::path& root,
    const SourceFilePolicy& policy);
SourceIndexResult IndexSourceFilesWithPolicy(
    const std::filesystem::path& root,
    const SourceFilePolicy& policy);
} // namespace agentguard
