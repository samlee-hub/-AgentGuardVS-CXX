[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Solution,

    [Parameter(Mandatory = $true)]
    [string]$Task,

    [string]$Provider = $env:AGENTGUARD_LLM_PROVIDER,
    [string]$Model,
    [string]$RunsRoot,
    [switch]$Force,
    [switch]$NoCache,
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

$arguments = @('analyze', '--solution', $Solution, '--task', $Task, '--provider', $Provider, '--json')
if (-not [string]::IsNullOrWhiteSpace($Model)) {
    $arguments += @('--model', $Model)
}
if (-not [string]::IsNullOrWhiteSpace($RunsRoot)) {
    $arguments += @('--runs-root', $RunsRoot)
}
if ($Force) {
    $arguments += '--force'
}
if ($NoCache) {
    $arguments += '--no-cache'
}

$output = & $AgentGuardExe @arguments
$exitCode = $LASTEXITCODE
$output
exit $exitCode
