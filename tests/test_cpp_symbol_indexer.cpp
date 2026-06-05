#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "indexing/CppSymbolIndexer.h"

namespace
{
namespace fs = std::filesystem;
using agentguard::IndexCppSymbols;

void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

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

TEST(CppSymbolIndexerTest, ExtractsMemberVariableCandidates)
{
    const fs::path root = fs::temp_directory_path() / "agentguard_member_variables";
    const fs::path fixture_path = root / "LibrarySystem.h";
    WriteText(
        fixture_path,
        "class LibrarySystem {\nprivate:\n    std::vector<Book> books_;\n    int borrow_limit_ = 3;\n};\n");

    const auto info = IndexCppSymbols(fixture_path, root);

    EXPECT_NE(
        std::find(info.member_variables.begin(), info.member_variables.end(), "books_"),
        info.member_variables.end());
    EXPECT_NE(
        std::find(info.member_variables.begin(), info.member_variables.end(), "borrow_limit_"),
        info.member_variables.end());
    fs::remove_all(root);
}

TEST(CppSymbolIndexerTest, DoesNotTreatControlStatementsAsFunctions)
{
    const fs::path root = fs::temp_directory_path() / "agentguard_control_functions";
    const fs::path fixture_path = root / "Control.cpp";
    WriteText(
        fixture_path,
        "bool Check(int value)\n{\n    if (value > 0) {\n        return true;\n    }\n    return false;\n}\n");

    const auto info = IndexCppSymbols(fixture_path, root);

    EXPECT_EQ(info.functions, (std::vector<std::string>{"Check"}));
    fs::remove_all(root);
}
} // namespace
