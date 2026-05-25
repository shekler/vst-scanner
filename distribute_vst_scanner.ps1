# Creates a portable package: vst_scanner.exe + launchers (no dev tools required on target PC)

param(
    [string]$OutputDir = "vst_scanner_portable",
    [switch]$Help
)

function Show-Usage {
    Write-Host "Usage: .\distribute_vst_scanner.ps1 [output_directory]" -ForegroundColor White
    Write-Host "  Default output: vst_scanner_portable" -ForegroundColor White
}

function Write-Status { param([string]$Message) Write-Host "[INFO] $Message" -ForegroundColor Blue }
function Write-Success { param([string]$Message) Write-Host "[SUCCESS] $Message" -ForegroundColor Green }
function Write-Err { param([string]$Message) Write-Host "[ERROR] $Message" -ForegroundColor Red }

if ($Help) { Show-Usage; exit 0 }

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"
$TemplateDir = Join-Path $ScriptDir "portable_template"

Write-Status "Creating portable VST Scanner package..."

$scannerExe = $null
foreach ($rel in @("bin\Release\vst_scanner.exe", "bin\vst_scanner.exe")) {
    $full = Join-Path $BuildDir $rel
    if (Test-Path $full) { $scannerExe = $full; break }
}

if (-not $scannerExe) {
    Write-Err "vst_scanner.exe not found. Build first:"
    Write-Host "  cmake --build build --config Release" -ForegroundColor Yellow
    exit 1
}

$templateFiles = @("scan_vst_simple.bat", "scan_vst_simple.ps1", "README.md")
foreach ($f in $templateFiles) {
    if (-not (Test-Path (Join-Path $TemplateDir $f))) {
        Write-Err "Missing template: portable_template\$f"
        exit 1
    }
}

$FullOutputDir = Join-Path $ScriptDir $OutputDir
if (Test-Path $FullOutputDir) {
    Write-Status "Removing existing $OutputDir..."
    Remove-Item $FullOutputDir -Recurse -Force
}
New-Item -ItemType Directory -Path $FullOutputDir | Out-Null

Copy-Item $scannerExe -Destination $FullOutputDir
foreach ($f in $templateFiles) {
    Copy-Item (Join-Path $TemplateDir $f) -Destination $FullOutputDir
}

Write-Success "Portable package created: $FullOutputDir"
Write-Host "  vst_scanner.exe" -ForegroundColor White
foreach ($f in $templateFiles) { Write-Host "  $f" -ForegroundColor White }
