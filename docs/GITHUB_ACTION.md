# GitHub Action Prototype

AgentGuardVS-CXX includes an initial GitHub Actions setup for open-source project hygiene and future PR review automation. The current workflows do not call real LLM APIs.

## Workflows

### `windows-build.yml`

Runs on `windows-latest`:

1. Check out the repository.
2. Configure CMake with the Visual Studio 2022 Debug preset.
3. Build AgentGuardVS-CXX.
4. Run unit tests.

The workflow sets `AGENTGUARD_LLM_PROVIDER=fake` and does not require API keys.

### `examples.yml`

Runs on `windows-latest`:

1. Build `AgentGuardVS`.
2. Build `examples/library-system`.
3. Run the library-system self-test.
4. Run `examples/run-library-demo.ps1 -Provider file`.
5. Upload `report.md`, `report.json`, and the verification build log as workflow artifacts.

This validates the demo path without OpenAI, DeepSeek, or Claude network calls.

## Prototype Composite Action

`action/agentguard-review/action.yml` is an early local wrapper. It accepts:

- `solution`
- `task`
- `provider`
- `model`
- `runs-root`
- `agentguard-exe`

Example:

```yaml
- name: Build AgentGuard
  run: cmake --build --preset vs2022-debug

- name: AgentGuard analyze prototype
  uses: ./action/agentguard-review
  with:
    solution: examples/library-system/Library.sln
    task: Add case-insensitive title search.
    provider: fake
    runs-root: agentguard-runs
    agentguard-exe: build/vs2022-debug/Debug/AgentGuardVS.exe
```

This action is not a complete PR reviewer yet. A production version should add diff input, workspace mapping, verify/review phases, artifact upload, annotations, and explicit secret handling for optional real provider calls.

## Secret Policy

CI workflows in this repository must not use real API keys by default. If a downstream user chooses to run real provider smoke tests, they should configure secrets in their own repository and keep those jobs separate from unit-test and demo workflows.
