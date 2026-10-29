# SPDX-License-Identifier: GPL-3.0-or-later

# Draws the board into a rectangle of 16-bit pixels, in flat fills on whole pixels: the picture of the ESP32-P4's panel,
# which has no Qt Quick. A portable library (standard section 3.1): it includes no platform header, uses no exceptions, RTTI,
# iostream or clock, and allocates nothing (the caller owns the pixels), and it is tested on the desktop.
add_library(snake_raster
    src/board_painter.cpp)
add_library(snake::raster ALIAS snake_raster)

target_include_directories(snake_raster
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(snake_raster PUBLIC cxx_std_23)
target_compile_options(snake_raster
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/GR->"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti>")
target_link_libraries(snake_raster
    PUBLIC snake::sim
    PRIVATE icl::warnings icl::hardening icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
