# Development Rules

## 1. Core Language

- The platform core must be implemented in **C++20**.
- The project is managed with CMake and must remain compatible with the Visual Studio-oriented workflow defined for the repository.

## 2. Python Boundary

- Python is not part of the core execution path.
- Python may be added later only for:
  - Benchmark batch analysis
  - Summary statistics
  - CSV or Markdown export
  - Visualization preparation

## 3. Workspace Isolation

- All real source changes must occur only inside:
  - `runs/<task_id>/repo`
- The original user Visual Studio project must never be modified directly.
- Any future workflow that would bypass this rule is invalid.

## 4. LLM Boundary

- An LLM or external coding agent may propose structured output only.
- The accepted modification payload is a `PatchPlan`.
- The LLM must not directly create, edit, or delete repository files.

## 5. Patch Application Boundary

- `PatchApplier` is the only component allowed to mutate source files.
- Patch application must be validated against task constraints before execution.
- Later implementation must defend against forbidden files and path traversal.

## 6. Testing Requirement

- Every module must be accompanied by focused tests appropriate to its responsibility.
- Tests should cover both normal behavior and boundary conditions relevant to platform safety.
- Build verification, parsing, workspace isolation, and patch application all require explicit tests as the project grows.

## 7. Scope Discipline

- Each development step should implement only the requested scope.
- Avoid unrelated refactors.
- Avoid deleting documentation unless explicitly requested.
- If implementation reveals a design mismatch, state the mismatch and apply only the smallest needed correction.

## 8. Development Checkpoint Rule

Before starting a new step, the project documents should be revisited to confirm:

1. The step still supports the platform's reliability-control positioning.
2. The step does not violate workspace isolation.
3. The step does not shift core behavior into Python.
4. The step does not give direct file-write authority to the LLM.
