# SPDX-License-Identifier: GPL-3.0-or-later

# The rules of the game. A portable library (standard section 3.1): it includes no Qt or platform header, uses no
# exceptions, RTTI, iostream or clock, and allocates nothing after construction, so the same code runs on the
# desktop, on phones and on the ESP32-P4. Exceptions and RTTI are switched off for its sources.
add_library(snake_sim
    src/input_buffer.cpp
    src/random.cpp
    src/simulation.cpp)
add_library(snake::sim ALIAS snake_sim)

target_include_directories(snake_sim
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(snake_sim PUBLIC cxx_std_23)
target_compile_options(snake_sim
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/GR->"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti>")
target_link_libraries(snake_sim
    PRIVATE icl::warnings icl::hardening icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
