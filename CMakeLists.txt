cmake_minimum_required(VERSION 3.20)

project(ddsp_realtime VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Project Options
# ==============================================================================
option(BUILD_CORE_ONLY "Build only the core library" OFF)
option(BUILD_EXAMPLES "Build example applications" ON)
option(BUILD_UNITY_PLUGIN "Build Unity plugin example" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings example" ON)
option(USE_COREML_DELEGATE "Enable CoreML delegate (Apple platforms)" ON)

# ==============================================================================
# C++ Standard
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Build Configuration
# ==============================================================================
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "========================================")
message(STATUS "DDSP Realtime Build Configuration")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build core only: ${BUILD_CORE_ONLY}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "  - Unity plugin: ${BUILD_UNITY_PLUGIN}")
message(STATUS "  - Python bindings: ${BUILD_PYTHON_BINDINGS}")
message(STATUS "Use CoreML delegate: ${USE_COREML_DELEGATE}")
message(STATUS "========================================")

# ==============================================================================
# Core Library (Always Built)
# ==============================================================================
add_subdirectory(core)

# ==============================================================================
# Examples (Optional)
# ==============================================================================
if(BUILD_EXAMPLES AND NOT BUILD_CORE_ONLY)
    if(BUILD_UNITY_PLUGIN)
        message(STATUS "Adding Unity plugin example")
        add_subdirectory(examples/unity-plugin)
    endif()

    if(BUILD_PYTHON_BINDINGS)
        message(STATUS "Adding Python bindings example")
        add_subdirectory(examples/python-server)
    endif()
endif()

# ==============================================================================
# Installation
# ==============================================================================
include(GNUInstallDirs)

# Install models
install(DIRECTORY models/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/ddsp_realtime/models
    FILES_MATCHING PATTERN "*.tflite"
)

# Install documentation
install(FILES README.md LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
