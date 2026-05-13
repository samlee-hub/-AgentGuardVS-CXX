#include "vs/SlnParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>

#include "core/PathUtils.h"

namespace agentguard
{
namespace
{
bool EndsWithVcxproj(const std::string& path)
{
    std::string lowered = path;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    constexpr std::string_view extension = ".vcxproj";
    return lowered.size() >= extension.size() &&
           lowered.compare(lowered.size() - extension.size(), extension.size(), extension) == 0;
}
} // namespace

SlnParseResult ParseSln(const std::filesystem::path& sln_path)
{
    if (!std::filesystem::exists(sln_path) || !std::filesystem::is_regular_file(sln_path))
    {
        throw std::invalid_argument("Solution path must point to an existing .sln file.");
    }

    std::ifstream input(sln_path);
    if (!input)
    {
        throw std::runtime_error("Unable to open solution file.");
    }

    const std::regex project_pattern(
        R"SLN(^\s*Project\("([^"]+)"\)\s*=\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"\s*$)SLN");

    SlnParseResult result;
    result.solution_path = NormalizePath(sln_path);

    std::string line;
    while (std::getline(input, line))
    {
        std::smatch match;
        if (!std::regex_match(line, match, project_pattern))
        {
            continue;
        }

        const std::string project_type_guid = match[1].str();
        const std::string project_name = match[2].str();
        const std::string project_path = match[3].str();
        const std::string project_guid = match[4].str();

        if (!EndsWithVcxproj(project_path))
        {
            result.warnings.push_back("Ignored non-C++ solution entry: " + project_name);
            continue;
        }

        VSProjectInfo project;
        project.project_name = project_name;
        project.project_path = project_path;
        project.project_guid = project_guid;
        project.project_type_guid = project_type_guid;
        result.projects.push_back(std::move(project));
    }

    return result;
}
} // namespace agentguard
