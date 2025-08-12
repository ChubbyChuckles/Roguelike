# Ultra-robust recursive asset sorter for all biomes

$baseOld = "F:\Coding\Roguelike\assets"
$baseNew = "$baseOld\biomes"

$biomeMap = @{
    "Pixel Crawler - Castle Environment 0.3" = "castle"
    "Pixel Crawler - Cave" = "cave"
    "Pixel Crawler - Cemetery 0.4" = "cemetery"
    "Pixel Crawler - Desert" = "desert"
    "Pixel Crawler - Fairy Forest 1.7" = "fairy_forest"
    "Pixel Crawler - Forge 1.2" = "forge"
    "Pixel Crawler - Garden Environment" = "garden"
    "Pixel Crawler - Hideout 1.0" = "hideout"
    "Pixel Crawler - Library" = "library"
    "Pixel Crawler - Sewer" = "sewer"
}

$neverOverwrite = @("Terms.txt")


# Normalize a single path segment (folder or file name)
function Normalize-Segment {
    param($segment)
    return ($segment -replace ' ', '_' -replace '-', '_' -replace '[^a-zA-Z0-9_.]', '').ToLower()
}

$log = @()
foreach ($oldBiome in $biomeMap.Keys) {
    $newBiome = $biomeMap[$oldBiome]

    $oldRoot = Join-Path -Path $baseOld -ChildPath $oldBiome
    if (-not (Test-Path -LiteralPath $oldRoot)) { continue }

    Write-Host "Processing $oldBiome..."

    Get-ChildItem -LiteralPath $oldRoot -Recurse -File | ForEach-Object {
        $src = $_.FullName
        $rel = $src.Substring($oldRoot.Length).TrimStart("\", "/")
        # Normalize only the destination path
        $relParts = $rel -split "[\\/]"
        $relNorm = ($relParts | ForEach-Object { Normalize-Segment $_ }) -join "\"
        $dst = "$baseNew\$newBiome\$relNorm"

        # Never overwrite user-edited files
        if ($neverOverwrite -contains $_.Name -and (Test-Path $dst)) {
            Write-Host "Skipping user-edited file: $dst"
            $log += "SKIPPED: $src -> $dst"
            return
        }

    $dstDir = Split-Path $dst
    if (-not (Test-Path -LiteralPath $dstDir)) { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }

        try {
            Move-Item -LiteralPath $src -Destination $dst -Force
            Write-Host "Moved: $src -> $dst"
            $log += "MOVED: $src -> $dst"
        } catch {
            Write-Host "FAILED: $src -> $dst ($($_.Exception.Message))"
            $log += "FAILED: $src -> $dst ($($_.Exception.Message))"
        }
    }
}

    # After moving, delete all empty directories in the old biome folders
    foreach ($oldBiome in $biomeMap.Keys) {
        $oldRoot = Join-Path -Path $baseOld -ChildPath $oldBiome
        if (-not (Test-Path -LiteralPath $oldRoot)) { continue }
        # Get all directories, deepest first
        Get-ChildItem -LiteralPath $oldRoot -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
            if (-not (Get-ChildItem -LiteralPath $_.FullName)) {
                try {
                    Remove-Item -LiteralPath $_.FullName -Force
                    Write-Host "Deleted empty folder: $($_.FullName)"
                    $log += "DELETED: $($_.FullName)"
                } catch {
                    Write-Host "FAILED TO DELETE: $($_.FullName) ($($_.Exception.Message))"
                    $log += "FAILED TO DELETE: $($_.FullName) ($($_.Exception.Message))"
                }
            }
        }
    }

Write-Host "`n--- Move Summary ---"
$log | ForEach-Object { Write-Host $_ }
Write-Host "`nAll biomes processed!"