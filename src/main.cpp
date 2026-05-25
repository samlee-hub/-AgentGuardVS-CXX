#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/ExtraValidators.hpp>
#include <CLI/Formatter.hpp>
#include <nlohmann/json.hpp>

#include "agents/RepairLoop.h"
#include "agents/ReviewerAgent.h"
#include "agents/SemanticAnalyzerAgent.h"
#include "core/PathUtils.h"
#include "core/Workspace.h"
#include "diagnostics/ErrorParser.h"
#include "indexing/ContextSelector.h"
#include "indexing/CppSymbolIndexer.h"
#include "indexing/DependencyGraph.h"
#include "indexing/FileIndexer.h"
#include "indexing/SymbolIndex.h"
#include "llm/ClaudeProvider.h"
#include "llm/CachedLLMProvider.h"
#include "llm/DeepSeekProvider.h"
#include "llm/FakeLLMProvider.h"
#include "llm/FileLLMProvider.h"
#include "llm/LLMProviderConfig.h"
#include "llm/OpenAIProvider.h"
#include "patching/DiffReporter.h"
#include "project/CommandVerifier.h"
#include "project/ProjectProfile.h"
#include "report/ReportWriter.h"
#include "security/SecurityPolicy.h"
#include "vs/MSBuildRunner.h"
#include "vs/SlnParser.h"
#include "vs/VcxprojParser.h"

namespace
{
namespace fs = std::filesystem;

struct RunOptions
{
    fs::path solution;
    std::string task;
    std::string provider = "fake";
    std::string model;
    fs::path response_file;
    std::string configuration = "Debug";
    std::string platform = "x64";
    fs::path runs_root;
    std::string runs_root_source = "default";
    int max_repair_rounds = 3;
    bool dry_run = false;
    bool no_llm = false;
    bool force = false;
    bool json = false;
};

struct LlmSmokeOptions
{
    std::string input;
    bool json = false;
};

struct AnalyzeOptions
{
    fs::path solution;
    std::string task;
    std::string provider = "fake";
    std::string model;
    fs::path response_file;
    fs::path runs_root;
    std::string runs_root_source = "default";
    bool force = false;
    bool json = false;
    bool no_cache = false;
};

struct ReviewOptions
{
    fs::path workspace;
    std::string task;
    std::string provider = "fake";
    std::string model;
    fs::path response_file;
    bool json = false;
    bool no_cache = false;
};

struct VerifyOptions
{
    fs::path workspace;
    std::string configuration = "Debug";
    std::string platform = "x64";
    bool json = false;
};

struct DetectOptions
{
    fs::path project;
    bool json = false;
};

struct VerifyProfileOptions
{
    fs::path project;
    bool json = false;
};

#ifdef _WIN32
std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int required_size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required_size <= 0)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }

    std::string result(static_cast<std::size_t>(required_size), '\0');
    const int converted = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        required_size,
        nullptr,
        nullptr);
    if (converted != required_size)
    {
        throw std::runtime_error("WideCharToMultiByte produced an unexpected byte count.");
    }

    return result;
}
#endif

std::string SanitizeTaskIdPart(std::string value)
{
    for (char& ch : value)
    {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_')
        {
            ch = '-';
        }
    }
    value.erase(std::unique(value.begin(), value.end(), [](char left, char right) {
        return left == '-' && right == '-';
    }), value.end());
    if (value.empty())
    {
        return "task";
    }
    return value.substr(0, 32);
}

std::string MakeTaskId(const fs::path& solution, const std::string& task)
{
    const auto hash_value = std::hash<std::string>{}(solution.string() + "|" + task);
    std::ostringstream stream;
    stream << SanitizeTaskIdPart(solution.stem().string()) << "-" << std::hex << (hash_value & 0xfffffff);
    return stream.str();
}

fs::path DefaultRunsRoot()
{
#ifdef _WIN32
    if (const char* local_app_data = std::getenv("LOCALAPPDATA");
        local_app_data != nullptr && std::string(local_app_data).empty() == false)
    {
        return fs::path(local_app_data) / "AgentGuardVS" / "runs";
    }
#endif
    return fs::temp_directory_path() / "AgentGuardVS" / "runs";
}

void ResolveRunsRoot(fs::path& runs_root, std::string& runs_root_source, const bool user_specified)
{
    runs_root_source = user_specified ? "user_specified" : "default";
    if (runs_root.empty())
    {
        runs_root = DefaultRunsRoot();
    }
}

nlohmann::json ErrorJson(
    const std::string& command,
    const std::string& error_code,
    const std::string& message,
    const std::string& suggestion)
{
    return nlohmann::json{
        {"ok", false},
        {"command", command},
        {"error_code", error_code},
        {"message", message},
        {"suggestion", suggestion}
    };
}

int EmitJsonError(
    const std::string& command,
    const std::string& error_code,
    const std::string& message,
    const std::string& suggestion)
{
    std::cout << ErrorJson(command, error_code, message, suggestion).dump() << "\n";
    return 1;
}

std::string ClassifyExceptionError(const std::string& message)
{
    if (message.find("OPENAI_API_KEY") != std::string::npos ||
        message.find("DEEPSEEK_API_KEY") != std::string::npos ||
        message.find("ANTHROPIC_API_KEY") != std::string::npos)
    {
        return "API_KEY_MISSING";
    }
    if (message.find("MODEL") != std::string::npos || message.find("model") != std::string::npos)
    {
        return "MODEL_MISSING";
    }
    if (message.find("Unsupported AGENTGUARD_LLM_PROVIDER") != std::string::npos)
    {
        return "PROVIDER_UNSUPPORTED";
    }
    if (message.find("Git baseline failed") != std::string::npos)
    {
        return "GIT_BASELINE_FAILED";
    }
    if (message.find("Semantic analysis failed") != std::string::npos ||
        message.find("Semantic review failed") != std::string::npos ||
        message.find("parse") != std::string::npos)
    {
        return "INVALID_LLM_JSON";
    }
    if (message.find("protected") != std::string::npos)
    {
        return "PROTECTED_FILE_TOUCHED";
    }
    if (message.find("approval") != std::string::npos)
    {
        return "APPROVAL_REQUIRED";
    }
    return "REVIEW_REJECTED";
}

