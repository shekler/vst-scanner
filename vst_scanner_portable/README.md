# VST Scanner - Portable Package

This is a portable version of the VST Scanner that can be used without installing CMake or development tools.

## Usage

### Windows Batch File
`
scan_vst_simple.bat <directory_path> [output_file.json]
`

### PowerShell
`
.\scan_vst_simple.ps1 <directory_path> [output_file.json]
`

## Examples

Scan a directory and save to default file:
`
scan_vst_simple.bat "C:\Program Files\VSTPlugins"
`

Scan a directory and save to specific file:
`
scan_vst_simple.bat "C:\Program Files\VSTPlugins" my_plugins.json
`

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

- st_scanner.exe - The main scanner executable
- scan_vst_simple.bat - Windows batch launcher
- scan_vst_simple.ps1 - PowerShell launcher
- README.md - This file
