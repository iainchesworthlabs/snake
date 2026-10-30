// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/joystick_steering.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/simulation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <vector>

using namespace snake::input;
using namespace snake::sim;

namespace
{

/// A game on a board with walls, the snake heading right in the middle.
Simulation new_game()
{
    return Simulation(EdgeRule::WALLS, 7);
}

/// A game that is over: the snake is there but nothing steers it.
Simulation finished_game()
{
    const std::vector<Cell> snake = {{10, 10}, {9, 10}, {8, 10}};
    const std::optional<Simulation> game = Simulation::from_setup(
        {.edges = EdgeRule::WALLS, .heading = Direction::RIGHT, .snake = snake, .food = {0, 0}, .running = false});
    REQUIRE(game.has_value());
    return *game;
}

} // namespace

TEST_CASE("A knob near the middle steers nothing, and a knob straight out steers that way", "[joystick-steering]")
{
    const Simulation game = new_game();
    JoystickSteering stick;
    CHECK_FALSE(stick.move(0.0F, 0.1F, game));
    CHECK(stick.move(0.0F, 0.9F, game) == Direction::UP);
    CHECK(stick.move(0.0F, -0.9F, game) == Direction::DOWN);
    CHECK(stick.move(-0.9F, 0.0F, game) == Direction::LEFT);
    CHECK(stick.move(0.9F, 0.0F, game) == Direction::RIGHT);
}

TEST_CASE("A diagonal asks for the way across the snake's line", "[joystick-steering]")
{
    const Simulation game = new_game();
    REQUIRE(game.heading() == Direction::RIGHT);
    JoystickSteering stick;
    CHECK(stick.move(0.6F, 0.6F, game) == Direction::UP);
    stick.release();
    CHECK(stick.move(0.6F, -0.6F, game) == Direction::DOWN);
}

TEST_CASE("A diagonal is judged against the last turn that is waiting, and waits while one is", "[joystick-steering]")
{
    Simulation game = new_game();
    JoystickSteering stick;
    REQUIRE(game.turn(Direction::UP));

    // A straight direction is offered at once: the flick to the diagonal and back is two turns.
    CHECK(stick.move(0.9F, 0.0F, game) == Direction::RIGHT);
    // A diagonal is left until the tick has taken the turn that waits.
    stick.release();
    CHECK_FALSE(stick.move(0.6F, 0.6F, game));

    static_cast<void>(game.tick());
    // Now the snake heads up, and the same diagonal is a turn to the right: the staircase.
    CHECK(stick.again(game) == Direction::RIGHT);
}

TEST_CASE("A thumb that lifts is forgotten, and a game that is over is not steered", "[joystick-steering]")
{
    const Simulation game = new_game();
    JoystickSteering stick;
    CHECK_FALSE(stick.held());
    CHECK_FALSE(stick.again(game));

    REQUIRE(stick.move(0.0F, 0.9F, game));
    CHECK(stick.held());
    CHECK(stick.again(game) == Direction::UP);
    stick.release();
    CHECK_FALSE(stick.held());
    CHECK_FALSE(stick.again(game));

    const Simulation over = finished_game();
    CHECK_FALSE(stick.move(0.0F, 0.9F, over));
    CHECK(stick.held());
}
