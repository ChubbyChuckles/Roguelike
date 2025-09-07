param(
    [int]$Iterations = 25,
    [string]$TestRelPath = 'build/tests/Debug/test_start_screen_phase10_4_reduced_motion.exe',
    [switch]$SavePerIterationLogs,
    [switch]$DumpOnFailure,
    [int]$TailLines = 40
)

$repoRoot = (Split-Path $PSScriptRoot -Parent)
$testExe = Join-Path $repoRoot $TestRelPath

if (-not (Test-Path $testExe)) {
    Write-Host "ERROR: test executable not found: $testExe" -ForegroundColor Red
    exit 2
}

Write-Host "Stress loop starting" -ForegroundColor Cyan
Write-Host " Test: $testExe" -ForegroundColor DarkCyan
Write-Host " Iterations: $Iterations" -ForegroundColor DarkCyan

$logDir = Join-Path $repoRoot 'build/rm_loop_logs'
if ($SavePerIterationLogs) {
    if (-not (Test-Path $logDir)) { New-Item -ItemType Directory -Path $logDir | Out-Null }
    Write-Host " Logging per-iteration to $logDir" -ForegroundColor DarkCyan
}

$failed = $false
for ($i = 1; $i -le $Iterations; $i++) {
    $iterTag = ('iter_{0:0000}' -f $i)
    $iterLog = Join-Path $logDir ("$iterTag.txt")
    if ($SavePerIterationLogs) {
        & $testExe *>&1 | Tee-Object -FilePath $iterLog | Out-Null
    }
    else {
        & $testExe *>&1 | Out-Null
    }
    $exit = $LASTEXITCODE
    if ($exit -ne 0) {
        Write-Host "Iteration $i FAIL exit=$exit" -ForegroundColor Red
        if ($DumpOnFailure) {
            $probe = Join-Path $repoRoot 'rm_probe.txt'
            $guard = Join-Path $repoRoot 'rm_guard.txt'
            if (Test-Path $probe) { Write-Host "== tail rm_probe.txt ==" -ForegroundColor Yellow; Get-Content $probe -Tail $TailLines }
            if (Test-Path $guard) { Write-Host "== tail rm_guard.txt ==" -ForegroundColor Yellow; Get-Content $guard -Tail $TailLines }
            if ($SavePerIterationLogs -and (Test-Path $iterLog)) { Write-Host "== tail $iterTag.txt ==" -ForegroundColor Yellow; Get-Content $iterLog -Tail $TailLines }
        }
        $failed = $true
        break
    }
    else {
        Write-Host "Iteration $i OK" -ForegroundColor Green
    }
}
if (-not $failed) { Write-Host "All iterations passed" -ForegroundColor Green }
