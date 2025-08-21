#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Moves a C/C++ file pair (.c and .h) to a new directory and updates all imports throughout the project.

.DESCRIPTION
    This script moves a .c file and its corresponding .h file to a target directory,
    then updates all #include statements throughout the entire project to maintain
    build compatibility. It handles both absolute and relative path references.

.PARAMETER FilePath
    The path(s) to the .c file(s) to move (the .h file(s) are automatically detected)

.PARAMETER TargetDirectory
    The directory where the files should be moved

.PARAMETER ProjectRoot
    The root directory of the project (defaults to current directory's parent if in tools/)

.EXAMPLE
    .\move_file_pair.ps1 -FilePath "src\core\example.c" -TargetDirectory "src\core\utilities"
    
.EXAMPLE
    .\move_file_pair.ps1 -FilePath @("src\core\test1.c", "src\core\test2.c") -TargetDirectory "src\core\subsystem"
    
.EXAMPLE
    .\move_file_pair.ps1 -FilePath "C:\project\src\core\test.c" -TargetDirectory "src\core\subsystem" -ProjectRoot "C:\project"
#>

param(
    [Parameter(Mandatory=$true, HelpMessage="Path(s) to the .c file(s) to move")]
    [string[]]$FilePath,
    
    [Parameter(Mandatory=$true, HelpMessage="Target directory for the files")]
    [string]$TargetDirectory,
    
    [Parameter(HelpMessage="Project root directory (auto-detected if not specified)")]
    [string]$ProjectRoot = ""
)

# Auto-detect project root if not specified
if ([string]::IsNullOrEmpty($ProjectRoot)) {
    $currentDir = Get-Location
    if ($currentDir.Path -match "\\tools$") {
        $ProjectRoot = Split-Path $currentDir.Path -Parent
    } else {
        $ProjectRoot = $currentDir.Path
    }
    Write-Host "Auto-detected project root: $ProjectRoot" -ForegroundColor Green
}

# Ensure we're working with absolute paths
$ProjectRoot = Resolve-Path $ProjectRoot -ErrorAction Stop
$TargetDirectory = if ([System.IO.Path]::IsPathRooted($TargetDirectory)) { $TargetDirectory } else { Join-Path $ProjectRoot $TargetDirectory }

# Convert file paths to absolute paths
$AbsoluteFilePaths = @()
foreach ($file in $FilePath) {
    $absPath = if ([System.IO.Path]::IsPathRooted($file)) { $file } else { Join-Path $ProjectRoot $file }
    $AbsoluteFilePaths += $absPath
}

Write-Host "=== Move File Pairs Tool ===" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot"
Write-Host "Target Directory: $TargetDirectory"
Write-Host "Files to move: $($AbsoluteFilePaths.Count)"
foreach ($file in $AbsoluteFilePaths) {
    Write-Host "  - $(Split-Path $file -Leaf)" -ForegroundColor Gray
}

# Validate inputs
$fileInfoList = @()
$allHeaderFiles = @()
$replacementMap = @{}

foreach ($sourceFile in $AbsoluteFilePaths) {
    if (-not (Test-Path $sourceFile)) {
        Write-Error "Source file not found: $sourceFile"
        exit 1
    }

    if (-not $sourceFile.EndsWith('.c', [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Error "Source file must be a .c file: $sourceFile"
        exit 1
    }
    
    # Determine file names and paths
    $sourceFileInfo = Get-Item $sourceFile
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($sourceFileInfo.Name)
    $sourceDir = $sourceFileInfo.Directory.FullName
    $headerFile = Join-Path $sourceDir "$baseName.h"
    
    $targetCFile = Join-Path $TargetDirectory "$baseName.c"
    $targetHFile = Join-Path $TargetDirectory "$baseName.h"
    
    # Check if header file exists
    $hasHeaderFile = Test-Path $headerFile
    
    # Calculate relative paths for include updates
    $sourceRelativeDir = $sourceDir.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
    $targetRelativeDir = $TargetDirectory.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
    
    # Determine old and new include patterns
    $oldIncludePath = "$sourceRelativeDir/$baseName.h".Replace('//', '/')
    $newIncludePath = "$targetRelativeDir/$baseName.h".Replace('//', '/')
    
    # Calculate alternative include path patterns for different reference styles
    $oldCoreRelativePath = ""
    $newCoreRelativePath = ""
    
    # Handle core-relative paths (src/core/* -> core/*)
    if ($sourceRelativeDir -match "^src/core" -and $targetRelativeDir -match "^src/core") {
        $oldCoreRelativePath = $sourceRelativeDir.Replace("src/core", "core").Replace('//', '/') + "/$baseName.h"
        $newCoreRelativePath = $targetRelativeDir.Replace("src/core", "core").Replace('//', '/') + "/$baseName.h"
    } elseif ($sourceRelativeDir -match "^src/core" -and $targetRelativeDir -notmatch "^src/core") {
        # Moving from core to non-core: old core-relative path exists, new doesn't
        $oldCoreRelativePath = $sourceRelativeDir.Replace("src/core", "core").Replace('//', '/') + "/$baseName.h"
        $newCoreRelativePath = ""
    } elseif ($sourceRelativeDir -notmatch "^src/core" -and $targetRelativeDir -match "^src/core") {
        # Moving from non-core to core: old core-relative path doesn't exist, new does
        $oldCoreRelativePath = ""
        $newCoreRelativePath = $targetRelativeDir.Replace("src/core", "core").Replace('//', '/') + "/$baseName.h"
    }
    
    # Store file info
    $fileInfo = @{
        SourceFile = $sourceFile
        HeaderFile = $headerFile
        HasHeaderFile = $hasHeaderFile
        BaseName = $baseName
        TargetCFile = $targetCFile
        TargetHFile = $targetHFile
        OldIncludePath = $oldIncludePath
        NewIncludePath = $newIncludePath
        OldCoreRelativePath = $oldCoreRelativePath
        NewCoreRelativePath = $newCoreRelativePath
    }
    
    $fileInfoList += $fileInfo
    
    if ($hasHeaderFile) {
        $allHeaderFiles += $headerFile
        
        # Build replacement map for this file
        $replacementMap[$oldIncludePath] = $newIncludePath
        if ($oldCoreRelativePath) {
            $replacementMap[$oldCoreRelativePath] = $newCoreRelativePath
        }
        
        # For same-directory references, use just the filename
        $targetIsCore = ($targetRelativeDir -eq "src/core")
        if ($targetIsCore) {
            # When moving to src/core, other files in src/core should reference with just filename
            $replacementMap["$baseName.h"] = "$baseName.h"  # Keep filename references as filenames
        } else {
            $replacementMap["$baseName.h"] = $newIncludePath  # For other directories, use full path
        }
        
        # Add comprehensive relative path patterns that might exist
        # Generate patterns for different directory depths and common reference styles
        $relativePatternsToReplace = @()
        
        # Add patterns for going up directory levels to reach the file
        for ($i = 1; $i -le 5; $i++) {
            $upPattern = ("../" * $i) + $oldIncludePath
            $relativePatternsToReplace += $upPattern
        }
        
        # Add common project-relative patterns
        $relativePatternsToReplace += @(
            $oldIncludePath,                    # Full project-relative path
            "$baseName.h"                       # Just filename
        )
        
        # Add core-specific patterns if applicable
        if ($oldCoreRelativePath) {
            $relativePatternsToReplace += $oldCoreRelativePath
        }
        
        # Add source-specific patterns for common directory structures
        if ($sourceRelativeDir -match "src/") {
            $relativePatternsToReplace += $sourceRelativeDir.Replace("src/", "") + "/$baseName.h"
        }
        
        foreach ($pattern in $relativePatternsToReplace) {
            if ($pattern -and $pattern -ne "") {
                # Determine the appropriate replacement based on context
                if ($targetIsCore -and ($pattern -eq "$baseName.h" -or $pattern -match "src/core/$baseName\.h")) {
                    # For core directory, use just filename for references within core
                    $replacementMap[$pattern] = "$baseName.h"
                } else {
                    $replacementMap[$pattern] = $newIncludePath
                }
            }
        }
    }
}

Write-Host "`nFile Analysis:" -ForegroundColor Yellow
foreach ($fileInfo in $fileInfoList) {
    Write-Host "  C File: $($fileInfo.BaseName).c -> $TargetDirectory"
    Write-Host "  H File: $(if ($fileInfo.HasHeaderFile) { "$($fileInfo.BaseName).h" } else { "NOT FOUND" }) -> $TargetDirectory"
    if (-not $fileInfo.HasHeaderFile) {
        Write-Warning "Header file not found: $($fileInfo.HeaderFile)"
    }
}

# Create target directory if it doesn't exist
if (-not (Test-Path $TargetDirectory)) {
    Write-Host "Creating target directory: $TargetDirectory" -ForegroundColor Green
    New-Item -Path $TargetDirectory -ItemType Directory -Force | Out-Null
}

# Check if any files are missing headers and get user confirmation
$missingHeaders = $fileInfoList | Where-Object { -not $_.HasHeaderFile }
if ($missingHeaders.Count -gt 0) {
    Write-Warning "Some header files are missing:"
    foreach ($missing in $missingHeaders) {
        Write-Host "  - $($missing.BaseName).h not found" -ForegroundColor Red
    }
    $response = Read-Host "Continue with only .c files for those missing headers? (y/n)"
    if ($response -ne 'y' -and $response -ne 'Y') {
        Write-Host "Operation cancelled."
        exit 0
    }
}

Write-Host "`nInclude Path Analysis:" -ForegroundColor Yellow
foreach ($fileInfo in $fileInfoList) {
    if ($fileInfo.HasHeaderFile) {
        Write-Host "  $($fileInfo.BaseName):"
        Write-Host "    Old: $($fileInfo.OldIncludePath)"
        Write-Host "    New: $($fileInfo.NewIncludePath)"
        if ($fileInfo.OldCoreRelativePath) {
            Write-Host "    Old (core-relative): $($fileInfo.OldCoreRelativePath)"
            Write-Host "    New (core-relative): $($fileInfo.NewCoreRelativePath)"
        }
    }
}

# Function to update includes in a file
function Update-IncludesInFile {
    param(
        [string]$FilePath,
        [hashtable]$ReplacementMap
    )
    
    try {
        $content = Get-Content -Path $FilePath -Raw -ErrorAction Stop
        if (-not $content) { return $false }
        
        $changed = $false
        $currentFileDir = Split-Path $FilePath -Parent
        $currentRelativeDir = $currentFileDir.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
        
        foreach ($oldInclude in $ReplacementMap.Keys) {
            $newInclude = $ReplacementMap[$oldInclude]
            
            # Calculate the correct relative path from current file to the new include location
            $newIncludeDir = Split-Path $newInclude -Parent
            $correctPath = $newInclude
            
            # For files within src/core/, calculate proper relative path
            if ($currentRelativeDir -match "^src/core" -and $newIncludeDir -match "^src/core") {
                # Both in src/core - calculate relative path
                $currentParts = $currentRelativeDir -split '/'
                $targetParts = $newIncludeDir -split '/'
                
                # Find common parts
                $commonLength = 0
                $minLength = [Math]::Min($currentParts.Length, $targetParts.Length)
                for ($i = 0; $i -lt $minLength; $i++) {
                    if ($currentParts[$i] -eq $targetParts[$i]) {
                        $commonLength++
                    } else {
                        break
                    }
                }
                
                # Calculate relative path
                $upLevels = $currentParts.Length - $commonLength
                $downPath = $targetParts[$commonLength..($targetParts.Length-1)] -join '/'
                $headerName = Split-Path $newInclude -Leaf
                
                if ($upLevels -gt 0 -and $downPath) {
                    $correctPath = ("../" * $upLevels) + $downPath + "/" + $headerName
                } elseif ($upLevels -gt 0) {
                    $correctPath = ("../" * ($upLevels-1)) + $headerName
                } elseif ($downPath) {
                    $correctPath = $downPath + "/" + $headerName
                } else {
                    $correctPath = $headerName
                }
            } elseif ($currentRelativeDir -match "tests/unit") {
                # For test files, use full relative path from tests/unit/
                $correctPath = "../../$newInclude"
            }
            
            # Handle exact path matches
            $patterns = @(
                "#include `"$oldInclude`"",
                "#include <$oldInclude>",
                # Handle core-relative paths
                "#include `"$($oldInclude.Replace('src/core/', 'core/'))`"",
                # Handle various relative path patterns
                "#include `"../$oldInclude`"",
                "#include `"../../$oldInclude`"",
                "#include `"../../../$oldInclude`""
            )
            
            foreach ($pattern in $patterns) {
                if ($content -match [regex]::Escape($pattern)) {
                    $replacement = $pattern.Replace($oldInclude, $correctPath).Replace("core/", "").Replace("src/core/", "")
                    # Clean up double slashes and fix angle/quote brackets
                    if ($pattern.StartsWith("#include <")) {
                        $replacement = "#include <$correctPath>"
                    } else {
                        $replacement = "#include `"$correctPath`""
                    }
                    $content = $content -replace [regex]::Escape($pattern), $replacement
                    $changed = $true
                }
            }
            
            # Handle filename-only includes
            $baseName = [System.IO.Path]::GetFileNameWithoutExtension($oldInclude)
            $filenamePattern = "#include `"$baseName.h`""
            if ($content -match [regex]::Escape($filenamePattern)) {
                # If target is in same directory, keep filename-only; otherwise use calculated path
                $targetDir = Split-Path ($newIncludeDir) -Leaf
                $currentDir = Split-Path $currentRelativeDir -Leaf
                
                if ($currentRelativeDir -eq $newIncludeDir) {
                    # Same directory - keep filename only
                    # No change needed
                } else {
                    # Different directory - use calculated relative path
                    $replacement = "#include `"$correctPath`""
                    $content = $content -replace [regex]::Escape($filenamePattern), $replacement
                    $changed = $true
                }
            }
        }
        
        # Special handling for test files that may have inconsistent include patterns
        if ($FilePath -match "tests[\\/]unit[\\/]") {
            foreach ($oldInclude in $ReplacementMap.Keys) {
                $newInclude = $ReplacementMap[$oldInclude]
                
                # Fix common test file include patterns
                $testPatterns = @(
                    "#include `"src/$oldInclude`"",
                    "#include `"core/$($oldInclude.Replace('src/core/', ''))`""
                )
                
                foreach ($pattern in $testPatterns) {
                    if ($content -match [regex]::Escape($pattern)) {
                        # For test files, use the full relative path from tests/unit/
                        $testInclude = "../../$newInclude"
                        $content = $content -replace [regex]::Escape($pattern), "#include `"$testInclude`""
                        $changed = $true
                    }
                }
            }
        }
        
        if ($changed) {
            Set-Content -Path $FilePath -Value $content -NoNewline
            return $true
        }

        return $false
    }
    catch {
        Write-Warning "Failed to process file $FilePath`: $_"
        return $false
    }
}

# Function to calculate relative path between directories
function Get-RelativePath {
    param([string]$From, [string]$To)
    
    $fromParts = $From.Replace($ProjectRoot, "").Split('\', [System.StringSplitOptions]::RemoveEmptyEntries)
    $toParts = $To.Replace($ProjectRoot, "").Split('\', [System.StringSplitOptions]::RemoveEmptyEntries)
    
    # Find common base
    $commonLength = 0
    $minLength = [Math]::Min($fromParts.Length, $toParts.Length)
    for ($i = 0; $i -lt $minLength; $i++) {
        if ($fromParts[$i] -eq $toParts[$i]) {
            $commonLength++
        } else {
            break
        }
    }
    
    # Build relative path
    $upLevels = $fromParts.Length - $commonLength
    
    $relativeParts = @()
    for ($i = 0; $i -lt $upLevels; $i++) {
        $relativeParts += ".."
    }
    for ($i = $commonLength; $i -lt $toParts.Length; $i++) {
        $relativeParts += $toParts[$i]
    }
    
    if ($relativeParts.Length -eq 0) {
        return "."
    }
    
    return ($relativeParts -join '/')
}

Write-Host "`nSearching for files to update..." -ForegroundColor Yellow

# Find all source files to update
$filesToUpdate = @()
$sourceExtensions = @('*.c', '*.cpp', '*.cc', '*.cxx', '*.h', '*.hpp', '*.hxx')

foreach ($ext in $sourceExtensions) {
    $files = Get-ChildItem -Path $ProjectRoot -Recurse -Filter $ext -File | Where-Object {
        $_.FullName -notlike "*\.git\*" -and 
        $_.FullName -notlike "*\build\*" -and
        $_.FullName -notlike "*\bin\*" -and
        $_.FullName -notlike "*_backup*"
    }
    $filesToUpdate += $files
}

# Also check CMakeLists.txt files
$cmakeFiles = Get-ChildItem -Path $ProjectRoot -Recurse -Filter "CMakeLists.txt" -File
$filesToUpdate += $cmakeFiles

Write-Host "Found $($filesToUpdate.Count) files to check for updates."

# Update includes in all relevant files
Write-Host "`nUpdating include statements..." -ForegroundColor Yellow
$updatedCount = 0
$progressCount = 0

foreach ($file in $filesToUpdate) {
    $progressCount++
    if ($progressCount % 50 -eq 0) {
        Write-Host "  Progress: $progressCount/$($filesToUpdate.Count)" -ForegroundColor Gray
    }
    
    if (Update-IncludesInFile -FilePath $file.FullName -ReplacementMap $replacementMap) {
        $updatedCount++
        Write-Host "  Updated: $($file.Name)" -ForegroundColor Green
    }
}

Write-Host "Updated includes in $updatedCount files." -ForegroundColor Green

# Move the files
Write-Host "`nMoving files..." -ForegroundColor Yellow

$movedFiles = @()
try {
    foreach ($fileInfo in $fileInfoList) {
        # Move .c file
        Move-Item -Path $fileInfo.SourceFile -Destination $fileInfo.TargetCFile -Force
        Write-Host "  Moved: $($fileInfo.BaseName).c" -ForegroundColor Green
        $movedFiles += $fileInfo.TargetCFile
        
        # Move .h file if it exists
        if ($fileInfo.HasHeaderFile) {
            Move-Item -Path $fileInfo.HeaderFile -Destination $fileInfo.TargetHFile -Force
            Write-Host "  Moved: $($fileInfo.BaseName).h" -ForegroundColor Green
            $movedFiles += $fileInfo.TargetHFile
        }
    }
    
    Write-Host "`nFiles moved successfully!" -ForegroundColor Green
    
    # Update includes in moved files themselves
    Write-Host "`nUpdating includes in moved files..." -ForegroundColor Yellow
    foreach ($fileInfo in $fileInfoList) {
        # Update the moved C file's includes
        $movedCFile = $fileInfo.TargetCFile
        if (Test-Path $movedCFile) {
            $content = Get-Content -Path $movedCFile -Raw
            if ($content) {
                $changed = $false
                $targetDir = Split-Path $movedCFile -Parent
                $targetRelativeDir = $targetDir.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
                
                # Fix includes for files now in the same directory (should be just filename)
                foreach ($otherFileInfo in $fileInfoList) {
                    if ($otherFileInfo.HasHeaderFile) {
                        $headerName = "$($otherFileInfo.BaseName).h"
                        $patterns = @(
                            "#include `"$($otherFileInfo.OldIncludePath)`"",
                            "#include `"$($otherFileInfo.OldCoreRelativePath)`"",
                            "#include `"../$($otherFileInfo.BaseName)/$headerName`"",
                            "#include `"../$(Split-Path $targetRelativeDir -Leaf)/$headerName`""
                        )
                        foreach ($pattern in $patterns) {
                            if ($pattern -and $content -match [regex]::Escape($pattern)) {
                                $content = $content -replace [regex]::Escape($pattern), "#include `"$headerName`""
                                $changed = $true
                            }
                        }
                    }
                }
                
                # Fix includes for other core headers - calculate proper relative paths
                # Get the current file's directory relative to src/core/
                $currentFileDir = Split-Path $targetRelativeDir -Leaf
                
                # Pattern: #include "core/something/header.h" -> #include "../something/header.h"
                $content = [regex]::Replace($content, '#include "core/([^"]+)"', { param($match)
                    $targetPath = $match.Groups[1].Value
                    return "#include `"../$targetPath`""
                })
                
                # Handle src/core includes with a two-step approach
                # First, find all src/core includes and process them
                $srcCoreMatches = [regex]::Matches($content, '#include "src/core/([^"]+)"')
                foreach ($match in $srcCoreMatches) {
                    $oldInclude = $match.Value
                    $targetPath = $match.Groups[1].Value
                    
                    # If the target path is in the same directory as the moved file, just use filename
                    if ($targetPath.Contains("$currentFileDir/")) {
                        $fileName = Split-Path $targetPath -Leaf
                        $newInclude = "#include `"$fileName`""
                    } else {
                        # Otherwise, go up one level and then to the target
                        $newInclude = "#include `"../$targetPath`""
                    }
                    
                    $content = $content.Replace($oldInclude, $newInclude)
                }
                
                # Fix any remaining absolute project paths
                $absoluteMatches = [regex]::Matches($content, '#include "([^"]*src/core/[^"]+)"')
                foreach ($match in $absoluteMatches) {
                    $oldInclude = $match.Value
                    $fullPath = $match.Groups[1].Value
                    $corePath = $fullPath -replace '.*src/core/', ''
                    
                    # If the target path is in the same directory as the moved file, just use filename
                    if ($corePath.Contains("$currentFileDir/")) {
                        $fileName = Split-Path $corePath -Leaf
                        $newInclude = "#include `"$fileName`""
                    } else {
                        $newInclude = "#include `"../$corePath`""
                    }
                    
                    $content = $content.Replace($oldInclude, $newInclude)
                }
                
                if ($changed) {
                    Set-Content -Path $movedCFile -Value $content -NoNewline
                    Write-Host "  Updated includes in: $(Split-Path $movedCFile -Leaf)" -ForegroundColor Green
                }
            }
        }
        
        # Update the moved H file's includes if it exists
        if ($fileInfo.HasHeaderFile) {
            $movedHFile = $fileInfo.TargetHFile
            if (Test-Path $movedHFile) {
                $content = Get-Content -Path $movedHFile -Raw
                if ($content) {
                    $changed = $false
                    $targetDir = Split-Path $movedHFile -Parent
                    $targetRelativeDir = $targetDir.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
                    
                    # Fix includes for files now in the same directory
                    foreach ($otherFileInfo in $fileInfoList) {
                        if ($otherFileInfo.HasHeaderFile) {
                            $headerName = "$($otherFileInfo.BaseName).h"
                            $patterns = @(
                                "#include `"$($otherFileInfo.OldIncludePath)`"",
                                "#include `"$($otherFileInfo.OldCoreRelativePath)`"",
                                "#include `"../$($otherFileInfo.BaseName)/$headerName`"",
                                "#include `"../$(Split-Path $targetRelativeDir -Leaf)/$headerName`""
                            )
                            foreach ($pattern in $patterns) {
                                if ($pattern -and $content -match [regex]::Escape($pattern)) {
                                    $content = $content -replace [regex]::Escape($pattern), "#include `"$headerName`""
                                    $changed = $true
                                }
                            }
                        }
                    }
                    
                    # Fix includes for other core headers using relative paths
                    $currentFileDir = Split-Path $targetRelativeDir -Leaf
                    
                    $content = [regex]::Replace($content, '#include "core/([^"]+)"', { param($match)
                        return "#include `"../$($match.Groups[1].Value)`""
                    })
                    
                    # Handle src/core includes with a two-step approach
                    $srcCoreMatches = [regex]::Matches($content, '#include "src/core/([^"]+)"')
                    foreach ($match in $srcCoreMatches) {
                        $oldInclude = $match.Value
                        $targetPath = $match.Groups[1].Value
                        
                        # If the target path is in the same directory as the moved file, just use filename
                        if ($targetPath.Contains("$currentFileDir/")) {
                            $fileName = Split-Path $targetPath -Leaf
                            $newInclude = "#include `"$fileName`""
                        } else {
                            # Otherwise, go up one level and then to the target
                            $newInclude = "#include `"../$targetPath`""
                        }
                        
                        $content = $content.Replace($oldInclude, $newInclude)
                    }
                    
                    # Fix any remaining absolute project paths
                    $absoluteMatches = [regex]::Matches($content, '#include "([^"]*src/core/[^"]+)"')
                    foreach ($match in $absoluteMatches) {
                        $oldInclude = $match.Value
                        $fullPath = $match.Groups[1].Value
                        $corePath = $fullPath -replace '.*src/core/', ''
                        
                        # If the target path is in the same directory as the moved file, just use filename
                        if ($corePath.Contains("$currentFileDir/")) {
                            $fileName = Split-Path $corePath -Leaf
                            $newInclude = "#include `"$fileName`""
                        } else {
                            $newInclude = "#include `"../$corePath`""
                        }
                        
                        $content = $content.Replace($oldInclude, $newInclude)
                    }
                    
                    if ($changed) {
                        Set-Content -Path $movedHFile -Value $content -NoNewline
                        Write-Host "  Updated includes in: $(Split-Path $movedHFile -Leaf)" -ForegroundColor Green
                    }
                }
            }
        }
    }
}
catch {
    Write-Error "Failed to move files: $_"
    exit 1
}

# Update CMakeLists.txt to reflect new file paths
Write-Host "`nUpdating CMakeLists.txt..." -ForegroundColor Yellow

foreach ($cmakeFile in $cmakeFiles) {
    try {
        $content = Get-Content -Path $cmakeFile.FullName -Raw
        if (-not $content) { continue }
        
        $originalContent = $content
        
        foreach ($fileInfo in $fileInfoList) {
            # Update source file references
            $oldSourceRef = $fileInfo.SourceFile.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
            $newSourceRef = $fileInfo.TargetCFile.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
            
            if ($content -match [regex]::Escape($oldSourceRef)) {
                $content = $content -replace [regex]::Escape($oldSourceRef), $newSourceRef
            }
            
            if ($fileInfo.HasHeaderFile) {
                $oldHeaderRef = $fileInfo.HeaderFile.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
                $newHeaderRef = $fileInfo.TargetHFile.Replace($ProjectRoot, "").TrimStart('\', '/').Replace('\', '/')
                
                if ($content -match [regex]::Escape($oldHeaderRef)) {
                    $content = $content -replace [regex]::Escape($oldHeaderRef), $newHeaderRef
                }
            }
        }
        
        if ($content -ne $originalContent) {
            Set-Content -Path $cmakeFile.FullName -Value $content -NoNewline
            Write-Host "  Updated: $($cmakeFile.Name)" -ForegroundColor Green
        }
    }
    catch {
        Write-Warning "Failed to update CMakeLists.txt at $($cmakeFile.FullName): $_"
    }
}

# Verify the build
Write-Host "`nTesting build..." -ForegroundColor Yellow
$buildDir = Join-Path $ProjectRoot "build"

if (Test-Path $buildDir) {
    try {
        Set-Location $ProjectRoot
        $result = & cmake --build build --config Debug -j4 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Build successful!" -ForegroundColor Green
        } else {
            Write-Warning "❌ Build failed. You may need to manually fix remaining issues."
            Write-Host "Build output (last 20 lines):" -ForegroundColor Yellow
            $result | Select-Object -Last 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
        }
    }
    catch {
        Write-Warning "Failed to run build test: $_"
    }
} else {
    Write-Host "No build directory found - skipping build test." -ForegroundColor Yellow
}

# Summary
Write-Host "`n=== Operation Complete ===" -ForegroundColor Cyan
Write-Host "✅ Files moved: $($fileInfoList.Count) .c files$(if ($allHeaderFiles.Count -gt 0) { " and $($allHeaderFiles.Count) .h files" })"
Write-Host "✅ Include statements updated in: $updatedCount files"
Write-Host "✅ Target directory: $TargetDirectory"

Write-Host "`nMoved files:" -ForegroundColor Green
foreach ($fileInfo in $fileInfoList) {
    Write-Host "  - $($fileInfo.BaseName).c$(if ($fileInfo.HasHeaderFile) { " and $($fileInfo.BaseName).h" })"
}

if ($updatedCount -eq 0) {
    Write-Host "⚠️  No include statements needed updating - please verify manually." -ForegroundColor Yellow
}

Write-Host "`nRecommended next steps:"
Write-Host "1. Test the build: cmake --build build --config Debug"
Write-Host "2. Run unit tests to ensure functionality"
Write-Host "3. Commit changes if everything works correctly"

Write-Host "`nOperation completed successfully! 🎉" -ForegroundColor Green
