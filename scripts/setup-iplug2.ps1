# Downloads iPlug2 SDKs and prebuilt libraries required to compile plugins.
# Run once after cloning iPlug2, or when dependencies are missing.

param(
    [string]$IPlug2Root = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $IPlug2Root) {
    $IPlug2Root = (Resolve-Path (Join-Path $scriptRoot "..\..\iPlug2")).Path
}

function Invoke-BashScript {
    param([string]$ScriptPath, [string]$WorkingDirectory)
    $bashCandidates = @(
        "C:\Program Files\Git\bin\bash.exe",
        "C:\Program Files (x86)\Git\bin\bash.exe"
    )
    $bash = $bashCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $bash) {
        $bashCmd = Get-Command bash -ErrorAction SilentlyContinue
        if ($bashCmd) { $bash = $bashCmd.Source }
    }
    if (-not $bash) {
        throw "Git Bash not found. Install Git for Windows."
    }
    Push-Location $WorkingDirectory
    try {
        & $bash $ScriptPath
        if ($LASTEXITCODE -ne 0) { throw "Script failed: $ScriptPath (exit $LASTEXITCODE)" }
    }
    finally {
        Pop-Location
    }
}

if (-not (Test-Path $IPlug2Root)) {
    throw "iPlug2 not found at: $IPlug2Root`nExpected sibling layout: Dev/iPlug2 and Dev/plugins"
}

Write-Host "Using iPlug2 at: $IPlug2Root"

$iplugDeps = Join-Path $IPlug2Root "Dependencies\IPlug"
$buildDeps = Join-Path $IPlug2Root "Dependencies\Build\win"

Write-Host "`n[1/2] Downloading plugin SDKs (VST3, CLAP, WAM)..."
Invoke-BashScript (Join-Path $iplugDeps "download-iplug-sdks.sh") $iplugDeps

if (-not (Test-Path $buildDeps)) {
    Write-Host "`n[2/2] Downloading prebuilt Windows libraries..."
    Invoke-BashScript (Join-Path $IPlug2Root "Dependencies\download-prebuilt-libs.sh") (Join-Path $IPlug2Root "Dependencies")
}
else {
    Write-Host "`n[2/2] Prebuilt Windows libraries already present."
}

Write-Host "`niPlug2 dependencies are ready."
