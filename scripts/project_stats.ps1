# KITurbo Project Statistics
# Sophisticated analysis of the project structure and development metrics

param(
    [switch]$Detailed,
    [switch]$Export,
    [switch]$Json,                # Emit JSON summary (stdout)
    [switch]$IncludeBuild,        # Include build/ & CMake artifacts
    [int]$GitDays = 30,           # Days of git history to summarize (if repo)
    [string]$OutputPath = "project-stats-report.txt",
    [string]$Root
)

# Resolve project root (supports invoking from build/ or any subdir)
if (-not $Root -or -not (Test-Path -LiteralPath $Root)) {
    $candidate = (Resolve-Path -LiteralPath $PSScriptRoot).Path
    # script lives in <root>/scripts so parent is likely root
    $parent = Split-Path -Path $candidate -Parent
    $tryPaths = @($parent, (Split-Path -Path $parent -Parent))
    $found = $null
    foreach ($p in $tryPaths) {
        if ((Test-Path -LiteralPath (Join-Path $p '.git')) -or (Test-Path -LiteralPath (Join-Path $p 'src'))) { $found = $p; break }
    }
    if (-not $found) { $found = (Get-Location).Path }
    $Root = $found
}
$Global:ProjectRoot = (Resolve-Path -LiteralPath $Root).Path
if (-not $ProjectRoot.EndsWith([System.IO.Path]::DirectorySeparatorChar)) { $Global:ProjectRoot += [System.IO.Path]::DirectorySeparatorChar }

function Write-ColorText {
    param([string]$Text, [string]$Color = "White")
    $colors = @{
        "Header" = "Cyan"
        "Success" = "Green"
        "Warning" = "Yellow"
        "Error" = "Red"
        "Info" = "Blue"
        "Highlight" = "Magenta"
    }
    $colorValue = if ($colors.ContainsKey($Color)) { $colors[$Color] } else { $Color }
    Write-Host $Text -ForegroundColor $colorValue
}

function Format-Number {
    param([int]$Number)
    return $Number.ToString("N0")
}

function Format-Bytes {
    param([long]$Bytes)
    if ($Bytes -gt 1MB) { return "{0:N1} MB" -f ($Bytes / 1MB) }
    elseif ($Bytes -gt 1KB) { return "{0:N1} KB" -f ($Bytes / 1KB) }
    else { return "$Bytes bytes" }
}

