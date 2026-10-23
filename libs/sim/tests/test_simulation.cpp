// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <vector>

using namespace snake::sim;

namespace
{

/// A game in an exact position, which must be a legal one.
Simulation game_from(EdgeRule edges, Direction heading, std::vector<Cell> snake, Cell food, std::uint64_t seed = 1)
{
    const std::optional<Simulation> game =
        Simulation::from_setup({.edges = edges, .heading = heading, .snake = snake, .food = food, .seed = seed});
    REQUIRE(game.has_value());
    return *game;
}

bool on_snake(const Simulation& game, Cell cell)
{
    for (std::size_t part = 0; part < game.length(); ++part)
    {
        if (game.snake()[part] == cell)
        {
            return true;
        }
    }
    return false;
}

std::vector<Cell> cells_of(const Simulation& game)
{
    std::vector<Cell> cells;
    for (std::size_t part = 0; part < game.length(); ++part)
    {
        cells.push_back(game.snake()[part]);
    }
    return cells;
}

} // namespace

TEST_CASE("A new game has a snake of length 3 in the middle heading right", "[simulation][setup]")
{
    const Simulation game(EdgeRule::WALLS, 42);
    CHECK(game.running());
    CHECK_FALSE(game.won());
    CHECK(game.length() == 3);
    CHECK(game.heading() == Direction::RIGHT);
    CHECK(cells_of(game) == std::vector<Cell>{{10, 10}, {9, 10}, {8, 10}});
    CHECK(game.head() == Cell{10, 10});
}

TEST_CASE("A new game has exactly one food, on an empty cell, chosen from the seed", "[simulation][setup]")
{
    // The expected cells come from an independent computation of the generator and the empty-cell rank.
    CHECK(Simulation(EdgeRule::WALLS, 42).food() == Cell{4, 6});
    CHECK(Simulation(EdgeRule::WALLS, 1).food() == Cell{12, 14});
    CHECK(Simulation(EdgeRule::WRAP, 7).food() == Cell{18, 13});
}

TEST_CASE("The food never starts on the snake, whatever the seed", "[simulation][setup]")
{
    for (std::uint64_t seed = 0; seed < 500; ++seed)
    {
        const Simulation game(EdgeRule::WALLS, seed);
        REQUIRE(on_board(game.food()));
        REQUIRE_FALSE(on_snake(game, game.food()));
    }
}

TEST_CASE("The snake moves one cell per tick in its heading and keeps its length", "[simulation][move]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {0, 0});
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(cells_of(game) == std::vector<Cell>{{11, 10}, {10, 10}, {9, 10}});
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(cells_of(game) == std::vector<Cell>{{12, 10}, {11, 10}, {10, 10}});
    CHECK(game.food() == Cell{0, 0});
}

TEST_CASE("Each heading moves the head one cell the right way", "[simulation][move]")
{
    const struct
    {
        Direction heading;
        Cell next;
    } cases[] = {
        {Direction::UP, {5, 4}}, {Direction::DOWN, {5, 6}}, {Direction::LEFT, {4, 5}}, {Direction::RIGHT, {6, 5}}};
    for (const auto& item : cases)
    {
        // A one-cell snake can head anywhere.
        Simulation game = game_from(EdgeRule::WALLS, item.heading, {{5, 5}}, {0, 0});
        REQUIRE(game.tick() == TickOutcome::MOVED);
        CHECK(game.head() == item.next);
    }
}

TEST_CASE("A queued turn takes effect on the next tick, and only one turn is taken per tick", "[simulation][move]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {0, 0});
    REQUIRE(game.turn(Direction::UP));
    REQUIRE(game.turn(Direction::LEFT));
    CHECK(game.heading() == Direction::RIGHT);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.heading() == Direction::UP);
    CHECK(game.head() == Cell{10, 9});
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.heading() == Direction::LEFT);
    CHECK(game.head() == Cell{9, 9});
}

