// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/cell.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/snapshot.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

constexpr std::array<Direction, 4> ALL_DIRECTIONS{Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};

Simulation build(EdgeRule edges, Direction heading, const std::vector<Cell>& snake, Cell food)
{
    const auto game = Simulation::from_setup({.edges = edges, .heading = heading, .snake = snake, .food = food});
    REQUIRE(game.has_value());
    return *game;
}

/// True when turning to `direction` and ticking does not end the game.
bool survives(const Simulation& game, Direction direction)
{
    Simulation copy = game;
    if (!copy.turn(direction))
    {
        return false; // a reversal or a repeat is dropped, and the snake carries on straight
    }
    const TickOutcome outcome = copy.tick();
    return outcome != TickOutcome::HIT_WALL && outcome != TickOutcome::HIT_SELF;
}

bool straight_on_survives(const Simulation& game)
{
    Simulation copy = game;
    const TickOutcome outcome = copy.tick();
    return outcome != TickOutcome::HIT_WALL && outcome != TickOutcome::HIT_SELF;
}

/// Plays one game with the autoplayer for at most `max_ticks`, and returns it.
Simulation play(EdgeRule edges, std::uint64_t seed, std::uint32_t max_ticks)
{
    Simulation game(edges, seed);
    while (game.running() && game.ticks() < max_ticks)
    {
        static_cast<void>(game.turn(autoplay(game)));
        static_cast<void>(game.tick());
    }
    return game;
}

} // namespace

TEST_CASE("The autoplayer goes straight on when the food is straight ahead", "[autoplayer]")
{
    const Simulation game = build(EdgeRule::WALLS, Direction::RIGHT, {{5, 5}, {4, 5}, {3, 5}}, {9, 5});
    CHECK(autoplay(game) == Direction::RIGHT);
}

TEST_CASE("The autoplayer turns towards food above, below and beside the snake", "[autoplayer]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    CHECK(autoplay(build(EdgeRule::WALLS, Direction::RIGHT, snake, {10, 5})) == Direction::UP);
    CHECK(autoplay(build(EdgeRule::WALLS, Direction::RIGHT, snake, {10, 15})) == Direction::DOWN);
}

TEST_CASE("The autoplayer takes the first of up, down, left, right when two ways are equally short", "[autoplayer]")
{
    const std::vector<Cell> snake{{10, 10}, {9, 10}, {8, 10}};
    // Four steps either way: right then down or down then right, and right then up or up then right.
    CHECK(autoplay(build(EdgeRule::WALLS, Direction::RIGHT, snake, {12, 12})) == Direction::DOWN);
    CHECK(autoplay(build(EdgeRule::WALLS, Direction::RIGHT, snake, {12, 8})) == Direction::UP);
}

TEST_CASE("The autoplayer never steers into the wall to reach food beyond it", "[autoplayer]")
{
    const std::vector<Cell> snake{{19, 5}, {18, 5}, {17, 5}};
    const Direction direction = autoplay(build(EdgeRule::WALLS, Direction::RIGHT, snake, {5, 5}));
    CHECK(direction == Direction::UP);
}

TEST_CASE("With wrapping the autoplayer takes the short way through the edge", "[autoplayer][wrap]")
{
    const std::vector<Cell> snake{{1, 5}, {2, 5}, {3, 5}};
    CHECK(autoplay(build(EdgeRule::WRAP, Direction::LEFT, snake, {18, 5})) == Direction::LEFT);
    const std::vector<Cell> top{{5, 1}, {5, 2}, {5, 3}};
    CHECK(autoplay(build(EdgeRule::WRAP, Direction::UP, top, {5, 18})) == Direction::UP);
}

