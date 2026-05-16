# Contributing

Thanks for helping improve AgentGuardVS-CXX. This project is a control layer for AI-assisted Visual Studio C++ changes; contributions should preserve isolation, auditability, and clear failure behavior.

## Build

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug
```

If you use vcpkg explicitly:

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>\scripts\buildsystems\vcpkg.cmake"
cmake --build build\vs2022-debug --config Debug
```

## Run Tests

```powershell
ctest --test-dir build\vs2022-debug -C Debug --output-on-failure
```

The test suite must not call real LLM APIs. Use `FakeLLMProvider` or `FileLLMProvider` for automated tests.

## API Key Policy

Do not commit API keys, tokens, local secret files, raw provider responses containing secrets, or private endpoint credentials. Keep secrets in environment variables or a secret manager.

The repository intentionally includes `.env.example` with empty values only.

## Adding A Provider

1. Add a provider implementation under `src/llm`.
2. Keep provider configuration in `LLMProviderConfig`.
3. Validate missing API key and missing model errors before making network calls.
4. Add unit tests that prove missing-secret behavior without calling the network.
5. Add any real provider smoke test as a manual opt-in path only.
6. Do not write prompts, raw responses, or reports containing secrets.

## Adding A Benchmark Task

Benchmark tasks live under `benchmarks/tasks.jsonl`. Each task should be small enough to reproduce, name the target example or fixture, define acceptance expectations, and avoid real API keys. Prefer tasks that can be checked with `fake` or `file` provider paths first, then optionally smoke-tested with a real provider outside CI.

Run:

```powershell
.\scripts\run-benchmarks.ps1 -Provider fake
```

When a task depends on an example project, keep the original project read-only and let AgentGuard create an isolated workspace under `runs/`.

## Pull Requests

Before opening a PR:

1. Explain the user-visible behavior change.
2. State whether the change affects security boundaries, CLI JSON, reports, Skill/Plugin behavior, or provider behavior.
3. Add focused tests for new behavior.
4. Run `ctest --test-dir build\vs2022-debug -C Debug --output-on-failure`.
5. For demo or workflow changes, run `.\examples\run-library-demo.ps1 -Provider fake`.

CI must not call real LLM APIs.

## Code Style

- Use C++20 and existing project conventions.
- Keep platform safety checks explicit and easy to audit.
- Prefer structured JSON parsing over ad hoc string parsing.
- Keep CLI `--json` output stable for agent integration.
- Avoid broad refactors unless they directly support the change.
- Redact secrets before writing reports or logs.

## Adding Tests

- Prefer focused GoogleTest cases for C++ behavior.
- Use temporary directories for filesystem tests.
- Keep original target projects read-only; write only to test temp directories or isolated workspaces.
- Add CLI JSON tests for new agent-facing commands or flags.
- Add regression tests for safety boundaries such as protected files, Git baseline isolation, and approval stops.

## Documentation Expectations

Keep documentation accurate and conservative. Do not describe AgentGuardVS-CXX as a replacement for developers, IDEs, test suites, or code review. It is an LLM-assisted reliability and audit layer for Visual Studio C++ workflows.
