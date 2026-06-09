# Install a built plugin into the user VST3/CLAP folder.
# Usage: .\scripts\install-plugin.ps1 -Plugin CamelotSynth [-Format vst3]

param(
    [Parameter(Mandatory = $true)]
    [string]$Plugin,

    [ValidateSet("vst3", "clap")]
    [string]$Format = "vst3"
)

$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = Resolve-Path (Join-Path $scriptRoot "..")
$pluginDir = Join-Path $repoRoot $Plugin
$source = Join-Path $pluginDir "build\out\${Plugin}.${Format}"

$destRoot = if ($Format -eq "vst3") {
    Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
}
else {
    Join-Path $env:LOCALAPPDATA "Programs\Common\CLAP"
}
$dest = Join-Path $destRoot "${Plugin}.${Format}"
$staging = "${dest}.pending"

if (-not (Test-Path $source)) {
    throw "Built bundle not found: $source`nRun: .\scripts\build.ps1 -Plugin $Plugin -Format $Format -Config Release"
}

function Test-DllLocked {
    param([string]$BundlePath)
    $dll = Join-Path $BundlePath "Contents\x86_64-win\${Plugin}.${Format}"
    if ($Format -eq "clap") {
        $dll = Join-Path $BundlePath "${Plugin}.clap"
    }
    if (-not (Test-Path $dll)) { return $false }
    try {
        $stream = [System.IO.File]::Open($dll, [System.IO.FileMode]::Open, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
        $stream.Close()
        return $false
    }
    catch {
        return $true
    }
}

function Get-HostWarnings {
    $names = @("reaper", "Ableton Live", "FL64", "Cubase", "Studio One", "Bitwig", "Logic")
    $running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $names -contains $_.ProcessName -or $_.ProcessName -like "reaper*"
    }
    if ($running) {
        return ($running | ForEach-Object { $_.ProcessName } | Sort-Object -Unique) -join ", "
    }
    return $null
}

function Install-Bundle {
    param(
        [string]$From,
        [string]$To
    )
    New-Item -ItemType Directory -Force -Path $destRoot | Out-Null
    if (Test-Path $To) {
        Remove-Item -Recurse -Force $To
    }
    # robocopy returns 0-7 for success; >=8 is failure
    $null = robocopy $From $To /MIR /R:2 /W:1 /NFL /NDL /NJH /NJS
    if ($LASTEXITCODE -ge 8) {
        throw "robocopy failed with exit code $LASTEXITCODE"
    }
}

# Finish a staged install once the host has released the DLL.
if (Test-Path $staging) {
    if (Test-DllLocked $dest) {
        $hosts = Get-HostWarnings
        Write-Host "Still locked: $dest"
        if ($hosts) { Write-Host "Running: $hosts" }
        Write-Host "Close your DAW (and remove the plugin from any track), then run this again."
        exit 1
    }
    Write-Host "Applying staged install from $staging ..."
    Install-Bundle -From $staging -To $dest
    Remove-Item -Recurse -Force $staging
    Write-Host "Installed: $dest"
    exit 0
}

if (Test-DllLocked $dest) {
    $hosts = Get-HostWarnings
    Write-Host "Destination is in use: $dest"
    if ($hosts) { Write-Host "Likely host: $hosts" }
    Write-Host "Staging build to $staging (close your DAW, then run this script again)."
    if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
    Install-Bundle -From $source -To $staging
    Write-Host "Staged. Close Reaper/your DAW, then:"
    Write-Host "  .\scripts\install-plugin.ps1 -Plugin $Plugin -Format $Format"
    exit 0
}

Write-Host "Installing $source -> $dest"
Install-Bundle -From $source -To $dest
Write-Host "Installed: $dest"