std::string ReadTextIfPresent(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool IsAlwaysProtectedPath(const std::string& path)
{
    static const agentguard::SecurityPolicy policy = agentguard::DefaultSecurityPolicy();
    return agentguard::IsProtectedPath(policy, path);
}

std::vector<std::string> SelectAllowedFiles(
    const std::vector<agentguard::ContextCandidate>& candidates,
    const std::vector<agentguard::SourceFileInfo>& files)
{
    std::vector<std::string> allowed;
    for (const auto& candidate : candidates)
    {
        const auto normalized = fs::path(candidate.file_path).generic_string();
        if (IsAlwaysProtectedPath(normalized))
        {
            continue;
        }
        allowed.push_back(normalized);
        if (allowed.size() >= 5)
        {
            break;
        }
    }

    for (const auto& file : files)
    {
        if (!allowed.empty())
        {
            break;
        }
        const auto normalized = fs::path(file.file_path).generic_string();
        if (!IsAlwaysProtectedPath(normalized))
        {
            allowed.push_back(normalized);
        }
    }

    return allowed;
}

std::vector<std::string> BuildForbiddenFiles(
    const std::vector<agentguard::SourceFileInfo>& files,
    const std::vector<std::string>& allowed_files)
{
    std::unordered_set<std::string> allowed_set(allowed_files.begin(), allowed_files.end());
    std::vector<std::string> forbidden;
    for (const auto& file : files)
    {
        const auto normalized = fs::path(file.file_path).generic_string();
        if (!allowed_set.contains(normalized) || IsAlwaysProtectedPath(normalized))
        {
            forbidden.push_back(normalized);
        }
        if (forbidden.size() >= 20)
        {
            break;
        }
    }
    return forbidden;
}

std::vector<agentguard::RelevantFileContent> LoadRelevantFileContents(
    const fs::path& repo_root,
    const std::vector<std::string>& relative_paths)
{
    std::vector<agentguard::RelevantFileContent> contents;
    for (const auto& relative_path : relative_paths)
    {
        const auto safe_path = agentguard::SafeRelativePath(fs::path(relative_path));
        contents.push_back(agentguard::RelevantFileContent{
            safe_path.generic_string(),
            ReadTextIfPresent(repo_root / safe_path)
        });
    }
    return contents;
}

void WriteTextFile(const fs::path& path, const std::string& content)
{
    agentguard::EnsureDirectory(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw std::runtime_error("Unable to write file: " + path.string());
    }
    output << content;
}

void WriteJsonOrRawText(const fs::path& path, const std::string& content)
{
    try
    {
        WriteTextFile(path, nlohmann::json::parse(content).dump(2));
    }
    catch (const std::exception&)
    {
        WriteTextFile(path, nlohmann::json{{"raw_text", content}}.dump(2));
    }
}

struct ArtifactPaths
{
    fs::path root;
    fs::path prompts;
    fs::path raw_responses;
    fs::path build_logs;
    fs::path diffs;
    fs::path reports;
};

ArtifactPaths EnsureArtifactPaths(const fs::path& task_root)
{
    ArtifactPaths paths;
    paths.root = task_root / "artifacts";
    paths.prompts = paths.root / "prompts";
    paths.raw_responses = paths.root / "raw_responses";
    paths.build_logs = paths.root / "build_logs";
    paths.diffs = paths.root / "diffs";
    paths.reports = paths.root / "reports";
    agentguard::EnsureDirectory(paths.prompts);
    agentguard::EnsureDirectory(paths.raw_responses);
    agentguard::EnsureDirectory(paths.build_logs);
    agentguard::EnsureDirectory(paths.diffs);
    agentguard::EnsureDirectory(paths.reports);
    return paths;
}

void CopyFileIfPresent(const fs::path& source, const fs::path& destination)
{
    if (!fs::exists(source))
    {
        return;
    }
    agentguard::EnsureDirectory(destination.parent_path());
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
}

void WriteDiffArtifacts(const ArtifactPaths& artifacts, const agentguard::DiffReport& diff_report)
{
    WriteTextFile(artifacts.diffs / "workspace.diff", diff_report.diff_text);
    WriteTextFile(artifacts.diffs / "workspace.diffstat", diff_report.diff_stat);
}

std::map<std::string, std::string> BuildArtifactsMap(
    const fs::path& task_root,
    const fs::path& report_md = {},
    const fs::path& report_json = {})
{
    const auto artifacts = EnsureArtifactPaths(task_root);
    std::map<std::string, std::string> result{
        {"prompts_dir", artifacts.prompts.generic_string()},
        {"raw_responses_dir", artifacts.raw_responses.generic_string()},
        {"build_logs_dir", artifacts.build_logs.generic_string()},
        {"diffs_dir", artifacts.diffs.generic_string()},
        {"reports_dir", artifacts.reports.generic_string()},
        {"analyze_prompt", (artifacts.prompts / "semantic_analyze_prompt.txt").generic_string()},
        {"analyze_raw_response", (artifacts.raw_responses / "semantic_analyze_raw_response.json").generic_string()},
        {"review_prompt", (artifacts.prompts / "semantic_review_prompt.txt").generic_string()},
        {"review_raw_response", (artifacts.raw_responses / "semantic_review_raw_response.json").generic_string()},
        {"build_log", (artifacts.build_logs / "verify_build.log").generic_string()},
        {"diff_text", (artifacts.diffs / "workspace.diff").generic_string()},
        {"diff_stat", (artifacts.diffs / "workspace.diffstat").generic_string()}
    };
    if (!report_md.empty())
    {
        result["report_md"] = report_md.generic_string();
    }
    if (!report_json.empty())
    {
        result["report_json"] = report_json.generic_string();
    }
    return result;
}

fs::path MetricsPath(const fs::path& task_root)
{
    return task_root / "artifacts" / "metrics.json";
}

nlohmann::json MetricsToJson(const agentguard::ReportMetrics& metrics)
{
    return nlohmann::json{
        {"analyze_duration_ms", metrics.analyze_duration_ms},
        {"verify_duration_ms", metrics.verify_duration_ms},
        {"review_duration_ms", metrics.review_duration_ms},
        {"modified_file_count", metrics.modified_file_count},
        {"allowed_file_count", metrics.allowed_file_count},
        {"suspected_file_count", metrics.suspected_file_count},
        {"protected_file_count", metrics.protected_file_count},
        {"build_error_count", metrics.build_error_count},
        {"repair_rounds", metrics.repair_rounds},
        {"scope_expansions", metrics.scope_expansions},
        {"provider", metrics.provider},
        {"model", metrics.model},
        {"cache_hit", metrics.cache_hit}
    };
}

agentguard::ReportMetrics MetricsFromJson(const nlohmann::json& json_value)
{
    agentguard::ReportMetrics metrics;
    metrics.analyze_duration_ms = json_value.value("analyze_duration_ms", 0);
    metrics.verify_duration_ms = json_value.value("verify_duration_ms", 0);
    metrics.review_duration_ms = json_value.value("review_duration_ms", 0);
    metrics.modified_file_count = json_value.value("modified_file_count", 0U);
    metrics.allowed_file_count = json_value.value("allowed_file_count", 0U);
    metrics.suspected_file_count = json_value.value("suspected_file_count", 0U);
    metrics.protected_file_count = json_value.value("protected_file_count", 0U);
    metrics.build_error_count = json_value.value("build_error_count", 0U);
    metrics.repair_rounds = json_value.value("repair_rounds", 0);
    metrics.scope_expansions = json_value.value("scope_expansions", 0);
    metrics.provider = json_value.value("provider", "");
    metrics.model = json_value.value("model", "");
    metrics.cache_hit = json_value.value("cache_hit", false);
    return metrics;
}

agentguard::ReportMetrics LoadMetrics(const fs::path& task_root)
{
    const auto path = MetricsPath(task_root);
    if (!fs::exists(path))
    {
        return {};
    }
    return MetricsFromJson(nlohmann::json::parse(ReadTextIfPresent(path)));
}

void SaveMetrics(const fs::path& task_root, const agentguard::ReportMetrics& metrics)
{
    EnsureArtifactPaths(task_root);
    WriteTextFile(MetricsPath(task_root), MetricsToJson(metrics).dump(2));
}

std::string EffectiveModelName(const std::string& provider, const std::string& explicit_model)
{
    if (!explicit_model.empty())
    {
        return explicit_model;
    }
    const auto config = agentguard::LoadLLMProviderConfigFromEnvironment();
    if (provider == "openai")
    {
        return config.openai_model;
    }
    if (provider == "deepseek")
    {
        return config.deepseek_model;
    }
    if (provider == "claude")
    {
        return config.claude_model;
    }
    if (provider == "file")
    {
        return config.file_response_path.string();
    }
    return provider;
}

std::string HashSourceIndex(const std::vector<agentguard::SourceFileInfo>& files)
{
    nlohmann::json json_value = nlohmann::json::array();
    for (const auto& file : files)
    {
        json_value.push_back({
            {"file_path", file.file_path},
            {"includes", file.includes},
            {"classes", file.classes},
            {"structs", file.structs},
            {"enums", file.enums},
            {"functions", file.functions},
            {"methods", file.methods},
            {"member_variables", file.member_variables},
            {"macros", file.macros},
            {"command_strings", file.command_strings},
            {"symbol_references", file.symbol_references},
            {"keywords", file.keywords}
        });
    }
    return agentguard::StableHashText(json_value.dump());
}

std::string HashRelevantFiles(const std::vector<agentguard::RelevantFileContent>& files)
{
    nlohmann::json json_value = nlohmann::json::array();
    for (const auto& file : files)
    {
        json_value.push_back({
            {"relative_path", file.relative_path},
            {"content_hash", agentguard::StableHashText(file.content)}
        });
    }
    return agentguard::StableHashText(json_value.dump());
}

std::size_t CountModifiedFilesFromDiff(const agentguard::DiffReport& diff_report)
{
    std::size_t count = 0;
    std::istringstream stream(diff_report.diff_text);
    std::string line;
    while (std::getline(stream, line))
    {
        if (line.rfind("diff --git ", 0) == 0)
        {
            ++count;
        }
    }
    return count;
}

nlohmann::json MakeFakeSemanticReviewJson()
{
    return nlohmann::json{
        {"meets_requirement", true},
        {"requires_scope_expansion", false},
        {"suggested_scope_additions", nlohmann::json::array()},
        {"risks", nlohmann::json::array()},
        {"confidence", 0.75},
        {"next_action", "accept"},
        {"notes", nlohmann::json::array({"Generated by fake semantic review provider."})}
    };
}

fs::path ResolveWorkspaceRepoRoot(const fs::path& workspace)
{
    const fs::path absolute_workspace = fs::absolute(workspace);
    if (absolute_workspace.filename() == "repo")
    {
        return absolute_workspace;
    }
    if (fs::is_directory(absolute_workspace / "repo"))
    {
        return absolute_workspace / "repo";
    }
    return absolute_workspace;
}

fs::path ResolveTaskRootFromWorkspace(const fs::path& workspace)
{
    const fs::path absolute_workspace = fs::absolute(workspace);
    if (absolute_workspace.filename() == "repo")
    {
        return absolute_workspace.parent_path();
    }
    return absolute_workspace;
}

fs::path FindFirstSolution(const fs::path& repo_root)
{
    if (!fs::exists(repo_root) || !fs::is_directory(repo_root))
    {
        return {};
    }

    for (const auto& entry : fs::recursive_directory_iterator(repo_root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sln")
        {
            return entry.path();
        }
    }
    return {};
}

agentguard::BuildResult LoadLatestBuildResult(const fs::path& task_root)
{
    const fs::path logs_root = task_root / "logs";
    agentguard::BuildResult result;
    result.success = true;
    result.return_code = 0;
    if (!fs::exists(logs_root))
    {
        return result;
    }

    fs::path latest_log;
    fs::file_time_type latest_time{};
    for (const auto& entry : fs::directory_iterator(logs_root))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".log")
        {
            continue;
        }
        const auto write_time = entry.last_write_time();
        if (latest_log.empty() || write_time > latest_time)
        {
            latest_log = entry.path();
            latest_time = write_time;
        }
    }

    if (latest_log.empty())
    {
        return result;
    }

    result.stdout_text = ReadTextIfPresent(latest_log);
    result.command = latest_log.string();
    if (result.stdout_text.find("Return code: 0") == std::string::npos)
    {
        result.success = false;
        result.return_code = 1;
    }
    result.parsed_errors = agentguard::ParseBuildErrors(result);
    return result;
}

