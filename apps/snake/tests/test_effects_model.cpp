// SPDX-License-Identifier: GPL-3.0-or-later
#include "viewmodel/effects_model.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QPoint>
#include <QSignalSpy>
#include <cstdint>

using snake::ui::EffectsModel;
using snake::ui::GameViewModel;
using Event = GameViewModel::Event;

namespace
{

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

} // namespace

TEST_CASE("The effects model starts still", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    CHECK(model.game() == &game);
    CHECK_FALSE(model.active());
    CHECK_FALSE(model.reducedMotion());
    model.setGame(&game);
}

TEST_CASE("Eating starts a burst at the head, and the model goes active until it dies", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    QSignalSpy active(&model, &EffectsModel::activeChanged);
    QSignalSpy changed(&model, &EffectsModel::changed);
    game.start();

    emit game.tickEvent(Event::Ate);
    CHECK(model.active());
    CHECK(active.count() == 1);
    CHECK(changed.count() >= 1);
    const auto head = game.session().simulation().head();
    REQUIRE_FALSE(model.effects().particles().empty());
    CHECK(model.effects().particles().front().x == head.x + 0.5);
    CHECK(model.effects().particles().size() == 8);

    for (int i = 0; i < 60 && model.active(); ++i)
    {
        model.advance(1.0 / 60.0);
    }
    CHECK_FALSE(model.active());
    CHECK(active.count() == 2);
}

TEST_CASE("The bonus item gives a bigger burst", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    game.start();
    emit game.tickEvent(Event::AteBonus);
    CHECK(model.effects().particles().size() == 14);
}

TEST_CASE("The points of a food are what the score grew by", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    game.start();
    // No score has changed, so the food shows no points; the burst is there.
    emit game.tickEvent(Event::Ate);
    CHECK(model.effects().scores().empty());
    CHECK_FALSE(model.effects().particles().empty());
}

TEST_CASE("Winning is eating", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    game.start();
    emit game.tickEvent(Event::Won);
    CHECK_FALSE(model.effects().particles().empty());
}

TEST_CASE("A crash shakes unless motion is reduced", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    game.start();
    emit game.tickEvent(Event::Crashed);
    CHECK(model.active());
    model.advance(0.01);
    CHECK(model.effects().shake() != QPoint());

    EffectsModel calm;
    calm.setGame(&game);
    calm.setReducedMotion(true);
    emit game.tickEvent(Event::Crashed);
    calm.advance(0.01);
    CHECK(calm.effects().shake() == QPoint());
}

TEST_CASE("Reduced motion can be changed and says so once", "[effects-model]")
{
    EffectsModel model;
    QSignalSpy reduced(&model, &EffectsModel::reducedMotionChanged);
    model.setReducedMotion(true);
    model.setReducedMotion(true);
    CHECK(reduced.count() == 1);
    CHECK(model.reducedMotion());
    model.setReducedMotion(false);
    CHECK(reduced.count() == 2);
}

TEST_CASE("A crash with the game ending leaves the shake going", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    EffectsModel model;
    model.setGame(&game);
    game.start();
    for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
    // The game is over, and the board is still shaking from the crash.
    CHECK(model.active());
}

TEST_CASE("A new game starts with a clean board", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    EffectsModel model;
    model.setGame(&game);
    game.start();
    emit game.tickEvent(Event::Ate);
    REQUIRE(model.active());
    // Restart is in the pause dialog.
    game.pause();
    game.restart();
    CHECK_FALSE(model.active());
    CHECK(model.effects().particles().empty());

    emit game.tickEvent(Event::Ate);
    game.newGame();
    CHECK(model.effects().particles().empty());
}

TEST_CASE("A pause stops the show and the resume goes on with it", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    game.start();
    // Some play first, so that resuming is not the start of a game.
    game.advance(0.5);
    REQUIRE(game.session().simulation().ticks() > 0);
    emit game.tickEvent(Event::Ate);
    REQUIRE(model.active());
    game.pause();
    CHECK_FALSE(model.active());
    CHECK_FALSE(model.effects().particles().empty());
    game.resume();
    CHECK(model.active());
}

TEST_CASE("Letting go of a game stops the model listening to it", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    EffectsModel model;
    model.setGame(&game);
    model.setGame(nullptr);
    CHECK(model.game() == nullptr);
    emit game.tickEvent(Event::Ate);
    CHECK(model.effects().particles().empty());
    CHECK_FALSE(model.active());
}

TEST_CASE("A crash starts the death of the snake, which can be waited out or skipped", "[effects-model]")
{
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    EffectsModel model;
    model.setGame(&game);
    QSignalSpy dying(&model, &EffectsModel::dyingChanged);
    game.start();
    CHECK_FALSE(model.dying());
    for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
    CHECK(model.dying());
    CHECK(dying.count() == 1);
    // The pieces of the snake that died are what the board shows to begin with.
    CHECK(model.effects().death_look().flash);

    model.skipDeath();
    CHECK_FALSE(model.dying());
    CHECK(dying.count() == 2);
    CHECK(model.effects().death_look().popped == game.session().simulation().snake().size() - 1);

    // Waiting does it too.
    game.restart();
    for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(model.dying());
    for (int i = 0; i < 300 && model.dying(); ++i)
    {
        model.advance(0.01);
    }
    CHECK_FALSE(model.dying());
}
