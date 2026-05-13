# Task Spec

## Purpose

`TaskSpec` is the machine-readable contract that constrains a single AI-assisted engineering task before patch generation and application begin.

## Required Fields

| Field | Type | Meaning |
| --- | --- | --- |
| `task_id` | string | Stable identifier for the run and workspace directory. |
| `user_request` | string | Original task description provided by the user. |
| `target_solution` | string | Path to the target Visual Studio solution under analysis. |
| `target_project` | string | Preferred or primary Visual Studio project when relevant. |
| `allowed_files` | array of strings | Relative paths that a validated patch may modify. |
| `forbidden_files` | array of strings | Relative paths that must not be modified. |
| `acceptance_criteria` | array of strings | Conditions used to judge whether the task is sufficiently handled. |
| `max_repair_rounds` | integer | Upper bound on automated build-fix iterations. |

## Field Semantics

### `task_id`

- Must be safe for workspace folder creation.
- Must map to `runs/<task_id>/...`.

### `user_request`

- Preserves the original human intent.
- May later be combined with selected code context and diagnostics.

### `target_solution`

- Identifies the Visual Studio solution relevant to the task.
- The source solution is never edited directly.

### `target_project`

- Narrows planning when a solution contains multiple projects.
- May be empty in early dry-run analysis if no project is selected yet.

### `allowed_files`

- Defines the writable patch boundary.
- Every patch target must be validated against this list.

### `forbidden_files`

- Defines an explicit denylist.
- If a file appears in both lists, the denylist takes precedence.

### `acceptance_criteria`

- Captures task-specific expectations such as:
  - "Build remains successful."
  - "Existing login flow is not intentionally modified."
  - "The requested cooldown behavior is represented in the planned changes."

### `max_repair_rounds`

- Prevents unbounded repair loops.
- The initial project plan uses a small bounded value such as `3`.

## Example Shape

```json
{
  "task_id": "summon-cooldown-001",
  "user_request": "Add resource cost and a global 3 second summon cooldown.",
  "target_solution": "Server.sln",
  "target_project": "Server",
  "allowed_files": [
    "Server/Game/SummonService.cpp",
    "Server/Game/SummonService.h"
  ],
  "forbidden_files": [
    "Server/Auth/LoginService.cpp"
  ],
  "acceptance_criteria": [
    "All edits stay inside the isolated workspace.",
    "The intended gameplay behavior is represented in the patch plan.",
    "The repair loop does not exceed the configured limit."
  ],
  "max_repair_rounds": 3
}
```

## Contract Rule

`TaskSpec` constrains patch generation. It does not itself apply code changes and it must never weaken the rule that all real modifications happen only inside `runs/<task_id>/repo`.
