// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "snake/platform.hpp"

TEST_CASE("The platform layer reports a host name", "[platform]")
{
    CHECK_FALSE(snake::platform::host_name().empty());
}
