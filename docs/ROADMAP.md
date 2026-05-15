# Roadmap

AgentGuardVS-CXX is focused first on Visual Studio C++ and MSBuild. The roadmap keeps that path stable before broader expansion.

## P0: Git Baseline And Diff Isolation

Status: implemented.

- Workspace repos exclude the source `.git`.
- Each workspace initializes an independent baseline commit.
- DiffReporter rejects parent Git top-levels.
- Reports record `git_top_level` and `diff_base`.

## P1: Skill Installation

Status: implemented.

- Repository-level `agentguard-vs-cxx` Skill.
- Wrapper scripts for analyze, verify, review, and demo full-cycle execution.
- Local install script for Codex Skill directories.

## P2: Symbol And Dependency Graph

Status: first version implemented.

- Lightweight symbol index for classes, structs, enums, functions, methods, includes, macros, and domain strings.
- Include dependency graph and reverse include lookup.
- Better public-header blast-radius reporting.
- Test file detection and risk scoring.

## P3: GitHub Action Prototype

Status: first version implemented.

- Windows build workflow.
- Unit tests without real API calls.
- Library demo workflow using fake or file providers.
- Prototype `agentguard-review` action metadata.

## P4: Broader Integrations

Status: planned.

- Additional language and IDE support after the Visual Studio C++ workflow is reliable.
- More provider adapters.
- Richer report formats and CI annotations.

These items are not commitments to fully autonomous software engineering. They are incremental reliability improvements around agent-assisted development.
