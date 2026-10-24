# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 4.0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Chooses the vcpkg checkout for this configure. Must run before project().
include(IclVcpkgRoot)
icl_resolve_vcpkg("${CMAKE_CURRENT_SOURCE_DIR}" SNAKE)

project(snake
    VERSION 0.1.0
    DESCRIPTION "Snake game on Qt 6 and C++23"
    HOMEPAGE_URL "https://github.com/iainchesworthlabs/snake"
    LANGUAGES CXX)

set(ICL_PROJECT_PREFIX SNAKE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizers apply to every target, so they are configured first.
include(IclSanitizers)
include(IclWarnings)
include(IclHardening)
include(IclTracy)
include(IclCoverage)
include(IclPlatform)
include(IclQtApp)

option(SNAKE_BUILD_TESTS "Build the tests" ON)
option(SNAKE_BUILD_GUI "Build the Qt application" ON)

if(SNAKE_BUILD_TESTS)
    include(CTest)
    find_package(Catch2 3 CONFIG REQUIRED)
    include(Catch)
endif()

add_subdirectory(libs/sim)
add_subdirectory(libs/input)
add_subdirectory(libs/profile)
add_subdirectory(libs/core)

if(SNAKE_BUILD_GUI)
    add_subdirectory(apps/snake)
endif()
