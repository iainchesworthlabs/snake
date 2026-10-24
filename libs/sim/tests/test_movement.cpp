// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/movement.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace snake::sim;

TEST_CASE("A step moves one cell in the direction", "[movement]")
{
    CHECK(step({5, 5}, Direction::UP, EdgeRule::WALLS) == Cell{5, 4});
    CHECK(step({5, 5}, Direction::DOWN, EdgeRule::WALLS) == Cell{5, 6});
    CHECK(step({5, 5}, Direction::LEFT, EdgeRule::WALLS) == Cell{4, 5});
    CHECK(step({5, 5}, Direction::RIGHT, EdgeRule::WALLS) == Cell{6, 5});
}

TEST_CASE("With walls a step off the board gives nothing", "[movement]")
{
    CHECK_FALSE(step({5, 0}, Direction::UP, EdgeRule::WALLS).has_value());
    CHECK_FALSE(step({5, 19}, Direction::DOWN, EdgeRule::WALLS).has_value());
    CHECK_FALSE(step({0, 5}, Direction::LEFT, EdgeRule::WALLS).has_value());
    CHECK_FALSE(step({19, 5}, Direction::RIGHT, EdgeRule::WALLS).has_value());
}

TEST_CASE("With wrapping a step off one edge lands on the opposite edge", "[movement]")
{
    CHECK(step({5, 0}, Direction::UP, EdgeRule::WRAP) == Cell{5, 19});
    CHECK(step({5, 19}, Direction::DOWN, EdgeRule::WRAP) == Cell{5, 0});
    CHECK(step({0, 5}, Direction::LEFT, EdgeRule::WRAP) == Cell{19, 5});
    CHECK(step({19, 5}, Direction::RIGHT, EdgeRule::WRAP) == Cell{0, 5});
}

TEST_CASE("A step that stays on the board is the same with either edge rule", "[movement]")
{
    CHECK(step({0, 0}, Direction::DOWN, EdgeRule::WRAP) == step({0, 0}, Direction::DOWN, EdgeRule::WALLS));
    CHECK(step({19, 19}, Direction::LEFT, EdgeRule::WRAP) == step({19, 19}, Direction::LEFT, EdgeRule::WALLS));
}
