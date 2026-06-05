# Architecture

AgentGuardVS-CXX is organized as a local command-line control layer around external AI coding agents. The core platform is C++20; PowerShell scripts are used for installation and demo orchestration. `AgentGuardVS.exe` is a legacy binary name and does not limit the platform to Visual Studio projects.

## CLI Layer

`AgentGuardVS.exe` exposes agent-friendly commands:

- `detect`: detect a project profile from a project root.
- `analyze`: create an isolated workspace, detect a `ProjectProfile`, index files through `SourceFilePolicy`, and produce `semantic_scope.json`.
- `verify`: run profile verification commands for `--project`, or run legacy MSBuild verification for `--workspace`.
- `verify-profile`: run configured or inferred verification commands from a project profile.
- `doctor`: inspect profile detection, indexing, and verification readiness.
- `review`: review the workspace diff and build result after edits.
- `run`: execute the older Visual Studio C++ end-to-end repair-loop path.
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

## ProjectProfile Layer

`src/project` is the generalization layer beyond Visual Studio C++. It exposes `ProjectKind`, `LanguageKind`, `SourceFilePolicy`, and verification commands.

Detected `ProjectKind` values:

- `visualstudio-cxx`
- `cmake-cpp`
- `python`
- `node`
- `dotnet`
- `java`
- `unreal`
- `custom`
- `unknown`

Detection uses multiple signals: `.sln` / `.vcxproj`, `CMakeLists.txt`, `package.json`, Python project files, `.csproj` / `.fsproj`, Maven/Gradle files, `.uproject`, and `agentguard.json`.

`SourceFilePolicy` controls source indexing per profile. It includes source extensions, config-file patterns, include/exclude globs, default excluded directories, max file size, binary-file skip, and generated-file skip. Default exclusions include `.git`, `.vs`, `build`, `out`, `dist`, `node_modules`, `__pycache__`, `.venv`, `venv`, `target`, `bin`, `obj`, `runs`, `artifacts`, `coverage`, `.pytest_cache`, `.gradle`, `.idea`, and unnecessary `.vscode` content.

`agentguard.json` can override the detected profile:

```json
{
  "project_kind": "custom",
  "languages": ["cpp", "python", "typescript"],
  "include": ["src/**", "tests/**"],
  "exclude": ["build/**", "node_modules/**", "runs/**"],
  "source_extensions": [".cpp", ".h", ".py", ".ts"],
  "verify_commands": [
    {
      "name": "unit-tests",
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

JSON parse failures are surfaced as clear `agentguard.json parse error` messages. Relative paths are normalized through the project root and path traversal is rejected.

## SemanticAnalyzerAgent

`SemanticAnalyzerAgent` builds an LLM prompt from the task, project metadata, indexed files, and static candidate lists. It expects structured JSON and writes:

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

## Verification

Visual Studio C++ workspaces keep the MSBuild path: `verify --workspace` locates a `.sln`, invokes MSBuild with the requested configuration/platform, writes `logs/verify_build.log`, and returns a compact build summary.

Profile-based projects use `CommandVerifier`: `verify --project` and `verify-profile --project` detect the profile, build a command plan, and execute commands with:

- `name`
- `command`
- `args`
- `working_directory`
- `timeout_seconds`
- `required`
- `skip_if_missing_executable`

Missing executables are reported as `SKIP` when explicitly allowed by the command; otherwise they are `FAIL`. Optional command failures do not fail the whole verification result.

## Safety Boundaries

- Original projects are copied into an isolated `runs/<task_id>/repo`; normal analyze/review workflows do not edit the source project.
- `.git`, generated output, dependency caches, run artifacts, and secret-looking files are excluded or protected by default.
- `agentguard.json` include/exclude and command working directories are normalized relative to the project root; path traversal is rejected.
- Patch plans remain machine-auditable `create`, `modify`, and `delete` operations and are constrained by allowed/forbidden files.
- Visual Studio C++ remains the mature end-to-end `run` repair-loop path. Non-VS projects use `detect`, `analyze --project`, isolated workspace edits, `verify --project` or configured project commands, `review`, and audit reports.

## ErrorParser And ErrorClassifier

Build output is parsed into structured diagnostics so agents can reason about compiler, linker, and MSBuild failures. Error classification is intentionally conservative; it supports repair context but does not replace compiler output.

## ReportWriter

`ReportWriter` emits `reports/report.md` and `reports/report.json`. Reports include:

- source project path
- workspace repo path
- project kind, languages, and profile source
- indexed/skipped file counts
- verify commands and verify result
- Git top-level
- diff base
- source/workspace modification status
- semantic scope
- build or command verification result
- review result
- modified files and forbidden write attempts
- risks and follow-up notes

## Skill Integration Layer

`skills/agentguard-vs-cxx` contains a distributable Codex Skill. It tells Codex when to use AgentGuard, which wrapper scripts or CLI commands to call, and which safety rules are mandatory.

The wrapper scripts prefer stable JSON commands:

- `run-analyze.ps1`
- `run-verify.ps1`
- `run-review.ps1`
- `run-full-cycle.ps1`

The Skill is a workflow control layer. It does not replace developer review.
