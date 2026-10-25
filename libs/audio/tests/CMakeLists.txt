# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(snake_audio_tests
    test_note.cpp
    test_synth.cpp
    test_tunes.cpp)
target_link_libraries(snake_audio_tests
    PRIVATE snake::audio snake::sim Catch2::Catch2WithMain icl::warnings icl::coverage)

catch_discover_tests(snake_audio_tests PROPERTIES LABELS unit)
