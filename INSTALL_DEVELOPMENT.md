# VST Scanner - Development Setup Guide

This guide explains how to set up the development environment to build the VST Scanner from source.

## Prerequisites

### Windows

1. **Visual Studio 2019 or 2022** (Community edition is free)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - During installation, make sure to include:
     - MSVC v143 - VS 2022 C++ x64/x86 build tools
     - Windows 10/11 SDK
     - CMake tools for Visual Studio

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

```powershell
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . --config Release
```

## Creating a Portable Distribution

After building the project, you can create a portable package for end users:

```powershell
# Create portable package
.\distribute_vst_scanner.ps1

# Or specify custom output directory
.\distribute_vst_scanner.ps1 my_portable_package
```

This creates a folder with:
- `vst_scanner.exe` - The compiled scanner
- `scan_vst_simple.bat` - Simple batch launcher
- `scan_vst_simple.ps1` - PowerShell launcher
- `README.md` - Usage instructions

## Troubleshooting

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
- `build/bin/vst_scanner.exe` (or similar path)
- No error messages during build process

You can then test the scanner:
```powershell
.\build\bin\vst_scanner.exe "C:\path\to\vst\plugins" test_output.json
```

## Next Steps

Once you have a working build:

1. **Create portable distribution** using `distribute_vst_scanner.ps1`
2. **Test the portable version** on a clean machine
3. **Distribute the portable package** to end users

The portable version requires no installation and works on any Windows 10+ machine. 