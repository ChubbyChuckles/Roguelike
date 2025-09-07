# Analyze recent failed CI runs for the 'CI' workflow, download logs, and summarize errors.
param(
    [string]$WorkflowName = 'CI',
    [string]$OutDir = 'build/ci_logs'
)

# Resolve repo
function Get-RepoFromGitRemote {
    try {
        $url = $null
        try { $url = git remote get-url origin 2>$null } catch { }
        if (-not $url) { try { $url = git config --get remote.origin.url 2>$null } catch { } }
    }
    catch { return $null }
    if (-not $url) { return $null }
    if ($url -match '^https?://') {
        try { $u = [uri]$url; $p = $u.AbsolutePath.TrimStart('/') -replace '\\.git$', ''; return $p } catch { }
    }
    if ($url -match '[:/](?<owner>[^/]+)/(?<repo>[^/]+?)(?:\\.git)?$') { return "${Matches.owner}/${Matches.repo}" }
    return $null
}

# Resolve token: prefer environment; optionally dot-source a local, gitignored wrapper that sets $env:GITHUB_TOKEN
function Get-GitHubToken {
    # Prefer an environment variable set by CI or the user
    if ($env:GITHUB_TOKEN) { return $env:GITHUB_TOKEN }

    # Developers may create a local wrapper next to this script (gitignored).
    # The wrapper should set $env:GITHUB_TOKEN but MUST NOT be committed.
    # Example contents (local_github_token.ps1) -- DO NOT COMMIT:
    #   # local_github_token.ps1 (gitignored)
    #   # $env:GITHUB_TOKEN = 'ghp_xxxxxxxxxxxxxxxxxxxxxxxxxxxxx'

    $wrapperCandidates = @('local_github_token.ps1', 'cancel-run-with-token.ps1')
    foreach ($w in $wrapperCandidates) {
        $wrapper = Join-Path $PSScriptRoot $w
        if (Test-Path $wrapper) {
            Write-Host "Sourcing local wrapper: $wrapper"
            try {
                # Dot-source so it can set $env:GITHUB_TOKEN in this session
                . $wrapper
            }
            catch {
                Write-Warning "Failed to source wrapper '$wrapper': $($_.Exception.Message)"
            }
            if ($env:GITHUB_TOKEN) { return $env:GITHUB_TOKEN }
        }
    }

    return $null
}

# Normalize repo to owner/repo
function Normalize-Repo([string]$r) {
    if (-not $r) { return $null }
    # If full URL, parse
    if ($r -match '^https?://') {
        try {
            $u = [uri]$r
            $p = $u.AbsolutePath.TrimStart('/')
            $p = $p -replace '\.git$', ''
            return $p
        }
        catch { }
    }
    # If it looks like SSH or contains path separators, extract owner/repo
    if ($r -match '[:/](?<owner>[^/]+)/(?<repo>[^/]+?)(?:\.git)?$') {
        return "${Matches.owner}/${Matches.repo}"
    }
    # Otherwise, strip trailing .git if present
    return ($r -replace '\.git$', '')
}

$repo = Get-RepoFromGitRemote
$repo = Normalize-Repo $repo
if (-not $repo) { $repo = 'ChubbyChuckles/Roguelike' }
$token = Get-GitHubToken
if (-not $token) { Write-Error 'No GitHub token available (env or wrapper).'; exit 1 }

$headers = @{ Authorization = "token $token"; 'User-Agent' = 'roguelike-ci-analyzer' }

# List recent runs
$runsUri = "https://api.github.com/repos/$repo/actions/runs?per_page=50"
Write-Host "GET $runsUri"
try { $resp = Invoke-RestMethod -Uri $runsUri -Headers $headers -ErrorAction Stop } catch { Write-Error "Failed to list runs: $($_.Exception.Message)"; exit 2 }
if (-not $resp.workflow_runs) { Write-Error 'No workflow runs found.'; exit 2 }

# Filter to the named workflow
$ciRuns = $resp.workflow_runs | Where-Object { $_.name -eq $WorkflowName } | Sort-Object -Property created_at -Descending
if (-not $ciRuns) { Write-Error "No runs found for workflow '$WorkflowName'"; exit 2 }

Write-Host "Latest runs for '$WorkflowName':"
$ciRuns | Select-Object -First 6 | ForEach-Object { Write-Host ("- id={0} title='{1}' status={2} conclusion={3} created={4}" -f $_.id, $_.display_title, $_.status, ($_.conclusion ?? '-'), $_.created_at) }

# Determine latest failed (or cancelled) completed run
$latestFailed = $ciRuns | Where-Object { $_.status -eq 'completed' -and $_.conclusion -ne 'success' } | Select-Object -First 1
if (-not $latestFailed) { Write-Host 'No failed/cancelled runs to analyze.'; exit 0 }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$rid = $latestFailed.id
$zip = Join-Path $OutDir ("run_{0}.zip" -f $rid)
$dest = Join-Path $OutDir ("run_{0}" -f $rid)
Write-Host ("\nDownloading logs for latest failed run {0} (status={1} conclusion={2})" -f $rid, $latestFailed.status, $latestFailed.conclusion)
$logUri = "https://api.github.com/repos/$repo/actions/runs/$rid/logs"
try { Invoke-WebRequest -Uri $logUri -Headers $headers -OutFile $zip -ErrorAction Stop | Out-Null }
catch { Write-Error ("Failed to download logs for {0}: {1}" -f $rid, $_.Exception.Message); exit 3 }

if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
try { Expand-Archive -Path $zip -DestinationPath $dest -Force }
catch { Write-Error ("Failed to extract logs for {0}: {1}" -f $rid, $_.Exception.Message); exit 4 }

$patterns = @(
    'cmake error',
    'error:',
    'msb\d+: error',
    'ninja: build stopped',
    'make: ***',
    'ctest.*failed',
    'Segmentation fault',
    'Assertion failed'
)

$logFiles = Get-ChildItem -Path $dest -Recurse -File -Include *.txt, *.log -ErrorAction SilentlyContinue
if (-not $logFiles) { Write-Host 'No log files found after extraction.'; exit 0 }

Write-Host 'Error summary (all matching lines):'
foreach ($lf in $logFiles) {
    $matches = @()
    foreach ($p in $patterns) {
        # Use Select-String directly for efficiency
        try { $matches += Select-String -Path $lf.FullName -Pattern $p -SimpleMatch -CaseSensitive:$false -ErrorAction SilentlyContinue } catch { }
    }
    if ($matches.Count -gt 0) {
        # Dedupe by line text to avoid repetition when multiple patterns overlap
        $uniq = $matches | Group-Object Line | ForEach-Object { $_.Group[0] }
        Write-Host ("- {0}" -f $lf.FullName)
        foreach ($m in $uniq) { Write-Host ("  > {0}" -f $m.Line.Trim()) }
    }
}

Write-Host "\nDone. See $OutDir/run_$rid for full logs."
