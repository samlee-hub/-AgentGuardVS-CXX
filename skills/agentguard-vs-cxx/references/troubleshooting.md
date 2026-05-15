# Troubleshooting

Use this reference when AgentGuardVS-CXX or its wrapper scripts return an error.

## AGENTGUARD_EXE is not set

Set `AGENTGUARD_EXE` to the built `AgentGuardVS.exe`, or pass `-AgentGuardExe` to scripts that support it.

```powershell
$env:AGENTGUARD_EXE = "<repo>\\build\\vs2022-debug\\Debug\\AgentGuardVS.exe"
```

Do not hardcode a personal machine path in committed files.

## MSBuild not found

Install Visual Studio 2022 or Visual Studio Build Tools with C++ build tools. Verify MSBuild is available through Visual Studio Installer, `vswhere`, common Build Tools paths, or `PATH`.

## API key missing

Set the provider key only in environment variables:

- `OPENAI_API_KEY`
- `DEEPSEEK_API_KEY`
- `ANTHROPIC_API_KEY`

Do not write API keys into Skill files, scripts, prompts, logs, reports, or commits.

## Model missing or unavailable

Set the provider model variable:

- `AGENTGUARD_OPENAI_MODEL`
- `AGENTGUARD_DEEPSEEK_MODEL`
- `AGENTGUARD_CLAUDE_MODEL`

If the provider rejects the model, switch to a model available to the configured account.

## Git baseline failed

AgentGuardVS-CXX needs Git to initialize an isolated workspace baseline. Install Git for Windows and ensure `git.exe` is available from `PATH`.

If a workspace is inside another Git repository, the isolated repo must still have its own `.git` and `git_top_level` must equal the workspace repo path.

## protected file conflict

If a patch, build error, or review suggestion touches `protected_files`, stop. Do not ask for an override unless the user explicitly changes the task policy.

## approval required

If `needs_approval_files` appear in review output, stop and ask the user before editing. Record the user decision in the final report.

## Invalid LLM JSON

Read the raw response artifact and prompt artifact in the workspace. Do not continue automatic repair until the JSON problem is resolved.
