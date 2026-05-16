# Roadmap

AgentGuardVS-CXX is focused first on Visual Studio C++ and MSBuild. The roadmap keeps that path stable before broader expansion.

## P0: Installation And Demo

Status: implemented.

- Source build path for Windows + Visual Studio 2022.
- Release package layout and installer scripts.
- `examples/library-system` demo with offline providers.
- Codex Skill/Plugin local installation path.

## P1: Benchmark And Reports

Status: first version implemented.

- Benchmark task JSONL format.
- Offline benchmark runner.
- Markdown and JSON reports with metrics and artifacts.
- LLM response cache with `--no-cache`.

## P2: Symbol Dependency Graph

Status: first version implemented.

- Lightweight symbol index for classes, structs, enums, functions, methods, includes, macros, and domain strings.
- Include dependency graph and reverse include lookup.
- Better public-header blast-radius reporting.
- Test file detection and risk scoring.

## P3: GitHub Action And CI

Status: first version implemented.

- Windows build workflow.
- Unit tests without real API calls.
- Library demo workflow using fake or file providers.
- Prototype `agentguard-review` action metadata.

## Later: Broader Integrations

Status: planned.

- MCP Server for richer tool integration.
- More provider adapters.
- Richer report formats and CI annotations.
- Additional IDE/platform support after the Visual Studio C++ workflow remains stable.

## Completed Technical Foundations

- Git baseline and diff isolation.
- Workspace repos exclude the source `.git`.
- Each workspace initializes an independent baseline commit.
- DiffReporter rejects parent Git top-levels.
- Reports record `git_top_level` and `diff_base`.

These items are not commitments to fully autonomous software engineering. They are incremental reliability improvements around agent-assisted development.
