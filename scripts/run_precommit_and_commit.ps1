Param(
    [string]$MaxAttempts = 3,
    [switch]$AutoStageUntracked
)

$ErrorActionPreference = 'Stop'

function Invoke-PreCommitOnce {
    Write-Host "[precommit] Running pre-commit hook manually..."
    $env:ROGUE_LICENSE_AUTO = '1' # allow auto-fix license headers
    bash .githooks/pre-commit
    return $LASTEXITCODE
}

function Set-PreCommitHook {
    $hookDir = (git rev-parse --git-path hooks)
    if (-not (Test-Path $hookDir)) { New-Item -ItemType Directory -Path $hookDir | Out-Null }
    $targetHook = Join-Path $hookDir 'pre-commit'
    if (-not (Test-Path $targetHook)) {
        Copy-Item .githooks/pre-commit $targetHook
        Write-Host "[precommit] Installed git hook to $targetHook"
    }
}

Set-PreCommitHook

if ($AutoStageUntracked) {
    git add -A
}

$attempt = 1
while ($attempt -le [int]$MaxAttempts) {
    try {
        Invoke-PreCommitOnce
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[precommit] All checks passed on attempt $attempt" -ForegroundColor Green
            break
        } else {
            Write-Host "[precommit] Checks failed (attempt $attempt)" -ForegroundColor Yellow
        }
    } catch {
        Write-Host "[precommit] Exception during pre-commit: $_" -ForegroundColor Red
    }
    if ($attempt -eq [int]$MaxAttempts) {
        Write-Host "[precommit] Reached max attempts ($MaxAttempts). Aborting commit." -ForegroundColor Red
        exit 1
    }
    Write-Host "[precommit] Attempting auto-fixes (format, license headers) then retry..."
    # Auto format if possible
    cmake --build build/precommit_format_check --target format --config Debug 2>$null | Out-Null
    git add -u
    if ($AutoStageUntracked) { git add -A }
    $attempt++
}

# Commit using pending commit message file
if (-not (Test-Path .pending_commit_message.txt)) {
    Write-Host "[precommit] Missing .pending_commit_message.txt" -ForegroundColor Red
    exit 1
}

$msg = Get-Content .pending_commit_message.txt -Raw
if ([string]::IsNullOrWhiteSpace($msg)) {
    Write-Host "[precommit] Commit message file empty" -ForegroundColor Red
    exit 1
}

Write-Host "[precommit] Creating commit..."
# Use first line summary if multiple entries; keep full file as body.
$lines = $msg -split "`r?`n"
$subject = $lines | Where-Object { $_ -ne '' } | Select-Object -First 1
$body = ($lines | Select-Object -Skip 1) -join "`n"

# Compose temp message file
$tmp = New-TemporaryFile
Set-Content -Path $tmp -Value $subject
if ($body.Trim().Length -gt 0) {
    Add-Content -Path $tmp -Value ""
    Add-Content -Path $tmp -Value $body
}

# If there are staged changes, commit
$staged = git diff --cached --name-only
if (-not $staged) {
    Write-Host "[precommit] No staged changes to commit." -ForegroundColor Yellow
    exit 0
}

git commit -F $tmp

# Clean up
Remove-Item $tmp -ErrorAction SilentlyContinue

Write-Host "[precommit] Commit created successfully." -ForegroundColor Green
