// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/version.hpp"

#include <catch2/catch_test_macros.hpp>

#include <format>

TEST_CASE("The version string is the three numbers joined", "[version]")
{
    CHECK(snake::VERSION_STRING ==
          std::format("{}.{}.{}", snake::VERSION_MAJOR, snake::VERSION_MINOR, snake::VERSION_PATCH));
}
