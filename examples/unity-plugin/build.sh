#!/bin/bash
set -e

# ==============================================================================
# DDSP Unity Plugin Build Script
# ==============================================================================

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "${SCRIPT_DIR}/../.." && pwd )"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=========================================="
echo "Building DDSP Unity Plugin"
echo "=========================================="
echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo ""

# ==============================================================================
# Options
# ==============================================================================
BUILD_TYPE="${BUILD_TYPE:-Release}"
PLATFORM="${PLATFORM:-macos}"  # Options: macos, ios, visionos
USE_COREML="${USE_COREML:-ON}"

echo "Build type: ${BUILD_TYPE}"
echo "Platform: ${PLATFORM}"
echo "CoreML delegate: ${USE_COREML}"
echo ""

# ==============================================================================
# Create build directory
# ==============================================================================
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# ==============================================================================
# Platform-specific CMake configuration
# ==============================================================================
if [ "${PLATFORM}" == "macos" ]; then
    cmake "${SCRIPT_DIR}" \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DUSE_COREML_DELEGATE=${USE_COREML} \
        -DTFLITE_ROOT="${PROJECT_ROOT}/third_party/tflite"

elif [ "${PLATFORM}" == "ios" ]; then
    cmake "${SCRIPT_DIR}" \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
        -DBUILD_XCFRAMEWORK=ON \
        -DUSE_COREML_DELEGATE=${USE_COREML} \
        -DTFLITE_ROOT="${PROJECT_ROOT}/third_party/tflite"

elif [ "${PLATFORM}" == "visionos" ]; then
    cmake "${SCRIPT_DIR}" \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_SYSTEM_NAME=visionOS \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=1.0 \
        -DBUILD_XCFRAMEWORK=ON \
        -DUSE_COREML_DELEGATE=${USE_COREML} \
        -DTFLITE_ROOT="${PROJECT_ROOT}/third_party/tflite"
fi

# ==============================================================================
# Build
# ==============================================================================
echo ""
echo "=========================================="
echo "Building..."
echo "=========================================="
cmake --build . --config ${BUILD_TYPE} -j$(sysctl -n hw.ncpu)

echo ""
echo "=========================================="
echo "Build Complete!"
echo "=========================================="
echo "Output: ${BUILD_DIR}"
ls -lh "${BUILD_DIR}" | grep -E '\.bundle|\.framework|\.so|\.dylib'
echo ""
