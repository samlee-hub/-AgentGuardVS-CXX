#include "indexing/CppSymbolIndexer.h"

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
        throw std::runtime_error("Unable to open C++ source file.");
    }

    return std::string(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

bool IsControlIdentifier(const std::string& value);

void AppendMatches(
    std::vector<std::string>& output,
    const std::string& content,
    const std::regex& pattern,
    const std::size_t group_index)
{
    for (auto it = std::sregex_iterator(content.begin(), content.end(), pattern);
         it != std::sregex_iterator();
         ++it)
    {
        output.push_back((*it)[group_index].str());
    }
}

void AppendIdentifierMatches(
    std::vector<std::string>& output,
    const std::string& content,
    const std::regex& pattern,
    const std::size_t group_index)
{
    for (auto it = std::sregex_iterator(content.begin(), content.end(), pattern);
         it != std::sregex_iterator();
         ++it)
    {
        const std::string value = (*it)[group_index].str();
        if (!IsControlIdentifier(value))
        {
            output.push_back(value);
        }
    }
}

void AppendUnique(std::vector<std::string>& output, const std::string& value)
{
    if (value.empty())
    {
        return;
    }
    if (std::find(output.begin(), output.end(), value) == output.end())
    {
        output.push_back(value);
    }
}

bool IsControlIdentifier(const std::string& value)
{
    static const std::unordered_set<std::string> controls{
        "if", "for", "while", "switch", "return", "sizeof", "catch"
    };
    return controls.contains(value);
}

bool LooksLikeCommandString(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }
    bool has_upper = false;
    for (const char ch : value)
    {
        if (ch >= 'a' && ch <= 'z')
        {
            return false;
        }
        if (ch >= 'A' && ch <= 'Z')
        {
            has_upper = true;
        }
    }
    return has_upper;
}
} // namespace

SourceFileInfo IndexCppSymbols(
    const std::filesystem::path& file_path,
    const std::filesystem::path& root)
{
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path))
    {
        throw std::invalid_argument("Symbol indexer input must be an existing source file.");
    }

    const std::string content = ReadAllText(file_path);

    SourceFileInfo info;
    if (!root.empty())
    {
        info.file_path = std::filesystem::relative(file_path, root).string();
    }
    else
    {
        info.file_path = file_path.filename().string();
    }

    const std::regex include_pattern(
        R"CPP((?:^|[\r\n])\s*#\s*include\s*([<"][^>"]+[>"]))CPP");
    for (auto it = std::sregex_iterator(content.begin(), content.end(), include_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        std::string include_value = (*it)[1].str();
        if (!include_value.empty() && include_value.front() == '"' && include_value.back() == '"')
        {
            include_value = include_value.substr(1, include_value.size() - 2);
        }
        info.includes.push_back(include_value);
    }

    AppendMatches(
        info.classes,
        content,
        std::regex(R"CPP((?:^|[\r\n])\s*class\s+([A-Za-z_]\w*))CPP"),
        1);
    AppendMatches(info.structs, content, std::regex(R"CPP(\bstruct\s+([A-Za-z_]\w*))CPP"), 1);
    AppendMatches(info.enums, content, std::regex(R"CPP(\benum(?:\s+class)?\s+([A-Za-z_]\w*))CPP"), 1);
    AppendIdentifierMatches(
        info.functions,
        content,
        std::regex(R"CPP((?:^|[\r\n])\s*[A-Za-z_:<>~*&\s]+\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?\{)CPP"),
        1);

    AppendMatches(
        info.methods,
        content,
        std::regex(R"CPP(\b([A-Za-z_]\w*::[A-Za-z_]\w*)\s*\()CPP"),
        1);
    AppendMatches(
        info.member_variables,
        content,
        std::regex(
            R"CPP((?:^|[\r\n])\s*(?:static\s+)?(?:const\s+)?(?:std::[A-Za-z_]\w*(?:<[^;\r\n]+>)?|[A-Za-z_]\w*(?:::[A-Za-z_]\w*)?(?:<[^;\r\n]+>)?)[*&\s]+([A-Za-z_]\w*)\s*(?:=[^;\r\n]*)?;)CPP"),
        1);
    AppendMatches(
        info.macros,
        content,
        std::regex(R"CPP((?:^|[\r\n])\s*#\s*define\s+([A-Za-z_]\w*))CPP"),
        1);

    const std::regex string_pattern(R"CPP("([^"\r\n]{1,128})")CPP");
    for (auto it = std::sregex_iterator(content.begin(), content.end(), string_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        const std::string value = (*it)[1].str();
        if (LooksLikeCommandString(value))
        {
            AppendUnique(info.command_strings, value);
        }
    }

    const std::regex reference_pattern(R"CPP(\b([A-Za-z_]\w*)\s*\()CPP");
    for (auto it = std::sregex_iterator(content.begin(), content.end(), reference_pattern);
         it != std::sregex_iterator();
         ++it)
    {
        const std::string value = (*it)[1].str();
        if (!IsControlIdentifier(value))
        {
            AppendUnique(info.symbol_references, value);
        }
    }

    for (const auto& value : info.macros)
    {
        AppendUnique(info.keywords, value);
    }
    for (const auto& value : info.command_strings)
    {
        AppendUnique(info.keywords, value);
    }
    for (const auto& value : info.symbol_references)
    {
        AppendUnique(info.keywords, value);
    }
    for (const auto& value : info.member_variables)
    {
        AppendUnique(info.keywords, value);
    }

    return info;
}
} // namespace agentguard
