@echo off
setlocal enabledelayedexpansion

REM VST Scanner Batch Script
REM Scans a directory for VST plugins and outputs information to JSON

if "%1"=="" (
    echo VST Scanner - Scan for VST plugins and output to JSON
    echo.
    echo Usage: %0 ^<directory_path^> [output_file.json] [options]
    echo.
    echo Arguments:
    echo   directory_path    Path to scan for VST plugins
    echo   output_file.json  Optional output file (default: vst_scan_%%date:~-4,4%%%%date:~-10,2%%%%date:~-7,2%%_%%time:~0,2%%%%time:~3,2%%%%time:~6,2%%.json)
    echo.
    echo Options:
    echo   --build-only      Only build the scanner, don't run it
    echo   --clean           Clean build directory before building
    echo   --help            Show this help message
    echo.
    echo Examples:
    echo   %0 C:\path\to\vst\plugins
    echo   %0 C:\path\to\vst\plugins my_plugins.json
    echo   %0 C:\path\to\vst\plugins --clean
    exit /b 1
)

REM Parse command line arguments
set BUILD_ONLY=false
set CLEAN_BUILD=false
set DIRECTORY=
set OUTPUT_FILE=

:parse_args
if "%1"=="" goto :end_parse
if "%1"=="--build-only" (
    set BUILD_ONLY=true
    shift
    goto :parse_args
)
if "%1"=="--clean" (
    set CLEAN_BUILD=true
    shift
    goto :parse_args
)
if "%1"=="--help" (
    goto :show_help
)
if "%1"=="-h" (
    goto :show_help
)
if "!DIRECTORY!"=="" (
    set DIRECTORY=%1
    shift
    goto :parse_args
)
if "!OUTPUT_FILE!"=="" (
    set OUTPUT_FILE=%1
    shift
    goto :parse_args
)
shift
goto :parse_args

:show_help
echo VST Scanner - Scan for VST plugins and output to JSON
echo.
echo Usage: %0 ^<directory_path^> [output_file.json] [options]
echo.
echo Arguments:
echo   directory_path    Path to scan for VST plugins
echo   output_file.json  Optional output file (default: vst_scan_%%date:~-4,4%%%%date:~-10,2%%%%date:~-7,2%%_%%time:~0,2%%%%time:~3,2%%%%time:~6,2%%.json)
echo.
echo Options:
echo   --build-only      Only build the scanner, don't run it
echo   --clean           Clean build directory before building
echo   --help            Show this help message
echo.
echo Examples:
echo   %0 C:\path\to\vst\plugins
echo   %0 C:\path\to\vst\plugins my_plugins.json
echo   %0 C:\path\to\vst\plugins --clean
exit /b 0

:end_parse

REM Check if directory is provided
if "!DIRECTORY!"=="" (
    echo [ERROR] Directory path is required
    exit /b 1
)

REM Check if directory exists
if not exist "!DIRECTORY!" (
    echo [ERROR] Directory does not exist: !DIRECTORY!
    exit /b 1
)

REM Set default output file if not provided
if "!OUTPUT_FILE!"=="" (
    set OUTPUT_FILE=vst_scan_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.json
    set OUTPUT_FILE=!OUTPUT_FILE: =0!
)

REM Get script directory
set SCRIPT_DIR=%~dp0
set BUILD_DIR=!SCRIPT_DIR!build

echo [INFO] VST Scanner starting...
echo [INFO] Directory to scan: !DIRECTORY!
echo [INFO] Output file: !OUTPUT_FILE!
echo [INFO] Build directory: !BUILD_DIR!

REM Create build directory
if "!CLEAN_BUILD!"=="true" (
    echo [INFO] Cleaning build directory...
    if exist "!BUILD_DIR!" rmdir /s /q "!BUILD_DIR!"
)

if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"
cd /d "!BUILD_DIR!"

REM Check if CMake is available
cmake --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake is not installed or not in PATH
    echo Please install CMake from: https://cmake.org/download/
    exit /b 1
)

REM Configure and build
echo [INFO] Configuring project with CMake...
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo [INFO] Building VST scanner...
cmake --build . --config Release
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] Build completed successfully!

if "!BUILD_ONLY!"=="true" (
    echo [INFO] Build-only mode: skipping scan
    exit /b 0
)

REM Run the scanner
echo [INFO] Running VST scanner...
set SCANNER_EXE=

REM Look for the executable in different possible locations
if exist "bin\vst_scanner.exe" (
    set SCANNER_EXE=bin\vst_scanner.exe
) else if exist "bin\Release\vst_scanner.exe" (
    set SCANNER_EXE=bin\Release\vst_scanner.exe
) else if exist "vst_scanner.exe" (
    set SCANNER_EXE=vst_scanner.exe
)

if "!SCANNER_EXE!"=="" (
    echo [ERROR] Could not find vst_scanner executable
    echo [INFO] Looking for executable in build directory...
    dir /s /b vst_scanner.exe 2>nul
    exit /b 1
)

!SCANNER_EXE! "!DIRECTORY!" "!OUTPUT_FILE!"
if errorlevel 1 (
    echo [ERROR] VST scan failed
    exit /b 1
)

echo [SUCCESS] VST scan completed successfully!
echo [INFO] Results saved to: !OUTPUT_FILE!

REM Show summary if the file exists
if exist "!OUTPUT_FILE!" (
    echo [INFO] Scan summary:
    REM Note: Windows batch doesn't have built-in JSON parsing
    REM You can use PowerShell or install jq for Windows to parse the JSON
    echo [INFO] Check the JSON file for detailed results
)

endlocal 