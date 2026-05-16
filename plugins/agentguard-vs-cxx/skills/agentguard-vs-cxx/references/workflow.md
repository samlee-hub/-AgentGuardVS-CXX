# AgentGuardVS-CXX Workflow

This workflow controls AI-assisted changes to Visual Studio C++ projects. It is a guardrail and audit process, not a guarantee that the generated code is semantically perfect.

## Flow

1. User provides a C++ task and target `.sln`.
2. Run analyze:
   - Creates an isolated workspace.
   - Initializes an independent Git baseline.
   - Produces `semantic_scope.json`.
   - Produces `report.md` and `report.json`.
3. Read `semantic_scope.json`.
4. Edit only `allowed_files` in the isolated workspace repo.
5. Run verify:
   - Builds the workspace solution with MSBuild.
   - Writes `logs/verify_build.log`.
   - Returns JSON build status.
6. Run review:
   - Reads the workspace diff from the isolated Git repo.
   - Reads build result and parsed errors.
   - Produces `semantic_review.json`.
7. Follow review `next_action`:
   - `accept`: finish and report artifacts.
   - `repair`: repair within current `allowed_files`, then rerun verify and review.
   - `expand_scope`: add only suggested safe files that are not protected and do not require approval, record the reason, then rerun verify and review.
   - `ask_user`: stop and ask the user which files or decision require approval.
   - `stop`: stop and report the risk.

## Loop Rules

- Do not repair blindly after build failure.
- Do not expand scope without review evidence.
- Do not touch `protected_files`.
- Do not touch `needs_approval_files` until the user confirms.
- Do not edit the original source project.
- Do not remove functionality only to make the build pass.

## Minimum Completion Evidence

Completion requires:

- Workspace path.
- Modified files.
- Build result.
- Review result.
- Diff summary.
- Risk items.
- `report.md` and `report.json`.
