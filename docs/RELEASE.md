# Release Guide

AgentGuardVS-CXX release packages target Windows machines with Visual Studio 2022 or Build Tools installed. Release validation uses only offline providers (`fake` or `file`) and must not require real LLM API keys.

## Tag A Release

Use a clean working tree and a version tag:

```powershell
git status --short
git tag v0.1.0
git push origin v0.1.0
```

## Build And Test

```powershell
cmake -S . -B build\vs2022-release -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-release --config Release
ctest --test-dir build\vs2022-release -C Release --output-on-failure
```

Do not set `OPENAI_API_KEY`, `DEEPSEEK_API_KEY`, or `ANTHROPIC_API_KEY` in CI release validation. Use:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "fake"
```

## Generate Release Zip

```powershell
$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-release\Release\AgentGuardVS.exe").Path
.\scripts\package-release.ps1 -Version "0.1.0"
```

The zip is written under `dist\` by default.

## Validate The Package

Extract the zip into a clean directory, then run:

```powershell
.\scripts\doctor.ps1
.\examples\run-library-demo.ps1 -Provider fake
.\AgentGuardVS.exe --version
```

Expected result:

- The demo builds `examples/library-system`.
- The demo self-test passes.
- AgentGuard analyze, verify, and review complete with an offline provider.
- Reports are written under `runs/library-demo`.
- No real API key is required.

## Package Contents

The release package contains:

- `AgentGuardVS.exe`
- `skills/agentguard-vs-cxx`
- `plugins/agentguard-vs-cxx` when present
- `docs`
- `examples`
- `scripts`
- `README.md`
- `LICENSE`
- `.env.example`

The package must not include local `build`, `.vs`, `runs`, `x64`, `Win32`, raw local test artifacts, or real secrets.

## Supported Platforms

Current support is focused on:

- Windows 11 or supported Windows development machines.
- Visual Studio 2022 or Visual Studio Build Tools 2022.
- MSBuild-based Visual Studio C++ solutions.
- PowerShell 5.1 or newer.

Other IDEs, operating systems, and build systems are future work, not current release guarantees.