void AppendReviewFiles(
    std::vector<std::string>& paths,
    const std::vector<agentguard::SemanticFileReference>& files)
{
    for (const auto& file : files)
    {
        if (std::find(paths.begin(), paths.end(), file.path) == paths.end())
        {
            paths.push_back(file.path);
        }
    }
}

std::vector<agentguard::RelevantFileContent> LoadSemanticReviewFileContents(
    const fs::path& repo_root,
    const agentguard::SemanticScopeResult& scope)
{
    std::vector<std::string> paths;
    AppendReviewFiles(paths, scope.allowed_files);
    AppendReviewFiles(paths, scope.context_files);
    AppendReviewFiles(paths, scope.suspected_files);

    std::vector<agentguard::RelevantFileContent> contents;
    for (const auto& path_text : paths)
    {
        const auto safe_path = agentguard::SafeRelativePath(fs::path(path_text));
        contents.push_back(agentguard::RelevantFileContent{
            safe_path.generic_string(),
            ReadTextIfPresent(repo_root / safe_path)
        });
    }
    return contents;
}

nlohmann::json MakePlannerJson(
    const std::string& task_id,
    const RunOptions& options,
    const std::string& target_project,
    const std::vector<std::string>& allowed_files,
    const std::vector<std::string>& forbidden_files)
{
    return nlohmann::json{
        {"task_id", task_id},
        {"user_request", options.task},
        {"target_solution", options.solution.string()},
        {"target_project", target_project},
        {"allowed_files", allowed_files},
        {"forbidden_files", forbidden_files},
        {"acceptance_criteria", nlohmann::json::array({
            "All edits stay inside the isolated workspace.",
            "Build verification is used before completion.",
            "PatchApplier remains the only file mutation path."
        })},
        {"max_repair_rounds", options.max_repair_rounds}
    };
}

nlohmann::json MakeFakeSemanticScopeJson(
    const std::string& task,
    const std::vector<std::string>& static_allowed_files,
    const std::vector<std::string>& static_forbidden_files,
    const agentguard::ImpactAnalysis& impact = {})
{
    nlohmann::json allowed = nlohmann::json::array();
    for (const auto& path : static_allowed_files)
    {
        allowed.push_back({
            {"path", path},
            {"reason", "Selected by static context analysis."},
            {"confidence", 0.8}
        });
    }

    nlohmann::json protected_files = nlohmann::json::array();
    for (const auto& path : static_forbidden_files)
    {
        protected_files.push_back({
            {"path", path},
            {"reason", "Not selected by static context analysis."},
            {"confidence", 0.7}
        });
    }

    return nlohmann::json{
        {"task_summary", task},
        {"allowed_files", allowed},
        {"context_files", nlohmann::json::array()},
        {"suspected_files", nlohmann::json::array()},
        {"protected_files", protected_files},
        {"needs_approval_files", nlohmann::json::array()},
        {"risk_level", impact.public_header_risk ? "medium" : "low"},
        {"recommendation", "proceed"},
        {"related_symbols", impact.related_symbols},
        {"dependency_reasons", impact.dependency_reasons},
        {"blast_radius", impact.blast_radius},
        {"public_interface_changes", impact.public_interface_changes},
        {"missing_context", nlohmann::json::array()},
        {"notes", nlohmann::json::array({"Generated by fake semantic analyzer provider."})}
    };
}

nlohmann::json MakeNoOpPatchPlanJson(const std::vector<agentguard::RelevantFileContent>& files)
{
    if (files.empty())
    {
        return nlohmann::json{{"changes", nlohmann::json::array()}};
    }

    const auto& file = files.front();
    return nlohmann::json{
        {"changes", nlohmann::json::array({
            {
                {"target_file", file.relative_path},
                {"change_type", "modify"},
                {"original_snippet", file.content},
                {"replacement_snippet", file.content},
                {"explanation", "No-op patch generated by --no-llm mode."}
            }
        })}
    };
}

void WriteDryRunReport(
    const fs::path& path,
    const agentguard::TaskSpec& task_spec,
    const std::vector<agentguard::ContextCandidate>& candidates,
    const std::vector<agentguard::SourceFileInfo>& source_files,
    const std::vector<agentguard::VSProjectInfo>& projects)
{
    std::ofstream output(path, std::ios::trunc);
    output << "# Dry Run Report\n\n";
    output << "## TaskSpec\n\n";
    output << nlohmann::json(task_spec).dump(2) << "\n\n";
    output << "## Projects\n\n";
    for (const auto& project : projects)
    {
        output << "- " << project.project_name << ": " << project.project_path << "\n";
    }
    output << "\n## Source Files\n\n";
    output << "- Count: " << source_files.size() << "\n";
    output << "\n## Related Files\n\n";
    if (candidates.empty())
    {
        output << "- None\n";
    }
    for (const auto& candidate : candidates)
    {
        output << "- " << candidate.file_path << " score=" << candidate.score << "\n";
    }
}

