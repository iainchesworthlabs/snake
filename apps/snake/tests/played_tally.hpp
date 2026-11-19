// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/cell.hpp"
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/simulation.hpp"
#include "snake/sim/tally.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace snake::test
{

/// What a game has to have done, for building the Tally of it.
struct Played
{
    std::uint16_t blueberries = 0;
    std::uint8_t combo = 1; // the combo after each blueberry; counts only when there is a blueberry
    std::uint16_t foods = 0;
    bool bonus = false;
    bool portal = false;
    bool shed = false;
    bool wrap = false;
};

/// A Tally that has seen `played`, built the way the application builds one: from the outcome of each tick and the
/// Simulation after it. The Simulation is set up with the combo and the food count wanted.
inline sim::Tally tally_of(const Played& played)
{
    using namespace sim;
    const auto required = [](const Simulation::Setup& setup)
    {
        const std::optional<Simulation> game = Simulation::from_setup(setup);
        REQUIRE(game.has_value());
        return *game;
    };
    static const std::vector<Cell> SNAKE{{5, 5}, {4, 5}, {3, 5}};
    const Simulation game =
        required({.edges = EdgeRule::WALLS,
                  .heading = Direction::RIGHT,
                  .snake = SNAKE,
                  .food = {10, 10},
                  .seed = 3,
                  .scoring = {.foods_eaten = played.foods, .combo = played.combo, .ticks_since_food = 0}});
    Tally tally;
    if (played.wrap)
    {
        // The head goes over the right edge of a board that wraps and comes out on the left.
        static const std::vector<Cell> EDGE_SNAKE{
            {BOARD_COLUMNS - 1, 5}, {BOARD_COLUMNS - 2, 5}, {BOARD_COLUMNS - 3, 5}};
        Simulation edge = required(
            {.edges = EdgeRule::WRAP, .heading = Direction::RIGHT, .snake = EDGE_SNAKE, .food = {10, 10}, .seed = 3});
        tally.start(edge.head());
        tally.observe(edge.tick(), edge);
        REQUIRE(tally.wrapped());
    }
    else
    {
        tally.start(game.head());
    }
    if (played.blueberries == 0)
    {
        tally.observe(TickOutcome::MOVED, game); // takes the food count in
    }
    for (std::uint16_t count = 0; count < played.blueberries; ++count)
    {
        tally.observe(TickOutcome::ATE, game);
    }
    if (played.bonus)
    {
        tally.observe(TickOutcome::ATE_BONUS, game);
    }
    if (played.portal)
    {
        tally.observe(TickOutcome::TELEPORTED, game);
    }
    if (played.shed)
    {
        tally.note_shed();
    }
    return tally;
}

/// A game that did everything a game can: every achievement of a game, in one.
inline sim::Tally full_tally()
{
    return tally_of(Played{
        .blueberries = 6, .combo = 5, .foods = 40, .bonus = true, .portal = true, .shed = true, .wrap = true});
}

} // namespace snake::test
