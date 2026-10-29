// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

// Two pairs: (5, 5) with (15, 3), and (2, 12) with (17, 15).
constexpr Simulation::PortalCells PORTALS{{Cell{5, 5}, Cell{15, 3}, Cell{2, 12}, Cell{17, 15}}};

std::optional<Simulation> portal_game(Direction heading, const std::vector<Cell>& snake, Cell food,
                                      const Simulation::PortalCells& portals = PORTALS)
{
    return Simulation::from_setup({.edges = EdgeRule::WALLS,
                                   .heading = heading,
                                   .snake = snake,
                                   .food = food,
                                   .seed = 1,
                                   .variant = Variant::PORTALS,
                                   .portals = portals});
}

Simulation required_game(Direction heading, const std::vector<Cell>& snake, Cell food)
{
    const std::optional<Simulation> game = portal_game(heading, snake, food);
    REQUIRE(game.has_value());
    return *game;
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

} // namespace

TEST_CASE("A portal leads to the other portal of its pair, in both directions", "[portals]")
{
    CHECK(PORTALS.exit_from({5, 5}) == Cell{15, 3});
    CHECK(PORTALS.exit_from({15, 3}) == Cell{5, 5});
    CHECK(PORTALS.exit_from({2, 12}) == Cell{17, 15});
    CHECK(PORTALS.exit_from({17, 15}) == Cell{2, 12});
    CHECK_FALSE(PORTALS.exit_from({6, 5}).has_value());
    CHECK(PORTALS.contains({17, 15}));
    CHECK_FALSE(PORTALS.contains({0, 0}));
}

TEST_CASE("A head that steps onto a portal comes out of the other one and goes on the way it was going", "[portals]")
{
    Simulation game = required_game(Direction::RIGHT, {{4, 5}, {3, 5}, {2, 5}}, {10, 10});
    REQUIRE(game.tick() == TickOutcome::TELEPORTED);
    CHECK(cells_of(game) == std::vector<Cell>{{15, 3}, {4, 5}, {3, 5}});
    CHECK(game.heading() == Direction::RIGHT);
    CHECK(game.running());
    CHECK(game.length() == 3);

    // The next step is from the exit, in the same direction.
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.head() == Cell{16, 3});
}

TEST_CASE("The second portal of a pair leads back to the first, and the other pair works the same", "[portals]")
{
    Simulation back = required_game(Direction::LEFT, {{16, 3}, {17, 3}, {18, 3}}, {10, 10});
    REQUIRE(back.tick() == TickOutcome::TELEPORTED);
    CHECK(back.head() == Cell{5, 5});

    Simulation other = required_game(Direction::DOWN, {{2, 11}, {2, 10}, {2, 9}}, {10, 10});
    REQUIRE(other.tick() == TickOutcome::TELEPORTED);
    CHECK(other.head() == Cell{17, 15});
    CHECK(other.heading() == Direction::DOWN);
}

TEST_CASE("A turn made as the head comes out of a portal goes on from the exit", "[portals]")
{
    Simulation game = required_game(Direction::RIGHT, {{4, 5}, {3, 5}, {2, 5}}, {10, 10});
    REQUIRE(game.tick() == TickOutcome::TELEPORTED);
    REQUIRE(game.turn(Direction::DOWN));
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.head() == Cell{15, 4});
}

/// A snake from the head at (4, 5) down to row 6, along it to column 15 and up to (15, 3), and then `extra` more cells
/// up column 15: (15, 3) is on the exit of the portal at (5, 5), which the head is about to step onto.
std::vector<Cell> winding_snake(std::size_t extra)
{
    std::vector<Cell> snake{{4, 5}, {4, 6}};
    for (std::uint8_t x = 5; x <= 15; ++x)
    {
        snake.push_back({x, 6});
    }
    for (std::uint8_t y = 5; y >= 3; --y)
    {
        snake.push_back({15, y});
    }
    for (std::size_t more = 0; more < extra; ++more)
    {
        snake.push_back({15, static_cast<std::uint8_t>(2 - more)});
    }
    return snake;
}