int RunCommand(const RunOptions& options)
{
    const bool use_fake_provider = options.no_llm || options.provider == "fake";
    if (!use_fake_provider)
    {
        if (options.json)
        {
            return EmitJsonError(
                "run",
                "PROVIDER_UNSUPPORTED",
                "run currently supports --provider fake or --no-llm for patch execution.",
                "Use analyze/review for real provider calls, or run with --provider fake.");
        }
        std::cerr << "run currently supports --provider fake or --no-llm for patch execution.\n";
        return 2;
    }
    if (!fs::exists(options.solution) || options.solution.extension() != ".sln")
    {
        if (options.json)
        {
            return EmitJsonError(
                "run",
                "SOLUTION_PARSE_FAILED",
                "--solution must point to an existing .sln file.",
                "Pass the absolute path to a Visual Studio solution file.");
        }
        std::cerr << "--solution must point to an existing .sln file.\n";
        return 2;
    }

    const fs::path source_root = fs::absolute(options.solution).parent_path();
    const std::string task_id = MakeTaskId(options.solution, options.task);

    const agentguard::WorkspacePaths workspace = agentguard::RunWorkspace(agentguard::WorkspaceOptions{
        source_root,
        options.runs_root,
        task_id,
        options.force
    }).Prepare();

    const fs::path workspace_solution = workspace.repo_root / options.solution.filename();
    const agentguard::SlnParseResult sln_result = agentguard::ParseSln(workspace_solution);

    agentguard::ProjectInfo project_info;
    project_info.solution_name = workspace_solution.stem().string();
    project_info.solution_path = workspace_solution.string();

    for (const auto& sln_project : sln_result.projects)
    {
        auto parsed_project = agentguard::ParseVcxproj(workspace.repo_root / fs::path(sln_project.project_path));
        parsed_project.project_guid = sln_project.project_guid;
        parsed_project.project_type_guid = sln_project.project_type_guid;
        project_info.projects.push_back(std::move(parsed_project));
    }

    const auto source_files = agentguard::IndexSourceFiles(workspace.repo_root);
    std::vector<agentguard::SourceFileInfo> symbol_files;
    symbol_files.reserve(source_files.size());
    for (const auto& source_file : source_files)
    {
        symbol_files.push_back(agentguard::IndexCppSymbols(workspace.repo_root / source_file.file_path, workspace.repo_root));
    }

    const auto candidates = agentguard::SelectRelevantFiles(options.task, symbol_files);
    const auto allowed_files = SelectAllowedFiles(candidates, symbol_files);
    const auto forbidden_files = BuildForbiddenFiles(symbol_files, allowed_files);
    const std::string target_project = project_info.projects.empty() ? "" : project_info.projects.front().project_name;

    agentguard::FakeLLMProvider planner_provider(
        "",
        MakePlannerJson(task_id, options, target_project, allowed_files, forbidden_files));

    agentguard::PlannerInput planner_input;
    planner_input.user_request = options.task;
    planner_input.project_info = project_info;
    planner_input.source_files = symbol_files;
    planner_input.context_candidates = candidates;

    agentguard::SemanticAnalyzerInput semantic_input;
    semantic_input.user_request = options.task;
    semantic_input.solution_path = workspace_solution;
    semantic_input.project_info = project_info;
    semantic_input.source_files = symbol_files;
    semantic_input.context_candidates = candidates;
    semantic_input.relevant_files = LoadRelevantFileContents(workspace.repo_root, allowed_files);
    semantic_input.static_allowed_files = allowed_files;
    semantic_input.static_forbidden_files = forbidden_files;

    const auto semantic_json = MakeFakeSemanticScopeJson(options.task, allowed_files, forbidden_files);
    agentguard::FakeLLMProvider semantic_provider(semantic_json.dump(), semantic_json);
    const auto semantic_result = agentguard::TryAnalyzeSemanticScope(semantic_input, semantic_provider);
    if (!semantic_result.success)
    {
        std::cerr << semantic_result.error_message << "\n";
        return 1;
    }
    planner_input.has_semantic_scope = true;
    planner_input.semantic_scope = semantic_result.scope;

    const auto planner_result = agentguard::TryPlanTask(planner_input, planner_provider);
    if (!planner_result.success)
    {
        std::cerr << planner_result.error_message << "\n";
        return 1;
    }

    const auto relevant_contents = LoadRelevantFileContents(workspace.repo_root, planner_result.task_spec.allowed_files);

    if (options.dry_run)
    {
        const fs::path dry_run_report = workspace.reports_root / "dry_run_report.md";
        WriteDryRunReport(dry_run_report, planner_result.task_spec, candidates, symbol_files, project_info.projects);

        agentguard::ReportWriteRequest report_request;
        report_request.reports_directory = workspace.reports_root;
        report_request.source_project = source_root;
        report_request.workspace_repo = workspace.repo_root;
        report_request.source_modified = "false";
        report_request.task_spec = planner_result.task_spec;
        report_request.related_files = planner_result.task_spec.allowed_files;
        report_request.diff_report = agentguard::CaptureWorkspaceDiff(workspace.repo_root);
        report_request.risk_warnings = {"Dry-run only: no patch was applied and MSBuild was not invoked."};
        const auto report = agentguard::WriteReport(report_request);

        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", true},
                {"success", true},
                {"command", "run"},
                {"mode", "dry_run"},
                {"task_id", task_id},
                {"workspace", workspace.task_root.string()},
                {"runs_root", options.runs_root.string()},
                {"runs_root_source", options.runs_root_source},
                {"report", report.markdown_path.string()},
                {"dry_run_report", dry_run_report.string()},
                {"allowed_files", planner_result.task_spec.allowed_files},
                {"forbidden_files", planner_result.task_spec.forbidden_files}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Dry run completed\n";
            std::cout << "Workspace: " << workspace.task_root.string() << "\n";
            std::cout << "Report: " << dry_run_report.string() << "\n";
            std::cout << "TaskSpec:\n" << nlohmann::json(planner_result.task_spec).dump(2) << "\n";
            std::cout << "Related files:\n";
            for (const auto& file : planner_result.task_spec.allowed_files)
            {
                std::cout << "- " << file << "\n";
            }
        }
        return 0;
    }

    agentguard::FakeLLMProvider implementer_provider("", MakeNoOpPatchPlanJson(relevant_contents));
    agentguard::FakeLLMProvider fixer_provider("", MakeNoOpPatchPlanJson(relevant_contents));

    agentguard::RepairLoopInput repair_input;
    repair_input.repo_root = workspace.repo_root;
    repair_input.logs_directory = workspace.logs_root;
    repair_input.reports_directory = workspace.reports_root;
    repair_input.planner_input = planner_input;
    repair_input.initial_relevant_files = relevant_contents;
    repair_input.git_diff_summary = "";
    repair_input.planner_provider = &planner_provider;
    repair_input.implementer_provider = &implementer_provider;
    repair_input.build_fixer_provider = &fixer_provider;
    repair_input.build_executor = [&](const agentguard::TaskSpec&, int) {
        return agentguard::BuildSolution(workspace_solution, options.configuration, options.platform);
    };

    const auto repair_result = agentguard::RunRepairLoop(repair_input);

    agentguard::ReportWriteRequest report_request;
    report_request.reports_directory = workspace.reports_root;
    report_request.source_project = source_root;
    report_request.workspace_repo = workspace.repo_root;
    report_request.source_modified = "false";
    report_request.task_spec = repair_result.report.task_spec;
    report_request.related_files = repair_result.report.related_files;
    report_request.build_result = repair_result.report.build_result;
    report_request.repair_rounds = repair_result.report.repair_rounds;
    report_request.diff_report = agentguard::CaptureWorkspaceDiff(workspace.repo_root);
    report_request.risk_warnings = repair_result.success
        ? std::vector<std::string>{}
        : std::vector<std::string>{repair_result.error_message};
    const auto report = agentguard::WriteReport(report_request);

    if (options.json)
    {
        std::cout << nlohmann::json{
            {"ok", repair_result.success},
            {"success", repair_result.success},
            {"command", "run"},
            {"task_id", task_id},
            {"workspace", workspace.task_root.string()},
            {"runs_root", options.runs_root.string()},
            {"runs_root_source", options.runs_root_source},
            {"report", report.markdown_path.string()},
            {"repair_rounds", repair_result.report.repair_rounds},
            {"build_success", repair_result.report.build_result.success},
            {"error_code", repair_result.success ? "" : "BUILD_FAILED"}
        }.dump() << "\n";
    }
    else
    {
        std::cout << "Run completed\n";
        std::cout << "Workspace: " << workspace.task_root.string() << "\n";
        std::cout << "Report: " << report.markdown_path.string() << "\n";
    }
    return repair_result.success ? 0 : 1;
}

std::unique_ptr<agentguard::ILLMProvider> CreateSmokeProvider(const agentguard::LLMProviderConfig& config)
{
    if (config.provider == "openai")
    {
        return std::make_unique<agentguard::OpenAIProvider>(config);
    }

    if (config.provider == "deepseek")
    {
        return std::make_unique<agentguard::DeepSeekProvider>(config);
    }

    if (config.provider == "claude")
    {
        return std::make_unique<agentguard::ClaudeProvider>(config);
    }

    if (config.provider == "file")
    {
        if (config.file_response_path.empty())
        {
            throw std::runtime_error(
                "AGENTGUARD_LLM_RESPONSE_FILE is required when AGENTGUARD_LLM_PROVIDER=file.");
        }
        return std::make_unique<agentguard::FileLLMProvider>(config.file_response_path);
    }

    if (config.provider == "fake")
    {
        return std::make_unique<agentguard::FakeLLMProvider>(
            nlohmann::json{{"provider", "fake"}}.dump(),
            nlohmann::json{{"provider", "fake"}});
    }

    throw std::runtime_error("Unsupported AGENTGUARD_LLM_PROVIDER: " + config.provider);
}

