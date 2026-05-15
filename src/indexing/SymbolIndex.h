#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "core/Models.h"

namespace agentguard
{
struct SymbolIndexEntry
{
    std::string name;
    std::string kind;
    std::string file_path;
};

struct SymbolIndex
{
    std::vector<SymbolIndexEntry> entries;
    std::unordered_map<std::string, std::vector<std::string>> files_by_symbol;
};

SymbolIndex BuildSymbolIndex(const std::vector<SourceFileInfo>& files);

std::vector<std::string> FindSymbolFiles(
    const SymbolIndex& index,
    const std::string& symbol_name);
} // namespace agentguard
