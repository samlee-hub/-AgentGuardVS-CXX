# AgentGuard Generalization Phase 1

## Direction

Phase 1 keeps all existing Visual Studio C++ behavior intact and adds a generic project layer next to it. The current `.sln`, `.vcxproj`, MSBuild, analyze, verify, review, and Skill workflows remain supported.

The new layer introduces project detection and command-profile verification so AgentGuard can start supporting non-Visual-Studio projects without rewriting the existing workflow.

## Scope

- Add a generic project profile model.
- Detect common project types from project-root markers.
- Read optional `agentguard.json` files for user-defined verification commands.
- Add a command-profile verifier that runs configured commands and records structured results.
- Add CLI commands for project detection and profile verification.
- Keep `run`, `analyze`, `verify`, `review`, and `llm-smoke` behavior compatible.

## Out Of Scope

- Replacing MSBuild verification.
- Changing semantic scope prompts for all languages.
- Adding full Node, Python, Java, .NET, or Unreal semantic analyzers.
- Modifying external test projects.

## Resulting Capability

After this phase, AgentGuard can inspect a project root, classify it as `visualstudio-cxx`, `node-web`, `python`, `cmake-cpp`, `dotnet`, `java`, `unreal`, or `custom`, and execute explicit verification commands from `agentguard.json`.

This makes the platform usable for front-end and back-end repositories through command profiles while preserving the existing Visual Studio C++ path.
