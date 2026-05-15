param(
    [string]$AgentGuardExe = "",
    [string]$Configuration = "Debug",
    [string]$Platform = "x64",
    [string]$RunsRoot = "",
    [switch]$SkipDemoEdit
)

$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Resolve-AgentGuardExe {
    param([string]$ExplicitPath, [string]$RepoRoot)

    $candidates = @()
    if ($ExplicitPath) {
        $candidates += $ExplicitPath
    }
    if ($env:AGENTGUARD_EXE) {
        $candidates += $env:AGENTGUARD_EXE
    }

    $candidates += @(
        (Join-Path $RepoRoot "build\vs2022-debug\Debug\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\vs2022-release\Release\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\Debug\AgentGuardVS.exe"),
        (Join-Path $RepoRoot "build\Release\AgentGuardVS.exe")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "AgentGuardVS.exe was not found. Build the project first or pass -AgentGuardExe <path>."
}

function Resolve-MSBuild {
    $command = Get-Command MSBuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($found -and (Test-Path $found)) {
            return $found
        }
    }

    $fallbacks = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($candidate in $fallbacks) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "MSBuild.exe was not found. Install Visual Studio 2022 or Build Tools with the C++ workload."
}

function Invoke-JsonCommand {
    param([string]$Exe, [string[]]$Arguments)

    $output = & $Exe @Arguments
    $exit = $LASTEXITCODE
    $text = ($output | Out-String).Trim()
    if (-not $text) {
        throw "$Exe returned no JSON output."
    }

    try {
        $json = $text | ConvertFrom-Json
    }
    catch {
        throw "$Exe returned non-JSON output: $text"
    }

    if ($exit -ne 0 -or ($json.PSObject.Properties.Name -contains "ok" -and -not $json.ok)) {
        $code = if ($json.error_code) { $json.error_code } else { "UNKNOWN" }
        $message = if ($json.message) { $json.message } else { $text }
        throw "$Exe failed with $code`: $message"
    }

    return $json
}

function Apply-DemoEdit {
    param([string]$Workspace)

    $repo = Join-Path $Workspace "repo"
    $header = Join-Path $repo "LibraryApp\LibrarySystem.h"
    $source = Join-Path $repo "LibraryApp\LibrarySystem.cpp"
    $tests = Join-Path $repo "LibraryApp\LibraryTests.cpp"

    Set-Content -Path $header -Encoding UTF8 -Value @'
#pragma once

#include "Book.h"

#include <optional>
#include <string>
#include <vector>

class LibrarySystem
{
public:
    bool AddBook(const Book& book);
    bool RemoveBook(int id);
    bool UpdateBook(int id, const std::string& title, const std::string& author, int total_count);
    std::optional<Book> FindBook(int id) const;
    std::vector<Book> SearchByTitleKeyword(const std::string& keyword) const;
    std::vector<Book> ListBooks() const;
    bool BorrowBook(int id);
    bool ReturnBook(int id);

private:
    std::vector<Book> books_;
};
'@

    Set-Content -Path $source -Encoding UTF8 -Value @'
#include "LibrarySystem.h"

#include <algorithm>
#include <cctype>

namespace
{
bool IsValidBook(const Book& book)
{
    return book.id > 0 && !book.title.empty() && !book.author.empty() &&
           book.total_count >= 0 && book.borrowed_count >= 0 &&
           book.borrowed_count <= book.total_count;
}

std::string ToLowerCopy(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}
} // namespace

bool LibrarySystem::AddBook(const Book& book)
{
    if (!IsValidBook(book) || FindBook(book.id).has_value())
    {
        return false;
    }

    books_.push_back(book);
    return true;
}

bool LibrarySystem::RemoveBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count > 0)
    {
        return false;
    }

    books_.erase(it);
    return true;
}

bool LibrarySystem::UpdateBook(int id, const std::string& title, const std::string& author, int total_count)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || title.empty() || author.empty() || total_count < it->borrowed_count)
    {
        return false;
    }

    it->title = title;
    it->author = author;
    it->total_count = total_count;
    return true;
}

std::optional<Book> LibrarySystem::FindBook(int id) const
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end())
    {
        return std::nullopt;
    }

    return *it;
}

std::vector<Book> LibrarySystem::SearchByTitleKeyword(const std::string& keyword) const
{
    if (keyword.empty())
    {
        return {};
    }

    const std::string lowered_keyword = ToLowerCopy(keyword);
    std::vector<Book> matches;
    for (const auto& book : books_)
    {
        if (ToLowerCopy(book.title).find(lowered_keyword) != std::string::npos)
        {
            matches.push_back(book);
        }
    }
    return matches;
}

std::vector<Book> LibrarySystem::ListBooks() const
{
    return books_;
}

bool LibrarySystem::BorrowBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count >= it->total_count)
    {
        return false;
    }

    ++it->borrowed_count;
    return true;
}

bool LibrarySystem::ReturnBook(int id)
{
    const auto it = std::find_if(books_.begin(), books_.end(), [id](const Book& book) {
        return book.id == id;
    });
    if (it == books_.end() || it->borrowed_count == 0)
    {
        return false;
    }

    --it->borrowed_count;
    return true;
}
'@

    Set-Content -Path $tests -Encoding UTF8 -Value @'
#include "LibraryTests.h"

