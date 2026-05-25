# Architecture

AgentGuardVS-CXX is organized as a local command-line control layer around external AI coding agents. The core platform is C++20; PowerShell scripts are used for installation and demo orchestration.

## CLI Layer

`AgentGuardVS.exe` exposes agent-friendly commands:

- `analyze`: create an isolated workspace and produce `semantic_scope.json`.
- `verify`: run MSBuild in the isolated workspace and write build logs.
- `review`: review the workspace diff and build result after edits.
- `detect`: detect a generic project profile from a project root.
- `verify-profile`: run configured generic verification commands from `agentguard.json`.
- `run`: execute the older end-to-end repair-loop path.
- `llm-smoke`: manually test provider connectivity.

Commands intended for Skill automation support `--json`, returning a compact machine-readable summary while detailed artifacts are written into the run directory.

## Workspace Layer

The workspace layer copies the source project into `runs/<task_id>/repo` without copying the source `.git` directory. It then initializes an independent Git repository inside the workspace:

```text
git init
git add .
git commit -m "agentguard baseline"
```

Diff reporting validates that Git top-level equals the workspace repo path before reading diffs. This prevents accidental use of a parent repository.

## SemanticAnalyzerAgent

`SemanticAnalyzerAgent` builds an LLM prompt from the task, solution/project metadata, indexed files, and static candidate lists. It expects structured JSON and writes:

- `semantic_scope.json`
- `semantic_analyze_prompt.txt`
- `semantic_analyze_raw_response.json`

The output classifies files as `allowed`, `context`, `suspected`, `needs_approval`, or `protected`.

## SemanticReviewAgent

`SemanticReviewAgent` reviews the post-edit diff and build status. It writes:

- `semantic_review.json`
- `semantic_review_prompt.txt`
- `semantic_review_raw_response.json`

The review action can be `accept`, `repair`, `expand_scope`, `ask_user`, or `stop`.

## MSBuildVerifier

`MSBuildVerifier` locates the solution inside the workspace and invokes MSBuild with the requested configuration and platform. It writes `logs/verify_build.log` and returns a compact build summary for CLI JSON output and reports.

## Generic Project Profile Layer

`src/project` is the first generalization layer beyond Visual Studio C++. It detects project roots from common markers such as `.sln`, `.vcxproj`, `package.json`, `pyproject.toml`, `CMakeLists.txt`, `.csproj`, Java build files, and `.uproject`.

An optional `agentguard.json` file can override the detected type and define explicit `verify_steps`. `verify-profile` executes those steps through the existing `ProcessRunner` and returns structured command results. This layer is additive and does not replace the MSBuild-backed Visual Studio C++ workflow.

## ErrorParser And ErrorClassifier

Build output is parsed into structured diagnostics so agents can reason about compiler, linker, and MSBuild failures. Error classification is intentionally conservative; it supports repair context but does not replace compiler output.

## ReportWriter

`ReportWriter` emits `reports/report.md` and `reports/report.json`. Reports include:

- source project path
- workspace repo path
- Git top-level
- diff base
- source/workspace modification status
- semantic scope
- build result
- review result
- risks and follow-up notes

## Skill Integration Layer

`skills/agentguard-vs-cxx` contains a distributable Codex Skill. It tells Codex when to use AgentGuardVS-CXX, which wrapper scripts to call, and which safety rules are mandatory.

The wrapper scripts prefer stable JSON commands:

- `run-analyze.ps1`
- `run-verify.ps1`
- `run-review.ps1`
- `run-full-cycle.ps1`

The Skill is a workflow control layer. It does not replace developer review.