std::unique_ptr<agentguard::ILLMProvider> CreateProviderFromConfig(const agentguard::LLMProviderConfig& config)
{
    return CreateSmokeProvider(config);
}

agentguard::LLMProviderConfig BuildProviderConfig(
    const std::string& provider,
    const std::string& model,
    const fs::path& response_file)
{
    auto config = agentguard::LoadLLMProviderConfigFromEnvironment();
    config.provider = provider;
    if (!response_file.empty())
    {
        config.file_response_path = response_file;
    }

    if (!model.empty())
    {
        if (provider == "openai")
        {
            config.openai_model = model;
        }
        else if (provider == "deepseek")
        {
            config.deepseek_model = model;
        }
        else if (provider == "claude")
        {
            config.claude_model = model;
        }
    }

    return config;
}

std::unique_ptr<agentguard::ILLMProvider> CreateAnalyzeProvider(
    const AnalyzeOptions& options,
    const std::vector<std::string>& static_allowed_files,
    const std::vector<std::string>& static_forbidden_files,
    const agentguard::ImpactAnalysis& impact)
{
    if (options.provider == "fake")
    {
        const nlohmann::json response =
            MakeFakeSemanticScopeJson(options.task, static_allowed_files, static_forbidden_files, impact);
        return std::make_unique<agentguard::FakeLLMProvider>(response.dump(), response);
    }

    const auto config = BuildProviderConfig(options.provider, options.model, options.response_file);
    return CreateProviderFromConfig(config);
}

std::unique_ptr<agentguard::ILLMProvider> CreateReviewProvider(const ReviewOptions& options)
{
    if (options.provider == "fake")
    {
        const auto response = MakeFakeSemanticReviewJson();
        return std::make_unique<agentguard::FakeLLMProvider>(response.dump(), response);
    }

    const auto config = BuildProviderConfig(options.provider, options.model, options.response_file);
    return CreateProviderFromConfig(config);
}

