cmake_minimum_required(VERSION 3.20)

project(ddsp_core VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Options
# ==============================================================================
option(DDSP_BUILD_SHARED "Build ddsp_core as shared library" OFF)

# ==============================================================================
# Source Files
# ==============================================================================
set(DDSP_CORE_SOURCES
    src/PredictControlsModel.cpp
    src/HarmonicSynthesizer.cpp
    src/NoiseSynthesizer.cpp
    src/InferencePipeline.cpp
    src/MidiInputProcessor.cpp
)

set(DDSP_CORE_HEADERS
    include/ddsp/DDSPTypes.h
    include/ddsp/InputUtils.h
    include/ddsp/PredictControlsModel.h
    include/ddsp/HarmonicSynthesizer.h
    include/ddsp/NoiseSynthesizer.h
    include/ddsp/InferencePipeline.h
    include/ddsp/MidiInputProcessor.h
)

# ==============================================================================
# Library Target
# ==============================================================================
if(DDSP_BUILD_SHARED)
    add_library(ddsp_core SHARED ${DDSP_CORE_SOURCES} ${DDSP_CORE_HEADERS})
else()
    add_library(ddsp_core STATIC ${DDSP_CORE_SOURCES} ${DDSP_CORE_HEADERS})
endif()

# Add alias for modern CMake usage
add_library(ddsp::core ALIAS ddsp_core)

# ==============================================================================
# Include Directories
# ==============================================================================
target_include_directories(ddsp_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# ==============================================================================
# TensorFlow Lite Configuration
# ==============================================================================
# Users should set TFLITE_ROOT to point to third_party/tflite
if(NOT DEFINED TFLITE_ROOT)
    set(TFLITE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tflite")
    message(STATUS "TFLITE_ROOT not set, using default: ${TFLITE_ROOT}")
endif()

target_include_directories(ddsp_core
    PRIVATE
        ${TFLITE_ROOT}/include
)

# Platform-specific TFLite linking
if(APPLE)
    if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # iOS: Use prebuilt frameworks or static libs
        set(TFLITE_LIB_PATH "${TFLITE_ROOT}/lib/ios")
    else()
        # macOS
        set(TFLITE_LIB_PATH "${TFLITE_ROOT}/lib/macos")
    endif()
elseif(UNIX)
    # Linux
    set(TFLITE_LIB_PATH "${TFLITE_ROOT}/lib/linux")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Find TFLite library
find_library(TFLITE_LIB
    NAMES tensorflowlite_c libtensorflowlite_c tensorflowlite libtensorflowlite
    PATHS ${TFLITE_LIB_PATH}
    NO_DEFAULT_PATH
)

if(TFLITE_LIB)
    message(STATUS "Found TFLite library: ${TFLITE_LIB}")
    target_link_libraries(ddsp_core PRIVATE ${TFLITE_LIB})
else()
    message(WARNING "TFLite library not found in ${TFLITE_LIB_PATH}")
    message(WARNING "Please run: scripts/download_tflite.sh")
endif()

# Apple-specific frameworks
if(APPLE)
    target_link_libraries(ddsp_core PRIVATE
        "-framework Foundation"
        "-framework Accelerate"
    )

    # CoreML delegate for iOS/macOS
    if(USE_COREML_DELEGATE)
        find_library(COREML_DELEGATE_LIB
            NAMES TensorFlowLiteCCoreML coreml_delegate
            PATHS ${TFLITE_LIB_PATH}
            NO_DEFAULT_PATH
        )

        if(COREML_DELEGATE_LIB)
            message(STATUS "Found CoreML delegate: ${COREML_DELEGATE_LIB}")
            target_link_libraries(ddsp_core PRIVATE ${COREML_DELEGATE_LIB})
            target_link_libraries(ddsp_core PRIVATE "-framework CoreML")
        endif()
    endif()
endif()

# ==============================================================================
# JUCE Configuration (Audio Processing)
# ==============================================================================
# JUCE is used for audio utilities (FFT, resampling, etc.)
set(JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/juce" CACHE PATH "Path to JUCE")

if(EXISTS "${JUCE_ROOT}/CMakeLists.txt")
    message(STATUS "Found JUCE at: ${JUCE_ROOT}")
    add_subdirectory(${JUCE_ROOT} ${CMAKE_BINARY_DIR}/juce_build EXCLUDE_FROM_ALL)

    target_link_libraries(ddsp_core
        PUBLIC
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_core
    )
else()
    message(WARNING "JUCE not found at ${JUCE_ROOT}")
    message(WARNING "Please run: git submodule update --init --recursive")
endif()

# ==============================================================================
# Compiler Settings
# ==============================================================================
target_compile_features(ddsp_core PUBLIC cxx_std_17)

target_compile_definitions(ddsp_core PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:Debug>:DEBUG>
)

# Warning flags
if(MSVC)
    target_compile_options(ddsp_core PRIVATE /W4)
else()
    target_compile_options(ddsp_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# Installation
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS ddsp_core
    EXPORT ddsp_coreTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ddsp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets for use by other CMake projects
install(EXPORT ddsp_coreTargets
    FILE ddsp_coreTargets.cmake
    NAMESPACE ddsp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ddsp_core
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ddsp_coreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ddsp_coreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ddsp_core
)