#include "LibrarySystem.h"

#include <iostream>
#include <string>

namespace
{
bool Expect(bool condition, const std::string& name)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << name << '\n';
        return false;
    }
    return true;
}
} // namespace

bool RunLibrarySelfTests()
{
    LibrarySystem library;
    bool ok = true;

    ok &= Expect(library.AddBook(Book{1, "Clean Code", "Robert Martin", 2, 0}), "add first book");
    ok &= Expect(!library.AddBook(Book{1, "Duplicate", "Someone", 1, 0}), "reject duplicate id");
    ok &= Expect(library.AddBook(Book{2, "The C++ Programming Language", "Bjarne Stroustrup", 1, 0}), "add second book");

    const auto clean_code = library.FindBook(1);
    ok &= Expect(clean_code.has_value() && clean_code->title == "Clean Code", "find book by id");
    ok &= Expect(library.UpdateBook(1, "Clean Code 2nd Edition", "Robert Martin", 2), "update book");

    const auto code_matches = library.SearchByTitleKeyword("code");
    ok &= Expect(code_matches.size() == 1 && code_matches.front().id == 1, "case-insensitive title keyword search");
    const auto cpp_matches = library.SearchByTitleKeyword("c++ PROGRAMMING");
    ok &= Expect(cpp_matches.size() == 1 && cpp_matches.front().id == 2, "case-insensitive mixed-case title search");
    ok &= Expect(library.SearchByTitleKeyword("").empty(), "empty title keyword returns no matches");

    ok &= Expect(library.BorrowBook(1), "borrow available book");
    ok &= Expect(library.BorrowBook(1), "borrow second copy");
    ok &= Expect(!library.BorrowBook(1), "reject borrow when no copies remain");
    ok &= Expect(!library.RemoveBook(1), "reject removing borrowed book");
    ok &= Expect(library.ReturnBook(1), "return borrowed book");
    ok &= Expect(library.RemoveBook(2), "remove available book");
    ok &= Expect(library.ListBooks().size() == 1, "list remaining books");

    return ok;
}
'@
}

$repoRoot = Resolve-RepoRoot
$agentGuard = Resolve-AgentGuardExe -ExplicitPath $AgentGuardExe -RepoRoot $repoRoot
$msbuild = Resolve-MSBuild
$solution = Join-Path $PSScriptRoot "library-system\Library.sln"
$taskFile = Join-Path $PSScriptRoot "tasks\add-case-insensitive-title-search.md"
$scopeResponse = Join-Path $PSScriptRoot "reports\sample-semantic-scope.json"
$reviewResponse = Join-Path $PSScriptRoot "reports\sample-semantic-review.json"
if (-not $RunsRoot) {
    $RunsRoot = Join-Path $repoRoot "runs\library-demo"
}

Write-Host "Building original library-system demo..."
& $msbuild $solution "/p:Configuration=$Configuration" "/p:Platform=$Platform" /m /nologo
if ($LASTEXITCODE -ne 0) {
    throw "Initial library-system build failed."
}

$demoExe = Get-ChildItem -Path (Join-Path $PSScriptRoot "library-system") -Recurse -Filter "LibraryApp.exe" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $demoExe) {
    throw "LibraryApp.exe was not produced by the initial build."
}

Write-Host "Running original self-test..."
& $demoExe.FullName
if ($LASTEXITCODE -ne 0) {
    throw "Original library-system self-test failed."
}

$task = Get-Content $taskFile -Raw
Write-Host "Running AgentGuard analyze..."
$analyze = Invoke-JsonCommand -Exe $agentGuard -Arguments @(
    "analyze",
    "--solution", $solution,
    "--task", $task,
    "--provider", "file",
    "--response-file", $scopeResponse,
    "--runs-root", $RunsRoot,
    "--force",
    "--json"
)

if ($SkipDemoEdit) {
    Write-Host "Analyze completed. Demo edit was skipped."
    Write-Host "Workspace: $($analyze.workspace)"
    Write-Host "Read semantic scope: $($analyze.semantic_scope)"
    Write-Host "Only edit files listed in allowed_files, then run verify and review."
    exit 0
}

Write-Host "Applying deterministic demo edit inside isolated workspace only..."
Apply-DemoEdit -Workspace $analyze.workspace

Write-Host "Running AgentGuard verify..."
$verify = Invoke-JsonCommand -Exe $agentGuard -Arguments @(
    "verify",
    "--workspace", $analyze.workspace,
    "--configuration", $Configuration,
    "--platform", $Platform,
    "--json"
)

Write-Host "Running AgentGuard review..."
$review = Invoke-JsonCommand -Exe $agentGuard -Arguments @(
    "review",
    "--workspace", $analyze.workspace,
    "--task", $task,
    "--provider", "file",
    "--response-file", $reviewResponse,
    "--json"
)

[pscustomobject]@{
    ok = $true
    workspace = $analyze.workspace
    semantic_scope = $analyze.semantic_scope
    verify_ok = $verify.ok
    build_log = $verify.log_path
    review_next_action = $review.next_action
    semantic_review = $review.semantic_review
    report = $analyze.report
    report_json = $analyze.report_json
    note = "Original demo project was built and self-tested; code edits were applied only in the AgentGuard workspace."
} | ConvertTo-Json -Depth 5
