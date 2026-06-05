[CmdletBinding()]
param(
    [string]$AgentGuardExe = $env:AGENTGUARD_EXE,
    [string]$OutputDirectory = "",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoRoot {
    $scriptRoot = Split-Path -Parent $MyInvocation.ScriptName
    return (Resolve-Path (Join-Path $scriptRoot '..')).Path
}

function Resolve-AgentGuardExe {
    param([string]$ExplicitPath, [string]$RepoRoot)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $candidates += $ExplicitPath
    }
    $candidates += @(
        (Join-Path $RepoRoot 'build\vs2022-release\Release\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\vs2022-debug\Debug\AgentGuardVS.exe'),
        (Join-Path $RepoRoot 'build\phase5-debug\Debug\AgentGuardVS.exe')
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw 'AgentGuardVS.exe was not found. Build first or pass -AgentGuardExe.'
}

function Copy-DirectoryIfPresent {
    param([string]$Source, [string]$Destination)
    if (Test-Path $Source) {
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
}

function Copy-DirectoryFiltered {
    param([string]$Source, [string]$Destination)
    if (-not (Test-Path $Source)) {
        return
    }

    $excludedDirectories = @('.git', '.vs', 'build', 'runs', 'x64', 'Win32', 'Debug', 'Release', 'out')
    $sourceRoot = (Resolve-Path $Source).Path
    $sourceUri = [System.Uri]::new(($sourceRoot.TrimEnd('\') + '\'))
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    Get-ChildItem -LiteralPath $sourceRoot -Recurse -Force | ForEach-Object {
        $relative = [System.Uri]::UnescapeDataString($sourceUri.MakeRelativeUri([System.Uri]::new($_.FullName)).ToString())
        $relative = $relative -replace '/', '\'
        if ([string]::IsNullOrWhiteSpace($relative)) {
            return
        }

        $segments = $relative -split '[\\/]'
        foreach ($segment in $segments) {
            if ($excludedDirectories -contains $segment) {
                return
            }
        }

        $target = Join-Path $Destination $relative
        if ($_.PSIsContainer) {
            New-Item -ItemType Directory -Force -Path $target | Out-Null
        }
        else {
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
        }
    }
}

$repoRoot = Resolve-RepoRoot
$exe = Resolve-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'dist'
}
$OutputDirectory = (New-Item -ItemType Directory -Force -Path $OutputDirectory).FullName

$packageName = "AgentGuardVS-CXX-$Version"
$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) $packageName
if (Test-Path $stagingRoot) {
    $resolved = (Resolve-Path $stagingRoot).Path
    if (-not $resolved.StartsWith([System.IO.Path]::GetTempPath(), [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove staging directory outside temp: $resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stagingRoot | Out-Null

Copy-Item -LiteralPath $exe -Destination (Join-Path $stagingRoot 'AgentGuardVS.exe') -Force
Copy-DirectoryIfPresent (Join-Path $repoRoot 'skills') (Join-Path $stagingRoot 'skills')
Copy-DirectoryIfPresent (Join-Path $repoRoot 'plugins') (Join-Path $stagingRoot 'plugins')
Copy-DirectoryFiltered (Join-Path $repoRoot 'docs') (Join-Path $stagingRoot 'docs')
Copy-DirectoryFiltered (Join-Path $repoRoot 'examples') (Join-Path $stagingRoot 'examples')
Copy-DirectoryFiltered (Join-Path $repoRoot 'scripts') (Join-Path $stagingRoot 'scripts')

foreach ($file in @('README.md', 'LICENSE', 'CONTRIBUTING.md', 'SECURITY.md', 'CHANGELOG.md', '.env.example')) {
    $source = Join-Path $repoRoot $file
    if (Test-Path $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $stagingRoot $file) -Force
    }
}

$zipPath = Join-Path $OutputDirectory "$packageName.zip"
if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $zipPath -Force

[pscustomobject]@{
    package = $zipPath
    root = $stagingRoot
    exe = $exe
    includes = @(
        'AgentGuardVS.exe',
        'skills/agentguard-vs-cxx',
        'plugins/agentguard-vs-cxx when present',
        'docs',
        'examples',
        'scripts',
        'README.md',
        'LICENSE',
        '.env.example'
    )
} | ConvertTo-Json -Depth 4
