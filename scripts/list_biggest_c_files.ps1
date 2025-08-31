# PowerShell script to list the 10 biggest .c files by lines of code in the src directory

param(
    [string]$SrcPath = "src"  # Default to 'src' relative to script location
)

# Get the full path to the src directory
$srcDir = Join-Path (Get-Location) $SrcPath

if (-not (Test-Path $srcDir)) {
    Write-Host "Error: Source directory '$srcDir' not found." -ForegroundColor Red
    exit 1
}

# Find all .c files recursively
$cFiles = Get-ChildItem -Path $srcDir -Recurse -Filter "*.c" -File

# Calculate lines of code for each file
$fileStats = $cFiles | ForEach-Object {
    try {
        $lineCount = (Get-Content $_.FullName -ErrorAction Stop).Count
        [PSCustomObject]@{
            FileName = $_.FullName
            LineCount = $lineCount
        }
    } catch {
        Write-Warning "Could not read file: $($_.FullName)"
        $null
    }
} | Where-Object { $_ -ne $null }

# Sort by line count descending and take top 10
$top10 = $fileStats | Sort-Object -Property LineCount -Descending | Select-Object -First 10

# Display the results
Write-Host "Top 10 biggest .c files by lines of code in '$srcDir':"
Write-Host ("-" * 80)
$top10 | ForEach-Object {
    Write-Host ("{0,-10} {1}" -f $_.LineCount, $_.FileName)
}
