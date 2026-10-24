// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"

namespace snake::sim
{

/// The direction a computer player would steer in for the next tick. The title screen uses it to show the game
/// playing itself. The attract game is an ordinary Simulation with a seed of its own, so it never touches the food
/// generator of a real game, and this function holds no state and uses no random numbers: the same game position
/// always gives the same answer.
///
/// It looks for the shortest way to the food, counting a cell the snake's tail is about to leave as free by the time
/// the head gets there, and takes the first step of it. Where there are several shortest ways the first of up, down,
/// left and right wins. It will not take that step if the cells the snake could reach from there are fewer than its
/// length, because that is how it traps itself. With no way to the food, or a step that is not safe, it takes the
/// step that does not end the game and leaves the most room, again preferring up, down, left, right in that order.
/// When every step ends the game it keeps the current heading.
///
/// Call it with no turns waiting in the game's input buffer, once before each tick: `game.turn(autoplay(game))`.
[[nodiscard]] Direction autoplay(const Simulation& game) noexcept;

} // namespace snake::sim
