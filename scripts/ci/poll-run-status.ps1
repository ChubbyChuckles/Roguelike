# Polls a GitHub Actions run until it completes (or times out).
# Reads token from the wrapper file `cancel-run-with-token.ps1` (no printing of token).
param(
    [long]$RunId = 17498071515,
    [int]$IntervalSeconds = 5,
    [int]$MaxSeconds = 300
)

$wrapper = Join-Path $PSScriptRoot 'cancel-run-with-token.ps1'
if (-not (Test-Path $wrapper)) {
    Write-Error "Wrapper file not found at $wrapper; please set GITHUB_TOKEN in the environment or create the wrapper."
    exit 2
}

# Extract token silently from the wrapper file. Pattern: $env:GITHUB_TOKEN = 'TOKEN'
try {
    $content = Get-Content -Raw -Path $wrapper -ErrorAction Stop
    if ($content -match '\$env:GITHUB_TOKEN\s*=\s*["\''](?<tok>[^"\'']+)["\'']') {
        $tok = $Matches['tok']
        $env:GITHUB_TOKEN = $tok
    }
    else {
        throw 'token-not-found'
    }
}
catch {
    Write-Error "Failed to read token from wrapper: $_"
    exit 3
}

$uri = "https://api.github.com/repos/ChubbyChuckles/Roguelike/actions/runs/$RunId"
$headers = @{ Authorization = "token $env:GITHUB_TOKEN"; 'User-Agent' = 'roguelike-ci-poller' }

$elapsed = 0
while ($elapsed -lt $MaxSeconds) {
    try {
        $r = Invoke-RestMethod -Uri $uri -Headers $headers -ErrorAction Stop
    }
    catch {
        Write-Warning "HTTP request failed: $($_.Exception.Message)"
        Start-Sleep -Seconds $IntervalSeconds
        $elapsed += $IntervalSeconds
        continue
    }
    $ts = Get-Date -Format o
    Write-Host "$ts status=$($r.status) conclusion=$($r.conclusion)"
    if ($r.status -eq 'completed') {
        Write-Host "Run completed: conclusion=$($r.conclusion)"
        exit 0
    }
    Start-Sleep -Seconds $IntervalSeconds
    $elapsed += $IntervalSeconds
}
Write-Error "Timed out after $MaxSeconds seconds waiting for run $RunId to complete."
exit 1
