cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FroxelFluidsGPU LANGUAGES C CXX)
endif()

# This code uses the WebGPU C API and wgpu-native's device polling extension.
if(NOT TARGET webgpu)
    include(FetchContent)
    set(WEBGPU_BACKEND "WGPU" CACHE STRING "WebGPU implementation")
    set(WEBGPU_BUILD_FROM_SOURCE OFF CACHE BOOL "Build the WebGPU backend from source")
    FetchContent_Declare(webgpu
        GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
        GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab # wgpu-native v24.0.3.1
    )
    FetchContent_MakeAvailable(webgpu)
endif()

add_library(ff_gpu STATIC
    src/gpu/buffer.cpp
    src/gpu/compute.cpp
    src/gpu/context.cpp
    src/gpu/timer.cpp
)
target_compile_features(ff_gpu PUBLIC cxx_std_20)
set_target_properties(ff_gpu PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(ff_gpu PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ff_gpu PUBLIC webgpu)
