[CmdletBinding()]
param(
    [ValidateSet("fake", "file", "openai", "deepseek", "claude")]
    [string]$Provider = "fake",
    [string]$Model = "",
    [string]$AgentGuardExe = "",
    [string]$TasksFile = "",
    [string]$AnalyzeResponseFile = "",
    [string]$ReviewResponseFile = "",
    [string]$RunsRoot = "",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    return (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-AgentGuardExe {
    param([string]$ExplicitPath, [string]$RepoRoot)

    $candidates = @()
    if ($ExplicitPath) { $candidates += $ExplicitPath }
    if ($env:AGENTGUARD_EXE) { $candidates += $env:AGENTGUARD_EXE }
    $candidates += @(
        (Join-Path $RepoRoot "build\vs2022-debug\Debug\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\vs2022-release\Release\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\Debug\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\Release\AgentGuardVS.exe")
    )

    foreach ($candidate in $candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "AgentGuardVS.exe was not found. Build the project first or pass -AgentGuardExe <path>."
}

function Invoke-AgentGuardJson {
    param([string]$Exe, [string[]]$Arguments)

    $output = & $Exe @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | Out-String).Trim()
    if (-not $text) {
        throw "AgentGuard returned no JSON output."
    }

    try {
        $json = $text | ConvertFrom-Json
    }
    catch {
        throw "AgentGuard returned non-JSON output: $text"
    }

    if ($exitCode -ne 0 -or ($json.PSObject.Properties.Name -contains "ok" -and -not $json.ok)) {
        $code = if ($json.error_code) { $json.error_code } else { "UNKNOWN" }
        $message = if ($json.message) { $json.message } else { $text }
        throw "AgentGuard failed with $code`: $message"
    }

    return $json
}

function Read-BenchmarkTasks {
    param([string]$Path)

    Get-Content -LiteralPath $Path |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ | ConvertFrom-Json }
}

function Test-ScopeFiles {
    param($ScopeJson, [string[]]$ExpectedFiles, [string[]]$ProtectedFiles)

    $allowed = @($ScopeJson.allowed_files | ForEach-Object { $_.path })
    $context = @($ScopeJson.context_files | ForEach-Object { $_.path })
    $suspected = @($ScopeJson.suspected_files | ForEach-Object { $_.path })
    $needsApproval = @($ScopeJson.needs_approval_files | ForEach-Object { $_.path })
    $protected = @($ScopeJson.protected_files | ForEach-Object { $_.path })

    $missingExpected = @($ExpectedFiles | Where-Object {
        $file = $_
        -not ($allowed -contains $file -or $context -contains $file -or $suspected -contains $file -or $needsApproval -contains $file)
    })
    $protectedInAllowed = @($ProtectedFiles | Where-Object {
        $allowed -contains $_
    })

    [pscustomobject]@{
        missing_expected = $missingExpected
        protected_in_allowed = $protectedInAllowed
        protected_count = $protected.Count
    }
}

$repoRoot = Resolve-RepoRoot
$agentGuard = Resolve-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
if (-not $TasksFile) {
    $TasksFile = Join-Path $repoRoot "benchmarks\tasks.jsonl"
}
if (-not $RunsRoot) {
    $RunsRoot = Join-Path $repoRoot "runs\benchmarks\$Provider"
}
if (-not $AnalyzeResponseFile) {
    $AnalyzeResponseFile = Join-Path $repoRoot "examples\reports\sample-semantic-scope.json"
}
if (-not $ReviewResponseFile) {
    $ReviewResponseFile = Join-Path $repoRoot "examples\reports\sample-semantic-review.json"
}

$tasks = @(Read-BenchmarkTasks -Path $TasksFile)
$results = [System.Collections.Generic.List[object]]::new()
$started = Get-Date

foreach ($task in $tasks) {
    $taskStarted = Get-Date
    $taskRunRoot = Join-Path $RunsRoot $task.id
    $solution = Join-Path $repoRoot $task.solution
    $taskFile = Join-Path $repoRoot $task.task
    $taskText = Get-Content -LiteralPath $taskFile -Raw

    $status = "failed"
    $errorType = ""
    $workspace = ""
    $report = ""
    $reportJson = ""
    $buildOk = $false
    $reviewAction = ""
    $scopeCheck = $null

    try {
        $analyzeArgs = @(
            "analyze",
            "--solution", $solution,
            "--task", $taskText,
            "--provider", $Provider,
            "--runs-root", $taskRunRoot,
            "--force",
            "--json"
        )
        if ($Model) {
            $analyzeArgs += @("--model", $Model)
        }
        if ($Provider -eq "file") {
            $analyzeArgs += @("--response-file", $AnalyzeResponseFile)
        }

        $analyze = Invoke-AgentGuardJson -Exe $agentGuard -Arguments $analyzeArgs
        $workspace = $analyze.workspace
        $report = $analyze.report
        $reportJson = $analyze.report_json

        $scopeJson = Get-Content -LiteralPath $analyze.semantic_scope -Raw | ConvertFrom-Json
        $scopeCheck = Test-ScopeFiles `
            -ScopeJson $scopeJson `
            -ExpectedFiles @($task.expected_files) `
            -ProtectedFiles @($task.protected_files)

        $verify = Invoke-AgentGuardJson -Exe $agentGuard -Arguments @(
            "verify",
            "--workspace", $workspace,
            "--configuration", $Configuration,
            "--platform", $Platform,
            "--json"
        )
        $buildOk = [bool]$verify.ok

        $reviewArgs = @(
            "review",
            "--workspace", $workspace,
            "--task", $taskText,
            "--provider", $Provider,
            "--json"
        )
        if ($Model) {
            $reviewArgs += @("--model", $Model)
        }
        if ($Provider -eq "file") {
            $reviewArgs += @("--response-file", $ReviewResponseFile)
        }
        $review = Invoke-AgentGuardJson -Exe $agentGuard -Arguments $reviewArgs
        $reviewAction = $review.next_action

        if ($buildOk -and $reviewAction -and
            $scopeCheck.missing_expected.Count -eq 0 -and
            $scopeCheck.protected_in_allowed.Count -eq 0) {
            $status = "passed"
        }
        else {
            $status = "failed"
            $errorType = "scope_or_review"
        }
    }
    catch {
        $status = "failed"
        $errorType = $_.Exception.Message
    }

    $elapsed = [int]((Get-Date) - $taskStarted).TotalMilliseconds
    $results.Add([pscustomobject]@{
        id = $task.id
        name = $task.name
        difficulty = $task.difficulty
        status = $status
        build_success = $buildOk
        review_next_action = $reviewAction
        repair_rounds = 0
        duration_ms = $elapsed
        workspace = $workspace
        report = $report
        report_json = $reportJson
        missing_expected_files = if ($scopeCheck) { $scopeCheck.missing_expected } else { @() }
        protected_in_allowed = if ($scopeCheck) { $scopeCheck.protected_in_allowed } else { @() }
        error_type = $errorType
    }) | Out-Null
}

$passed = @($results | Where-Object { $_.status -eq "passed" }).Count
$total = $results.Count
$averageDuration = if ($total -gt 0) {
    [math]::Round((($results | Measure-Object -Property duration_ms -Average).Average), 2)
} else {
    0
}

$summary = [pscustomobject]@{
    ok = ($passed -eq $total)
    provider = $Provider
    total = $total
    passed = $passed
    failed = $total - $passed
    success_rate = if ($total -gt 0) { [math]::Round($passed / $total, 4) } else { 0 }
    average_duration_ms = $averageDuration
    average_repair_rounds = 0
    started = $started.ToString("o")
    finished = (Get-Date).ToString("o")
    results = $results
}

$summary | ConvertTo-Json -Depth 8
if (-not $summary.ok) {
    exit 1
}
