# Fail if forbidden APIs appear in RT DSP headers (ProcessBlock path).
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$dspDir = Join-Path $repoRoot "src\audioagent\dsp"
$transportFile = Join-Path $dspDir "SampleTransport.h"
$playerFile = Join-Path $dspDir "SamplePlayer.h"
$engineFile = Join-Path $repoRoot "src\audioagent\SamplerEngine.h"

$forbidden = @(
  '\bstd::mutex\b',
  '\bstd::lock_guard\b',
  '\bstd::unique_lock\b',
  '\bstd::condition_variable\b',
  '\bpitchShiftObj_',
  '\bpitchYINObj_',
  '\bnew\s+',
  '\bdelete\s+',
  '\bmalloc\s*\(',
  '\bfree\s*\('
)

$rtFiles = @($playerFile, $transportFile, $engineFile)
$violations = @()

foreach ($file in $rtFiles) {
  if (-not (Test-Path $file)) {
    Write-Error "Missing RT file: $file"
  }

  $lines = Get-Content $file
  for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    foreach ($pattern in $forbidden) {
      if ($line -match $pattern) {
        $violations += "${file}:$($i + 1): $line"
      }
    }
  }
}

# Fail if KickScheduler is invoked from ProcessBlock body (must run from Tick only).
$transportLines = Get-Content $transportFile
$inProcessBlock = $false
$braceDepth = 0
for ($i = 0; $i -lt $transportLines.Count; $i++) {
  $line = $transportLines[$i]
  if ($line -match 'void ProcessBlock\s*\(') {
    $inProcessBlock = $true
    $braceDepth = 0
  }
  if ($inProcessBlock) {
    $braceDepth += ([regex]::Matches($line, '\{')).Count
    $braceDepth -= ([regex]::Matches($line, '\}')).Count
    if ($line -match 'KickScheduler') {
      $violations += "${transportFile}:$($i + 1): KickScheduler must not be called from ProcessBlock"
    }
    if ($braceDepth -le 0 -and $line -match '\}') {
      $inProcessBlock = $false
    }
  }
}

if ($violations.Count -gt 0) {
  Write-Host "RT audio audit failed:" -ForegroundColor Red
  $violations | ForEach-Object { Write-Host "  $_" }
  exit 1
}

Write-Host "RT audio audit passed ($($rtFiles.Count) files)." -ForegroundColor Green
exit 0
