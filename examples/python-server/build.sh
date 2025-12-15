#!/bin/bash
set -e

# ==============================================================================
# DDSP Python Bindings Build Script
# ==============================================================================

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../.." && pwd )"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=========================================="
echo "Building DDSP Python Bindings"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo ""

# ==============================================================================
# Options
# ==============================================================================
BUILD_TYPE="${BUILD_TYPE:-Release}"
PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE:-$(which python3)}"

echo "Build type: ${BUILD_TYPE}"
echo "Python: ${PYTHON_EXECUTABLE}"
echo ""

# Check Python version
echo "Python version:"
${PYTHON_EXECUTABLE} --version
echo ""

# ==============================================================================
# Create build directory
# ==============================================================================
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# ==============================================================================
# CMake Configuration
# ==============================================================================
echo "=========================================="
echo "Configuring..."
echo "=========================================="
cmake "${SCRIPT_DIR}" \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
    -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} \
    -DTFLITE_ROOT="${PROJECT_ROOT}/third_party/tflite"

# ==============================================================================
# Build
# ==============================================================================
echo ""
echo "=========================================="
echo "Building..."
echo "=========================================="
cmake --build . --config ${BUILD_TYPE} -j$(sysctl -n hw.ncpu)

# ==============================================================================
# Install to lib/
# ==============================================================================
echo ""
echo "=========================================="
echo "Installing..."
echo "=========================================="
cmake --install .

echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo "Python module installed to: ${SCRIPT_DIR}/lib/"
ls -lh "${SCRIPT_DIR}/lib/" | grep -E '\.so|\.pyd'
echo ""
echo "You can now run the server:"
echo "  cd ${SCRIPT_DIR}"
echo "  python3 server.py"
echo ""
