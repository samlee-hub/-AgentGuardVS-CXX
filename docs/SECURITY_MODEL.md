# Security Model

AgentGuardVS-CXX is designed to reduce uncontrolled AI edits in Visual Studio C++ projects. It provides process boundaries and audit artifacts, not a guarantee that an LLM will always make correct decisions.

## Original Project Is Read-Only

The platform should not directly modify the original target project. It copies the project into an isolated workspace and performs analysis, edits, verification, diffing, and review there.

```text
source project -> runs/<task_id>/repo
```

The source `.git` directory is not copied.

## Isolated Workspace Baseline

Each workspace repo gets its own Git baseline. Diff capture is allowed only when Git top-level equals the workspace repo path. This prevents accidental diffs from a parent repository.

## File Scope Rules

`allowed_files`
: Files the agent may edit directly.

`context_files`
: Files the agent may read but must not edit.

`suspected_files`
: Files that may be relevant but are not editable by default. They can be proposed for expansion after review or build evidence.

`needs_approval_files`
: Files that require a user decision before modification.

`protected_files`
: Files that must not be modified.

## API Keys

API keys must remain outside the repository. Use environment variables or a secret manager:

- `OPENAI_API_KEY`
- `DEEPSEEK_API_KEY`
- `ANTHROPIC_API_KEY`

Do not write secrets into Skill files, logs, reports, sample JSON, or tests.

## Review Stops

If semantic review returns `ask_user`, the agent must stop and wait for user confirmation.

If semantic review returns `stop`, the agent must stop and report the risk.

If review requests `expand_scope`, the agent must record the reason and request or apply expansion only according to the configured workflow.

## Current Limits

- LLM-assisted semantic review can be wrong.
- Build success does not prove business correctness.
- The platform cannot validate runtime behavior beyond the target project's own tests.
- Enterprise repositories may require stricter protected-file policies.
- Current validation is centered on small Visual Studio C++ projects and controlled examples.
- The primary supported workflow is Visual Studio C++ with MSBuild.
- Human review remains required before merging meaningful changes.
