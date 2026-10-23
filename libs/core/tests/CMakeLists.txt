# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_core_tests
    test_game.cpp
    test_greeting.cpp
    test_logger.cpp
    test_parse.cpp
    test_platform.cpp
    test_timing_loop.cpp
    test_window_settings.cpp)
target_link_libraries(snake_core_tests
    PRIVATE snake::core Catch2::Catch2WithMain icl::warnings icl::coverage)

# Every Catch2 case becomes a CTest test carrying the `unit` label (the labels are
# unit, integration, system, ui, perf and fuzz).
catch_discover_tests(snake_core_tests PROPERTIES LABELS unit)
