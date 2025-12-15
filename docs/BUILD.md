# Build Guide

Comprehensive build instructions for DDSP Realtime on various platforms.

## Table of Contents

- [Prerequisites](#prerequisites)
- [Dependency Setup](#dependency-setup)
- [Building the Core Library](#building-the-core-library)
- [Building Examples](#building-examples)
- [Platform-Specific Instructions](#platform-specific-instructions)
- [Troubleshooting](#troubleshooting)

## Prerequisites

### All Platforms

- **CMake** 3.20 or later
- **Git** (for submodules)
- **C++17** compatible compiler

### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install CMake
brew install cmake
```

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip
```

### Windows

- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ tools
- Install [CMake](https://cmake.org/download/)
- Install [Git for Windows](https://git-scm.com/download/win)

## Dependency Setup

### 1. Clone Repository

```bash
git clone https://github.com/yourusername/ddsp-realtime.git
cd ddsp-realtime
```

### 2. Initialize Submodules

JUCE is included as a Git submodule:

```bash
git submodule update --init --recursive
```

### 3. Setup TensorFlow Lite

TFLite libraries are **not** included in the repository. You have two options:

#### Option A: Use Prebuilt Libraries (Recommended)

1. Download prebuilt TFLite from [TensorFlow Lite releases](https://www.tensorflow.org/lite/guide/build_cmake)
2. Extract to `third_party/tflite/lib/<platform>/`

Directory structure:
```
third_party/tflite/
├── include/
│   └── tensorflow/lite/  # Headers
└── lib/
    ├── macos/
    │   ├── libtensorflowlite.a
    │   └── libtensorflowlite_c.dylib
    ├── ios/
    │   └── TensorFlowLiteC.framework
    └── linux/
        └── libtensorflowlite.so
```

#### Option B: Build from Source

```bash
# Clone TensorFlow repo
git clone https://github.com/tensorflow/tensorflow.git
cd tensorflow

# Build TFLite
mkdir build && cd build
cmake ../tensorflow/lite -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Copy outputs to ddsp-realtime/third_party/tflite/
```

### 4. Run Setup Script

```bash
./scripts/setup_deps.sh
```

This script will:
- Initialize Git submodules
- Guide you through TFLite setup
- Verify required tools

## Building the Core Library

### Quick Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Options

#### Core Library Only

```bash
cmake .. -DBUILD_CORE_ONLY=ON
make ddsp_core -j$(nproc)
```

#### With Examples

```bash
cmake .. \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_UNITY_PLUGIN=ON \
    -DBUILD_PYTHON_BINDINGS=ON
make -j$(nproc)
```

#### Debug Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

#### With CoreML Acceleration (macOS/iOS)

```bash
cmake .. -DUSE_COREML_DELEGATE=ON
make -j$(nproc)
```

### Install

```bash
# Install to system directories
sudo make install

# Or specify custom prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
make install
```

Installed files:
```
/usr/local/
├── lib/
│   └── libddsp_core.a
├── include/
│   └── ddsp/
│       ├── InferencePipeline.h
│       └── ...
└── share/
    └── ddsp_realtime/
        └── models/
            └── Violin.tflite
```

## Building Examples

### Unity Plugin

```bash
cd examples/unity-plugin
./build.sh

# For iOS
PLATFORM=ios ./build.sh

# For visionOS
PLATFORM=visionos ./build.sh
```

Output:
- **macOS**: `build/AudioPluginDDSP.bundle`
- **iOS/visionOS**: `build/AudioPluginDDSP.framework`

### Python Bindings

```bash
cd examples/python-server

# Install Python dependencies
pip3 install -r requirements.txt

# Build Python module
./build.sh
```

Output: `lib/ddsp_python.so` (or `.pyd` on Windows)

Test the module:
```bash
python3 -c "import sys; sys.path.append('lib'); import ddsp_python; print('Success!')"
```

## Platform-Specific Instructions

### macOS

#### Apple Silicon (arm64)

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DUSE_COREML_DELEGATE=ON
make -j$(sysctl -n hw.ncpu)
```

#### Intel (x86_64)

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
make -j$(sysctl -n hw.ncpu)
```

#### Universal Binary (both architectures)

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
make -j$(sysctl -n hw.ncpu)
```

### iOS

```bash
cmake .. \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_COREML_DELEGATE=ON \
    -DBUILD_UNITY_PLUGIN=ON
make -j$(sysctl -n hw.ncpu)
```

### Linux

#### Standard Build

```bash
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
```

#### Cross-compile for ARM

```bash
cmake .. \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
make -j$(nproc)
```

### Windows (MSVC)

```bash
# Open Visual Studio Developer Command Prompt
mkdir build && cd build

cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

## Troubleshooting

### TFLite Not Found

**Error**:
```
CMake Warning: TFLite library not found in third_party/tflite/lib/macos
```

**Solution**:
1. Ensure TFLite libraries are in the correct platform directory
2. Check library naming: `libtensorflowlite_c.dylib` (macOS), `.so` (Linux), `.dll` (Windows)
3. Set `TFLITE_ROOT` explicitly:
   ```bash
   cmake .. -DTFLITE_ROOT=/path/to/tflite
   ```

### JUCE Not Found

**Error**:
```
CMake Warning: JUCE not found at third_party/juce
```

**Solution**:
```bash
git submodule update --init --recursive
```

### Python Module Import Error

**Error**:
```python
ImportError: ddsp_python.so: cannot open shared object file
```

**Solution**:
1. Ensure module is in Python path:
   ```bash
   export PYTHONPATH=/path/to/examples/python-server/lib:$PYTHONPATH
   ```
2. Check library dependencies:
   ```bash
   ldd lib/ddsp_python.so  # Linux
   otool -L lib/ddsp_python.so  # macOS
   ```

### CoreML Delegate Crash (macOS/iOS)

**Symptom**: Application crashes when loading model with CoreML

**Solution**:
1. Ensure model is compatible with CoreML
2. Try disabling CoreML:
   ```bash
   cmake .. -DUSE_COREML_DELEGATE=OFF
   ```
3. Check model conversion:
   ```python
   # Use TFLite's official conversion tools
   converter = tf.lite.TFLiteConverter.from_saved_model(model_path)
   converter.target_spec.supported_ops = [
       tf.lite.OpsSet.TFLITE_BUILTINS,
       tf.lite.OpsSet.SELECT_TF_OPS
   ]
   tflite_model = converter.convert()
   ```

### Unity Plugin Not Loading

**Symptom**: Unity doesn't recognize the plugin

**Solution**:
1. Verify plugin location:
   - **macOS**: `Assets/Plugins/macOS/AudioPluginDDSP.bundle`
   - **iOS**: `Assets/Plugins/iOS/AudioPluginDDSP.framework`
2. Check Unity plugin settings (Inspector → Platform Settings)
3. Enable "Load on startup" in plugin import settings
4. Verify entrypoint:
   ```cpp
   extern "C" UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(...)
   ```

### Linker Errors

**Error**:
```
undefined reference to TfLiteInterpreterCreate
```

**Solution**:
1. Ensure TFLite library is linked correctly in CMakeLists.txt
2. Check library path is correct:
   ```bash
   find third_party/tflite -name "*.a" -o -name "*.so" -o -name "*.dylib"
   ```
3. Rebuild with verbose output:
   ```bash
   make VERBOSE=1
   ```

## Advanced Configuration

### Custom TFLite Location

```bash
cmake .. -DTFLITE_ROOT=/custom/path/to/tflite
```

### Custom JUCE Location

```bash
cmake .. -DJUCE_ROOT=/custom/path/to/juce
```

### Static vs Shared Library

```bash
# Build as shared library
cmake .. -DDDSP_BUILD_SHARED=ON

# Build as static library (default)
cmake .. -DDDSP_BUILD_SHARED=OFF
```

### Compiler Optimization Flags

```bash
# Maximum optimization
cmake .. -DCMAKE_CXX_FLAGS="-O3 -march=native"

# With debug symbols
cmake .. -DCMAKE_CXX_FLAGS="-O2 -g"
```

## Next Steps

- [API Reference](API.md) - Learn the core API
- [Examples Guide](EXAMPLES.md) - Run the example applications
- [Architecture](ARCHITECTURE.md) - Understand the system design
