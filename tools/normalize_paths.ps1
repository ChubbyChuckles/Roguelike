# Purpose: Detect (and optionally fix) backslash path separators in text files to improve portability.
# Usage:
#   pwsh -File tools/normalize_paths.ps1 -Root . -CheckOnly
#   pwsh -File tools/normalize_paths.ps1 -Root . -Fix
param(
    [string]$Root = '.',
    [switch]$CheckOnly,
    [switch]$Fix,
    [string[]]$IncludeGlobs = @('src/**/*.c', 'src/**/*.h', 'assets/**/*.cfg', 'assets/**/*.json', 'cmake/**/*.cmake', 'CMakeLists.txt'),
    [string[]]$ExcludeGlobs = @('third_party/**', 'build/**', '.git/**')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Convert-GlobToRegex {
    param([string]$glob)
    $escaped = [Regex]::Escape($glob)
    $escaped = $escaped -replace '\\
', '/' # normalize
    $escaped = $escaped -replace '\\\*\\\\*', '.*'
    $escaped = $escaped -replace '\\\*', '[^/]*'
    '^' + $escaped + '$'
}

function Test-ExcludePath([string]$path) {
    $rel = ([IO.Path]::GetFullPath($path)).Substring(([IO.Path]::GetFullPath($Root)).Length).TrimStart('\\', '/')
    $rel = $rel -replace '\\', '/'
    foreach ($g in $ExcludeGlobs) {
        $rx = Convert-GlobToRegex $g
        if ($rel -match $rx) { return $true }
    }
    return $false
}

$includeRegexes = $IncludeGlobs | ForEach-Object { Convert-GlobToRegex $_ }

$rootFull = [IO.Path]::GetFullPath($Root)
if (-not (Test-Path $rootFull)) { throw "Root not found: $Root" }

$files = Get-ChildItem -Path $rootFull -Recurse -File | Where-Object {
    $rel = $_.FullName.Substring($rootFull.Length).TrimStart('\\', '/') -replace '\\', '/'
    -not (Test-ExcludePath $_.FullName) -and ($includeRegexes | Where-Object { $rel -match $_ } | Measure-Object).Count -gt 0
}

$violations = @()
foreach ($f in $files) {
    $content = Get-Content -LiteralPath $f.FullName -Raw -Encoding UTF8
    if ($content -match '\\\\') {
        # Record all lines with backslashes in string or path contexts; conservative match
        $lines = $content -split "\r?\n"
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match '\\\\') {
                $violations += [pscustomobject]@{ File = $f.FullName; Line = $i + 1; Text = $lines[$i] }
            }
        }
        if ($Fix) {
            $new = $content -replace '\\', '/'
            if ($new -ne $content) {
                Set-Content -LiteralPath $f.FullName -Value $new -NoNewline -Encoding UTF8
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "Found $($violations.Count) potential backslash path occurrences:" -ForegroundColor Yellow
    $violations | Select-Object -First 50 | ForEach-Object { Write-Host ("{0}:{1}: {2}" -f $_.File, $_.Line, $_.Text) }
    if (-not $Fix) {
        Write-Host "Run with -Fix to convert backslashes to forward slashes in matched files." -ForegroundColor Yellow
    }
}

if ($CheckOnly -and $violations.Count -gt 0) {
    exit 2
}

Write-Host "normalize_paths.ps1 completed. Violations: $($violations.Count)." -ForegroundColor Green
