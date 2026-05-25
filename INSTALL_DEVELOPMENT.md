# VST Scanner - Development Setup Guide

This guide explains how to set up the development environment to build the VST Scanner from source.

See also [README.md](README.md) for usage, scan options, and quick start.

## Prerequisites

### Windows

1. **Visual Studio 2019, 2022, or 2026 (v18)** (Community edition is free)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Workload: **Desktop development with C++**
   - Individual components:
     - MSVC v143/v145 C++ x64/x86 build tools (latest)
     - Windows 10/11 SDK
     - C++ CMake tools for Windows (optional if CMake on PATH)

2. **CMake** (if not installed with Visual Studio)
   - Download from: https://cmake.org/download/
   - Add to PATH during installation

### Alternative: Visual Studio Build Tools Only

If you don't want the full Visual Studio IDE:

1. **Visual Studio Build Tools 2022**
   - Download from: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
   - Install with:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools
     - Windows 10/11 SDK
     - CMake tools for Visual Studio

2. **CMake** (separate installation)
   - Download from: https://cmake.org/download/

## Building the Project

### Method 1: Using the PowerShell Script (Recommended)

```powershell
# Build only (don't run scan)
.\scan_vst.ps1 <any_directory> -BuildOnly

# Or build and scan
.\scan_vst.ps1 "C:\path\to\vst\plugins"
```

### Method 2: Manual CMake Build

**Important:** never run `cmake` from the repo root (`G:\vst-scanner`). The VST SDK forbids in-source builds. Always use a separate `build` folder and pass `..` as the source path.

```powershell
cd G:\vst-scanner

# If you accidentally configured in the repo root, delete:
#   CMakeCache.txt, CMakeFiles\  (in G:\vst-scanner, not in build\)

mkdir build -Force
cd build

# Configure (note the .. at the end — points to parent = project root)
cmake -G "Visual Studio 18 2026" -A x64 ..
# cmake -G "Visual Studio 17 2022" -A x64 ..

# Build
cmake --build . --config Release
```

Output: `build\bin\Release\vst_scanner.exe`

### Method 3: CMake one-liner (Windows)

Works from any working directory — avoids `cd build` mistakes:

```powershell
.\scripts\init-sdk.ps1
cmake -G "Visual Studio 18 2026" -A x64 -S G:\vst-scanner -B G:\vst-scanner\build
cmake --build G:\vst-scanner\build --config Release
```

Replace `G:\vst-scanner` with your clone path. Use `Visual Studio 17 2022` if VS 2026 is not installed.

## Creating a Portable Distribution

After building the project, you can create a portable package for end users:

```powershell
# Create portable package
.\distribute_vst_scanner.ps1

# Or specify custom output directory
.\distribute_vst_scanner.ps1 my_portable_package
```

Copies `build\bin\Release\vst_scanner.exe` plus files from `portable_template/` into `vst_scanner_portable/` (gitignored).

## Troubleshooting

### "In-source builds are not allowed"

You ran `cmake` from `G:\vst-scanner` (repo root) without a separate build directory.

```powershell
cd G:\vst-scanner
Remove-Item CMakeCache.txt, CMakeFiles -Recurse -Force -ErrorAction SilentlyContinue
mkdir build -Force
cd build
cmake -G "Visual Studio 18 2026" -A x64 ..
```

The `..` is required — it tells CMake the source is the parent folder.

### "cmake is not recognized"

CMake is installed but the terminal was opened **before** install, so PATH is stale.

**Quick fix (current session only):**
```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
cmake --version
```

**Or use full path:**
```powershell
& "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 18 2026" -A x64 ..
```

**Permanent:** close and reopen the terminal (or restart Cursor), then `cmake` should work.

**If still missing:** `winget install Kitware.CMake` and reopen terminal.

### "No CMAKE_C_COMPILER could be found"

This means CMake can't find a C++ compiler. Solutions:

1. **Install Visual Studio** with C++ build tools
2. **Set environment variables**:
   ```powershell
   $env:CC = "cl.exe"
   $env:CXX = "cl.exe"
   ```
3. **Use Developer Command Prompt**:
   - Open "Developer Command Prompt for VS 2022"
   - Navigate to your project
   - Run the build commands

### "MSBuild not found"

The script will automatically fall back to alternative build methods, but for best results:

1. **Install Visual Studio** (not just Build Tools)
2. **Or install MSBuild separately**:
   ```powershell
   # Install via winget
   winget install Microsoft.VisualStudio.2022.BuildTools
   ```

### Build Errors

If you encounter build errors:

1. **Update Visual Studio** to the latest version
2. **Install Windows 10/11 SDK** if missing
3. **Check VST3 SDK submodules** are properly initialized:
   ```powershell
   git submodule update --init --recursive
   ```

## Verification

After successful build, you should see:
- `build/bin/Release/vst_scanner.exe` (Visual Studio multi-config generator)
- No error messages during build process

You can then test the scanner:
```powershell
.\scripts\init-sdk.ps1
.\build\bin\Release\vst_scanner.exe "C:\path\to\vst\plugins" -o test_output.json
```

## Next Steps

Once you have a working build:

1. **Create portable distribution** using `distribute_vst_scanner.ps1`
2. **Test the portable version** on a clean machine
3. **Distribute the portable package** to end users

The portable version requires no installation and works on any Windows 10+ machine. 