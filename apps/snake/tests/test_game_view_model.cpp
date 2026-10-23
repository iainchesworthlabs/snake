// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/logger.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <chrono>
#include <memory>
#include <sstream>

using namespace std::chrono_literals;
using snake::ui::GameViewModel;

namespace
{

constexpr auto FAST_TICK = 1ms;

// Runs the event loop until the condition holds or five seconds have passed.
template <typename Condition>
bool wait_for(Condition condition)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (!condition() && elapsed.elapsed() < 5000)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return condition();
}

} // namespace

TEST_CASE("A new view-model is ready and has no frames", "[viewmodel]")
{
    const GameViewModel model;
    CHECK(model.state() == "ready");
    CHECK(model.frame() == 0);
}

TEST_CASE("Starting makes the game run and emits one stateChanged", "[viewmodel]")
{
    GameViewModel model(nullptr, FAST_TICK);
    int state_changes = 0;
    QObject::connect(&model, &GameViewModel::stateChanged, [&state_changes] { ++state_changes; });

    model.start();
    CHECK(model.state() == "running");
    CHECK(state_changes == 1);

    model.start();
    CHECK(state_changes == 1);
}

TEST_CASE("Frames are counted while running, one frameChanged each", "[viewmodel]")
{
    GameViewModel model(nullptr, FAST_TICK);
    int frame_changes = 0;
    QObject::connect(&model, &GameViewModel::frameChanged, [&frame_changes] { ++frame_changes; });

    model.start();
    REQUIRE(wait_for([&model] { return model.frame() >= 3; }));
    model.pause();

    CHECK(frame_changes == model.frame());
}

TEST_CASE("A paused game stops counting and a resumed one continues", "[viewmodel]")
{
    GameViewModel model(nullptr, FAST_TICK);
    model.start();
    REQUIRE(wait_for([&model] { return model.frame() >= 2; }));

    model.pause();
    CHECK(model.state() == "paused");
    const auto frozen = model.frame();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(model.frame() == frozen);

    model.resume();
    CHECK(model.state() == "running");
    CHECK(wait_for([&model, frozen] { return model.frame() > frozen; }));
}

TEST_CASE("Ending stops the game for good", "[viewmodel]")
{
    GameViewModel model(nullptr, FAST_TICK);
    model.start();
    model.end();
    CHECK(model.state() == "game over");
    const auto frozen = model.frame();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(model.frame() == frozen);

    model.start();
    CHECK(model.state() == "game over");
}

TEST_CASE("Each counted frame is logged at trace level", "[viewmodel]")
{
    std::ostringstream out;
    const auto logger = std::make_shared<const snake::StreamLogger>(out, snake::LogLevel::LEVEL_TRACE);
    GameViewModel model(logger, FAST_TICK);
    model.start();
    REQUIRE(wait_for([&model] { return model.frame() >= 2; }));
    model.pause();
    CHECK(out.str().starts_with("[TRACE] Frame 0\n[TRACE] Frame 1\n"));
}
