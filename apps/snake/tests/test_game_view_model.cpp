// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/snapshot.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QChar>
#include <QString>
#include <Qt>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using snake::sim::Direction;
using snake::ui::GameViewModel;
using State = GameViewModel::State;
using Mode = GameViewModel::Mode;

namespace
{

// The first game uses seed 1, the next 2, and so on, so a test can tell games apart.
GameViewModel::SeedSource counting_seeds(std::uint64_t first = 1)
{
    return [next = first]() mutable { return next++; };
}

constexpr double FRAME = 1.0 / 60.0;

// Plays `frames` frames of 1/60 s.
void run_frames(GameViewModel& model, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        model.advance(FRAME);
    }
}

// Counts the signals a view-model emits.
struct Signals
{
    int state = 0;
    int score = 0;
    int mode = 0;
    int board = 0;
    int progress = 0;

    explicit Signals(GameViewModel& model)
    {
        QObject::connect(&model, &GameViewModel::stateChanged, [this] { ++state; });
        QObject::connect(&model, &GameViewModel::scoreChanged, [this] { ++score; });
        QObject::connect(&model, &GameViewModel::modeChanged, [this] { ++mode; });
        QObject::connect(&model, &GameViewModel::boardChanged, [this] { ++board; });
        QObject::connect(&model, &GameViewModel::progressChanged, [this] { ++progress; });
    }
};

int key_for(Direction direction)
{
    switch (direction)
    {
    case Direction::UP:
        return Qt::Key_Up;
    case Direction::DOWN:
        return Qt::Key_Down;
    case Direction::LEFT:
        return Qt::Key_Left;
    case Direction::RIGHT:
        break;
    }
    return Qt::Key_Right;
}

std::uint32_t ticks_of(const GameViewModel& model)
{
    return model.session().simulation().ticks();
}

} // namespace

TEST_CASE("A new view-model waits to start a game of Classic with no score", "[viewmodel]")
{
    const GameViewModel model(counting_seeds());
    CHECK(model.state() == State::NotStarted);
    CHECK(model.mode() == Mode::Classic);
    CHECK(model.score() == "000000");
    CHECK(ticks_of(model) == 0);
}

TEST_CASE("The default view-model draws its seeds from the system", "[viewmodel]")
{
    const GameViewModel first;
    const GameViewModel second;
    CHECK(first.state() == State::NotStarted);
    // 64 random bits: two games that start the same would mean the seed is not random.
    CHECK(snake::sim::state_hash(first.session().simulation()) !=
          snake::sim::state_hash(second.session().simulation()));
}

TEST_CASE("Starting runs the game and emits one stateChanged", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    model.start();
    CHECK(model.state() == State::Running);
    CHECK(emitted.state == 1);
    CHECK(emitted.board == 0);

    model.start();
    CHECK(emitted.state == 1);
}

TEST_CASE("Time before the start of the game is not counted", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    model.advance(10.0);
    model.start();
    model.advance(0.1);
    CHECK(ticks_of(model) == 0);
}

TEST_CASE("A tick runs when the interval has passed and not before", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    model.start();
    // Classic starts at 150 ms a tick.
    model.advance(0.149);
    CHECK(ticks_of(model) == 0);
    CHECK(emitted.board == 0);
    model.advance(0.001);
    CHECK(ticks_of(model) == 1);
    CHECK(emitted.board == 1);
}

TEST_CASE("Easy ticks slower than Classic", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    model.setMode(Mode::Easy);
    model.start();
    model.advance(0.179);
    CHECK(ticks_of(model) == 0);
    model.advance(0.001);
    CHECK(ticks_of(model) == 1);
}

TEST_CASE("A game follows the clock whatever the frames are", "[viewmodel]")
{
    GameViewModel by_frames(counting_seeds());
    GameViewModel by_halves(counting_seeds());
    by_frames.start();
    by_halves.start();
    run_frames(by_frames, 60);
    for (int i = 0; i < 120; ++i)
    {
        by_halves.advance(FRAME / 2.0);
    }
    CHECK(ticks_of(by_frames) == 6);
    CHECK(snake::sim::state_hash(by_frames.session().simulation()) ==
          snake::sim::state_hash(by_halves.session().simulation()));
}

TEST_CASE("A long stall does not make the game race", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    model.start();
    model.advance(30.0);
    // At most three ticks of a Classic game's first interval fit in the 250 ms the pacer keeps.
    CHECK(ticks_of(model) == 1);
    model.advance(std::numeric_limits<double>::max());
    CHECK(ticks_of(model) <= 3);
}

