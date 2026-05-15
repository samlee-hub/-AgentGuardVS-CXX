# Install AgentGuardVS-CXX

This guide describes a local Windows setup for AgentGuardVS-CXX.

## Requirements

- Windows 11 or a compatible Windows development machine.
- Visual Studio 2022 or Visual Studio Build Tools 2022.
- C++ workload and MSBuild components.
- Git.
- PowerShell 5.1 or newer.
- CMake 3.21+ if building from source.

## Visual Studio And MSBuild

Install one of:

- Visual Studio 2022 Community, Professional, or Enterprise with "Desktop development with C++".
- Visual Studio Build Tools 2022 with the C++ build tools workload.

Confirm MSBuild is available:

```powershell
where.exe MSBuild.exe
```

If `MSBuild.exe` is not on `PATH`, AgentGuardVS-CXX and the demo scripts try to locate it through Visual Studio Installer metadata.

## Git

Git is required because AgentGuardVS-CXX creates an independent baseline inside every isolated workspace.

```powershell
git --version
```

If Git is missing, workspace creation or diff reporting fails with a clear Git baseline error.

## Build From Source

```powershell
cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug
ctest --test-dir build\vs2022-debug -C Debug --output-on-failure
```

Set the executable path for wrapper scripts:

```powershell
$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
```

To persist it:

```powershell
setx AGENTGUARD_EXE "<path-to-AgentGuardVS.exe>"
```

## Provider Configuration

The offline demo uses the `file` provider and does not need a real API key.

For real semantic analysis and review, choose one provider and set only the variables you need:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "openai"
$env:AGENTGUARD_OPENAI_MODEL = "<model>"
$env:OPENAI_API_KEY = "<api-key>"
```

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "deepseek"
$env:AGENTGUARD_DEEPSEEK_MODEL = "<model>"
$env:DEEPSEEK_API_KEY = "<api-key>"
```

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "claude"
$env:AGENTGUARD_CLAUDE_MODEL = "<model>"
$env:ANTHROPIC_API_KEY = "<api-key>"
```

Never commit API keys. Real provider calls should be used for manual smoke tests and real workflows, not unit tests.

## Install The Codex Skill

```powershell
.\scripts\install-skill.ps1 -AgentGuardExe $env:AGENTGUARD_EXE -Force
```

Default install target:

```text
$HOME\.agents\skills\agentguard-vs-cxx
```

You can override it:

```powershell
.\scripts\install-skill.ps1 -AgentGuardExe $env:AGENTGUARD_EXE -SkillRoot "<custom-skill-root>" -Force
```

Open a new Codex session and use `/skills` to confirm `agentguard-vs-cxx` is visible.

## Run The Demo

```powershell
.\examples\run-library-demo.ps1
```

The script builds and self-tests `examples/library-system`, runs AgentGuard analyze, edits only the isolated workspace, runs verify, runs review, and prints the report paths.
