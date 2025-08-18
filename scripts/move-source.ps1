<#!
.SYNOPSIS
  Move a C source file (and its header if present) to a new directory and rewrite includes & CMakeLists references.

.DESCRIPTION
  Given an absolute path to a .c file inside the project, this script will:
    * Detect project root (or use -ProjectRoot) by locating a CMakeLists.txt or .git folder.
    * Optionally move matching header (.h) residing beside the source.
    * Update all #include "..." directives across the project that resolve to the moved header path, rewriting them with a new relative path from each including file to the new header location.
    * Update CMakeLists.txt files replacing occurrences of the old relative path(s) with the new ones (source and header if present).
    * Provide dry-run (-WhatIf) and safety checks (duplicate header name ambiguity, missing targets, etc.).
    * Optionally use git mv if repository is git controlled (disable with -NoGitMv).
    * Expose cache/stat style summary of modifications and warnings.

.PARAMETER Source
  Absolute path to the .c file to move.

.PARAMETER TargetDirectory
  Target directory (absolute or relative to project root) to move the .c (and .h) into. Created if missing.

.PARAMETER ProjectRoot
  Optional explicit project root. Auto-detected if omitted.

.PARAMETER RunBuild
  If specified, will attempt a post-move build (cmake --build . --config Release) in the existing build directory if present.

.PARAMETER StageChanges
  If specified and git repo detected, will git add modified files after operation.

.PARAMETER NoGitMv
  Use filesystem Move-Item instead of git mv even if git repo.

.PARAMETER Verbose
  Show detailed per-file modifications.

.EXAMPLE
  ./scripts/move-source.ps1 -Source "C:\proj\src\core\inventory_query.c" -TargetDirectory src\inventory -RunBuild -StageChanges

.NOTES
  Assumes Windows PowerShell 5+ or PowerShell Core. Paths normalized to forward slashes in source & CMake.
#>
[CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='Medium')]
param(
  [Parameter(Mandatory=$true)][string]$Source,
  [Parameter(Mandatory=$true)][string]$TargetDirectory,
  [string]$ProjectRoot,
  [switch]$RunBuild,
  [switch]$StageChanges,
  [switch]$NoGitMv
)

function Resolve-NormalizedPath([string]$Path){
  $p = Resolve-Path -LiteralPath $Path -ErrorAction Stop
  return ($p.ProviderPath)
}

function Get-ProjectRoot([string]$start){
  $dir = (Get-Item -LiteralPath $start).FullName
  if(Test-Path -LiteralPath (Join-Path $dir 'CMakeLists.txt')){ return $dir }
  while($true){
    $parent = Split-Path -Path $dir -Parent
    if(-not $parent -or $parent -eq $dir){ break }
    if(Test-Path -LiteralPath (Join-Path $parent 'CMakeLists.txt') -or Test-Path -LiteralPath (Join-Path $parent '.git')){ return $parent }
    $dir = $parent
  }
  throw "Unable to detect project root from $start"
}

function Get-RelativePath([string]$FromDir,[string]$ToPath){
  $uriFrom = (New-Object System.Uri ((Resolve-Path -LiteralPath $FromDir).ProviderPath + [IO.Path]::DirectorySeparatorChar))
  $uriTo = New-Object System.Uri (Resolve-Path -LiteralPath $ToPath).ProviderPath
  $rel = $uriFrom.MakeRelativeUri($uriTo).ToString()
  return ($rel -replace '%20',' ') -replace '\\','/'
}

function Get-NormalizedIncludePath([string]$FromFile,[string]$HeaderPath){
  # Produce shortest usable relative path (without leading ./)
  $fromDir = Split-Path -Path $FromFile -Parent
  $rel = Get-RelativePath -FromDir $fromDir -ToPath $HeaderPath
  if($rel.StartsWith('./')){ $rel = $rel.Substring(2) }
  return $rel
}

function Test-GitRepo([string]$root){ return (Test-Path -LiteralPath (Join-Path $root '.git')) }

