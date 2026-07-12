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
    include_directories(${ONNXRUNTIME_DIR}/include)
    target_link_directories(GestureCore PRIVATE ${ONNXRUNTIME_DIR}/lib)
    target_link_libraries(GestureCore PRIVATE onnxruntime.lib)
    target_compile_definitions(GestureCore PRIVATE WITH_ONNXRUNTIME)
else()
    message(WARNING "ONNXRUNTIME_DIR not specified. Building with dummy inference engine. Use -DONNXRUNTIME_DIR to enable ONNX Runtime.")
endif()
