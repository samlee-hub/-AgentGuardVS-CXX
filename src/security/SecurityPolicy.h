#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace agentguard
{
struct SecurityFinding
{
    std::string type;
    std::string file;
    std::string reason;
    std::string severity;
};

struct SecurityPolicy
{
    std::vector<std::string> protected_path_patterns;
    std::vector<std::string> read_only_path_patterns;
    std::vector<std::string> generated_path_patterns;
    std::vector<std::string> third_party_path_patterns;
    std::vector<std::string> secret_file_patterns;
    std::vector<std::string> dangerous_command_patterns;
    std::vector<std::string> prompt_injection_patterns;
};

SecurityPolicy DefaultSecurityPolicy();

bool IsProtectedPath(const SecurityPolicy& policy, const std::string& path);
bool IsReadOnlyPath(const SecurityPolicy& policy, const std::string& path);
bool IsGeneratedPath(const SecurityPolicy& policy, const std::string& path);
bool IsThirdPartyPath(const SecurityPolicy& policy, const std::string& path);
bool IsSecretPath(const SecurityPolicy& policy, const std::string& path);
bool IsDangerousCommand(const SecurityPolicy& policy, const std::string& command_line);

std::vector<SecurityFinding> DetectPromptInjectionText(
    const SecurityPolicy& policy,
    const std::string& path,
    const std::string& content);

std::string RedactSensitiveText(std::string text, bool redact_user_paths = false);

bool HasReparsePointInPath(
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& target_path);

void ValidateWorkspaceWritePath(
    const SecurityPolicy& policy,
    const std::filesystem::path& workspace_root,
    const std::filesystem::path& relative_path);
} // namespace agentguard
