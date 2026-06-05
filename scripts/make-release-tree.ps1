<#
.SYNOPSIS
    Generate a clean public release tree for AgentGuardVS-CXX.

.DESCRIPTION
    Copies an allowlist of release-relevant content from the development
    repository into a separate output directory, removes build/runtime
    artifacts via a denylist, sanitizes machine-specific absolute paths,
    optionally stages the rebuilt Release binary, and writes
    RELEASE_MANIFEST.md, RELEASE_AUDIT.md and RELEASE_CHECKSUMS.sha256.

    This is a release-engineering tool. It NEVER modifies SourceRoot.
    It refuses to run when OutputRoot equals SourceRoot or is nested inside it.

.EXAMPLE
    pwsh -File scripts/make-release-tree.ps1 `
        -SourceRoot <dev-repo> `
        -OutputRoot <output-dir> `
        -Version 0.2.0 -IncludeBinary -Force
#>
[CmdletBinding()]
param(
    [string]$SourceRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$OutputRoot = '',
    [string]$Version = '0.2.0',
    [string]$BuildConfig = 'Release',
    [string]$AgentGuardExe = '',
    [string]$TestSummary = '',
    [string]$SourceSizeText = '',
    [string[]]$ExtraSanitize = @(),
    [switch]$IncludeBinary,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
# 0. Path safety
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $SourceRoot)) {
    throw "SourceRoot does not exist: $SourceRoot"
}
$srcResolved = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\', '/')

# Default OutputRoot: a sibling "<repo>-RELEASE" directory (never inside the repo).
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $repoLeaf = Split-Path -Leaf $srcResolved
    $repoParent = Split-Path -Parent $srcResolved
    $OutputRoot = Join-Path $repoParent ($repoLeaf + '-RELEASE')
}

# OutputRoot may not exist yet, so resolve its parent and rebuild the full path.
$outParent = Split-Path -Parent $OutputRoot
$outLeaf = Split-Path -Leaf $OutputRoot
if (-not (Test-Path -LiteralPath $outParent)) {
    throw "OutputRoot parent does not exist: $outParent"
}
$outResolved = (Join-Path (Resolve-Path -LiteralPath $outParent).Path $outLeaf).TrimEnd('\', '/')

$srcCmp = $srcResolved.ToLowerInvariant()
$outCmp = $outResolved.ToLowerInvariant()

if ($srcCmp -eq $outCmp) {
    throw "Refusing to run: OutputRoot equals SourceRoot ($outResolved)."
}
if ($outCmp.StartsWith($srcCmp + '\') -or $outCmp.StartsWith($srcCmp + '/')) {
    throw "Refusing to run: OutputRoot ($outResolved) is nested inside SourceRoot ($srcResolved)."
}
if ($srcCmp.StartsWith($outCmp + '\') -or $srcCmp.StartsWith($outCmp + '/')) {
    throw "Refusing to run: SourceRoot is nested inside OutputRoot."
}

Write-Host "SourceRoot : $srcResolved"
Write-Host "OutputRoot : $outResolved"
Write-Host "Version    : $Version"

# ---------------------------------------------------------------------------
# 1. (Re)create OutputRoot
# ---------------------------------------------------------------------------
if (Test-Path -LiteralPath $outResolved) {
    if (-not $Force) {
        throw "OutputRoot already exists. Re-run with -Force to replace it: $outResolved"
    }
    # Final guard: never delete the source, and only delete a path that still
    # resolves outside SourceRoot.
    $delResolved = (Resolve-Path -LiteralPath $outResolved).Path.TrimEnd('\', '/').ToLowerInvariant()
    if ($delResolved -eq $srcCmp -or $srcCmp.StartsWith($delResolved + '\')) {
        throw "Safety abort: deletion target overlaps SourceRoot ($delResolved)."
    }
    Write-Host "Removing existing OutputRoot ..."
    Remove-Item -LiteralPath $outResolved -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $outResolved | Out-Null

# ---------------------------------------------------------------------------
# 2. Allowlist copy
# ---------------------------------------------------------------------------
$allowFiles = @(
    'README.md', 'LICENSE', 'CHANGELOG.md', 'CONTRIBUTING.md', 'SECURITY.md',
    '.env.example', '.gitignore', 'CMakeLists.txt', 'CMakePresets.json', 'vcpkg.json'
)
$allowDirs = @(
    'src', 'tests', 'docs', 'scripts', 'skills', 'plugins', 'examples',
    'action', '.github', 'benchmarks'
)

foreach ($f in $allowFiles) {
    $s = Join-Path $srcResolved $f
    if (Test-Path -LiteralPath $s) {
        Copy-Item -LiteralPath $s -Destination (Join-Path $outResolved $f) -Force
    }
}
foreach ($d in $allowDirs) {
    $s = Join-Path $srcResolved $d
    if (Test-Path -LiteralPath $s) {
        Copy-Item -LiteralPath $s -Destination (Join-Path $outResolved $d) -Recurse -Force
    }
}

# ---------------------------------------------------------------------------
# 3. Denylist cleanup (artifacts / history / runtime that slipped in)
# ---------------------------------------------------------------------------
$denyDirs = @(
    '.git', '.vs', 'build', 'out', 'dist', 'runs', 'node_modules',
    '__pycache__', '.pytest_cache', '.venv', 'venv', 'x64', 'Win32',
    'Debug', 'Release', 'obj', 'vcpkg_installed', 'TestResults', 'superpowers'
)
$denyFileGlobs = @(
    '*.obj', '*.pdb', '*.ilk', '*.lib', '*.exp', '*.tlog', '*.lastbuildstate',
    '*.idb', '*.ipdb', '*.iobj', '*.pch', '*.aps', '*.sdf', '*.log', '*.tmp',
    '*.bak', '*.old', '*.cache', '*.binlog', '*.suo', '*.user', '*.VC.db',
    '*.opendb', '*.zip', '*.7z', '*.rar'
)

# Remove denied directories (deepest first so nested ones disappear cleanly).
$denyDirsLower = $denyDirs | ForEach-Object { $_.ToLowerInvariant() }
Get-ChildItem -LiteralPath $outResolved -Recurse -Directory -Force |
    Sort-Object { $_.FullName.Length } -Descending |
    Where-Object { $denyDirsLower -contains $_.Name.ToLowerInvariant() } |
    ForEach-Object {
        if (Test-Path -LiteralPath $_.FullName) {
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        }
    }

# Remove denied files by glob.
foreach ($glob in $denyFileGlobs) {
    Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force -Filter $glob |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
}

# Drop any now-empty directories.
do {
    $empties = @(Get-ChildItem -LiteralPath $outResolved -Recurse -Directory -Force |
        Where-Object { @(Get-ChildItem -LiteralPath $_.FullName -Force).Count -eq 0 })
    foreach ($e in $empties) { Remove-Item -LiteralPath $e.FullName -Force }
} while ($empties.Count -gt 0)

# ---------------------------------------------------------------------------
# 4. Sanitize machine-specific absolute paths in text/report files
# ---------------------------------------------------------------------------
$userHome = $env:USERPROFILE
$sanitizeExt = @('.md', '.json', '.html', '.csv', '.txt', '.yml', '.yaml', '.py', '.bat', '.cmake')
# Do not rewrite the path-detection scanners (their literals are intentional).
$sanitizeSkip = @('preflight-release.ps1', 'make-release-tree.ps1')
$sanitizedFiles = @()

$srcFwd = $srcResolved -replace '\\', '/'
$srcEsc = $srcResolved -replace '\\', '\\'
$homeEsc = if (-not [string]::IsNullOrWhiteSpace($userHome)) { $userHome -replace '\\', '\\' } else { '' }
foreach ($file in (Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force)) {
    if ($sanitizeExt -notcontains $file.Extension.ToLowerInvariant()) { continue }
    if ($sanitizeSkip -contains $file.Name) { continue }
    $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($null -eq $text) { continue }
    $orig = $text
    # Source root: both slash flavours and escaped (JSON/Python "\\") forms,
    # with and without trailing separator. Replace escaped forms first.
    $text = $text.Replace($srcEsc + '\\', '')
    $text = $text.Replace($srcEsc, '.')
    $text = $text.Replace($srcResolved + '\', '')
    $text = $text.Replace($srcFwd + '/', '')
    $text = $text.Replace($srcResolved, '.')
    $text = $text.Replace($srcFwd, '.')
    if (-not [string]::IsNullOrWhiteSpace($userHome)) {
        $homeFwd = $userHome -replace '\\', '/'
        $text = $text.Replace($homeEsc, '<user-home>')
        $text = $text.Replace($userHome, '<user-home>')
        $text = $text.Replace($homeFwd, '<user-home>')
    }
    foreach ($extra in $ExtraSanitize) {
        if ([string]::IsNullOrWhiteSpace($extra)) { continue }
        $e = $extra.TrimEnd('\', '/')
        $eFwd = $e -replace '\\', '/'
        $eEsc = $e -replace '\\', '\\'
        $text = $text.Replace($eEsc, '<external-path>')   # escaped (\\) form first
        $text = $text.Replace($e, '<external-path>')
        $text = $text.Replace($eFwd, '<external-path>')
    }
    if ($text -ne $orig) {
        Set-Content -LiteralPath $file.FullName -Value $text -NoNewline -Encoding UTF8
        $sanitizedFiles += $file.FullName.Substring($outResolved.Length).TrimStart('\', '/')
    }
}

# ---------------------------------------------------------------------------
# 5. Stage Release binary (optional)
# ---------------------------------------------------------------------------
$binStaged = $false
$binSource = ''
$binSizeText = 'n/a'
if ($IncludeBinary) {
    if ([string]::IsNullOrWhiteSpace($AgentGuardExe)) {
        $candidates = @(
            (Join-Path $srcResolved 'build\release-check\Release\AgentGuardVS.exe'),
            (Join-Path $srcResolved 'build\vs2022-release\Release\AgentGuardVS.exe')
        )
        foreach ($c in $candidates) { if (Test-Path -LiteralPath $c) { $AgentGuardExe = $c; break } }
    }
    if (-not [string]::IsNullOrWhiteSpace($AgentGuardExe) -and (Test-Path -LiteralPath $AgentGuardExe)) {
        New-Item -ItemType Directory -Force -Path (Join-Path $outResolved 'bin') | Out-Null
        Copy-Item -LiteralPath $AgentGuardExe -Destination (Join-Path $outResolved 'bin\AgentGuardVS.exe') -Force
        $binStaged = $true
        $binSource = (Resolve-Path -LiteralPath $AgentGuardExe).Path
        $binBytes = (Get-Item -LiteralPath (Join-Path $outResolved 'bin\AgentGuardVS.exe')).Length
        $binSizeText = '{0:N2} MB' -f ($binBytes / 1MB)
    }
    else {
        Write-Warning "IncludeBinary requested but no AgentGuardVS.exe was found."
    }
}

# Ensure the published repo never tracks the local binary directory.
$giPath = Join-Path $outResolved '.gitignore'
$giLines = @()
if (Test-Path -LiteralPath $giPath) { $giLines = Get-Content -LiteralPath $giPath }
if ($giLines -notcontains '/bin/') {
    Add-Content -LiteralPath $giPath -Value "`r`n# Local release binary (rebuild from source)`r`n/bin/`r`nRELEASE_CHECKSUMS.sha256"
}

# ---------------------------------------------------------------------------
# 6. Post-clean security/privacy scan
# ---------------------------------------------------------------------------
$scanFiles = Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force |
    Where-Object { $sanitizeSkip -notcontains $_.Name }

$apiKeyHits = @()
$absPathHits = @()
foreach ($file in $scanFiles) {
    if ($file.Extension.ToLowerInvariant() -in @('.exe', '.dll', '.pdb', '.png', '.jpg', '.ico')) { continue }
    $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
    if ($null -eq $text) { continue }
    $rel = $file.FullName.Substring($outResolved.Length).TrimStart('\', '/')
    if ($text -match 'sk-[A-Za-z0-9]{16,}' -or $text -match 'ghp_[A-Za-z0-9]{20,}' -or $text -match 'AKIA[0-9A-Z]{16}') {
        $apiKeyHits += $rel
    }
    $hasAbs = $false
    if ($text -match [regex]::Escape($srcResolved) -or $text -match [regex]::Escape($srcFwd) -or $text -match [regex]::Escape($srcEsc)) { $hasAbs = $true }
    # The real user's home (single or escaped backslash) -- fictional test users (e.g. "alice") do not match.
    if (-not [string]::IsNullOrWhiteSpace($userHome)) {
        if ($text -match [regex]::Escape($userHome) -or $text -match [regex]::Escape($homeEsc) -or
            $text -match [regex]::Escape(($userHome -replace '\\', '/'))) { $hasAbs = $true }
    }
    foreach ($extra in $ExtraSanitize) {
        if ([string]::IsNullOrWhiteSpace($extra)) { continue }
        $eT = $extra.TrimEnd('\', '/')
        if ($text -match [regex]::Escape($eT) -or $text -match [regex]::Escape(($eT -replace '\\', '\\'))) { $hasAbs = $true }
    }
    if ($hasAbs) { $absPathHits += $rel }
}

# ---------------------------------------------------------------------------
# 7. Manifest
# ---------------------------------------------------------------------------
$generated = Get-Date -Format 'yyyy-MM-dd HH:mm:ss zzz'
$includedDirs = (Get-ChildItem -LiteralPath $outResolved -Directory -Force | Sort-Object Name | ForEach-Object { $_.Name })
$relSizeBytes = (Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum
$relSizeText = '{0:N2} MB' -f ($relSizeBytes / 1MB)

# Publish-safe (drive-free) provenance strings.
$srcDisplay = Split-Path -Leaf $srcResolved
$outDisplay = Split-Path -Leaf $outResolved
if ($binStaged -and $binSource.ToLowerInvariant().StartsWith($srcResolved.ToLowerInvariant())) {
    $binDisplay = $binSource.Substring($srcResolved.Length).TrimStart('\', '/')
}
elseif ($binStaged) { $binDisplay = Split-Path -Leaf $binSource }
else { $binDisplay = 'not staged' }

$manifest = @"
# AgentGuardVS-CXX Release Manifest

- **Project**: AgentGuardVS-CXX
- **Version**: $Version
- **Generated**: $generated
- **Build configuration**: $BuildConfig
- **Binary included**: $binStaged
- **Binary source**: $(if ($binStaged) { $binDisplay } else { 'not staged' })
- **Release tree size**: $relSizeText

## Included top-level directories

$($includedDirs | ForEach-Object { "- $_" } | Out-String)
## Included top-level files

$($allowFiles | Where-Object { Test-Path (Join-Path $outResolved $_) } | ForEach-Object { "- $_" } | Out-String)
## Excluded from the release

- Version control / IDE: ``.git/``, ``.vs/``
- Build output: ``build/``, ``out/``, ``dist/``, ``x64/``, ``Win32/``, ``Debug/``, ``Release/``, ``obj/``, ``vcpkg_installed/``
- Runtime / history: ``runs/`` (including ``benchmarks/AgentGuardBench/runs/``), ``TestResults/``
- Large benchmark run snapshots and isolated mirror repositories
- Internal planning drafts: ``docs/superpowers/``
- Build intermediates: ``*.obj``, ``*.pdb``, ``*.ilk``, ``*.lib``, ``*.exp``, ``*.tlog``, ``*.idb``, ``*.iobj``, ``*.binlog``, ``*.log``, ``*.tmp``, ``*.bak``, archives

## Test results

$(if ([string]::IsNullOrWhiteSpace($TestSummary)) { 'See RELEASE_AUDIT.md.' } else { $TestSummary })

## How to build

``````powershell
cmake -S . -B build/vs2022-release -G "Visual Studio 17 2022" -A x64 ``
  -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build/vs2022-release --config Release
ctest --test-dir build/vs2022-release -C Release --output-on-failure
``````

## How to run

``````powershell
# If you staged the local binary it is at bin/AgentGuardVS.exe; otherwise use your build output.
.\bin\AgentGuardVS.exe detect --project . --json
.\bin\AgentGuardVS.exe verify-profile --project . --json
.\bin\AgentGuardVS.exe analyze --project <path-to-project> --json
``````
"@
Set-Content -LiteralPath (Join-Path $outResolved 'RELEASE_MANIFEST.md') -Value $manifest -Encoding UTF8

# ---------------------------------------------------------------------------
# 8. Audit
# ---------------------------------------------------------------------------
$apiOk = ($apiKeyHits.Count -eq 0)
$pathOk = ($absPathHits.Count -eq 0)
$audit = @"
# AgentGuardVS-CXX Release Audit

- **Purpose**: produce a clean public release tree without development history,
  build intermediates, runtime run snapshots, local caches, or machine-specific paths,
  while keeping source, docs, tests, examples, scripts, agent skill/plugin assets,
  and minimal evaluation evidence.
- **Source repository**: $srcDisplay (development repository)
- **Release output**: $outDisplay
- **Version**: $Version
- **Generated**: $generated

## Sizes

- Source repository (with history/build/runs): $(if ([string]::IsNullOrWhiteSpace($SourceSizeText)) { 'see report' } else { $SourceSizeText })
- Release tree: $relSizeText
- Staged binary: $binSizeText

## Exclusions applied

Excluded ``.git``, ``.vs``, ``build``, ``out``, ``dist``, ``runs`` (incl. benchmark runs),
``vcpkg_installed``, ``node_modules``, ``__pycache__``, ``.pytest_cache``, ``docs/superpowers``,
and all build intermediates / archives by glob.

## Findings

- Absolute path / username leak: $(if ($pathOk) { 'NONE (sanitized + verified)' } else { 'FOUND -> ' + ($absPathHits -join ', ') })
- API key / token risk: $(if ($apiOk) { 'NONE' } else { 'FOUND -> ' + ($apiKeyHits -join ', ') })
- Sanitized files: $(if ($sanitizedFiles.Count -eq 0) { 'none required' } else { $sanitizedFiles -join ', ' })
- Debug binary shipped as release: $(if ($binStaged -and $BuildConfig -ne 'Release') { 'YES (NON-OFFICIAL RELEASE)' } else { 'no' })
- Release build completed: see TestSummary / report
- ctest completed: $(if ([string]::IsNullOrWhiteSpace($TestSummary)) { 'see report' } else { $TestSummary })

## Conclusion

$(if (-not $apiOk) { 'FAIL: secret-like strings present.' }
  elseif (-not $pathOk) { 'RC: residual absolute paths require manual review.' }
  else { 'PASS: release tree is clean (build/test status recorded above).' })
"@
Set-Content -LiteralPath (Join-Path $outResolved 'RELEASE_AUDIT.md') -Value $audit -Encoding UTF8

# ---------------------------------------------------------------------------
# 9. Checksums (last; covers every shipped file except itself)
# ---------------------------------------------------------------------------
$checksumPath = Join-Path $outResolved 'RELEASE_CHECKSUMS.sha256'
$lines = Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force |
    Where-Object { $_.FullName -ne $checksumPath } |
    Sort-Object FullName |
    ForEach-Object {
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $rel = $_.FullName.Substring($outResolved.Length).TrimStart('\', '/') -replace '\\', '/'
        "$hash  $rel"
    }
Set-Content -LiteralPath $checksumPath -Value $lines -Encoding ASCII

# ---------------------------------------------------------------------------
# 10. Summary
# ---------------------------------------------------------------------------
[pscustomobject]@{
    sourceRoot      = $srcResolved
    outputRoot      = $outResolved
    version         = $Version
    releaseSize     = $relSizeText
    binaryStaged    = $binStaged
    binarySource    = $binSource
    sanitizedFiles  = $sanitizedFiles
    apiKeyHits      = $apiKeyHits
    absolutePathHits = $absPathHits
    fileCount       = @(Get-ChildItem -LiteralPath $outResolved -Recurse -File -Force).Count
} | ConvertTo-Json -Depth 5
