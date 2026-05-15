#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "indexing/CppSymbolIndexer.h"
#include "indexing/DependencyGraph.h"
#include "indexing/SymbolIndex.h"

namespace
{
namespace fs = std::filesystem;

fs::path MakeFixtureRoot()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path root = fs::temp_directory_path() / ("agentguard_deps_" + std::to_string(stamp));
    fs::create_directories(root / "src");
    return root;
}

void WriteText(const fs::path& path, const std::string& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::trunc);
    output << text;
}

TEST(DependencyGraphTest, HeaderReverseIncludeFindsCppDependents)
{
    const fs::path root = MakeFixtureRoot();
    WriteText(root / "src" / "PublicApi.h", "class PublicApi { public: void FindBook(); };\n");
    WriteText(root / "src" / "Library.cpp", "#include \"PublicApi.h\"\nvoid Use() {}\n");
    WriteText(root / "src" / "Other.cpp", "#include \"src/PublicApi.h\"\n");

    const std::vector<agentguard::SourceFileInfo> files{
        agentguard::IndexCppSymbols(root / "src" / "PublicApi.h", root),
        agentguard::IndexCppSymbols(root / "src" / "Library.cpp", root),
        agentguard::IndexCppSymbols(root / "src" / "Other.cpp", root)
    };

    const auto graph = agentguard::BuildDependencyGraph(root, agentguard::ProjectInfo{}, files);
    const auto dependents = agentguard::FindReverseIncludeDependents(graph, "src/PublicApi.h");

    EXPECT_EQ(dependents, (std::vector<std::string>{"src/Library.cpp", "src/Other.cpp"}));
    fs::remove_all(root);
}

TEST(SymbolIndexTest, FindsDefinitionsReferencesMacrosAndCommandStrings)
{
    const fs::path root = MakeFixtureRoot();
    WriteText(
        root / "src" / "LibrarySystem.h",
        "#define LIBRARY_COMMAND \"SUMMON\"\nclass LibrarySystem { public: bool FindBook(); };\n");
    WriteText(
        root / "src" / "LibraryTests.cpp",
        "#include \"LibrarySystem.h\"\nvoid Test() { LibrarySystem library; library.FindBook(); }\n");

    const std::vector<agentguard::SourceFileInfo> files{
        agentguard::IndexCppSymbols(root / "src" / "LibrarySystem.h", root),
        agentguard::IndexCppSymbols(root / "src" / "LibraryTests.cpp", root)
    };

    const auto index = agentguard::BuildSymbolIndex(files);

    EXPECT_EQ(agentguard::FindSymbolFiles(index, "FindBook"),
        (std::vector<std::string>{"src/LibrarySystem.h", "src/LibraryTests.cpp"}));
    EXPECT_EQ(agentguard::FindSymbolFiles(index, "LIBRARY_COMMAND"),
        (std::vector<std::string>{"src/LibrarySystem.h"}));
    EXPECT_EQ(agentguard::FindSymbolFiles(index, "SUMMON"),
        (std::vector<std::string>{"src/LibrarySystem.h"}));
    fs::remove_all(root);
}

TEST(ImpactAnalysisTest, DetectsTestsAndPublicHeaderBlastRadius)
{
    const fs::path root = MakeFixtureRoot();
    WriteText(root / "include" / "LibrarySystem.h", "class LibrarySystem { public: bool FindBook(); };\n");
    WriteText(root / "src" / "LibrarySystem.cpp", "#include \"../include/LibrarySystem.h\"\n");
    WriteText(root / "tests" / "LibrarySystemTests.cpp", "#include \"../include/LibrarySystem.h\"\n");

    std::vector<agentguard::SourceFileInfo> files{
        agentguard::IndexCppSymbols(root / "include" / "LibrarySystem.h", root),
        agentguard::IndexCppSymbols(root / "src" / "LibrarySystem.cpp", root),
        agentguard::IndexCppSymbols(root / "tests" / "LibrarySystemTests.cpp", root)
    };
    const auto graph = agentguard::BuildDependencyGraph(root, agentguard::ProjectInfo{}, files);
    const auto index = agentguard::BuildSymbolIndex(files);

    const auto impact = agentguard::AnalyzeImpact(
        "Modify LibrarySystem FindBook in public header",
        files,
        graph,
        index);

    EXPECT_TRUE(impact.public_header_risk);
    EXPECT_EQ(impact.changed_header_blast_radius, "medium");
    EXPECT_EQ(impact.blast_radius, "medium");
    EXPECT_EQ(impact.likely_tests, (std::vector<std::string>{"tests/LibrarySystemTests.cpp"}));
    EXPECT_NE(
        std::find(
            impact.reverse_include_dependents.begin(),
            impact.reverse_include_dependents.end(),
            "src/LibrarySystem.cpp"),
        impact.reverse_include_dependents.end());
    fs::remove_all(root);
}
} // namespace