function Get-TrackedFiles {
    param([string]$RootPath)
    Push-Location $RootPath
    try {
        $hasGit = Get-Command git -ErrorAction SilentlyContinue
        if ($hasGit -and (Test-Path -LiteralPath (Join-Path $RootPath '.git'))) {
            try {
                $raw = & git -C $RootPath ls-files --cached --others --exclude-standard -z 2>$null
                $paths = ($raw -split "`0") | Where-Object { $_ -and ($_ -ne '') }
                return $paths | ForEach-Object { Join-Path $RootPath $_ }
            } catch { }
        }
        $all = Get-ChildItem -Path $RootPath -Recurse -File | ForEach-Object { $_.FullName }
        $ignoreFile = Join-Path $RootPath '.gitignore'
        if (Test-Path -LiteralPath $ignoreFile) {
            $patterns = Get-Content -LiteralPath $ignoreFile | Where-Object { $_ -and -not ($_.Trim().StartsWith('#')) } | ForEach-Object { $_.Trim() }
            $regexes = @()
            foreach ($p in $patterns) {
                $pp = $p
                if ($pp.EndsWith('/')) { $pp = "$pp*" }
                $rx = [System.Text.RegularExpressions.Regex]::Escape($pp)
                $rx = $rx.Replace('\\*\\*', '.*')
                $rx = $rx.Replace('\\*', '[^/\\]*')
                $rx = $rx.Replace('\\?', '.')
                if ($pp.StartsWith('/')) { $rx = '^' + $rx.TrimStart('/') } else { $rx = '.*' + $rx }
                $regexes += New-Object System.Text.RegularExpressions.Regex($rx, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            }
            return $all | Where-Object { $path = $_; -not ($regexes | Where-Object { $_.IsMatch($path.Substring($RootPath.Length)) } | Select-Object -First 1) }
        }
        return $all
    } finally { Pop-Location }
}

function Get-FileStats {
    param(
        [string]$FilePath,
        [string]$Language
    )
    try {
        $content = Get-Content -LiteralPath $FilePath -ErrorAction Stop
        $lines = @($content).Count
        $codeLines = 0; $commentLines = 0; $blankLines = 0; $inBlockComment = $false
        foreach ($line in $content) {
            $trimmed = ($line -as [string]).Trim()
            if ([string]::IsNullOrWhiteSpace($trimmed)) { $blankLines++; continue }
            switch ($Language) {
                'python' { if ($trimmed.StartsWith('#')) { $commentLines++ } else { $codeLines++ } }
                'powershell' { if ($trimmed.StartsWith('#')) { $commentLines++ } else { $codeLines++ } }
                'yaml' { if ($trimmed.StartsWith('#')) { $commentLines++ } else { $codeLines++ } }
                'ini' { if ($trimmed.StartsWith(';') -or $trimmed.StartsWith('#')) { $commentLines++ } else { $codeLines++ } }
                'toml' { if ($trimmed.StartsWith('#')) { $commentLines++ } else { $codeLines++ } }
                'rst' { if ($trimmed.StartsWith('..')) { $commentLines++ } else { $codeLines++ } }
                'markdown' {
                    if ($inBlockComment) { $commentLines++; if ($trimmed -match '-->') { $inBlockComment = $false } }
                    elseif ($trimmed -match '<!--') { $commentLines++; if (-not ($trimmed -match '-->')) { $inBlockComment = $true } }
                    else { $codeLines++ }
                }
                'c_cxx' {
                    if ($trimmed -match '^//') { $commentLines++ }
                    elseif ($inBlockComment) { $commentLines++; if ($trimmed -match '\*/') { $inBlockComment = $false } }
                    elseif ($trimmed -match '/\*') { $commentLines++; if (-not ($trimmed -match '\*/')) { $inBlockComment = $true } }
                    else { $codeLines++ }
                }
                'js_ts' {
                    if ($trimmed -match '^//') { $commentLines++ }
                    elseif ($inBlockComment) { $commentLines++; if ($trimmed -match '\*/') { $inBlockComment = $false } }
                    elseif ($trimmed -match '/\*') { $commentLines++; if (-not ($trimmed -match '\*/')) { $inBlockComment = $true } }
                    else { $codeLines++ }
                }
                'css' {
                    if ($inBlockComment) { $commentLines++; if ($trimmed -match '\*/') { $inBlockComment = $false } }
                    elseif ($trimmed -match '/\*') { $commentLines++; if (-not ($trimmed -match '\*/')) { $inBlockComment = $true } }
                    else { $codeLines++ }
                }
                default { $codeLines++ }
            }
        }
        return @{ TotalLines = $lines; CodeLines = $codeLines; CommentLines = $commentLines; BlankLines = $blankLines; Size = (Get-Item -LiteralPath $FilePath).Length }
    } catch { return @{ TotalLines = 0; CodeLines = 0; CommentLines = 0; BlankLines = 0; Size = 0 } }
}
                    

function Get-ComplexityMetrics {
    param(
        [string]$FilePath,
        [string]$Language
    )
    try {
        $content = Get-Content -LiteralPath $FilePath -Raw -ErrorAction Stop
        switch ($Language) {
            'python' {
                $classes = ([regex]::Matches($content, "class\s+\w+")).Count
                $functions = ([regex]::Matches($content, "def\s+\w+")).Count
                $imports = ([regex]::Matches($content, "\bimport\b|\bfrom\s+\w+\s+import\b")).Count
                $conditionals = ([regex]::Matches($content, "\b(if|elif|else|for|while|try|except|finally|with)\b")).Count
            }
            'powershell' {
                $classes = ([regex]::Matches($content, "\bclass\s+\w+")).Count
                $functions = ([regex]::Matches($content, "\bfunction\s+\w+")).Count
                $imports = ([regex]::Matches($content, "\bImport-Module\b|using\s+module\b")).Count
                $conditionals = ([regex]::Matches($content, "\b(if|elseif|else|for|foreach|while|try|catch|finally|switch)\b")).Count
            }
            'c_cxx' {
                # Approximate counts for C source / headers
                $classes = ([regex]::Matches($content, "struct\s+\w+\s*\\{" )).Count + ([regex]::Matches($content, "enum\s+\w*\s*\\{" )).Count
                # Improved function detection: allow multi-line signatures and qualifiers, brace may be on next line
                # Simpler robust function definition pattern (brace same or next line)
                $functionPattern = '(?m)^[ \t]*(?:static\s+)?(?:inline\s+)?(?:extern\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*[ \t\*]+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*(?:\r?\n)?\s*\{'
                $functions = ([regex]::Matches($content, $functionPattern)).Count
                $imports = ([regex]::Matches($content, "(?m)^\s*#\s*include\b" )).Count
                $conditionals = ([regex]::Matches($content, "\b(if|else if|else|for|while|switch|case|default|goto)\b" )).Count
            }
            'js_ts' {
                $classes = ([regex]::Matches($content, "\bclass\s+\w+")).Count
                $functions = ([regex]::Matches($content, "\bfunction\s+\w+|=>")).Count
                $imports = ([regex]::Matches($content, "\bimport\s+|require\(")).Count
                $conditionals = ([regex]::Matches($content, "\b(if|else if|else|for|while|try|catch|finally|switch)\b")).Count
            }
            default {
                $classes = 0; $functions = 0; $imports = 0; $conditionals = 0
            }
        }
        return @{ Classes = $classes; Functions = $functions; Imports = $imports; Conditionals = $conditionals }
    }
    catch { return @{ Classes = 0; Functions = 0; Imports = 0; Conditionals = 0 } }
}

function Get-LanguageForFile {
    param([string]$Path)
    $ext = [System.IO.Path]::GetExtension($Path)
    if (-not $ext) { $ext = '' }
    $ext = $ext.ToLowerInvariant()
    switch ($ext) {
    '.c' { return 'c_cxx' }
    '.h' { return 'c_cxx' }
    '.hpp' { return 'c_cxx' }
    '.hh' { return 'c_cxx' }
    '.cxx' { return 'c_cxx' }
    '.cc' { return 'c_cxx' }
    '.py' { return 'python' }
        {$_ -in '.ps1','.psm1','.psd1'} { return 'powershell' }
        {$_ -in '.yml','.yaml'} { return 'yaml' }
        '.ini' { return 'ini' }
        '.toml' { return 'toml' }
        '.rst' { return 'rst' }
        '.md' { return 'markdown' }
        {$_ -in '.js','.jsx','.ts','.tsx'} { return 'js_ts' }
        {$_ -in '.css','.scss'} { return 'css' }
        {$_ -in '.json','.xml','.svg'} { return 'other_text' }
    default { return 'other_text' }
    }
}

function Get-TrackedFiles {
    # Prefer git to honor .gitignore precisely; fall back to manual scan
    $hasGit = Get-Command git -ErrorAction SilentlyContinue
    if ($hasGit -and (Test-Path -LiteralPath '.git')) {
        try {
            $raw = & git ls-files --cached --others --exclude-standard -z 2>$null
            $paths = ($raw -split "`0") | Where-Object { $_ -and ($_ -ne '') }
            return $paths
        } catch {
            # fall through
        }
    }
    # Fallback: scan all files and apply a minimal .gitignore filter (best-effort)
    $all = Get-ChildItem -Recurse -File | ForEach-Object { $_.FullName }
    $ignoreFile = '.gitignore'
    if (Test-Path -LiteralPath $ignoreFile) {
        $patterns = Get-Content -LiteralPath $ignoreFile | Where-Object { $_ -and -not ($_.Trim().StartsWith('#')) } | ForEach-Object { $_.Trim() }
        $regexes = @()
        foreach ($p in $patterns) {
            $pp = $p
            if ($pp.EndsWith('/')) { $pp = "$pp*" }
            $rx = [System.Text.RegularExpressions.Regex]::Escape($pp)
            $rx = $rx.Replace('\*\*', '.*')
            $rx = $rx.Replace('\*', '[^/\\]*')
            $rx = $rx.Replace('\?', '.')
            if ($pp.StartsWith('/')) { $rx = '^' + $rx.TrimStart('/') } else { $rx = '.*' + $rx }
            $regexes += New-Object System.Text.RegularExpressions.Regex($rx, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        }
        return $all | Where-Object { $path = $_; -not ($regexes | Where-Object { $_.IsMatch($path) } | Select-Object -First 1) }
    }
    return $all
}

# Main analysis
Write-ColorText "============================================================" "Header"
Write-ColorText "                     PROJECT ANALYSIS" "Header"
Write-ColorText "============================================================" "Header"
Write-Host ""

# Initialize counters
$totalFiles = 0
$totalLines = 0
$totalCodeLines = 0
$totalCommentLines = 0
$totalBlankLines = 0
$totalSize = 0
$totalClasses = 0
$totalFunctions = 0
$totalImports = 0
$totalConditionals = 0
$_sumDepth = 0
$docsLines = 0
$langCounts = @{}
$engineTokenHits = 0
$headerLines = 0
$sourceLines = 0

$fileDetails = @()
$directoryStats = @{}

# Build candidate file list honoring .gitignore (via git if available)
$tracked = Get-TrackedFiles -RootPath $ProjectRoot
# If git returned very few files (e.g., sparse checkout), fall back to manual scan merge
if ($tracked.Count -lt 50) {
    $manual = Get-ChildItem -Path $ProjectRoot -Recurse -File | ForEach-Object { $_.FullName }
    $tracked = @([System.Linq.Enumerable]::Distinct([string[]]($tracked + $manual)))
}
if (-not $tracked) { $tracked = @() }

# Collect candidate source roots
$candidateExts = @('.c','.h','.cpp','.cxx','.cc','.hpp','.hh','.cmake','.md','.json','.yml','.yaml','.ini','.toml','.ps1','.py')
$codeFiles = $tracked | Where-Object { $candidateExts -contains ([System.IO.Path]::GetExtension($_).ToLower()) } | ForEach-Object { $_ }
# Filter out build / CMake artifacts unless explicitly included
if (-not $IncludeBuild) {
    $codeFiles = $codeFiles | Where-Object { $_ -notmatch "[\\/]build[\\/]" -and $_ -notmatch "CMakeFiles" -and $_ -notmatch "CompilerIdC" -and $_ -notmatch "CMakeConfigureLog" }
}
# If extremely low count, broaden search under src/ and tests/ explicitly
if ($codeFiles.Count -lt 50) {
    $extraRoots = @('src','tests','include') | ForEach-Object { Join-Path $ProjectRoot $_ } | Where-Object { Test-Path -LiteralPath $_ }
    foreach ($root in $extraRoots) {
        $more = Get-ChildItem -Path $root -Recurse -File | Where-Object { $candidateExts -contains ([System.IO.Path]::GetExtension($_.FullName).ToLower()) } | ForEach-Object { $_.FullName }
        $codeFiles += $more
    }
    $codeFiles = $codeFiles | Sort-Object -Unique
}

Write-ColorText "Project Overview:" "Success"
Write-Host "  Files analyzed (non-ignored): $($codeFiles.Count)"
Write-Host ""
if ($codeFiles.Count -eq 0) {
    Write-ColorText "No analyzable files found (check .gitignore or repository state)." "Error"
    exit 1
}

foreach ($full in $codeFiles) {
    $language = Get-LanguageForFile -Path $full
    $stats = Get-FileStats -FilePath $full -Language $language
    $complexity = Get-ComplexityMetrics -FilePath $full -Language $language

    $totalFiles++
    $totalLines += $stats.TotalLines
    $totalCodeLines += $stats.CodeLines
    $totalCommentLines += $stats.CommentLines
    $totalBlankLines += $stats.BlankLines
    $totalSize += $stats.Size
    $totalClasses += $complexity.Classes
    $totalFunctions += $complexity.Functions
    $totalImports += $complexity.Imports
    $totalConditionals += $complexity.Conditionals

    # Directory statistics
    $relativePath = $full.Substring($ProjectRoot.Length).TrimStart(@("\","/"))
    if (-not $relativePath) { $relativePath = [System.IO.Path]::GetFileName($full) }
    $directory = if ($relativePath.Contains("\")) {
        $relativePath.Substring(0, $relativePath.LastIndexOf("\"))
    } else {
        "root"
    }

    if (-not $directoryStats.ContainsKey($directory)) {
        $directoryStats[$directory] = @{ Files = 0; Lines = 0; Size = 0 }
    }
    $directoryStats[$directory].Files++
    $directoryStats[$directory].Lines += $stats.TotalLines
    $directoryStats[$directory].Size += $stats.Size

    # Additional per-file derived metrics (C only currently)
    $macros = 0; $todo = 0; $includesLocal = 0; $publicApiDecls = 0
    if ($language -eq 'c_cxx') {
        try {
            $rawFile = Get-Content -LiteralPath $full -Raw -ErrorAction SilentlyContinue
            if ($rawFile) {
                $macros = ([regex]::Matches($rawFile, '(?m)^\s*#\s*define\b')).Count
                $todo = ([regex]::Matches($rawFile, '(?i)TODO|FIXME')).Count
                $includesLocal = ([regex]::Matches($rawFile, '(?m)^\s*#\s*include\b')).Count
                if ($full.ToLower().EndsWith('.h')) {
                    $publicPattern = '(?ms)^[\t ]*(?:extern\s+)?(?:inline\s+)?(?!static)(?:[A-Za-z_][A-Za-z0-9_\*\s]+)?[A-Za-z_][A-Za-z0-9_]*\s*\([^;{}]*?\)\s*;'
                    $publicApiDecls = ([regex]::Matches($rawFile, $publicPattern)).Count
                }
            }
        } catch {}
    }

    $fileDetails += @{
        Path = $relativePath
        Stats = $stats
        Complexity = $complexity
        Language = $language
        Macros = $macros
        Todos = $todo
        Includes = $includesLocal
        PublicAPI = $publicApiDecls
    }

    # Aggregate extras
    $_sumDepth += (($relativePath -split "[\\/]").Count - 1)
    if ($language -in @('markdown','rst')) { $docsLines += $stats.CodeLines }
    if ($language -eq 'c_cxx') {
        if ($full.ToLower().EndsWith('.h') -or $full.ToLower().EndsWith('.hpp') -or $full.ToLower().EndsWith('.hh')) { $headerLines += $stats.TotalLines } else { $sourceLines += $stats.TotalLines }
    }
    if (-not $langCounts.ContainsKey($language)) { $langCounts[$language] = 0 }
    $langCounts[$language] += 1
    if ($language -eq 'c_cxx') {
        try {
            $raw = Get-Content -LiteralPath $full -Raw -ErrorAction SilentlyContinue
            if ($raw) {
                # Count engine/platform related tokens (SDL, rogue_ prefixes, TODO markers)
                $engineTokenHits += ([regex]::Matches($raw, 'SDL_|rogue_|RoguePlayer|RogueEnemy|TODO')).Count
            }
        } catch { }
    }
}

# Calculate percentages
$commentPercentage = if ($totalLines -gt 0) { [math]::Round(($totalCommentLines / $totalLines) * 100, 1) } else { 0 }
$codePercentage = if ($totalLines -gt 0) { [math]::Round(($totalCodeLines / $totalLines) * 100, 1) } else { 0 }
$blankPercentage = if ($totalLines -gt 0) { [math]::Round(($totalBlankLines / $totalLines) * 100, 1) } else { 0 }

# File Statistics
Write-ColorText "File Statistics:" "Info"
Write-Host "  Total Files: $(Format-Number $totalFiles)"
Write-Host "  Total Lines: $(Format-Number $totalLines)"
Write-Host "  Code Lines: $(Format-Number $totalCodeLines) ($codePercentage%)"
Write-Host "  Comment Lines: $(Format-Number $totalCommentLines) ($commentPercentage%)"
Write-Host "  Blank Lines: $(Format-Number $totalBlankLines) ($blankPercentage%)"
Write-Host "  Total Size: $(Format-Bytes $totalSize)"
Write-Host ""

# Code Structure Analysis
Write-ColorText "Code Structure Analysis:" "Info"
Write-Host "  Classes: $(Format-Number $totalClasses)"
Write-Host "  Functions: $(Format-Number $totalFunctions)"
Write-Host "  Import Statements: $(Format-Number $totalImports)"
Write-Host "  Control Structures: $(Format-Number $totalConditionals)"
Write-Host ""

# Complexity-derived Productivity (Lines/Hour)
$avgDepth = if ($totalFiles -gt 0) { [double]$_sumDepth / [double]$totalFiles } else { 0.0 }
$conditionalDensity = if ($totalCodeLines -gt 0) { [double]$totalConditionals / [double]$totalCodeLines } else { 0.0 }
$avgFunctionsPerFile = if ($totalFiles -gt 0) { [double]$totalFunctions / [double]$totalFiles } else { 0.0 }
$languageDiversity = ($langCounts.Keys | Measure-Object).Count
$docsCoverage = if ($totalLines -gt 0) { [double]$docsLines / [double]$totalLines } else { 0.0 }

# Normalize sub-metrics into [0,1]
$nCond = [Math]::Min(1.0, ($conditionalDensity / 0.05))             # 0.05 cond/line -> 1.0
$nFuncs = [Math]::Min(1.0, ($avgFunctionsPerFile / 10.0))
$nDepth = [Math]::Min(1.0, ($avgDepth / 6.0))
$nLang  = [Math]::Min(1.0, ([double]$languageDiversity / 5.0))
$nUI    = [Math]::Min(1.0, ([double]$engineTokenHits / 200.0))
$nDocs  = [Math]::Min(1.0, $docsCoverage)

# Composite complexity index (>=1): higher means harder
$complexityIndex = 1.0 + (0.5 * $nCond) + (0.2 * $nFuncs) + (0.2 * $nDepth) + (0.1 * $nLang) + (0.1 * $nUI)
# Documentation reduces effective complexity slightly (up to 10%)
$complexityIndex = $complexityIndex * (1.0 - (0.1 * $nDocs))
if ($complexityIndex -lt 0.5) { $complexityIndex = 0.5 }

$baseLinesPerHour = 30.0
$effectiveLinesPerHour = $baseLinesPerHour / $complexityIndex
$totalHours = if ($effectiveLinesPerHour -gt 0) { [double]$totalCodeLines / $effectiveLinesPerHour } else { 0.0 }
$days = $totalHours / 8.0
$weeks = $days / 5.0

Write-ColorText "Development Effort Estimation (complexity-aware):" "Highlight"
Write-Host "  Complexity Index: $([math]::Round($complexityIndex, 2))"
Write-Host "  Effective Lines/Hour: $([math]::Round($effectiveLinesPerHour, 1)) (base $baseLinesPerHour)"
Write-Host "  Estimated Hours: $([math]::Round($totalHours, 1))"
Write-Host "  Estimated Days: $([math]::Round($days, 1))"
Write-Host "  Estimated Weeks: $([math]::Round($weeks, 1))"
Write-Host ""

# Directory Breakdown
Write-ColorText "Directory Breakdown:" "Info"
$sortedDirs = $directoryStats.GetEnumerator() | Sort-Object { $_.Value.Lines } -Descending
foreach ($dir in $sortedDirs) {
    $dirName = if ($dir.Key -eq "root") { "Root Directory" } else { $dir.Key }
    Write-Host "  $dirName"
    Write-Host "    Files: $($dir.Value.Files) | Lines: $(Format-Number $dir.Value.Lines) | Size: $(Format-Bytes $dir.Value.Size)"
}
Write-Host ""

# Quality Indicators
Write-ColorText "Project Quality Indicators:" "Success"
$qualityScore = 0
$_macroTotal = ($fileDetails | Measure-Object -Property Macros -Sum).Sum
$_todoTotal = ($fileDetails | Measure-Object -Property Todos -Sum).Sum
$_publicApiTotal = ($fileDetails | Measure-Object -Property PublicAPI -Sum).Sum
$_cFiles = ($fileDetails | Where-Object { $_.Language -eq 'c_cxx' })
$_avgIncludesC = if ($_cFiles.Count -gt 0) { ($_cFiles | Measure-Object -Property Includes -Average).Average } else { 0 }

# Documentation score
$docScore = if ($commentPercentage -ge 20) { 25 } elseif ($commentPercentage -ge 15) { 20 } elseif ($commentPercentage -ge 10) { 15 } else { 5 }
$qualityScore += $docScore

# Code organization score
$orgScore = if ($directoryStats.Count -ge 5) { 20 } elseif ($directoryStats.Count -ge 3) { 15 } else { 10 }
$qualityScore += $orgScore

# Modularity score (consider public API exposure for C projects)
$apiWeight = if ($_publicApiTotal -gt 0) { [Math]::Min(1.0, ($_publicApiTotal / [double]([Math]::Max(1,$totalFiles))) ) } else { 0 }
$modBase = if ($avgFunctionsPerFile -ge 5) { 15 } elseif ($avgFunctionsPerFile -ge 3) { 12 } else { 8 }
$modApiBonus = [Math]::Round(5 * $apiWeight)
$modScore = $modBase + $modApiBonus
$qualityScore += $modScore

# File size distribution
$avgLinesPerFile = if ($totalFiles -gt 0) { $totalLines / $totalFiles } else { 0 }
$sizeScore = if ($avgLinesPerFile -le 200) { 20 } elseif ($avgLinesPerFile -le 400) { 15 } else { 10 }
$qualityScore += $sizeScore

# Import complexity
$avgImportsPerFile = if ($totalFiles -gt 0) { $totalImports / $totalFiles } else { 0 }
$impScore = if ($avgImportsPerFile -le 5) { 15 } elseif ($avgImportsPerFile -le 10) { 10 } else { 5 }
$qualityScore += $impScore

Write-Host "  Documentation Coverage: $commentPercentage% (Score: $docScore/25)"
Write-Host "  Code Organization: $($directoryStats.Count) directories (Score: $orgScore/20)"
Write-Host "  Modularity: $([math]::Round($avgFunctionsPerFile, 1)) functions/file (Score: $modScore/20)"
Write-Host "  File Size Distribution: $([math]::Round($avgLinesPerFile, 1)) lines/file (Score: $sizeScore/20)"
Write-Host "  Import Complexity: $([math]::Round($avgImportsPerFile, 1)) imports/file (Score: $impScore/15)"
Write-Host "  Language Diversity: $languageDiversity languages"
if ($sourceLines -gt 0 -or $headerLines -gt 0) {
    $ratio = if ($headerLines -gt 0) { [math]::Round($sourceLines / [double]$headerLines,2) } else { 0 }
    Write-Host "  C/C++ Source/Header Lines: $sourceLines / $headerLines (S/H Ratio: $ratio)"
}
Write-Host "  Average Depth: $([math]::Round($avgDepth,2))"
Write-Host "  Docs Coverage (by lines): $([math]::Round($docsCoverage*100,1))%"
Write-Host "  Macros (total): $_macroTotal"
Write-Host "  Public API Prototypes: $_publicApiTotal"
Write-Host "  Average #includes per C/C++ file: $([math]::Round($_avgIncludesC,2))"
Write-Host "  TODO/FIXME Count: $_todoTotal"
Write-Host ""
Write-ColorText "Overall Quality Score: $qualityScore/100" "Highlight"

$qualityLevel = if ($qualityScore -ge 80) { "Excellent" }
                elseif ($qualityScore -ge 60) { "Good" }
                elseif ($qualityScore -ge 40) { "Fair" }
                else { "Needs Improvement" }
Write-ColorText "Quality Assessment: $qualityLevel" $(if ($qualityScore -ge 60) { "Success" } else { "Warning" })
Write-Host ""

if ($Detailed) {
    Write-ColorText "Top 10 Largest Files:" "Info"
    $sortedFiles = $fileDetails | Sort-Object { $_.Stats.TotalLines } -Descending | Select-Object -First 10
    foreach ($file in $sortedFiles) {
        Write-Host "  $($file.Path)"
        Write-Host "    Lines: $($file.Stats.TotalLines) | Classes: $($file.Complexity.Classes) | Functions: $($file.Complexity.Functions)"
    }
    Write-Host ""

    Write-ColorText "Most Complex Files:" "Info"
    $complexFiles = $fileDetails | Sort-Object { $_.Complexity.Conditionals } -Descending | Select-Object -First 10
    foreach ($file in $complexFiles) {
        if ($file.Complexity.Conditionals -gt 0) {
            Write-Host "  $($file.Path)"
            Write-Host "    Control Structures: $($file.Complexity.Conditionals) | Functions: $($file.Complexity.Functions)"
        }
    }
    Write-Host ""

    Write-ColorText "Top Files By TODO/FIXME:" "Info"
    $todoTop = $fileDetails | Where-Object { $_.Todos -gt 0 } | Sort-Object { $_.Todos } -Descending | Select-Object -First 5
    foreach ($file in $todoTop) {
        Write-Host "  $($file.Path) -> TODOs: $($file.Todos)"
    }
    if (-not $todoTop) { Write-Host "  (none)" }
    Write-Host ""

    Write-ColorText "Top Header Public APIs:" "Info"
    $apiTop = $fileDetails | Where-Object { $_.PublicAPI -gt 0 } | Sort-Object { $_.PublicAPI } -Descending | Select-Object -First 5
    foreach ($file in $apiTop) { Write-Host "  $($file.Path) -> Public prototypes: $($file.PublicAPI)" }
    if (-not $apiTop) { Write-Host "  (none)" }
    Write-Host ""
}

# Optional git activity summary
$gitSummary = $null
if (Test-Path -LiteralPath (Join-Path $ProjectRoot '.git')) {
    try {
        $since = (Get-Date).AddDays(-1 * $GitDays)
        $rawLog = git -C $ProjectRoot log --since="$($since.ToString('yyyy-MM-dd'))" --name-only --pretty=format:'%H' 2>$null
        if ($rawLog) {
            $linesLog = $rawLog -split "`n" | Where-Object { $_ -ne '' }
            $commitCount = ($linesLog | Where-Object { $_ -match '^[0-9a-f]{7,40}$' }).Count
            $changedFiles = ($linesLog | Where-Object { $_ -notmatch '^[0-9a-f]{7,40}$' } | Sort-Object -Unique)
            $gitSummary = @{ Commits = $commitCount; ChangedFiles = $changedFiles.Count; UniqueFiles = $changedFiles }
            Write-ColorText "Recent Git Activity (last $GitDays days):" "Info"
            Write-Host "  Commits: $commitCount | Changed Files: $($changedFiles.Count)"
            Write-Host ""
        }
    } catch {}
}

# Export option
if ($Export) {
    $reportContent = @"
KITURBO PROJECT ANALYSIS REPORT
Generated: $(Get-Date)

FILE STATISTICS:
- Total Files: $totalFiles
- Total Lines: $totalLines
- Code Lines: $totalCodeLines ($codePercentage%)
- Comment Lines: $totalCommentLines ($commentPercentage%)
- Blank Lines: $totalBlankLines ($blankPercentage%)
- Total Size: $(Format-Bytes $totalSize)

CODE STRUCTURE:
- Classes: $totalClasses
- Functions: $totalFunctions
- Import Statements: $totalImports
- Control Structures: $totalConditionals

COMPLEXITY & PRODUCTIVITY:
- Complexity Index: $([math]::Round($complexityIndex, 2))
- Effective Lines/Hour: $([math]::Round($effectiveLinesPerHour, 1))
- Estimated Hours: $([math]::Round($totalHours, 1))
- Estimated Days: $([math]::Round($days, 1))
- Estimated Weeks: $([math]::Round($weeks, 1))

QUALITY SCORE: $qualityScore/100 ($qualityLevel)
"@

    # Always save exported .txt reports into project_stats folder
    $fileName = Split-Path -Path $OutputPath -Leaf
    if (-not $fileName -or -not ($fileName -like '*.txt')) { $fileName = 'project-stats-report.txt' }
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($fileName)
    if (-not $baseName) { $baseName = 'project-stats-report' }
    $ext = '.txt'
    $ts = Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'
    $fileName = "$baseName`_$ts$ext"
    $destDir = Join-Path (Get-Location) 'project_stats'
    if (-not (Test-Path -LiteralPath $destDir)) { New-Item -ItemType Directory -Path $destDir | Out-Null }
    $destPath = Join-Path $destDir $fileName

    $reportContent | Out-File -FilePath $destPath -Encoding UTF8
    Write-ColorText "Report exported to: $destPath" "Success"
}

if ($Json) {
    $jsonObj = [ordered]@{
        totalFiles = $totalFiles
        totalLines = $totalLines
        codeLines = $totalCodeLines
        commentLines = $totalCommentLines
        blankLines = $totalBlankLines
        totalSizeBytes = $totalSize
        classes = $totalClasses
        functions = $totalFunctions
        imports = $totalImports
        controlStructures = $totalConditionals
        complexityIndex = [math]::Round($complexityIndex,3)
        effectiveLinesPerHour = [math]::Round($effectiveLinesPerHour,2)
        estimatedHours = [math]::Round($totalHours,1)
        qualityScore = $qualityScore
        qualityLevel = $qualityLevel
        macros = $_macroTotal
        todos = $_todoTotal
        publicApiPrototypes = $_publicApiTotal
        avgIncludesPerCFile = [math]::Round($_avgIncludesC,2)
        languageDiversity = $languageDiversity
        sourceLines = $sourceLines
        headerLines = $headerLines
        sourceHeaderRatio = if ($headerLines -gt 0) { [math]::Round($sourceLines / [double]$headerLines,2) } else { 0 }
        git = $gitSummary
        topLargest = ($fileDetails | Sort-Object { $_.Stats.TotalLines } -Descending | Select-Object -First 5 | ForEach-Object { @{ path=$_.Path; lines=$_.Stats.TotalLines } })
        topComplex = ($fileDetails | Sort-Object { $_.Complexity.Conditionals } -Descending | Select-Object -First 5 | ForEach-Object { @{ path=$_.Path; control=$_.Complexity.Conditionals } })
        topTodo = ($fileDetails | Where-Object { $_.Todos -gt 0 } | Sort-Object { $_.Todos } -Descending | Select-Object -First 5 | ForEach-Object { @{ path=$_.Path; todos=$_.Todos } })
    }
    $jsonObj | ConvertTo-Json -Depth 6
}

Write-Host ""
Write-ColorText "Analysis Complete!" "Success"
Write-ColorText "============================================================" "Header"
