// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/audio/tunes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

using namespace snake::audio;

namespace
{

constexpr std::array<Effect, EFFECT_COUNT> EFFECTS = {Effect::EAT,   Effect::BONUS,    Effect::CRASH,  Effect::SELECT,
                                                      Effect::START, Effect::NEW_BEST, Effect::PORTAL, Effect::POTATO,
                                                      Effect::SHED,  Effect::EXPLODE};
constexpr std::array<Tune, TUNE_COUNT> TUNES = {Tune::TITLE, Tune::GAME};

std::uint32_t steps_of(const Channel& channel)
{
    std::uint32_t steps = 0;
    for (const TuneNote& note : channel.notes)
    {
        steps += note.steps;
    }
    return steps;
}

} // namespace

TEST_CASE("Every effect has segments that sound", "[tunes]")
{
    for (const Effect effect : EFFECTS)
    {
        INFO("effect " << static_cast<int>(effect));
        const auto segments = effect_segments(effect);
        REQUIRE_FALSE(segments.empty());
        for (const Segment& segment : segments)
        {
            CHECK(segment.duration_ms > 0);
            CHECK(segment.volume > 0);
            CHECK(segment.from_note > REST);
            CHECK(segment.from_note <= MAX_NOTE);
            CHECK(segment.to_note > REST);
            CHECK(segment.to_note <= MAX_NOTE);
        }
    }
}

TEST_CASE("An effect that is not one of the effects has no segments", "[tunes]")
{
    CHECK(effect_segments(static_cast<Effect>(200)).empty());
}

TEST_CASE("No effect is long enough to be mistaken for music", "[tunes]")
{
    for (const Effect effect : EFFECTS)
    {
        std::uint32_t total = 0;
        for (const Segment& segment : effect_segments(effect))
        {
            total += segment.duration_ms;
        }
        INFO("effect " << static_cast<int>(effect));
        CHECK(total <= 1000);
    }
}

TEST_CASE("The channels of a tune last the same number of steps, so they stay together when it loops", "[tunes]")
{
    for (const Tune tune : TUNES)
    {
        INFO("tune " << static_cast<int>(tune));
        const TuneData data = tune_data(tune);
        REQUIRE(data.step_ms > 0);
        REQUIRE_FALSE(data.channels.empty());
        REQUIRE(data.channels.size() <= MAX_CHANNELS);
        const std::uint32_t steps = steps_of(data.channels[0]);
        CHECK(steps > 0);
        for (const Channel& channel : data.channels)
        {
            CHECK(steps_of(channel) == steps);
            CHECK(channel.volume > 0);
            CHECK_FALSE(channel.notes.empty());
            for (const TuneNote& note : channel.notes)
            {
                CHECK(note.steps > 0);
                CHECK(note.note <= MAX_NOTE);
            }
        }
    }
}

TEST_CASE("A tune that is not one of the tunes is empty", "[tunes]")
{
    const TuneData data = tune_data(static_cast<Tune>(200));
    CHECK(data.channels.empty());
    CHECK(data.step_ms == 0);
}
