[CmdletBinding()]
param(
    [string]$AgentGuardExe,
    [string]$SkillRoot,
    [switch]$SkipFakeDemo,
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

function Test-FakeAnalyze {
    param([string]$Exe, [string]$RepoRoot)

    $solution = Join-Path $RepoRoot 'examples\library-system\Library.sln'
    $task = Join-Path $RepoRoot 'examples\tasks\add-case-insensitive-title-search.md'
    if (-not (Test-Path -LiteralPath $solution -PathType Leaf) -or
        -not (Test-Path -LiteralPath $task -PathType Leaf)) {
        return 'demo files missing'
    }

    $runsRoot = Join-Path $RepoRoot 'runs\doctor-fake'
    $taskText = Get-Content -LiteralPath $task -Raw
    $output = & $Exe analyze --solution $solution --task $taskText --provider fake --runs-root $runsRoot --force --json 2>&1
    $exit = $LASTEXITCODE
    $text = ($output | Out-String).Trim()
    if ($exit -ne 0) {
        return "failed: $text"
    }
    try {
        $json = $text | ConvertFrom-Json
        if ($json.ok) {
            return "ok: workspace=$($json.workspace)"
        }
        return "failed: $text"
    }
    catch {
        return "failed: non-json output $text"
    }
}

$repoRoot = Resolve-RepoRoot
if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
    $SkillRoot = Join-Path $HOME '.agents\skills'
}

$checks = [System.Collections.Generic.List[object]]::new()
$exe = Find-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot

$osStatus = if ($PSVersionTable.OS) { $PSVersionTable.OS } else { [Environment]::OSVersion.VersionString }
Add-Check $checks 'windows' ($PSVersionTable.PSEdition -eq 'Desktop' -or $IsWindows) $osStatus 'Run on Windows 11 or a compatible Windows development machine.'
Add-Check $checks 'powershell' ($PSVersionTable.PSVersion.Major -ge 5) $PSVersionTable.PSVersion.ToString() 'Use PowerShell 5.1 or newer.'

$git = Get-Command git.exe -ErrorAction SilentlyContinue
Add-Check $checks 'git' ([bool]$git) ($(if ($git) { $git.Source } else { 'not found' })) 'Install Git for Windows.'

$msbuild = Find-MSBuild
Add-Check $checks 'msbuild' (-not [string]::IsNullOrWhiteSpace($msbuild)) ($(if ($msbuild) { $msbuild } else { 'not found' })) 'Install Visual Studio 2022 or Build Tools with C++ workload.'

Add-Check $checks 'agentguard-exe' (-not [string]::IsNullOrWhiteSpace($exe)) ($(if ($exe) { $exe } else { 'not found' })) 'Build from source, download a release package, or set AGENTGUARD_EXE.'

$provider = if ($env:AGENTGUARD_LLM_PROVIDER) { $env:AGENTGUARD_LLM_PROVIDER } else { 'fake (default)' }
Add-Check $checks 'llm-provider' $true $provider 'Use fake for offline checks, or configure a real provider.'

$providerKeyStatus = switch ($env:AGENTGUARD_LLM_PROVIDER) {
    'openai' { if ($env:OPENAI_API_KEY -and $env:AGENTGUARD_OPENAI_MODEL) { 'configured' } else { 'missing OPENAI_API_KEY or AGENTGUARD_OPENAI_MODEL' } }
    'deepseek' { if ($env:DEEPSEEK_API_KEY -and $env:AGENTGUARD_DEEPSEEK_MODEL) { 'configured' } else { 'missing DEEPSEEK_API_KEY or AGENTGUARD_DEEPSEEK_MODEL' } }
    'claude' { if ($env:ANTHROPIC_API_KEY -and $env:AGENTGUARD_CLAUDE_MODEL) { 'configured' } else { 'missing ANTHROPIC_API_KEY or AGENTGUARD_CLAUDE_MODEL' } }
    default { 'not required for fake provider' }
}
Add-Check $checks 'llm-credentials' ($providerKeyStatus -notlike 'missing*') $providerKeyStatus 'Set provider credentials in environment variables only.'

$installedSkill = Join-Path $SkillRoot 'agentguard-vs-cxx\SKILL.md'
Add-Check $checks 'codex-skill' (Test-Path -LiteralPath $installedSkill -PathType Leaf) ($(if (Test-Path -LiteralPath $installedSkill -PathType Leaf) { $installedSkill } else { 'not installed' })) 'Run .\scripts\install-plugin.ps1.'

if ($SkipFakeDemo) {
    Add-Check $checks 'fake-demo-analyze' $true 'skipped' 'Run doctor without -SkipFakeDemo to execute fake analyze.'
}
elseif ([string]::IsNullOrWhiteSpace($exe)) {
    Add-Check $checks 'fake-demo-analyze' $false 'skipped: AgentGuardVS.exe missing' 'Build or install AgentGuardVS.exe first.'
}
else {
    $fakeResult = Test-FakeAnalyze -Exe $exe -RepoRoot $repoRoot
    Add-Check $checks 'fake-demo-analyze' ($fakeResult -like 'ok:*') $fakeResult 'Check examples/library-system and AgentGuardVS.exe.'
}

$ok = -not ($checks | Where-Object { -not $_.ok })
$summary = [pscustomobject]@{
    ok = $ok
    repo = $repoRoot
    checks = $checks
}

if ($Json) {
    $summary | ConvertTo-Json -Depth 5
}
else {
    Write-Host 'AgentGuardVS-CXX doctor'
    $checks | Format-Table name, ok, status, fix -AutoSize
}

if (-not $ok) {
    exit 1
}
