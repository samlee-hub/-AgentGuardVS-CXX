# Semantic Scope Schema

`semantic_scope.json` separates file access into five categories. Treat the category as authoritative until review changes it.

## allowed_files

Files Codex may edit directly inside the isolated workspace.

Rules:

- Only modify these files.
- Keep changes related to the user task.
- Rerun verify and review after edits.

## context_files

Files Codex may read but must not edit.

Use them for interfaces, data structures, calling patterns, and project conventions.

## suspected_files

Files that may be related but are not writable by default.

They can become writable only when semantic review returns `expand_scope` and the file is included in a safe suggested scope addition or otherwise explicitly approved by the workflow.

## needs_approval_files

Files that require user confirmation before editing.

Stop and ask the user before touching them. Do not infer approval from build failure or model suggestions.

## protected_files

Files that must never be edited in the current task.

If review or build diagnostics point to a protected file, stop and report the risk.

## Related Review Fields

`semantic_review.json` uses:

- `next_action`: `accept`, `repair`, `expand_scope`, `ask_user`, or `stop`.
- `suggested_scope_additions`: files proposed for expansion.
- `risks`: semantic or safety risks.
- `confidence`: model confidence, not a substitute for verification.
