#!/bin/sh
cd "$(dirname "$0")" || exit 1
mkdir -p ../build
c++ -std=c++20 -Wall -Wextra \
    -I../build/_deps/glm-src -I../build/_deps/doctest-src -I../froxel_grid/src -Isrc \
    ../froxel_grid/src/camera/*.cpp src/transport/transport.cpp tests/transport_tests.cpp \
    -o ../build/transport_tests && ../build/transport_tests "$@"
