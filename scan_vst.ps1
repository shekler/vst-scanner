# VST Scanner PowerShell Script
# Scans a directory for VST plugins and outputs information to JSON

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Directory,
    
    [Parameter(Mandatory=$false)]
    [Alias("o")]
    [string]$OutputFile,
    
    [Parameter(Mandatory=$false)]
    [Alias("c")]
    [string]$CumulativeFile,
    
    [switch]$BuildOnly,
    [switch]$Clean,
    [switch]$Help
)

# Function to show usage
function Show-Usage {
    Write-Host "VST Scanner - Scan for VST plugins and output to JSON" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\scan_vst.ps1 <directory_path> [options]" -ForegroundColor White
    Write-Host ""
    Write-Host "Arguments:" -ForegroundColor Yellow
    Write-Host "  directory_path    Path to scan for VST plugins" -ForegroundColor White
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -OutputFile <file.json>     Output to specific file" -ForegroundColor White
    Write-Host "  -o <file.json>              Short form for OutputFile" -ForegroundColor White
    Write-Host "  -CumulativeFile <file.json> Append to existing cumulative file" -ForegroundColor White
    Write-Host "  -c <file.json>              Short form for CumulativeFile" -ForegroundColor White
    Write-Host "  -BuildOnly                  Only build the scanner, don't run it" -ForegroundColor White
    Write-Host "  -Clean                      Clean build directory before building" -ForegroundColor White
    Write-Host "  -Help                       Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "Examples:" -ForegroundColor Yellow
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins" -ForegroundColor White
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins -OutputFile my_plugins.json" -ForegroundColor White
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins -o my_plugins.json" -ForegroundColor White
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins -CumulativeFile cumulative_plugins.json" -ForegroundColor White
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins -c cumulative_plugins.json" -ForegroundColor White
    Write-Host "  .\scan_vst.ps1 C:\path\to\vst\plugins -Clean" -ForegroundColor White
}

# Function to print colored output
function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Show help if requested
if ($Help) {
    Show-Usage
    exit 0
}

# Check if directory is provided
if (-not $Directory) {
    Write-Error "Directory path is required"
    Show-Usage
    exit 1
}

# Check if both OutputFile and CumulativeFile are used
if ($OutputFile -and $CumulativeFile) {
    Write-Error "Cannot use both -OutputFile and -CumulativeFile options"
    exit 1
}

# Set default output file if not provided and not using cumulative
if (-not $OutputFile -and -not $CumulativeFile) {
    $OutputFile = "vst_scan_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
}

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"

Write-Status "VST Scanner starting..."
Write-Status "Directory to scan: $Directory"
if ($CumulativeFile) {
    Write-Status "Cumulative file: $CumulativeFile"
} else {
    Write-Status "Output file: $OutputFile"
}
Write-Status "Build directory: $BuildDir"

# Create build directory
if ($Clean) {
    Write-Status "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item $BuildDir -Recurse -Force
    }
}

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

Set-Location $BuildDir

# Check for CMake cache conflicts and clean if necessary
$cmakeCachePath = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path $cmakeCachePath) {
    Write-Status "Checking CMake cache for generator conflicts..."
    # Look for any indication of Unix Makefiles in the cache
    $cacheContent = Get-Content $cmakeCachePath -Raw
    if ($cacheContent -match "Unix Makefiles" -and $cmakeGenerator -like "*Visual Studio*") {
        Write-Warning "Detected Unix Makefiles configuration. Cleaning build directory for Visual Studio..."
        Remove-Item $BuildDir -Recurse -Force
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
        Set-Location $BuildDir
    }
}

