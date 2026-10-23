// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/sim/edge_rule.hpp"
#include "snake/sim/mode.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace snake::sim;

TEST_CASE("Easy wraps at the edges and Classic has walls", "[mode]")
{
    CHECK(edge_rule_of(Mode::EASY) == EdgeRule::WRAP);
    CHECK(edge_rule_of(Mode::CLASSIC) == EdgeRule::WALLS);
}

TEST_CASE("The speed curves are the ones in the requirements", "[mode][speed]")
{
    CHECK(speed_curve_of(Mode::EASY).start_ms == 180);
    CHECK(speed_curve_of(Mode::EASY).step_ms == 2);
    CHECK(speed_curve_of(Mode::EASY).floor_ms == 120);
    CHECK(speed_curve_of(Mode::CLASSIC).start_ms == 150);
    CHECK(speed_curve_of(Mode::CLASSIC).step_ms == 3);
    CHECK(speed_curve_of(Mode::CLASSIC).floor_ms == 70);
}

TEST_CASE("Easy starts at 180 ms and gets 2 ms faster for each food", "[speed]")
{
    CHECK(tick_interval_ms(Mode::EASY, 0) == 180);
    CHECK(tick_interval_ms(Mode::EASY, 1) == 178);
    CHECK(tick_interval_ms(Mode::EASY, 10) == 160);
    CHECK(tick_interval_ms(Mode::EASY, 29) == 122);
}

TEST_CASE("Easy stops at 120 ms", "[speed]")
{
    CHECK(tick_interval_ms(Mode::EASY, 30) == 120);
    CHECK(tick_interval_ms(Mode::EASY, 31) == 120);
    CHECK(tick_interval_ms(Mode::EASY, 397) == 120);
}

TEST_CASE("Classic starts at 150 ms and gets 3 ms faster for each food", "[speed]")
{
    CHECK(tick_interval_ms(Mode::CLASSIC, 0) == 150);
    CHECK(tick_interval_ms(Mode::CLASSIC, 1) == 147);
    CHECK(tick_interval_ms(Mode::CLASSIC, 10) == 120);
    CHECK(tick_interval_ms(Mode::CLASSIC, 26) == 72);
}

TEST_CASE("Classic stops at 70 ms, not at the 69 ms the steps would give", "[speed]")
{
    CHECK(tick_interval_ms(Mode::CLASSIC, 27) == 70);
    CHECK(tick_interval_ms(Mode::CLASSIC, 28) == 70);
    CHECK(tick_interval_ms(Mode::CLASSIC, 397) == 70);
}

TEST_CASE("The interval never rises as food is eaten and never drops below the floor", "[speed]")
{
    for (const Mode mode : {Mode::EASY, Mode::CLASSIC})
    {
        std::uint16_t previous = tick_interval_ms(mode, 0);
        for (std::size_t foods = 1; foods <= 400; ++foods)
        {
            const std::uint16_t interval = tick_interval_ms(mode, foods);
            CHECK(interval <= previous);
            CHECK(interval >= speed_curve_of(mode).floor_ms);
            previous = interval;
        }
    }
}

TEST_CASE("A curve with no step stays at its start, and a huge food count does not overflow", "[speed]")
{
    CHECK(tick_interval_ms(SpeedCurve{.start_ms = 100, .step_ms = 0, .floor_ms = 50}, 1000) == 100);
    CHECK(tick_interval_ms(Mode::CLASSIC, std::size_t{1} << 40) == 70);
}
