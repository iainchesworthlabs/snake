// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/autoplayer.hpp"
#include "snake/sim/direction.hpp"
#include "snake/sim/mode.hpp"
#include "snake/sim/scoring.hpp"
#include "snake/sim/snapshot.hpp"
#include "snake/sim/tally.hpp"
#include "viewmodel/game_view_model.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QChar>
#include <QSignalSpy>
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
    // A different seed gives a game of its own. The new game goes from the end to running in one change of state.
    CHECK(emitted.state == 3);
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

TEST_CASE("Restarting begins a new game of the same mode at once", "[viewmodel]")
{
    GameViewModel model(counting_seeds(3));
    model.setMode(Mode::Easy);
    model.start();
    model.advance(0.18);
    REQUIRE(ticks_of(model) == 1);
    const auto first_game = snake::sim::state_hash(model.session().simulation());

    Signals emitted(model);
    model.restart();
    CHECK(model.state() == State::Running);
    CHECK(model.mode() == Mode::Easy);
    CHECK(ticks_of(model) == 0);
    CHECK(snake::sim::state_hash(model.session().simulation()) != first_game);
    CHECK(emitted.board == 1);
}

TEST_CASE("The points are the score as a number", "[viewmodel]")
{
    GameViewModel model(counting_seeds(5));
    CHECK(model.points() == 0);
    // Plays the autoplayer until it has eaten, so the score is not zero.
    model.start();
    for (int i = 0; i < 2000 && model.state() == State::Running && model.points() == 0; ++i)
    {
        const auto direction = snake::sim::autoplay(model.session().simulation());
        static_cast<void>(model.handleKey(key_for(direction), false));
        model.advance(0.15);
    }
    REQUIRE(model.points() > 0);
    CHECK(model.score().toInt() == model.points());
}

TEST_CASE("The demo game plays itself, takes seeds of its own and never ends", "[viewmodel][demo]")
{
    std::uint64_t calls = 0;
    GameViewModel model([&calls]() mutable { return ++calls; });
    const std::uint64_t before = calls;
    QSignalSpy autoplay(&model, &GameViewModel::autoplayChanged);
    CHECK_FALSE(model.autoplay());

    model.setAutoplay(true);
    CHECK(model.autoplay());
    CHECK(autoplay.count() == 1);
    // The demo is under way at once, and no seed of a real game was used for it.
    CHECK(model.state() == State::Running);
    CHECK(calls == before);
    model.setAutoplay(true);
    CHECK(autoplay.count() == 1);

    // Five minutes of play: the snake eats, and when a game ends the next begins at once.
    bool ate = false;
    for (int step = 0; step < 6000; ++step)
    {
        model.advance(0.05);
        ate = ate || model.points() > 0;
        REQUIRE(model.state() == State::Running);
    }
    CHECK(ate);
    CHECK(calls == before);

    // Switched off, the games take the seeds of the real thing again.
    model.setAutoplay(false);
    model.newGame();
    CHECK(calls == before + 1);
}

TEST_CASE("The demo is the same every time", "[viewmodel][demo]")
{
    GameViewModel first(counting_seeds(1));
    GameViewModel second(counting_seeds(99));
    for (GameViewModel* model : {&first, &second})
    {
        model->setAutoplay(true);
        for (int step = 0; step < 400; ++step)
        {
            model->advance(0.05);
        }
    }
    CHECK(snake::sim::state_hash(first.session().simulation()) ==
          snake::sim::state_hash(second.session().simulation()));
}

TEST_CASE("The row of the head is told, for the preview to follow", "[viewmodel][demo]")
{
    GameViewModel model(counting_seeds(1));
    CHECK(model.headRow() == model.session().simulation().head().y);
    CHECK(model.headRow() == 10);
}

