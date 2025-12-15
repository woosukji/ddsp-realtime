cmake_minimum_required(VERSION 3.20)

project(DDSPUnityPlugin VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Options
# ==============================================================================
option(BUILD_XCFRAMEWORK "Build as XCFramework for iOS/visionOS" OFF)
option(USE_COREML_DELEGATE "Enable CoreML delegate" ON)

# ==============================================================================
# Find ddsp_core
# ==============================================================================
# Assume ddsp_core is built in the parent project
if(NOT TARGET ddsp::core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_BINARY_DIR}/ddsp_core)
endif()

# ==============================================================================
# Unity SDK
# ==============================================================================
set(UNITY_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/unity_sdk")

if(NOT EXISTS "${UNITY_SDK_ROOT}")
    message(FATAL_ERROR "Unity SDK not found at: ${UNITY_SDK_ROOT}")
endif()

# ==============================================================================
# Unity Plugin Source
# ==============================================================================
set(UNITY_PLUGIN_SOURCES
    src/DDSPUnityPlugin.cpp
    src/DDSPUnityPlugin.h
)

# ==============================================================================
# Build Plugin
# ==============================================================================
if(BUILD_XCFRAMEWORK)
    # XCFramework build for iOS/visionOS
    add_library(AudioPluginDDSP MODULE ${UNITY_PLUGIN_SOURCES})
else()
    # Regular build
    add_library(AudioPluginDDSP MODULE ${UNITY_PLUGIN_SOURCES})
endif()

# Link ddsp_core
target_link_libraries(AudioPluginDDSP PRIVATE ddsp::core)

# Include Unity SDK
target_include_directories(AudioPluginDDSP PRIVATE
    ${UNITY_SDK_ROOT}/NativeAudioPlugins/Common
)

# ==============================================================================
# Platform-Specific Settings
# ==============================================================================
if(APPLE)
    if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
        # iOS settings
        set_target_properties(AudioPluginDDSP PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.ddsp.unity.plugin
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer"
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
        )
    elseif(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
        # visionOS settings
        set_target_properties(AudioPluginDDSP PROPERTIES
            FRAMEWORK TRUE
            MACOSX_FRAMEWORK_IDENTIFIER com.ddsp.unity.plugin
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
        )
    else()
        # macOS settings
        set_target_properties(AudioPluginDDSP PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "bundle"
        )
    endif()
endif()

# ==============================================================================
# Installation
# ==============================================================================
install(TARGETS AudioPluginDDSP
    LIBRARY DESTINATION lib
    BUNDLE DESTINATION .
    FRAMEWORK DESTINATION .
)
