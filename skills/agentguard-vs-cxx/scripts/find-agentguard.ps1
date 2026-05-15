[CmdletBinding()]
param(
    [string]$RepoRoot
)

$ErrorActionPreference = 'Stop'

function Write-JsonError {
    param(
        [string]$Code,
        [string]$Message,
        [string]$Suggestion
    )

    [pscustomobject]@{
        ok = $false
        error_code = $Code
        message = $Message
        suggestion = $Suggestion
    } | ConvertTo-Json -Compress
}

function Test-AgentGuardExe {
    param([string]$Path)
    return -not [string]::IsNullOrWhiteSpace($Path) -and (Test-Path -LiteralPath $Path -PathType Leaf)
}

if (Test-AgentGuardExe $env:AGENTGUARD_EXE) {
    (Resolve-Path -LiteralPath $env:AGENTGUARD_EXE).Path
    exit 0
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$skillRoot = Split-Path -Parent $scriptRoot
$candidateRepoRoots = @()

if (-not [string]::IsNullOrWhiteSpace($RepoRoot)) {
    $candidateRepoRoots += $RepoRoot
}

$candidateRepoRoots += (Resolve-Path -LiteralPath (Join-Path $skillRoot '..\..') -ErrorAction SilentlyContinue | ForEach-Object { $_.Path })
$candidateRepoRoots += (Get-Location).Path

$relativeCandidates = @(
    'build\vs2022-debug\Debug\AgentGuardVS.exe',
    'build\vs2022-release\Release\AgentGuardVS.exe',
    'build\Debug\AgentGuardVS.exe',
    'build\Release\AgentGuardVS.exe'
)

foreach ($root in $candidateRepoRoots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique) {
    foreach ($relative in $relativeCandidates) {
        $candidate = Join-Path $root $relative
        if (Test-AgentGuardExe $candidate) {
            (Resolve-Path -LiteralPath $candidate).Path
            exit 0
        }
    }
}

$pathCommand = Get-Command AgentGuardVS.exe -ErrorAction SilentlyContinue
if ($pathCommand -and (Test-AgentGuardExe $pathCommand.Source)) {
    $pathCommand.Source
    exit 0
}

Write-JsonError `
    -Code 'AGENTGUARD_EXE_MISSING' `
    -Message 'AgentGuardVS.exe was not found.' `
    -Suggestion 'Build AgentGuardVS-CXX and set AGENTGUARD_EXE to the full path of AgentGuardVS.exe.'
exit 1
