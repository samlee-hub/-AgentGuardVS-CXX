# AgentGuardVS-CXX

[![Windows Build](https://github.com/samlee-hub/-AgentGuardVS-CXX/actions/workflows/windows-build.yml/badge.svg)](https://github.com/samlee-hub/-AgentGuardVS-CXX/actions/workflows/windows-build.yml)
[![Examples](https://github.com/samlee-hub/-AgentGuardVS-CXX/actions/workflows/examples.yml/badge.svg)](https://github.com/samlee-hub/-AgentGuardVS-CXX/actions/workflows/examples.yml)

AgentGuardVS-CXX is a control layer for AI coding agents working on multi-project repositories. It detects a project profile, analyzes impact scope, isolates edits, runs project-specific verification commands, performs LLM-assisted semantic review, and writes audit reports.

`AgentGuardVS.exe` remains the binary name for compatibility. The `VS` name is legacy and no longer means the platform is limited to Visual Studio projects.

## What Problem It Solves

AI coding agents can move quickly inside large repositories, but real projects often have fragile build settings, generated output, dependency caches, public interfaces, indirect dependencies, and linked configuration spread across project files. A small request can accidentally touch protected files, miss related tests, or enter a repair loop after a verification failure.

AgentGuardVS-CXX adds a controlled workflow around those agents:

- Create an isolated workspace before edits happen.
- Ask an LLM to classify semantic impact scope.
- Allow direct edits only in `allowed_files`.
- Keep `context_files` read-only.
- Require explicit user approval for `needs_approval_files`.
- Forbid edits to `protected_files`.
- Verify changes with MSBuild or profile-specific commands.
- Review the final diff and build result before accepting the task.
- Produce JSON and Markdown reports for audit and later debugging.

## What It Is Not

AgentGuardVS-CXX is not an IDE, a replacement for Codex, Claude Code, Cursor, MSBuild, CMake, npm, pytest, dotnet, Maven, Gradle, UnrealBuildTool, or your test framework. It does not guarantee business correctness, and it should not be treated as a fully autonomous engineer. It is a reliability and audit layer around agent workflows.

## Supported Project Types

Mature path:

- `visualstudio-cxx`: `.sln` / `.vcxproj` discovery, C/C++ scope indexing, isolated workspace, MSBuild verification, semantic review, and audit reports.

Full profile-based path:

- `cmake-cpp`: C/C++ and CMake config indexing, isolated workspace analyze, CMake configure/build verify commands, review, and audit.
- `python`: `.py` / `.pyi` and Python config indexing, isolated workspace analyze, `compileall` / optional pytest verification, review, and audit.
- `node`: JavaScript/TypeScript and `package.json` indexing, isolated workspace analyze, npm test/build or package checks, review, and audit.
- `dotnet`: C# / F# project indexing, isolated workspace analyze, `dotnet build` / optional `dotnet test`, review, and audit.
- `java`: `.java` plus Maven/Gradle config indexing, isolated workspace analyze, Maven/Gradle test verification, review, and audit.
- `custom`: behavior from `agentguard.json`, including language list, source policy, and verification commands.

Experimental profile/verify path:

- `unreal`: `.uproject`, `.uplugin`, `*.Build.cs`, `*.Target.cs`, and Source C++ files are detected and indexed. Full Unreal compilation is not assumed; configure verification through `agentguard.json`.
- `unknown`: safe detection and indexing fallbacks only. Add `agentguard.json` for reliable use.

## Core Workflow

```text
user task
  -> detect ProjectProfile
  -> analyze
  -> semantic_scope.json
  -> edit allowed_files in isolated workspace
  -> verify with MSBuild or profile verify_commands
  -> review semantic completeness
  -> report.md / report.json
```

Review can return `accept`, `repair`, `expand_scope`, `ask_user`, or `stop`. `ask_user` and `stop` are hard stops for the agent.

## Main Features

- Visual Studio `.sln` and `.vcxproj` inspection.
- ProjectProfile detection for Visual Studio C++, CMake C++, Python, Node, .NET, Java, Unreal, custom, and unknown roots.
- SourceFilePolicy indexing for language-specific source files and common config files.
- Isolated `runs/<task_id>/repo` workspaces.
- Independent Git baseline inside every workspace.
- LLM-assisted semantic scope analysis and semantic review.
- Five-level file scope model: `allowed`, `context`, `suspected`, `needs_approval`, `protected`.
- MSBuild verification for legacy Visual Studio C++ workspaces.
- CommandVerifier for profile-based verification commands.
- Stable `--json` CLI summaries for agent integration.
- Auditable Markdown and JSON reports.
- Repository-level Codex Skill and wrapper scripts.
- Offline demo paths using the built-in fake and file providers.

## Quick Start

Requirements: Windows, Visual Studio 2022 or Build Tools with the C++ workload when building this project, Git, PowerShell, and CMake if you build from source. Target projects may also require their own tools, such as Python, Node/npm, dotnet, Maven, Gradle, or Unreal tooling.

### 1 Minute: What This Does

AgentGuardVS-CXX is a guardrail around AI edits. It creates an isolated workspace, classifies the safe edit scope, verifies with the project profile's command plan, performs semantic review, and writes audit reports. The original project is not edited by the platform workflow.

### Source Developer Entry

```powershell
git clone https://github.com/samlee-hub/-AgentGuardVS-CXX.git
cd -AgentGuardVS-CXX
.\scripts\bootstrap.ps1
```

Build from source:

> Dependencies are managed via vcpkg manifest mode (`vcpkg.json`). Install vcpkg and set the `VCPKG_ROOT` environment variable before configuring.

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build\vs2022-debug --config Debug

$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
.\scripts\install-plugin.ps1 -Force
```

### Release User Entry

Download a release zip, extract it, then run:

```powershell
.\scripts\install.ps1
agentguard --version
.\scripts\doctor.ps1
.\examples\run-library-demo.ps1 -Provider fake
```

Open a new PowerShell window after `install.ps1` if `agentguard` is not immediately found; user-level `PATH` changes are picked up by new shells.

### Codex User Entry

```powershell
.\scripts\install-plugin.ps1
```

Then start Codex in a target repository and invoke the installed Skill:

```text
@AgentGuardVS-CXX Use AgentGuardVS-CXX to safely modify this project.
```

### Offline And API Routes

No API key is needed for a basic local check:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "fake"
.\examples\run-library-demo.ps1 -Provider fake
```

Use a real provider only for live semantic analysis and review:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "deepseek" # or openai / claude
$env:AGENTGUARD_DEEPSEEK_MODEL = "<model>"
$env:DEEPSEEK_API_KEY = "<deepseek-api-key>"
```

The demo builds the original sample project, runs its self-test, creates an isolated AgentGuard workspace, applies a deterministic example edit inside that workspace, verifies with MSBuild, and runs semantic review.

## Installation Requirements

- Windows 11 or a supported Windows development machine.
- Visual Studio 2022 or Visual Studio Build Tools 2022 with C++ build tools.
- MSBuild available from Visual Studio or Build Tools.
- Git available on `PATH`.
- PowerShell 5.1 or newer.
- CMake 3.21+ when building from source.

See [docs/INSTALL.md](docs/INSTALL.md) for detailed setup and environment variables.
See [docs/PLUGIN_INSTALL.md](docs/PLUGIN_INSTALL.md) for GitHub-distributed Codex Plugin installation.
See [docs/RELEASE_PACKAGE.md](docs/RELEASE_PACKAGE.md) for the recommended release zip layout.

## Build

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
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

## Install The Codex Plugin / Skill

The repository includes a distributable Codex Plugin at `plugins/agentguard-vs-cxx` and the raw Skill at `skills/agentguard-vs-cxx`.

```powershell
.\scripts\install-plugin.ps1 -AgentGuardExe $env:AGENTGUARD_EXE -Force
```

After installation, restart Codex and use `@AgentGuardVS-CXX` or `/skills` to discover `agentguard-vs-cxx`. Current distribution is through GitHub clone plus local plugin installation; this README does not claim the plugin is available in an official searchable Plugin Directory.

## Run The Library-System Demo

```powershell
.\examples\run-library-demo.ps1 -Provider fake
```

The source demo lives in `examples/library-system`. The script does not modify that original project for code edits. The edit and verification happen under an isolated workspace in `runs/library-demo`.

The demo is intentionally small. It validates the AgentGuard workflow and Skill integration path; it is not evidence that every enterprise C++ project is fully covered.

## Reliability Evaluation

AgentGuardVS-CXX was evaluated with an agent-autonomous black-box test on a self-developed Visual Studio C++ multiplayer networking project.

In 12 natural-language coding tasks, Codex autonomously discovered and invoked the AgentGuardVS-CXX Skill in 12/12 tasks, generated isolated workspaces and audit reports in 12/12 tasks, and caused 0/12 source repository pollution. One task exposed an unauthorized modification caused by inaccurate fake-provider scope analysis, which is tracked as a current limitation.

Summary:

Metric
Result
Skill autonomous usage
12/12 = 100%
Workspace isolation
12/12 = 100%
Source repository pollution
0/12 = 0%
Report generation
12/12 = 100%
Unauthorized modification
1/12 = 8.33%
Build pass rate among verified tasks
6/6 = 100%。

## AgentGuard Bench

This repository also includes [AgentGuard Bench](benchmarks/AgentGuardBench), a reproducible raw-vs-guarded AI coding agent evaluation framework for a real Visual Studio C++ multiplayer game architecture testbed.

The benchmark defines 12 machine-readable tasks, isolated run preparation, raw and guarded prompts, scope checking, MSBuild verification, semantic checks, failure classification, and CSV/Markdown/HTML reports. Current generated reports are baseline prepared-run evidence; run raw and guarded agent executions inside the prepared workspaces before treating the metrics as comparative model results.

## Generic Project Profiles

AgentGuardVS-CXX now uses `ProjectProfile` as the main abstraction for non-trivial project operations. `detect`, `analyze --project`, `verify --project`, `verify-profile`, `doctor`, reports, and audit JSON all consume the same profile data.

Detect a project profile:

```powershell
AgentGuardVS.exe detect --project "<path-to-project-root>" --json
```

Analyze a project root into an isolated workspace:

```powershell
AgentGuardVS.exe analyze --project "<path-to-project-root>" --task "Change behavior X" --provider fake --json
```

Run configured verification commands:

```powershell
AgentGuardVS.exe verify --project "<path-to-project-root>" --json
AgentGuardVS.exe verify-profile --project "<path-to-project-root>" --json
```

Optional `agentguard.json` example:

```json
{
  "project_kind": "custom",
  "languages": ["cpp", "python", "typescript"],
  "include": ["src/**", "tests/**"],
  "exclude": ["build/**", "node_modules/**", "runs/**"],
  "source_extensions": [".cpp", ".h", ".py", ".ts"],
  "verify_commands": [
    {
      "name": "unit",
      "command": "python",
      "args": ["-m", "pytest"],
      "working_directory": ".",
      "timeout_seconds": 120,
      "required": true,
      "skip_if_missing_executable": true
    }
  ]
}
```

Minimal examples:

```powershell
# CMake C++
AgentGuardVS.exe analyze --project . --task "Refactor parser error handling" --provider fake --json
AgentGuardVS.exe verify --project . --json

# Python
AgentGuardVS.exe analyze --project D:\repo\pyapp --task "Add validation to loader" --provider fake --json
AgentGuardVS.exe verify --project D:\repo\pyapp --json

# Node
AgentGuardVS.exe analyze --project D:\repo\webapp --task "Fix settings persistence" --provider fake --json
AgentGuardVS.exe verify --project D:\repo\webapp --json

# Custom command profile
AgentGuardVS.exe detect --project D:\repo\mixed --json
AgentGuardVS.exe verify-profile --project D:\repo\mixed --json
```

Automatic detection currently recognizes Visual Studio C++, CMake C++, Node, Python, .NET, Java, Unreal, custom, and unknown project roots. The legacy `run` repair-loop command is still Visual Studio C++ centered; for other project types use `analyze --project`, edit inside the emitted isolated workspace, then run `verify --project` or configured project commands and `review`.

## Report Example

Reports are written under the task workspace:

```text
runs/<task_id>/
  repo/
  logs/verify_build.log
  artifacts/
    prompts/
    raw_responses/
    build_logs/
    diffs/
    reports/
  reports/report.md
  reports/report.json
  semantic_scope.json
  semantic_review.json
```

`report.md` starts with a one-screen summary: accepted status, build result, review next action, whether the source project was modified, modified files, risks, and artifact paths.

`report.json` includes `schema_version`, compatibility fields, and structured sections:

- `task`
- `source_project`
- `workspace`
- `semantic_scope`
- `build`
- `review`
- `diff`
- `risks`
- `metrics`
- `artifacts`
- `audit`

Metrics include analyze, verify, and review duration, changed file counts, build error count, repair rounds, scope expansions, provider, model, and cache hit status.
Audit includes project kind, languages, profile source, indexed/skipped file counts, verify commands, verify result, modified files, and forbidden write attempts.

## Safety Boundaries

- The platform does not edit the original target project.
- Workspace repos get their own Git baseline.
- `protected_files` are never valid edit targets.
- `needs_approval_files` require a user decision before modification.
- API keys must remain in environment variables or secret stores.
- LLM semantic judgment can be wrong; human review remains necessary.

See [docs/SECURITY_MODEL.md](docs/SECURITY_MODEL.md) for details.

## Current Limitations

- The most mature end-to-end repair loop remains Visual Studio C++ and MSBuild.
- LLM-assisted semantic judgment may be incomplete or wrong.
- Build success does not prove business logic is correct.
- Current non-VS validation covers profile detection, source indexing, command verification, analyze workspace creation, CLI JSON, and audit fields; language-specific semantic analyzers are still lightweight.
- Enterprise-scale repositories still require human review, policy tuning, and project-specific tests.
- The symbol and dependency analysis is lightweight; it is not a complete C++ compiler or clang-based AST.

See [docs/LIMITATIONS.md](docs/LIMITATIONS.md) for a fuller statement of current boundaries.

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

Initial Windows build and examples workflows are included under `.github/workflows`. See [docs/GITHUB_ACTION.md](docs/GITHUB_ACTION.md) for the current CI shape and the prototype `agentguard-review` action.
