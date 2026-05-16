# Install AgentGuardVS-CXX

This guide is for Windows developers who want to run AgentGuardVS-CXX locally, either from source or from a release package.

## Requirements

- Windows 11 or another supported Windows development machine.
- PowerShell 5.1 or newer.
- Git for Windows.
- Visual Studio 2022 or Visual Studio Build Tools 2022.
- The "Desktop development with C++" workload, including MSBuild and the C++ toolchain.
- CMake 3.21+ only when building from source.

## Five-Minute Local Check

From the repository or extracted release directory:

```powershell
.\scripts\bootstrap.ps1
```

`bootstrap.ps1` checks Git, MSBuild, CMake, `AgentGuardVS.exe`, environment variables, and the Codex Skill install location. It does not write API keys or modify your machine.

If PowerShell blocks scripts, allow local scripts for the current user:

```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

## Check Git

AgentGuardVS-CXX needs Git because every isolated workspace gets its own baseline commit.

```powershell
git --version
where.exe git.exe
```

If Git is missing, install Git for Windows and reopen PowerShell.

## Check MSBuild

Install one of:

- Visual Studio 2022 Community, Professional, or Enterprise with "Desktop development with C++".
- Visual Studio Build Tools 2022 with the C++ build tools workload.

Check whether MSBuild is already visible:

```powershell
where.exe MSBuild.exe
```

If it is not on `PATH`, AgentGuardVS-CXX also tries Visual Studio Installer metadata through `vswhere.exe`.

## Build From Source

Use this route when cloning the repository:

```powershell
git clone https://github.com/samlee-hub/-AgentGuardVS-CXX.git
cd -AgentGuardVS-CXX

cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug
```

Run tests when `ctest.exe` is available:

```powershell
ctest --test-dir build\vs2022-debug -C Debug --output-on-failure
```

If `ctest.exe` is not on `PATH`, open a Developer PowerShell for Visual Studio or use the CMake bundled with Visual Studio.

## Set AGENTGUARD_EXE

For the current PowerShell session:

```powershell
$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
```

Persist it for future shells:

```powershell
setx AGENTGUARD_EXE "<path-to-AgentGuardVS.exe>"
```

The one-step installer also sets this user-level variable:

```powershell
.\scripts\install.ps1
```

## Provider Configuration

Use `fake` when you want an offline run with no API key:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "fake"
```

Use `file` when replaying checked-in sample provider responses:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "file"
$env:AGENTGUARD_LLM_RESPONSE_FILE = "<path-to-sample-response.json>"
```

Use a real provider only when you need live semantic analysis or review:

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "openai"
$env:AGENTGUARD_OPENAI_MODEL = "<model>"
$env:OPENAI_API_KEY = "<openai-api-key>"
```

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "deepseek"
$env:AGENTGUARD_DEEPSEEK_MODEL = "<model>"
$env:DEEPSEEK_API_KEY = "<deepseek-api-key>"
```

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "claude"
$env:AGENTGUARD_CLAUDE_MODEL = "<model>"
$env:ANTHROPIC_API_KEY = "<anthropic-api-key>"
```

Do not commit real API keys. Use `scripts/setup-env.example.ps1` as a placeholder reference only.

## Install The Codex Skill

After `AGENTGUARD_EXE` points to a built or downloaded executable:

```powershell
.\scripts\install-plugin.ps1 -Force
```

Default install target:

```text
$HOME\.agents\skills\agentguard-vs-cxx
```

Override the install root when needed:

```powershell
.\scripts\install-plugin.ps1 -SkillRoot "<custom-skill-root>" -Force
```

Open a new Codex session and use `/skills` to confirm `agentguard-vs-cxx` is visible.

## Run The Demo Without API Keys

The fake provider route requires no network access or API key:

```powershell
.\examples\run-library-demo.ps1 -Provider fake
```

The file provider route replays checked-in sample responses:

```powershell
.\examples\run-library-demo.ps1 -Provider file
```

Both routes keep the original demo project read-only for code edits. AgentGuard creates an isolated workspace under `runs\...`, applies the deterministic demo edit there, builds that workspace, runs semantic review, and prints report paths.

## Diagnose An Install

```powershell
.\scripts\doctor.ps1
```

`doctor.ps1` checks `AgentGuardVS.exe`, Git, MSBuild, provider environment variables, Codex Skill installation, and a fake-provider analyze path.

## Uninstall Local Integration

```powershell
.\scripts\uninstall.ps1
```

The uninstall script removes the installed Skill only after confirmation. It does not delete user projects, run history, or API key environment variables.
