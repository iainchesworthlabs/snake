// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

using namespace snake::sim;

namespace
{

/// The rules that must hold after every tick, whatever the inputs were.
void check_invariants(const Simulation& game, std::size_t foods_eaten)
{
    std::array<bool, BOARD_CELLS> seen{};
    for (std::size_t part = 0; part < game.length(); ++part)
    {
        const Cell cell = game.snake()[part];
        REQUIRE(on_board(cell));
        const std::size_t index = to_index(cell);
        REQUIRE_FALSE(seen[index]); // no two parts share a cell
        seen[index] = true;
        if (part > 0)
        {
            // Each part is one step from the one before, counting a step across the edge when the edges wrap.
            const Cell before = game.snake()[part - 1];
            const int dx = (before.x - cell.x + BOARD_COLUMNS) % BOARD_COLUMNS;
            const int dy = (before.y - cell.y + BOARD_ROWS) % BOARD_ROWS;
            const bool step_x = (dx == 1 || dx == BOARD_COLUMNS - 1) && dy == 0;
            const bool step_y = (dy == 1 || dy == BOARD_ROWS - 1) && dx == 0;
            REQUIRE((step_x || step_y));
            if (game.edge_rule() == EdgeRule::WALLS)
            {
                REQUIRE((before.x > cell.x ? before.x - cell.x : cell.x - before.x) <= 1);
                REQUIRE((before.y > cell.y ? before.y - cell.y : cell.y - before.y) <= 1);
            }
        }
    }
    REQUIRE(game.length() == Simulation::INITIAL_LENGTH + foods_eaten);
    if (!game.won())
    {
        REQUIRE(on_board(game.food()));
        REQUIRE_FALSE(seen[to_index(game.food())]); // the food is on an empty cell
    }
}

} // namespace

TEST_CASE("Random play never breaks the rules of the board", "[simulation][property]")
{
    constexpr Direction ALL[] = {Direction::UP, Direction::DOWN, Direction::LEFT, Direction::RIGHT};
    for (const EdgeRule edges : {EdgeRule::WALLS, EdgeRule::WRAP})
    {
        Random inputs(2026);
        std::uint64_t seed = 1;
        std::uint64_t ticks = 0;
        while (ticks < 200000)
        {
            Simulation game(edges, seed++);
            std::size_t eaten = 0;
            while (game.running() && ticks < 200000)
            {
                // Mostly keep going, sometimes turn, sometimes press something silly such as a reversal.
                if (inputs.below(4) == 0)
                {
                    static_cast<void>(game.turn(ALL[inputs.below(4)]));
                }
                const Cell head_before = game.head();
                const std::size_t length_before = game.length();
                const TickOutcome outcome = game.tick();
                ++ticks;
                if (outcome == TickOutcome::ATE || outcome == TickOutcome::WON)
                {
                    ++eaten;
                }
                if (outcome == TickOutcome::MOVED || outcome == TickOutcome::ATE)
                {
                    REQUIRE(game.head() != head_before);
                    REQUIRE(game.length() == length_before + (outcome == TickOutcome::ATE ? 1 : 0));
                }
                check_invariants(game, eaten);
            }
        }
    }
}

TEST_CASE("A game that ends stays ended", "[simulation][property]")
{
    for (std::uint64_t seed = 0; seed < 50; ++seed)
    {
        Simulation game(EdgeRule::WALLS, seed);
        // Nothing steers, so the snake runs straight into the right wall.
        int guard = 0;
        while (game.running() && guard++ < 100)
        {
            static_cast<void>(game.tick());
        }
        REQUIRE_FALSE(game.running());
        REQUIRE(game.tick() == TickOutcome::NOT_RUNNING);
    }
}
