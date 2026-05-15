param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Add-Result {
    param(
        [System.Collections.Generic.List[object]]$Results,
        [string]$Name,
        [bool]$Ok,
        [string]$Message
    )

    $Results.Add([pscustomobject]@{
        name = $Name
        ok = $Ok
        message = $Message
    }) | Out-Null
}

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Get-ScannableFiles {
    param([string]$RepoRoot)

    $excludedDirectories = @(
        ".git",
        ".vs",
        "build",
        "runs",
        "x64",
        "Win32",
        "Debug",
        "Release",
        "logs",
        "out",
        "node_modules",
        "vcpkg_installed"
    )

    Get-ChildItem -Path $RepoRoot -Recurse -File -Force |
        Where-Object {
            $relative = $_.FullName.Substring($RepoRoot.Length).TrimStart([char[]]@('\', '/'))
            $segments = $relative -split '[\\/]'
            foreach ($directory in $excludedDirectories) {
                if ($segments -contains $directory) {
                    return $false
                }
            }
            return $true
        }
}

function Test-Pattern {
    param(
        [object[]]$Files,
        [string]$Pattern
    )

    $matchedFiles = @()
    foreach ($file in $Files) {
        $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -ne $text -and $text -match $Pattern) {
            $matchedFiles += $file.FullName
        }
    }
    return $matchedFiles
}

function Test-ReadmeLinks {
    param([string]$RepoRoot)

    $readme = Join-Path $RepoRoot "README.md"
    if (-not (Test-Path $readme)) {
        return @("README.md is missing.")
    }

    $content = Get-Content -LiteralPath $readme -Raw
    $problems = @()
    $matches = [regex]::Matches($content, '\[[^\]]+\]\(([^)]+)\)')
    foreach ($match in $matches) {
        $target = $match.Groups[1].Value
        if ($target.StartsWith("http://") -or
            $target.StartsWith("https://") -or
            $target.StartsWith("#") -or
            $target.StartsWith("mailto:")) {
            continue
        }
        $target = ($target -split "#")[0]
        if (-not $target) {
            continue
        }
        $path = Join-Path $RepoRoot $target
        if (-not (Test-Path $path)) {
            $problems += "Broken README link: $target"
        }
    }
    return $problems
}

function Find-CTest {
    $command = Get-Command ctest.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $install = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($install) {
            $candidate = Join-Path $install "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }
    return ""
}

$repoRoot = Get-RepoRoot
$results = [System.Collections.Generic.List[object]]::new()
$files = @(Get-ScannableFiles -RepoRoot $repoRoot)

$secretMatches = Test-Pattern -Files $files -Pattern 'sk-[A-Za-z0-9]{16,}'
Add-Result $results "api-key-pattern-scan" ($secretMatches.Count -eq 0) `
    ($(if ($secretMatches.Count -eq 0) { "No API key-like strings found." } else { "Matches: $($secretMatches -join ', ')" }))

$pathMatches = @(Test-Pattern -Files $files -Pattern '(D:\\|D:/|C:\\Users\\lenovo|C:/Users/lenovo)' |
    Where-Object { $_ -ne (Join-Path $repoRoot "scripts\preflight-release.ps1") })
Add-Result $results "local-absolute-path-scan" ($pathMatches.Count -eq 0) `
    ($(if ($pathMatches.Count -eq 0) { "No release-blocking local paths found." } else { "Matches: $($pathMatches -join ', ')" }))

$readmeProblems = @(Test-ReadmeLinks -RepoRoot $repoRoot)
Add-Result $results "readme-links" ($readmeProblems.Count -eq 0) `
    ($(if ($readmeProblems.Count -eq 0) { "README local links resolve." } else { $readmeProblems -join '; ' }))

$skillRoot = Join-Path $repoRoot "skills\agentguard-vs-cxx"
$skillOk = (Test-Path (Join-Path $skillRoot "SKILL.md")) -and
           (Test-Path (Join-Path $skillRoot "scripts")) -and
           (Test-Path (Join-Path $skillRoot "references"))
Add-Result $results "skill-directory" $skillOk "skills/agentguard-vs-cxx contains SKILL.md, scripts, and references."

$examplesOk = (Test-Path (Join-Path $repoRoot "examples\library-system\Library.sln")) -and
              (Test-Path (Join-Path $repoRoot "examples\run-library-demo.ps1"))
Add-Result $results "examples-directory" $examplesOk "examples/library-system and demo runner are present."

$runTestsOk = (Test-Path (Join-Path $repoRoot "build\vs2022-debug\RUN_TESTS.vcxproj")) -or
              (Test-Path (Join-Path $repoRoot "build\vs2022-debug\CTestTestfile.cmake"))
Add-Result $results "run-tests-target" $runTestsOk "RUN_TESTS or CTest metadata is present in build/vs2022-debug."

$workspaceTest = Join-Path $repoRoot "tests\test_workspace.cpp"
$diffTest = Join-Path $repoRoot "tests\test_diff_reporter.cpp"
$gitBaselineOk = (Test-Path $workspaceTest) -and
                 (Test-Path $diffTest) -and
                 ((Get-Content $workspaceTest -Raw) -match "Baseline") -and
                 ((Get-Content $diffTest -Raw) -match "Baseline")
Add-Result $results "git-baseline-tests" $gitBaselineOk "Workspace and DiffReporter Git baseline tests are present."

if (-not $SkipTests) {
    $ctest = Find-CTest
    if (-not $ctest) {
        Add-Result $results "ctest" $false "ctest.exe was not found."
    }
    elseif (-not (Test-Path (Join-Path $repoRoot "build\vs2022-debug"))) {
        Add-Result $results "ctest" $false "build/vs2022-debug does not exist. Build before preflight."
    }
    else {
        & $ctest --test-dir (Join-Path $repoRoot "build\vs2022-debug") -C Debug --output-on-failure
        Add-Result $results "ctest" ($LASTEXITCODE -eq 0) "ctest completed with exit code $LASTEXITCODE."
    }
}
else {
    Add-Result $results "ctest" $true "Skipped by -SkipTests."
}

$ok = -not ($results | Where-Object { -not $_.ok })
$summary = [pscustomobject]@{
    ok = $ok
    results = $results
}

$summary | ConvertTo-Json -Depth 4
if (-not $ok) {
    exit 1
}
