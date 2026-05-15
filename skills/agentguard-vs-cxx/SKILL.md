---
name: agentguard-vs-cxx
description: Use AgentGuardVS-CXX when modifying Visual Studio C++ projects with .sln, .vcxproj, MSBuild, or Windows C++ code. This skill enforces semantic scope analysis, controlled file modification, MSBuild verification, semantic review, and audit reporting.
---

# AgentGuardVS-CXX

Use this skill for Visual Studio C++ engineering tasks involving `.sln`, `.vcxproj`, MSBuild, Windows C++ services, WinSock/C++ projects, or user requests to modify C++ code with build verification.

AgentGuardVS-CXX is a control workflow for AI-assisted changes. It does not replace developer review, project tests, or user approval for risky files.

## Required Workflow

Prefer the bundled scripts in `scripts/` over hand-written CLI commands.

1. Run `scripts/run-analyze.ps1` before editing.
2. Read the emitted `semantic_scope.json`.
3. Edit only files listed in `allowed_files`.
4. Treat `context_files` as read-only.
5. Treat `suspected_files` as locked until semantic review returns `expand_scope`.
6. Stop and ask the user before editing any `needs_approval_files`.
7. Never edit `protected_files`.
8. Make changes only inside the isolated workspace repo from analyze output.
9. Run `scripts/run-verify.ps1` after edits.
10. Run `scripts/run-review.ps1` after verify, including failed builds.
11. If review returns `repair`, repair only inside the current allowed files.
12. If review returns `expand_scope`, add only review-approved safe files and record the reason.
13. If review returns `ask_user`, stop and wait for user confirmation.
14. If review returns `stop`, stop and report the risk.

## Script Usage

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
AgentGuardVS.exe analyze --solution "<path-to-solution.sln>" --task "<task>" --provider fake --json
AgentGuardVS.exe verify --workspace "<workspace-or-repo>" --configuration Debug --platform x64 --json
AgentGuardVS.exe review --workspace "<workspace-or-repo>" --task "<task>" --provider fake --json
```

## References

- Read `references/workflow.md` when deciding the next action after analyze, verify, or review.
- Read `references/semantic-scope-schema.md` when interpreting scope files.
- Read `references/troubleshooting.md` when a wrapper script or CLI command fails.

## Final Response Requirements

Always report:

- Modified files.
- Build result.
- Semantic review result and `next_action`.
- Diff summary.
- Risk items.
- `report.md` and `report.json` paths.
- Files still requiring user approval.
- Any skipped verification and the exact reason.
