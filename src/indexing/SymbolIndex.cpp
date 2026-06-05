#include "indexing/SymbolIndex.h"

#include <algorithm>
#include <filesystem>

namespace agentguard
{
namespace
{
std::string NormalizePath(const std::string& path)
{
    return std::filesystem::path(path).generic_string();
}

void AddUnique(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

void AddEntries(
    SymbolIndex& index,
    const SourceFileInfo& file,
    const std::string& kind,
    const std::vector<std::string>& names)
{
    const std::string normalized_path = NormalizePath(file.file_path);
    for (const auto& name : names)
    {
        if (name.empty())
        {
            continue;
        }
        index.entries.push_back(SymbolIndexEntry{name, kind, normalized_path});
        AddUnique(index.files_by_symbol[name], normalized_path);
    }
}

void AddMethodOwners(SymbolIndex& index, const SourceFileInfo& file)
{
    const std::string normalized_path = NormalizePath(file.file_path);
    for (const auto& method : file.methods)
    {
        const auto separator = method.find("::");
        if (separator == std::string::npos || separator == 0)
        {
            continue;
        }
        const std::string owner = method.substr(0, separator);
        index.entries.push_back(SymbolIndexEntry{owner, "method_owner", normalized_path});
        AddUnique(index.files_by_symbol[owner], normalized_path);
    }
}
} // namespace

SymbolIndex BuildSymbolIndex(const std::vector<SourceFileInfo>& files)
{
    SymbolIndex index;
    for (const auto& file : files)
    {
        AddEntries(index, file, "class", file.classes);
        AddEntries(index, file, "struct", file.structs);
        AddEntries(index, file, "enum", file.enums);
        AddEntries(index, file, "function", file.functions);
        AddEntries(index, file, "method", file.methods);
        AddMethodOwners(index, file);
        AddEntries(index, file, "member", file.member_variables);
        AddEntries(index, file, "macro", file.macros);
        AddEntries(index, file, "command_string", file.command_strings);
        AddEntries(index, file, "reference", file.symbol_references);
    }

    for (auto& [_, paths] : index.files_by_symbol)
    {
        std::sort(paths.begin(), paths.end());
        paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    }
    std::sort(index.entries.begin(), index.entries.end(), [](const auto& left, const auto& right) {
        if (left.name != right.name)
        {
            return left.name < right.name;
        }
        if (left.file_path != right.file_path)
        {
            return left.file_path < right.file_path;
        }
        return left.kind < right.kind;
    });
    return index;
}

std::vector<std::string> FindSymbolFiles(
    const SymbolIndex& index,
    const std::string& symbol_name)
{
    const auto it = index.files_by_symbol.find(symbol_name);
    if (it == index.files_by_symbol.end())
    {
        return {};
    }
    return it->second;
}
} // namespace agentguard