namespace
{

/// Plays a game of Hot potato, steered by the computer player, until the snake has swallowed a bomb. False if it
/// did not within `limit` ticks. The game takes one tick for each call, because the frame is a little over the
/// interval.
bool play_until_bomb(GameViewModel& game, int limit = 6000)
{
    game.setMode(Mode::HotPotato);
    game.start();
    for (int tick = 0; tick < limit && game.state() == State::Running; ++tick)
    {
        static_cast<void>(game.turn(snake::sim::autoplay(game.session().simulation())));
        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        if (game.hasBomb())
        {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("The four modes are the sim's four and back", "[viewmodel][modes]")
{
    using snake::sim::Mode;
    CHECK(snake::ui::to_sim(GameViewModel::Mode::Easy) == Mode::EASY);
    CHECK(snake::ui::to_sim(GameViewModel::Mode::Classic) == Mode::CLASSIC);
    CHECK(snake::ui::to_sim(GameViewModel::Mode::Portals) == Mode::PORTALS);
    CHECK(snake::ui::to_sim(GameViewModel::Mode::HotPotato) == Mode::HOT_POTATO);
    for (const Mode mode : {Mode::EASY, Mode::CLASSIC, Mode::PORTALS, Mode::HOT_POTATO})
    {
        CHECK(snake::ui::to_sim(snake::ui::from_sim(mode)) == mode);
    }
}

TEST_CASE("Setting the mode starts a game of that mode with its own rules", "[viewmodel][modes]")
{
    GameViewModel game(counting_seeds());
    game.setMode(Mode::Portals);
    CHECK(game.mode() == Mode::Portals);
    CHECK(game.session().simulation().variant() == snake::sim::Variant::PORTALS);
    CHECK(game.session().simulation().portals().has_value());
    game.setMode(Mode::HotPotato);
    CHECK(game.mode() == Mode::HotPotato);
    CHECK(game.session().simulation().variant() == snake::sim::Variant::HOT_POTATO);
    game.setMode(Mode::Classic);
    CHECK(game.session().simulation().variant() == snake::sim::Variant::PLAIN);
}

TEST_CASE("Each tick outcome is the event that the sound and the effects know it by", "[viewmodel][events]")
{
    using snake::sim::TickOutcome;
    using Event = GameViewModel::Event;
    CHECK(GameViewModel::event_of(TickOutcome::ATE_POTATO) == Event::AtePotato);
    CHECK(GameViewModel::event_of(TickOutcome::TELEPORTED) == Event::Teleported);
    CHECK(GameViewModel::event_of(TickOutcome::EXPLODED) == Event::Exploded);
    CHECK(GameViewModel::event_of(TickOutcome::ATE) == Event::Ate);
}

TEST_CASE("A game with no bomb has nothing to shed", "[viewmodel][shed]")
{
    GameViewModel game(counting_seeds());
    game.setMode(Mode::HotPotato);
    CHECK_FALSE(game.hasBomb());
    CHECK_FALSE(game.canShed());
    CHECK(game.fuse() == Catch::Approx(1.0));
    CHECK_FALSE(game.shed()); // not running
    game.start();
    QSignalSpy events(&game, &GameViewModel::tickEvent);
    CHECK_FALSE(game.shed()); // no bomb
    CHECK(events.count() == 0);
}

TEST_CASE("A bomb in the snake can be shed once it has left the head, and the fuse burns down", "[viewmodel][shed]")
{
    bool tried = false;
    for (std::uint64_t first = 1; first <= 20 && !tried; ++first)
    {
        GameViewModel game(counting_seeds(first));
        QSignalSpy events(&game, &GameViewModel::tickEvent);
        if (!play_until_bomb(game))
        {
            continue;
        }
        tried = true;
        // It has just been swallowed: it is in the head, and the fuse is full.
        CHECK(game.hasBomb());
        CHECK_FALSE(game.canShed());
        CHECK(game.fuse() == Catch::Approx(1.0));
        CHECK_FALSE(game.shed());
        CHECK(events.back().at(0).value<GameViewModel::Event>() == GameViewModel::Event::AtePotato);

        // A tick later it is in the second part and can be shed; the fuse is shorter.
        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        REQUIRE(game.state() == State::Running);
        CHECK(game.canShed());
        CHECK(game.fuse() < 1.0);
        const double fuse = game.fuse();
        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        if (game.state() == State::Running)
        {
            CHECK(game.fuse() < fuse);
        }

        const std::size_t length = game.session().simulation().length();
        const std::size_t index = *game.session().simulation().lump_index();
        QSignalSpy boards(&game, &GameViewModel::boardChanged);
        REQUIRE(game.shed());
        CHECK(events.back().at(0).value<GameViewModel::Event>() == GameViewModel::Event::Shed);
        CHECK(boards.count() == 1);
        CHECK_FALSE(game.hasBomb());
        CHECK_FALSE(game.canShed());
        CHECK(game.session().simulation().length() == index);
        CHECK(game.lastShed().size() == length - index);
        CHECK(game.state() == State::Running);
        CHECK_FALSE(game.shed()); // nothing left to shed
    }
    CHECK(tried);
}

TEST_CASE("A bomb that is not shed goes off and ends the game", "[viewmodel][shed]")
{
    bool tried = false;
    for (std::uint64_t first = 1; first <= 20 && !tried; ++first)
    {
        GameViewModel game(counting_seeds(first));
        QSignalSpy events(&game, &GameViewModel::tickEvent);
        if (!play_until_bomb(game))
        {
            continue;
        }
        tried = true;
        for (int tick = 0; tick < 400 && game.state() == State::Running; ++tick)
        {
            static_cast<void>(game.turn(snake::sim::autoplay(game.session().simulation())));
            game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        }
        CHECK(game.state() == State::GameOver);
        bool bang = false;
        for (const QList<QVariant>& event : events)
        {
            bang |= event.at(0).value<GameViewModel::Event>() == GameViewModel::Event::Exploded;
        }
        CHECK(bang);
    }
    CHECK(tried);
}

TEST_CASE("Enter sheds in Hot potato, and does nothing to a running game of another mode", "[viewmodel][shed][keys]")
{
    bool tried = false;
    for (std::uint64_t first = 1; first <= 20 && !tried; ++first)
    {
        GameViewModel game(counting_seeds(first));
        if (!play_until_bomb(game))
        {
            continue;
        }
        tried = true;
        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        REQUIRE(game.canShed());
        CHECK(game.handleKey(Qt::Key_Return, false));
        CHECK_FALSE(game.hasBomb());
        CHECK(game.state() == State::Running);
    }
    CHECK(tried);

    GameViewModel classic(counting_seeds());
    classic.start();
    CHECK(classic.handleKey(Qt::Key_Return, false));
    CHECK(classic.state() == State::Running);
}

namespace
{

/// What the tick events of a game add up to, counted by the test and not by the tally.
struct EventCounts
{
    std::uint16_t ate = 0;
    std::uint16_t bonus = 0;
    std::uint16_t potato = 0;
    std::uint16_t teleported = 0;
    std::uint16_t crashed = 0;
    std::uint16_t exploded = 0;
    std::uint16_t won = 0;
};

EventCounts count_events(const QSignalSpy& events)
{
    using Event = GameViewModel::Event;
    EventCounts counts;
    for (const auto& arguments : events)
    {
        switch (arguments.at(0).value<Event>())
        {
        case Event::Ate:
            ++counts.ate;
            break;
        case Event::AteBonus:
            ++counts.bonus;
            break;
        case Event::AtePotato:
            ++counts.potato;
            break;
        case Event::Teleported:
            ++counts.teleported;
            break;
        case Event::Crashed:
            ++counts.crashed;
            break;
        case Event::Exploded:
            ++counts.exploded;
            break;
        case Event::Won:
            ++counts.won;
            break;
        case Event::Shed:
            break;
        }
    }
    return counts;
}

/// True when two cells are not next to each other.
bool apart(snake::sim::Cell from, snake::sim::Cell to)
{
    const int dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    const int dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    return dx + dy != 1;
}

/// Plays the game in `mode` with the computer player steering, one tick for each call to advance(), until it ends or
/// `limit` ticks have run. Returns whether the head ever moved to a cell that is not next to the one it left.
bool play_with_autoplay(GameViewModel& game, Mode mode, int limit)
{
    game.setMode(mode);
    game.start();
    bool jumped = false;
    for (int tick = 0; tick < limit && game.state() == State::Running; ++tick)
    {
        static_cast<void>(game.turn(snake::sim::autoplay(game.session().simulation())));
        const snake::sim::Cell before = game.session().simulation().head();
        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        jumped = jumped || apart(before, game.session().simulation().head());
    }
    return jumped;
}

} // namespace

TEST_CASE("A new game has an empty tally", "[viewmodel][tally]")
{
    const GameViewModel game(counting_seeds());
    snake::sim::Tally expected;
    expected.start(game.session().simulation().head());
    CHECK(game.tally() == expected);
    CHECK(game.tally().ended_by() == snake::sim::Ending::NONE);
}

TEST_CASE("A game played with given seeds leaves the tally that its events add up to", "[viewmodel][tally]")
{
    using snake::sim::Ending;
    for (const Mode mode : {Mode::Classic, Mode::Easy, Mode::Portals, Mode::HotPotato})
    {
        for (std::uint64_t seed = 1; seed <= 3; ++seed)
        {
            CAPTURE(static_cast<int>(mode), seed);
            GameViewModel game(counting_seeds(seed));
            QSignalSpy events(&game, &GameViewModel::tickEvent);
            const bool jumped = play_with_autoplay(game, mode, 3000);
            const EventCounts counts = count_events(events);
            const snake::sim::Tally& tally = game.tally();
            const snake::sim::Simulation& sim = game.session().simulation();

            CHECK(tally.blueberries() == counts.ate + counts.won);
            CHECK(tally.bonus_items() == counts.bonus);
            CHECK(tally.bombs_eaten() == counts.potato);
            CHECK(tally.portals_used() == counts.teleported);
            CHECK(tally.sheds() == 0);
            CHECK(tally.foods_eaten() == sim.foods_eaten());
            CHECK(tally.foods_eaten() == tally.blueberries() + tally.bonus_items() + tally.bombs_eaten());
            CHECK(tally.top_combo() >= (tally.blueberries() > 0 ? 1 : 0));
            CHECK(tally.top_combo() <= snake::sim::Scoring::MAX_COMBO);
            // Only the edge of a board that wraps lets the head jump, and a portal is the other jump.
            CHECK(tally.wrapped() == (jumped && mode == Mode::Easy && tally.portals_used() == 0));
            if (game.state() == State::GameOver)
            {
                CHECK(tally.ended_by() != Ending::NONE);
                CHECK(counts.crashed + counts.exploded + counts.won == 1);
            }
            else
            {
                CHECK(tally.ended_by() == Ending::NONE);
            }
        }
    }
}

TEST_CASE("The tally of a game of Easy shows that the snake wrapped and of Classic that a wall ended it",
          "[viewmodel][tally]")
{
    using snake::sim::Ending;
    bool wrapped = false;
    for (std::uint64_t seed = 1; seed <= 20 && !wrapped; ++seed)
    {
        GameViewModel game(counting_seeds(seed));
        play_with_autoplay(game, Mode::Easy, 3000);
        wrapped = game.tally().wrapped();
    }
    CHECK(wrapped);

    // A snake that is left to itself in Classic ends in the wall.
    GameViewModel classic(counting_seeds());
    classic.start();
    for (int tick = 0; tick < 100 && classic.state() == State::Running; ++tick)
    {
        classic.advance((classic.session().tick_interval_ms() + 1) / 1000.0);
    }
    REQUIRE(classic.state() == State::GameOver);
    CHECK(classic.tally().ended_by() == Ending::WALL);
    CHECK_FALSE(classic.tally().wrapped());
}

TEST_CASE("A new game of any kind clears the tally", "[viewmodel][tally]")
{
    GameViewModel game(counting_seeds());
    const auto empty = [&game]
    {
        snake::sim::Tally expected;
        expected.start(game.session().simulation().head());
        return expected;
    };
    const auto play = [&game]
    {
        game.start();
        for (int tick = 0; tick < 3000 && game.state() == State::Running; ++tick)
        {
            static_cast<void>(game.turn(snake::sim::autoplay(game.session().simulation())));
            game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
            if (game.tally().blueberries() >= 3)
            {
                break;
            }
        }
        REQUIRE(game.tally().blueberries() >= 3);
    };

    play();
    game.newGame();
    CHECK(game.tally() == empty());

    play();
    game.restart();
    CHECK(game.tally() == empty());
    CHECK(game.state() == State::Running);

    play();
    game.setMode(Mode::Easy);
    CHECK(game.tally() == empty());

    // Setting the mode it already has changes nothing, the game in hand included.
    play();
    const snake::sim::Tally kept = game.tally();
    game.setMode(Mode::Easy);
    CHECK(game.tally() == kept);
}

TEST_CASE("A shed that happens is counted and one that does not is not", "[viewmodel][tally][shed]")
{
    bool tried = false;
    for (std::uint64_t first = 1; first <= 20 && !tried; ++first)
    {
        GameViewModel game(counting_seeds(first));
        if (!play_until_bomb(game))
        {
            continue;
        }
        tried = true;
        CHECK(game.tally().bombs_eaten() == 1);
        CHECK_FALSE(game.shed()); // the bomb is still in the head
        CHECK(game.tally().sheds() == 0);

        game.advance((game.session().tick_interval_ms() + 1) / 1000.0);
        REQUIRE(game.canShed());
        REQUIRE(game.shed());
        CHECK(game.tally().sheds() == 1);
        CHECK_FALSE(game.shed()); // nothing left to shed
        CHECK(game.tally().sheds() == 1);
        CHECK(game.tally().bombs_eaten() == 1);
    }
    CHECK(tried);
}
