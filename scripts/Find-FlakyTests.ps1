param(
    [ValidateSet("Debug", "Release", "Both")]
    [string]$BuildConfigs = "Both",
    [string]$BuildDir = "build",
    [int]$NumRuns = 50,
    [switch]$DryRun,
    [switch]$Verbose,
    [switch]$ParallelTests,
    [string]$OutputDir = ".",
    [double]$Threshold = 0.01,
    [string]$RegexFilter,
    [ValidateSet("Console", "JSON", "CSV", "HTML", "TXT", "All")]
    [string]$ReportFormat = "TXT"
)

# Default ParallelTests to true if not provided
if (-not $PSBoundParameters.ContainsKey('ParallelTests')) { $ParallelTests = $true }

function Get-MaxCores {
    try {
        if ($IsWindows) {
            $cores = (Get-CimInstance -ClassName Win32_ComputerSystem).NumberOfLogicalProcessors
            if ($cores -gt 0) { return [int]$cores }
        }
        else {
            $n = & nproc 2>$null
            if ($LASTEXITCODE -eq 0) { return [int]$n }
        }
    }
    catch {}
    return [Environment]::ProcessorCount
}
$Script:MaxCores = Get-MaxCores

function Get-ProjectRoot {
    param([string]$StartPath = $PWD.Path)
    $p = $StartPath
    while ($p) {
        if ((Test-Path (Join-Path $p 'CMakeLists.txt') -PathType Leaf) -or (Test-Path (Join-Path $p '.git') -PathType Container)) { return $p }
        $parent = Split-Path $p -Parent
        if (-not $parent -or $parent -eq $p) { break }
        $p = $parent
    }
    throw "Project root not found."
}

function Invoke-BuildConfig {
    param(
        [string]$ProjectRoot,
        [string]$Config,
        [string]$BuildDir
    )
    $buildPath = Join-Path $ProjectRoot $BuildDir
    $cache = Join-Path $buildPath 'CMakeCache.txt'
    if (-not (Test-Path $cache)) {
        Write-Verbose "Configuring CMake: $Config"
        & cmake -S $ProjectRoot -B $buildPath -DCMAKE_BUILD_TYPE=$Config
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $Config" }
    }
    Write-Verbose "Building: $Config"
    & cmake --build $buildPath --config $Config -j $Script:MaxCores
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $Config" }
}

