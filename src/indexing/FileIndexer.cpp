#include "indexing/FileIndexer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace agentguard
{
namespace
{
namespace fs = std::filesystem;

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool WildcardMatch(std::string_view pattern, std::string_view text)
{
    std::size_t pattern_index = 0;
    std::size_t text_index = 0;
    std::size_t star_index = std::string_view::npos;
    std::size_t match_index = 0;
    while (text_index < text.size())
    {
        if (pattern_index < pattern.size() &&
            (pattern[pattern_index] == '?' || pattern[pattern_index] == text[text_index]))
        {
            ++pattern_index;
            ++text_index;
            continue;
        }
        if (pattern_index < pattern.size() && pattern[pattern_index] == '*')
        {
            star_index = pattern_index++;
            match_index = text_index;
            continue;
        }
        if (star_index != std::string_view::npos)
        {
            pattern_index = star_index + 1;
            text_index = ++match_index;
            continue;
        }
        return false;
    }
    while (pattern_index < pattern.size() && pattern[pattern_index] == '*')
    {
        ++pattern_index;
    }
    return pattern_index == pattern.size();
}

bool MatchesPattern(const std::string& pattern, const std::string& path)
{
    std::string normalized_pattern = fs::path(pattern).generic_string();
    std::string normalized_path = fs::path(path).generic_string();
    std::replace(normalized_pattern.begin(), normalized_pattern.end(), '\\', '/');
    std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
    if (normalized_pattern.ends_with("/**"))
    {
        const auto prefix = normalized_pattern.substr(0, normalized_pattern.size() - 3);
        return normalized_path == prefix || normalized_path.starts_with(prefix + "/");
    }
    if (normalized_pattern.find('*') != std::string::npos ||
        normalized_pattern.find('?') != std::string::npos)
    {
        return WildcardMatch(normalized_pattern, normalized_path) ||
            WildcardMatch(normalized_pattern, fs::path(normalized_path).filename().generic_string());
    }
    return normalized_path == normalized_pattern || normalized_path.starts_with(normalized_pattern + "/");
}

bool MatchesAnyPattern(const std::vector<std::string>& patterns, const std::string& path)
{
    for (const auto& pattern : patterns)
    {
        if (MatchesPattern(pattern, path))
        {
            return true;
        }
    }
    return false;
}

bool IsExcludedDirectory(const fs::path& path, const SourceFilePolicy& policy)
{
    const auto directory_name = path.filename().string();
    return std::find(policy.exclude_dirs.begin(), policy.exclude_dirs.end(), directory_name) !=
        policy.exclude_dirs.end();
}

bool HasSupportedExtension(const fs::path& path, const SourceFilePolicy& policy)
{
    std::string extension = ToLower(path.extension().string());
    for (const auto& supported : policy.source_extensions)
    {
        if (ToLower(supported) == extension)
        {
            return true;
        }
    }
    return false;
}

bool IsConfigFile(const fs::path& path, const SourceFilePolicy& policy)
{
    const auto filename = path.filename().generic_string();
    const auto generic = path.generic_string();
    for (const auto& pattern : policy.config_file_patterns)
    {
        if (MatchesPattern(pattern, filename) || MatchesPattern(pattern, generic))
        {
            return true;
        }
    }
    return false;
}

bool LooksGenerated(const fs::path& path)
{
    const std::string lowered = ToLower(path.generic_string());
    return lowered.find(".generated.") != std::string::npos ||
        lowered.find("/generated/") != std::string::npos ||
        lowered.find("\\generated\\") != std::string::npos ||
        lowered.ends_with(".designer.cs");
}

bool LooksBinary(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }
    std::array<char, 4096> buffer{};
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto read_count = input.gcount();
    return std::find(buffer.begin(), buffer.begin() + read_count, '\0') != buffer.begin() + read_count;
}

SourceFilePolicy DefaultCppPolicy()
{
    SourceFilePolicy policy;
    policy.source_extensions = {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".hxx"};
    policy.config_file_patterns = {
        "CMakeLists.txt", "*.sln", "*.vcxproj", "agentguard.json"
    };
    policy.exclude_dirs = {
        ".git", ".vs", "x64", "Debug", "Release", "build", "out",
        "Binaries", "Intermediate", "Saved", "runs", "artifacts"
    };
    return policy;
}
} // namespace

SourceIndexResult IndexSourceFilesWithPolicy(const fs::path& root, const SourceFilePolicy& policy)
{
    if (!fs::exists(root) || !fs::is_directory(root))
    {
        throw std::invalid_argument("Index root must be an existing directory.");
    }

    SourceIndexResult result;
    std::error_code error;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
         !error && it != end;
         it.increment(error))
    {
        const auto& path = it->path();
        if (it->is_directory(error) && IsExcludedDirectory(path, policy))
        {
            it.disable_recursion_pending();
            continue;
        }

        if (!it->is_regular_file(error))
        {
            continue;
        }

        const auto relative = fs::relative(path, root, error).generic_string();
        if (error)
        {
            ++result.skipped_files;
            result.skipped_reasons.push_back("relative path failed: " + path.generic_string());
            error.clear();
            continue;
        }
        if (!policy.include.empty() && !MatchesAnyPattern(policy.include, relative))
        {
            ++result.skipped_files;
            continue;
        }
        if (MatchesAnyPattern(policy.exclude, relative))
        {
            ++result.skipped_files;
            continue;
        }
        if (!HasSupportedExtension(path, policy) && !IsConfigFile(path, policy))
        {
            continue;
        }
        if (policy.skip_generated_files && LooksGenerated(path))
        {
            ++result.skipped_files;
            result.skipped_reasons.push_back("generated: " + relative);
            continue;
        }
        if (policy.max_file_size > 0 && fs::file_size(path, error) > policy.max_file_size)
        {
            ++result.skipped_files;
            result.skipped_reasons.push_back("too large: " + relative);
            continue;
        }
        if (policy.skip_binary_files && LooksBinary(path))
        {
            ++result.skipped_files;
            result.skipped_reasons.push_back("binary: " + relative);
            continue;
        }

        SourceFileInfo info;
        info.file_path = relative;
        result.files.push_back(std::move(info));
    }

    std::sort(result.files.begin(), result.files.end(), [](const SourceFileInfo& left, const SourceFileInfo& right) {
        return left.file_path < right.file_path;
    });
    return result;
}

std::vector<SourceFileInfo> IndexSourceFiles(const fs::path& root)
{
    return IndexSourceFilesWithPolicy(root, DefaultCppPolicy()).files;
}

std::vector<SourceFileInfo> IndexSourceFiles(const fs::path& root, const SourceFilePolicy& policy)
{
    return IndexSourceFilesWithPolicy(root, policy).files;
}
} // namespace agentguard
