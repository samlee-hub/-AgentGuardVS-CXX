#include "patching/PatchApplier.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "core/PathUtils.h"
#include "security/SecurityPolicy.h"

namespace agentguard
{
namespace
{
namespace fs = std::filesystem;

struct PlannedChange
{
    FileChange change;
    fs::path relative_path;
    fs::path absolute_path;
    fs::path backup_path;
    std::string original_content;
    std::optional<std::string> replacement_content;
    bool existed = false;
};

std::string ReadTextFile(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to read file: " + path.string());
    }

    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void WriteTextFile(const fs::path& path, const std::string& content)
{
    EnsureDirectory(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Unable to write file: " + path.string());
    }

    output << content;
    if (!output)
    {
        throw std::runtime_error("Unable to finish writing file: " + path.string());
    }
}

std::string NormalizeConstraintPath(const std::string& path)
{
    return SafeRelativePath(fs::path(path)).generic_string();
}

std::unordered_set<std::string> NormalizeConstraintSet(const std::vector<std::string>& paths)
{
    std::unordered_set<std::string> normalized;
    for (const auto& path : paths)
    {
        normalized.insert(NormalizeConstraintPath(path));
    }
    return normalized;
}

bool IsSupportedChangeType(const std::string& change_type)
{
    return change_type == "modify" || change_type == "create" || change_type == "delete";
}

PatchApplyResult Fail(std::string error)
{
    PatchApplyResult result;
    result.errors.push_back(std::move(error));
    return result;
}

void RestoreAppliedChange(const PlannedChange& planned)
{
    if (planned.existed)
    {
        WriteTextFile(planned.absolute_path, planned.original_content);
        return;
    }

    std::error_code ignored;
    fs::remove(planned.absolute_path, ignored);
}
} // namespace

PatchApplyResult ApplyPatchPlan(
    const PatchPlan& plan,
    const TaskSpec& spec,
    const fs::path& repo_root)
{
    if (plan.changes.empty())
    {
        return Fail("PatchPlan does not contain any file changes.");
    }

    const fs::path normalized_repo_root = NormalizePath(repo_root);
    const auto allowed_files = NormalizeConstraintSet(spec.allowed_files);
    const auto forbidden_files = NormalizeConstraintSet(spec.forbidden_files);
    const auto security_policy = DefaultSecurityPolicy();

    std::vector<PlannedChange> planned_changes;
    planned_changes.reserve(plan.changes.size());

    try
    {
        for (const auto& change : plan.changes)
        {
            if (!IsSupportedChangeType(change.change_type))
            {
                return Fail("Unsupported patch change type: " + change.change_type);
            }

            const fs::path relative_path = SafeRelativePath(fs::path(change.target_file));
            const std::string normalized_target = relative_path.generic_string();

            if (forbidden_files.contains(normalized_target))
            {
                return Fail("Patch targets a forbidden file: " + normalized_target);
            }

            if (!allowed_files.contains(normalized_target))
            {
                return Fail("Patch targets a file outside TaskSpec.allowed_files: " + normalized_target);
            }

            const fs::path absolute_path = NormalizePath(normalized_repo_root / relative_path);
            ValidateWorkspaceWritePath(security_policy, normalized_repo_root, relative_path);
            if (!IsSubPath(absolute_path, normalized_repo_root))
            {
                return Fail("Patch target escapes the isolated workspace: " + normalized_target);
            }

            PlannedChange planned;
            planned.change = change;
            planned.relative_path = relative_path;
            planned.absolute_path = absolute_path;
            planned.backup_path = absolute_path;
            planned.backup_path += ".bak";
            planned.existed = fs::exists(absolute_path);

            if (change.change_type == "create")
            {
                if (planned.existed)
                {
                    return Fail("Create target already exists: " + normalized_target);
                }

                planned.replacement_content = change.replacement_snippet;
            }
            else
            {
                if (!planned.existed)
                {
                    return Fail("Patch target does not exist: " + normalized_target);
                }

                planned.original_content = ReadTextFile(absolute_path);

                if (change.change_type == "modify")
                {
                    const auto snippet_position = planned.original_content.find(change.original_snippet);
                    if (snippet_position == std::string::npos)
                    {
                        return Fail("Original snippet was not found in: " + normalized_target);
                    }

                    std::string updated_content = planned.original_content;
                    updated_content.replace(
                        snippet_position,
                        change.original_snippet.size(),
                        change.replacement_snippet);
                    planned.replacement_content = std::move(updated_content);
                }
            }

            planned_changes.push_back(std::move(planned));
        }
    }
    catch (const std::exception& exception)
    {
        return Fail(exception.what());
    }

    PatchApplyResult result;
    std::vector<PlannedChange> applied_changes;
    applied_changes.reserve(planned_changes.size());

    try
    {
        for (const auto& planned : planned_changes)
        {
            if (planned.existed)
            {
                WriteTextFile(planned.backup_path, planned.original_content);
            }

            if (planned.change.change_type == "delete")
            {
                fs::remove(planned.absolute_path);
            }
            else
            {
                WriteTextFile(planned.absolute_path, *planned.replacement_content);
            }

            result.applied_files.push_back(planned.relative_path.generic_string());
            applied_changes.push_back(planned);
        }
    }
    catch (const std::exception& exception)
    {
        for (auto it = applied_changes.rbegin(); it != applied_changes.rend(); ++it)
        {
            try
            {
                RestoreAppliedChange(*it);
            }
            catch (...)
            {
            }
        }

        result.errors.push_back(exception.what());
        result.success = false;
        result.applied_files.clear();
        return result;
    }

    result.success = true;
    return result;
}
} // namespace agentguard