function Invoke-TestRuns {
    param(
        [string]$Config,
        [string]$BuildDir,
        [int]$NumRuns,
        [string]$ProjectRoot,
        [hashtable]$TestResults,
        [string]$RegexFilter
    )
    $testDir = Join-Path (Join-Path $ProjectRoot $BuildDir) "tests/$Config"
    if (-not (Test-Path $testDir)) { throw "Test directory not found: $testDir" }
    $testExes = Get-ChildItem $testDir -Filter 'test_*.exe' | ForEach-Object { $_.Name }
    if ($RegexFilter) {
        $testExes = $testExes | Where-Object { (($_ -replace '\.exe$', '') -match $RegexFilter) }
    }
    if (-not $testExes) { Write-Warning "No test executables found in $testDir"; return }

    $work = @()
    foreach ($exe in $testExes) {
        $name = $exe -replace '\.exe$', ''
        if (-not $TestResults.ContainsKey($name)) { $TestResults[$name] = @{} }
        $TestResults[$name].Config = $Config
        $TestResults[$name].Runs = @()
        $TestResults[$name].EnvLog = @{ OS = $PSVersionTable.OS; Timestamp = Get-Date }
        1..$NumRuns | ForEach-Object { $work += [pscustomobject]@{ TestExe = $exe; TestName = $name; Run = $_ } }
    }

    $root = Join-Path $ProjectRoot $BuildDir
    $throttle = if ($ParallelTests) { [int]$Script:MaxCores } else { 1 }

    $runOne = {
        param($item)
        $Config = $using:Config
        $testDir = $using:testDir
        $root = $using:root
        $DryRun = $using:DryRun
        $RegexFilter = $using:RegexFilter
        # Support both non-parallel invocation (& $runOne $w) and -Parallel pipeline (uses $_)
        $w = if ($null -ne $item) { $item } else { $_ }
        $name = $w.TestName; $i = [int]$w.Run
        $tmp = Join-Path ([IO.Path]::GetTempPath()) ("flaky_" + [guid]::NewGuid().ToString())
        New-Item -ItemType Directory -Path $tmp -Force | Out-Null
        $start = Get-Date
        $exit = 0; $out = ""
        if ($DryRun) {
            Start-Sleep -Milliseconds (Get-Random -Min 50 -Max 150)
            $out = "Dry run: $name #$i"
        }
        else {
            try {
                $ctest = & ctest --test-dir $root -C $Config -R "^$name$" --output-on-failure 2>&1
                $out = $ctest
                if ($ctest -match 'No tests were found') {
                    $exit = 2
                }
                else {
                    $exit = $LASTEXITCODE
                }
            }
            catch {
                $out = $_ | Out-String
                $exit = 3
            }
        }
        $dur = ((Get-Date) - $start).TotalSeconds
        [pscustomobject]@{ TestName = $name; Run = $i; ExitCode = $exit; Duration = [double]$dur; Output = $out; TempDir = $tmp }
    }

    $results = @()
    if ($work.Count -gt 0) {
        if ($throttle -gt 1) {
            $results = $work | ForEach-Object -Parallel $runOne -ThrottleLimit $throttle
        }
        else {
            foreach ($w in $work) { $results += (& $runOne $w) }
        }
    }

    # Filter out any malformed results and aggregate by TestName
    $results = $results | Where-Object { $_ -and $_.TestName }
    foreach ($g in $results | Group-Object TestName) {
        $n = $g.Name
        if (-not $TestResults.ContainsKey($n)) { $TestResults[$n] = @{} }
        if (-not $TestResults[$n].Runs) { $TestResults[$n].Runs = @() }
        if (-not $TestResults[$n].Config) { $TestResults[$n].Config = $Config }
        foreach ($r in $g.Group | Sort-Object Run) {
            $TestResults[$n].Runs += @{ Run = $r.Run; ExitCode = $r.ExitCode; Duration = $r.Duration; Output = $r.Output; TempDir = $r.TempDir }
            if ($Verbose) { Write-Output ("[{0}] {1} run {2}: exit {3}, {4:N3}s" -f $Config, $n, $r.Run, $r.ExitCode, [double]$r.Duration) }
        }
    }
}

function Measure-TestResults {
    param(
        [hashtable]$TestResults,
        [double]$Threshold
    )
    $flaky = @{}
    foreach ($name in $TestResults.Keys) {
        $runs = $TestResults[$name].Runs
        if (-not $runs -or $runs.Count -eq 0) {
            $TestResults[$name].Metrics = @{ FailureRate = 0.0; FailCount = 0; RunCount = 0; StdDevDuration = 0.0; AvgDuration = 0.0; PValue = 1.0; IsFlaky = $false; Intermittent = $false; ConsistentFlake = $false }
            continue
        }
        # Dedupe by Run index in case of accidental duplicate entries
        $runsUnique = $runs | Sort-Object Run -Descending | Group-Object Run | ForEach-Object { $_.Group[0] }
        $total = ($runsUnique | Measure-Object).Count
        $failCount = ($runsUnique | Where-Object { $_.ExitCode -ne 0 } | Measure-Object).Count
        $rate = if ($total -gt 0) { [double]$failCount / [double]$total } else { 0.0 }
        # Clamp for safety
        if ($rate -lt 0) { $rate = 0.0 }
        if ($rate -gt 1) { $rate = 1.0 }
        $durs = $runsUnique.Duration | Where-Object { $_ -ne $null }
        $avg = if ($durs) { ($durs | Measure-Object -Average).Average } else { 0.0 }
        $var = if ($durs) { ($durs | ForEach-Object { [Math]::Pow(($_ - $avg), 2) } | Measure-Object -Average).Average } else { 0.0 }
        $std = [Math]::Sqrt([double]$var)
        $p = if ($rate -eq 0 -or $rate -eq 1) { 1.0 } else { [Math]::Min(1.0, 2 * [Math]::Min($rate, 1 - $rate) * $total) }
        $isFlaky = ($rate -gt $Threshold) -or ($p -lt 0.05)
        $TestResults[$name].Metrics = @{ FailureRate = $rate; FailCount = $failCount; RunCount = $total; StdDevDuration = $std; AvgDuration = $avg; PValue = $p; IsFlaky = $isFlaky; Intermittent = ($rate -gt 0 -and $rate -lt 1); ConsistentFlake = ($rate -ge 1) }
        if ($isFlaky) { $flaky[$name] = $TestResults[$name] }
    }
    return $flaky
}

