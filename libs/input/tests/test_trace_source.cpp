// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/sample_source.hpp"
#include "snake/input/trace_source.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace snake::input;

TEST_CASE("A trace plays back in order and then runs dry", "[trace]")
{
    const std::array<AccelSample, 3> samples = {AccelSample{.x = 0.1F, .y = 0.2F, .z = 0.9F, .time_us = 0},
                                                AccelSample{.x = 0.2F, .y = 0.3F, .z = 0.8F, .time_us = 10'000},
                                                AccelSample{.x = 0.3F, .y = 0.4F, .z = 0.7F, .time_us = 20'000}};
    TraceSource source(samples);
    CHECK_FALSE(source.finished());
    for (const AccelSample& expected : samples)
    {
        const auto sample = source.next();
        REQUIRE(sample);
        CHECK(sample->x == expected.x);
        CHECK(sample->y == expected.y);
        CHECK(sample->z == expected.z);
        CHECK(sample->time_us == expected.time_us);
    }
    CHECK(source.finished());
    CHECK_FALSE(source.next());
    CHECK_FALSE(source.next());
}

TEST_CASE("A trace can be played again from the start", "[trace]")
{
    const std::array<AccelSample, 2> samples = {AccelSample{.x = 1.0F}, AccelSample{.y = 1.0F}};
    TraceSource source(samples);
    CHECK(source.next());
    CHECK(source.next());
    REQUIRE(source.finished());
    source.rewind();
    CHECK_FALSE(source.finished());
    CHECK(source.next()->x == 1.0F);
}

TEST_CASE("An empty trace is finished at once", "[trace]")
{
    TraceSource source({});
    CHECK(source.finished());
    CHECK_FALSE(source.next());
}

TEST_CASE("A trace can be used through the source interface", "[trace]")
{
    const std::array<AccelSample, 1> samples = {AccelSample{.z = 1.0F}};
    TraceSource trace(samples);
    SampleSource& source = trace;
    CHECK(source.next()->z == 1.0F);
    CHECK_FALSE(source.next());
}
