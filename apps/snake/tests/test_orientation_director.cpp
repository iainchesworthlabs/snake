// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/orientation_director.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using snake::ui::GameViewModel;
using snake::ui::OrientationDirector;

namespace
{

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

} // namespace

TEST_CASE("The screen is held from Play until the player is back at the title", "[orientation]")
{
    std::vector<bool> calls;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    OrientationDirector director([&calls](bool locked) { calls.push_back(locked); });
    director.setActive(true);
    director.setGame(&game);
    CHECK_FALSE(director.locked());
    CHECK(calls.empty());

    game.start();
    CHECK(director.locked());
    REQUIRE(calls.size() == 1);
    CHECK(calls.back());

    // Pausing, restarting and the end of the game go on holding it, and nothing is asked again.
    game.pause();
    game.resume();
    game.restart();
    CHECK(director.locked());
    CHECK(calls.size() == 1);

    game.newGame();
    CHECK_FALSE(director.locked());
    REQUIRE(calls.size() == 2);
    CHECK_FALSE(calls.back());
}

TEST_CASE("A computer's window is never held", "[orientation]")
{
    std::vector<bool> calls;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    OrientationDirector director([&calls](bool locked) { calls.push_back(locked); });
    director.setGame(&game);
    CHECK_FALSE(director.active());
    game.start();
    CHECK_FALSE(director.locked());
    CHECK(calls.empty());
}

TEST_CASE("A game that is on when the device stops being a phone releases the screen", "[orientation]")
{
    std::vector<bool> calls;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    OrientationDirector director([&calls](bool locked) { calls.push_back(locked); });
    director.setActive(true);
    director.setGame(&game);
    game.start();
    REQUIRE(director.locked());
    director.setActive(false);
    CHECK_FALSE(director.locked());
    REQUIRE(calls.size() == 2);
    CHECK_FALSE(calls.back());
}

TEST_CASE("A director that goes away while it holds the screen lets it go", "[orientation]")
{
    std::vector<bool> calls;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    {
        OrientationDirector director([&calls](bool locked) { calls.push_back(locked); });
        director.setActive(true);
        director.setGame(&game);
        game.start();
        REQUIRE(calls.size() == 1);
    }
    REQUIRE(calls.size() == 2);
    CHECK_FALSE(calls.back());
}

TEST_CASE("A game that is let go of no longer holds the screen", "[orientation]")
{
    std::vector<bool> calls;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    OrientationDirector director([&calls](bool locked) { calls.push_back(locked); });
    director.setActive(true);
    director.setGame(&game);
    game.start();
    REQUIRE(director.locked());
    director.setGame(nullptr);
    CHECK_FALSE(director.locked());
    CHECK(director.game() == nullptr);
    CHECK_FALSE(calls.back());
}
