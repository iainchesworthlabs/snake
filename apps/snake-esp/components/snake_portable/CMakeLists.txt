# SPDX-License-Identifier: GPL-3.0-or-later

# The portable libraries of Snake (libs/sim, libs/input, libs/audio, libs/profile, libs/raster) as one ESP-IDF component. They
# are the same sources that the desktop and the phones build, compiled here with the project's warnings as errors, with
# exceptions and RTTI off (which is ESP-IDF's default).
get_filename_component(SNAKE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

set(snake_libraries sim input audio profile raster)
set(snake_sources)
set(snake_includes)
foreach(library IN LISTS snake_libraries)
    file(GLOB library_sources "${SNAKE_ROOT}/libs/${library}/src/*.cpp")
    list(APPEND snake_sources ${library_sources})
    list(APPEND snake_includes "${SNAKE_ROOT}/libs/${library}/include")
endforeach()

idf_component_register(SRCS ${snake_sources} INCLUDE_DIRS ${snake_includes})

# libs/input keeps one header of its own next to its sources.
target_include_directories(${COMPONENT_LIB} PRIVATE "${SNAKE_ROOT}/libs/input/src")
# C++23, as the standard has it (ESP-IDF's default is a newer one), and the project's warnings, on the libraries only.
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++23>
    -Wall -Wextra -Wpedantic -Werror -Wshadow -Wcast-align -Wunused -Wconversion -Wsign-conversion
    -Wnull-dereference -Wdouble-promotion -Wimplicit-fallthrough -Wformat=2
    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor -Woverloaded-virtual -Wold-style-cast>)
