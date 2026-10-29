# SPDX-License-Identifier: GPL-3.0-or-later

# The ESP32-P4 front end of Snake: an ESP-IDF project, built with `idf.py` and not by the CMake of the rest of the repository
# (see README.md). The rules, the input, the sound and the picture of the board come from the portable libraries in
# libs/, as one component; the panel, the touch screen and LVGL come from Espressif's board support package.
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(snake_esp)