TEST_CASE("A head that would come out on its own body ends the game and the snake stays where it was", "[portals]")
{
    // (15, 3), the exit, is the second part from the tail, which has not moved away yet.
    Simulation game = required_game(Direction::RIGHT, winding_snake(1), {10, 10});
    const std::vector<Cell> before = cells_of(game);
    CHECK(game.tick() == TickOutcome::HIT_SELF);
    CHECK_FALSE(game.running());
    CHECK(cells_of(game) == before);
}

TEST_CASE("The tail's cell is free to come out on, because the tail moves away", "[portals]")
{
    // The tail is on the exit, (15, 3).
    Simulation game = required_game(Direction::RIGHT, winding_snake(0), {10, 10});
    const std::size_t length = game.length();
    REQUIRE(game.tick() == TickOutcome::TELEPORTED);
    CHECK(game.head() == Cell{15, 3});
    CHECK(game.length() == length);
    CHECK(game.running());
    // The old tail's cell is the head's now, and the part that was before the tail is the tail.
    CHECK(game.snake().back() == Cell{15, 4});
}

TEST_CASE("Entering a portal that the body lies on still takes the head through", "[portals]")
{
    // The body lies on the portal at (15, 3), where the head came out earlier. Going in at it leads to (5, 5), which is
    // free, so the body being there is of no matter.
    Simulation game = required_game(Direction::UP, {{15, 4}, {16, 4}, {16, 3}, {15, 3}}, {10, 10});
    REQUIRE(game.tick() == TickOutcome::TELEPORTED);
    CHECK(game.head() == Cell{5, 5});
}

TEST_CASE("A portal on the edge works like any other", "[portals]")
{
    const Simulation::PortalCells edge{{Cell{19, 8}, Cell{0, 14}, Cell{2, 2}, Cell{12, 12}}};
    const std::optional<Simulation> start = portal_game(Direction::RIGHT, {{17, 8}, {16, 8}, {15, 8}}, {10, 10}, edge);
    REQUIRE(start.has_value());
    Simulation game = *start;
    REQUIRE(game.tick() == TickOutcome::MOVED);
    REQUIRE(game.tick() == TickOutcome::TELEPORTED);
    CHECK(game.head() == Cell{0, 14});
    // Heading right still, from the left edge.
    REQUIRE(game.tick() == TickOutcome::MOVED);
    CHECK(game.head() == Cell{1, 14});
}

TEST_CASE("Eating still works around the portals", "[portals]")
{
    Simulation game = required_game(Direction::RIGHT, {{10, 8}, {9, 8}, {8, 8}}, {11, 8});
    CHECK(game.tick() == TickOutcome::ATE);
    CHECK(game.length() == 4);
    CHECK(game.score() == 10);
    // The new food is not on a portal or the snake.
    CHECK_FALSE(PORTALS.contains(game.food()));
    CHECK_FALSE(on_snake(game, game.food()));
}

TEST_CASE("A new game of portals has four portals, each on its own cell, away from the snake's row", "[portals][setup]")
{
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        const Simulation game(EdgeRule::WALLS, seed, Variant::PORTALS);
        REQUIRE(game.variant() == Variant::PORTALS);
        REQUIRE(game.portals().has_value());
        const auto& cells = game.portals()->cells;
        for (std::size_t first = 0; first < cells.size(); ++first)
        {
            REQUIRE(on_board(cells[first]));
            REQUIRE(cells[first].y != game.head().y);
            REQUIRE_FALSE(on_snake(game, cells[first]));
            REQUIRE(cells[first] != game.food());
            for (std::size_t second = first + 1; second < cells.size(); ++second)
            {
                REQUIRE(cells[first] != cells[second]);
                // Not next to one another.
                const int dx = cells[first].x - cells[second].x;
                const int dy = cells[first].y - cells[second].y;
                REQUIRE(dx * dx + dy * dy > 1);
            }
        }
    }
}

TEST_CASE("The portals follow from the seed, and a plain game has none", "[portals][setup]")
{
    const Simulation one(EdgeRule::WALLS, 77, Variant::PORTALS);
    const Simulation two(EdgeRule::WALLS, 77, Variant::PORTALS);
    CHECK(one.portals() == two.portals());
    CHECK(state_hash(one) == state_hash(two));
    CHECK(one.portals() != Simulation(EdgeRule::WALLS, 78, Variant::PORTALS).portals());

    const Simulation plain(EdgeRule::WALLS, 77);
    CHECK(plain.variant() == Variant::PLAIN);
    CHECK_FALSE(plain.portals().has_value());
}

