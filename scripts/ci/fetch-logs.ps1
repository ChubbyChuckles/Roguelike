param(
    [Parameter(Mandatory = $false)][int]$RunId,
    [string]$OutDir = "build/ci_logs"
)

function Test-GhCli {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        Write-Error "GitHub CLI (gh) not found. Install from https://cli.github.com and run 'gh auth login'."
        exit 1
    }
}

Test-GhCli

if (-not $RunId) {
    $RunId = (gh run list --workflow CI --limit 1 --json databaseId | ConvertFrom-Json)[0].databaseId
}
if (-not $RunId) {
    Write-Error "No CI runs found."
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "Downloading job logs for run #$RunId to $OutDir ..."
gh run view $RunId --log > "$OutDir/run_$RunId.log"

Write-Host "Downloading artifacts for run #$RunId ..."
gh run download $RunId --dir $OutDir 2>$null

Write-Host "Done. Logs and artifacts saved to $OutDir"
