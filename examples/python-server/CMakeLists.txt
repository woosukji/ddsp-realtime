cmake_minimum_required(VERSION 3.20)

project(DDSPPythonBindings VERSION 1.0.0 LANGUAGES CXX)

# ==============================================================================
# Find Python
# ==============================================================================
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

message(STATUS "Python3 found: ${Python3_EXECUTABLE}")
message(STATUS "Python3 include dirs: ${Python3_INCLUDE_DIRS}")
message(STATUS "Python3 libraries: ${Python3_LIBRARIES}")

# ==============================================================================
# Find ddsp_core
# ==============================================================================
if(NOT TARGET ddsp::core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../core ${CMAKE_BINARY_DIR}/ddsp_core)
endif()

# ==============================================================================
# Fetch pybind11
# ==============================================================================
include(FetchContent)
FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# ==============================================================================
# Create Python Extension Module
# ==============================================================================
pybind11_add_module(ddsp_python bindings/bindings.cpp)

# Link against ddsp_core
target_link_libraries(ddsp_python PRIVATE ddsp::core)

# Ensure headers are found
target_include_directories(ddsp_python PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../core/include
)

# ==============================================================================
# Installation
# ==============================================================================
# Install to lib/ directory for easy importing
install(TARGETS ddsp_python
    LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Print install message
message(STATUS "Python module will be installed to: ${CMAKE_CURRENT_SOURCE_DIR}/lib")