$Source = (Resolve-Path -LiteralPath $Source).ProviderPath
if(-not $Source.EndsWith('.c')){ throw "Source must be a .c file" }
if(-not (Test-Path -LiteralPath $Source)){ throw "Source file not found: $Source" }

if(-not $ProjectRoot){ $ProjectRoot = Get-ProjectRoot (Split-Path -Path $Source -Parent) }
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).ProviderPath

# Resolve / create target directory
if(-not [IO.Path]::IsPathRooted($TargetDirectory)){ $TargetDirectory = Join-Path $ProjectRoot $TargetDirectory }
if(-not (Test-Path -LiteralPath $TargetDirectory)){ if($PSCmdlet.ShouldProcess($TargetDirectory,'Create Directory')){ New-Item -ItemType Directory -Path $TargetDirectory | Out-Null } }
$TargetDirectory = (Resolve-Path -LiteralPath $TargetDirectory).ProviderPath

$OldSource = $Source
$BaseName = [IO.Path]::GetFileNameWithoutExtension($OldSource)
$OldDir    = Split-Path -Path $OldSource -Parent
$HeaderCandidate = Join-Path $OldDir ($BaseName + '.h')
$HasHeader = Test-Path -LiteralPath $HeaderCandidate
$OldHeader = if($HasHeader){ (Resolve-Path -LiteralPath $HeaderCandidate).ProviderPath } else { $null }

$NewSource = Join-Path $TargetDirectory ([IO.Path]::GetFileName($OldSource))
$NewHeader = if($HasHeader){ Join-Path $TargetDirectory ([IO.Path]::GetFileName($OldHeader)) } else { $null }

if(Test-Path -LiteralPath $NewSource){ throw "Target source already exists: $NewSource" }
if($HasHeader -and (Test-Path -LiteralPath $NewHeader)){ throw "Target header already exists: $NewHeader" }

$git = (Test-GitRepo $ProjectRoot) -and -not $NoGitMv

Write-Host "Project Root: $ProjectRoot" -ForegroundColor Cyan
Write-Host "Moving: $OldSource" -ForegroundColor Cyan
if($HasHeader){ Write-Host "Header:  $OldHeader" -ForegroundColor Cyan }
Write-Host "   -> Dir: $TargetDirectory" -ForegroundColor Cyan

# Move files
if($PSCmdlet.ShouldProcess($NewSource,'Move Source/Header')){
  if($git){ & git mv -- "$OldSource" "$NewSource" | Out-Null } else { Move-Item -LiteralPath $OldSource -Destination $NewSource }
  if($HasHeader){ if($git){ & git mv -- "$OldHeader" "$NewHeader" | Out-Null } else { Move-Item -LiteralPath $OldHeader -Destination $NewHeader } }
}

# Gather all candidate code files (exclude just-moved ones)
$CodeFiles = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -Include *.c,*.h | Where-Object {
  $_.FullName -ne $NewSource -and (-not $HasHeader -or $_.FullName -ne $NewHeader)
}

$Modified = @()
# (Unused placeholder removed)
if($HasHeader){
  # Precompute canonical old header path for resolution (case-insensitive compare)
  $oldHeaderLower = $OldHeader.ToLowerInvariant()
  # Detect duplicate header names elsewhere (potential ambiguity for bare includes)
  $sameName = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -Filter ([IO.Path]::GetFileName($OldHeader)) | Where-Object { $_.FullName -ne $NewHeader }
  $AmbiguousBare = $sameName.Count -gt 0
  if($AmbiguousBare){ Write-Warning "Multiple headers named $([IO.Path]::GetFileName($OldHeader)) exist. Bare includes may be ambiguous; only those resolving to original path will be changed." }

  foreach($f in $CodeFiles){
    $text = Get-Content -LiteralPath $f.FullName -Raw
  $fileChanged = $false
    $newLines = $text -split "`n" | ForEach-Object {
      $line = $_
      if($line -match '^[ \t]*#[ \t]*include[ \t]*"([^"]+)"'){ $inc = $Matches[1]
        # Try resolve relative to file dir
        $candidate1 = Join-Path (Split-Path -Path $f.FullName -Parent) $inc
        $resolved = $null
        if(Test-Path -LiteralPath $candidate1){ $resolved = (Resolve-Path -LiteralPath $candidate1).ProviderPath }
        else {
          # Try project root relative
          $candidate2 = Join-Path $ProjectRoot $inc
          if(Test-Path -LiteralPath $candidate2){ $resolved = (Resolve-Path -LiteralPath $candidate2).ProviderPath }
        }
        if($resolved -and ($resolved.ToLowerInvariant() -eq $oldHeaderLower)){
          $newInc = Get-NormalizedIncludePath -FromFile $f.FullName -HeaderPath $NewHeader
          if($newInc -ne $inc){ $line = $line -replace [regex]::Escape('"'+$inc+'"'), ('"'+$newInc+'"'); $fileChanged = $true }
        }
      }
      $line
    } | Out-String
  if($fileChanged -eq $true){
      if($PSCmdlet.ShouldProcess($f.FullName,'Rewrite includes')){ $newLines | Set-Content -LiteralPath $f.FullName -Encoding UTF8 }
      $Modified += $f.FullName
    }
  }
}

