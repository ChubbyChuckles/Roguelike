# Computes a semantic version from git tags and prepares release notes.
# Usage: pwsh -File scripts/ci/compute-version.ps1 [-OutDir build]
[CmdletBinding()]
param(
    [string]$OutDir = "build",
    [switch]$WriteGithubOutputs
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

# Ensure git is available
$git = Get-Command git -ErrorAction Stop

# Fetch tags to ensure describe works in shallow clones
try { git fetch --tags --force | Out-Null } catch { }

# Compute version via git describe (restrict to SemVer tags only)
# Only consider tags that look like vMAJOR.MINOR.PATCH to avoid chaining on CI-generated names
$describe = (git describe --tags --match "v[0-9]*.[0-9]*.[0-9]*" --long --dirty --always 2>$null)
$shortSha = (git rev-parse --short HEAD).Trim()
$base = $null
$semver = "0.0.0"
$version = "0.0.0+dev+g$shortSha"
$isPrerelease = $true

if ($describe) {
    # Match v1.2.3 or v1.2.3-12-gabcd123 or plain commit
    if ($describe -match '^v(\d+)\.(\d+)\.(\d+)$') {
        $semver = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
        $version = $semver
        $isPrerelease = $false
        $base = $semver
    }
    elseif ($describe -match '^v(\d+)\.(\d+)\.(\d+)-(\d+)-g([0-9a-fA-F]+)') {
        $base = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
        $n = [int]$Matches[4]
        $g = $Matches[5]
        $semver = "$base-$n+g$g"
        $version = $semver
        $isPrerelease = $true
    }
    else {
        # No SemVer tag reachable; treat as dev build without a base tag
        $base = "v0.0.0"
        $semver = "0.0.0+dev+g$shortSha"
        $version = $semver
        $isPrerelease = $true
    }
}

# Write version.json and release_notes.md
@{
    version       = $version
    semver        = $semver
    short_sha     = $shortSha
    is_prerelease = $isPrerelease
} | ConvertTo-Json -Depth 3 | Set-Content -Path (Join-Path $OutDir 'version.json') -Encoding utf8

$notes = @()
$notes += "# Release $version"
$notes += ""
$notes += "- Commit: $shortSha"
$notes += "- Base: $base"
$notesString = ($notes -join "`n") + "`n"
$notesString | Set-Content -Path (Join-Path $OutDir 'release_notes.md') -Encoding utf8

Write-Host "Computed version: $version (base: $base, sha: $shortSha)"

if ($WriteGithubOutputs) {
    "version=$version" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "semver=$semver" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "short_sha=$shortSha" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
    "is_prerelease=$isPrerelease" | Out-File -FilePath $env:GITHUB_OUTPUT -Encoding utf8 -Append
}
