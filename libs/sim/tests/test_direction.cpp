// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/direction.hpp"

#include <catch2/catch_test_macros.hpp>

using snake::sim::Direction;

TEST_CASE("Each direction has one opposite", "[direction]")
{
    CHECK(snake::sim::opposite(Direction::UP) == Direction::DOWN);
    CHECK(snake::sim::opposite(Direction::DOWN) == Direction::UP);
    CHECK(snake::sim::opposite(Direction::LEFT) == Direction::RIGHT);
    CHECK(snake::sim::opposite(Direction::RIGHT) == Direction::LEFT);
}

TEST_CASE("Turning back on itself is a reversal and nothing else is", "[direction]")
{
    constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    for (const Direction from : ALL)
    {
        for (const Direction to : ALL)
        {
            const bool expected = (from == Direction::UP && to == Direction::DOWN) ||
                                  (from == Direction::DOWN && to == Direction::UP) ||
                                  (from == Direction::LEFT && to == Direction::RIGHT) ||
                                  (from == Direction::RIGHT && to == Direction::LEFT);
            CHECK(snake::sim::is_reversal(from, to) == expected);
        }
    }
}
