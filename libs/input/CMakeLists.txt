# SPDX-License-Identifier: GPL-3.0-or-later

# How the player's hands become directions: the tilt processor and its sample source, the swipe recogniser, and the key
# and gamepad maps. A portable library (standard section 3.1), like libs/sim: it includes no Qt or platform header,
# uses no exceptions, RTTI, iostream or clock, and allocates nothing, so the ESP32-P4 can link it. It may use floating
# point, because none of it is part of the simulation. Reading the sensor, the touch screen and the keyboard is the
# business of the platform. Exceptions and RTTI are switched off for its sources.
add_library(snake_input
    src/direction_selector.cpp
    src/gamepad_map.cpp
    src/key_map.cpp
    src/swipe_recogniser.cpp
    src/tilt_processor.cpp)
add_library(snake::input ALIAS snake_input)

target_include_directories(snake_input
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(snake_input PUBLIC cxx_std_23)
target_compile_options(snake_input
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/GR->"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti>")
target_link_libraries(snake_input
    PUBLIC snake::sim
    PRIVATE icl::warnings icl::hardening icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
