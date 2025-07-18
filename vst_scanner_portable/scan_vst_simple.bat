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
