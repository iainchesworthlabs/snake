// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/sound_sink.hpp"
#include "snake/audio/tunes.hpp"
#include "snake/sim/simulation.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/profile_view_model.hpp"
#include "viewmodel/sound_director.hpp"
#include "viewmodel/ui_events.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <cstdint>
#include <optional>

using snake::audio::Effect;
using snake::audio::Tune;
using snake::sim::TickOutcome;
using snake::ui::GameViewModel;
using snake::ui::ProfileViewModel;
using snake::ui::SoundDirector;
using snake::ui::SoundSink;
using snake::ui::UiEvents;
using Event = GameViewModel::Event;

namespace
{

/// Writes down what it is asked to play.
class RecordingSink final : public SoundSink
{
public:
    void play(Effect effect) override
    {
        m_Log.push_back("effect " + QString::number(static_cast<int>(effect)));
    }
    void play_music(Tune tune) override
    {
        m_Log.push_back("music " + QString::number(static_cast<int>(tune)));
    }
    void stop_music() override
    {
        m_Log.push_back("stop");
    }
    void set_volume(float gain) override
    {
        m_Volume = gain;
    }

    /// The gain it was last given, or -1 if it has not been given one. Kept out of the log of sounds.
    [[nodiscard]] float volume() const
    {
        return m_Volume;
    }

    /// What has been asked since the last call.
    QStringList take()
    {
        QStringList log;
        log.swap(m_Log);
        return log;
    }

private:
    QStringList m_Log;
    float m_Volume = -1.0F;
};

QString effect(Effect value)
{
    return "effect " + QString::number(static_cast<int>(value));
}

QString music(Tune value)
{
    return "music " + QString::number(static_cast<int>(value));
}

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

/// A game, a profile and a director on a recording sink, the game in Classic where a wall ends it.
struct Rig
{
    RecordingSink sink;
    GameViewModel game{counting_seeds()};
    ProfileViewModel profile;
    SoundDirector director{&sink};

    Rig()
    {
        game.setMode(GameViewModel::Mode::Classic);
        director.setGame(&game);
        director.setProfile(&profile);
    }
};

/// Plays straight on until the wall.
void crash(GameViewModel& game)
{
    for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
}

} // namespace

TEST_CASE("A tick's outcome is an event only when it did more than move", "[sound]")
{
    CHECK(GameViewModel::event_of(TickOutcome::ATE) == Event::Ate);
    CHECK(GameViewModel::event_of(TickOutcome::ATE_BONUS) == Event::AteBonus);
    CHECK(GameViewModel::event_of(TickOutcome::HIT_WALL) == Event::Crashed);
    CHECK(GameViewModel::event_of(TickOutcome::HIT_SELF) == Event::Crashed);
    CHECK(GameViewModel::event_of(TickOutcome::WON) == Event::Won);
    CHECK_FALSE(GameViewModel::event_of(TickOutcome::MOVED).has_value());
    CHECK_FALSE(GameViewModel::event_of(TickOutcome::NOT_RUNNING).has_value());
}

TEST_CASE("A game that runs into the wall sends one crash event before it is over", "[sound]")
{
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    QStringList order;
    QObject::connect(&game, &GameViewModel::tickEvent,
                     [&](Event event) { order.push_back(event == Event::Crashed ? "crash" : "other"); });
    QObject::connect(&game, &GameViewModel::stateChanged, [&] { order.push_back("state"); });
    game.start();
    order.clear();
    crash(game);
    // Some other events may come first if the snake eats on the way; the crash comes once, then the state.
    CHECK(order.count("crash") == 1);
    REQUIRE(order.size() >= 2);
    CHECK(order[order.size() - 2] == "crash");
    CHECK(order.back() == "state");
}

TEST_CASE("Eating sends an event", "[sound]")
{
    // Some game, run straight on, eats on the way: the generator is pinned, so the first seed that does is always the
    // same.
    bool found = false;
    for (std::uint64_t seed = 1; seed < 400 && !found; ++seed)
    {
        GameViewModel game([seed] { return seed; });
        game.setMode(GameViewModel::Mode::Classic);
        QSignalSpy events(&game, &GameViewModel::tickEvent);
        game.start();
        for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
        {
            game.advance(0.15);
        }
        for (const auto& arguments : events)
        {
            if (arguments.at(0).value<Event>() == Event::Ate)
            {
                found = true;
            }
        }
    }
    CHECK(found);
}

