#include "project/ProjectProfile.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
bool Exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error);
}

std::string NormalizeProjectPath(const std::filesystem::path& root, const std::filesystem::path& path)
{
    std::error_code error;
    auto relative = std::filesystem::relative(path, root, error);
    if (error)
    {
        relative = path.filename();
    }
    auto text = relative.generic_string();
    if (text == ".")
    {
        return {};
    }
    return text;
}

bool IsIgnoredDirectory(const std::filesystem::path& path)
{
    static const std::unordered_set<std::string> ignored{
        ".git",
        ".vs",
        "build",
        "Debug",
        "Release",
        "x64",
        "Binaries",
        "Intermediate",
        "Saved",
        "DerivedDataCache",
        "__pycache__"
    };
    return ignored.contains(path.filename().string());
}

std::vector<std::filesystem::path> FindFilesByExtension(
    const std::filesystem::path& root,
    const std::vector<std::string>& extensions)
{
    std::vector<std::filesystem::path> files;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && it != end)
    {
        const auto current = it->path();
        if (it->is_directory(error) && IsIgnoredDirectory(current))
        {
            it.disable_recursion_pending();
        }
        else if (it->is_regular_file(error))
        {
            const auto extension = current.extension().string();
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

bool HasPackageScript(const std::filesystem::path& package_json, const std::string& script)
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

std::vector<std::string> ReadStringList(const nlohmann::json& json_value, const char* field)
{
    if (!json_value.contains(field))
    {
        return {};
    }
    return json_value.at(field).get<std::vector<std::string>>();
}

void ApplyAgentGuardConfig(ProjectProfile& profile, const std::filesystem::path& config_path)
{
    std::ifstream input(config_path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to read agentguard.json.");
    }

    const auto json_value = nlohmann::json::parse(input);
    if (json_value.contains("project_type"))
    {
        profile.project_type = ProjectTypeFromString(json_value.at("project_type").get<std::string>());
    }
    if (json_value.contains("adapter"))
    {
        profile.adapter = json_value.at("adapter").get<std::string>();
    }
    else
    {
        profile.adapter = ProjectTypeToString(profile.project_type);
    }
    profile.source_roots = ReadStringList(json_value, "source_roots");
    profile.test_roots = ReadStringList(json_value, "test_roots");
    profile.protected_files = ReadStringList(json_value, "protected_files");
    if (json_value.contains("verify_steps"))
    {
        profile.verify_steps = json_value.at("verify_steps").get<std::vector<CommandStep>>();
    }
}

ProjectProfile InferProjectProfile(const std::filesystem::path& root)
{
    ProjectProfile profile;
    profile.root = NormalizePath(root);

    const auto sln_files = FindFilesByExtension(root, {".sln"});
    const auto vcxproj_files = FindFilesByExtension(root, {".vcxproj"});
    if (!sln_files.empty() || !vcxproj_files.empty())
    {
        profile.project_type = ProjectType::VisualStudioCxx;
        profile.adapter = ProjectTypeToString(profile.project_type);
        for (const auto& path : sln_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
        for (const auto& path : vcxproj_files)
        {
            profile.project_files.push_back(NormalizeProjectPath(root, path));
        }
        return profile;
    }

    if (Exists(root / "package.json"))
    {
        profile.project_type = ProjectType::NodeWeb;
        profile.adapter = ProjectTypeToString(profile.project_type);
        profile.project_files.push_back("package.json");
        const auto package_json = root / "package.json";
        if (HasPackageScript(package_json, "build"))
        {
            profile.verify_steps.push_back(CommandStep{"build", "npm", {"run", "build"}});
        }
        if (HasPackageScript(package_json, "test"))
        {
            profile.verify_steps.push_back(CommandStep{"test", "npm", {"test"}});
        }
        if (HasPackageScript(package_json, "lint"))
        {
            profile.verify_steps.push_back(CommandStep{"lint", "npm", {"run", "lint"}});
        }
        return profile;
    }

    if (Exists(root / "pyproject.toml") || Exists(root / "requirements.txt"))
    {
        profile.project_type = ProjectType::Python;
        profile.adapter = ProjectTypeToString(profile.project_type);
        if (Exists(root / "pyproject.toml"))
        {
            profile.project_files.push_back("pyproject.toml");
        }
        if (Exists(root / "requirements.txt"))
        {
            profile.project_files.push_back("requirements.txt");
        }
        profile.verify_steps.push_back(CommandStep{"test", "python", {"-m", "pytest"}});
        return profile;
    }

    if (Exists(root / "CMakeLists.txt"))
    {
        profile.project_type = ProjectType::CMakeCpp;
        profile.adapter = ProjectTypeToString(profile.project_type);
        profile.project_files.push_back("CMakeLists.txt");
        profile.verify_steps.push_back(CommandStep{"build", "cmake", {"--build", "build"}});
        profile.verify_steps.push_back(CommandStep{"test", "ctest", {"--test-dir", "build"}});
        return profile;
    }

    if (!FindFilesByExtension(root, {".csproj"}).empty())
    {
        profile.project_type = ProjectType::DotNet;
        profile.adapter = ProjectTypeToString(profile.project_type);
        profile.verify_steps.push_back(CommandStep{"build", "dotnet", {"build"}});
        profile.verify_steps.push_back(CommandStep{"test", "dotnet", {"test"}});
        return profile;
    }

    if (Exists(root / "pom.xml") || Exists(root / "build.gradle") || Exists(root / "build.gradle.kts"))
    {
        profile.project_type = ProjectType::Java;
        profile.adapter = ProjectTypeToString(profile.project_type);
        return profile;
    }

    if (!FindFilesByExtension(root, {".uproject"}).empty())
    {
        profile.project_type = ProjectType::Unreal;
        profile.adapter = ProjectTypeToString(profile.project_type);
        return profile;
    }

    profile.project_type = ProjectType::Unknown;
    profile.adapter = ProjectTypeToString(profile.project_type);
    return profile;
}
} // namespace

std::string ProjectTypeToString(ProjectType type)
{
    switch (type)
    {
    case ProjectType::VisualStudioCxx:
        return "visualstudio-cxx";
    case ProjectType::CMakeCpp:
        return "cmake-cpp";
    case ProjectType::NodeWeb:
        return "node-web";
    case ProjectType::Python:
        return "python";
    case ProjectType::DotNet:
        return "dotnet";
    case ProjectType::Java:
        return "java";
    case ProjectType::Unreal:
        return "unreal";
    case ProjectType::Custom:
        return "custom";
    case ProjectType::Unknown:
        return "unknown";
    }
    return "unknown";
}

ProjectType ProjectTypeFromString(const std::string& value)
{
    if (value == "visualstudio-cxx")
    {
        return ProjectType::VisualStudioCxx;
    }
    if (value == "cmake-cpp")
    {
        return ProjectType::CMakeCpp;
    }
    if (value == "node-web")
    {
        return ProjectType::NodeWeb;
    }
    if (value == "python")
    {
        return ProjectType::Python;
    }
    if (value == "dotnet")
    {
        return ProjectType::DotNet;
    }
    if (value == "java")
    {
        return ProjectType::Java;
    }
    if (value == "unreal")
    {
        return ProjectType::Unreal;
    }
    if (value == "custom")
    {
        return ProjectType::Custom;
    }
    return ProjectType::Unknown;
}

ProjectProfile DetectProjectProfile(const std::filesystem::path& root)
{
    if (root.empty())
    {
        throw std::invalid_argument("Project root cannot be empty.");
    }
    if (!Exists(root) || !std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Project root does not exist or is not a directory.");
    }

    auto profile = InferProjectProfile(root);
    const auto config_path = root / "agentguard.json";
    if (Exists(config_path))
    {
        ApplyAgentGuardConfig(profile, config_path);
        if (std::find(profile.project_files.begin(), profile.project_files.end(), "agentguard.json") ==
            profile.project_files.end())
        {
            profile.project_files.push_back("agentguard.json");
        }
    }
    return profile;
}

void to_json(nlohmann::json& json_value, const CommandStep& step)
{
    json_value = nlohmann::json{
        {"name", step.name},
        {"executable", step.executable},
        {"arguments", step.arguments}
    };
}

void from_json(const nlohmann::json& json_value, CommandStep& step)
{
    json_value.at("name").get_to(step.name);
    json_value.at("executable").get_to(step.executable);
    if (json_value.contains("arguments"))
    {
        json_value.at("arguments").get_to(step.arguments);
    }
}

void to_json(nlohmann::json& json_value, const ProjectProfile& profile)
{
    json_value = nlohmann::json{
        {"root", profile.root.string()},
        {"project_type", ProjectTypeToString(profile.project_type)},
        {"adapter", profile.adapter},
        {"project_files", profile.project_files},
        {"source_roots", profile.source_roots},
        {"test_roots", profile.test_roots},
        {"protected_files", profile.protected_files},
        {"verify_steps", profile.verify_steps}
    };
}
} // namespace agentguard
