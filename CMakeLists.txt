cmake_minimum_required(VERSION 3.25)

project(AgentGuardVS_CXX VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)
find_path(CLI11_INCLUDE_DIRS "CLI/CLI.hpp" REQUIRED)

add_library(AgentGuardCore
    src/core/Models.cpp
    src/core/PathUtils.cpp
    src/core/ProcessRunner.cpp
    src/core/SemanticModels.cpp
    src/core/Workspace.cpp
    src/vs/SlnParser.cpp
    src/vs/VcxprojParser.cpp
    src/vs/MSBuildLocator.cpp
    src/vs/MSBuildRunner.cpp
    src/indexing/FileIndexer.cpp
    src/indexing/CppSymbolIndexer.cpp
    src/indexing/SymbolIndex.cpp
    src/indexing/DependencyGraph.cpp
    src/indexing/ContextSelector.cpp
    src/diagnostics/ErrorClassifier.cpp
    src/diagnostics/ErrorParser.cpp
    src/patching/PatchPlan.cpp
    src/patching/PatchApplier.cpp
    src/patching/DiffReporter.cpp
    src/project/CommandVerifier.cpp
    src/project/ProjectProfile.cpp
    src/security/SecurityPolicy.cpp
    src/llm/ClaudeProvider.cpp
    src/llm/CachedLLMProvider.cpp
    src/llm/DeepSeekProvider.cpp
    src/llm/FakeLLMProvider.cpp
    src/llm/FileLLMProvider.cpp
    src/llm/HttpJsonClient.cpp
    src/llm/LLMProviderConfig.cpp
    src/llm/OpenAIProvider.cpp
    src/agents/PlannerAgent.cpp
    src/agents/SemanticAnalyzerAgent.cpp
    src/agents/ImplementerAgent.cpp
    src/agents/BuildFixerAgent.cpp
    src/agents/ReviewerAgent.cpp
    src/agents/RepairLoop.cpp
    src/report/ReportWriter.cpp
    src/evaluation/BenchmarkTask.cpp
    src/evaluation/Metrics.cpp
)

target_include_directories(AgentGuardCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(AgentGuardCore
    PUBLIC
        nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(AgentGuardCore
        PRIVATE
            winhttp
    )
endif()

add_executable(AgentGuardVS
    src/main.cpp
)

target_link_libraries(AgentGuardVS
    PRIVATE
        AgentGuardCore
)

target_include_directories(AgentGuardVS
    PRIVATE
        ${CLI11_INCLUDE_DIRS}
)

enable_testing()
add_subdirectory(tests)
