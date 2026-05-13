#pragma once

#include <filesystem>

namespace agentguard
{
std::filesystem::path NormalizePath(const std::filesystem::path& path);
bool IsSubPath(const std::filesystem::path& candidate, const std::filesystem::path& base);
void EnsureDirectory(const std::filesystem::path& path);
std::filesystem::path SafeRelativePath(const std::filesystem::path& path);
bool HasForbiddenPathTraversal(const std::filesystem::path& path);
} // namespace agentguard
