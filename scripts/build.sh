#!/bin/bash
#
# Build and test script
# Usage: ./scripts/build.sh [--clean] [--gtk3] [--gtk4] [--minimal]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Default options
CLEAN=0
GTK3=ON
GTK4=OFF

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN=1
            shift
            ;;
        --gtk3)
            GTK3=ON
            GTK4=OFF
            shift
            ;;
        --gtk4)
            GTK3=OFF
            GTK4=ON
            shift
            ;;
        --minimal)
            GTK3=OFF
            GTK4=OFF
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--clean] [--gtk3] [--gtk4] [--minimal]"
            exit 1
            ;;
    esac
done

if [ $CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring..."
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DWITH_GTK3=$GTK3 \
    -DWITH_GTK4=$GTK4

echo "Building..."
cmake --build . --parallel $(nproc)

echo "Running tests..."
ctest --output-on-failure

echo ""
echo "Build successful!"
echo "Install with: sudo cmake --install ."
