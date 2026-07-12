cmake_minimum_required(VERSION 3.20)
project(GestureCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Windows Libraries (Media Foundation, Shared Memory, Device Info)
link_libraries(
    mf
    mfplat
    mfreadwrite
    mfuuid
    ole32
    user32
    setupapi
)

# Header directories
include_directories(include)

# Source files
file(GLOB_RECURSE SRC_FILES
    src/*.cpp
    include/*.h
)

add_executable(GestureCore ${SRC_FILES})

# Compiler Definitions & Flags for Windows
target_compile_definitions(GestureCore PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# ONNX Runtime placeholder configuration
# Usage: cmake -DONNXRUNTIME_DIR="C:/path/to/onnxruntime" ..
if(ONNXRUNTIME_DIR)
    message(STATUS "ONNX Runtime dir defined: ${ONNXRUNTIME_DIR}")
    if(EXISTS "${ONNXRUNTIME_DIR}/build/native/include")
        include_directories(${ONNXRUNTIME_DIR}/build/native/include)
    else()
        include_directories(${ONNXRUNTIME_DIR}/include)
    endif()

    if(EXISTS "${ONNXRUNTIME_DIR}/runtimes/win-x64/native")
        target_link_directories(GestureCore PRIVATE ${ONNXRUNTIME_DIR}/runtimes/win-x64/native)
    else()
        target_link_directories(GestureCore PRIVATE ${ONNXRUNTIME_DIR}/lib)
    endif()

    target_link_libraries(GestureCore PRIVATE onnxruntime.lib)
    target_compile_definitions(GestureCore PRIVATE WITH_ONNXRUNTIME)

    # Check if DirectML provider is available in the ONNX Runtime directories
    find_path(DML_HEADER NAMES dml_provider_factory.h HINTS ${ONNXRUNTIME_DIR}/include ${ONNXRUNTIME_DIR}/build/native/include)
    if(DML_HEADER)
        message(STATUS "DirectML header found, enabling DirectML Execution Provider.")
        target_compile_definitions(GestureCore PRIVATE WITH_DIRECTML)
    else()
        message(STATUS "DirectML header NOT found. Building ONNX Runtime for CPU execution provider only.")
    endif()
else()
    message(WARNING "ONNXRUNTIME_DIR not specified. Building with dummy inference engine. Use -DONNXRUNTIME_DIR to enable ONNX Runtime.")
endif()
