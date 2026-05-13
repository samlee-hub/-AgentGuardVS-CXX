#include "indexing/FileIndexer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string_view>

namespace agentguard
{
namespace
{
constexpr std::array<std::string_view, 10> kExcludedDirectories{
    ".git",
    ".vs",
    "x64",
    "Debug",
    "Release",
    "build",
    "out",
    "Binaries",
    "Intermediate",
    "Saved"
};

constexpr std::array<std::string_view, 5> kSourceExtensions{
    ".cpp",
    ".cc",
    ".cxx",
    ".h",
    ".hpp"
};

bool IsExcludedDirectory(const std::filesystem::path& path)
{
    const auto directory_name = path.filename().string();
    return std::find(
               kExcludedDirectories.begin(),
               kExcludedDirectories.end(),
               directory_name) != kExcludedDirectories.end();
}

bool HasSupportedExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return std::find(kSourceExtensions.begin(), kSourceExtensions.end(), extension) !=
           kSourceExtensions.end();
}
} // namespace

std::vector<SourceFileInfo> IndexSourceFiles(const std::filesystem::path& root)
{
    if (!std::filesystem::exists(root) || !std::filesystem::is_directory(root))
    {
        throw std::invalid_argument("Index root must be an existing directory.");
    }

    std::vector<SourceFileInfo> files;

    for (std::filesystem::recursive_directory_iterator it(root), end; it != end; ++it)
    {
        const auto& path = it->path();
        if (it->is_directory() && IsExcludedDirectory(path))
        {
            it.disable_recursion_pending();
            continue;
        }

        if (!it->is_regular_file() || !HasSupportedExtension(path))
        {
            continue;
        }

        SourceFileInfo info;
        info.file_path = std::filesystem::relative(path, root).string();
        files.push_back(std::move(info));
    }

    std::sort(files.begin(), files.end(), [](const SourceFileInfo& left, const SourceFileInfo& right) {
        return left.file_path < right.file_path;
    });

    return files;
}
} // namespace agentguard
