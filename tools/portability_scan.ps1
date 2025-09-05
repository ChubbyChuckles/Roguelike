# Scans the codebase for platform-specific code patterns to improve portability awareness.
param(
    [string]$Root = '.',
    [string[]]$IncludeGlobs = @('src/**/*.c', 'src/**/*.h'),
    [string[]]$ExcludeGlobs = @('third_party/**', 'build/**', '.git/**')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Convert-GlobToRegex([string]$glob) {
    $escaped = [Regex]::Escape($glob) -replace '\\', '/'
    $escaped = $escaped -replace '/\*\*/', '/.*/'
    $escaped = $escaped -replace '\*', '[^/]*'
    return '^' + $escaped + '$'
}

function Test-PathExcluded([string]$rel, [string[]]$patterns) {
    foreach ($p in $patterns) { if ($rel -match (Convert-GlobToRegex $p)) { return $true } }
    return $false
}

$rootFull = [IO.Path]::GetFullPath($Root)
$files = Get-ChildItem -Path $rootFull -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($rootFull.Length).TrimStart('\\', '/') -replace '\\', '/'
    if (Test-PathExcluded -rel $rel -patterns $ExcludeGlobs) { return }
    foreach ($g in $IncludeGlobs) { if ($rel -match (Convert-GlobToRegex $g)) { $_ } }
}

$patterns = @(
    @{ name = 'windows_fopen_s'; regex = '\bfopen_s\s*\('; severity = 'warn'; message = 'Use portable fopen + error handling instead of fopen_s.' },
    @{ name = 'windows_sleep'; regex = '\bSleep\s*\('; severity = 'warn'; message = 'Use portable sleep helpers (SDL_Delay) instead of Sleep().' },
    @{ name = 'msvc_strfuncs'; regex = '\b(strcpy_s|strcat_s|_stricmp|_snprintf)\b'; severity = 'info'; message = 'MSVC-specific CRT variants detected.' },
    @{ name = 'posix_sleep'; regex = '\b(nanosleep|usleep)\s*\('; severity = 'info'; message = 'POSIX-only sleep used; ensure guarded by platform macros.' },
    @{ name = 'backslash_include'; regex = '^\s*#\s*include\s*\"[^\"]*\\\\[^\"]*\"'; severity = 'warn'; message = 'Backslash in include path; use forward slashes.' }
)

$findings = @()
foreach ($f in $files) {
    $lines = Get-Content -LiteralPath $f.FullName -Encoding UTF8
    for ($i = 0; $i -lt $lines.Count; $i++) {
        foreach ($p in $patterns) {
            if ($lines[$i] -match $p.regex) {
                $findings += [pscustomobject]@{ file = $f.FullName; line = $i + 1; rule = $p.name; severity = $p.severity; message = $p.message; snippet = $lines[$i] }
            }
        }
    }
}

if ($findings.Count -gt 0) {
    $findings | Sort-Object file, line | Format-Table -AutoSize
    Write-Host ("Portability scan: {0} findings." -f $findings.Count) -ForegroundColor Yellow
}
else {
    Write-Host "Portability scan: no issues found." -ForegroundColor Green
}

exit 0