TEST_CASE("Each event of the game has its sound", "[sound]")
{
    Rig rig;
    rig.sink.take();
    emit rig.game.tickEvent(Event::Ate);
    CHECK(rig.sink.take() == QStringList{effect(Effect::EAT)});
    emit rig.game.tickEvent(Event::AteBonus);
    CHECK(rig.sink.take() == QStringList{effect(Effect::BONUS)});
    emit rig.game.tickEvent(Event::Crashed);
    CHECK(rig.sink.take() == QStringList{effect(Effect::CRASH)});
    emit rig.game.tickEvent(Event::Won);
    CHECK(rig.sink.take() == QStringList{effect(Effect::NEW_BEST)});
}

TEST_CASE("The title has its tune from the start", "[sound]")
{
    RecordingSink sink;
    GameViewModel game(counting_seeds());
    SoundDirector director(&sink);
    CHECK(director.game() == nullptr);
    CHECK(sink.take().isEmpty());
    director.setGame(&game);
    CHECK(director.game() == &game);
    CHECK(sink.take() == QStringList{music(Tune::TITLE)});
    // The same game again changes nothing.
    director.setGame(&game);
    CHECK(sink.take().isEmpty());
}

TEST_CASE("A game starts with a jingle and its tune, pauses to silence and resumes with the tune", "[sound]")
{
    Rig rig;
    rig.sink.take();

    rig.game.start();
    CHECK(rig.sink.take() == QStringList{effect(Effect::START), music(Tune::GAME)});

    rig.game.pause();
    CHECK(rig.sink.take() == QStringList{"stop"});

    // Resuming is not a new game: no jingle.
    rig.game.resume();
    CHECK(rig.sink.take() == QStringList{music(Tune::GAME)});

    // Back to the title from a pause.
    rig.game.pause();
    rig.sink.take();
    rig.game.newGame();
    CHECK(rig.sink.take() == QStringList{music(Tune::TITLE)});
}

TEST_CASE("A crash ends the music and Play again starts the jingle again", "[sound]")
{
    Rig rig;
    rig.game.start();
    rig.sink.take();
    crash(rig.game);
    const QStringList heard = rig.sink.take();
    CHECK(heard.contains(effect(Effect::CRASH)));
    CHECK(heard.contains("stop"));
    CHECK(heard.indexOf(effect(Effect::CRASH)) < heard.indexOf("stop"));

    rig.game.restart();
    CHECK(rig.sink.take() == QStringList{effect(Effect::START), music(Tune::GAME)});
}

TEST_CASE("A signal that changes nothing does not restart the tune", "[sound]")
{
    Rig rig;
    rig.sink.take();
    emit rig.game.stateChanged();
    emit rig.game.stateChanged();
    CHECK(rig.sink.take().isEmpty());
}

TEST_CASE("A new best has its jingle and a worse game does not", "[sound]")
{
    Rig rig;
    rig.sink.take();
    rig.profile.record_game(snake::sim::Mode::CLASSIC, 300);
    CHECK(rig.sink.take() == QStringList{effect(Effect::NEW_BEST)});

    // The best has to be saved on the board to be beaten.
    for (const QChar letter : QString("MUM"))
    {
        rig.profile.typeCharacter(QString(letter));
    }
    REQUIRE(rig.profile.saveScore());
    rig.sink.take();
    rig.profile.record_game(snake::sim::Mode::CLASSIC, 100);
    CHECK(rig.sink.take().isEmpty());
}

TEST_CASE("With the sound off there are no effects, but the music goes on", "[sound]")
{
    Rig rig;
    rig.profile.setSound(false);
    rig.sink.take();

    emit rig.game.tickEvent(Event::Ate);
    UiEvents::instance().buttonPressed();
    rig.profile.record_game(snake::sim::Mode::CLASSIC, 300);
    rig.game.start();
    CHECK(rig.sink.take() == QStringList{music(Tune::GAME)});

    rig.profile.setSound(true);
    UiEvents::instance().buttonPressed();
    CHECK(rig.sink.take() == QStringList{effect(Effect::SELECT)});
}

TEST_CASE("With the music off there is no tune, and turning it on brings back the one for the moment", "[sound]")
{
    Rig rig;
    rig.sink.take();
    rig.profile.setMusic(false);
    CHECK(rig.sink.take() == QStringList{"stop"});

    // Effects still sound, and no tune starts with the game.
    rig.game.start();
    CHECK(rig.sink.take() == QStringList{effect(Effect::START)});

    rig.profile.setMusic(true);
    CHECK(rig.sink.take() == QStringList{music(Tune::GAME)});

    // Another setting does not restart it.
    rig.profile.setReducedMotion(true);
    CHECK(rig.sink.take().isEmpty());
}

