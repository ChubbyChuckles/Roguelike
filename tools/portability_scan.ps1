# Scans repository for potentially non-portable constructs (Windows path separators, CRLF endings, etc.)
param(
    [string]$Root='.',
    [switch]$FailOnFind,
    [string[]]$IncludeGlobs = @('src/**/*.c','src/**/*.h','assets/**/*.cfg','assets/**/*.json','CMakeLists.txt','cmake/**/*.cmake'),
    [string[]]$ExcludeGlobs = @('third_party/**','build/**','.git/**'),
    [switch]$ShowAll
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Convert-GlobToRegex { param([string]$glob)
    $norm = $glob -replace '\\','/'
    $escaped = [Regex]::Escape($norm)
    $escaped = $escaped -replace '\*\*','.*'
    $escaped = $escaped -replace '\*','[^/]*'
    '^' + $escaped + '$'
}

$rootFull = [IO.Path]::GetFullPath($Root)
if (-not (Test-Path $rootFull)) { throw "Root not found: $Root" }
$includeRegexes = $IncludeGlobs | ForEach-Object { Convert-GlobToRegex $_ }
$excludeRegexes = $ExcludeGlobs | ForEach-Object { Convert-GlobToRegex $_ }

function Match-Include($rel) { foreach($r in $includeRegexes){ if($rel -match $r){ return $true } } return $false }
function Match-Exclude($rel) { foreach($r in $excludeRegexes){ if($rel -match $r){ return $true } } return $false }

$files = Get-ChildItem -Path $rootFull -Recurse -File | Where-Object {
    $rel = $_.FullName.Substring($rootFull.Length).TrimStart(@([char]'\',[char]'/')) -replace '\\','/'

    (Match-Include $rel) -and -not (Match-Exclude $rel)
}

$issues = @()
foreach($f in $files){
    $content = Get-Content -LiteralPath $f.FullName -Raw -Encoding UTF8
    $hasBackslash = $content -match '\\'
    $hasCRLF = $content -match "\r\n"
    if($hasBackslash -or $hasCRLF){
        $lines = $content -split "\n"  # keep potential \r for detection
        for($i=0;$i -lt $lines.Count;$i++){
            $line = $lines[$i]
            $flag = ''
            if($line -match '\\'){ $flag += 'B' } # Backslash
            if($line -match '\r$'){ $flag += 'C' } # CR before newline
            if($flag -ne ''){
                $issues += [pscustomobject]@{ File=$f.FullName; Line=$i+1; Flags=$flag; Text=$line.TrimEnd() }
                if(-not $ShowAll -and $issues.Count -gt 500){ break }
            }
        }
    }
}

if($issues.Count -gt 0){
    Write-Host "Portability scan: found $($issues.Count) flagged lines (B=backslash path, C=CR line ending)." -ForegroundColor Yellow
    $issues | Select-Object -First 80 | ForEach-Object { Write-Host ("{0}:{1}:{2}: {3}" -f $_.File,$_.Line,$_.Flags,$_.Text) }
    if(-not $ShowAll -and $issues.Count -gt 80){ Write-Host "(Truncated; use -ShowAll to list every line)" -ForegroundColor DarkYellow }
} else {
    Write-Host "Portability scan: no issues detected." -ForegroundColor Green
}

if($FailOnFind -and $issues.Count -gt 0){ exit 3 }

Write-Host "portability_scan.ps1 completed." -ForegroundColor Green