# Update CMakeLists.txt entries
$CMakeFiles = Get-ChildItem -LiteralPath $ProjectRoot -Recurse -File -Filter CMakeLists.txt
$OldSourceRelBefore = ($OldSource -replace [regex]::Escape($ProjectRoot + [IO.Path]::DirectorySeparatorChar),'') -replace '\\','/'
$OldHeaderRelBefore = if($HasHeader){ ($OldHeader -replace [regex]::Escape($ProjectRoot + [IO.Path]::DirectorySeparatorChar),'') -replace '\\','/' } else { $null }
$NewSourceRel = ($NewSource -replace [regex]::Escape($ProjectRoot + [IO.Path]::DirectorySeparatorChar),'') -replace '\\','/'
$NewHeaderRel = if($HasHeader){ ($NewHeader -replace [regex]::Escape($ProjectRoot + [IO.Path]::DirectorySeparatorChar),'') -replace '\\','/' } else { $null }

foreach($cm in $CMakeFiles){
  $txt = Get-Content -LiteralPath $cm.FullName -Raw
  $replaced = $false
  if($txt -match [regex]::Escape($OldSourceRelBefore)){ $txt = $txt -replace [regex]::Escape($OldSourceRelBefore), $NewSourceRel; $replaced = $true }
  if($HasHeader -and $txt -match [regex]::Escape($OldHeaderRelBefore)){ $txt = $txt -replace [regex]::Escape($OldHeaderRelBefore), $NewHeaderRel; $replaced = $true }
  if($replaced){ if($PSCmdlet.ShouldProcess($cm.FullName,'Update CMakeLists path(s)')){ $txt | Set-Content -LiteralPath $cm.FullName -Encoding UTF8 }; $Modified += $cm.FullName }
}

# Optionally stage changes
if($StageChanges -and (Test-GitRepo $ProjectRoot)){
  if($PSCmdlet.ShouldProcess('git add','Stage modified files')){ & git add -- $NewSource; if($HasHeader){ & git add -- $NewHeader }
    foreach($m in ($Modified | Sort-Object -Unique)){ & git add -- $m }
  }
}

Write-Host "Move complete." -ForegroundColor Green
Write-Host "New source: $NewSource" -ForegroundColor Green
if($HasHeader){ Write-Host "New header: $NewHeader" -ForegroundColor Green }
Write-Host ("Modified files: {0}" -f ($Modified | Sort-Object -Unique).Count)
if($Modified.Count -gt 0){ ($Modified | Sort-Object -Unique) | ForEach-Object { Write-Host "  $_" } }

if($RunBuild){
  $buildDir = Join-Path $ProjectRoot 'build'
  if(Test-Path -LiteralPath $buildDir){
    Write-Host "Running build (Release)..." -ForegroundColor Yellow
    Push-Location $buildDir
    try { & cmake --build . --config Release | Write-Host } catch { Write-Warning "Build failed: $_" }
    Pop-Location
  } else {
    Write-Warning "Build directory not found; skipping build."
  }
}

