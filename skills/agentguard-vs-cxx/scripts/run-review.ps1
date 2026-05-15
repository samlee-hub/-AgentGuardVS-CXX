[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Workspace,

    [Parameter(Mandatory = $true)]
    [string]$Task,

    [string]$Provider = $env:AGENTGUARD_LLM_PROVIDER,
    [string]$Model,
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

if ([string]::IsNullOrWhiteSpace($Provider)) {
    $Provider = 'fake'
}

$arguments = @('review', '--workspace', $Workspace, '--task', $Task, '--provider', $Provider, '--json')
if (-not [string]::IsNullOrWhiteSpace($Model)) {
    $arguments += @('--model', $Model)
}

$output = & $AgentGuardExe @arguments
$exitCode = $LASTEXITCODE
$output
exit $exitCode
