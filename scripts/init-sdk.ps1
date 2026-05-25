# Initialize and pin VST3 SDK submodule (v3.8.0) with nested submodules.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $Root

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "git is required"
    exit 1
}

Write-Host "[INFO] Updating vst3sdk submodule..." -ForegroundColor Blue
git submodule update --init --recursive vst3sdk
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Set-Location (Join-Path $Root "vst3sdk")
$tag = "v3.8.0_build_66"
$current = git describe --tags --exact-match 2>$null
if ($current -ne $tag) {
    Write-Host "[INFO] Checking out $tag..." -ForegroundColor Blue
    git fetch --tags origin
    git checkout $tag
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    git submodule update --init --recursive
}

Set-Location $Root
Write-Host "[SUCCESS] VST3 SDK ready at $(git -C vst3sdk rev-parse --short HEAD)" -ForegroundColor Green
