# Find-FlakyTests PowerShell Module

param(
    [ValidateSet("Debug", "Release", "Both")]
    [string]$BuildConfigs = "Both",
    [string]$BuildDir = "build",
    [int]$NumRuns = 50,
    [switch]$DryRun,
    [switch]$Verbose,
    [string]$OutputDir = ".",
    [double]$Threshold = 0.01,
    [ValidateSet("Console", "JSON", "CSV", "HTML", "All")]
    [string]$ReportFormat = "All"
)

# ... (all previous functions here, wrapped in try-catch where appropriate)

# Example wrapped function
function Get-ProjectRoot {
    param([string]$StartPath = $PWD.Path)
    try {
        $CurrentPath = $StartPath
        while ($CurrentPath -ne $null) {
            if (Test-Path (Join-Path $CurrentPath "CMakeLists.txt") -PathType Leaf -or Test-Path (Join-Path $CurrentPath ".git") -PathType Container) {
                return $CurrentPath
            }
            $Parent = Split-Path $CurrentPath -Parent
            if ($Parent -eq $CurrentPath) { break }
            $CurrentPath = $Parent
        }
        throw "Project root not found."
    } catch {
        Write-Error "Root detection failed: $_"
        return $null
    }
}

# For Invoke-TestRuns, add validation and compression
function Invoke-TestRuns {
    # ... previous
    for ($i = 1; $i -le $NumRuns; $i++) {
        try {
            # ... execution
            $Output = Get-Content $LogPath -Raw
            if ([string]::IsNullOrWhiteSpace($Output)) {
                $Output = "No output captured - possible execution failure"
            }
            if ((Get-Item $LogPath).Length -gt 1MB) {
                Compress-Archive -Path $LogPath -DestinationPath ($LogPath + ".zip") -Force
                Remove-Item $LogPath
                $LogPath = $LogPath + ".zip"
            }
        } catch {
            Write-Warning "Run $i failed: $_"
            $ExitCode = -1
            $Output = "Error: $_"
        } finally {
            # Cleanup temp if not dry run
            if (-not $DryRun -and $TempDir) { Remove-Item $TempDir -Recurse -Force -ErrorAction SilentlyContinue }
        }
        # ... add to runs
    }
}

# Portability check
if ($PSVersionTable.PSEdition -eq "Core") {
    Write-Verbose "PowerShell Core detected - using cross-platform paths."
    # Adjust paths if needed for Linux
    $BuildDir = $BuildDir -replace '\\', '/'
}

# Export functions
Export-ModuleMember -Function Get-ProjectRoot, Invoke-BuildConfig, Invoke-TestRuns, Invoke-VariationInjection, Analyze-TestResults, Categorize-Flakiness, Export-Reports

# Main script wrapper (for .ps1 compatibility)
try {
    $ProjectRoot = Get-ProjectRoot
    # ... rest of main
} catch {
    Write-Error "Script execution failed: $_"
    exit 1
} finally {
    # Global cleanup if needed
}
