[CmdletBinding()]
param(
    [string]$AgentGuardExe,
    [string]$SkillRoot,
    [switch]$Force,
    [switch]$NoPathUpdate
)

$ErrorActionPreference = 'Stop'

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

function Add-UserPath {
    param([string]$Directory)

    $current = [Environment]::GetEnvironmentVariable('Path', 'User')
    $parts = @()
    if ($current) {
        $parts = $current -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    }
    if ($parts -notcontains $Directory) {
        $newPath = (@($parts) + $Directory) -join ';'
        [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
        return $true
    }
    return $false
}

function Write-AgentGuardShim {
    param([string]$Exe)

    $binRoot = Join-Path $env:LOCALAPPDATA 'AgentGuardVS-CXX\bin'
    New-Item -ItemType Directory -Force -Path $binRoot | Out-Null

    $cmdPath = Join-Path $binRoot 'agentguard.cmd'
    $cmd = @"
@echo off
"$Exe" %*
"@
    Set-Content -LiteralPath $cmdPath -Value $cmd -Encoding ASCII

    $ps1Path = Join-Path $binRoot 'agentguard.ps1'
    $ps1 = @"
& '$Exe' @args
exit `$LASTEXITCODE
"@
    Set-Content -LiteralPath $ps1Path -Value $ps1 -Encoding ASCII

    return $binRoot
}

$repoRoot = Resolve-RepoRoot
$isWindows = if (Get-Variable IsWindows -ErrorAction SilentlyContinue) { $IsWindows } else { $true }
if (-not $isWindows) {
    throw 'AgentGuardVS-CXX install.ps1 requires Windows.'
}

$git = Get-Command git.exe -ErrorAction SilentlyContinue
if (-not $git) {
    throw 'Git was not found. Install Git for Windows and reopen PowerShell.'
}

$msbuild = Find-MSBuild
if (-not $msbuild) {
    throw 'MSBuild was not found. Install Visual Studio 2022 or Build Tools with the C++ workload.'
}

$exe = Find-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
if ([string]::IsNullOrWhiteSpace($exe)) {
    throw 'AgentGuardVS.exe was not found. Build from source or download a release package, then rerun install.ps1 -AgentGuardExe <path>.'
}

[Environment]::SetEnvironmentVariable('AGENTGUARD_EXE', $exe, 'User')
$env:AGENTGUARD_EXE = $exe

$shimDirectory = Write-AgentGuardShim -Exe $exe
$pathUpdated = $false
if (-not $NoPathUpdate) {
    $pathUpdated = Add-UserPath -Directory $shimDirectory
}

$installPlugin = Join-Path $PSScriptRoot 'install-plugin.ps1'
if ($Force) {
    if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
        $pluginOutput = & $installPlugin -AgentGuardExe $exe -Force
    }
    else {
        $pluginOutput = & $installPlugin -AgentGuardExe $exe -SkillRoot $SkillRoot -Force
    }
}
else {
    if ([string]::IsNullOrWhiteSpace($SkillRoot)) {
        $pluginOutput = & $installPlugin -AgentGuardExe $exe
    }
    else {
        $pluginOutput = & $installPlugin -AgentGuardExe $exe -SkillRoot $SkillRoot
    }
}
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$pluginInstall = $null
$pluginText = ($pluginOutput | Out-String).Trim()
if ($pluginText) {
    $pluginInstall = $pluginText | ConvertFrom-Json
}

$version = & $exe --version
$previousProvider = $env:AGENTGUARD_LLM_PROVIDER
$env:AGENTGUARD_LLM_PROVIDER = 'fake'
$smoke = & $exe llm-smoke --input 'smoke' --json
if ($null -ne $previousProvider) {
    $env:AGENTGUARD_LLM_PROVIDER = $previousProvider
}
else {
    Remove-Item Env:\AGENTGUARD_LLM_PROVIDER -ErrorAction SilentlyContinue
}

[pscustomobject]@{
    ok = $true
    agentguard_exe = $exe
    user_agentguard_exe_set = $true
    shim_directory = $shimDirectory
    user_path_updated = $pathUpdated
    plugin_install = $pluginInstall
    version = ($version | Out-String).Trim()
    smoke = (($smoke | Out-String).Trim() | ConvertFrom-Json)
    next_steps = @(
        'Open a new PowerShell window so user PATH changes are visible.',
        'Run: agentguard --version',
        'Run: .\scripts\doctor.ps1',
        'Run: .\examples\run-library-demo.ps1 -Provider fake',
        'Set real provider API keys only through environment variables when needed.'
    )
} | ConvertTo-Json -Depth 6
