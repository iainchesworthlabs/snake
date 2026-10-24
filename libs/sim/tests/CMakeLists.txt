# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_sim_tests
    test_cell.cpp
    test_direction.cpp
    test_fnv1a.cpp
    test_input_buffer.cpp
    test_mode.cpp
    test_occupancy_set.cpp
    test_random.cpp
    test_ring_buffer.cpp
    test_scoring.cpp
    test_session.cpp
    test_simulation.cpp
    test_simulation_properties.cpp
    test_simulation_scoring.cpp
    test_simulation_setup.cpp
    test_snapshot.cpp)
target_link_libraries(snake_sim_tests
    PRIVATE snake::sim Catch2::Catch2WithMain icl::warnings icl::coverage)

catch_discover_tests(snake_sim_tests PROPERTIES LABELS unit)
