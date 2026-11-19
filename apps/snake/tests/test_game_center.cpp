// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/game_center.hpp"
#include "viewmodel/game_center_director.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using snake::ui::GameCenterDirector;
using snake::ui::GameViewModel;
using snake::ui::device::GameCenter;
using snake::ui::device::NoGameCenter;

namespace
{

class RecordingGameCenter final : public GameCenter
{
public:
    [[nodiscard]] bool available() const override
    {
        return signed_in;
    }
    void submit_score(std::string_view leaderboard_id, std::int64_t score) override
    {
        scores.emplace_back(std::string(leaderboard_id), score);
    }
    void show_leaderboard(std::string_view leaderboard_id) override
    {
        shown.emplace_back(leaderboard_id);
    }
    void sign_in(bool value)
    {
        signed_in = value;
        notify_change();
    }

    bool signed_in = true;
    std::vector<std::pair<std::string, std::int64_t>> scores;
    std::vector<std::string> shown;
};

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

// Plays a game straight on until it hits the wall (Classic).
void play_into_the_wall(GameViewModel& game)
{
    game.start();
    for (int i = 0; i < 20 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
}

} // namespace

TEST_CASE("Each mode has its own leaderboard identifier", "[game-center]")
{
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Easy) ==
          "com.iainchesworthlabs.snake.leaderboard.easy");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Classic) ==
          "com.iainchesworthlabs.snake.leaderboard.classic");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::Portals) ==
          "com.iainchesworthlabs.snake.leaderboard.portals");
    CHECK(GameCenterDirector::leaderboard_id(GameViewModel::Mode::HotPotato) ==
          "com.iainchesworthlabs.snake.leaderboard.hotpotato");
}

TEST_CASE("The score of a game is sent to Game Center when the game ends", "[game-center]")
{
    RecordingGameCenter game_center;
    GameViewModel game(counting_seeds());
    GameCenterDirector director(&game_center);
    director.setGame(&game);
    CHECK(director.game() == &game);

    play_into_the_wall(game);
    REQUIRE(game_center.scores.size() == 1);
    CHECK(game_center.scores.front().first == "com.iainchesworthlabs.snake.leaderboard.classic");
    CHECK(game_center.scores.front().second == game.points());
}

TEST_CASE("Nothing is sent while the player is not signed in", "[game-center]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    GameViewModel game(counting_seeds());
    GameCenterDirector director(&game_center);
    director.setGame(&game);

    play_into_the_wall(game);
    CHECK(game_center.scores.empty());
    director.showLeaderboard(GameViewModel::Mode::Classic);
    CHECK(game_center.shown.empty());
}

TEST_CASE("Without a Game Center nothing is available and nothing happens", "[game-center]")
{
    GameViewModel game(counting_seeds());
    GameCenterDirector none(static_cast<GameCenter*>(nullptr));
    none.setGame(&game);
    CHECK_FALSE(none.available());
    none.showLeaderboard(GameViewModel::Mode::Easy);
    play_into_the_wall(game);

    NoGameCenter no_game_center;
    GameCenterDirector quiet(&no_game_center);
    CHECK_FALSE(quiet.available());
    quiet.showLeaderboard(GameViewModel::Mode::Easy);
}

TEST_CASE("The leaderboard of a mode is shown on request", "[game-center]")
{
    RecordingGameCenter game_center;
    GameCenterDirector director(&game_center);
    director.showLeaderboard(GameViewModel::Mode::Portals);
    REQUIRE(game_center.shown.size() == 1);
    CHECK(game_center.shown.front() == "com.iainchesworthlabs.snake.leaderboard.portals");
}

TEST_CASE("Signing in or out tells the interface", "[game-center]")
{
    RecordingGameCenter game_center;
    game_center.signed_in = false;
    GameCenterDirector director(&game_center);
    QSignalSpy spy(&director, &GameCenterDirector::availableChanged);

    CHECK_FALSE(director.available());
    game_center.sign_in(true);
    CHECK(director.available());
    CHECK(spy.count() == 1);
}

TEST_CASE("A director that has gone leaves the Game Center quiet", "[game-center]")
{
    RecordingGameCenter game_center;
    {
        GameCenterDirector director(&game_center);
    }
    game_center.sign_in(false);
    CHECK_FALSE(game_center.available());
}
