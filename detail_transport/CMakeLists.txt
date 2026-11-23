foreach(required IN ITEMS ff_camera glm::glm)
    if(NOT TARGET ${required})
        message(FATAL_ERROR "Detail transport needs the parent project's ${required} target.")
    endif()
endforeach()

function(ff_transport_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(ff_transport STATIC
    src/transport/transport.cpp
)
target_compile_features(ff_transport PUBLIC cxx_std_20)
target_include_directories(ff_transport PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(ff_transport PUBLIC ff_camera glm::glm)
ff_transport_warnings(ff_transport)

if(TARGET doctest::doctest)
    add_executable(transport_tests tests/transport_tests.cpp)
    target_link_libraries(transport_tests PRIVATE ff_transport doctest::doctest)
    ff_transport_warnings(transport_tests)
    add_test(NAME transport_tests COMMAND transport_tests)
endif()