TEST_CASE("Times that are not times are ignored", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    model.start();
    model.advance(-1.0);
    model.advance(0.0);
    model.advance(std::numeric_limits<double>::quiet_NaN());
    model.advance(0.1);
    CHECK(ticks_of(model) == 0);
    model.advance(0.05);
    CHECK(ticks_of(model) == 1);
}

TEST_CASE("A paused game does not tick, and only the time before the pause counts when it resumes", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    model.start();
    model.advance(0.1);
    model.pause();
    CHECK(model.state() == State::Paused);
    model.advance(5.0);
    CHECK(ticks_of(model) == 0);

    model.resume();
    CHECK(model.state() == State::Running);
    CHECK(emitted.state == 3);
    // The 0.1 s before the pause are kept, and the 5 s of the pause are not counted: 0.05 s more make the interval.
    model.advance(0.04);
    CHECK(ticks_of(model) == 0);
    model.advance(0.01);
    CHECK(ticks_of(model) == 1);
}

TEST_CASE("The progress is how far the game is between ticks", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    CHECK(model.tickProgress() == 0.0);
    model.start();
    CHECK(model.tickProgress() == 0.0);

    // The first interval of Classic is 150 ms.
    model.advance(0.075);
    CHECK(model.tickProgress() == Catch::Approx(0.5));
    CHECK(emitted.progress == 1);
    model.advance(0.1);
    CHECK(ticks_of(model) == 1);
    CHECK(model.tickProgress() == Catch::Approx(0.025 / 0.15));
    CHECK(emitted.progress == 2);

    // The picture stays where it was while the game is paused and carries on from there.
    const double paused_at = model.tickProgress();
    model.pause();
    model.advance(1.0);
    CHECK(model.tickProgress() == paused_at);
    CHECK(emitted.progress == 2);
    model.resume();
    CHECK(model.tickProgress() == paused_at);

    model.newGame();
    CHECK(model.tickProgress() == 0.0);
}

TEST_CASE("Pause and resume do nothing in the wrong state", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    model.pause();
    model.resume();
    CHECK(model.state() == State::NotStarted);
    model.start();
    model.resume();
    CHECK(model.state() == State::Running);
    CHECK(emitted.state == 1);
}

TEST_CASE("An arrow key turns the snake at the next tick", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    model.start();
    CHECK(model.session().simulation().heading() == Direction::RIGHT);
    CHECK(model.handleKey(Qt::Key_Up, false));
    CHECK(model.session().simulation().heading() == Direction::RIGHT);
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::UP);
}

TEST_CASE("W A S D steer", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    model.start();
    CHECK(model.handleKey(Qt::Key_S, false));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::DOWN);
    CHECK(model.handleKey(Qt::Key_A, false));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::LEFT);
    CHECK(model.handleKey(Qt::Key_W, false));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::UP);
    CHECK(model.handleKey(Qt::Key_D, false));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::RIGHT);
}

TEST_CASE("A reversal is dropped", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    model.start();
    CHECK(model.handleKey(Qt::Key_Left, false));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::RIGHT);
}

TEST_CASE("Keys pressed before the start or during a pause do not steer", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    CHECK(model.handleKey(Qt::Key_Up, false));
    model.start();
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::RIGHT);

    model.pause();
    CHECK(model.handleKey(Qt::Key_Down, false));
    model.resume();
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::RIGHT);
}

TEST_CASE("A held key's repeats are used up and do nothing", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    model.start();
    CHECK(model.handleKey(Qt::Key_Up, true));
    model.advance(0.15);
    CHECK(model.session().simulation().heading() == Direction::RIGHT);

    CHECK(model.handleKey(Qt::Key_Space, true));
    CHECK(model.state() == State::Running);
}

TEST_CASE("A key the game does not use is not taken", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    CHECK_FALSE(model.handleKey(Qt::Key_F5, false));
    CHECK_FALSE(model.handleKey(Qt::Key_Q, false));
}

TEST_CASE("Enter starts the game, and Space pauses and resumes it", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    CHECK(model.handleKey(Qt::Key_Space, false));
    CHECK(model.state() == State::NotStarted);

    CHECK(model.handleKey(Qt::Key_Return, false));
    CHECK(model.state() == State::Running);
    CHECK(model.handleKey(Qt::Key_Space, false));
    CHECK(model.state() == State::Paused);
    CHECK(model.handleKey(Qt::Key_Space, false));
    CHECK(model.state() == State::Running);
}

