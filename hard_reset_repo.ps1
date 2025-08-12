## Password prompt before reset
$password = Read-Host -AsSecureString 'Enter password to reset the repo'
$bstr = [System.Runtime.InteropServices.Marshal]::SecureStringToBSTR($password)
$plain = [System.Runtime.InteropServices.Marshal]::PtrToStringAuto($bstr)
if ($plain -ne 'egon') {
    Write-Host 'Incorrect password. Aborting.' -ForegroundColor Red
    exit 1
}
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