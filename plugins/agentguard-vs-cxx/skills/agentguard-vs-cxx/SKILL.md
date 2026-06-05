---
name: agentguard-vs-cxx
description: Use AgentGuardVS-CXX as a multi-project AgentGuard workflow before modifying, refactoring, or fixing repositories that need scoped, auditable agent edits. It supports Visual Studio C++ as the mature path and also profile-based CMake C++, Python, Node, .NET, Java, Unreal, and custom projects through ProjectProfile, SourceFilePolicy, isolated workspaces, command verification, semantic review, and audit reports. Do not trigger it for read-only browsing, code explanation, or simple questions.
---

# AgentGuardVS-CXX

Use this skill when a coding task needs guardrails: profile detection, semantic scope analysis, isolated workspace edits, verification, semantic review, and audit artifacts.

`AgentGuardVS.exe` is the legacy binary name. It does not mean the current workflow only supports Visual Studio.

Visual Studio C++ (`.sln`, `.vcxproj`, MSBuild) is the most mature end-to-end path. Other project types depend on detected or configured `ProjectProfile` data and `verify_commands`.

## Required Workflow

Prefer the bundled scripts in `scripts/` for legacy Visual Studio C++ tasks. For non-VS project roots, use the raw `--project` CLI commands until wrappers are expanded.

1. Run analyze before editing.
2. Read the emitted `semantic_scope.json`.
3. Edit only files listed in `allowed_files`.
4. Treat `context_files` as read-only.
5. Treat `suspected_files` as locked until semantic review returns `expand_scope`.
6. Stop and ask the user before editing any `needs_approval_files`.
7. Never edit `protected_files`.
8. Make changes only inside the isolated workspace repo from analyze output.
9. Run project verification after edits.
10. Run semantic review after verification, including failed builds.
11. Do not blindly retry after analyze, verify, or review failure. Read the JSON error, prompt, raw response, build log, command output, and report artifacts first.
12. If review returns `repair`, repair only inside the current allowed files.
13. If review returns `expand_scope`, add only review-approved safe files and record the reason.
14. If review returns `ask_user`, stop and wait for user confirmation.
15. If review returns `stop`, stop and report the risk.

## Script Usage

For Visual Studio C++:

```powershell
skills/agentguard-vs-cxx/scripts/run-analyze.ps1 `
  -Solution "<path-to-solution.sln>" `
  -Task "<user task>" `
  -Provider $env:AGENTGUARD_LLM_PROVIDER `
  -Model $env:AGENTGUARD_DEEPSEEK_MODEL

skills/agentguard-vs-cxx/scripts/run-verify.ps1 `
  -Workspace "<runs task root or repo>"

skills/agentguard-vs-cxx/scripts/run-review.ps1 `
  -Workspace "<runs task root or repo>" `
  -Task "<user task>" `
  -Provider $env:AGENTGUARD_LLM_PROVIDER `
  -Model $env:AGENTGUARD_DEEPSEEK_MODEL
```

## Raw CLI Examples

```powershell
AgentGuardVS.exe detect --project "<path-to-project-root>" --json
AgentGuardVS.exe analyze --project "<path-to-project-root>" --task "<task>" --provider fake --json
AgentGuardVS.exe verify --project "<path-to-project-root>" --json
AgentGuardVS.exe verify-profile --project "<path-to-project-root>" --json
AgentGuardVS.exe doctor --project "<path-to-project-root>" --json

AgentGuardVS.exe analyze --solution "<path-to-solution.sln>" --task "<task>" --provider fake --json
AgentGuardVS.exe verify --workspace "<workspace-or-repo>" --configuration Debug --platform x64 --json
AgentGuardVS.exe review --workspace "<workspace-or-repo>" --task "<task>" --provider fake --json
```

## Safety Rules

- Only modify `allowed_files`.
- Never modify `protected_files`.
- Ask the user before modifying any `needs_approval_files`.
- Keep `context_files` read-only.
- Do not expand scope because a build failed; expand only when semantic review returns `expand_scope`.
- Do not place API keys in Skill files, repository files, reports, logs, prompts, raw responses, or cache files.
- Use `--no-cache` when validating provider behavior or investigating stale model output.

## References

- Read `references/workflow.md` when deciding the next action after analyze, verify, or review.
- Read `references/semantic-scope-schema.md` when interpreting scope files.
- Read `references/troubleshooting.md` when a wrapper script or CLI command fails.

## Final Response Requirements

Always report:

- Modified files.
- Project kind and profile source.
- Verification result, including PASS, SKIP, and FAIL commands.
- Semantic review result and `next_action`.
- Diff summary.
- Risk items.
- Metrics summary, including provider, model, cache hit, durations, and build error count when available.
- `report.md` and `report.json` paths.
- Artifact paths for prompts, raw responses, build logs, diffs, and reports when available.
- Files still requiring user approval.
- Any skipped verification and the exact reason.
