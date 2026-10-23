// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/window_settings.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("The main window keeps the size and multisampling of the old code", "[window]")
{
    CHECK(snake::MAIN_WINDOW.width == 1024);
    CHECK(snake::MAIN_WINDOW.height == 768);
    CHECK(snake::MAIN_WINDOW.multisample_count == 4);
}
