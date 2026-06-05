#include "project/ProjectProfile.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
namespace fs = std::filesystem;

bool Exists(const fs::path& path)
{
    std::error_code error;
    return fs::exists(path, error);
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string NormalizeProjectPath(const fs::path& root, const fs::path& path)
{
    std::error_code error;
    auto relative = fs::relative(path, root, error);
    if (error)
    {
        relative = path.filename();
    }
    auto text = relative.generic_string();
    return text == "." ? std::string{} : text;
}

void AddUnique(std::vector<std::string>& values, const std::string& value)
{
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

void AddUnique(std::vector<LanguageKind>& values, LanguageKind value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

std::vector<std::string> DefaultExcludeDirs()
{
    return {
        ".git", ".vs", "build", "out", "dist", "node_modules", "__pycache__",
        ".venv", "venv", "target", "bin", "obj", "runs", "artifacts",
        "coverage", ".pytest_cache", ".gradle", ".idea", ".vscode", "Debug",
        "Release", "x64", "Binaries", "Intermediate", "Saved", "DerivedDataCache"
    };
}

std::vector<std::string> DefaultConfigPatterns()
{
    return {
        "CMakeLists.txt", "package.json", "pyproject.toml", "requirements.txt",
        "setup.py", "pom.xml", "build.gradle", "settings.gradle",
        "build.gradle.kts", "settings.gradle.kts", "*.csproj", "*.fsproj",
        "*.sln", "agentguard.json"
    };
}

std::vector<std::string> CppExtensions()
{
    return {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hh", ".hxx"};
}

std::vector<std::string> JsTsExtensions()
{
    return {".js", ".jsx", ".ts", ".tsx", ".mjs", ".cjs"};
}

SourceFilePolicy BasePolicy()
{
    SourceFilePolicy policy;
    policy.exclude_dirs = DefaultExcludeDirs();
    policy.config_file_patterns = DefaultConfigPatterns();
    return policy;
}

SourceFilePolicy PolicyForKind(ProjectKind kind)
{
    auto policy = BasePolicy();
    switch (kind)
    {
    case ProjectKind::VisualStudioCxx:
    case ProjectKind::CMakeCpp:
        policy.source_extensions = CppExtensions();
        break;
    case ProjectKind::Python:
        policy.source_extensions = {".py", ".pyi"};
        break;
    case ProjectKind::Node:
        policy.source_extensions = JsTsExtensions();
        break;
    case ProjectKind::DotNet:
        policy.source_extensions = {".cs", ".csproj", ".fs", ".fsproj"};
        break;
    case ProjectKind::Java:
        policy.source_extensions = {".java"};
        break;
    case ProjectKind::Unreal:
        policy.source_extensions = CppExtensions();
        policy.source_extensions.push_back(".uproject");
        policy.source_extensions.push_back(".uplugin");
        policy.source_extensions.push_back(".cs");
        break;
    case ProjectKind::Custom:
    case ProjectKind::Unknown:
        policy.source_extensions = CppExtensions();
        for (const auto& extension : std::vector<std::string>{".py", ".pyi", ".js", ".jsx", ".ts", ".tsx",
                 ".mjs", ".cjs", ".java", ".cs", ".csproj", ".fs", ".fsproj", ".uproject", ".uplugin"})
        {
            policy.source_extensions.push_back(extension);
        }
        break;
    }
    return policy;
}

std::vector<LanguageKind> LanguagesForKind(ProjectKind kind)
{
    switch (kind)
    {
    case ProjectKind::VisualStudioCxx:
    case ProjectKind::CMakeCpp:
        return {LanguageKind::Cpp};
    case ProjectKind::Python:
        return {LanguageKind::Python};
    case ProjectKind::Node:
        return {LanguageKind::JavaScript, LanguageKind::TypeScript};
    case ProjectKind::DotNet:
        return {LanguageKind::CSharp, LanguageKind::FSharp};
    case ProjectKind::Java:
        return {LanguageKind::Java};
    case ProjectKind::Unreal:
        return {LanguageKind::Unreal, LanguageKind::Cpp, LanguageKind::CSharp};
    case ProjectKind::Custom:
        return {LanguageKind::Custom};
    case ProjectKind::Unknown:
        return {LanguageKind::Unknown};
    }
    return {LanguageKind::Unknown};
}

std::vector<fs::path> FindFilesByExtension(
    const fs::path& root,
    const std::vector<std::string>& extensions)
{
    std::vector<fs::path> files;
    std::error_code error;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error);
    const fs::recursive_directory_iterator end;
    while (!error && it != end)
    {
        const auto current = it->path();
        if (it->is_directory(error))
        {
            const auto name = current.filename().string();
            const auto excluded = DefaultExcludeDirs();
            if (std::find(excluded.begin(), excluded.end(), name) != excluded.end())
            {
                it.disable_recursion_pending();
            }
        }
        else if (it->is_regular_file(error))
        {
            const auto extension = ToLower(current.extension().string());
            if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end())
            {
                files.push_back(current);
            }
        }
        it.increment(error);
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::vector<fs::path> FindTopLevelFilesByExtension(
    const fs::path& root,
    const std::vector<std::string>& extensions)
{
    std::vector<fs::path> files;
    std::error_code error;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
         !error && it != end;
         it.increment(error))
    {
        if (!it->is_regular_file(error))
        {
            continue;
        }
        const auto extension = ToLower(it->path().extension().string());
        if (std::find(extensions.begin(), extensions.end(), extension) != extensions.end())
        {
            files.push_back(it->path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

bool HasPackageScript(const fs::path& package_json, const std::string& script)
{
    try
    {
        std::ifstream input(package_json, std::ios::binary);
        const auto json_value = nlohmann::json::parse(input);
        return json_value.contains("scripts") &&
            json_value.at("scripts").is_object() &&
            json_value.at("scripts").contains(script);
    }
    catch (const std::exception&)
    {
        return false;
    }
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

bool SlnLooksDotNet(const fs::path& root, const std::vector<fs::path>& sln_files)
{
    if (!FindFilesByExtension(root, {".csproj", ".fsproj"}).empty())
    {
        return true;
    }
    for (const auto& sln : sln_files)
    {
        const std::string text = ToLower(ReadTextIfPresent(sln));
        if (text.find(".csproj") != std::string::npos || text.find(".fsproj") != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

std::vector<std::string> ReadStringList(const nlohmann::json& json_value, const char* field)
{
    if (!json_value.contains(field))
    {
        return {};
    }
    if (!json_value.at(field).is_array())
    {
        throw std::runtime_error(std::string("agentguard.json field must be an array: ") + field);
    }
    return json_value.at(field).get<std::vector<std::string>>();
}

std::string NormalizeConfigRelativePath(const fs::path& root, const std::string& value)
{
    const fs::path relative = SafeRelativePath(fs::path(value));
    const fs::path absolute = NormalizePath(root / relative);
    if (!IsSubPath(absolute, NormalizePath(root)))
    {
        throw std::runtime_error("agentguard.json path escapes project root: " + value);
    }
    return relative.generic_string();
}

std::vector<std::string> NormalizeConfigPathList(
    const fs::path& root,
    const std::vector<std::string>& values)
{
    std::vector<std::string> normalized;
    for (const auto& value : values)
    {
        normalized.push_back(NormalizeConfigRelativePath(root, value));
    }
    return normalized;
}

void SyncCompatibilityFields(ProjectProfile& profile)
{
    profile.project_type = profile.kind;
    profile.adapter = ProjectKindToString(profile.kind);
    profile.source_extensions = profile.source_policy.source_extensions;
    profile.exclude_dirs = profile.source_policy.exclude_dirs;
    profile.verify_steps = profile.verify_commands;
}

CommandStep MakeCommand(
    std::string name,
    std::string command,
    std::vector<std::string> args,
    bool required,
    bool skip_if_missing)
{
    CommandStep step;
    step.name = std::move(name);
    step.command = std::move(command);
    step.args = std::move(args);
    step.working_directory = ".";
    step.timeout_seconds = 120;
    step.required = required;
    step.skip_if_missing_executable = skip_if_missing;
    return step;
}

std::string FirstPresetName(const fs::path& presets_path, const std::string& field)
{
    try
    {
        std::ifstream input(presets_path, std::ios::binary);
        const auto json_value = nlohmann::json::parse(input);
        if (!json_value.contains(field) || !json_value.at(field).is_array() ||
            json_value.at(field).empty())
        {
            return {};
        }
        return json_value.at(field).at(0).value("name", "");
    }
    catch (const std::exception&)
    {
        return {};
    }
}

void PopulateVerifyCommands(ProjectProfile& profile)
{
    switch (profile.kind)
    {
    case ProjectKind::VisualStudioCxx:
        profile.verify_commands.push_back(MakeCommand(
            "msbuild",
            "MSBuild.exe",
            {profile.project_files.empty() ? "." : profile.project_files.front(), "/m", "/p:Configuration=Debug", "/p:Platform=x64"},
            true,
            true));
        break;
    case ProjectKind::CMakeCpp:
    {
        if (Exists(profile.root / "build" / "agentguard-multilang" / "CMakeCache.txt"))
        {
            profile.verify_commands.push_back(MakeCommand(
                "cmake-build-existing",
                "cmake",
                {"--build", "build/agentguard-multilang", "--config", "Debug"},
                true,
                true));
            profile.verify_commands.push_back(MakeCommand(
                "ctest-existing",
                "ctest",
                {"--test-dir", "build/agentguard-multilang", "-C", "Debug", "--output-on-failure"},
                false,
                true));
            break;
        }
        const auto presets = profile.root / "CMakePresets.json";
        const std::string configure_preset = FirstPresetName(presets, "configurePresets");
        const std::string build_preset = FirstPresetName(presets, "buildPresets");
        if (!configure_preset.empty() && !build_preset.empty())
        {
            profile.verify_commands.push_back(MakeCommand("cmake-configure", "cmake", {"--preset", configure_preset}, true, true));
            profile.verify_commands.push_back(MakeCommand("cmake-build", "cmake", {"--build", "--preset", build_preset}, true, true));
        }
        else
        {
            profile.verify_commands.push_back(MakeCommand("cmake-configure", "cmake", {"-S", ".", "-B", "build"}, true, true));
            profile.verify_commands.push_back(MakeCommand("cmake-build", "cmake", {"--build", "build"}, true, true));
        }
        break;
    }
    case ProjectKind::Python:
        profile.verify_commands.push_back(MakeCommand("py-compile", "python", {"-m", "compileall", "."}, true, true));
        profile.verify_commands.push_back(MakeCommand("pytest", "pytest", {}, false, true));
        break;
    case ProjectKind::Node:
        if (Exists(profile.root / "package.json"))
        {
            if (HasPackageScript(profile.root / "package.json", "test"))
            {
                profile.verify_commands.push_back(MakeCommand("npm-test", "npm", {"test"}, true, true));
            }
            if (HasPackageScript(profile.root / "package.json", "build"))
            {
                profile.verify_commands.push_back(MakeCommand("npm-build", "npm", {"run", "build"}, false, true));
            }
        }
        if (profile.verify_commands.empty())
        {
            profile.verify_commands.push_back(MakeCommand("npm-package-check", "npm", {"pkg", "get", "name"}, false, true));
        }
        break;
    case ProjectKind::DotNet:
        profile.verify_commands.push_back(MakeCommand("dotnet-build", "dotnet", {"build"}, true, true));
        profile.verify_commands.push_back(MakeCommand("dotnet-test", "dotnet", {"test"}, false, true));
        break;
    case ProjectKind::Java:
        if (Exists(profile.root / "pom.xml"))
        {
            profile.verify_commands.push_back(MakeCommand("maven-test", "mvn", {"test"}, true, true));
        }
        else if (Exists(profile.root / "gradlew.bat"))
        {
            profile.verify_commands.push_back(MakeCommand("gradle-test", "gradlew.bat", {"test"}, true, false));
        }
        else
        {
            profile.verify_commands.push_back(MakeCommand("gradle-test", "gradle", {"test"}, true, true));
        }
        break;
    case ProjectKind::Unreal:
        break;
    case ProjectKind::Custom:
    case ProjectKind::Unknown:
        break;
    }
}

ProjectProfile InferProjectProfile(const fs::path& root)
{
    ProjectProfile profile;
    profile.root = NormalizePath(root);

    const auto top_level_sln_files = FindTopLevelFilesByExtension(root, {".sln"});
    const auto top_level_vcxproj_files = FindTopLevelFilesByExtension(root, {".vcxproj"});
    const auto sln_files = FindFilesByExtension(root, {".sln"});
    const auto vcxproj_files = FindFilesByExtension(root, {".vcxproj"});
    const auto uproject_files = FindFilesByExtension(root, {".uproject"});

    if (!uproject_files.empty())
    {
        profile.kind = ProjectKind::Unreal;
        profile.confidence = 0.95;
        profile.reasons.push_back("found .uproject");
        for (const auto& path : uproject_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
    }
    else if ((!top_level_sln_files.empty() || !top_level_vcxproj_files.empty()) &&
        !SlnLooksDotNet(root, top_level_sln_files))
    {
        profile.kind = ProjectKind::VisualStudioCxx;
        profile.confidence = 0.95;
        profile.reasons.push_back("found top-level .sln/.vcxproj C++ project metadata");
        for (const auto& path : top_level_sln_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
        for (const auto& path : top_level_vcxproj_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
    }
    else if (Exists(root / "CMakeLists.txt"))
    {
        profile.kind = ProjectKind::CMakeCpp;
        profile.confidence = 0.9;
        profile.reasons.push_back("found CMakeLists.txt");
        profile.project_files.push_back("CMakeLists.txt");
        if (Exists(root / "CMakePresets.json"))
        {
            profile.project_files.push_back("CMakePresets.json");
            profile.reasons.push_back("found CMakePresets.json");
        }
    }
    else if (Exists(root / "package.json"))
    {
        profile.kind = ProjectKind::Node;
        profile.confidence = 0.9;
        profile.reasons.push_back("found package.json");
        profile.project_files.push_back("package.json");
    }
    else if (Exists(root / "pyproject.toml") || Exists(root / "requirements.txt") || Exists(root / "setup.py"))
    {
        profile.kind = ProjectKind::Python;
        profile.confidence = 0.9;
        profile.reasons.push_back("found Python project marker");
        for (const auto& file : {"pyproject.toml", "requirements.txt", "setup.py"})
        {
            if (Exists(root / file))
            {
                profile.project_files.push_back(file);
            }
        }
    }
    else if (!FindFilesByExtension(root, {".csproj", ".fsproj"}).empty() || SlnLooksDotNet(root, sln_files))
    {
        profile.kind = ProjectKind::DotNet;
        profile.confidence = 0.9;
        profile.reasons.push_back("found .NET project metadata");
        for (const auto& path : sln_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
        for (const auto& path : FindFilesByExtension(root, {".csproj", ".fsproj"}))
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
    }
    else if (Exists(root / "pom.xml") || Exists(root / "build.gradle") || Exists(root / "build.gradle.kts"))
    {
        profile.kind = ProjectKind::Java;
        profile.confidence = 0.85;
        profile.reasons.push_back("found Java build file");
        for (const auto& file : {"pom.xml", "build.gradle", "build.gradle.kts", "settings.gradle", "settings.gradle.kts"})
        {
            if (Exists(root / file))
            {
                profile.project_files.push_back(file);
            }
        }
    }
    else if ((!sln_files.empty() || !vcxproj_files.empty()) && !SlnLooksDotNet(root, sln_files))
    {
        profile.kind = ProjectKind::VisualStudioCxx;
        profile.confidence = 0.75;
        profile.reasons.push_back("found nested .sln/.vcxproj C++ project metadata");
        for (const auto& path : sln_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
        for (const auto& path : vcxproj_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
    }
    else
    {
        profile.kind = ProjectKind::Unknown;
        profile.confidence = 0.1;
        profile.reasons.push_back("no recognized project markers");
    }

    profile.languages = LanguagesForKind(profile.kind);
    profile.source_policy = PolicyForKind(profile.kind);
    PopulateVerifyCommands(profile);
    SyncCompatibilityFields(profile);
    return profile;
}

void ApplyAgentGuardConfig(ProjectProfile& profile, const fs::path& config_path)
{
    nlohmann::json json_value;
    try
    {
        std::ifstream input(config_path, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Unable to read agentguard.json.");
        }
        json_value = nlohmann::json::parse(input);
    }
    catch (const nlohmann::json::parse_error& exception)
    {
        throw std::runtime_error(std::string("agentguard.json parse error: ") + exception.what());
    }

    profile.profile_source = "custom";
    profile.reasons.push_back("loaded agentguard.json");

    if (json_value.contains("project_kind"))
    {
        profile.kind = ProjectKindFromString(json_value.at("project_kind").get<std::string>());
    }
    else if (json_value.contains("project_type"))
    {
        profile.kind = ProjectKindFromString(json_value.at("project_type").get<std::string>());
    }
    else if (profile.kind == ProjectKind::Unknown)
    {
        profile.kind = ProjectKind::Custom;
    }

    if (json_value.contains("languages"))
    {
        profile.languages.clear();
        for (const auto& language : json_value.at("languages").get<std::vector<std::string>>())
        {
            AddUnique(profile.languages, LanguageKindFromString(language));
        }
    }
    else
    {
        profile.languages = LanguagesForKind(profile.kind);
    }

    auto policy = profile.source_policy.source_extensions.empty()
        ? PolicyForKind(profile.kind)
        : profile.source_policy;
    if (json_value.contains("include"))
    {
        policy.include = NormalizeConfigPathList(profile.root, ReadStringList(json_value, "include"));
    }
    if (json_value.contains("exclude"))
    {
        policy.exclude = NormalizeConfigPathList(profile.root, ReadStringList(json_value, "exclude"));
    }
    if (json_value.contains("source_extensions"))
    {
        policy.source_extensions = ReadStringList(json_value, "source_extensions");
    }
    if (json_value.contains("max_file_size"))
    {
        policy.max_file_size = json_value.at("max_file_size").get<std::uintmax_t>();
    }
    profile.source_policy = std::move(policy);

    if (json_value.contains("source_roots"))
    {
        profile.source_roots = NormalizeConfigPathList(profile.root, ReadStringList(json_value, "source_roots"));
    }
    if (json_value.contains("test_roots"))
    {
        profile.test_roots = NormalizeConfigPathList(profile.root, ReadStringList(json_value, "test_roots"));
    }
    if (json_value.contains("protected_files"))
    {
        profile.protected_files = NormalizeConfigPathList(profile.root, ReadStringList(json_value, "protected_files"));
    }

    if (json_value.contains("verify_commands"))
    {
        profile.verify_commands = json_value.at("verify_commands").get<std::vector<CommandStep>>();
    }
    else if (json_value.contains("verify_steps"))
    {
        profile.verify_commands = json_value.at("verify_steps").get<std::vector<CommandStep>>();
    }

    for (auto& command : profile.verify_commands)
    {
        if (command.working_directory.empty())
        {
            command.working_directory = ".";
        }
        command.working_directory = NormalizeConfigRelativePath(profile.root, command.working_directory);
    }

    AddUnique(profile.project_files, "agentguard.json");
    SyncCompatibilityFields(profile);
}
} // namespace

std::string ProjectKindToString(ProjectKind kind)
{
    switch (kind)
    {
    case ProjectKind::VisualStudioCxx:
        return "visualstudio-cxx";
    case ProjectKind::CMakeCpp:
        return "cmake-cpp";
    case ProjectKind::Python:
        return "python";
    case ProjectKind::Node:
        return "node";
    case ProjectKind::DotNet:
        return "dotnet";
    case ProjectKind::Java:
        return "java";
    case ProjectKind::Unreal:
        return "unreal";
    case ProjectKind::Custom:
        return "custom";
    case ProjectKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

ProjectKind ProjectKindFromString(const std::string& value)
{
    const std::string normalized = ToLower(value);
    if (normalized == "visualstudio-cxx")
    {
        return ProjectKind::VisualStudioCxx;
    }
    if (normalized == "cmake-cpp")
    {
        return ProjectKind::CMakeCpp;
    }
    if (normalized == "python")
    {
        return ProjectKind::Python;
    }
    if (normalized == "node" || normalized == "node-web")
    {
        return ProjectKind::Node;
    }
    if (normalized == "dotnet")
    {
        return ProjectKind::DotNet;
    }
    if (normalized == "java")
    {
        return ProjectKind::Java;
    }
    if (normalized == "unreal")
    {
        return ProjectKind::Unreal;
    }
    if (normalized == "custom")
    {
        return ProjectKind::Custom;
    }
    return ProjectKind::Unknown;
}

std::string ProjectTypeToString(ProjectType type)
{
    return ProjectKindToString(type);
}

ProjectType ProjectTypeFromString(const std::string& value)
{
    return ProjectKindFromString(value);
}

std::string LanguageKindToString(LanguageKind language)
{
    switch (language)
    {
    case LanguageKind::Cpp:
        return "cpp";
    case LanguageKind::Python:
        return "python";
    case LanguageKind::JavaScript:
        return "javascript";
    case LanguageKind::TypeScript:
        return "typescript";
    case LanguageKind::CSharp:
        return "csharp";
    case LanguageKind::FSharp:
        return "fsharp";
    case LanguageKind::Java:
        return "java";
    case LanguageKind::Unreal:
        return "unreal";
    case LanguageKind::Custom:
        return "custom";
    case LanguageKind::Unknown:
        return "unknown";
    }
    return "unknown";
}

LanguageKind LanguageKindFromString(const std::string& value)
{
    const std::string normalized = ToLower(value);
    if (normalized == "cpp" || normalized == "c++" || normalized == "cxx")
    {
        return LanguageKind::Cpp;
    }
    if (normalized == "python")
    {
        return LanguageKind::Python;
    }
    if (normalized == "javascript" || normalized == "js")
    {
        return LanguageKind::JavaScript;
    }
    if (normalized == "typescript" || normalized == "ts")
    {
        return LanguageKind::TypeScript;
    }
    if (normalized == "csharp" || normalized == "c#")
    {
        return LanguageKind::CSharp;
    }
    if (normalized == "fsharp" || normalized == "f#")
    {
        return LanguageKind::FSharp;
    }
    if (normalized == "java")
    {
        return LanguageKind::Java;
    }
    if (normalized == "unreal")
    {
        return LanguageKind::Unreal;
    }
    if (normalized == "custom")
    {
        return LanguageKind::Custom;
    }
    return LanguageKind::Unknown;
}

ProjectProfile DetectProjectProfile(const fs::path& root)
{
    if (root.empty())
    {
        throw std::invalid_argument("Project root cannot be empty.");
    }
    if (!Exists(root) || !fs::is_directory(root))
    {
        throw std::runtime_error("Project root does not exist or is not a directory.");
    }

    auto profile = InferProjectProfile(root);
    const auto config_path = root / "agentguard.json";
    if (Exists(config_path))
    {
        ApplyAgentGuardConfig(profile, config_path);
    }
    return profile;
}

bool IsPathAllowedBySourcePolicy(
    const fs::path& root,
    const SourceFilePolicy& policy,
    const fs::path& relative_path)
{
    const fs::path safe_relative = SafeRelativePath(relative_path);
    const fs::path absolute = NormalizePath(root / safe_relative);
    return IsSubPath(absolute, NormalizePath(root));
}

void to_json(nlohmann::json& json_value, const CommandStep& step)
{
    json_value = nlohmann::json{
        {"name", step.name},
        {"command", step.command},
        {"args", step.args},
        {"working_directory", step.working_directory},
        {"timeout_seconds", step.timeout_seconds},
        {"required", step.required},
        {"skip_if_missing_executable", step.skip_if_missing_executable}
    };
}

void from_json(const nlohmann::json& json_value, CommandStep& step)
{
    json_value.at("name").get_to(step.name);
    if (json_value.contains("command"))
    {
        json_value.at("command").get_to(step.command);
    }
    else
    {
        json_value.at("executable").get_to(step.command);
    }
    if (json_value.contains("args"))
    {
        json_value.at("args").get_to(step.args);
    }
    else if (json_value.contains("arguments"))
    {
        json_value.at("arguments").get_to(step.args);
    }
    step.working_directory = json_value.value("working_directory", ".");
    step.timeout_seconds = json_value.value("timeout_seconds", 0);
    step.required = json_value.value("required", true);
    step.skip_if_missing_executable = json_value.value("skip_if_missing_executable", false);
}

void to_json(nlohmann::json& json_value, const SourceFilePolicy& policy)
{
    json_value = nlohmann::json{
        {"include", policy.include},
        {"exclude", policy.exclude},
        {"source_extensions", policy.source_extensions},
        {"config_file_patterns", policy.config_file_patterns},
        {"exclude_dirs", policy.exclude_dirs},
        {"max_file_size", policy.max_file_size},
        {"skip_binary_files", policy.skip_binary_files},
        {"skip_generated_files", policy.skip_generated_files}
    };
}

void to_json(nlohmann::json& json_value, const ProjectProfile& profile)
{
    std::vector<std::string> languages;
    for (const auto language : profile.languages)
    {
        languages.push_back(LanguageKindToString(language));
    }

    json_value = nlohmann::json{
        {"root", profile.root.string()},
        {"kind", ProjectKindToString(profile.kind)},
        {"project_kind", ProjectKindToString(profile.kind)},
        {"project_type", ProjectKindToString(profile.kind)},
        {"adapter", profile.adapter},
        {"languages", languages},
        {"source_extensions", profile.source_extensions},
        {"exclude_dirs", profile.exclude_dirs},
        {"source_policy", profile.source_policy},
        {"project_files", profile.project_files},
        {"source_roots", profile.source_roots},
        {"test_roots", profile.test_roots},
        {"protected_files", profile.protected_files},
        {"verify_commands", profile.verify_commands},
        {"verify_steps", profile.verify_commands},
        {"confidence", profile.confidence},
        {"reasons", profile.reasons},
        {"profile_source", profile.profile_source}
    };
}
} // namespace agentguard
