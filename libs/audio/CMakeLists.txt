# SPDX-License-Identifier: GPL-3.0-or-later

# The sound of the game: sound effects and music made from square, triangle and noise voices in whole-number arithmetic,
# with the effects and tunes as data and no sample files. A portable library (standard section 3.1), like libs/sim: it
# includes no Qt or platform header, uses no exceptions, RTTI, iostream or clock, and allocates nothing, so the
# ESP32-P4 can link it. Getting the samples to a speaker is the business of the platform. Exceptions and RTTI are
# switched off for its sources.
add_library(snake_audio
    src/synth.cpp
    src/tunes.cpp)
add_library(snake::audio ALIAS snake_audio)

target_include_directories(snake_audio
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(snake_audio PUBLIC cxx_std_23)
target_compile_options(snake_audio
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/EHs-c-;/GR->"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-exceptions;-fno-rtti>")
target_link_libraries(snake_audio
    PRIVATE icl::warnings icl::hardening icl::coverage)

if(SNAKE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