function Group-Flakiness {
    param([hashtable]$TestResults)
    foreach ($name in $TestResults.Keys) {
        $tr = $TestResults[$name]
        $m = $tr.Metrics
        $cats = @(); $ev = @(); $recs = @()
        if ($m -and $m.AvgDuration -gt 0 -and $m.StdDevDuration -gt ($m.AvgDuration * 0.1)) { $cats += 'Timing/Race'; $ev += ("High duration variance: avg={0:N3}s, stddev={1:N3}s" -f $m.AvgDuration, $m.StdDevDuration); $recs += 'Use deterministic waits; avoid sleeps' }
        if ($m -and $m.Intermittent) { $cats += 'Intermittent'; $ev += 'Intermittent failures across runs'; $recs += 'Isolate shared state; fix seeds' }
        if ($cats.Count -eq 0) { $cats = @('Uncategorized') }
        if ($ev.Count -eq 0) { $ev = @('No specific signals detected') }
        $tr.Category = ($cats -join ', ')
        $tr.Evidence = ($ev -join '; ')
        $tr.Recommendations = $recs
    }
}

function Write-FlakyReports {
    param(
        [hashtable]$TestResults,
        [string]$OutputDir,
        [string]$Format
    )
    # Ensure output directory exists
    if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null }
    $entries = $TestResults.GetEnumerator() | Where-Object { $_.Value -and $_.Value.Metrics }
    $flaky = $entries | Where-Object { $_.Value.Metrics.IsFlaky }

    if ($Format -in @('Console', 'TXT', 'All')) {
        $txt = @()
        $ci = [System.Globalization.CultureInfo]::InvariantCulture
        if (-not $flaky -or ($flaky | Measure-Object).Count -eq 0) {
            $msg = 'No flaky tests detected.'
            $txt += $msg; Write-Output $msg
        }
        else {
            foreach ($e in $flaky) {
                $v = $e.Value; $name = $e.Key
                $ratePct = if ($null -ne $v.Metrics.FailureRate) { ([double]$v.Metrics.FailureRate).ToString('P2', $ci) } else { 'N/A' }
                $counts = if ($v.Metrics.RunCount -gt 0) { "($($v.Metrics.FailCount)/$($v.Metrics.RunCount))" } else { "(0/0)" }
                $line = "Test: $name [$($v.Config)]: $ratePct flaky $counts ($($v.Category)) - $($v.Evidence)"
                $txt += $line; Write-Output $line
                foreach ($r in ($v.Recommendations | Where-Object { $_ })) { $rec = "  Recommendation: $r"; $txt += $rec; Write-Output $rec }
            }
        }
        if ($Format -eq 'TXT') { $txt | Out-File (Join-Path $OutputDir 'flaky_report.txt') -Encoding UTF8 }
    }

    if ($Format -in @('JSON', 'All')) {
        $objs = foreach ($e in $flaky) { [pscustomobject]@{ Test = $e.Key; Config = $e.Value.Config; Metrics = $e.Value.Metrics; Category = $e.Value.Category; Evidence = $e.Value.Evidence; Recommendations = $e.Value.Recommendations } }
        $jsonPath = Join-Path $OutputDir 'flaky_tests.json'
        if ($null -eq $objs -or ($objs | Measure-Object).Count -eq 0) {
            '[]' | Out-File $jsonPath -Encoding UTF8
        }
        else {
            $objs | ConvertTo-Json -Depth 6 | Out-File $jsonPath -Encoding UTF8
        }
    }

    if ($Format -in @('CSV', 'All')) {
        $objs = foreach ($e in $flaky) { [pscustomobject]@{ Test = $e.Key; Config = $e.Value.Config; FailureRate = $e.Value.Metrics.FailureRate; FailCount = $e.Value.Metrics.FailCount; RunCount = $e.Value.Metrics.RunCount; Category = $e.Value.Category; Evidence = $e.Value.Evidence; Recommendations = ($e.Value.Recommendations -join '; ') } }
        $csvPath = Join-Path $OutputDir 'flaky_tests.csv'
        if ($null -eq $objs -or ($objs | Measure-Object).Count -eq 0) {
            $header = 'Test,Config,FailureRate,FailCount,RunCount,Category,Evidence,Recommendations'
            $header | Out-File $csvPath -Encoding UTF8
        }
        else {
            $objs | Export-Csv $csvPath -NoTypeInformation
        }
    }

    if ($Format -in @('HTML', 'All')) {
        $rows = foreach ($e in $flaky) {
            $v = $e.Value; $name = $e.Key;
            $rate = if ($null -ne $v.Metrics.FailureRate) { ([double]$v.Metrics.FailureRate).ToString('P2', [System.Globalization.CultureInfo]::InvariantCulture) } else { 'N/A' };
            $counts = if ($v.Metrics.RunCount -gt 0) { " ($($v.Metrics.FailCount)/$($v.Metrics.RunCount))" } else { '' };
            "<tr><td>$name</td><td>$($v.Config)</td><td>$rate$counts</td><td>$($v.Category)</td><td>$($v.Evidence)</td><td>$([string]::Join('<br>',($v.Recommendations|Where-Object{$_})))</td></tr>"
        }
        if ($null -eq $rows -or $rows.Count -eq 0) { $rows = @('<tr><td colspan="6">No flaky tests</td></tr>') }
        $rowsHtml = [string]::Join([Environment]::NewLine, @($rows))
        $html = @"
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Flaky Tests Report</title></head>
<body>
<h1>Flaky Tests Report</h1>
<table border="1">
<tr><th>Test</th><th>Config</th><th>Failure Rate</th><th>Category</th><th>Evidence</th><th>Recommendations</th></tr>
$rowsHtml
</table>
</body>
</html>
"@
        $html | Out-File (Join-Path $OutputDir 'flaky_tests.html') -Encoding UTF8
    }
}

$ProjectRoot = Get-ProjectRoot
Write-Output "Project root detected: $ProjectRoot"

$cfgs = @(); if ($BuildConfigs -in @('Both', 'Debug')) { $cfgs += 'Debug' }; if ($BuildConfigs -in @('Both', 'Release')) { $cfgs += 'Release' }
$all = @{}

foreach ($c in $cfgs) {
    if (-not $DryRun) { Invoke-BuildConfig -ProjectRoot $ProjectRoot -Config $c -BuildDir $BuildDir } else { Write-Output "Dry run: Would build $c" }
    Invoke-TestRuns -Config $c -BuildDir $BuildDir -NumRuns $NumRuns -ProjectRoot $ProjectRoot -TestResults $all -RegexFilter $RegexFilter
}

$flakyOut = Measure-TestResults -TestResults $all -Threshold $Threshold
Group-Flakiness -TestResults $all
Write-FlakyReports -TestResults $all -OutputDir $OutputDir -Format $ReportFormat

Write-Output ("Analysis and reports complete. Flaky tests: {0}" -f ($flakyOut.Keys -join ', '))
