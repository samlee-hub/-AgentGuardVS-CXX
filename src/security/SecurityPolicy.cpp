#include "security/SecurityPolicy.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizePolicyPath(const std::string& path)
{
    std::string value = std::filesystem::path(path).generic_string();
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.starts_with("./"))
    {
        value.erase(0, 2);
    }
    return ToLower(value);
}

std::vector<std::string> SplitPathSegments(const std::string& path)
{
    std::vector<std::string> segments;
    std::string current;
    for (const char ch : NormalizePolicyPath(path))
    {
        if (ch == '/')
        {
            if (!current.empty())
            {
                segments.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty())
    {
        segments.push_back(current);
    }
    return segments;
}

bool ContainsSegment(const std::string& path, const std::vector<std::string>& names)
{
    const auto segments = SplitPathSegments(path);
    for (const auto& segment : segments)
    {
        for (const auto& name : names)
        {
            if (segment == ToLower(name))
            {
                return true;
            }
        }
    }
    return false;
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

bool MatchesAnyWildcard(const std::string& value, const std::vector<std::string>& patterns)
{
    const std::string lowered = ToLower(value);
    for (const auto& pattern : patterns)
    {
        if (WildcardMatch(ToLower(pattern), lowered))
        {
            return true;
        }
    }
    return false;
}

bool ContainsAnyPhrase(const std::string& value, const std::vector<std::string>& phrases)
{
    const std::string lowered = ToLower(value);
    for (const auto& phrase : phrases)
    {
        if (lowered.find(ToLower(phrase)) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

bool HasWindowsReparsePoint(const std::filesystem::path& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.wstring().c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code ignored;
    return std::filesystem::is_symlink(std::filesystem::symlink_status(path, ignored));
#endif
}
} // namespace

SecurityPolicy DefaultSecurityPolicy()
{
    SecurityPolicy policy;
    policy.protected_path_patterns = {
        ".git", ".vs", "build", "out", "debug", "release", "x64",
        "third_party", "external", "generated", "node_modules", "packages"
    };
    policy.read_only_path_patterns = {".git", ".vs", "third_party", "external"};
    policy.generated_path_patterns = {"build", "out", "debug", "release", "x64", "generated"};
    policy.third_party_path_patterns = {"third_party", "external", "node_modules", "packages"};
    policy.secret_file_patterns = {
        ".env", "*.pem", "*.key", "*secret*", "*token*", "credentials*", "config.local.*"
    };
    policy.dangerous_command_patterns = {
        "remove-item -recurse",
        "remove-item -force",
        "del /s",
        "erase /s",
        "rmdir /s",
        "rd /s",
        "rm -rf",
        "format ",
        "reg delete",
        "shutdown "
    };
    policy.prompt_injection_patterns = {
        "ignore previous instructions",
        "ignore all previous instructions",
        "disable safety",
        "bypass safety",
        "bypass security",
        "modify protected files",
        "delete protected files",
        "reveal system prompt",
        "override developer instructions"
    };
    return policy;
}

bool IsProtectedPath(const SecurityPolicy& policy, const std::string& path)
{
    return ContainsSegment(path, policy.protected_path_patterns) ||
           IsReadOnlyPath(policy, path) ||
           IsGeneratedPath(policy, path) ||
           IsThirdPartyPath(policy, path) ||
           IsSecretPath(policy, path);
}

bool IsReadOnlyPath(const SecurityPolicy& policy, const std::string& path)
{
    return ContainsSegment(path, policy.read_only_path_patterns);
}

bool IsGeneratedPath(const SecurityPolicy& policy, const std::string& path)
{
    return ContainsSegment(path, policy.generated_path_patterns);
}

bool IsThirdPartyPath(const SecurityPolicy& policy, const std::string& path)
{
    return ContainsSegment(path, policy.third_party_path_patterns);
}

bool IsSecretPath(const SecurityPolicy& policy, const std::string& path)
{
    const std::string normalized = NormalizePolicyPath(path);
    const auto filename = ToLower(std::filesystem::path(normalized).filename().generic_string());
    return MatchesAnyWildcard(filename, policy.secret_file_patterns) ||
           MatchesAnyWildcard(normalized, policy.secret_file_patterns);
}

bool IsDangerousCommand(const SecurityPolicy& policy, const std::string& command_line)
{
    return ContainsAnyPhrase(command_line, policy.dangerous_command_patterns);
}

std::vector<SecurityFinding> DetectPromptInjectionText(
    const SecurityPolicy& policy,
    const std::string& path,
    const std::string& content)
{
    std::vector<SecurityFinding> findings;
    const std::string lowered = ToLower(content);
    for (const auto& phrase : policy.prompt_injection_patterns)
    {
        if (lowered.find(ToLower(phrase)) == std::string::npos)
        {
            continue;
        }
        findings.push_back(SecurityFinding{
            "prompt_injection",
            path,
            "Project text contains instruction-like phrase: " + phrase,
            "high"
        });
    }
    return findings;
}

std::string RedactSensitiveText(std::string text, bool redact_user_paths)
{
    text = std::regex_replace(
        text,
        std::regex(R"((Authorization\s*:\s*Bearer\s+)[A-Za-z0-9._\-]+)", std::regex_constants::icase),
        "$1[REDACTED_TOKEN]");
    text = std::regex_replace(
        text,
        std::regex(R"(((?:OPENAI|DEEPSEEK|ANTHROPIC)_API_KEY\s*=\s*)[^\s"';&]+)", std::regex_constants::icase),
        "$1[REDACTED_TOKEN]");
    text = std::regex_replace(
        text,
        std::regex(R"(sk-[A-Za-z0-9_\-]{8,})", std::regex_constants::icase),
        "[REDACTED_TOKEN]");

    if (redact_user_paths)
    {
        text = std::regex_replace(
            text,
            std::regex(R"(([A-Za-z]:\\Users\\)[^\\\s]+(\\))", std::regex_constants::icase),
            "$1[REDACTED_USER]$2");
    }
    return text;
}

bool HasReparsePointInPath(
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& target_path)
{
    const auto normalized_root = NormalizePath(workspace_root);
    const auto normalized_target = NormalizePath(target_path);
    if (!IsSubPath(normalized_target, normalized_root))
    {
        return true;
    }

    std::filesystem::path current;
    for (const auto& component : normalized_target)
    {
        current /= component;
        if (!std::filesystem::exists(current))
        {
            continue;
        }
        if (HasWindowsReparsePoint(current))
        {
            return true;
        }
    }
    return false;
}

void ValidateWorkspaceWritePath(
    const SecurityPolicy& policy,
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& relative_path)
{
    const auto safe_relative = SafeRelativePath(relative_path);
    const std::string normalized_relative = safe_relative.generic_string();
    if (IsProtectedPath(policy, normalized_relative) || IsSecretPath(policy, normalized_relative))
    {
        throw std::invalid_argument("Write target is protected by SecurityPolicy: " + normalized_relative);
    }

    const auto normalized_root = NormalizePath(workspace_root);
    const auto absolute_target = NormalizePath(normalized_root / safe_relative);
    if (!IsSubPath(absolute_target, normalized_root))
    {
        throw std::invalid_argument("Write target escapes workspace: " + normalized_relative);
    }
    if (HasReparsePointInPath(normalized_root, absolute_target))
    {
        throw std::invalid_argument("Write target crosses a symlink or reparse point: " + normalized_relative);
    }
}
} // namespace agentguard
