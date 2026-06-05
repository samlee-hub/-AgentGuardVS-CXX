# AgentGuard Generalization Phase 1

Status: superseded by the vNext multi-project ProjectProfile upgrade. This document is retained as historical phase-1 context. Current behavior is broader than the original phase-1 scope: `analyze --project`, `verify --project`, `verify-profile`, `doctor`, source indexing, and audit reports now consume the same `ProjectProfile` and `SourceFilePolicy` model.

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

After this phase, AgentGuard could inspect a project root, classify it as `visualstudio-cxx`, `node`, `python`, `cmake-cpp`, `dotnet`, `java`, `unreal`, or `custom`, and execute explicit verification commands from `agentguard.json`.

The current implementation goes further: profile-based analyze, policy indexing, command verification, and audit reporting are now part of the core workflow while preserving the existing Visual Studio C++ path.
