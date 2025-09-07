# Parses build and test logs to categorize failures and emit a compact summary.
[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$OutDir = "build/failure_report",
    [switch]$WriteGithubSummary
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

function Add-Category([string]$name, [ref]$categories) {
    if (-not $categories.Value.ContainsKey($name)) { $categories.Value[$name] = @() }
}

$categories = @{}
$logs = @()

# Collect relevant logs
$logs += Get-ChildItem -Path $BuildDir -Filter '*.log' -Recurse -ErrorAction SilentlyContinue
$ctestTemp = Join-Path $BuildDir 'Testing/Temporary'
if (Test-Path $ctestTemp) {
    $logs += Get-ChildItem -Path $ctestTemp -Filter '*.log' -Recurse -ErrorAction SilentlyContinue
}

foreach ($log in $logs) {
    $content = Get-Content -Raw $log.FullName
    if ($content -match 'LNK[0-9]+:') { Add-Category -name 'Linker Errors (MSVC)' -categories ([ref]$categories); $categories['Linker Errors (MSVC)'] += $log.FullName }
    if ($content -match 'undefined reference|Undefined symbols') { Add-Category -name 'Undefined Symbols' -categories ([ref]$categories); $categories['Undefined Symbols'] += $log.FullName }
    if ($content -match 'Segmentation fault|stack dump|stack trace|Access violation') { Add-Category -name 'Crashes' -categories ([ref]$categories); $categories['Crashes'] += $log.FullName }
    if ($content -match 'Assertion failed|FAILED|Expected:') { Add-Category -name 'Test Assertions' -categories ([ref]$categories); $categories['Test Assertions'] += $log.FullName }
    if ($content -match 'SDL2.dll|could not load SDL|No available video device') { Add-Category -name 'SDL2 Runtime/Init' -categories ([ref]$categories); $categories['SDL2 Runtime/Init'] += $log.FullName }
    if ($content -match 'clang-tidy') { Add-Category -name 'Clang-Tidy Issues' -categories ([ref]$categories); $categories['Clang-Tidy Issues'] += $log.FullName }
}

# Write summary
$summaryPath = Join-Path $OutDir 'summary.txt'
$out = @()
if ($categories.Keys.Count -eq 0) {
    $out += 'No categorized failures found in logs.'
}
else {
    foreach ($k in $categories.Keys) {
        $out += "## $k"
        ($categories[$k] | Select-Object -Unique) | ForEach-Object { $out += "- $_" }
        $out += ''
    }
}
($out -join "`n") | Set-Content -Path $summaryPath -Encoding utf8

if ($WriteGithubSummary) {
    ("### Failure Categorization`n" + ($out -join "`n") + "`n") | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Encoding utf8 -Append
}

Write-Host "Wrote failure report to $summaryPath"