TEST_CASE("A reversal is ignored and the snake carries on", "[simulation][move]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {0, 0});
    CHECK_FALSE(game.turn(Direction::LEFT));
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.head() == Cell{11, 10});
}

TEST_CASE("A fast up-then-down while moving right does not reverse the snake into itself", "[simulation][move]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {0, 0});
    CHECK(game.turn(Direction::UP));
    CHECK_FALSE(game.turn(Direction::DOWN));
    REQUIRE(game.tick() == TickOutcome::MOVED);
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.running());
    CHECK(game.head() == Cell{10, 8});
}

TEST_CASE("With walls, leaving the board ends the game on every side", "[simulation][edges]")
{
    const struct
    {
        Direction heading;
        Cell head;
    } cases[] = {
        {Direction::UP, {7, 0}}, {Direction::DOWN, {7, 19}}, {Direction::LEFT, {0, 7}}, {Direction::RIGHT, {19, 7}}};
    for (const auto& item : cases)
    {
        Simulation game = game_from(EdgeRule::WALLS, item.heading, {item.head}, {10, 10});
        CHECK(game.tick() == TickOutcome::HIT_WALL);
        CHECK_FALSE(game.running());
        CHECK_FALSE(game.won());
        // The snake stays where it was.
        CHECK(cells_of(game) == std::vector<Cell>{item.head});
    }
}

TEST_CASE("With wrap, leaving the board comes back on the opposite edge on every side", "[simulation][edges]")
{
    const struct
    {
        Direction heading;
        Cell head;
        Cell next;
    } cases[] = {{Direction::UP, {7, 0}, {7, 19}},
                 {Direction::DOWN, {7, 19}, {7, 0}},
                 {Direction::LEFT, {0, 7}, {19, 7}},
                 {Direction::RIGHT, {19, 7}, {0, 7}}};
    for (const auto& item : cases)
    {
        Simulation game = game_from(EdgeRule::WRAP, item.heading, {item.head}, {10, 10});
        CHECK(game.tick() == TickOutcome::MOVED);
        CHECK(game.running());
        CHECK(game.head() == item.next);
    }
}

TEST_CASE("A snake with a body wraps whole, a cell at a time", "[simulation][edges]")
{
    Simulation game = game_from(EdgeRule::WRAP, Direction::RIGHT, {{0, 4}, {19, 4}, {18, 4}}, {10, 10});
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(cells_of(game) == std::vector<Cell>{{1, 4}, {0, 4}, {19, 4}});
}

TEST_CASE("Running into the body ends the game", "[simulation][collision]")
{
    // The head is at (5, 5) and the body curls under it, so going down hits (5, 6).
    Simulation game = game_from(EdgeRule::WALLS, Direction::DOWN, {{5, 5}, {6, 5}, {6, 6}, {5, 6}, {4, 6}}, {0, 0});
    const std::vector<Cell> before = cells_of(game);
    CHECK(game.tick() == TickOutcome::HIT_SELF);
    CHECK_FALSE(game.running());
    CHECK(cells_of(game) == before);
}

TEST_CASE("The tail cell is free because the tail moves away this tick", "[simulation][collision]")
{
    // A square of four cells. The head goes down onto the cell the tail is leaving.
    Simulation game = game_from(EdgeRule::WALLS, Direction::DOWN, {{5, 5}, {6, 5}, {6, 6}, {5, 6}}, {0, 0});
    CHECK(game.tick() == TickOutcome::MOVED);
    CHECK(game.running());
    CHECK(cells_of(game) == std::vector<Cell>{{5, 6}, {5, 5}, {6, 5}, {6, 6}});
}

TEST_CASE("Only the tail cell is free, not the cell before it", "[simulation][collision]")
{
    // The tail is at (4, 6). The head goes down onto (5, 6), the cell before the tail, which is still body.
    Simulation game = game_from(EdgeRule::WALLS, Direction::DOWN, {{5, 5}, {6, 5}, {6, 6}, {5, 6}, {4, 6}}, {0, 0});
    CHECK(game.tick() == TickOutcome::HIT_SELF);
}

