[CmdletBinding()]
param(
    [string]$AgentGuardExe,
    [string]$SkillRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')
$sourceSkill = Join-Path $repoRoot 'skills\agentguard-vs-cxx'

if (-not (Test-Path -LiteralPath $sourceSkill -PathType Container)) {
    Write-Error "Skill source directory was not found: $sourceSkill"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
    $SkillRoot = Join-Path $HOME '.agents\skills'
}

$destinationSkill = Join-Path $SkillRoot 'agentguard-vs-cxx'

if ([string]::IsNullOrWhiteSpace($AgentGuardExe)) {
    if ([string]::IsNullOrWhiteSpace($env:AGENTGUARD_EXE)) {
        Write-Error 'AgentGuardVS.exe was not provided. Pass -AgentGuardExe or set AGENTGUARD_EXE.'
        exit 1
    }
    $AgentGuardExe = $env:AGENTGUARD_EXE
}

if (-not (Test-Path -LiteralPath $AgentGuardExe -PathType Leaf)) {
    Write-Error "AgentGuardVS.exe was not found: $AgentGuardExe"
    exit 1
}

if (Test-Path -LiteralPath $destinationSkill) {
    if (-not $Force) {
        Write-Error "Skill already exists at $destinationSkill. Re-run with -Force to overwrite it."
        exit 1
    }
    Remove-Item -LiteralPath $destinationSkill -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $destinationSkill | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceSkill 'SKILL.md') -Destination $destinationSkill -Force
Copy-Item -LiteralPath (Join-Path $sourceSkill 'scripts') -Destination $destinationSkill -Recurse -Force
Copy-Item -LiteralPath (Join-Path $sourceSkill 'references') -Destination $destinationSkill -Recurse -Force

$configSample = @"
# AgentGuardVS-CXX local configuration sample.
# Do not write API keys into this file if you plan to commit it.
`$env:AGENTGUARD_EXE="$AgentGuardExe"
`$env:AGENTGUARD_LLM_PROVIDER="fake"
`$env:AGENTGUARD_OPENAI_MODEL=""
`$env:AGENTGUARD_DEEPSEEK_MODEL=""
`$env:AGENTGUARD_CLAUDE_MODEL=""
"@

$configSample | Set-Content -LiteralPath (Join-Path $destinationSkill 'local-config.sample.ps1') -Encoding UTF8

[pscustomobject]@{
    ok = $true
    skill = $destinationSkill
    agentguard_exe = (Resolve-Path -LiteralPath $AgentGuardExe).Path
    next_steps = @(
        "Confirm SKILL.md exists.",
        "Run scripts\\find-agentguard.ps1.",
        "Set provider API keys only through environment variables when using real LLM providers."
    )
} | ConvertTo-Json -Depth 5