# Check if CMake is available
try {
    $cmakeVersion = cmake --version 2>$null
    if (-not $cmakeVersion) {
        throw "CMake not found"
    }
} catch {
    Write-Error "CMake is not installed or not in PATH"
    Write-Host "Please install CMake from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Check if Visual Studio or MSBuild is available
$msbuildPath = $null
$cmakeGenerator = "Visual Studio 17 2022"

# Try to find MSBuild
$possiblePaths = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles}\Microsoft Visual Studio\2019\*\MSBuild\Current\Bin\MSBuild.exe",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\*\MSBuild\Current\Bin\MSBuild.exe"
)

foreach ($path in $possiblePaths) {
    $found = Get-ChildItem -Path $path -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) {
        $msbuildPath = $found.FullName
        break
    }
}

if (-not $msbuildPath) {
    Write-Warning "MSBuild not found, trying alternative build methods..."
    $cmakeGenerator = "Unix Makefiles"
}

# Configure and build
Write-Status "Configuring project with CMake..."
$cmakeArgs = @("-G", $cmakeGenerator, "-DCMAKE_BUILD_TYPE=Release", "..")

$cmakeResult = & cmake @cmakeArgs 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed"
    Write-Host $cmakeResult -ForegroundColor Red
    
    # Check if it's a generator mismatch error
    if ($cmakeResult -match "generator.*Does not match the generator used previously") {
        Write-Host ""
        Write-Host "This error occurs when the build directory contains a previous CMake configuration" -ForegroundColor Yellow
        Write-Host "with a different generator. To fix this, run the script with the -Clean flag:" -ForegroundColor Yellow
        Write-Host "  .\scan_vst.ps1 `"$Directory`" -Clean" -ForegroundColor Cyan
    }
    
    exit 1
}

Write-Status "Building VST scanner..."
if ($cmakeGenerator -like "*Visual Studio*") {
    $buildResult = & cmake --build . --config Release 2>&1
} else {
    $buildResult = & cmake --build . --config Release 2>&1
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    Write-Host $buildResult -ForegroundColor Red
    exit 1
}

Write-Success "Build completed successfully!"

if ($BuildOnly) {
    Write-Status "Build-only mode: skipping scan"
    exit 0
}

# Run the scanner
Write-Status "Running VST scanner..."
$scannerArgs = @($Directory)

if ($CumulativeFile) {
    $scannerArgs += @("-c", $CumulativeFile)
    Write-Status "Using cumulative mode with file: $CumulativeFile"
} elseif ($OutputFile) {
    $scannerArgs += @("-o", $OutputFile)
    Write-Status "Output will be saved to: $OutputFile"
}

$scannerExe = $null

# Look for the executable in different possible locations
$possibleExePaths = @(
    "bin\vst_scanner.exe",
    "bin\Release\vst_scanner.exe",
    "vst_scanner.exe"
)

foreach ($exePath in $possibleExePaths) {
    if (Test-Path $exePath) {
        $scannerExe = $exePath
        break
    }
}

if (-not $scannerExe) {
    Write-Error "Could not find vst_scanner executable"
    Write-Status "Looking for executable in build directory..."
    Get-ChildItem -Recurse -Name "vst_scanner*" | ForEach-Object { Write-Host "  $_" }
    exit 1
}

$scannerResult = & $scannerExe @scannerArgs 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Success "VST scan completed successfully!"
    
    if ($CumulativeFile) {
        Write-Status "Results appended to: $CumulativeFile"
        $OutputFile = $CumulativeFile
    } else {
        Write-Status "Results saved to: $OutputFile"
    }
    
    # Show summary if the file exists
    if (Test-Path $OutputFile) {
        try {
            $jsonContent = Get-Content $OutputFile -Raw | ConvertFrom-Json
            Write-Status "Scan summary:"
            Write-Host "  Total plugins: $($jsonContent.totalPlugins)" -ForegroundColor White
            Write-Host "  Valid plugins: $($jsonContent.validPlugins)" -ForegroundColor White
        } catch {
            Write-Warning "Could not parse JSON output for summary"
        }
    }
} else {
    Write-Error "VST scan failed"
    Write-Host $scannerResult -ForegroundColor Red
    exit 1
} 