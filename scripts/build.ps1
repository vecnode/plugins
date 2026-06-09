# Build an iPlug2 plugin from this repository.
# Usage: .\scripts\build.ps1 -Plugin GainPlugin [-Format vst3] [-Config Release] [-Deploy] [-Install]

param(
    [Parameter(Mandatory = $true)]
    [string]$Plugin,

    [ValidateSet("cmake", "vs")]
    [string]$Method = "cmake",

    [ValidateSet("app", "vst2", "vst3", "clap", "aax", "all")]
    [string]$Format = "vst3",

    [ValidateSet("Debug", "Release", "Tracer")]
    [string]$Config = "Release",

    [switch]$Deploy,

    [switch]$Install,

    [string]$IPlug2Root = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = Resolve-Path (Join-Path $scriptRoot "..")
if (-not $IPlug2Root) {
    $IPlug2Root = (Resolve-Path (Join-Path $repoRoot "..\iPlug2")).Path
}

$pluginDir = Join-Path $repoRoot $Plugin
if (-not (Test-Path $pluginDir)) {
    throw "Plugin not found: $pluginDir"
}

if (-not (Test-Path (Join-Path $IPlug2Root "iPlug2.cmake"))) {
    throw "iPlug2 not found at: $IPlug2Root"
}

switch ($Method) {
    "cmake" {
        $buildDir = Join-Path $pluginDir "build"
        New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

        Write-Host "Configuring CMake..."
        $iplug2Dir = ($IPlug2Root -replace '\\', '/')
        $deployFlag = if ($Deploy) { "ON" } else { "OFF" }
        if (-not $Deploy) {
            Write-Host "Auto-deploy disabled (use -Deploy to copy into AppData VST3; close Reaper first)."
        }
        cmake -G "Visual Studio 17 2022" -A x64 `
            -S $pluginDir `
            -B $buildDir `
            -DIPLUG2_DIR="$iplug2Dir" `
            -DIPLUG_DEPLOY_PLUGINS="$deployFlag"
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

        $target = if ($Format -eq "all") { "ALL_BUILD" } else { "${Plugin}-${Format}" }
        Write-Host "Building $target ($Config)..."
        cmake --build $buildDir --config $Config --target $target
        if ($LASTEXITCODE -ne 0) { throw "Build failed" }

        $bundleDir = Join-Path $buildDir "out\${Plugin}.${Format}"
        if ((Test-Path $bundleDir) -and $Format -in @("vst3", "clap")) {
            Write-Host "`nBuilt: $bundleDir"
            if ($Install) {
                $installScript = Join-Path $scriptRoot "install-plugin.ps1"
                & $installScript -Plugin $Plugin -Format $Format
            }
            else {
                Write-Host "Install: .\scripts\install-plugin.ps1 -Plugin $Plugin -Format $Format"
            }
        }
        else {
            Write-Host "`nArtifacts are under: $pluginDir\build\out"
        }
    }

    "vs" {
        $sln = Join-Path $pluginDir "${Plugin}.sln"
        if (-not (Test-Path $sln)) {
            throw "Solution not found: $sln"
        }

        $vswhere = @(
            "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
            "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
        ) | Where-Object { Test-Path $_ } | Select-Object -First 1
        if (-not $vswhere) { throw "vswhere not found. Install Visual Studio 2022 with C++ workload." }
        $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if (-not $msbuild) { throw "MSBuild not found. Install Visual Studio 2022 with C++ workload." }

        $projectMap = @{
            app   = "${Plugin}-app"
            vst2  = "${Plugin}-vst2"
            vst3  = "${Plugin}-vst3"
            clap  = "${Plugin}-clap"
            aax   = "${Plugin}-aax"
            all   = $null
        }

        if ($Format -eq "all") {
            & $msbuild $sln /p:Configuration=$Config /p:Platform=x64 /m
        }
        else {
            $proj = Join-Path $pluginDir "projects\$($projectMap[$Format]).vcxproj"
            & $msbuild $proj /p:Configuration=$Config /p:Platform=x64 /m
        }
        if ($LASTEXITCODE -ne 0) { throw "Build failed" }

        Write-Host "`nArtifacts are under: $pluginDir\build-win"
    }
}
