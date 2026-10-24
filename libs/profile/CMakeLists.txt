# SPDX-License-Identifier: GPL-3.0-or-later

# What a player keeps between runs: the names, the leaderboards, the settings and the unlocks, and the versioned bytes
# they are saved as. A portable library (standard section 3.1), like libs/sim: it includes no Qt or platform header,
# uses no exceptions, RTTI, iostream or clock, and allocates nothing, so the ESP32-P4 can link it. Where the bytes are
# kept is the business of the platform service. Exceptions and RTTI are switched off for its sources.
add_library(snake_profile
    src/leaderboard.cpp
    src/player_name.cpp
    src/recent_names.cpp
    src/save_data.cpp
    src/unlocks.cpp)
add_library(snake::profile ALIAS snake_profile)

target_include_directories(snake_profile
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(snake_profile PUBLIC cxx_std_23)
target_compile_options(snake_profile
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/GR->"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti>")
target_link_libraries(snake_profile
    PUBLIC snake::sim
    PRIVATE icl::warnings icl::hardening icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
