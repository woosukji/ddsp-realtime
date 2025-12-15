#!/bin/bash
set -e

# ==============================================================================
# DDSP Realtime Dependency Setup Script
# ==============================================================================

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/.." && pwd )"

echo "=========================================="
echo "DDSP Realtime Dependency Setup"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo ""

# ==============================================================================
# 1. Initialize Git Submodules (JUCE)
# ==============================================================================
echo "Step 1: Initializing git submodules..."
cd "${PROJECT_ROOT}"

if [ -f ".gitmodules" ]; then
    git submodule update --init --recursive
    echo "✓ Submodules initialized"
else
    echo "! No .gitmodules file found. Skipping submodule init."
fi
echo ""

# ==============================================================================
# 2. Download TensorFlow Lite Libraries
# ==============================================================================
echo "Step 2: Setting up TensorFlow Lite..."
echo "Please choose your platform:"
echo "  1) macOS (arm64)"
echo "  2) macOS (x86_64)"
echo "  3) iOS"
echo "  4) Linux"
echo "  5) Skip TFLite setup"
read -p "Enter choice [1-5]: " PLATFORM_CHOICE

TFLITE_DIR="${PROJECT_ROOT}/third_party/tflite"

case $PLATFORM_CHOICE in
    1)
        echo "Setting up TFLite for macOS (arm64)..."
        mkdir -p "${TFLITE_DIR}/lib/macos"
        echo "! Please manually download TFLite libraries for macOS arm64 and place them in:"
        echo "  ${TFLITE_DIR}/lib/macos/"
        echo ""
        echo "Download from: https://www.tensorflow.org/lite/guide/build_cmake"
        ;;
    2)
        echo "Setting up TFLite for macOS (x86_64)..."
        mkdir -p "${TFLITE_DIR}/lib/macos"
        echo "! Please manually download TFLite libraries for macOS x86_64 and place them in:"
        echo "  ${TFLITE_DIR}/lib/macos/"
        ;;
    3)
        echo "Setting up TFLite for iOS..."
        mkdir -p "${TFLITE_DIR}/lib/ios"
        echo "! Please manually download TFLite libraries for iOS and place them in:"
        echo "  ${TFLITE_DIR}/lib/ios/"
        ;;
    4)
        echo "Setting up TFLite for Linux..."
        mkdir -p "${TFLITE_DIR}/lib/linux"
        echo "! Please manually download TFLite libraries for Linux and place them in:"
        echo "  ${TFLITE_DIR}/lib/linux/"
        ;;
    5)
        echo "Skipping TFLite setup"
        ;;
    *)
        echo "Invalid choice. Skipping TFLite setup."
        ;;
esac
echo ""

# ==============================================================================
# 3. Check for required tools
# ==============================================================================
echo "Step 3: Checking for required tools..."

# CMake
if command -v cmake &> /dev/null; then
    echo "✓ CMake found: $(cmake --version | head -n1)"
else
    echo "✗ CMake not found. Please install CMake 3.20 or later."
    echo "  macOS: brew install cmake"
    echo "  Linux: sudo apt-get install cmake"
fi

# C++ Compiler
if command -v c++ &> /dev/null; then
    echo "✓ C++ compiler found: $(c++ --version | head -n1)"
else
    echo "✗ C++ compiler not found. Please install a C++17-compatible compiler."
fi

# Python (for Python bindings)
if command -v python3 &> /dev/null; then
    echo "✓ Python3 found: $(python3 --version)"
else
    echo "✗ Python3 not found (optional, needed for Python server example)."
fi

echo ""

# ==============================================================================
# 4. Summary
# ==============================================================================
echo "=========================================="
echo "Setup Complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "  1. Place TFLite libraries in: ${TFLITE_DIR}/lib/<platform>/"
echo "  2. Build the project:"
echo "     mkdir build && cd build"
echo "     cmake .."
echo "     make -j\$(nproc)"
echo ""
echo "For more information, see:"
echo "  - ${PROJECT_ROOT}/README.md"
echo "  - ${PROJECT_ROOT}/docs/BUILD.md"
echo ""
