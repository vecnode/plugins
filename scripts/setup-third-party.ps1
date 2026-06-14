# Downloads third-party library sources required by plugins.
# Run once after cloning, or when bumping pinned versions.

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = Resolve-Path (Join-Path $scriptRoot "..")

function Install-GitHubTag {
    param(
        [string]$Name,
        [string]$RepoUrl,
        [string]$DestDir,
        [string]$VersionFile
    )

    $version = (Get-Content $VersionFile -Raw).Trim()
    $tag = "v$version"
    $upstreamMarker = Join-Path $DestDir "upstream\.git"

    $upstreamDir = Join-Path $DestDir "upstream"
    if ((Test-Path $upstreamMarker) -and -not $Force) {
        Write-Host "[$Name] upstream already present (use -Force to re-fetch)."
        return
    }

    if (Test-Path $upstreamDir) {
        Remove-Item -Recurse -Force $upstreamDir
    }
    New-Item -ItemType Directory -Force -Path $upstreamDir | Out-Null

    Write-Host "[$Name] Cloning $tag from $RepoUrl ..."
    git clone --depth 1 --branch $tag $RepoUrl (Join-Path $DestDir "upstream")
    if ($LASTEXITCODE -ne 0) { throw "Failed to clone $Name ($tag)" }

    Write-Host "[$Name] Ready at $(Join-Path $DestDir 'upstream')"
}

Write-Host "Fetching third-party sources into: $repoRoot"

Install-GitHubTag `
    -Name "audioFlux" `
    -RepoUrl "https://github.com/libAudioFlux/audioFlux.git" `
    -DestDir (Join-Path $repoRoot "third_party\audioFlux") `
    -VersionFile (Join-Path $repoRoot "third_party\audioFlux\VERSION")

Write-Host "`nThird-party dependencies are ready."