TEST_CASE("Moving onto the food makes the snake grow and keeps the tail", "[simulation][eat]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {11, 10}, 3);
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(game.running());
    CHECK(game.length() == 4);
    CHECK(cells_of(game) == std::vector<Cell>{{11, 10}, {10, 10}, {9, 10}, {8, 10}});
}

TEST_CASE("After eating there is new food on an empty cell", "[simulation][eat]")
{
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{10, 10}, {9, 10}, {8, 10}}, {11, 10}, seed);
        REQUIRE(game.tick() == TickOutcome::ATE);
        REQUIRE(on_board(game.food()));
        REQUIRE_FALSE(on_snake(game, game.food()));
        REQUIRE(game.food() != Cell{11, 10});
    }
}

TEST_CASE("Eating at the edge with wrap grows the snake across the edge", "[simulation][eat]")
{
    Simulation game = game_from(EdgeRule::WRAP, Direction::RIGHT, {{19, 3}, {18, 3}, {17, 3}}, {0, 3});
    REQUIRE(game.tick() == TickOutcome::ATE);
    CHECK(cells_of(game) == std::vector<Cell>{{0, 3}, {19, 3}, {18, 3}, {17, 3}});
}

TEST_CASE("The only empty cell is where the food goes", "[simulation][eat]")
{
    // A snake that covers every cell but the one it is about to eat and one more.
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{static_cast<std::uint8_t>(row % 2 == 0 ? column : BOARD_COLUMNS - 1 - column), row});
        }
    }
    // The path ends at (0, 19). The snake holds the first 398 cells, its head at (2, 19), and eats (1, 19).
    std::vector<Cell> snake(path.begin(), path.begin() + 398);
    std::vector<Cell> head_first(snake.rbegin(), snake.rend());
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        Simulation game = game_from(EdgeRule::WALLS, Direction::LEFT, head_first, {1, 19}, seed);
        REQUIRE(game.tick() == TickOutcome::ATE);
        REQUIRE(game.food() == Cell{0, 19});
    }
}

TEST_CASE("Filling the board ends the game as a perfect game", "[simulation][win]")
{
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{static_cast<std::uint8_t>(row % 2 == 0 ? column : BOARD_COLUMNS - 1 - column), row});
        }
    }
    // 399 cells, head at (1, 19), the last empty cell (0, 19) holds the food.
    std::vector<Cell> snake(path.begin(), path.begin() + 399);
    std::vector<Cell> head_first(snake.rbegin(), snake.rend());
    Simulation game = game_from(EdgeRule::WALLS, Direction::LEFT, head_first, {0, 19});
    REQUIRE(game.tick() == TickOutcome::WON);
    CHECK_FALSE(game.running());
    CHECK(game.won());
    CHECK(game.length() == BOARD_CELLS);
}

TEST_CASE("After the game has ended a tick changes nothing", "[simulation][state]")
{
    Simulation game = game_from(EdgeRule::WALLS, Direction::RIGHT, {{19, 7}}, {10, 10});
    REQUIRE(game.tick() == TickOutcome::HIT_WALL);
    const std::vector<Cell> before = cells_of(game);
    CHECK(game.tick() == TickOutcome::NOT_RUNNING);
    CHECK(game.tick() == TickOutcome::NOT_RUNNING);
    CHECK(cells_of(game) == before);
    CHECK_FALSE(game.running());
}

TEST_CASE("The same seed and inputs give the same game", "[simulation][determinism]")
{
    Simulation first(EdgeRule::WRAP, 99);
    Simulation second(EdgeRule::WRAP, 99);
    Random inputs(5);
    constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    for (int tick = 0; tick < 2000; ++tick)
    {
        const Direction input = ALL[inputs.below(4)];
        CHECK(first.turn(input) == second.turn(input));
        const TickOutcome outcome = first.tick();
        REQUIRE(outcome == second.tick());
        REQUIRE(cells_of(first) == cells_of(second));
        REQUIRE(first.food() == second.food());
    }
}
