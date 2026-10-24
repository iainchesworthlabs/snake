# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_profile_tests
    test_leaderboard.cpp
    test_player_name.cpp
    test_recent_names.cpp
    test_settings.cpp
    test_unlocks.cpp)
target_link_libraries(snake_profile_tests
    PRIVATE snake::profile Catch2::Catch2WithMain icl::warnings icl::coverage)

catch_discover_tests(snake_profile_tests PROPERTIES LABELS unit)
