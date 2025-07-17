#!/bin/bash

# VST Scanner Script
# Scans a directory for VST plugins and outputs information to JSON

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "VST Scanner - Scan for VST plugins and output to JSON"
    echo ""
    echo "Usage: $0 <directory_path> [options]"
    echo ""
    echo "Arguments:"
    echo "  directory_path    Path to scan for VST plugins"
    echo ""
    echo "Options:"
    echo "  -o <output_file.json>     Output to specific file"
    echo "  -c <cumulative_file.json> Append to existing cumulative file"
    echo "  --build-only              Only build the scanner, don't run it"
    echo "  --clean                   Clean build directory before building"
    echo "  --help                    Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 /path/to/vst/plugins"
    echo "  $0 /path/to/vst/plugins -o my_plugins.json"
    echo "  $0 /path/to/vst/plugins -c cumulative_plugins.json"
    echo "  $0 /path/to/vst/plugins --clean"
}

# Parse command line arguments
BUILD_ONLY=false
CLEAN_BUILD=false
DIRECTORY=""
OUTPUT_FILE=""
CUMULATIVE_FILE=""
USE_CUMULATIVE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -o)
            if [[ -n "$2" && "$2" != -* ]]; then
                OUTPUT_FILE="$2"
                shift 2
            else
                print_error "Option -o requires an argument"
                exit 1
            fi
            ;;
        -c)
            if [[ -n "$2" && "$2" != -* ]]; then
                CUMULATIVE_FILE="$2"
                USE_CUMULATIVE=true
                shift 2
            else
                print_error "Option -c requires an argument"
                exit 1
            fi
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        -*)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
        *)
            if [[ -z "$DIRECTORY" ]]; then
                DIRECTORY="$1"
            else
                print_error "Too many arguments"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Check if directory is provided
if [[ -z "$DIRECTORY" ]]; then
    print_error "Directory path is required"
    show_usage
    exit 1
fi

# Check if both -o and -c are used
if [[ -n "$OUTPUT_FILE" && "$USE_CUMULATIVE" == true ]]; then
    print_error "Cannot use both -o and -c options"
    exit 1
fi

# Set default output file if not provided and not using cumulative
if [[ -z "$OUTPUT_FILE" && "$USE_CUMULATIVE" == false ]]; then
    OUTPUT_FILE="vst_scan_$(date +%Y%m%d_%H%M%S).json"
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

print_status "VST Scanner starting..."
print_status "Directory to scan: $DIRECTORY"
print_status "Output file: $OUTPUT_FILE"
print_status "Build directory: $BUILD_DIR"

# Create build directory
if [[ "$CLEAN_BUILD" == true ]]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Check if CMake is available
if ! command -v cmake &> /dev/null; then
    print_error "CMake is not installed or not in PATH"
    exit 1
fi

# Check if compiler is available
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    # Windows with MSYS/Cygwin
    if ! command -v g++ &> /dev/null; then
        print_error "G++ compiler is not installed or not in PATH"
        exit 1
    fi
    CMAKE_GENERATOR="Unix Makefiles"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    if ! command -v clang++ &> /dev/null; then
        print_error "Clang++ compiler is not installed or not in PATH"
        exit 1
    fi
    CMAKE_GENERATOR="Unix Makefiles"
else
    # Linux
    if ! command -v g++ &> /dev/null; then
        print_error "G++ compiler is not installed or not in PATH"
        exit 1
    fi
    CMAKE_GENERATOR="Unix Makefiles"
fi

# Configure and build
print_status "Configuring project with CMake..."
cmake -G "$CMAKE_GENERATOR" -DCMAKE_BUILD_TYPE=Release ..

if [[ $? -ne 0 ]]; then
    print_error "CMake configuration failed"
    exit 1
fi

print_status "Building VST scanner..."
cmake --build . --config Release

if [[ $? -ne 0 ]]; then
    print_error "Build failed"
    exit 1
fi

print_success "Build completed successfully!"

if [[ "$BUILD_ONLY" == true ]]; then
    print_status "Build-only mode: skipping scan"
    exit 0
fi

# Run the scanner
print_status "Running VST scanner..."
SCANNER_ARGS=("$DIRECTORY")

if [[ "$USE_CUMULATIVE" == true ]]; then
    SCANNER_ARGS+=("-c" "$CUMULATIVE_FILE")
    print_status "Using cumulative mode with file: $CUMULATIVE_FILE"
elif [[ -n "$OUTPUT_FILE" ]]; then
    SCANNER_ARGS+=("-o" "$OUTPUT_FILE")
    print_status "Output will be saved to: $OUTPUT_FILE"
fi

if [[ -f "bin/vst_scanner" ]]; then
    ./bin/vst_scanner "${SCANNER_ARGS[@]}"
elif [[ -f "bin/Release/vst_scanner.exe" ]]; then
    ./bin/Release/vst_scanner.exe "${SCANNER_ARGS[@]}"
elif [[ -f "bin/vst_scanner.exe" ]]; then
    ./bin/vst_scanner.exe "${SCANNER_ARGS[@]}"
else
    print_error "Could not find vst_scanner executable"
    print_status "Looking for executable in build directory..."
    find . -name "vst_scanner*" -type f
    exit 1
fi

if [[ $? -eq 0 ]]; then
    print_success "VST scan completed successfully!"
    
    if [[ "$USE_CUMULATIVE" == true ]]; then
        print_status "Results appended to: $CUMULATIVE_FILE"
        OUTPUT_FILE="$CUMULATIVE_FILE"
    else
        print_status "Results saved to: $OUTPUT_FILE"
    fi
    
    # Show summary if jq is available
    if command -v jq &> /dev/null; then
        print_status "Scan summary:"
        jq -r '"Total plugins: " + (.totalPlugins | tostring) + "\nValid plugins: " + (.validPlugins | tostring)' "$OUTPUT_FILE"
    fi
else
    print_error "VST scan failed"
    exit 1
fi 