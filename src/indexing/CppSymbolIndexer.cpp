#include "indexing/CppSymbolIndexer.h"

#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>

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
    AppendMatches(
        info.functions,
        content,
        std::regex(R"CPP((?:^|[\r\n])\s*[A-Za-z_:<>~*&\s]+\s+([A-Za-z_]\w*)\s*\([^;{}]*\)\s*(?:const\s*)?\{)CPP"),
        1);

    return info;
}
} // namespace agentguard
