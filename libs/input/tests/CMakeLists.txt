# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_input_tests
    test_direction_selector.cpp
    test_gamepad_map.cpp
    test_key_map.cpp
    test_swipe_recogniser.cpp
    test_tilt_processor.cpp
    test_trace_source.cpp)
target_link_libraries(snake_input_tests
    PRIVATE snake::input Catch2::Catch2WithMain icl::warnings icl::coverage)

catch_discover_tests(snake_input_tests PROPERTIES LABELS unit)
