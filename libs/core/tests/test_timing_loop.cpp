// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/timing_loop.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

TEST_CASE("The frame rate is 60 Hz", "[timing]")
{
    CHECK(snake::TimingLoop::FRAME_RATE == 60);
}

TEST_CASE("One frame lasts 1/60 s", "[timing]")
{
    using namespace std::chrono_literals;
    constexpr auto delta = snake::TimingLoop::max_frame_delta_time();
    CHECK(delta.count() == 1);
    CHECK(std::chrono::duration_cast<std::chrono::microseconds>(delta) == 16666us);
    // Sixty frames fill one second.
    CHECK(std::chrono::duration_cast<std::chrono::seconds>(60 * delta) == 1s);
}

TEST_CASE("The frame interval is whole milliseconds rounded to the nearest", "[timing]")
{
    using namespace std::chrono_literals;
    CHECK(snake::TimingLoop::frame_interval() == 17ms);
}
