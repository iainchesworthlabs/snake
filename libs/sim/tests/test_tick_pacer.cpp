// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/mode.hpp"
#include "snake/sim/session.hpp"
#include "snake/sim/tick_pacer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using namespace snake::sim;

namespace
{

/// Counts the ticks due after `elapsed_us` more microseconds, at a fixed interval.
int ticks_due(TickPacer& pacer, std::uint32_t elapsed_us, std::uint16_t interval_ms)
{
    pacer.add(elapsed_us);
    int ticks = 0;
    while (pacer.due(interval_ms))
    {
        ++ticks;
    }
    return ticks;
}

} // namespace

TEST_CASE("No tick is due before the interval has passed", "[pacer]")
{
    TickPacer pacer;
    CHECK(ticks_due(pacer, 149'999, 150) == 0);
    CHECK(pacer.pending_us() == 149'999);
    CHECK(ticks_due(pacer, 1, 150) == 1);
    CHECK(pacer.pending_us() == 0);
}

TEST_CASE("A tick takes the interval out of the time that has passed and keeps the rest", "[pacer]")
{
    TickPacer pacer;
    CHECK(ticks_due(pacer, 160'000, 150) == 1);
    CHECK(pacer.pending_us() == 10'000);
    CHECK(ticks_due(pacer, 140'000, 150) == 1);
    CHECK(pacer.pending_us() == 0);
}

TEST_CASE("Frames of 16.67 ms give the same ticks as the clock would", "[pacer]")
{
    // Ten seconds at 60 frames a second, in the 16 667 microseconds a frame lasts, with a 150 ms interval.
    TickPacer pacer;
    int ticks = 0;
    for (int frame = 0; frame < 600; ++frame)
    {
        ticks += ticks_due(pacer, 16'667, 150);
    }
    // 600 frames last 10 000.2 ms, which holds 66 intervals of 150 ms.
    CHECK(ticks == 66);
}

TEST_CASE("More than one tick can be due after a long frame", "[pacer]")
{
    TickPacer pacer;
    CHECK(ticks_due(pacer, 200'000, 70) == 2);
    CHECK(pacer.pending_us() == 60'000);
}

TEST_CASE("A stall is capped so the game does not race to catch up", "[pacer]")
{
    TickPacer pacer;
    CHECK(ticks_due(pacer, 5'000'000, 70) == 3);
    CHECK(pacer.pending_us() == TickPacer::MAX_PENDING_US - (3 * 70'000));

    TickPacer slow;
    CHECK(ticks_due(slow, 5'000'000, 180) == 1);
}

TEST_CASE("The elapsed time cannot overflow", "[pacer]")
{
    TickPacer pacer;
    pacer.add(4'000'000'000U);
    pacer.add(4'000'000'000U);
    CHECK(pacer.pending_us() == TickPacer::MAX_PENDING_US);
}

TEST_CASE("Each tick takes the interval that applies when it is due", "[pacer]")
{
    TickPacer pacer;
    pacer.add(250'000);
    CHECK(pacer.due(180));
    // 70 ms are left. The game sped up, so the next interval is shorter and that is already enough.
    CHECK_FALSE(pacer.due(120));
    CHECK(pacer.due(70));
    CHECK(pacer.pending_us() == 0);
}

TEST_CASE("Reset forgets the time that has passed", "[pacer]")
{
    TickPacer pacer;
    pacer.add(100'000);
    pacer.reset();
    CHECK(pacer.pending_us() == 0);
    CHECK_FALSE(pacer.due(70));
}

TEST_CASE("A session driven by 60 Hz frames ticks at its own interval", "[pacer][session]")
{
    Session session(Mode::CLASSIC, 5);
    REQUIRE(session.start());
    TickPacer pacer;
    // 150 ms ticks at the start: a second of frames holds six of them.
    for (int frame = 0; frame < 60; ++frame)
    {
        pacer.add(16'667);
        while (session.state() == GameState::RUNNING && pacer.due(session.tick_interval_ms()))
        {
            static_cast<void>(session.tick());
        }
    }
    CHECK(session.simulation().ticks() == 6);
}

TEST_CASE("The loop stops when the game ends", "[pacer][session]")
{
    Session session(Mode::CLASSIC, 5);
    REQUIRE(session.start());
    TickPacer pacer;
    // Straight on into the right wall, then more time than the game needs.
    int guard = 0;
    while (session.state() != GameState::GAME_OVER && guard++ < 100)
    {
        pacer.add(TickPacer::MAX_PENDING_US);
        while (session.state() == GameState::RUNNING && pacer.due(session.tick_interval_ms()))
        {
            static_cast<void>(session.tick());
        }
    }
    CHECK(session.state() == GameState::GAME_OVER);
    const std::uint32_t ticks = session.simulation().ticks();
    pacer.add(TickPacer::MAX_PENDING_US);
    while (session.state() == GameState::RUNNING && pacer.due(session.tick_interval_ms()))
    {
        static_cast<void>(session.tick());
    }
    CHECK(session.simulation().ticks() == ticks);
}
