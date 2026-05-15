[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Solution,

    [Parameter(Mandatory = $true)]
    [string]$Task,

    [string]$Provider = $env:AGENTGUARD_LLM_PROVIDER,
    [string]$Model,
    [string]$RunsRoot,
    [string]$Configuration = 'Debug',
    [string]$Platform = 'x64',
    [switch]$Force,
    [string]$AgentGuardExe
)

$ErrorActionPreference = 'Stop'
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

$analyzeArgs = @{
    Solution = $Solution
    Task = $Task
}
if (-not [string]::IsNullOrWhiteSpace($Provider)) { $analyzeArgs.Provider = $Provider }
if (-not [string]::IsNullOrWhiteSpace($Model)) { $analyzeArgs.Model = $Model }
if (-not [string]::IsNullOrWhiteSpace($RunsRoot)) { $analyzeArgs.RunsRoot = $RunsRoot }
if (-not [string]::IsNullOrWhiteSpace($AgentGuardExe)) { $analyzeArgs.AgentGuardExe = $AgentGuardExe }
if ($Force) { $analyzeArgs.Force = $true }

$analyzeRaw = & (Join-Path $scriptRoot 'run-analyze.ps1') @analyzeArgs
$analyzeExit = $LASTEXITCODE
if ($analyzeExit -ne 0) {
    $analyzeRaw
    exit $analyzeExit
}
$analyze = $analyzeRaw | ConvertFrom-Json

$verifyRaw = & (Join-Path $scriptRoot 'run-verify.ps1') `
    -Workspace $analyze.workspace `
    -Configuration $Configuration `
    -Platform $Platform `
    -AgentGuardExe $AgentGuardExe
$verifyExit = $LASTEXITCODE

$reviewRaw = & (Join-Path $scriptRoot 'run-review.ps1') `
    -Workspace $analyze.workspace `
    -Task $Task `
    -Provider $Provider `
    -Model $Model `
    -AgentGuardExe $AgentGuardExe
$reviewExit = $LASTEXITCODE

[pscustomobject]@{
    ok = ($analyze.ok -and $verifyExit -eq 0 -and $reviewExit -eq 0)
    command = 'run-full-cycle'
    workspace = $analyze.workspace
    semantic_scope = $analyze.semantic_scope
    report = $analyze.report
    analyze = ($analyzeRaw | ConvertFrom-Json)
    verify = ($verifyRaw | ConvertFrom-Json)
    review = ($reviewRaw | ConvertFrom-Json)
} | ConvertTo-Json -Depth 20 -Compress

if ($verifyExit -ne 0) {
    exit $verifyExit
}
exit $reviewExit
