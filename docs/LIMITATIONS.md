# Limitations

AgentGuardVS-CXX is intentionally scoped. It is a reliability control layer for AI-assisted changes to Visual Studio C++ projects, not a general autonomous programming system.

## Current Primary Support

- Windows development machines.
- Visual Studio 2022 or Visual Studio Build Tools 2022.
- C++ projects represented by `.sln` and `.vcxproj` files.
- MSBuild verification.
- Codex Skill/Plugin workflows that can call local PowerShell scripts.

## LLM Limits

LLM-assisted semantic scope analysis and semantic review can be incomplete or wrong. The model may miss indirect dependencies, overstate risk, understate risk, or return invalid JSON. Human review remains necessary, especially for public headers, security-sensitive code, and large repositories.

## Build And Test Limits

MSBuild success does not guarantee business logic correctness. AgentGuardVS-CXX records build results and parsed diagnostics, but project-specific test suites and manual review are still required.

## Scale Limits

Current validation is centered on small Visual Studio C++ projects and controlled examples. Enterprise-scale repositories may need custom policy tuning, broader test commands, larger context selection improvements, and human approval workflows.

## Static Analysis Limits

The symbol and dependency analysis is lightweight. It is not a complete C++ compiler, clang AST, linker model, or whole-program semantic analyzer.

## Platform Limits

Other languages, IDEs, build systems, and operating systems are future expansion areas. They are not current guarantees.