TEST_CASE("The autoplayer follows its tail out of a corner", "[autoplayer]")
{
    // Walls above and to the left, the body to the right and below-right. The only way on is down, onto the cell the
    // tail is leaving.
    const std::vector<Cell> snake{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    CHECK(autoplay(build(EdgeRule::WALLS, Direction::LEFT, snake, {10, 10})) == Direction::DOWN);
}

TEST_CASE("When there is not enough room beyond the food it takes the safe step instead", "[autoplayer]")
{
    // The snake zigzags over the top 18 rows, 360 cells, and its head is at the left end of row 17. Only the bottom two
    // rows are free: 40 cells, fewer than the snake is long. The only safe step is down.
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < 18; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{(row % 2 == 0) ? column : static_cast<std::uint8_t>(BOARD_COLUMNS - 1 - column), row});
        }
    }
    const std::vector<Cell> snake(path.rbegin(), path.rend());
    const Simulation game = build(EdgeRule::WALLS, Direction::LEFT, snake, {10, 19});
    CHECK(autoplay(game) == Direction::DOWN);
}

TEST_CASE("The autoplayer keeps its heading when every step ends the game", "[autoplayer]")
{
    // A head in a corner with the body filling the two other neighbours' way out: up and left are walls, right is the
    // neck, and down is the body (not the tail).
    const std::vector<Cell> snake{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 2}, {1, 2}};
    const Simulation game = build(EdgeRule::WALLS, Direction::LEFT, snake, {10, 10});
    CHECK(autoplay(game) == Direction::LEFT);
}

TEST_CASE("The autoplayer gives the same answer for the same position", "[autoplayer]")
{
    const Simulation game(EdgeRule::WRAP, 5);
    CHECK(autoplay(game) == autoplay(game));
}

TEST_CASE("The autoplayer plays the same game every time for a seed", "[autoplayer]")
{
    const Simulation first = play(EdgeRule::WALLS, 21, 2000);
    const Simulation second = play(EdgeRule::WALLS, 21, 2000);
    CHECK(capture(first) == capture(second));
}

TEST_CASE("Two autoplayed games end as recorded, on every platform", "[autoplayer][golden]")
{
    // The values come from the library itself. A change to the autoplayer or to a rule makes them fail on purpose,
    // and they are updated in the same pull request.
    const Simulation walls = play(EdgeRule::WALLS, 21, 100'000);
    CHECK_FALSE(walls.running());
    CHECK(walls.ticks() == 1967);
    CHECK(walls.foods_eaten() == 109);
    CHECK(walls.score() == 2940);
    CHECK(state_hash(walls) == 0x07524f59c140c8a1ULL);

    const Simulation wrap = play(EdgeRule::WRAP, 21, 100'000);
    CHECK_FALSE(wrap.running());
    CHECK(wrap.ticks() == 2057);
    CHECK(wrap.foods_eaten() == 136);
    CHECK(wrap.score() == 5200);
    CHECK(state_hash(wrap) == 0xdb5848040853fa2eULL);
}

TEST_CASE("The autoplayer never makes a fatal move when a safe one exists", "[autoplayer][property]")
{
    for (const EdgeRule edges : {EdgeRule::WALLS, EdgeRule::WRAP})
    {
        for (std::uint64_t seed = 1; seed <= 12; ++seed)
        {
            Simulation game(edges, seed);
            while (game.running() && game.ticks() < 1500)
            {
                const Direction choice = autoplay(game);
                bool any_safe = straight_on_survives(game);
                for (const Direction direction : ALL_DIRECTIONS)
                {
                    any_safe = any_safe || survives(game, direction);
                }
                if (any_safe)
                {
                    const bool chosen_is_safe =
                        (choice == game.heading()) ? straight_on_survives(game) : survives(game, choice);
                    REQUIRE(chosen_is_safe);
                }
                static_cast<void>(game.turn(choice));
                static_cast<void>(game.tick());
            }
        }
    }
}

TEST_CASE("The autoplayer eats at least 50 foods in each of 24 games", "[autoplayer][property]")
{
    for (const EdgeRule edges : {EdgeRule::WALLS, EdgeRule::WRAP})
    {
        for (std::uint64_t seed = 1; seed <= 12; ++seed)
        {
            const Simulation game = play(edges, seed, 6000);
            CHECK(game.foods_eaten() >= 50);
        }
    }
}
