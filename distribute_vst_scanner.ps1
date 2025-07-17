# VST Scanner Distribution Script
# Creates a portable package for end users

param(
    [string]$OutputDir = "vst_scanner_portable",
    [switch]$Help
)

function Show-Usage {
    Write-Host "VST Scanner Distribution Script" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage: .\distribute_vst_scanner.ps1 [output_directory] [options]" -ForegroundColor White
    Write-Host ""
    Write-Host "Arguments:" -ForegroundColor Yellow
    Write-Host "  output_directory  Directory to create portable package (default: vst_scanner_portable)" -ForegroundColor White
    Write-Host ""
    Write-Host "Options:" -ForegroundColor Yellow
    Write-Host "  -Help             Show this help message" -ForegroundColor White
    Write-Host ""
    Write-Host "This script creates a portable package that can be distributed to users" -ForegroundColor White
    Write-Host "who don't have CMake or development tools installed." -ForegroundColor White
}

function Write-Status {
    param([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param([string]$Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
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

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ScriptDir "build"

Write-Status "Creating portable VST Scanner package..."

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory not found. Please run the scanner first to build the executable."
    Write-Host "Run: .\scan_vst.ps1 <directory> -BuildOnly" -ForegroundColor Yellow
    exit 1
}

# Look for the built executable
$scannerExe = $null
$possibleExePaths = @(
    "bin\vst_scanner.exe",
    "bin\Release\vst_scanner.exe",
    "vst_scanner.exe"
)

foreach ($exePath in $possibleExePaths) {
    $fullPath = Join-Path $BuildDir $exePath
    if (Test-Path $fullPath) {
        $scannerExe = $fullPath
        break
    }
}

if (-not $scannerExe) {
    Write-Error "Could not find vst_scanner executable in build directory"
    Write-Status "Available files in build directory:"
    Get-ChildItem -Path $BuildDir -Recurse -Name | ForEach-Object { Write-Host "  $_" }
    exit 1
}

# Create output directory
$FullOutputDir = Join-Path $ScriptDir $OutputDir
if (Test-Path $FullOutputDir) {
    Write-Status "Removing existing output directory..."
    Remove-Item $FullOutputDir -Recurse -Force
}

New-Item -ItemType Directory -Path $FullOutputDir | Out-Null

# Copy executable
Write-Status "Copying executable..."
Copy-Item $scannerExe -Destination $FullOutputDir

# Create simple launcher script
Write-Status "Creating launcher script..."
$launcherContent = @"
@echo off
REM VST Scanner Launcher
REM Usage: scan_vst_simple.bat <directory_path> [output_file.json]

if "%~1"=="" (
    echo Usage: scan_vst_simple.bat ^<directory_path^> [output_file.json]
    echo.
    echo Examples:
    echo   scan_vst_simple.bat C:\path\to\vst\plugins
    echo   scan_vst_simple.bat C:\path\to\vst\plugins my_plugins.json
    pause
    exit /b 1
)

if not exist "%~1" (
    echo Error: Directory does not exist: %~1
    pause
    exit /b 1
)

echo VST Scanner starting...
echo Directory to scan: %~1

if "%~2"=="" (
    set OUTPUT_FILE=vst_scan_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.json
    set OUTPUT_FILE=%OUTPUT_FILE: =0%
) else (
    set OUTPUT_FILE=%~2
)

echo Output file: %OUTPUT_FILE%

echo.
echo Scanning VST plugins...
vst_scanner.exe "%~1" "%OUTPUT_FILE%"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Scan completed successfully!
    echo Results saved to: %OUTPUT_FILE%
) else (
    echo.
    echo Scan failed with error code: %ERRORLEVEL%
)

echo.
pause
"@

$launcherPath = Join-Path $FullOutputDir "scan_vst_simple.bat"
$launcherContent | Out-File -FilePath $launcherPath -Encoding ASCII

# Create PowerShell launcher
$psLauncherContent = @"
# VST Scanner PowerShell Launcher
# Usage: .\scan_vst_simple.ps1 <directory_path> [output_file.json]

param(
    [Parameter(Mandatory=`$true, Position=0)]
    [string]`$Directory,
    
    [Parameter(Mandatory=`$false, Position=1)]
    [string]`$OutputFile
)

# Check if directory exists
if (-not (Test-Path `$Directory -PathType Container)) {
    Write-Host "Error: Directory does not exist: `$Directory" -ForegroundColor Red
    exit 1
}

# Set default output file if not provided
if (-not `$OutputFile) {
    `$OutputFile = "vst_scan_`$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
}

Write-Host "VST Scanner starting..." -ForegroundColor Blue
Write-Host "Directory to scan: `$Directory" -ForegroundColor Blue
Write-Host "Output file: `$OutputFile" -ForegroundColor Blue

# Get script directory
`$ScriptDir = Split-Path -Parent `$MyInvocation.MyCommand.Path
`$ScannerExe = Join-Path `$ScriptDir "vst_scanner.exe"

if (-not (Test-Path `$ScannerExe)) {
    Write-Host "Error: vst_scanner.exe not found in the same directory" -ForegroundColor Red
    exit 1
}

Write-Host "Scanning VST plugins..." -ForegroundColor Blue
`$result = & `$ScannerExe "`$Directory" "`$OutputFile" 2>&1

if (`$LASTEXITCODE -eq 0) {
    Write-Host "Scan completed successfully!" -ForegroundColor Green
    Write-Host "Results saved to: `$OutputFile" -ForegroundColor Blue
    
    # Show summary if the file exists
    if (Test-Path `$OutputFile) {
        try {
            `$jsonContent = Get-Content `$OutputFile -Raw | ConvertFrom-Json
            Write-Host "Scan summary:" -ForegroundColor Blue
            Write-Host "  Total plugins: `$(`$jsonContent.totalPlugins)" -ForegroundColor White
            Write-Host "  Valid plugins: `$(`$jsonContent.validPlugins)" -ForegroundColor White
        } catch {
            Write-Host "Could not parse JSON output for summary" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "Scan failed" -ForegroundColor Red
    Write-Host `$result -ForegroundColor Red
    exit 1
}
"@

$psLauncherPath = Join-Path $FullOutputDir "scan_vst_simple.ps1"
$psLauncherContent | Out-File -FilePath $psLauncherPath -Encoding UTF8

# Create README
Write-Status "Creating README..."
$readmeContent = @"
# VST Scanner - Portable Package

This is a portable version of the VST Scanner that can be used without installing CMake or development tools.

## Usage

### Windows Batch File
```
scan_vst_simple.bat <directory_path> [output_file.json]
```

### PowerShell
```
.\scan_vst_simple.ps1 <directory_path> [output_file.json]
```

## Examples

Scan a directory and save to default file:
```
scan_vst_simple.bat "C:\Program Files\VSTPlugins"
```

Scan a directory and save to specific file:
```
scan_vst_simple.bat "C:\Program Files\VSTPlugins" my_plugins.json
```

## Output

The scanner creates a JSON file with information about all VST plugins found in the specified directory.

## Requirements

- Windows 10 or later
- No additional software installation required

## Troubleshooting

If you encounter issues:
1. Make sure the directory path exists
2. Ensure you have read permissions for the directory
3. Try running as administrator if scanning system directories

## Files Included

- `vst_scanner.exe` - The main scanner executable
- `scan_vst_simple.bat` - Windows batch launcher
- `scan_vst_simple.ps1` - PowerShell launcher
- `README.md` - This file
"@

$readmePath = Join-Path $FullOutputDir "README.md"
$readmeContent | Out-File -FilePath $readmePath -Encoding UTF8

Write-Success "Portable package created successfully!"
Write-Status "Package location: $FullOutputDir"
Write-Status ""
Write-Status "Files created:"
Write-Host "  - vst_scanner.exe" -ForegroundColor White
Write-Host "  - scan_vst_simple.bat" -ForegroundColor White
Write-Host "  - scan_vst_simple.ps1" -ForegroundColor White
Write-Host "  - README.md" -ForegroundColor White
Write-Status ""
Write-Status "You can now distribute the '$OutputDir' folder to end users."
Write-Status "They can run the scanner without needing CMake or development tools." 