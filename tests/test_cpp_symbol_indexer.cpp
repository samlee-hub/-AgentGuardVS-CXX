#include <filesystem>

#include <gtest/gtest.h>

#include "indexing/CppSymbolIndexer.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::IndexCppSymbols;

TEST(CppSymbolIndexerTest, ExtractsIncludesTypesEnumsAndFunctions)
{
    const fs::path fixture_path =
        fs::path(__FILE__).parent_path() /
        "fixtures" /
        "mini_vs_project" /
        "IndexingSample" /
        "Game" /
        "Summon.cpp";

    const auto info = IndexCppSymbols(
        fixture_path,
        fs::path(__FILE__).parent_path() /
            "fixtures" /
            "mini_vs_project" /
            "IndexingSample");

    EXPECT_EQ(info.file_path, "Game\\Summon.cpp");
    EXPECT_EQ(info.includes, (std::vector<std::string>{"Summon.h", "<vector>"}));
    EXPECT_EQ(info.classes, (std::vector<std::string>{"SummonController"}));
    EXPECT_EQ(info.structs, (std::vector<std::string>{"SummonCost"}));
    EXPECT_EQ(info.enums, (std::vector<std::string>{"SummonState"}));
    EXPECT_EQ(info.functions, (std::vector<std::string>{"CanSummon", "ResetCooldown"}));
}
} // namespace
