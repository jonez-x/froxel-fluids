cmake_minimum_required(VERSION 3.25)

# Integration module: the parent owns the common WebGPU context and dependencies.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FroxelGridModule LANGUAGES CXX)
endif()

if(NOT TARGET ff_gpu OR NOT TARGET glm::glm)
    message(FATAL_ERROR
        "Froxel Grid needs the parent project's ff_gpu and glm::glm targets. "
        "Create the common GPU infrastructure first, then add_subdirectory(froxel_grid). See README.md.")
endif()

foreach(existing IN ITEMS ff_camera ff_froxel_gpu)
    if(TARGET ${existing})
        message(FATAL_ERROR
            "${existing} already exists. This module provides that target; remove the old target definition "
            "and compile the updated camera/volume implementation exactly once. See README.md.")
    endif()
endforeach()

# FloatVolume belongs to ff_froxel_gpu. Prevent duplicate definitions.
get_target_property(shared_gpu ff_gpu ALIASED_TARGET)
if(NOT shared_gpu)
    set(shared_gpu ff_gpu)
endif()
get_target_property(shared_sources ${shared_gpu} SOURCES)
foreach(shared_source IN LISTS shared_sources)
    get_filename_component(shared_name "${shared_source}" NAME)
    if(shared_name STREQUAL "float_volume.cpp")
        message(FATAL_ERROR
            "Remove float_volume.cpp from the common ff_gpu target; this W2 module compiles it.")
    endif()
endforeach()

add_library(ff_camera STATIC
    src/camera/camera.cpp
    src/camera/froxel_grid.cpp
    src/camera/history.cpp
)
target_compile_features(ff_camera PUBLIC cxx_std_20)
target_include_directories(ff_camera PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ff_camera PUBLIC glm::glm)
set_target_properties(ff_camera PROPERTIES CXX_EXTENSIONS OFF)

# Keep this header, directory and namespace separate from the parent's shaders.hpp.
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(shader_header " \n#pragma once\n#include <string_view>\nnamespace ff::froxel_shaders {\n")
foreach(shader IN ITEMS froxel_mapping froxel_density froxel_history)
    set(shader_path "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.wgsl")
    file(READ "${shader_path}" shader_source)
    string(APPEND shader_header "inline constexpr std::string_view ${shader}_wgsl = R\"WGSL(${shader_source})WGSL\";\n")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${shader_path}")
endforeach()
string(APPEND shader_header "}\n")
file(MAKE_DIRECTORY "${generated_dir}")
file(CONFIGURE OUTPUT "${generated_dir}/ff_froxel_shaders.hpp" CONTENT "${shader_header}" @ONLY)

add_library(ff_froxel_gpu STATIC
    src/gpu/float_volume.cpp
    src/gpu/froxel_mapping.cpp
    src/gpu/froxel_density.cpp
    src/gpu/froxel_history.cpp
    src/gpu/froxel_batch.cpp
)
target_compile_features(ff_froxel_gpu PUBLIC cxx_std_20)
target_include_directories(ff_froxel_gpu
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${generated_dir}"
)
target_link_libraries(ff_froxel_gpu PUBLIC ff_camera ff_gpu)
set_target_properties(ff_froxel_gpu PROPERTIES CXX_EXTENSIONS OFF)

foreach(w2_target IN ITEMS ff_camera ff_froxel_gpu)
    if(MSVC)
        target_compile_options(${w2_target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${w2_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
