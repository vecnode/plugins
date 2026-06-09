# Creates a new out-of-source iPlug2 plugin in this repository.
# Usage: .\scripts\new-plugin.ps1 -Name MyPlugin [-Template IPlugEffect] [-Manufacturer Vecnode]

param(
    [Parameter(Mandatory = $true)]
    [string]$Name,

    [string]$Template = "IPlugEffect",
    [string]$Manufacturer = "Vecnode",
    [string]$IPlug2Root = "",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if (-not $OutputRoot) { $OutputRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path }
if (-not $IPlug2Root) { $IPlug2Root = (Resolve-Path (Join-Path $OutputRoot "..\iPlug2")).Path }

if ($Name -match '\s') { throw "Plugin name cannot contain spaces." }
if ($Name -match '-') { Write-Warning "Hyphens in '$Name' are invalid in C++ class names. Set PLUG_CLASS_NAME in config.h to a valid identifier (e.g. MyPlugin)." }
if ($Manufacturer -match '\s') { throw "Manufacturer name cannot contain spaces." }

$duplicatePy = Join-Path $IPlug2Root "Examples\duplicate.py"
if (-not (Test-Path $duplicatePy)) {
    throw "duplicate.py not found. Is iPlug2 at $IPlug2Root ?"
}

$outputPath = Join-Path $OutputRoot $Name
if (Test-Path $outputPath) {
    throw "Plugin folder already exists: $outputPath"
}

$examplesDir = Join-Path $IPlug2Root "Examples"
Push-Location $examplesDir
try {
    Write-Host "Creating $Name from $Template..."
    & python $duplicatePy $Template $Name $Manufacturer $OutputRoot
    if ($LASTEXITCODE -ne 0) { throw "duplicate.py failed (exit $LASTEXITCODE)" }
}
finally {
    Pop-Location
}

# duplicate.py can double-replace IPLUG2_ROOT paths on Windows (../../.. then ..\..\..)
Get-ChildItem -Path $outputPath -Recurse -File | ForEach-Object {
    $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
    if ($content -and ($content -match 'iPlug2\\iPlug2|iPlug2/iPlug2')) {
        $new = $content -replace 'iPlug2\\iPlug2', 'iPlug2' -replace 'iPlug2/iPlug2', 'iPlug2'
        Set-Content -Path $_.FullName -Value $new -NoNewline
    }
}

$cmakeFile = Join-Path $outputPath "CMakeLists.txt"
if (Test-Path $cmakeFile) {
    $cmake = Get-Content $cmakeFile -Raw
    $cmake = $cmake -replace '(?s)if\(NOT DEFINED IPLUG2_DIR\)\s+set\(IPLUG2_DIR "\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\./\.\.".*?\)\s+endif\(\)', @'
if(NOT DEFINED IPLUG2_DIR)
  get_filename_component(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../iPlug2" ABSOLUTE)
  file(TO_CMAKE_PATH "${IPLUG2_DIR}" IPLUG2_DIR)
  set(IPLUG2_DIR "${IPLUG2_DIR}" CACHE PATH "iPlug2 root directory")
endif()
'@
    if ($cmake -notmatch 'get_filename_component\(IPLUG2_DIR') {
        $cmake = $cmake -replace 'set\(IPLUG2_DIR "\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\./\.\."', 'get_filename_component(IPLUG2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../iPlug2" ABSOLUTE)`n  file(TO_CMAKE_PATH "${IPLUG2_DIR}" IPLUG2_DIR)`n  set(IPLUG2_DIR "${IPLUG2_DIR}"'
    }
    Set-Content -Path $cmakeFile -Value $cmake -NoNewline
}

$embedSnippet = @"

include(`${CMAKE_CURRENT_SOURCE_DIR}/../scripts/cmake/embed-win-resources.cmake)
iplug_embed_win_resources(`${PROJECT_NAME})
"@
if ((Get-Content $cmakeFile -Raw) -notmatch 'iplug_embed_win_resources') {
    Add-Content -Path $cmakeFile -Value $embedSnippet
}

Write-Host "`nCreated: $outputPath"
Write-Host "Next: edit config.h (PLUG_MFR_ID, metadata), then build with .\scripts\build.ps1 -Plugin $Name"
