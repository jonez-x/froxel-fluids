# World solver (W1): coarse world grid, Stable Fluids (Stam 1999) on the CPU.
# C++ port of the former Rust CPU prototype (git history). Targets created here:
#   ff_world      the solver (Field, Sim)
#   world_demo    front view summing density along z (option FF_WORLD_BUILD_APPS, needs raylib)
#   world_tests   doctest tests (needs doctest::doctest)
option(FF_WORLD_BUILD_APPS "Build the world solver demo" ON)

function(ff_world_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(ff_world STATIC
    src/world/field.cpp
    src/world/sim.cpp
)
target_compile_features(ff_world PUBLIC cxx_std_20)
target_include_directories(ff_world PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
ff_world_warnings(ff_world)

if(FF_WORLD_BUILD_APPS AND TARGET raylib)
    add_executable(world_demo
        apps/world_demo/main.cpp
        apps/world_demo/render.cpp
    )
    target_link_libraries(world_demo PRIVATE ff_world raylib)
    ff_world_warnings(world_demo)
endif()

if(TARGET doctest::doctest)
    add_executable(world_tests tests/world_tests.cpp)
    target_link_libraries(world_tests PRIVATE ff_world doctest::doctest)
    ff_world_warnings(world_tests)
    add_test(NAME world_tests COMMAND world_tests)
endif()
