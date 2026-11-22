cmake_minimum_required(VERSION 3.25)

# Froxel Fluids: parent project. Fetches the shared dependencies, provides the common targets the
# modules expect (glm::glm, doctest::doctest, ff_gpu) and adds the modules in dependency order:
#   world_solver/          W1  ff_world        coarse world grid, Stable Fluids (CPU)
#   froxel_grid/           W2  ff_camera, ff_froxel_gpu
#   turbulence_synthesis/  W5  ff_froxel_link, ff_detail, ff_detail_gpu
#   app/                       the integrated program
project(FroxelFluids LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/dependencies.cmake)

enable_testing()

add_subdirectory(world_solver)

# froxel_grid and turbulence_synthesis need the shared WebGPU target ff_gpu (gpu/, see gpu/README.md).
add_subdirectory(gpu)

if(TARGET ff_gpu)
    add_subdirectory(froxel_grid)
    add_subdirectory(turbulence_synthesis)
    # app/ only has a README so far; add it once it has its own CMakeLists.txt.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app/CMakeLists.txt")
        add_subdirectory(app)
    endif()
else()
    message(WARNING "ff_gpu is missing (see gpu/README.md): skipping froxel_grid, turbulence_synthesis and app.")
endif()
