[CmdletBinding()]
param(
    [string]$AgentGuardExe,
    [string]$PluginRoot,
    [string]$SkillRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
}

function Resolve-CodexHome {
    if (-not [string]::IsNullOrWhiteSpace($env:CODEX_HOME)) {
        return $env:CODEX_HOME
    }
    return (Join-Path $HOME '.codex')
}

function Resolve-AgentGuardExe {
    param([string]$ExplicitPath, [string]$RepoRoot)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $candidates += $ExplicitPath
    }
    if (-not [string]::IsNullOrWhiteSpace($env:AGENTGUARD_EXE)) {
        $candidates += $env:AGENTGUARD_EXE
    }
    $candidates += @(
        (Join-Path $RepoRoot 'build\vs2022-release\Release\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\vs2022-debug\Debug\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\phase5-debug\Debug\AgentGuardVS.exe')
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw 'AgentGuardVS.exe was not found. Build first, pass -AgentGuardExe, or set AGENTGUARD_EXE.'
}

$repoRoot = Resolve-RepoRoot
$sourcePlugin = Join-Path $repoRoot 'plugins\agentguard-vs-cxx'
if (-not (Test-Path -LiteralPath $sourcePlugin -PathType Container)) {
    throw "Plugin source directory was not found: $sourcePlugin"
}

$resolvedExe = Resolve-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
if ([string]::IsNullOrWhiteSpace($PluginRoot)) {
    $PluginRoot = Join-Path (Resolve-CodexHome) 'plugins'
}

$destinationPlugin = Join-Path $PluginRoot 'agentguard-vs-cxx'
if (Test-Path -LiteralPath $destinationPlugin) {
    if (-not $Force) {
        throw "Plugin already exists at $destinationPlugin. Re-run with -Force to overwrite it."
    }
    Remove-Item -LiteralPath $destinationPlugin -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PluginRoot | Out-Null
Copy-Item -LiteralPath $sourcePlugin -Destination $destinationPlugin -Recurse -Force

[pscustomobject]@{
    ok = $true
    plugin = $destinationPlugin
    agentguard_exe = $resolvedExe
    next_steps = @(
        "Set AGENTGUARD_EXE to $resolvedExe in your shell or profile.",
        "Restart Codex so it can discover the plugin.",
        "Use @AgentGuardVS-CXX or /skills to invoke agentguard-vs-cxx.",
        "For GitHub distribution, add this repository as a local marketplace/plugin source; it is not claiming official Plugin Directory availability."
    )
} | ConvertTo-Json -Depth 5
