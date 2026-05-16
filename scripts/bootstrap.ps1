[CmdletBinding()]
param(
    [string]$AgentGuardExe,
    [string]$SkillRoot,
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

function Add-Check {
    param(
        [System.Collections.Generic.List[object]]$Checks,
        [string]$Name,
        [bool]$Ok,
        [string]$Status,
        [string]$Fix
    )

    $Checks.Add([pscustomobject]@{
        name = $Name
        ok = $Ok
        status = $Status
        fix = $Fix
    }) | Out-Null
}

function Resolve-RepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
}

function Find-AgentGuardExe {
    param([string]$ExplicitPath, [string]$RepoRoot)

    $candidates = @()
    if ($ExplicitPath) {
        $candidates += $ExplicitPath
    }
    if ($env:AGENTGUARD_EXE) {
        $candidates += $env:AGENTGUARD_EXE
    }
    $candidates += @(
        (Join-Path $RepoRoot 'AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'release\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\vs2022-debug\Debug\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\vs2022-release\Release\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\Debug\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\Release\AgentGuardVS.exe')
    )

    foreach ($candidate in $candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $command = Get-Command AgentGuardVS.exe -ErrorAction SilentlyContinue
    if ($command -and (Test-Path -LiteralPath $command.Source -PathType Leaf)) {
        return $command.Source
    }

    $aliasCommand = Get-Command agentguard -ErrorAction SilentlyContinue
    if ($aliasCommand) {
        return $aliasCommand.Source
    }

    return ''
}

function Find-MSBuild {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
            Select-Object -First 1
        if ($found -and (Test-Path -LiteralPath $found -PathType Leaf)) {
            return $found
        }
    }

    return ''
}

function Test-EnvPresent {
    param([string]$Name)
    return -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($Name, 'Process')) -or
           -not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($Name, 'User'))
}

$repoRoot = Resolve-RepoRoot
if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
    $SkillRoot = Join-Path $HOME '.agents\skills'
}

$checks = [System.Collections.Generic.List[object]]::new()

$git = Get-Command git.exe -ErrorAction SilentlyContinue
Add-Check $checks 'git' ([bool]$git) ($(if ($git) { $git.Source } else { 'not found' })) 'Install Git for Windows and reopen PowerShell.'

$msbuild = Find-MSBuild
Add-Check $checks 'msbuild' (-not [string]::IsNullOrWhiteSpace($msbuild)) ($(if ($msbuild) { $msbuild } else { 'not found' })) 'Install Visual Studio 2022 or Build Tools with the C++ workload.'

$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
Add-Check $checks 'cmake' $true ($(if ($cmake) { $cmake.Source } else { 'not found; optional unless building from source' })) 'Install CMake 3.21+ only if you plan to build from source.'

$exe = Find-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
Add-Check $checks 'agentguard-exe' (-not [string]::IsNullOrWhiteSpace($exe)) ($(if ($exe) { $exe } else { 'not found' })) 'Build the project or set AGENTGUARD_EXE to AgentGuardVS.exe.'

$provider = if ($env:AGENTGUARD_LLM_PROVIDER) { $env:AGENTGUARD_LLM_PROVIDER } else { 'fake (default)' }
Add-Check $checks 'llm-provider' $true $provider 'Use fake for offline checks, or set openai/deepseek/claude plus the matching model and API key.'

$apiConfigured = (Test-EnvPresent 'OPENAI_API_KEY') -or (Test-EnvPresent 'DEEPSEEK_API_KEY') -or (Test-EnvPresent 'ANTHROPIC_API_KEY')
Add-Check $checks 'api-keys' $true ($(if ($apiConfigured) { 'one or more provider keys configured' } else { 'not required for fake provider' })) 'Set API keys only in environment variables; never commit them.'

$sourceSkill = Join-Path $repoRoot 'skills\agentguard-vs-cxx\SKILL.md'
$installedSkill = Join-Path $SkillRoot 'agentguard-vs-cxx\SKILL.md'
Add-Check $checks 'skill-source' (Test-Path -LiteralPath $sourceSkill -PathType Leaf) $sourceSkill 'Keep skills/agentguard-vs-cxx in source and release packages.'
Add-Check $checks 'skill-installed' (Test-Path -LiteralPath $installedSkill -PathType Leaf) ($(if (Test-Path -LiteralPath $installedSkill -PathType Leaf) { $installedSkill } else { 'not installed' })) 'Run .\scripts\install-plugin.ps1 after building AgentGuardVS.exe.'

$executionPolicy = Get-ExecutionPolicy -Scope CurrentUser
Add-Check $checks 'powershell-policy' $true "CurrentUser=$executionPolicy" 'If scripts are blocked, run: Set-ExecutionPolicy -Scope CurrentUser RemoteSigned'

$ok = -not ($checks | Where-Object { -not $_.ok })
$summary = [pscustomobject]@{
    ok = $ok
    repo = $repoRoot
    checks = $checks
    next_steps = @(
        'Build from source if AgentGuardVS.exe is missing.',
        'Run .\scripts\install-plugin.ps1 after AGENTGUARD_EXE is available.',
        'Run .\examples\run-library-demo.ps1 -Provider fake to exercise the offline demo.'
    )
}

if ($Json) {
    $summary | ConvertTo-Json -Depth 5
}
else {
    Write-Host 'AgentGuardVS-CXX bootstrap check'
    $checks | Format-Table name, ok, status, fix -AutoSize
    Write-Host ''
    Write-Host 'Next steps:'
    foreach ($step in $summary.next_steps) {
        Write-Host "  - $step"
    }
}

if (-not $ok) {
    exit 1
}
