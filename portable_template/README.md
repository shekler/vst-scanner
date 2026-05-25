# VST Scanner - Portable Package

Run without CMake or Visual Studio. Requires Windows 10+ x64.

## Usage

**Batch:**
```
scan_vst_simple.bat <directory_path> [output_file.json]
```

**PowerShell:**
```
.\scan_vst_simple.ps1 <directory_path> [output_file.json]
```

**Direct:**
```
vst_scanner.exe <directory_path> -o output.json
vst_scanner.exe <directory_path> -q -j 4 -o output.json
```

## Examples

```
scan_vst_simple.bat "C:\Program Files\Common Files\VST3"
scan_vst_simple.bat "H:\VSTPlugins\My Plugins" my_scan.json
```

## Output

JSON file with plugin paths, names, vendors, versions, categories, and validation status.

## Troubleshooting

- Run as administrator if scanning system folders
- Ensure the plugin directory exists and is readable
- Some plugins may fail to load (listed with `"isValid": false`)
