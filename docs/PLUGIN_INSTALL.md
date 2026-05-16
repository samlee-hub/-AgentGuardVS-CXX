# Plugin Installation

AgentGuardVS-CXX is distributed as a GitHub-hosted Codex Plugin/Skill package. It is not claiming availability in an official searchable Plugin Directory. Use GitHub clone plus local plugin installation or a local marketplace entry.

## From GitHub Clone To Enabled Plugin

```powershell
git clone https://github.com/samlee-hub/-AgentGuardVS-CXX.git
cd -AgentGuardVS-CXX

cmake -S . -B build\vs2022-debug -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022-debug --config Debug --target AgentGuardVS

$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
.\scripts\install-plugin.ps1 -AgentGuardExe $env:AGENTGUARD_EXE -Force
```

Restart Codex, then invoke:

```text
@AgentGuardVS-CXX Use AgentGuardVS-CXX to safely modify this Visual Studio C++ project.
```

You can also inspect `/skills` and use `agentguard-vs-cxx` directly when the host exposes Skill discovery.

## Local Development Install

Use `CODEX_HOME` to control the local plugin root without hardcoding a user path:

```powershell
$env:CODEX_HOME = "$HOME\.codex"
$env:AGENTGUARD_EXE = (Resolve-Path ".\build\vs2022-debug\Debug\AgentGuardVS.exe").Path
.\scripts\install-plugin.ps1 -PluginRoot (Join-Path $env:CODEX_HOME "plugins") -AgentGuardExe $env:AGENTGUARD_EXE -Force
```

The installed plugin contains:

```text
plugins/agentguard-vs-cxx/
  .codex-plugin/plugin.json
  skills/agentguard-vs-cxx/SKILL.md
  skills/agentguard-vs-cxx/scripts/
  skills/agentguard-vs-cxx/references/
  assets/
```

## Set AGENTGUARD_EXE

The wrapper scripts first read `AGENTGUARD_EXE`:

```powershell
$env:AGENTGUARD_EXE = "C:\path\to\AgentGuardVS.exe"
```

If `AGENTGUARD_EXE` is missing, `find-agentguard.ps1` searches common build locations relative to the repository and then `PATH`.

## Set API Keys

Use environment variables only. Do not write API keys into repository files, Skill files, plugin files, reports, logs, prompts, raw responses, or cache files.

```powershell
$env:AGENTGUARD_LLM_PROVIDER = "fake" # fake, file, openai, deepseek, or claude
$env:AGENTGUARD_OPENAI_MODEL = ""
$env:AGENTGUARD_DEEPSEEK_MODEL = ""
$env:AGENTGUARD_CLAUDE_MODEL = ""

# Set only when using that provider:
$env:OPENAI_API_KEY = "<openai-api-key>"
$env:DEEPSEEK_API_KEY = "<deepseek-api-key>"
$env:ANTHROPIC_API_KEY = "<anthropic-api-key>"
```

For CI and demos, prefer `fake` or `file`.

## Enable In Codex

After installation:

1. Restart Codex.
2. Check `/skills` if your Codex host exposes it.
3. Invoke `@AgentGuardVS-CXX` or ask for `agentguard-vs-cxx`.
4. Confirm the Skill calls `run-analyze.ps1`, edits only `allowed_files`, then calls `run-verify.ps1` and `run-review.ps1`.

## Common Errors

- `AGENTGUARD_EXE_MISSING`: build the project and set `AGENTGUARD_EXE`.
- `Workspace already exists and force is false`: rerun analyze with `-Force` only if replacing that isolated run is intended.
- `API_KEY_MISSING`: switch to `fake` or set the provider key through an environment variable.
- `INVALID_LLM_JSON`: inspect `artifacts/prompts` and `artifacts/raw_responses`; do not blindly retry.
- `MSBUILD_NOT_FOUND`: install Visual Studio 2022 or Build Tools with C++ workload.
