# SPDX-License-Identifier: GPL-3.0-or-later

# The version is written as constants in a header; code reads them with `if constexpr`
# or as plain values. CMake never defines a macro that changes which code compiles.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/snake/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/snake/version.hpp"
    @ONLY)

add_library(snake_core
    src/game.cpp
    src/game_state.cpp
    src/logger.cpp)
add_library(snake::core ALIAS snake_core)

target_include_directories(snake_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>")
target_compile_features(snake_core PUBLIC cxx_std_23)
target_link_libraries(snake_core
    PRIVATE icl::warnings icl::hardening icl::tracy icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
