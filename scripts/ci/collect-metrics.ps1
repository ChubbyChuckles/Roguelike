# Collects build/test timing metrics and cache stats and writes JSON + summary.
[CmdletBinding()]
param(
    [string]$BuildDir = "build",
    [string]$OutDir = "build/metrics",
    [switch]$WriteGithubSummary
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

# Parse CTest summary if present
$ctestLog = Join-Path $BuildDir 'Testing/Temporary/LastTest.log'
$summary = @{
    total = 0; passed = 0; failed = 0; total_time_sec = 0.0
}
if (Test-Path $ctestLog) {
    $text = Get-Content -Raw $ctestLog
    if ($text -match 'Total Test time \(real\) = ([0-9\.]+) sec') { $summary.total_time_sec = [double]$Matches[1] }
    if ($text -match '([0-9]+) tests,\s*([0-9]+) passed,\s*([0-9]+) failed') {
        $summary.total = [int]$Matches[1]
        $summary.passed = [int]$Matches[2]
        $summary.failed = [int]$Matches[3]
    }
}

# Build log size
$buildLog = Get-ChildItem -Path $BuildDir -Filter 'build_*.log' -ErrorAction SilentlyContinue | Select-Object -First 1
$buildLogSize = if ($buildLog) { $buildLog.Length } else { 0 }

# ccache stats if available
$ccache = Get-Command ccache -ErrorAction SilentlyContinue
$ccacheStats = $null
if ($ccache) {
    try {
        $json = ccache -s --json 2>$null
        $ccacheStats = $json | ConvertFrom-Json
    }
    catch { }
}

$metrics = @{
    ctest           = $summary
    build_log_bytes = $buildLogSize
    ccache          = $ccacheStats
}
$metrics | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $OutDir 'metrics.json') -Encoding utf8

if ($WriteGithubSummary) {
    $out = @()
    $out += "### Build & Test Metrics"
    $out += "- Tests: $($summary.total) (passed: $($summary.passed), failed: $($summary.failed))"
    $out += "- Total test time: $($summary.total_time_sec)s"
    $out += "- Build log size: $buildLogSize bytes"
    if ($ccacheStats) { $out += "- ccache: hits=$($ccacheStats.statistics.cache_hit), misses=$($ccacheStats.statistics.cache_miss)" }
    $outString = ($out -join "`n") + "`n"
    $outString | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Encoding utf8 -Append
}

Write-Host "Metrics written to $OutDir/metrics.json"
