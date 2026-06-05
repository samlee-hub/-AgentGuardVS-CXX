# AgentGuardVS-CXX Workflow

This workflow controls AI-assisted changes to profiled projects. It is a guardrail and audit process, not a guarantee that generated code is semantically perfect.

Visual Studio C++ is the mature path. Other project types rely on `ProjectProfile`, `SourceFilePolicy`, and configured or inferred `verify_commands`.

## Flow

1. User provides a task and target project root or legacy `.sln`.
2. Run detect when the project type is unclear.
3. Run analyze:
   - Creates an isolated workspace.
   - Initializes an independent Git baseline.
   - Detects a project profile.
   - Indexes files through the profile source policy.
   - Produces `semantic_scope.json`.
   - Produces `report.md` and `report.json`.
4. Read `semantic_scope.json`.
5. Edit only `allowed_files` in the isolated workspace repo.
6. Run verify:
   - For `--project`, runs profile `verify_commands`.
   - For legacy `--workspace`, builds the workspace solution with MSBuild.
   - Writes command/build output and JSON status.
7. Run review:
   - Reads the workspace diff from the isolated Git repo.
   - Reads build or command result evidence where available.
   - Produces `semantic_review.json`.
8. Follow review `next_action`:
   - `accept`: finish and report artifacts.
   - `repair`: repair within current `allowed_files`, then rerun verify and review.
   - `expand_scope`: add only suggested safe files that are not protected and do not require approval, record the reason, then rerun verify and review.
   - `ask_user`: stop and ask the user which files or decision require approval.
   - `stop`: stop and report the risk.

## Loop Rules

- Do not repair blindly after build or command failure.
- Do not expand scope without review evidence.
- Do not touch `protected_files`.
- Do not touch `needs_approval_files` until the user confirms.
- Do not edit the original source project.
- Do not remove functionality only to make verification pass.

## Minimum Completion Evidence

Completion requires:

- Workspace path.
- Project kind and profile source.
- Modified files.
- Verification result, including PASS, SKIP, and FAIL commands.
- Review result.
- Diff summary.
- Risk items.
- `report.md` and `report.json`.
