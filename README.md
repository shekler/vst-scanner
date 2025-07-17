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

- **CMake** (version 3.25.0 or higher)
- **C++ compiler**:
  - Windows: Visual Studio 2019/2022 or MinGW
  - macOS: Xcode Command Line Tools
  - Linux: GCC 7+ or Clang 7+

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

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
cmake --build . --config Release

# Run the scanner
./bin/vst_scanner /path/to/vst/plugins output.json
```

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

- **Windows**: `.vst3` files and folders
- **macOS**: `.vst3` bundles and `.bundle` files
- **Linux**: `.vst3` files and `.so` libraries

## Command Line Options

The VST scanner supports the following command-line options:

### Basic Options
- `-o <output_file.json>`: Output results to a specific file (default: stdout)
- `-c <cumulative_file.json>`: Append to existing cumulative file
- `-h, --help`: Show help message

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

1. **CMake not found**: Install CMake from https://cmake.org/download/
2. **Compiler not found**: Install appropriate C++ compiler for your platform
3. **VST3 SDK missing**: The SDK is included as a submodule in this project

### Runtime Issues

1. **Permission denied**: Make sure you have read access to the plugin directory
2. **No plugins found**: Check that the directory contains VST3 plugins
3. **Invalid plugins**: Some plugins may be corrupted or incompatible

### Platform-Specific Notes

#### Windows
- Requires Visual Studio 2019/2022 or MinGW
- PowerShell script automatically detects Visual Studio installations
- Supports both `.vst3` files and folders

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
├── vst_scanner.cpp          # Main C++ source code
├── CMakeLists.txt           # CMake build configuration
├── scan_vst.sh              # Bash script for Linux/macOS
├── scan_vst.ps1             # PowerShell script for Windows
├── README.md                # This file
└── vst3sdk/                 # VST3 SDK (submodule)
```

### Building from Source

```bash
# Clone the repository with submodules
git clone --recursive https://github.com/your-repo/vst-scanner.git
cd vst-scanner

# Build using the provided scripts
./scan_vst.sh /path/to/test/plugins --build-only
```

### Customization

You can modify `vst_scanner.cpp` to:
- Add more plugin information fields
- Change the JSON output format
- Add filtering options
- Implement additional validation

## License

This project uses the VST3 SDK which is licensed under the Steinberg VST3 License or GPL v3. See the VST3 SDK license files for details.

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
