param(
    [string]$SourceDir = "src"
)

# Function to check if a file has a Doxygen file header
function HasDoxygenFileHeader {
    param([string]$FilePath)
    # Look for /** @file or /// @file at the top, ignoring leading whitespace/comments
    $lines = Get-Content $FilePath | Where-Object { $_.Trim() -ne "" } | Select-Object -First 10
    foreach ($line in $lines) {
        if ($line -match '/\*\*\s*@file' -or $line -match '///\s*@file') {
            return $true
        }
    }
    return $false
}

# Function to find functions without Doxygen docstrings
function GetFunctionsWithoutDocstrings {
    param([string]$FilePath)
    $lines = Get-Content $FilePath
    $functions = @()

    # Regex for C function definition (simplified)
    $functionRegex = '^\s*(?:static\s+)?(?:\w+\s+)+\w+\s*\([^)]*\)\s*\{'

    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match $functionRegex) {
            # Check previous lines for Doxygen comment
            $hasDoc = $false
            for ($j = $i - 1; $j -ge 0 -and $j -ge $i - 5; $j--) {
                if ($lines[$j] -match '/\*\*' -or $lines[$j] -match '///') {
                    $hasDoc = $true
                    break
                }
                if ($lines[$j].Trim() -ne "") {
                    break  # Stop if non-empty line without doc
                }
            }
            if (-not $hasDoc) {
                $functions += $lines[$i].Trim()
            }
        }
    }
    return $functions
}

# Main script
$files = Get-ChildItem -Path $SourceDir -Recurse -Include "*.c", "*.h" | Where-Object { -not $_.PSIsContainer }

$filesWithoutHeader = @()
$filesWithMissingDocs = @{}

foreach ($file in $files) {
    $relativePath = $file.FullName.Replace((Get-Location).Path + "\", "")

    if (-not (HasDoxygenFileHeader -FilePath $file.FullName)) {
        $filesWithoutHeader += $relativePath
    }

    $missingFunctions = GetFunctionsWithoutDocstrings -FilePath $file.FullName
    if ($missingFunctions.Count -gt 0) {
        $filesWithMissingDocs[$relativePath] = $missingFunctions
    }
}

# Output results
Write-Host "Files without Doxygen file headers:"
foreach ($file in $filesWithoutHeader) {
    Write-Host "  $file"
}

Write-Host "`nFiles with functions missing Doxygen docstrings:"
foreach ($file in $filesWithMissingDocs.Keys) {
    Write-Host "  $file"
    foreach ($func in $filesWithMissingDocs[$file]) {
        Write-Host "    $func"
    }
}

if ($filesWithoutHeader.Count -eq 0 -and $filesWithMissingDocs.Count -eq 0) {
    Write-Host "All files have Doxygen headers and function docstrings!"
}
