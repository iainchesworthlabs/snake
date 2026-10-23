// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/game.hpp"
#include "snake/logger.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>

using snake::Game;
using snake::GameState;

TEST_CASE("A new game is ready and has ticked no frames", "[game]")
{
    const Game game;
    CHECK(game.state() == GameState::READY);
    CHECK(game.frame_count() == 0);
}

TEST_CASE("The state moves along the allowed transitions only", "[game]")
{
    Game game;

    CHECK_FALSE(game.pause());
    CHECK_FALSE(game.resume());
    CHECK_FALSE(game.end());
    CHECK(game.state() == GameState::READY);

    CHECK(game.start());
    CHECK(game.state() == GameState::RUNNING);
    CHECK_FALSE(game.start());
    CHECK_FALSE(game.resume());

    CHECK(game.pause());
    CHECK(game.state() == GameState::PAUSED);
    CHECK_FALSE(game.pause());

    CHECK(game.resume());
    CHECK(game.state() == GameState::RUNNING);

    CHECK(game.end());
    CHECK(game.state() == GameState::GAME_OVER);
    CHECK_FALSE(game.start());
    CHECK_FALSE(game.end());
}

TEST_CASE("A paused game can end", "[game]")
{
    Game game;
    REQUIRE(game.start());
    REQUIRE(game.pause());
    CHECK(game.end());
    CHECK(game.state() == GameState::GAME_OVER);
}

TEST_CASE("Frames are counted only while the game runs", "[game]")
{
    Game game;
    CHECK_FALSE(game.tick());
    CHECK(game.frame_count() == 0);

    REQUIRE(game.start());
    CHECK(game.tick());
    CHECK(game.tick());
    CHECK(game.frame_count() == 2);

    REQUIRE(game.pause());
    CHECK_FALSE(game.tick());
    CHECK(game.frame_count() == 2);

    REQUIRE(game.resume());
    CHECK(game.tick());
    CHECK(game.frame_count() == 3);
}

TEST_CASE("Each frame is logged at trace level, counting from 0", "[game]")
{
    std::ostringstream out;
    const auto logger = std::make_shared<const snake::StreamLogger>(out, snake::LogLevel::TRACE);
    Game game(logger);
    REQUIRE(game.start());

    REQUIRE(game.tick());
    REQUIRE(game.tick());

    CHECK(out.str() == "[TRACE] Frame 0\n[TRACE] Frame 1\n");
}

TEST_CASE("Frames are not logged at the default level", "[game]")
{
    std::ostringstream out;
    const auto logger = std::make_shared<const snake::StreamLogger>(out);
    Game game(logger);
    REQUIRE(game.start());
    REQUIRE(game.tick());
    CHECK(out.str().empty());
}

TEST_CASE("States have names", "[game]")
{
    CHECK(snake::to_string_view(GameState::READY) == "ready");
    CHECK(snake::to_string_view(GameState::RUNNING) == "running");
    CHECK(snake::to_string_view(GameState::PAUSED) == "paused");
    CHECK(snake::to_string_view(GameState::GAME_OVER) == "game over");
}
