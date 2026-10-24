// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/session.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

using namespace snake::sim;

namespace
{

/// A running Classic session that the snake will end by driving into the right wall.
Session running_session(Mode mode = Mode::CLASSIC, std::uint64_t seed = 42)
{
    Session session(mode, seed);
    REQUIRE(session.start());
    return session;
}

/// Ticks until the game ends, for a snake that nothing steers.
void run_into_wall(Session& session)
{
    for (int guard = 0; session.state() == GameState::RUNNING && guard < 100; ++guard)
    {
        static_cast<void>(session.tick());
    }
}

/// A Classic session in a given position.
Session session_from(Simulation::Setup setup, std::vector<Cell> snake, GameState state)
{
    setup.snake = snake;
    const std::optional<Simulation> game = Simulation::from_setup(setup);
    REQUIRE(game.has_value());
    const std::optional<Session> session = Session::from_simulation(Mode::CLASSIC, *game, state);
    REQUIRE(session.has_value());
    return *session;
}

} // namespace

TEST_CASE("A new session is not started and has the mode's rules", "[session]")
{
    const Session easy(Mode::EASY, 1);
    CHECK(easy.state() == GameState::NOT_STARTED);
    CHECK(easy.mode() == Mode::EASY);
    CHECK(easy.simulation().edge_rule() == EdgeRule::WRAP);
    CHECK(easy.tick_interval_ms() == 180);

    const Session classic(Mode::CLASSIC, 1);
    CHECK(classic.mode() == Mode::CLASSIC);
    CHECK(classic.simulation().edge_rule() == EdgeRule::WALLS);
    CHECK(classic.tick_interval_ms() == 150);
}

TEST_CASE("Start moves a new session to running, once", "[session][state]")
{
    Session session(Mode::CLASSIC, 1);
    CHECK(session.start());
    CHECK(session.state() == GameState::RUNNING);
    CHECK_FALSE(session.start());
    CHECK(session.state() == GameState::RUNNING);
}

TEST_CASE("Pause and resume move between running and paused", "[session][state]")
{
    Session session = running_session();
    CHECK(session.pause());
    CHECK(session.state() == GameState::PAUSED);
    CHECK_FALSE(session.pause());
    CHECK_FALSE(session.start());
    CHECK(session.resume());
    CHECK(session.state() == GameState::RUNNING);
    CHECK_FALSE(session.resume());
}

TEST_CASE("A session that has not started cannot pause or resume", "[session][state]")
{
    Session session(Mode::CLASSIC, 1);
    CHECK_FALSE(session.pause());
    CHECK_FALSE(session.resume());
    CHECK(session.state() == GameState::NOT_STARTED);
}

TEST_CASE("Before the start the game does not move and takes no turns", "[session][state]")
{
    Session session(Mode::CLASSIC, 1);
    const Cell head = session.simulation().head();
    CHECK(session.tick() == TickOutcome::NOT_RUNNING);
    CHECK_FALSE(session.turn(Direction::UP));
    CHECK(session.simulation().head() == head);
}

TEST_CASE("While paused the game does not move and drops turns", "[session][state]")
{
    Session session = running_session();
    REQUIRE(session.tick() == TickOutcome::MOVED);
    REQUIRE(session.pause());
    const Cell head = session.simulation().head();
    CHECK(session.tick() == TickOutcome::NOT_RUNNING);
    CHECK_FALSE(session.turn(Direction::UP));
    CHECK(session.simulation().head() == head);

    // The dropped turn does not steer the first tick after the game resumes.
    REQUIRE(session.resume());
    REQUIRE(session.tick() == TickOutcome::MOVED);
    CHECK(session.simulation().heading() == Direction::RIGHT);
}

TEST_CASE("A running session takes turns and ticks the game", "[session]")
{
    Session session = running_session();
    CHECK(session.turn(Direction::UP));
    CHECK_FALSE(session.turn(Direction::UP)); // a repeat is dropped, as in the input buffer
    CHECK(session.tick() == TickOutcome::MOVED);
    CHECK(session.simulation().heading() == Direction::UP);
}

TEST_CASE("A tick that ends the game moves the session to game over", "[session][state]")
{
    Session session = running_session();
    run_into_wall(session);
    REQUIRE(session.state() == GameState::GAME_OVER);
    CHECK_FALSE(session.simulation().running());
    CHECK_FALSE(session.simulation().won());
}

