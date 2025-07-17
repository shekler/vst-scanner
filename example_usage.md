# VST Scanner - Example Usage

This document provides practical examples of how to use the VST scanner.

## Basic Usage

### 1. Scan a Single Directory

```bash
# Linux/macOS
./scan_vst.sh /path/to/vst/plugins

# Windows PowerShell
.\scan_vst.ps1 C:\path\to\vst\plugins

# Windows Batch
scan_vst.bat C:\path\to\vst\plugins
```

### 2. Save Results to a Specific File

```bash
# Linux/macOS
./scan_vst.sh /path/to/vst/plugins my_plugins.json

# Windows PowerShell
.\scan_vst.ps1 C:\path\to\vst\plugins my_plugins.json

# Windows Batch
scan_vst.bat C:\path\to\vst\plugins my_plugins.json
```

## Common VST Plugin Locations

### Windows
```bash
# System VST3 plugins
./scan_vst.sh "C:\Program Files\Common Files\VST3"

# User VST3 plugins
./scan_vst.sh "%USERPROFILE%\AppData\Local\Programs\Common Files\VST3"

# Legacy VST2 plugins
./scan_vst.sh "C:\Program Files\VSTPlugins"
```

### macOS
```bash
# System VST3 plugins
./scan_vst.sh "/Library/Audio/Plug-Ins/VST3"

# User VST3 plugins
./scan_vst.sh "~/Library/Audio/Plug-Ins/VST3"

# System VST2 plugins
./scan_vst.sh "/Library/Audio/Plug-Ins/VST"
```

### Linux
```bash
# System VST3 plugins
./scan_vst.sh "/usr/local/lib/vst3"

# User VST3 plugins
./scan_vst.sh "~/.vst3"

# Alternative locations
./scan_vst.sh "/opt/vst3"
./scan_vst.sh "/usr/lib/vst3"
```

## Advanced Usage

### 1. Clean Build and Scan

```bash
# Linux/macOS
./scan_vst.sh /path/to/vst/plugins --clean

# Windows PowerShell
.\scan_vst.ps1 C:\path\to\vst\plugins -Clean

# Windows Batch
scan_vst.bat C:\path\to\vst\plugins --clean
```

### 2. Build Only (Don't Run Scan)

```bash
# Linux/macOS
./scan_vst.sh /path/to/vst/plugins --build-only

# Windows PowerShell
.\scan_vst.ps1 C:\path\to\vst\plugins -BuildOnly

# Windows Batch
scan_vst.bat C:\path\to\vst\plugins --build-only
```

### 3. Batch Processing Multiple Directories

```bash
#!/bin/bash
# scan_multiple.sh

directories=(
    "/Library/Audio/Plug-Ins/VST3"
    "~/Library/Audio/Plug-Ins/VST3"
    "/usr/local/lib/vst3"
)

for dir in "${directories[@]}"; do
    if [ -d "$dir" ]; then
        echo "Scanning: $dir"
        ./scan_vst.sh "$dir" "scan_$(basename "$dir").json"
    else
        echo "Directory not found: $dir"
    fi
done
```

### 4. PowerShell Batch Processing

```powershell
# scan_multiple.ps1

$directories = @(
    "C:\Program Files\Common Files\VST3",
    "$env:USERPROFILE\AppData\Local\Programs\Common Files\VST3",
    "C:\Program Files\VSTPlugins"
)

foreach ($dir in $directories) {
    if (Test-Path $dir -PathType Container) {
        Write-Host "Scanning: $dir"
        $outputFile = "scan_$(Split-Path $dir -Leaf).json"
        .\scan_vst.ps1 $dir $outputFile
    } else {
        Write-Host "Directory not found: $dir"
    }
}
```

## Processing the JSON Output

### 1. Using jq (Linux/macOS)

```bash
# Count total plugins
jq '.totalPlugins' scan_results.json

# Count valid plugins
jq '.validPlugins' scan_results.json

# List all plugin names
jq -r '.plugins[] | select(.isValid) | .name' scan_results.json

# List plugins by vendor
jq -r '.plugins[] | select(.isValid) | "\(.name) - \(.vendor)"' scan_results.json

# Find plugins with specific category
jq -r '.plugins[] | select(.isValid and (.category == "Fx")) | .name' scan_results.json

# Create a summary report
jq -r '
  "VST Scanner Report",
  "==================",
  "Total plugins found: \(.totalPlugins)",
  "Valid plugins: \(.validPlugins)",
  "Invalid plugins: \(.totalPlugins - .validPlugins)",
  "",
  "Plugins by vendor:",
  (.plugins | group_by(.vendor) | .[] | "  \(.[0].vendor): \(length) plugins")
' scan_results.json
```

