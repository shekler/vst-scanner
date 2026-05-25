# VST Scanner

A C++ application that uses the official VST3 SDK to scan directories for VST plugins and output their information to JSON format.

## Features

- **Cross-platform**: Works on Windows, macOS, and Linux
- **Comprehensive scanning**: Finds VST3 plugins in specified directories
- **JSON output**: Exports detailed plugin information in structured JSON format
- **Error handling**: Gracefully handles invalid or corrupted plugins
- **Easy to use**: Simple command-line interface with shell/PowerShell scripts

## What it scans

The VST scanner extracts the following information from each VST plugin:

- **Basic info**: Name, vendor, version, category
- **Technical details**: Class ID (CID), SDK version, cardinality, flags
- **Categories**: Main category and subcategories
- **Validation**: Whether the plugin is valid and any error messages
- **File path**: Full path to the plugin file

## Requirements

### Prerequisites

- **Git** (for VST3 SDK submodule)
- **CMake** 3.25.0+ ([download](https://cmake.org/download/) or `winget install Kitware.CMake`)
- **C++ compiler**:
  - Windows: Visual Studio 2019/2022/2026 with **Desktop development with C++** (see [INSTALL_DEVELOPMENT.md](INSTALL_DEVELOPMENT.md))
  - macOS: Xcode Command Line Tools
  - Linux: GCC 13+ or Clang 7+ (see [VST3 SDK requirements](https://steinbergmedia.github.io/vst3_dev_portal/pages/Getting+Started/How+to+setup+my+system.html))

### VST3 SDK (submodule)

This project uses the official [Steinberg VST3 SDK](https://github.com/steinbergmedia/vst3sdk) as a git submodule, pinned to **v3.8.0** (`v3.8.0_build_66`).

```bash
# Clone with SDK
git clone --recursive git@github.com:shekler/vst-scanner.git
cd vst-scanner

# Or init SDK after clone
git submodule update --init --recursive
./scripts/init-sdk.sh   # Linux/macOS
# .\scripts\init-sdk.ps1  # Windows
```

### Optional

- **jq** (for JSON processing in bash script)
- **PowerShell** (for Windows users)

## Quick Start

### Using the Shell Script (Linux/macOS)

```bash
# Make the script executable
chmod +x scan_vst.sh

# Scan a directory for VST plugins
./scan_vst.sh /path/to/vst/plugins

# Scan and save to specific file
./scan_vst.sh /path/to/vst/plugins my_plugins.json

# Clean build and scan
./scan_vst.sh /path/to/vst/plugins --clean

# Handle paths with spaces (always use quotes)
./scan_vst.sh "/path/with spaces/vst plugins"
./scan_vst.sh "/Users/My User/Music/VST Plugins"
```

### Using PowerShell (Windows)

```powershell
# Scan a directory for VST plugins
.\scan_vst.ps1 C:\path\to\vst\plugins

# Scan and save to specific file
.\scan_vst.ps1 C:\path\to\vst\plugins -OutputFile my_plugins.json
.\scan_vst.ps1 C:\path\to\vst\plugins -o my_plugins.json

# Scan and append to cumulative file
.\scan_vst.ps1 C:\path\to\vst\plugins -CumulativeFile cumulative_plugins.json
.\scan_vst.ps1 C:\path\to\vst\plugins -c cumulative_plugins.json

# Clean build and scan
.\scan_vst.ps1 C:\path\to\vst\plugins -Clean

# Handle paths with spaces (always use quotes)
.\scan_vst.ps1 "C:\My Music\VST Plugins"
.\scan_vst.ps1 "C:\Program Files (x86)\Steinberg\VSTPlugins"
```

### Manual Build and Run

Full details and troubleshooting: **[INSTALL_DEVELOPMENT.md](INSTALL_DEVELOPMENT.md)**

#### 1. Get the code and SDK

```bash
git clone --recursive git@github.com:shekler/vst-scanner.git
cd vst-scanner
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
./scripts/init-sdk.sh          # Linux/macOS
# .\scripts\init-sdk.ps1       # Windows
```

#### 2. Configure and build

**Do not run `cmake` from the repo root** — the VST SDK forbids in-source builds. Use a `build/` folder (or `-S`/`-B` below).

**Windows (Visual Studio):**

```powershell
.\scripts\init-sdk.ps1
mkdir build -Force
cd build
cmake -G "Visual Studio 18 2026" -A x64 ..
# Or VS 2022: cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

**Windows (one-liner from any directory):**

```powershell
cmake -G "Visual Studio 18 2026" -A x64 -S G:\vst-scanner -B G:\vst-scanner\build
cmake --build G:\vst-scanner\build --config Release
```

**Linux / macOS:**

```bash
./scripts/init-sdk.sh
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

#### 3. Run the scanner

| Platform | Built executable |
|----------|------------------|
| Windows (VS) | `build\bin\Release\vst_scanner.exe` |
| Linux / macOS | `build/bin/vst_scanner` |

```powershell
# Windows example
.\build\bin\Release\vst_scanner.exe "C:\Program Files\Common Files\VST3" -o output.json -q
```

**CMake Presets** (optional): `cmake --preset vs2026` then `cmake --build --preset release-vs`

**Windows CMake install:** `winget install Kitware.CMake` (restart terminal afterward)

## Output Format

The scanner outputs a JSON file with the following structure:

```json
{
  "scanTime": "1703123456789",
  "totalPlugins": 5,
  "validPlugins": 4,
  "plugins": [
    {
      "path": "/path/to/plugin.vst3",
      "isValid": true,
      "name": "My Plugin",
      "vendor": "My Company",
      "version": "1.0.0",
      "category": "Fx",
      "cid": "12345678-1234-1234-1234-123456789012",
      "sdkVersion": "VST 3.7.0",
      "cardinality": 1,
      "flags": 0,
      "subCategories": ["Fx", "Distortion"]
    },
    {
      "path": "/path/to/invalid.vst3",
      "isValid": false,
      "error": "Failed to load plugin: Invalid format"
    }
  ]
}
```

## Supported Plugin Formats

- **Windows**: `.vst3` bundle folders and single-file `.vst3` modules (DLL)
- **macOS**: `.vst3` bundles and `.bundle` files
- **Linux**: `.vst3` files and `.so` libraries

## Command Line Options

The VST scanner supports the following command-line options:

### Basic Options
- `-o <output_file.json>`: Output results to a specific file (default: stdout)
- `-c <cumulative_file.json>`: Append to existing cumulative file
- `--timeout <seconds>`: Per-plugin factory load timeout (default: 3)
- `--try-license-load`: Load DLLs even when PACE/iLok wrappers detected (slow; default skips them)
- `--no-isolate`: Load plugins in-process on Windows (risky with iLok/license dialogs)
- `--no-factory`: List discovered `.vst3` paths only; never load plugin DLLs
- Progress logs to console by default; use `-q` to silence
- `-q`, `--quiet`: Summary only, no per-plugin lines
- `-h`, `--help`: Show help message

### Shell Script Options

- `--build-only`: Only build the scanner, don't run it
- `--clean`: Clean build directory before building
- `--help`: Show help message

### PowerShell Options

- `-BuildOnly`: Only build the scanner, don't run it
- `-Clean`: Clean build directory before building
- `-Help`: Show help message

## Examples

### Basic Scanning

```bash
# Scan and output to console
./vst_scanner /path/to/vst/plugins

# Scan and save to file
./vst_scanner /path/to/vst/plugins -o my_plugins.json
```

### Cumulative Scanning

The scanner supports cumulative scanning, which allows you to build up a comprehensive database of plugins across multiple scans:

```bash
# First scan - creates new cumulative file
./vst_scanner /path/to/vst/plugins -c cumulative_plugins.json

# Second scan - adds new plugins to existing file
./vst_scanner /path/to/other/plugins -c cumulative_plugins.json

# Third scan - continues building the cumulative database
./vst_scanner /path/to/more/plugins -c cumulative_plugins.json
```

**Benefits of cumulative scanning:**
- Build a complete plugin database over time
- Avoid re-scanning the same plugins
- Merge results from multiple directories
- Maintain a single source of truth for all your plugins

**How it works:**
- The scanner reads existing plugins from the cumulative file
- New plugins are added only if they don't already exist (based on file path)
- The final output contains all plugins from previous scans plus new ones
- Duplicate plugins are automatically filtered out

### Scan Common VST Directories

```bash
# Windows
./vst_scanner "C:\Program Files\Common Files\VST3" -o windows_vst3.json
./vst_scanner "C:\Program Files\VSTPlugins" -o windows_vst2.json

# macOS
./vst_scanner "/Library/Audio/Plug-Ins/VST3" -o mac_vst3.json
./vst_scanner "~/Library/Audio/Plug-Ins/VST3" -o mac_user_vst3.json

# Linux
./vst_scanner "/usr/local/lib/vst3" -o linux_vst3.json
./vst_scanner "~/.vst3" -o linux_user_vst3.json
```

### Batch Processing with Cumulative Scanning

```bash
# Create a cumulative database from multiple directories
./vst_scanner "/path/to/dir1" -c all_plugins.json
./vst_scanner "/path/to/dir2" -c all_plugins.json
./vst_scanner "/path/to/dir3" -c all_plugins.json

# Or use a loop for multiple directories
for dir in /path/to/dir1 /path/to/dir2 /path/to/dir3; do
    ./vst_scanner "$dir" -c all_plugins.json
done
```

## Troubleshooting

### Build Issues

See **[INSTALL_DEVELOPMENT.md](INSTALL_DEVELOPMENT.md)** for:

- In-source build errors (`CMakeCache.txt` in repo root)
- `cmake` not recognized (stale PATH)
- Missing compiler (use Developer PowerShell for VS)
- SDK submodule / `vst3sdk/cmake` missing

Quick fixes:

1. **CMake not found**: `winget install Kitware.CMake`, then reopen terminal
2. **In-source builds not allowed**: delete `CMakeCache.txt` and `CMakeFiles\` from repo root; configure only inside `build/` with `..` or `-S`/`-B`
3. **Compiler not found**: Install VS with **Desktop development with C++**; build from **Developer PowerShell for VS**
4. **SDK missing**: `.\scripts\init-sdk.ps1` or `git submodule update --init --recursive`

### Runtime Issues

1. **Permission denied**: Make sure you have read access to the plugin directory
2. **No plugins found**: Check that the directory contains VST3 plugins
3. **Invalid plugins**: Some plugins may be corrupted or incompatible
4. **License / headless-unsafe plugins**: PACE/iLok/license strings → `"missingLicense": true`. Plugins under an `iZotope` folder (e.g. Insight 2) are skipped without loading → `"failed": true`, `"scanSource": "skipped"`. Other plugins load via a hidden subprocess on Windows (3s timeout). Use `--try-license-load` to force load protected bundles. `--no-isolate` loads in the main process (dialogs may appear). Path-only inventory: `--no-factory`

### Platform-Specific Notes

#### Windows
- Requires Visual Studio 2019/2022/2026 (Desktop development with C++)
- Built exe: `build\bin\Release\vst_scanner.exe`
- PowerShell script (`scan_vst.ps1`) auto-detects VS and builds before scan
- Scans `.vst3` bundle folders

#### macOS
- Requires Xcode Command Line Tools
- Supports `.vst3` bundles and `.bundle` files
- May require code signing for some plugins

#### Linux
- Requires GCC 7+ or Clang 7+
- Supports `.vst3` files and `.so` libraries
- May need additional libraries for some plugins

## Development

### Project Structure

```
vst-scanner/
├── vst_scanner.cpp          # Main C++ source
├── CMakeLists.txt           # Build (links sdk_hosting + platform module loader)
├── CMakePresets.json        # Optional VS/Xcode/Ninja presets
├── scripts/init-sdk.sh      # Submodule init (v3.8.0)
├── scan_vst.sh / scan_vst.ps1   # Build + scan wrappers
├── portable_template/           # Launcher templates (source)
├── vst_scanner_portable/        # Distribution output (generated, gitignored)
├── distribute_vst_scanner.ps1
└── vst3sdk/                     # VST3 SDK 3.8.0 (git submodule)
```

### Building from Source

See **Manual Build and Run** above, or:

```powershell
# Windows — script builds then scans
.\scan_vst.ps1 "C:\path\to\vst\plugins" -BuildOnly
```

```bash
# Linux/macOS
./scan_vst.sh /path/to/vst/plugins --build-only
```

Optional portable zip folder for end users (no dev tools): `.\distribute_vst_scanner.ps1`

### Customization

You can modify `vst_scanner.cpp` to:
- Add more plugin information fields
- Change the JSON output format
- Add filtering options
- Implement additional validation

## License

This project uses the VST3 SDK (v3.8.0+), released under the [MIT license](https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt). See `vst3sdk/LICENSE.txt` for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test on multiple platforms
5. Submit a pull request

## Support

For issues and questions:
1. Check the troubleshooting section
2. Review the VST3 SDK documentation
3. Open an issue on GitHub
