param(
    [Parameter(Mandatory=$true)]
    [string]$CommitId
)

# Remove untracked files and directories
Write-Host "Cleaning untracked files..."
git clean -fd

# Hard reset local repo
Write-Host "Resetting local repository to $CommitId..."
git reset --hard $CommitId

# Force push to remote (origin, main branch)
Write-Host "Force pushing to remote main branch..."
git push origin HEAD:main --force