TEST_CASE("A pressed button has its sound", "[sound]")
{
    Rig rig;
    rig.sink.take();
    UiEvents::instance().buttonPressed();
    CHECK(rig.sink.take() == QStringList{effect(Effect::SELECT)});
}

TEST_CASE("A director stops listening to a game or profile it lets go of", "[sound]")
{
    Rig rig;
    rig.director.setGame(nullptr);
    rig.sink.take();
    emit rig.game.tickEvent(Event::Ate);
    CHECK(rig.sink.take().isEmpty());
    CHECK(rig.director.game() == nullptr);

    rig.director.setProfile(nullptr);
    CHECK(rig.director.profile() == nullptr);
    rig.profile.record_game(snake::sim::Mode::CLASSIC, 300);
    CHECK(rig.sink.take().isEmpty());
    // Without a profile, sound and music are on.
    rig.director.setGame(&rig.game);
    emit rig.game.tickEvent(Event::Ate);
    CHECK(rig.sink.take().contains(effect(Effect::EAT)));
    rig.director.setProfile(&rig.profile);
    rig.director.setProfile(&rig.profile);
}

TEST_CASE("A director without a sink is silent and does not mind", "[sound]")
{
    GameViewModel game(counting_seeds());
    ProfileViewModel profile;
    SoundDirector director(static_cast<SoundSink*>(nullptr));
    director.setGame(&game);
    director.setProfile(&profile);
    game.start();
    emit game.tickEvent(Event::Crashed);
    profile.setMusic(false);
    UiEvents::instance().buttonPressed();
    SUCCEED();
}

TEST_CASE("The default sink is the one the program set", "[sound]")
{
    RecordingSink sink;
    CHECK(snake::ui::default_sound_sink() == nullptr);
    snake::ui::set_default_sound_sink(&sink);
    SoundDirector director;
    snake::ui::set_default_sound_sink(nullptr);
    UiEvents::instance().buttonPressed();
    CHECK(sink.take() == QStringList{effect(Effect::SELECT)});
}

TEST_CASE("A program that is put away makes no music and no sound, and gets its tune back", "[sound]")
{
    Rig rig;
    rig.sink.take();
    CHECK(rig.director.active());

    rig.director.setActive(false);
    CHECK_FALSE(rig.director.active());
    CHECK(rig.sink.take() == QStringList{"stop"});
    emit rig.game.tickEvent(Event::Ate);
    UiEvents::instance().buttonPressed();
    CHECK(rig.sink.take().isEmpty());

    rig.director.setActive(false);
    CHECK(rig.sink.take().isEmpty());

    rig.director.setActive(true);
    CHECK(rig.sink.take() == QStringList{music(Tune::TITLE)});
}

TEST_CASE("A game that goes on while the program is away has its own tune when it comes back", "[sound]")
{
    Rig rig;
    rig.game.start();
    rig.director.setActive(false);
    rig.sink.take();
    rig.director.setActive(true);
    CHECK(rig.sink.take() == QStringList{music(Tune::GAME)});
}

TEST_CASE("The sink is told how loud to be when the profile is given and when the volume changes", "[sound][volume]")
{
    Rig rig;
    // The first run is as loud as the synthesiser goes.
    CHECK(rig.sink.volume() == 1.0F);

    rig.profile.setVolume(5);
    CHECK(rig.sink.volume() == Catch::Approx(0.25F)); // half of the steps, squared, so that the steps sound even
    rig.profile.setVolume(0);
    CHECK(rig.sink.volume() == 0.0F);
    rig.profile.setVolume(10);
    CHECK(rig.sink.volume() == 1.0F);
}

TEST_CASE("A volume saved before the director is made reaches the sink at once", "[sound][volume]")
{
    RecordingSink sink;
    ProfileViewModel profile;
    profile.setVolume(2);
    SoundDirector director(&sink);
    director.setProfile(&profile);
    CHECK(sink.volume() == Catch::Approx(0.04F));
}

TEST_CASE("The bomb, the portal, the shed and the explosion each have a sound", "[sound][modes]")
{
    Rig rig;
    rig.sink.take();
    emit rig.game.tickEvent(Event::AtePotato);
    CHECK(rig.sink.take() == QStringList{effect(Effect::POTATO)});
    emit rig.game.tickEvent(Event::Teleported);
    CHECK(rig.sink.take() == QStringList{effect(Effect::PORTAL)});
    emit rig.game.tickEvent(Event::Shed);
    CHECK(rig.sink.take() == QStringList{effect(Effect::SHED)});
    emit rig.game.tickEvent(Event::Exploded);
    CHECK(rig.sink.take() == QStringList{effect(Effect::EXPLODE)});
}
