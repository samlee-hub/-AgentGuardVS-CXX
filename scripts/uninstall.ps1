[CmdletBinding()]
param(
    [string]$SkillRoot,
    [switch]$Force,
    [switch]$RemoveEnvironment,
    [switch]$RemovePathShim
)

$ErrorActionPreference = 'Stop'

function Confirm-Action {
    param([string]$Question)

    if ($Force) {
        return $true
    }

    $answer = Read-Host "$Question [y/N]"
    return $answer -eq 'y' -or $answer -eq 'Y'
}

if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
    $SkillRoot = Join-Path $HOME '.agents\skills'
}

$removed = [System.Collections.Generic.List[string]]::new()
$skillPath = Join-Path $SkillRoot 'agentguard-vs-cxx'
if (Test-Path -LiteralPath $skillPath) {
    if (Confirm-Action "Remove Codex Skill at $skillPath?") {
        Remove-Item -LiteralPath $skillPath -Recurse -Force
        $removed.Add($skillPath) | Out-Null
    }
}

$shimDirectory = Join-Path $env:LOCALAPPDATA 'AgentGuardVS-CXX\bin'
if ($RemovePathShim -and (Test-Path -LiteralPath $shimDirectory)) {
    Remove-Item -LiteralPath $shimDirectory -Recurse -Force
    $removed.Add($shimDirectory) | Out-Null
}

if ($RemoveEnvironment -or ((-not $Force) -and (Confirm-Action 'Remove the user-level AGENTGUARD_EXE environment variable?'))) {
    [Environment]::SetEnvironmentVariable('AGENTGUARD_EXE', $null, 'User')
    Remove-Item Env:\AGENTGUARD_EXE -ErrorAction SilentlyContinue
    $removed.Add('User environment variable AGENTGUARD_EXE') | Out-Null
}

[pscustomobject]@{
    ok = $true
    removed = $removed
    retained = @(
        'User projects were not deleted.',
        'runs history was not deleted.',
        'API key environment variables were not changed.'
    )
} | ConvertTo-Json -Depth 4