TEST_CASE("Enter resumes a paused game and the keypad Enter starts one", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    CHECK(model.handleKey(Qt::Key_Enter, false));
    CHECK(model.state() == State::Running);
    model.pause();
    CHECK(model.handleKey(Qt::Key_Return, false));
    CHECK(model.state() == State::Running);
    // Enter in a running game does nothing.
    CHECK(model.handleKey(Qt::Key_Return, false));
    CHECK(model.state() == State::Running);
}

TEST_CASE("Escape pauses a running game", "[viewmodel][keys]")
{
    GameViewModel model(counting_seeds());
    CHECK(model.handleKey(Qt::Key_Escape, false));
    CHECK(model.state() == State::NotStarted);
    model.start();
    CHECK(model.handleKey(Qt::Key_Escape, false));
    CHECK(model.state() == State::Paused);
}

TEST_CASE("Running into the wall ends the game, and Enter starts a new one", "[viewmodel]")
{
    GameViewModel model(counting_seeds(7));
    Signals emitted(model);
    model.start();
    // Straight on, the snake meets the right wall within 10 ticks.
    for (int i = 0; i < 20 && model.state() == State::Running; ++i)
    {
        model.advance(0.15);
    }
    REQUIRE(model.state() == State::GameOver);
    CHECK(emitted.state == 2);
    const auto ticks = ticks_of(model);
    model.advance(5.0);
    CHECK(ticks_of(model) == ticks);

    const auto first_game = snake::sim::state_hash(model.session().simulation());
    CHECK(model.handleKey(Qt::Key_Return, false));
    CHECK(model.state() == State::Running);
    CHECK(ticks_of(model) == 0);
    CHECK(snake::sim::state_hash(model.session().simulation()) != first_game);
    // A different seed gives a game of its own.
    CHECK(emitted.state == 4);
}

TEST_CASE("The score is six digits and changes when food is eaten", "[viewmodel]")
{
    GameViewModel model(counting_seeds(3));
    Signals emitted(model);
    model.start();
    // The autoplayer steers by the arrow keys until the first food is eaten.
    for (int tick = 0; tick < 200 && model.session().simulation().score() == 0; ++tick)
    {
        model.handleKey(key_for(snake::sim::autoplay(model.session().simulation())), false);
        model.advance(0.15);
    }
    REQUIRE(model.session().simulation().score() > 0);
    CHECK(emitted.score == 1);
    CHECK(model.score().size() == 6);
    CHECK(model.score() == QString("%1").arg(model.session().simulation().score(), 6, 10, QChar('0')));
}

TEST_CASE("Changing the mode starts a new game in that mode", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    Signals emitted(model);
    model.start();
    model.advance(0.15);
    REQUIRE(ticks_of(model) == 1);

    model.setMode(Mode::Easy);
    CHECK(model.mode() == Mode::Easy);
    CHECK(model.state() == State::NotStarted);
    CHECK(ticks_of(model) == 0);
    CHECK(emitted.mode == 1);
    CHECK(emitted.state == 2);

    model.setMode(Mode::Easy);
    CHECK(emitted.mode == 1);
}

TEST_CASE("A new game keeps the mode and takes a new seed", "[viewmodel]")
{
    GameViewModel model(counting_seeds());
    model.setMode(Mode::Easy);
    const auto first = snake::sim::state_hash(model.session().simulation());
    Signals emitted(model);
    model.newGame();
    CHECK(model.mode() == Mode::Easy);
    CHECK(emitted.board == 1);
    CHECK(emitted.state == 0);
    CHECK(snake::sim::state_hash(model.session().simulation()) != first);
}

TEST_CASE("The same seed and the same inputs play the same game", "[viewmodel]")
{
    const auto play = []
    {
        GameViewModel model(counting_seeds(42));
        model.start();
        for (int i = 0; i < 30; ++i)
        {
            if (i % 5 == 1)
            {
                model.handleKey(Qt::Key_Down, false);
            }
            if (i % 5 == 3)
            {
                model.handleKey(Qt::Key_Right, false);
            }
            model.advance(0.15);
        }
        return snake::sim::state_hash(model.session().simulation());
    };
    CHECK(play() == play());
}
