# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_raster_tests
    test_board_painter.cpp)
target_link_libraries(snake_raster_tests
    PRIVATE snake::raster snake::sim Catch2::Catch2WithMain icl::warnings icl::coverage)

catch_discover_tests(snake_raster_tests PROPERTIES LABELS unit)
