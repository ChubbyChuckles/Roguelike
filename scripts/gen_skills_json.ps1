#!/usr/bin/env pwsh
$repo = 'C:\Users\Chuck\Desktop\CR_AI_Engineering\GameDev\Roguelike'
Set-Location $repo

# Collect PNGs
$files = Get-ChildItem -Path .\assets\skills -Filter *.png | Sort-Object Name
$result = @()

# deterministic seed (change if you want different pseudo-random output)
$seed = 20250816
$rng = New-Object System.Random $seed

foreach ($f in $files) {
    # strip leading numeric prefix like "01_" or "001-" and leading separators
    $base = $f.BaseName
    $stripped = ($base -replace '^[0-9]+[_\-\s]+','')
    if ($stripped -eq '') { $stripped = $base }

    # parse an integer prefix for optional deterministic mapping; if none found, use 0
    $m = [regex]::Match($base, '^(\\d+)')
    $n = 0
    if ($m.Success) { $n = [int]$m.Groups[1].Value }

    # sensible randomized ranges
    $skill_strength = $rng.Next(1,6)               # 1..5
    $max_rank = $rng.Next(1,6)                     # 1..5
    $is_passive = ($rng.NextDouble() -lt 0.25) ? 1 : 0  # ~25% passive
    if ($is_passive -eq 1) { $base_cooldown_ms = 0 } else { $base_cooldown_ms = $rng.Next(500,12001) }
    $cooldown_reduction_ms_per_rank = $rng.Next(10,301)
    $resource_cost_mana = ($is_passive -eq 1) ? 0 : $rng.Next(0,101)
    $action_point_cost = $rng.Next(0,3)
    $max_charges = ($is_passive -eq 1) ? 0 : $rng.Next(0,3)
    $charge_recharge_ms = ($max_charges -gt 0) ? $rng.Next(1000,60001) : 0
    $cast_time_ms = ($is_passive -eq 1) ? 0 : $rng.Next(0,1201)
    $input_buffer_ms = $rng.Next(0,201)
    $min_weave_ms = $rng.Next(0,501)
    $early_cancel_min_pct = $rng.Next(0,41)
    $tags = 0
    $synergy_id = -1
    $synergy_value_per_rank = 0
    $effect_spec_id = $rng.Next(0,51)

    $h = @{
        name = ($stripped -replace '_',' ')
        icon = "assets/skills/$($f.Name)"
        max_rank = $max_rank
        base_cooldown_ms = $base_cooldown_ms
        cooldown_reduction_ms_per_rank = $cooldown_reduction_ms_per_rank
        is_passive = $is_passive
        tags = $tags
        synergy_id = $synergy_id
        synergy_value_per_rank = $synergy_value_per_rank
        resource_cost_mana = $resource_cost_mana
        action_point_cost = $action_point_cost
        max_charges = $max_charges
        charge_recharge_ms = $charge_recharge_ms
        cast_time_ms = $cast_time_ms
        input_buffer_ms = $input_buffer_ms
        min_weave_ms = $min_weave_ms
        early_cancel_min_pct = $early_cancel_min_pct
        cast_type = 0
        combo_builder = 0
        combo_spender = 0
        effect_spec_id = $effect_spec_id
        skill_strength = $skill_strength
    }
    $result += (New-Object PSObject -Property $h)
}

# Backup existing JSON (if any)
$outFile = Join-Path $repo 'assets\skills_uhf87f.json'
if (Test-Path $outFile) {
    $time = Get-Date -Format 'yyyyMMdd_HHmmss'
    Copy-Item $outFile "$outFile.bak.$time"
}

$json = $result | ConvertTo-Json -Depth 5
Set-Content -Path $outFile -Value $json -Encoding utf8
Write-Output "WROTE $($result.Count) entries to $outFile (seed=$seed)" 