### 2. Using PowerShell (Windows)

```powershell
# Load JSON data
$data = Get-Content scan_results.json | ConvertFrom-Json

# Count total plugins
Write-Host "Total plugins: $($data.totalPlugins)"

# Count valid plugins
Write-Host "Valid plugins: $($data.validPlugins)"

# List all plugin names
$data.plugins | Where-Object { $_.isValid } | ForEach-Object { $_.name }

# List plugins by vendor
$data.plugins | Where-Object { $_.isValid } | ForEach-Object { "$($_.name) - $($_.vendor)" }

# Find plugins with specific category
$data.plugins | Where-Object { $_.isValid -and $_.category -eq "Fx" } | ForEach-Object { $_.name }

# Create a summary report
Write-Host "VST Scanner Report"
Write-Host "=================="
Write-Host "Total plugins found: $($data.totalPlugins)"
Write-Host "Valid plugins: $($data.validPlugins)"
Write-Host "Invalid plugins: $($data.totalPlugins - $data.validPlugins)"
Write-Host ""

Write-Host "Plugins by vendor:"
$data.plugins | Where-Object { $_.isValid } | Group-Object vendor | ForEach-Object {
    Write-Host "  $($_.Name): $($_.Count) plugins"
}
```

### 3. Using Python

```python
import json
import sys

# Load JSON data
with open('scan_results.json', 'r') as f:
    data = json.load(f)

# Basic statistics
print(f"Total plugins: {data['totalPlugins']}")
print(f"Valid plugins: {data['validPlugins']}")
print(f"Invalid plugins: {data['totalPlugins'] - data['validPlugins']}")

# List all valid plugins
valid_plugins = [p for p in data['plugins'] if p['isValid']]
print(f"\nValid plugins ({len(valid_plugins)}):")
for plugin in valid_plugins:
    print(f"  - {plugin['name']} ({plugin['vendor']})")

# Group by vendor
vendors = {}
for plugin in valid_plugins:
    vendor = plugin['vendor']
    if vendor not in vendors:
        vendors[vendor] = []
    vendors[vendor].append(plugin['name'])

print(f"\nPlugins by vendor:")
for vendor, plugins in vendors.items():
    print(f"  {vendor}: {len(plugins)} plugins")
    for plugin in plugins:
        print(f"    - {plugin}")
```

## Example Output

Here's what the JSON output looks like:

```json
{
  "scanTime": "1703123456789",
  "totalPlugins": 3,
  "validPlugins": 2,
  "plugins": [
    {
      "path": "/Library/Audio/Plug-Ins/VST3/Example Plugin.vst3",
      "isValid": true,
      "name": "Example Plugin",
      "vendor": "Example Company",
      "version": "1.0.0",
      "category": "Fx",
      "cid": "12345678-1234-1234-1234-123456789012",
      "sdkVersion": "VST 3.7.0",
      "cardinality": 1,
      "flags": 0,
      "subCategories": ["Fx", "Distortion"]
    },
    {
      "path": "/Library/Audio/Plug-Ins/VST3/Another Plugin.vst3",
      "isValid": true,
      "name": "Another Plugin",
      "vendor": "Another Company",
      "version": "2.1.0",
      "category": "Synth",
      "cid": "87654321-4321-4321-4321-210987654321",
      "sdkVersion": "VST 3.7.0",
      "cardinality": 1,
      "flags": 0,
      "subCategories": ["Synth", "Analog"]
    },
    {
      "path": "/Library/Audio/Plug-Ins/VST3/Corrupted Plugin.vst3",
      "isValid": false,
      "error": "Failed to load plugin: Invalid format"
    }
  ]
}
```

## Troubleshooting Examples

### 1. No Plugins Found

```bash
# Check if the directory exists and contains VST files
ls -la /path/to/vst/plugins

# Look for VST3 files specifically
find /path/to/vst/plugins -name "*.vst3" -type f

# Check file permissions
ls -la /path/to/vst/plugins/*.vst3
```

### 2. Build Errors

```bash
# Clean build and try again
./scan_vst.sh /path/to/vst/plugins --clean

# Check CMake version
cmake --version

# Check compiler
g++ --version  # Linux/macOS
cl --version   # Windows
```

### 3. Permission Issues

```bash
# Check file permissions
ls -la /path/to/vst/plugins

# Fix permissions if needed
chmod 644 /path/to/vst/plugins/*.vst3

# Run with elevated privileges (if necessary)
sudo ./scan_vst.sh /path/to/vst/plugins
``` 