TEST_CASE("Food and the bonus item never go on a portal, however long a game of portals runs", "[portals][properties]")
{
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        Simulation game(EdgeRule::WALLS, seed, Variant::PORTALS);
        for (int tick = 0; tick < 3000 && game.running(); ++tick)
        {
            static_cast<void>(game.turn(autoplay(game)));
            static_cast<void>(game.tick());
            REQUIRE_FALSE(game.portals()->contains(game.food()));
            if (game.bonus())
            {
                REQUIRE_FALSE(game.portals()->contains(game.bonus()->cell));
            }
        }
    }
}

TEST_CASE("The computer player keeps out of the portals while it has room", "[portals][autoplay]")
{
    for (std::uint64_t seed = 1; seed <= 10; ++seed)
    {
        Simulation game(EdgeRule::WALLS, seed, Variant::PORTALS);
        for (int tick = 0; tick < 2000 && game.running() && game.length() < 40; ++tick)
        {
            static_cast<void>(game.turn(autoplay(game)));
            REQUIRE(game.tick() != TickOutcome::TELEPORTED);
        }
    }
}

TEST_CASE("A game of portals is set up only with its four portals", "[portals][setup]")
{
    const std::vector<Cell> snake{{4, 5}, {3, 5}, {2, 5}};
    const auto build = [&](Variant variant, std::optional<Simulation::PortalCells> portals)
    {
        return Simulation::from_setup(
            {.snake = snake, .food = {10, 10}, .seed = 1, .variant = variant, .portals = portals});
    };
    CHECK(build(Variant::PORTALS, PORTALS).has_value());
    CHECK_FALSE(build(Variant::PORTALS, std::nullopt).has_value());
    CHECK_FALSE(build(Variant::PLAIN, PORTALS).has_value());
    CHECK_FALSE(build(Variant::HOT_POTATO, PORTALS).has_value());

    // Two portals on one cell, one off the board, one under the food.
    CHECK_FALSE(build(Variant::PORTALS, Simulation::PortalCells{{Cell{5, 5}, Cell{5, 5}, Cell{2, 12}, Cell{17, 15}}})
                    .has_value());
    CHECK_FALSE(build(Variant::PORTALS, Simulation::PortalCells{{Cell{5, 5}, Cell{15, 3}, Cell{2, 12}, Cell{20, 15}}})
                    .has_value());
    CHECK_FALSE(build(Variant::PORTALS, Simulation::PortalCells{{Cell{5, 5}, Cell{10, 10}, Cell{2, 12}, Cell{17, 15}}})
                    .has_value());
}

TEST_CASE("A bonus item on a portal is not a position", "[portals][setup]")
{
    const std::vector<Cell> snake{{4, 5}, {3, 5}, {2, 5}};
    CHECK_FALSE(Simulation::from_setup({.snake = snake,
                                        .food = {10, 10},
                                        .seed = 1,
                                        .bonus = Simulation::Bonus{.cell = {15, 3}},
                                        .variant = Variant::PORTALS,
                                        .portals = PORTALS})
                    .has_value());
}

TEST_CASE("Parts of a snake that are a portal apart are a legal snake, and parts that are not are refused",
          "[portals][setup]")
{
    // (4, 5) stepped onto the portal at (5, 5) and the head came out at (15, 3).
    CHECK(portal_game(Direction::RIGHT, {{15, 3}, {4, 5}, {3, 5}}, {10, 10}).has_value());
    // The same without portals, and with the wrong exit, are not.
    CHECK_FALSE(portal_game(Direction::RIGHT, {{17, 15}, {4, 5}, {3, 5}}, {10, 10}).has_value());
    CHECK_FALSE(
        Simulation::from_setup({.snake = std::vector<Cell>{{15, 3}, {4, 5}, {3, 5}}, .food = {10, 10}}).has_value());
}
