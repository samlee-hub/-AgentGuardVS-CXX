# Limitations

AgentGuardVS-CXX is intentionally scoped. It is a reliability control layer for AI-assisted changes across profiled projects, not a general autonomous programming system.

## Current Primary Support

- Windows development machines.
- Visual Studio 2022 or Visual Studio Build Tools 2022 when building this tool and when using the mature Visual Studio C++ path.
- Visual Studio C++ projects represented by `.sln` and `.vcxproj` files.
- Profile-based CMake C++, Python, Node, .NET, Java, Unreal, and custom project detection, indexing, command verification, and audit reporting.
- MSBuild verification for legacy workspaces and configured command verification for project profiles.
- Codex Skill/Plugin workflows that can call local PowerShell scripts.

## LLM Limits

LLM-assisted semantic scope analysis and semantic review can be incomplete or wrong. The model may miss indirect dependencies, overstate risk, understate risk, or return invalid JSON. Human review remains necessary, especially for public headers, security-sensitive code, and large repositories.

## Build And Test Limits

Build or command verification success does not guarantee business logic correctness. AgentGuardVS-CXX records build results, command results, and parsed diagnostics where available, but project-specific test suites and manual review are still required.

## Scale Limits

Current validation includes small Visual Studio C++ projects plus focused profile/index/command verification coverage for CMake C++, Python, Node, .NET, Java, Unreal, and custom profiles. Enterprise-scale repositories may need custom policy tuning, broader test commands, larger context selection improvements, and human approval workflows.

## Static Analysis Limits

The symbol and dependency analysis is lightweight. It is not a complete C++ compiler, clang AST, Python parser, TypeScript compiler, Java compiler, linker model, or whole-program semantic analyzer.

## Platform Limits

The most mature end-to-end repair loop remains Visual Studio C++. Other languages rely on `ProjectProfile`, `SourceFilePolicy`, and project-specific `verify_commands`.
