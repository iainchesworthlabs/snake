// SPDX-License-Identifier: GPL-3.0-or-later
#include "device/accelerometer.hpp"
#include "device/haptics.hpp"
#include "viewmodel/game_view_model.hpp"
#include "viewmodel/haptics_director.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using snake::ui::GameViewModel;
using snake::ui::HapticsDirector;
using snake::ui::device::Haptics;
using Event = GameViewModel::Event;

namespace
{

class RecordingHaptics final : public Haptics
{
public:
    void tap() override
    {
        ++taps;
    }
    void thud() override
    {
        ++thuds;
    }

    int taps = 0;
    int thuds = 0;
};

GameViewModel::SeedSource counting_seeds()
{
    return [next = std::uint64_t{1}]() mutable { return next++; };
}

} // namespace

TEST_CASE("Eating taps and a crash knocks", "[haptics]")
{
    RecordingHaptics haptics;
    GameViewModel game(counting_seeds());
    HapticsDirector director(&haptics);
    director.setGame(&game);
    CHECK(director.game() == &game);

    emit game.tickEvent(Event::Ate);
    emit game.tickEvent(Event::AteBonus);
    emit game.tickEvent(Event::Won);
    CHECK(haptics.taps == 3);
    CHECK(haptics.thuds == 0);

    emit game.tickEvent(Event::Crashed);
    CHECK(haptics.taps == 3);
    CHECK(haptics.thuds == 1);
}

TEST_CASE("A game that is let go of is no longer felt", "[haptics]")
{
    RecordingHaptics haptics;
    GameViewModel game(counting_seeds());
    HapticsDirector director(&haptics);
    director.setGame(&game);
    director.setGame(&game);
    director.setGame(nullptr);
    emit game.tickEvent(Event::Ate);
    CHECK(haptics.taps == 0);
    CHECK(director.game() == nullptr);
}

TEST_CASE("A real game that crashes into the wall knocks once", "[haptics]")
{
    RecordingHaptics haptics;
    GameViewModel game(counting_seeds());
    game.setMode(GameViewModel::Mode::Classic);
    HapticsDirector director(&haptics);
    director.setGame(&game);
    game.start();
    for (int i = 0; i < 40 && game.state() == GameViewModel::State::Running; ++i)
    {
        game.advance(0.15);
    }
    REQUIRE(game.state() == GameViewModel::State::GameOver);
    CHECK(haptics.thuds == 1);
}

TEST_CASE("Without haptics nothing happens and nothing breaks", "[haptics]")
{
    GameViewModel game(counting_seeds());
    HapticsDirector director(static_cast<Haptics*>(nullptr));
    director.setGame(&game);
    emit game.tickEvent(Event::Ate);
    emit game.tickEvent(Event::Crashed);
    snake::ui::device::NoHaptics none;
    none.tap();
    none.thud();
    SUCCEED();
}

TEST_CASE("The default haptics are the ones the program set", "[haptics]")
{
    RecordingHaptics haptics;
    CHECK(snake::ui::device::default_haptics() == nullptr);
    snake::ui::device::set_default_haptics(&haptics);
    GameViewModel game(counting_seeds());
    HapticsDirector director;
    snake::ui::device::set_default_haptics(nullptr);
    director.setGame(&game);
    emit game.tickEvent(Event::Ate);
    CHECK(haptics.taps == 1);
}

TEST_CASE("A reading of the accelerometer is turned into g and into the microsecond of the sample", "[accelerometer]")
{
    using snake::ui::device::accel_sample_from;
    using snake::ui::device::STANDARD_GRAVITY;

    // A device lying face up and still feels gravity on z.
    const auto flat = accel_sample_from(0.0, 0.0, STANDARD_GRAVITY, 1'000'000);
    CHECK(flat.x == 0.0F);
    CHECK(flat.y == 0.0F);
    CHECK(flat.z == Catch::Approx(1.0F));
    CHECK(flat.time_us == 1'000'000);

    // Tipped on its right edge: the right edge down makes x negative, as AccelSample says.
    const auto tipped = accel_sample_from(-STANDARD_GRAVITY / 2.0, STANDARD_GRAVITY / 4.0, STANDARD_GRAVITY, 5);
    CHECK(tipped.x == Catch::Approx(-0.5F));
    CHECK(tipped.y == Catch::Approx(0.25F));

    // The time is cut to 32 bits, and differences are what is used, so a wrap does not matter.
    const auto late = accel_sample_from(0, 0, 0, (std::uint64_t{7} << 32U) + 42U);
    CHECK(late.time_us == 42);
}

TEST_CASE("Swallowing a bomb and shedding tap, and a bomb going off knocks", "[haptics][modes]")
{
    RecordingHaptics haptics;
    GameViewModel game(counting_seeds());
    HapticsDirector director(&haptics);
    director.setGame(&game);

    emit game.tickEvent(Event::AtePotato);
    emit game.tickEvent(Event::Shed);
    CHECK(haptics.taps == 2);
    emit game.tickEvent(Event::Teleported); // a portal is not felt
    CHECK(haptics.taps == 2);
    CHECK(haptics.thuds == 0);
    emit game.tickEvent(Event::Exploded);
    CHECK(haptics.thuds == 1);
}
