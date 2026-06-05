[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Workspace,

    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [string]$AgentGuardExe
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if ([string]::IsNullOrWhiteSpace($AgentGuardExe)) {
    $AgentGuardExe = & (Join-Path $scriptRoot 'find-agentguard.ps1')
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$output = & $AgentGuardExe verify `
    --workspace $Workspace `
    --configuration $Configuration `
    --platform $Platform `
    --json
$exitCode = $LASTEXITCODE
$output
exit $exitCode
