# Architecture

## System View

AgentGuardVS-CXX is organized as a reliability pipeline:

1. Inspect the target Visual Studio solution.
2. Build a task-aware project summary.
3. Ask an external agent for structured guidance.
4. Validate and apply only approved patch operations.
5. Build, diagnose, report, and optionally repair through bounded iterations.

All file changes must remain inside `runs/<task_id>/repo`.

## Module Boundaries

### `core`

Shared data models, task metadata, path helpers, workspace management, and process execution utilities.

### `vs`

Visual Studio specific parsing and build-related components:

- `.sln` inspection
- `.vcxproj` inspection
- MSBuild discovery
- MSBuild invocation wrappers

### `indexing`

Codebase scanning and lightweight context extraction:

- Source file enumeration
- C++ symbol hints
- Task-relevant file selection

### `diagnostics`

Failure interpretation and risk classification:

- Compiler, linker, and MSBuild log parsing
- Error category mapping
- Risk analysis for the current task or patch outcome

### `patching`

Controlled code modification infrastructure:

- Structured `PatchPlan`
- Safe `PatchApplier`
- Git diff extraction and patch reporting

### `llm`

Provider abstraction for external AI output:

- Interfaces for text or JSON generation
- Fake and file-based providers for deterministic workflows
- Optional later network-backed providers

The provider may propose changes, but it may not write files directly.

### `agents`

Workflow coordinators that turn platform context into agent interactions:

- Planning
- Implementation plan generation
- Build-fix follow-up
- Review-oriented checks

### `report`

Human- and machine-readable output generation:

- Markdown reports
- JSON reports
- Build summaries
- Risk and audit details

### `evaluation`

Benchmark-oriented data structures and metrics helpers for vertical Visual Studio C++ reliability evaluation.

This is not a general SWE-bench clone. The benchmark scope is intentionally narrower: tasks are designed around Visual Studio C++ server projects, `.sln` / `.vcxproj` workflows, MSBuild validation, C++ compiler and linker diagnostics, isolated workspace repair loops, and audit-friendly reports. The goal is to measure whether an agent-control layer can safely plan, constrain, validate, and report changes in this specific engineering environment.

Initial benchmark tasks live in `benchmarks/tasks.jsonl`. Each task records the expected files, forbidden files, acceptance criteria, difficulty, and tags needed for later batch evaluation.

## Planned Runtime Flow

1. User provides a solution path and task request.
2. Workspace isolation copies the project into `runs/<task_id>/repo`.
3. Visual Studio parsers extract solution and project information.
4. Indexers summarize available code context.
5. A planner produces `TaskSpec`.
6. An implementer produces `PatchPlan`.
7. `PatchApplier` validates and applies changes in the isolated workspace.
8. Build execution and diagnostics classify any failures.
9. Repair agents may propose additional bounded `PatchPlan` rounds.
10. Reports and diff artifacts are emitted for audit.

## Design Principle

The architecture separates proposal from execution:

- Agent components propose.
- Platform components validate.
- PatchApplier mutates files.
- Diagnostics and reports make outcomes inspectable.
