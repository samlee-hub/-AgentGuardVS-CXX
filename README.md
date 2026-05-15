# AgentGuardVS-CXX

[![Windows Build](https://github.com/<owner>/AgentGuardVS-CXX/actions/workflows/windows-build.yml/badge.svg)](https://github.com/<owner>/AgentGuardVS-CXX/actions/workflows/windows-build.yml)
[![Library Demo](https://github.com/<owner>/AgentGuardVS-CXX/actions/workflows/library-demo.yml/badge.svg)](https://github.com/<owner>/AgentGuardVS-CXX/actions/workflows/library-demo.yml)

AgentGuardVS-CXX is a control layer for AI coding agents working on Visual Studio C++ projects: it analyzes impact scope, isolates edits, runs MSBuild verification, performs LLM-assisted semantic review, and writes audit reports.

## What Problem It Solves

AI coding agents can move quickly inside large C++ repositories, but Visual Studio projects often have fragile build settings, public headers, indirect include dependencies, and linked source files spread across `.sln` and `.vcxproj` files. A small request can accidentally touch protected files, miss related tests, or enter a repair loop after a build failure.

AgentGuardVS-CXX adds a controlled workflow around those agents:

- Create an isolated workspace before edits happen.
- Ask an LLM to classify semantic impact scope.
- Allow direct edits only in `allowed_files`.
- Keep `context_files` read-only.
- Require explicit user approval for `needs_approval_files`.
- Forbid edits to `protected_files`.
- Verify changes with MSBuild.
- Review the final diff and build result before accepting the task.
- Produce JSON and Markdown reports for audit and later debugging.

## What It Is Not

AgentGuardVS-CXX is not an IDE, a replacement for Codex, Claude Code, Cursor, MSBuild, or your test framework. It does not guarantee business correctness, and it should not be treated as a fully autonomous engineer. It is a reliability and audit layer for Visual Studio C++ agent workflows.

## Core Workflow

```text
user task
  -> analyze
  -> semantic_scope.json
  -> edit allowed_files in isolated workspace
  -> verify with MSBuild
  -> review semantic completeness
  -> report.md / report.json
```

Review can return `accept`, `repair`, `expand_scope`, `ask_user`, or `stop`. `ask_user` and `stop` are hard stops for the agent.

## Main Features

- Visual Studio `.sln` and `.vcxproj` inspection.
- Isolated `runs/<task_id>/repo` workspaces.
- Independent Git baseline inside every workspace.
- LLM-assisted semantic scope analysis and semantic review.
- Five-level file scope model: `allowed`, `context`, `suspected`, `needs_approval`, `protected`.
- MSBuild verification with build logs.
- Stable `--json` CLI summaries for agent integration.
- Auditable Markdown and JSON reports.
- Repository-level Codex Skill and wrapper scripts.
- Offline demo path using the built-in file provider.

## Quick Start

Requirements: Windows, Visual Studio 2022 or Build Tools with C++ workload, Git, PowerShell, and CMake if you build from source.

```powershell
git clone https://github.com/<owner>/AgentGuardVS-CXX.git
cd AgentGuardVS-CXX

cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug

$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
.\examples\run-library-demo.ps1
```

The demo builds the original sample project, runs its self-test, creates an isolated AgentGuard workspace, applies a deterministic example edit inside that workspace, verifies with MSBuild, and runs an offline semantic review.

## Installation Requirements

- Windows 11 or a supported Windows development machine.
- Visual Studio 2022 or Visual Studio Build Tools 2022 with C++ build tools.
- MSBuild available from Visual Studio or Build Tools.
- Git available on `PATH`.
- PowerShell 5.1 or newer.
- CMake 3.21+ when building from source.

See [docs/INSTALL.md](docs/INSTALL.md) for detailed setup and environment variables.

## Build

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug
ctest --test-dir build\vs2022-debug -C Debug --output-on-failure
```

## Configure An API Provider

Unit tests and the included demo do not require a network LLM. Real provider calls are intended for manual smoke tests and actual agent workflows.

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "openai"   # or deepseek / claude
$env:AGENTGUARD_OPENAI_MODEL = "<model>"
$env:OPENAI_API_KEY = "<api-key>"
```

Provider-specific variables:

- OpenAI: `OPENAI_API_KEY`, `AGENTGUARD_OPENAI_MODEL`
- DeepSeek: `DEEPSEEK_API_KEY`, `AGENTGUARD_DEEPSEEK_MODEL`
- Claude: `ANTHROPIC_API_KEY`, `AGENTGUARD_CLAUDE_MODEL`

Do not write API keys into the repository, Skill files, reports, or logs.

## Install The Codex Skill

The repository includes a distributable Skill at `skills/agentguard-vs-cxx`.

```powershell
.\scripts\install-skill.ps1 -AgentGuardExe $env:AGENTGUARD_EXE -Force
```

After installation, a new Codex session can use `/skills` to discover `agentguard-vs-cxx`. The Skill tells Codex to call the wrapper scripts first, then fall back to raw CLI commands only when needed.

## Run The Library-System Demo

```powershell
.\examples\run-library-demo.ps1
```

The source demo lives in `examples/library-system`. The script does not modify that original project. The edit and verification happen under an isolated workspace in `runs/library-demo`.

The demo is intentionally small. It validates the AgentGuard workflow and Skill integration path; it is not evidence that every enterprise C++ project is fully covered.

## Report Example

Reports are written under the task workspace:

```text
runs/<task_id>/
  repo/
  logs/verify_build.log
  reports/report.md
  reports/report.json
  semantic_scope.json
  semantic_review.json
```

`report.json` records source project, workspace repo, Git top-level, diff base, scope lists, build result, review result, and risk notes.

## Safety Boundaries

- The platform does not edit the original target project.
- Workspace repos get their own Git baseline.
- `protected_files` are never valid edit targets.
- `needs_approval_files` require a user decision before modification.
- API keys must remain in environment variables or secret stores.
- LLM semantic judgment can be wrong; human review remains necessary.

See [docs/SECURITY_MODEL.md](docs/SECURITY_MODEL.md) for details.

## Current Limitations

- The primary target is Visual Studio C++ and MSBuild.
- LLM-assisted semantic judgment may be incomplete or wrong.
- Build success does not prove business logic is correct.
- Current validation is centered on small Visual Studio C++ projects and controlled examples.
- Enterprise-scale repositories still require human review, policy tuning, and project-specific tests.
- The symbol and dependency analysis is lightweight; it is not a complete C++ compiler or clang-based AST.

## License

AgentGuardVS-CXX is released under the MIT License. MIT is intentionally permissive for this kind of developer tooling: it allows individuals, teams, and companies to adopt or adapt the project while keeping the copyright notice and warranty disclaimer.

## Roadmap

- P0: Independent Git baseline and diff isolation.
- P1: Distributable Codex Skill and installer.
- P2: Lightweight symbol-level and dependency-level impact analysis.
- P3: GitHub Action prototype.
- P4: Broader language and IDE integration after the Visual Studio C++ path is stable.

See [docs/ROADMAP.md](docs/ROADMAP.md).

## GitHub Actions

Initial Windows build and library demo workflows are included under `.github/workflows`. See [docs/GITHUB_ACTION.md](docs/GITHUB_ACTION.md) for the current CI shape and the prototype `agentguard-review` action.