TEST_CASE("After game over nothing changes and no state can be left", "[session][state]")
{
    Session session = running_session();
    run_into_wall(session);
    REQUIRE(session.state() == GameState::GAME_OVER);
    const Cell head = session.simulation().head();
    CHECK(session.tick() == TickOutcome::NOT_RUNNING);
    CHECK_FALSE(session.turn(Direction::DOWN));
    CHECK_FALSE(session.start());
    CHECK_FALSE(session.pause());
    CHECK_FALSE(session.resume());
    CHECK(session.state() == GameState::GAME_OVER);
    CHECK(session.simulation().head() == head);
}

TEST_CASE("Winning the game is also game over", "[session][state]")
{
    // A snake that fills 399 cells of the board and eats the last food.
    std::vector<Cell> path;
    for (std::uint8_t row = 0; row < BOARD_ROWS; ++row)
    {
        for (std::uint8_t column = 0; column < BOARD_COLUMNS; ++column)
        {
            path.push_back(Cell{static_cast<std::uint8_t>(row % 2 == 0 ? column : BOARD_COLUMNS - 1 - column), row});
        }
    }
    path.resize(399);
    const std::vector<Cell> head_first(path.rbegin(), path.rend());
    Session session =
        session_from({.edges = EdgeRule::WALLS, .heading = Direction::LEFT, .snake = {}, .food = {0, 19}, .seed = 1},
                     head_first, GameState::RUNNING);
    REQUIRE(session.tick() == TickOutcome::WON);
    CHECK(session.state() == GameState::GAME_OVER);
    CHECK(session.simulation().won());
}

TEST_CASE("The tick gets shorter as food is eaten and stops at the floor", "[session][speed]")
{
    Session session =
        session_from({.edges = EdgeRule::WALLS, .heading = Direction::RIGHT, .snake = {}, .food = {6, 5}, .seed = 1},
                     {{5, 5}, {4, 5}, {3, 5}}, GameState::RUNNING);
    CHECK(session.tick_interval_ms() == 150);
    REQUIRE(session.tick() == TickOutcome::ATE);
    CHECK(session.tick_interval_ms() == 147);

    Session late = session_from({.edges = EdgeRule::WALLS,
                                 .heading = Direction::RIGHT,
                                 .snake = {},
                                 .food = {6, 5},
                                 .seed = 1,
                                 .scoring = {.foods_eaten = 100}},
                                {{5, 5}, {4, 5}, {3, 5}}, GameState::RUNNING);
    CHECK(late.tick_interval_ms() == 70);
}

TEST_CASE("Bonus food speeds the game up like normal food", "[session][speed]")
{
    Session session = session_from({.edges = EdgeRule::WALLS,
                                    .heading = Direction::RIGHT,
                                    .snake = {},
                                    .food = {15, 15},
                                    .seed = 1,
                                    .bonus = Simulation::Bonus{.cell = {6, 5}}},
                                   {{5, 5}, {4, 5}, {3, 5}}, GameState::RUNNING);
    REQUIRE(session.tick() == TickOutcome::ATE_BONUS);
    CHECK(session.tick_interval_ms() == 147);
}

TEST_CASE("A saved game restores into the state it was saved in", "[session][restore]")
{
    const Session paused =
        session_from({.edges = EdgeRule::WALLS, .heading = Direction::RIGHT, .snake = {}, .food = {6, 5}, .seed = 1},
                     {{5, 5}, {4, 5}, {3, 5}}, GameState::PAUSED);
    CHECK(paused.state() == GameState::PAUSED);
}

TEST_CASE("A session cannot be built from a game that does not fit it", "[session][restore]")
{
    const std::vector<Cell> snake{{5, 5}, {4, 5}, {3, 5}};
    const std::optional<Simulation> walls =
        Simulation::from_setup({.edges = EdgeRule::WALLS, .heading = Direction::RIGHT, .snake = snake, .food = {6, 5}});
    REQUIRE(walls.has_value());
    // The mode's edge rule must match the game's.
    CHECK_FALSE(Session::from_simulation(Mode::EASY, *walls, GameState::PAUSED).has_value());
    // A game that is running is not game over.
    CHECK_FALSE(Session::from_simulation(Mode::CLASSIC, *walls, GameState::GAME_OVER).has_value());

    // A game that has ended is game over, and nothing else.
    Session ended = running_session();
    run_into_wall(ended);
    CHECK_FALSE(Session::from_simulation(Mode::CLASSIC, ended.simulation(), GameState::PAUSED).has_value());
    CHECK(Session::from_simulation(Mode::CLASSIC, ended.simulation(), GameState::GAME_OVER).has_value());
}
