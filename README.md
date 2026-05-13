# AgentGuardVS-CXX

AgentGuardVS-CXX is a C++20 reliability verification and automated repair platform for AI coding agents working on real Visual Studio C++ solutions.

## One-Sentence Positioning

AI proposes code changes; AgentGuardVS-CXX constrains, validates, audits, and repairs those changes through workspace isolation, structured patch plans, build verification, diagnostics, and reporting.

## Typical Use Cases

- Analyze a Visual Studio C++ solution before any modification is applied.
- Generate an auditable task specification and bounded modification scope.
- Apply only validated patch plans inside an isolated `runs/<task_id>/repo` workspace.
- Verify generated changes with Visual Studio-oriented build workflows.
- Parse compiler, linker, and MSBuild failures into actionable repair context.
- Produce reports and diff artifacts for review, experiments, and benchmarking.

## MVP Scope

The initial implementation focuses on:

1. Project documentation and engineering constraints.
2. C++20 repository skeleton and test structure.
3. Visual Studio solution and project inspection.
4. Source indexing and task context selection.
5. Build verification primitives, diagnostics, and report generation.
6. Agent orchestration abstractions based on structured `TaskSpec` and `PatchPlan` data.

The MVP does not attempt to recreate Codex, Claude Code, Cursor, or a general-purpose coding chat interface.

## Development Environment

- Windows 11
- Visual Studio 2022-compatible project workflow
- CMake-based project generation
- MSBuild integration in later development steps
- Git
- vcpkg
- PowerShell
- Codex CLI or an equivalent external AI coding workflow

## Core Language and Dependency Policy

- The core platform is implemented in **C++20**.
- Python is reserved for later benchmark aggregation, statistics, and result export only.
- Early project dependencies are limited to:
  - `nlohmann/json`
  - `CLI11`
  - `spdlog`
  - `GoogleTest`

## Non-Negotiable Safety Rule

All real file modifications must occur only inside an isolated `runs/<task_id>/repo` workspace. The original user Visual Studio project must never be edited directly by the platform.

## Benchmark Scope

`benchmarks/tasks.jsonl` defines a vertical Agent task set for Visual Studio C++ server projects. It is not a general SWE-bench replacement; it focuses on `.sln` / `.vcxproj` projects, MSBuild validation, C++ compile and link failures, workspace isolation, and reportable repair loops.

Python scripts under `scripts/` are auxiliary benchmark tools only. They may read `report.json` and `metrics.json` files from `runs/` to produce summaries, but they must not participate in the core C++ execution, patch application, build validation, or repair loop.

## CLI Usage

Dry-run analysis without calling a real LLM or MSBuild:

```powershell
AgentGuardVS.exe run `
  --solution "C:\path\to\Server.sln" `
  --task "Add summon resource cost and a global 3 second cooldown." `
  --configuration Debug `
  --platform x64 `
  --runs-root runs `
  --dry-run `
  --no-llm
```

In `--dry-run` mode the platform creates `runs/<task_id>/repo`, parses the solution and project files, indexes C++ source files, selects relevant context, writes a `TaskSpec`, and emits reports without applying any patch or invoking MSBuild.

Non-dry-run mode enters the repair loop. At this stage `--no-llm` uses deterministic fake providers, so real implementation patches should still be supplied through later provider work rather than direct file writes.
