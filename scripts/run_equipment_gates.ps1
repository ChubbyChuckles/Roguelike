Param(
    [switch]$Debug
)

# Phase 18.6 CI gating script: runs only equipment gating label tests.
$buildType = "Release"
$ctestArgs = "-C $buildType -L EQUIP_GATES --output-on-failure"

Write-Host "[Phase18.6] Running equipment gating tests ($buildType) ..."

if($Debug){ $ctestArgs += " -VV" }

# Ensure we are in build directory
if(!(Test-Path build)){ Write-Error "Build directory missing. Run standard build first."; exit 1 }
Push-Location build

# Rebuild only labeled tests & core (fast incremental)
cmake --build . --config $buildType | Out-Null

$proc = Start-Process ctest -ArgumentList $ctestArgs -NoNewWindow -PassThru -Wait
$code = $proc.ExitCode
Pop-Location

if($code -ne 0){
  Write-Error "Equipment gating tests failed (exit $code)."
  exit $code
}
Write-Host "[Phase18.6] All equipment gating tests passed."