int RunLlmSmokeCommand(const LlmSmokeOptions& options)
{
    try
    {
        const auto config = agentguard::LoadLLMProviderConfigFromEnvironment();
        const auto provider = CreateSmokeProvider(config);
        const auto response = provider->GenerateText(options.input);
        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", true},
                {"success", true},
                {"command", "llm-smoke"},
                {"provider", config.provider},
                {"response", response}
            }.dump() << "\n";
        }
        else
        {
            std::cout << response << "\n";
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "llm-smoke",
                ClassifyExceptionError(exception.what()),
                exception.what(),
                "Check provider, model, API key environment variables, and network access.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

int RunDetectCommand(const DetectOptions& options)
{
    try
    {
        const auto profile = agentguard::DetectProjectProfile(options.project);
        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", true},
                {"command", "detect"},
                {"profile", profile}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Project type: " << agentguard::ProjectTypeToString(profile.project_type) << "\n";
            std::cout << "Adapter: " << profile.adapter << "\n";
            std::cout << "Root: " << profile.root.string() << "\n";
            std::cout << "Verify steps: " << profile.verify_steps.size() << "\n";
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "detect",
                "PROJECT_DETECT_FAILED",
                exception.what(),
                "Check that --project points to an existing project directory.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

int RunVerifyProfileCommand(const VerifyProfileOptions& options)
{
    try
    {
        const auto profile = agentguard::DetectProjectProfile(options.project);
        const auto verification =
            agentguard::VerifyCommandProfile(profile, agentguard::ProcessRunner());

        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", verification.success},
                {"command", "verify-profile"},
                {"profile", profile},
                {"verification", verification}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Profile verification: "
                      << (verification.success ? "passed" : "failed") << "\n";
            for (const auto& step : verification.steps)
            {
                std::cout << "- " << step.name
                          << " return_code=" << step.return_code
                          << " success=" << (step.success ? "true" : "false") << "\n";
            }
        }
        return verification.success ? 0 : 1;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "verify-profile",
                "PROFILE_VERIFY_FAILED",
                exception.what(),
                "Check agentguard.json verify_steps and executable paths.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

int RunAnalyzeCommand(const AnalyzeOptions& options)
{
    try
    {
        if (!fs::exists(options.solution) || options.solution.extension() != ".sln")
        {
            if (options.json)
            {
                return EmitJsonError(
                    "analyze",
                    "SOLUTION_PARSE_FAILED",
                    "--solution must point to an existing .sln file.",
                    "Pass the absolute path to a Visual Studio solution file.");
            }
            std::cerr << "--solution must point to an existing .sln file.\n";
            return 2;
        }

        const fs::path source_root = fs::absolute(options.solution).parent_path();
        const std::string task_id = MakeTaskId(options.solution, options.task);
        const auto workspace = agentguard::RunWorkspace(agentguard::WorkspaceOptions{
            source_root,
            options.runs_root,
            task_id,
            options.force
        }).Prepare();

        const fs::path workspace_solution = workspace.repo_root / options.solution.filename();
        const auto sln_result = agentguard::ParseSln(workspace_solution);

        agentguard::ProjectInfo project_info;
        project_info.solution_name = workspace_solution.stem().string();
        project_info.solution_path = workspace_solution.string();
        for (const auto& sln_project : sln_result.projects)
        {
            auto parsed_project = agentguard::ParseVcxproj(workspace.repo_root / fs::path(sln_project.project_path));
            parsed_project.project_guid = sln_project.project_guid;
            parsed_project.project_type_guid = sln_project.project_type_guid;
            project_info.projects.push_back(std::move(parsed_project));
        }

        const auto source_files = agentguard::IndexSourceFiles(workspace.repo_root);
        std::vector<agentguard::SourceFileInfo> symbol_files;
        symbol_files.reserve(source_files.size());
        for (const auto& source_file : source_files)
        {
            symbol_files.push_back(agentguard::IndexCppSymbols(workspace.repo_root / source_file.file_path, workspace.repo_root));
        }

        const auto dependency_graph = agentguard::BuildDependencyGraph(workspace.repo_root, project_info, symbol_files);
        const auto symbol_index = agentguard::BuildSymbolIndex(symbol_files);
        const auto impact = agentguard::AnalyzeImpact(options.task, symbol_files, dependency_graph, symbol_index);
        const auto candidates = agentguard::SelectRelevantFiles(options.task, symbol_files);
        const auto allowed_files = SelectAllowedFiles(candidates, symbol_files);
        const auto forbidden_files = BuildForbiddenFiles(symbol_files, allowed_files);
        const auto relevant_contents = LoadRelevantFileContents(workspace.repo_root, allowed_files);
        const auto provider = CreateAnalyzeProvider(options, allowed_files, forbidden_files, impact);
        const auto artifacts = EnsureArtifactPaths(workspace.task_root);

        agentguard::SemanticAnalyzerInput input;
        input.user_request = options.task;
        input.solution_path = workspace_solution;
        input.project_info = project_info;
        input.source_files = symbol_files;
        input.context_candidates = candidates;
        input.relevant_files = relevant_contents;
        input.static_allowed_files = allowed_files;
        input.static_forbidden_files = forbidden_files;
        input.impact = impact;

        const std::string model_name = EffectiveModelName(options.provider, options.model);
        const std::string cache_key = agentguard::ComputeLLMCacheKey({
            {"kind", "analyze"},
            {"task", options.task},
            {"project_index_hash", HashSourceIndex(symbol_files)},
            {"relevant_file_hash", HashRelevantFiles(relevant_contents)},
            {"provider", options.provider},
            {"model", model_name}
        });
        agentguard::CachedLLMProvider cached_provider(
            *provider,
            workspace.task_root.parent_path() / "_llm_cache",
            cache_key,
            options.no_cache || options.provider == "file");

        const auto analyze_start = std::chrono::steady_clock::now();
        const auto result = agentguard::TryAnalyzeSemanticScope(input, cached_provider);
        const auto analyze_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - analyze_start).count();

        WriteTextFile(workspace.task_root / "semantic_analyze_prompt.txt", result.prompt);
        WriteTextFile(artifacts.prompts / "semantic_analyze_prompt.txt", result.prompt);
        if (!result.raw_response.empty())
        {
            WriteJsonOrRawText(workspace.task_root / "semantic_analyze_raw_response.json", result.raw_response);
            WriteJsonOrRawText(
                artifacts.raw_responses / "semantic_analyze_raw_response.json",
                result.raw_response);
        }

        if (!result.success)
        {
            if (options.json)
            {
                return EmitJsonError(
                    "analyze",
                    ClassifyExceptionError(result.error_message),
                    result.error_message,
                    "Inspect semantic_analyze_prompt.txt and semantic_analyze_raw_response.json in the workspace.");
            }
            std::cerr << result.error_message << "\n";
            return 1;
        }

        WriteTextFile(
            workspace.task_root / "semantic_scope.json",
            nlohmann::json(result.scope).dump(2));

        agentguard::TaskSpec report_spec;
        report_spec.task_id = task_id;
        report_spec.user_request = options.task;
        report_spec.target_solution = workspace_solution.string();
        report_spec.target_project = project_info.projects.empty() ? "" : project_info.projects.front().project_name;
        for (const auto& file : result.scope.allowed_files)
        {
            report_spec.allowed_files.push_back(file.path);
        }
        for (const auto& file : result.scope.protected_files)
        {
            report_spec.forbidden_files.push_back(file.path);
            report_spec.protected_files.push_back(file.path);
        }
        for (const auto& file : result.scope.context_files)
        {
            report_spec.context_files.push_back(file.path);
        }
        for (const auto& file : result.scope.suspected_files)
        {
            report_spec.suspected_files.push_back(file.path);
        }
        for (const auto& file : result.scope.needs_approval_files)
        {
            report_spec.needs_approval_files.push_back(file.path);
        }
        report_spec.has_semantic_scope = true;
        report_spec.semantic_scope = result.scope;
        report_spec.acceptance_criteria = {"Semantic scope analysis completed."};
        report_spec.max_repair_rounds = 0;

        const auto diff_report = agentguard::CaptureWorkspaceDiff(workspace.repo_root);
        WriteDiffArtifacts(artifacts, diff_report);
        agentguard::ReportMetrics metrics;
        metrics.analyze_duration_ms = analyze_duration_ms;
        metrics.modified_file_count = CountModifiedFilesFromDiff(diff_report);
        metrics.allowed_file_count = result.scope.allowed_files.size();
        metrics.suspected_file_count = result.scope.suspected_files.size();
        metrics.protected_file_count = result.scope.protected_files.size();
        metrics.build_error_count = 0;
        metrics.repair_rounds = 0;
        metrics.scope_expansions = 0;
        metrics.provider = options.provider;
        metrics.model = model_name;
        metrics.cache_hit = cached_provider.cache_hit();
        SaveMetrics(workspace.task_root, metrics);
        agentguard::ReportWriteRequest report_request;
        report_request.reports_directory = workspace.reports_root;
        report_request.source_project = source_root;
        report_request.workspace_repo = workspace.repo_root;
        report_request.source_modified = "false";
        report_request.task_spec = report_spec;
        report_request.related_files = report_spec.allowed_files;
        report_request.diff_report = diff_report;
        report_request.metrics = metrics;
        report_request.artifacts = BuildArtifactsMap(
            workspace.task_root,
            workspace.reports_root / "report.md",
            workspace.reports_root / "report.json");
        const auto report = agentguard::WriteReport(report_request);
        CopyFileIfPresent(report.markdown_path, artifacts.reports / "report.md");
        CopyFileIfPresent(report.json_path, artifacts.reports / "report.json");

        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", true},
                {"success", true},
                {"command", "analyze"},
                {"task_id", task_id},
                {"workspace", workspace.task_root.string()},
                {"runs_root", options.runs_root.string()},
                {"runs_root_source", options.runs_root_source},
                {"semantic_scope", (workspace.task_root / "semantic_scope.json").string()},
                {"semantic_scope_path", (workspace.task_root / "semantic_scope.json").string()},
                {"report", report.markdown_path.string()},
                {"report_json", report.json_path.string()},
                {"cache_hit", cached_provider.cache_hit()},
                {"counts", {
                    {"allowed", result.scope.allowed_files.size()},
                    {"context", result.scope.context_files.size()},
                    {"suspected", result.scope.suspected_files.size()},
                    {"protected", result.scope.protected_files.size()},
                    {"needs_approval", result.scope.needs_approval_files.size()}
                }}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Semantic analysis completed\n";
            std::cout << "Workspace: " << workspace.task_root.string() << "\n";
            std::cout << "allowed=" << result.scope.allowed_files.size()
                      << " context=" << result.scope.context_files.size()
                      << " suspected=" << result.scope.suspected_files.size()
                      << " protected=" << result.scope.protected_files.size()
                      << " needs_approval=" << result.scope.needs_approval_files.size()
                      << "\n";
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "analyze",
                ClassifyExceptionError(exception.what()),
                exception.what(),
                "Check solution path, provider configuration, Git availability, and workspace permissions.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

int RunReviewCommand(const ReviewOptions& options)
{
    try
    {
        const fs::path repo_root = ResolveWorkspaceRepoRoot(options.workspace);
        const fs::path task_root = ResolveTaskRootFromWorkspace(options.workspace);
        if (!fs::exists(repo_root) || !fs::is_directory(repo_root))
        {
            if (options.json)
            {
                return EmitJsonError(
                    "review",
                    "WORKSPACE_CREATE_FAILED",
                    "--workspace must point to an existing workspace or repo directory.",
                    "Pass runs/<task_id> or runs/<task_id>/repo.");
            }
            std::cerr << "--workspace must point to an existing workspace or repo directory.\n";
            return 2;
        }

        const fs::path scope_path = task_root / "semantic_scope.json";
        if (!fs::exists(scope_path))
        {
            if (options.json)
            {
                return EmitJsonError(
                    "review",
                    "SCOPE_CONFLICT",
                    "semantic_scope.json was not found beside the workspace repo.",
                    "Run analyze before review and pass the matching workspace.");
            }
            std::cerr << "semantic_scope.json was not found beside the workspace repo.\n";
            return 2;
        }

        const auto scope = nlohmann::json::parse(ReadTextIfPresent(scope_path))
            .get<agentguard::SemanticScopeResult>();
        agentguard::DiffReport diff_report;
        try
        {
            diff_report = agentguard::CaptureWorkspaceDiff(repo_root);
        }
        catch (const std::exception& exception)
        {
            diff_report.warnings.push_back(exception.what());
        }
        auto build_result = LoadLatestBuildResult(task_root);
        build_result.parsed_errors = agentguard::ParseBuildErrors(build_result);

        agentguard::SemanticReviewInput input;
        input.user_request = options.task;
        input.semantic_scope = scope;
        input.diff_text = diff_report.diff_text.empty() ? diff_report.diff_stat : diff_report.diff_text;
        input.build_result = build_result;
        input.parsed_errors = build_result.parsed_errors;
        input.relevant_files = LoadSemanticReviewFileContents(repo_root, scope);

        const auto provider = CreateReviewProvider(options);
        const auto artifacts = EnsureArtifactPaths(task_root);
        const std::string model_name = EffectiveModelName(options.provider, options.model);
        const std::string diff_hash = agentguard::StableHashText(input.diff_text);
        const std::string build_log_hash = agentguard::StableHashText(build_result.stdout_text + build_result.stderr_text);
        const std::string cache_key = agentguard::ComputeLLMCacheKey({
            {"kind", "review"},
            {"task", options.task},
            {"project_index_hash", agentguard::StableHashText(nlohmann::json(scope).dump())},
            {"relevant_file_hash", HashRelevantFiles(input.relevant_files)},
            {"diff_hash", diff_hash},
            {"build_log_hash", build_log_hash},
            {"provider", options.provider},
            {"model", model_name}
        });
        agentguard::CachedLLMProvider cached_provider(
            *provider,
            task_root.parent_path() / "_llm_cache",
            cache_key,
            options.no_cache || options.provider == "file");

        const auto review_start = std::chrono::steady_clock::now();
        const auto result = agentguard::TryReviewSemanticChange(input, cached_provider);
        const auto review_duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - review_start).count();

        WriteTextFile(task_root / "semantic_review_prompt.txt", result.prompt);
        WriteTextFile(artifacts.prompts / "semantic_review_prompt.txt", result.prompt);
        if (!result.raw_response.empty())
        {
            WriteJsonOrRawText(task_root / "semantic_review_raw_response.json", result.raw_response);
            WriteJsonOrRawText(
                artifacts.raw_responses / "semantic_review_raw_response.json",
                result.raw_response);
        }

        if (!result.success)
        {
            if (options.json)
            {
                return EmitJsonError(
                    "review",
                    ClassifyExceptionError(result.error_message),
                    result.error_message,
                    "Inspect semantic_review_prompt.txt and semantic_review_raw_response.json in the workspace.");
            }
            std::cerr << result.error_message << "\n";
            return 1;
        }

        WriteTextFile(task_root / "semantic_review.json", nlohmann::json(result.review).dump(2));
        WriteDiffArtifacts(artifacts, diff_report);

        auto metrics = LoadMetrics(task_root);
        metrics.review_duration_ms = review_duration_ms;
        metrics.modified_file_count = CountModifiedFilesFromDiff(diff_report);
        metrics.allowed_file_count = scope.allowed_files.size();
        metrics.suspected_file_count = scope.suspected_files.size();
        metrics.protected_file_count = scope.protected_files.size();
        metrics.build_error_count = build_result.parsed_errors.size();
        metrics.repair_rounds = 0;
        metrics.scope_expansions = result.review.requires_scope_expansion ? 1 : 0;
        metrics.provider = options.provider;
        metrics.model = model_name;
        metrics.cache_hit = cached_provider.cache_hit();
        SaveMetrics(task_root, metrics);

        agentguard::TaskSpec report_spec;
        report_spec.user_request = options.task;
        report_spec.has_semantic_scope = true;
        report_spec.semantic_scope = scope;
        for (const auto& file : scope.allowed_files)
        {
            report_spec.allowed_files.push_back(file.path);
        }
        for (const auto& file : scope.context_files)
        {
            report_spec.context_files.push_back(file.path);
        }
        for (const auto& file : scope.suspected_files)
        {
            report_spec.suspected_files.push_back(file.path);
        }
        for (const auto& file : scope.protected_files)
        {
            report_spec.protected_files.push_back(file.path);
            report_spec.forbidden_files.push_back(file.path);
        }
        for (const auto& file : scope.needs_approval_files)
        {
            report_spec.needs_approval_files.push_back(file.path);
        }
        agentguard::ReportWriteRequest report_request;
        report_request.reports_directory = task_root / "reports";
        report_request.source_project = task_root;
        report_request.workspace_repo = repo_root;
        report_request.source_modified = "false";
        report_request.task_spec = report_spec;
        report_request.related_files = report_spec.allowed_files;
        report_request.build_result = build_result;
        report_request.diff_report = diff_report;
        report_request.semantic_review = result.review;
        report_request.review_next_action = result.review.next_action;
        report_request.metrics = metrics;
        report_request.risk_warnings = result.review.notes;
        report_request.artifacts = BuildArtifactsMap(
            task_root,
            task_root / "reports" / "report.md",
            task_root / "reports" / "report.json");
        const auto report = agentguard::WriteReport(report_request);
        CopyFileIfPresent(report.markdown_path, artifacts.reports / "report.md");
        CopyFileIfPresent(report.json_path, artifacts.reports / "report.json");

        if (options.json)
        {
            std::cout << nlohmann::json{
                {"ok", true},
                {"success", true},
                {"command", "review"},
                {"workspace", task_root.string()},
                {"semantic_review_path", (task_root / "semantic_review.json").string()},
                {"semantic_review", (task_root / "semantic_review.json").string()},
                {"report", report.markdown_path.string()},
                {"report_json", report.json_path.string()},
                {"next_action", result.review.next_action},
                {"requires_scope_expansion", result.review.requires_scope_expansion},
                {"risk_count", result.review.risks.size()},
                {"confidence", result.review.confidence},
                {"cache_hit", cached_provider.cache_hit()},
                {"diff", {
                    {"git_top_level", diff_report.git_top_level.string()},
                    {"diff_base", diff_report.diff_base},
                    {"workspace_modified", diff_report.workspace_modified},
                    {"warnings", diff_report.warnings}
                }}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Semantic review completed\n";
            std::cout << "Workspace: " << task_root.string() << "\n";
            std::cout << "next_action=" << result.review.next_action
                      << " risk_count=" << result.review.risks.size()
                      << " requires_scope_expansion="
                      << (result.review.requires_scope_expansion ? "true" : "false")
                      << "\n";
        }
        return 0;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "review",
                ClassifyExceptionError(exception.what()),
                exception.what(),
                "Check semantic_scope.json, provider output, and workspace Git baseline.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

int RunVerifyCommand(const VerifyOptions& options)
{
    try
    {
        const fs::path repo_root = ResolveWorkspaceRepoRoot(options.workspace);
        const fs::path task_root = ResolveTaskRootFromWorkspace(options.workspace);
        if (!fs::exists(repo_root) || !fs::is_directory(repo_root))
        {
            if (options.json)
            {
                return EmitJsonError(
                    "verify",
                    "WORKSPACE_CREATE_FAILED",
                    "--workspace must point to an existing workspace or repo directory.",
                    "Pass runs/<task_id> or runs/<task_id>/repo.");
            }
            std::cerr << "--workspace must point to an existing workspace or repo directory.\n";
            return 2;
        }

        const fs::path solution = FindFirstSolution(repo_root);
        if (solution.empty())
        {
            if (options.json)
            {
                return EmitJsonError(
                    "verify",
                    "SOLUTION_PARSE_FAILED",
                    "No .sln file was found under the workspace.",
                    "Run analyze first and pass its workspace path.");
            }
            std::cerr << "No .sln file was found under the workspace.\n";
            return 2;
        }

        auto build_result = agentguard::BuildSolution(
            solution,
            options.configuration,
            options.platform);
        build_result.parsed_errors = agentguard::ParseBuildErrors(build_result);
        const auto artifacts = EnsureArtifactPaths(task_root);

        const fs::path log_path = task_root / "logs" / "verify_build.log";
        std::string log;
        log += "Command: " + build_result.command + "\n";
        log += "Return code: " + std::to_string(build_result.return_code) + "\n";
        log += "Stdout:\n" + build_result.stdout_text + "\n";
        log += "Stderr:\n" + build_result.stderr_text + "\n";
        WriteTextFile(log_path, log);
        WriteTextFile(artifacts.build_logs / "verify_build.log", log);

        auto metrics = LoadMetrics(task_root);
        metrics.verify_duration_ms = build_result.duration_ms;
        metrics.build_error_count = build_result.parsed_errors.size();
        SaveMetrics(task_root, metrics);

        agentguard::TaskSpec report_spec;
        report_spec.target_solution = solution.string();
        const fs::path scope_path = task_root / "semantic_scope.json";
        if (fs::exists(scope_path))
        {
            report_spec.has_semantic_scope = true;
            report_spec.semantic_scope = nlohmann::json::parse(ReadTextIfPresent(scope_path))
                .get<agentguard::SemanticScopeResult>();
            for (const auto& file : report_spec.semantic_scope.allowed_files)
            {
                report_spec.allowed_files.push_back(file.path);
            }
            for (const auto& file : report_spec.semantic_scope.suspected_files)
            {
                report_spec.suspected_files.push_back(file.path);
            }
            for (const auto& file : report_spec.semantic_scope.protected_files)
            {
                report_spec.protected_files.push_back(file.path);
                report_spec.forbidden_files.push_back(file.path);
            }
            for (const auto& file : report_spec.semantic_scope.needs_approval_files)
            {
                report_spec.needs_approval_files.push_back(file.path);
            }
        }
        agentguard::DiffReport diff_report;
        try
        {
            diff_report = agentguard::CaptureWorkspaceDiff(repo_root);
        }
        catch (const std::exception& exception)
        {
            diff_report.warnings.push_back(exception.what());
        }
        WriteDiffArtifacts(artifacts, diff_report);
        agentguard::ReportWriteRequest report_request;
        report_request.reports_directory = task_root / "reports";
        report_request.source_project = task_root;
        report_request.workspace_repo = repo_root;
        report_request.source_modified = "false";
        report_request.task_spec = report_spec;
        report_request.related_files = report_spec.allowed_files;
        report_request.build_result = build_result;
        report_request.diff_report = diff_report;
        report_request.metrics = metrics;
        report_request.artifacts = BuildArtifactsMap(
            task_root,
            task_root / "reports" / "report.md",
            task_root / "reports" / "report.json");
        const auto report = agentguard::WriteReport(report_request);
        CopyFileIfPresent(report.markdown_path, artifacts.reports / "report.md");
        CopyFileIfPresent(report.json_path, artifacts.reports / "report.json");

        if (options.json)
        {
            const std::string error_code = build_result.return_code == -1 ? "MSBUILD_NOT_FOUND" : "BUILD_FAILED";
            std::cout << nlohmann::json{
                {"ok", build_result.success},
                {"success", build_result.success},
                {"command", "verify"},
                {"solution", solution.string()},
                {"return_code", build_result.return_code},
                {"parsed_errors", build_result.parsed_errors.size()},
                {"error_code", build_result.success ? "" : error_code},
                {"build", {
                    {"success", build_result.success},
                    {"return_code", build_result.return_code},
                    {"parsed_errors", build_result.parsed_errors.size()}
                }},
                {"artifacts", {
                    {"log", log_path.string()},
                    {"build_log", (artifacts.build_logs / "verify_build.log").string()},
                    {"report", report.markdown_path.string()},
                    {"report_json", report.json_path.string()}
                }},
                {"log_path", log_path.string()}
            }.dump() << "\n";
        }
        else
        {
            std::cout << "Verification completed\n";
            std::cout << "Solution: " << solution.string() << "\n";
            std::cout << "Success: " << (build_result.success ? "true" : "false") << "\n";
            std::cout << "Log: " << log_path.string() << "\n";
        }
        return build_result.success ? 0 : 1;
    }
    catch (const std::exception& exception)
    {
        if (options.json)
        {
            return EmitJsonError(
                "verify",
                ClassifyExceptionError(exception.what()),
                exception.what(),
                "Check workspace path, MSBuild installation, and project configuration.");
        }
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
} // namespace

int AppMain(int argc, char** argv)
{
    CLI::App app{"AgentGuardVS-CXX"};
    app.set_version_flag("--version", "AgentGuardVS-CXX 0.1.0");

    RunOptions run_options;
    auto* run = app.add_subcommand("run", "Run an isolated AgentGuardVS-CXX task.");
    run->add_option("--solution", run_options.solution, "Path to the target .sln file.")->required();
    run->add_option("--task", run_options.task, "User task description.")->required();
    run->add_option("--provider", run_options.provider, "LLM provider: fake, file, openai, deepseek, claude.")
        ->default_val("fake");
    run->add_option("--model", run_options.model, "Model name for network-backed providers.");
    run->add_option("--response-file", run_options.response_file, "Response file for --provider file.");
    run->add_option("--configuration", run_options.configuration, "MSBuild configuration.")->default_val("Debug");
    run->add_option("--platform", run_options.platform, "MSBuild platform.")->default_val("x64");
    auto* run_runs_root =
        run->add_option("--runs-root", run_options.runs_root, "Root directory for isolated runs.");
    run->add_option("--max-repair-rounds", run_options.max_repair_rounds, "Maximum automated repair rounds.")
        ->default_val(3);
    run->add_flag("--dry-run", run_options.dry_run, "Generate TaskSpec and context without applying patches.");
    run->add_flag("--no-llm", run_options.no_llm, "Use deterministic FakeLLMProvider output.");
    run->add_flag("--force", run_options.force, "Overwrite an existing task workspace.");
    run->add_flag("--json", run_options.json, "Print a machine-readable JSON summary.");

    LlmSmokeOptions smoke_options;
    auto* llm_smoke = app.add_subcommand("llm-smoke", "Call the configured LLM provider and print raw output.");
    llm_smoke->add_option("--input", smoke_options.input, "Input text to send to the provider.")->required();
    llm_smoke->add_flag("--json", smoke_options.json, "Print a machine-readable JSON summary.");

    AnalyzeOptions analyze_options;
    auto* analyze = app.add_subcommand("analyze", "Analyze semantic file scope without applying patches.");
    analyze->add_option("--solution", analyze_options.solution, "Path to the target .sln file.")->required();
    analyze->add_option("--task", analyze_options.task, "User task description.")->required();
    analyze->add_option("--provider", analyze_options.provider, "LLM provider: fake, file, openai, deepseek, claude.")
        ->default_val("fake");
    analyze->add_option("--model", analyze_options.model, "Model name for network-backed providers.");
    analyze->add_option("--response-file", analyze_options.response_file, "Response file for --provider file.");
    auto* analyze_runs_root =
        analyze->add_option("--runs-root", analyze_options.runs_root, "Root directory for isolated runs.");
    analyze->add_flag("--force", analyze_options.force, "Overwrite an existing task workspace.");
    analyze->add_flag("--no-cache", analyze_options.no_cache, "Bypass LLM response cache for this analyze call.");
    analyze->add_flag("--json", analyze_options.json, "Print a machine-readable JSON summary.");

    VerifyOptions verify_options;
    auto* verify = app.add_subcommand("verify", "Build a workspace with MSBuild and write a verification log.");
    verify->add_option("--workspace", verify_options.workspace, "Workspace task root or repo directory.")->required();
    verify->add_option("--configuration", verify_options.configuration, "MSBuild configuration.")->default_val("Debug");
    verify->add_option("--platform", verify_options.platform, "MSBuild platform.")->default_val("x64");
    verify->add_flag("--json", verify_options.json, "Print a machine-readable JSON summary.");

    DetectOptions detect_options;
    auto* detect = app.add_subcommand("detect", "Detect a generic project profile.");
    detect->add_option("--project", detect_options.project, "Path to the target project root.")->required();
    detect->add_flag("--json", detect_options.json, "Print a machine-readable JSON summary.");

    VerifyProfileOptions verify_profile_options;
    auto* verify_profile =
        app.add_subcommand("verify-profile", "Run configured generic project verification commands.");
    verify_profile->add_option(
        "--project",
        verify_profile_options.project,
        "Path to the target project root.")->required();
    verify_profile->add_flag(
        "--json",
        verify_profile_options.json,
        "Print a machine-readable JSON summary.");

    ReviewOptions review_options;
    auto* review = app.add_subcommand("review", "Review semantic completeness after a patch or build failure.");
    review->add_option("--workspace", review_options.workspace, "Workspace task root or repo directory.")->required();
    review->add_option("--task", review_options.task, "User task description.")->required();
    review->add_option("--provider", review_options.provider, "LLM provider: fake, file, openai, deepseek, claude.")
        ->default_val("fake");
    review->add_option("--model", review_options.model, "Model name for network-backed providers.");
    review->add_option("--response-file", review_options.response_file, "Response file for --provider file.");
    review->add_flag("--no-cache", review_options.no_cache, "Bypass LLM response cache for this review call.");
    review->add_flag("--json", review_options.json, "Print a machine-readable JSON summary.");

    CLI11_PARSE(app, argc, argv);

    ResolveRunsRoot(run_options.runs_root, run_options.runs_root_source, run_runs_root->count() > 0);
    ResolveRunsRoot(
        analyze_options.runs_root,
        analyze_options.runs_root_source,
        analyze_runs_root->count() > 0);

    try
    {
        if (*analyze)
        {
            return RunAnalyzeCommand(analyze_options);
        }

        if (*verify)
        {
            return RunVerifyCommand(verify_options);
        }

        if (*detect)
        {
            return RunDetectCommand(detect_options);
        }

        if (*verify_profile)
        {
            return RunVerifyProfileCommand(verify_profile_options);
        }

        if (*review)
        {
            return RunReviewCommand(review_options);
        }

        if (*llm_smoke)
        {
            return RunLlmSmokeCommand(smoke_options);
        }

        if (*run)
        {
            return RunCommand(run_options);
        }

        std::cout << app.help();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    try
    {
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);

        std::vector<std::string> utf8_args;
        utf8_args.reserve(static_cast<std::size_t>(argc));
        for (int index = 0; index < argc; ++index)
        {
            utf8_args.push_back(WideToUtf8(argv[index]));
        }

        std::vector<char*> utf8_argv;
        utf8_argv.reserve(utf8_args.size());
        for (auto& arg : utf8_args)
        {
            utf8_argv.push_back(arg.data());
        }

        return AppMain(static_cast<int>(utf8_argv.size()), utf8_argv.data());
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << "\n";
        return 1;
    }
}
#else
int main(int argc, char** argv)
{
    return AppMain(argc, argv);
}
#endif
