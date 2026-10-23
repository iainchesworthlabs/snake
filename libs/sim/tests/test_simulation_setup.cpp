// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace snake::sim;

namespace
{

std::optional<Simulation> build(EdgeRule edges, Direction heading, const std::vector<Cell>& snake, Cell food)
{
    return Simulation::from_setup({.edges = edges, .heading = heading, .snake = snake, .food = food});
}

} // namespace

TEST_CASE("A legal position is accepted and read back as given", "[simulation][setup]")
{
    const auto game = build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {4, 5}, {3, 5}}, {9, 9});
    REQUIRE(game.has_value());
    CHECK(game->length() == 3);
    CHECK(game->head() == Cell{5, 5});
    CHECK(game->food() == Cell{9, 9});
    CHECK(game->heading() == Direction::RIGHT);
    CHECK(game->edge_rule() == EdgeRule::WALLS);
    CHECK(game->running());
}

TEST_CASE("A snake of one cell may head any way", "[simulation][setup]")
{
    CHECK(build(EdgeRule::WALLS, Direction::LEFT, {{5, 5}}, {9, 9}).has_value());
}

TEST_CASE("A head at the edge may point off the board when the edges are walls", "[simulation][setup]")
{
    // It is a legal position; the next tick ends the game.
    auto game = build(EdgeRule::WALLS, Direction::UP, {{5, 0}, {5, 1}}, {9, 9});
    REQUIRE(game.has_value());
    CHECK(game->tick() == TickOutcome::HIT_WALL);
}

TEST_CASE("An empty snake is rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {}, {9, 9}).has_value());
}

TEST_CASE("A snake longer than the board is rejected", "[simulation][setup]")
{
    const std::vector<Cell> too_many(BOARD_CELLS + 1, Cell{1, 1});
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, too_many, {9, 9}).has_value());
}

TEST_CASE("A cell off the board is rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{19, 5}, {20, 5}}, {9, 9}).has_value());
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}}, {20, 9}).has_value());
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}}, {9, 20}).has_value());
}

TEST_CASE("A cell used twice is rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::DOWN, {{5, 5}, {6, 5}, {6, 6}, {5, 6}, {5, 5}}, {9, 9}).has_value());
}

TEST_CASE("Parts that do not touch are rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {7, 5}}, {9, 9}).has_value());
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {6, 6}}, {9, 9}).has_value());
}

TEST_CASE("Parts on opposite edges touch only when the edges wrap", "[simulation][setup]")
{
    CHECK(build(EdgeRule::WRAP, Direction::UP, {{0, 5}, {19, 5}}, {9, 9}).has_value());
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::UP, {{0, 5}, {19, 5}}, {9, 9}).has_value());
}

TEST_CASE("A heading that points into the neck is rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::LEFT, {{5, 5}, {4, 5}}, {9, 9}).has_value());
    CHECK(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {4, 5}}, {9, 9}).has_value());
    // Through the edge, when the edges wrap.
    CHECK_FALSE(build(EdgeRule::WRAP, Direction::LEFT, {{0, 5}, {19, 5}}, {9, 9}).has_value());
}

TEST_CASE("Food on the snake is rejected", "[simulation][setup]")
{
    CHECK_FALSE(build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {4, 5}}, {4, 5}).has_value());
}
