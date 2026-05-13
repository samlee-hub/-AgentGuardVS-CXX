#include "core/PathUtils.h"

#include <stdexcept>

namespace agentguard
{
std::filesystem::path NormalizePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return {};
    }

    return std::filesystem::weakly_canonical(std::filesystem::absolute(path)).lexically_normal();
}

bool IsSubPath(const std::filesystem::path& candidate, const std::filesystem::path& base)
{
    const auto normalized_candidate = NormalizePath(candidate);
    const auto normalized_base = NormalizePath(base);

    auto candidate_it = normalized_candidate.begin();
    auto base_it = normalized_base.begin();

    for (; base_it != normalized_base.end(); ++base_it, ++candidate_it)
    {
        if (candidate_it == normalized_candidate.end() || *candidate_it != *base_it)
        {
            return false;
        }
    }

    return true;
}

void EnsureDirectory(const std::filesystem::path& path)
{
    if (path.empty())
    {
        throw std::invalid_argument("Directory path cannot be empty.");
    }

    std::filesystem::create_directories(path);
}

std::filesystem::path SafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty())
    {
        throw std::invalid_argument("Relative path cannot be empty.");
    }

    if (path.is_absolute() || HasForbiddenPathTraversal(path))
    {
        throw std::invalid_argument("Relative path must stay within its workspace.");
    }

    return path.lexically_normal();
}

bool HasForbiddenPathTraversal(const std::filesystem::path& path)
{
    for (const auto& component : path)
    {
        if (component == "..")
        {
            return true;
        }
    }

    return false;
}
} // namespace agentguard
