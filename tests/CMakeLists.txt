find_package(GTest CONFIG REQUIRED)

add_executable(test_models
    test_models.cpp
)

target_link_libraries(test_models
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_models)

add_executable(test_workspace
    test_workspace.cpp
)

target_link_libraries(test_workspace
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_workspace)

add_executable(test_sln_parser
    test_sln_parser.cpp
)

target_link_libraries(test_sln_parser
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_sln_parser)

add_executable(test_vcxproj_parser
    test_vcxproj_parser.cpp
)

target_link_libraries(test_vcxproj_parser
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_vcxproj_parser)

add_executable(test_file_indexer
    test_file_indexer.cpp
)

target_link_libraries(test_file_indexer
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_file_indexer)

add_executable(test_cpp_symbol_indexer
    test_cpp_symbol_indexer.cpp
)

target_link_libraries(test_cpp_symbol_indexer
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_cpp_symbol_indexer)

add_executable(test_context_selector
    test_context_selector.cpp
)

target_link_libraries(test_context_selector
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_context_selector)

add_executable(test_process_runner
    test_process_runner.cpp
)

target_link_libraries(test_process_runner
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_process_runner)

add_executable(test_msbuild_locator
    test_msbuild_locator.cpp
)

target_link_libraries(test_msbuild_locator
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_msbuild_locator)

add_executable(test_msbuild_runner
    test_msbuild_runner.cpp
)

target_link_libraries(test_msbuild_runner
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_msbuild_runner)

add_executable(test_error_parser
    test_error_parser.cpp
)

target_link_libraries(test_error_parser
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_error_parser)

add_executable(test_patch_applier
    test_patch_applier.cpp
)

target_link_libraries(test_patch_applier
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_patch_applier)

add_executable(test_report_writer
    test_report_writer.cpp
)

target_link_libraries(test_report_writer
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_report_writer)

add_executable(test_llm_provider
    test_llm_provider.cpp
)

target_link_libraries(test_llm_provider
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_llm_provider)

add_executable(test_planner_agent
    test_planner_agent.cpp
)

target_link_libraries(test_planner_agent
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_planner_agent)

add_executable(test_implementer_agent
    test_implementer_agent.cpp
)

target_link_libraries(test_implementer_agent
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_implementer_agent)

add_executable(test_build_fixer_agent
    test_build_fixer_agent.cpp
)

target_link_libraries(test_build_fixer_agent
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_build_fixer_agent)

add_executable(test_repair_loop
    test_repair_loop.cpp
)

target_link_libraries(test_repair_loop
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_repair_loop)

add_executable(test_cli_run
    test_cli_run.cpp
)

target_compile_definitions(test_cli_run
    PRIVATE
        AGENTGUARD_EXE="$<TARGET_FILE:AgentGuardVS>"
        MINI_VS_PROJECT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/mini_vs_project"
)

target_link_libraries(test_cli_run
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_cli_run)

add_executable(test_evaluation
    test_evaluation.cpp
)

target_link_libraries(test_evaluation
    PRIVATE
        AgentGuardCore
        GTest::gtest
        GTest::gtest_main
)

gtest_discover_tests(test_evaluation)
