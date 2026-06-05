#include "vs/VcxprojParser.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace agentguard
{
namespace
{
std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw std::runtime_error("Unable to open vcxproj file.");
    }

    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

void AppendUnique(std::vector<std::string>& values, const std::string& value)
{
    if (value.empty())
    {
        return;
    }

    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

std::vector<std::string> SplitSemicolonList(const std::string& text)
{
    std::vector<std::string> values;
    std::string current;

    for (const char ch : text)
    {
        if (ch == ';')
        {
            if (!current.empty() && current.rfind("%(", 0) != 0)
            {
                values.push_back(current);
            }
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty() && current.rfind("%(", 0) != 0)
    {
        values.push_back(current);
    }

    return values;
}
} // namespace

VSProjectInfo ParseVcxproj(const std::filesystem::path& vcxproj_path)
{
    if (!std::filesystem::exists(vcxproj_path) || !std::filesystem::is_regular_file(vcxproj_path))
    {
        throw std::invalid_argument("vcxproj path must point to an existing file.");
    }

    const std::string xml = ReadAllText(vcxproj_path);

    VSProjectInfo project;
    project.project_name = vcxproj_path.stem().string();
    project.project_path = vcxproj_path.string();

    const std::regex project_configuration_pattern(
        R"VCX(<(?:\w+:)?ProjectConfiguration\b[^>]*\bInclude="([^"]+)"[^>]*>)VCX",
        std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), project_configuration_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        const std::string include_value = (*it)[1].str();
        const auto separator = include_value.find('|');
        if (separator == std::string::npos)
        {
            continue;
        }

        AppendUnique(project.configurations, include_value.substr(0, separator));
        AppendUnique(project.platforms, include_value.substr(separator + 1));
    }

    const std::regex compile_pattern(
        R"VCX(<(?:\w+:)?ClCompile\b[^>]*\bInclude="([^"]+)"[^>]*/?>)VCX",
        std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), compile_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        AppendUnique(project.source_files, (*it)[1].str());
    }

    const std::regex include_pattern(
        R"VCX(<(?:\w+:)?ClInclude\b[^>]*\bInclude="([^"]+)"[^>]*/?>)VCX",
        std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), include_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        AppendUnique(project.header_files, (*it)[1].str());
    }

    const std::regex include_dirs_pattern(
        R"VCX(<(?:\w+:)?AdditionalIncludeDirectories\b[^>]*>([\s\S]*?)</(?:\w+:)?AdditionalIncludeDirectories>)VCX",
        std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), include_dirs_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        for (const auto& value : SplitSemicolonList((*it)[1].str()))
        {
            AppendUnique(project.include_directories, value);
        }
    }

    const std::regex definitions_pattern(
        R"VCX(<(?:\w+:)?PreprocessorDefinitions\b[^>]*>([\s\S]*?)</(?:\w+:)?PreprocessorDefinitions>)VCX",
        std::regex::icase);
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), definitions_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        for (const auto& value : SplitSemicolonList((*it)[1].str()))
        {
            AppendUnique(project.preprocessor_definitions, value);
        }
    }

    return project;
}
} // namespace agentguard
