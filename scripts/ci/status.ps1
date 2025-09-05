param(
    [int]$Limit = 10
)

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Error "GitHub CLI (gh) not found. Install from https://cli.github.com and run 'gh auth login'."
    exit 1
}

# List recent runs for workflow named 'CI'
$runsJson = gh run list --workflow CI --limit $Limit --json databaseId, displayTitle, headBranch, status, conclusion, createdAt, event, actor 2>$null
if (-not $runsJson) {
    Write-Error "No runs found or 'gh' not authenticated. Try 'gh auth login'."
    exit 1
}

$runs = $runsJson | ConvertFrom-Json
"Recent CI runs (latest $Limit):"
$runs | ForEach-Object {
    $c = $_.conclusion
    if (-not $c) { $c = "-" }
    "#{0}  {1}  [{2}]  {3}  status={4}  conclusion={5}  by {6}" -f $_.databaseId, $_.displayTitle, $_.headBranch, $_.createdAt, $_.status, $c, $_.actor
}

# Show details for the most recent failed run if any
$failed = $runs | Where-Object { $_.conclusion -eq 'failure' -or $_.conclusion -eq 'cancelled' -or $_.conclusion -eq 'timed_out' } | Select-Object -First 1
if ($failed) {
    "`nLatest non-successful run details (#${($failed.databaseId)}):"
    gh run view $failed.databaseId --log || gh run view $failed.databaseId
}
