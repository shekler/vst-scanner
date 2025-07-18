# VST Scanner PowerShell Launcher
# Usage: .\scan_vst_simple.ps1 <directory_path> [output_file.json]

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$Directory,
    
    [Parameter(Mandatory=$false, Position=1)]
    [string]$OutputFile
)

# Check if directory exists
if (-not (Test-Path $Directory -PathType Container)) {
    Write-Host "Error: Directory does not exist: $Directory" -ForegroundColor Red
    exit 1
}

# Set default output file if not provided
if (-not $OutputFile) {
    $OutputFile = "vst_scan_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
}

Write-Host "VST Scanner starting..." -ForegroundColor Blue
Write-Host "Directory to scan: $Directory" -ForegroundColor Blue
Write-Host "Output file: $OutputFile" -ForegroundColor Blue

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ScannerExe = Join-Path $ScriptDir "vst_scanner.exe"

if (-not (Test-Path $ScannerExe)) {
    Write-Host "Error: vst_scanner.exe not found in the same directory" -ForegroundColor Red
    exit 1
}

Write-Host "Scanning VST plugins..." -ForegroundColor Blue
$result = & $ScannerExe "$Directory" "$OutputFile" 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "Scan completed successfully!" -ForegroundColor Green
    Write-Host "Results saved to: $OutputFile" -ForegroundColor Blue
    
    # Show summary if the file exists
    if (Test-Path $OutputFile) {
        try {
            $jsonContent = Get-Content $OutputFile -Raw | ConvertFrom-Json
            Write-Host "Scan summary:" -ForegroundColor Blue
            Write-Host "  Total plugins: $($jsonContent.totalPlugins)" -ForegroundColor White
            Write-Host "  Valid plugins: $($jsonContent.validPlugins)" -ForegroundColor White
        } catch {
            Write-Host "Could not parse JSON output for summary" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "Scan failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    exit 1
}
