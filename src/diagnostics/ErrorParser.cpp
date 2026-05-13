#include "diagnostics/ErrorParser.h"

#include <regex>
#include <sstream>

#include "diagnostics/ErrorClassifier.h"

namespace agentguard
{
namespace
{
ParsedBuildError ParseLocatedError(const std::smatch& match, const std::string& raw_line)
{
    ParsedBuildError error;
    error.file = match[1].str();
    error.line = std::stoi(match[2].str());
    error.column = match[3].matched ? std::stoi(match[3].str()) : 0;
    error.code = match[4].str();
    error.message = match[5].str();
    error.category = ClassifyErrorCode(error.code);
    error.raw_line = raw_line;
    return error;
}

ParsedBuildError ParseUnlocatedError(const std::smatch& match, const std::string& raw_line)
{
    ParsedBuildError error;
    error.file = match[1].str();
    error.code = match[2].str();
    error.message = match[3].str();
    error.category = ClassifyErrorCode(error.code);
    error.raw_line = raw_line;
    return error;
}
} // namespace

std::vector<ParsedBuildError> ParseBuildErrors(const std::string& build_log)
{
    const std::regex located_pattern(
        R"(^(.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(?:fatal\s+)?error\s+([A-Z]+\d+)\s*:\s*(.*)$)");
    const std::regex unlocated_pattern(
        R"(^(.+?)\s*:\s*(?:fatal\s+)?error\s+([A-Z]+\d+)\s*:\s*(.*)$)");

    std::vector<ParsedBuildError> errors;
    std::istringstream lines(build_log);
    std::string line;
    while (std::getline(lines, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, located_pattern))
        {
            errors.push_back(ParseLocatedError(match, line));
            continue;
        }

        if (std::regex_match(line, match, unlocated_pattern))
        {
            errors.push_back(ParseUnlocatedError(match, line));
        }
    }

    return errors;
}

std::vector<ParsedBuildError> ParseBuildErrors(const BuildResult& build_result)
{
    return ParseBuildErrors(build_result.stdout_text + "\n" + build_result.stderr_text);
}
} // namespace agentguard
