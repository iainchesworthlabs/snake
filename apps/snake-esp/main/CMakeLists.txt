# SPDX-License-Identifier: GPL-3.0-or-later
idf_component_register(
    SRCS "main.cpp" "snake_app.cpp"
    INCLUDE_DIRS "."
    REQUIRES snake_portable)
