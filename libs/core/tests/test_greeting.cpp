// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "snake/greeting.hpp"

TEST_CASE("Greeter greets by name", "[greeting]")
{
    const snake::Greeter greeter("Ada");
    const auto text = greeter.greet();
    CHECK(text.starts_with("Hello, Ada, from "));
    CHECK(text.ends_with("."));
    CHECK(greeter.name() == "Ada");
}

TEST_CASE("Greeter accepts an empty name", "[greeting]")
{
    const snake::Greeter greeter("");
    CHECK(greeter.greet().starts_with("Hello, , from "));
}
