// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/sample_source.hpp"
#include "snake/input/tilt_processor.hpp"
#include "snake/input/trace_source.hpp"
#include "snake/sim/direction.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <tuple>
#include <vector>

using namespace snake::input;
using Catch::Approx;
using snake::sim::Direction;

namespace
{

constexpr std::uint32_t INTERVAL_US = 10'000; // 100 samples a second
constexpr float HOLD = 55.0F;                 // a comfortable angle to hold a phone, above the horizontal

constexpr float radians(float degrees)
{
    return degrees * std::numbers::pi_v<float> / 180.0F;
}

/// The gravity a device feels when it is held `hold` degrees above the horizontal, then rolled with its right edge
/// down by `right` degrees and its top edge down by `up` degrees. The roll turns the device about its long axis and
/// the pitch about its short one, and the two do not disturb each other.
AccelSample pose(float hold, float right, float up, std::uint32_t time_us)
{
    const float elevation = radians(hold - up);
    const float roll = radians(right);
    return {.x = -std::cos(elevation) * std::sin(roll),
            .y = std::sin(elevation),
            .z = std::cos(elevation) * std::cos(roll),
            .time_us = time_us};
}

/// A recording that the test builds up from stretches of steady holds, 100 samples a second.
class Recording
{
public:
    explicit Recording(float hold = HOLD) : m_Hold(hold) {}

    /// Holds the pose for `seconds`.
    Recording& hold_for(float seconds, float right, float up)
    {
        const auto count = static_cast<std::size_t>(std::lround(seconds * 100.0F));
        for (std::size_t i = 0; i < count; ++i)
        {
            m_Samples.push_back(pose(m_Hold, right, up, m_Time));
            m_Time += INTERVAL_US;
        }
        return *this;
    }

    [[nodiscard]] const std::vector<AccelSample>& samples() const
    {
        return m_Samples;
    }

private:
    float m_Hold;
    std::uint32_t m_Time = 0;
    std::vector<AccelSample> m_Samples;
};

/// Feeds the samples and returns the direction after each one.
std::vector<std::optional<Direction>> directions_after_each(TiltProcessor& tilt,
                                                            const std::vector<AccelSample>& samples)
{
    std::vector<std::optional<Direction>> directions;
    for (const AccelSample& sample : samples)
    {
        directions.push_back(tilt.update(sample).direction);
    }
    return directions;
}

/// The directions in the order they were set, each once: a stretch of the same direction is one entry, and a stretch
/// of nothing is skipped.
std::vector<Direction> runs(const std::vector<std::optional<Direction>>& directions)
{
    std::vector<Direction> result;
    for (const auto& direction : directions)
    {
        if (direction && (result.empty() || result.back() != *direction))
        {
            result.push_back(*direction);
        }
    }
    return result;
}

/// Settles the processor on a pose by holding it for a second.
TiltReading settle(TiltProcessor& tilt, float right, float up, std::uint32_t& time_us, float hold = HOLD)
{
    TiltReading reading;
    for (int i = 0; i < 100; ++i)
    {
        reading = tilt.update(pose(hold, right, up, time_us));
        time_us += INTERVAL_US;
    }
    return reading;
}

} // namespace

TEST_CASE("The first sample is the neutral pose", "[tilt]")
{
    TiltProcessor tilt;
    const TiltReading reading = tilt.update(pose(HOLD, 7.0F, -3.0F, 0));
    CHECK(reading.right_degrees == Approx(0.0F).margin(0.01));
    CHECK(reading.up_degrees == Approx(0.0F).margin(0.01));
    CHECK_FALSE(reading.direction);
}

TEST_CASE("Holding still at any angle gives no direction", "[tilt]")
{
    for (const float hold : {5.0F, 30.0F, 55.0F, 75.0F})
    {
        CAPTURE(hold);
        TiltProcessor tilt;
        const Recording recording = Recording(hold).hold_for(2.0F, 4.0F, -4.0F);
        for (const auto& direction : directions_after_each(tilt, recording.samples()))
        {
            CHECK_FALSE(direction);
        }
    }
}

TEST_CASE("Tipping an edge down steers that way", "[tilt]")
{
    struct Case
    {
        float right;
        float up;
        Direction expected;
    };
    // The top edge down heads up, the bottom edge down heads down, and the left and right edges head that way.
    for (const Case& c : {Case{25.0F, 0.0F, Direction::RIGHT}, Case{-25.0F, 0.0F, Direction::LEFT},
                          Case{0.0F, 25.0F, Direction::UP}, Case{0.0F, -25.0F, Direction::DOWN}})
    {
        CAPTURE(c.right, c.up);
        TiltProcessor tilt;
        std::uint32_t time_us = 0;
        settle(tilt, 0.0F, 0.0F, time_us);
        const TiltReading reading = settle(tilt, c.right, c.up, time_us);
        CHECK(reading.direction == c.expected);
        CHECK(reading.right_degrees == Approx(c.right).margin(0.2));
        CHECK(reading.up_degrees == Approx(c.up).margin(0.2));
    }
}

TEST_CASE("The angles do not depend on how the device is held", "[tilt]")
{
    for (const float hold : {5.0F, 30.0F, 55.0F, 75.0F})
    {
        CAPTURE(hold);
        TiltProcessor tilt;
        std::uint32_t time_us = 0;
        settle(tilt, 0.0F, 0.0F, time_us, hold);
        const TiltReading right = settle(tilt, 22.0F, 0.0F, time_us, hold);
        CHECK(right.direction == Direction::RIGHT);
        CHECK(right.right_degrees == Approx(22.0F).margin(0.2));
        CHECK(right.up_degrees == Approx(0.0F).margin(0.2));
        // The top edge down heads up. The bottom edge down is left out, because from a hold at 75 degrees it would turn
        // the device past upright.
        const TiltReading up = settle(tilt, 0.0F, 22.0F, time_us, hold);
        CHECK(up.direction == Direction::UP);
        CHECK(up.right_degrees == Approx(0.0F).margin(0.2));
        CHECK(up.up_degrees == Approx(22.0F).margin(0.2));
    }
}

TEST_CASE("Calibration makes the pose the player holds neutral", "[tilt][calibration]")
{
    // The device is held rolled 15 degrees and tipped 10 degrees: that is neutral, so nothing steers.
    TiltProcessor tilt;
    const Recording recording = Recording().hold_for(1.0F, 15.0F, 10.0F);
    for (const auto& direction : directions_after_each(tilt, recording.samples()))
    {
        CHECK_FALSE(direction);
    }
    // From there it is 20 degrees more that steers.
    std::uint32_t time_us = 100 * INTERVAL_US;
    CHECK(settle(tilt, 35.0F, 10.0F, time_us).direction == Direction::RIGHT);
}

TEST_CASE("Calibrating again takes the current pose as neutral and clears the direction", "[tilt][calibration]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    REQUIRE(settle(tilt, 25.0F, 0.0F, time_us).direction == Direction::RIGHT);

    tilt.calibrate();
    CHECK_FALSE(tilt.reading().direction);
    CHECK(tilt.reading().right_degrees == 0.0F);
    const TiltReading held = settle(tilt, 25.0F, 0.0F, time_us);
    CHECK_FALSE(held.direction);
    CHECK(held.right_degrees == Approx(0.0F).margin(0.2));
    CHECK(settle(tilt, 45.0F, 0.0F, time_us).direction == Direction::RIGHT);
    CHECK(settle(tilt, 25.0F, 25.0F, time_us).direction == Direction::UP);
}

TEST_CASE("Calibrating before any sample makes the first sample neutral", "[tilt][calibration]")
{
    TiltProcessor tilt;
    tilt.calibrate();
    CHECK_FALSE(tilt.reading().direction);
    const TiltReading reading = tilt.update(pose(HOLD, 30.0F, 0.0F, 0));
    CHECK(reading.right_degrees == Approx(0.0F).margin(0.01));
    CHECK_FALSE(reading.direction);
}

TEST_CASE("Nothing happens inside the dead zone or short of the threshold", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    CHECK_FALSE(settle(tilt, 10.0F, 0.0F, time_us).direction);
    CHECK_FALSE(settle(tilt, 15.0F, 0.0F, time_us).direction);
    CHECK_FALSE(settle(tilt, 17.5F, 0.0F, time_us).direction);
    CHECK(settle(tilt, 18.5F, 0.0F, time_us).direction == Direction::RIGHT);
}

TEST_CASE("A direction holds between the dead zone and the threshold and lets go inside the dead zone", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    REQUIRE(settle(tilt, 22.0F, 0.0F, time_us).direction == Direction::RIGHT);
    CHECK(settle(tilt, 15.0F, 0.0F, time_us).direction == Direction::RIGHT);
    CHECK_FALSE(settle(tilt, 10.0F, 0.0F, time_us).direction);
    // Back at 15 degrees it does not set again; it needs the threshold.
    CHECK_FALSE(settle(tilt, 15.0F, 0.0F, time_us).direction);
}

TEST_CASE("With both axes past the threshold the larger tilt wins", "[tilt]")
{
    for (const auto& [right, up, expected] :
         {std::tuple{28.0F, 21.0F, Direction::RIGHT}, std::tuple{21.0F, 28.0F, Direction::UP},
          std::tuple{-28.0F, -21.0F, Direction::LEFT}, std::tuple{-21.0F, -28.0F, Direction::DOWN}})
    {
        CAPTURE(right, up);
        TiltProcessor tilt;
        std::uint32_t time_us = 0;
        settle(tilt, 0.0F, 0.0F, time_us);
        CHECK(settle(tilt, right, up, time_us).direction == expected);
    }
}

TEST_CASE("On a diagonal the axis in control keeps priority until the other is 5 degrees larger", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    REQUIRE(settle(tilt, 24.0F, 0.0F, time_us).direction == Direction::RIGHT);
    CHECK(settle(tilt, 24.0F, 24.0F, time_us).direction == Direction::RIGHT);
    CHECK(settle(tilt, 24.0F, 28.0F, time_us).direction == Direction::RIGHT);
    CHECK(settle(tilt, 24.0F, 30.0F, time_us).direction == Direction::UP);
}

TEST_CASE("A diagonal hold with a shaky hand does not flicker", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    REQUIRE(settle(tilt, 25.0F, 20.0F, time_us).direction == Direction::RIGHT);

    // Two seconds with each axis wobbling by up to 2 degrees from a fixed pseudo-random sequence.
    std::uint32_t state = 12345;
    const auto jitter = [&state]()
    {
        state = (state * 1664525U) + 1013904223U;
        return (static_cast<float>(state >> 16U) / 65535.0F * 4.0F) - 2.0F;
    };
    std::vector<AccelSample> samples;
    for (int i = 0; i < 200; ++i)
    {
        samples.push_back(pose(HOLD, 25.0F + jitter(), 22.0F + jitter(), time_us));
        time_us += INTERVAL_US;
    }
    for (const auto& direction : directions_after_each(tilt, samples))
    {
        CHECK(direction == Direction::RIGHT);
    }
}

TEST_CASE("Hand shake around the dead zone sets nothing", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);

    std::uint32_t state = 777;
    const auto jitter = [&state]()
    {
        state = (state * 1664525U) + 1013904223U;
        return (static_cast<float>(state >> 16U) / 65535.0F * 10.0F) - 5.0F;
    };
    std::vector<AccelSample> samples;
    for (int i = 0; i < 300; ++i)
    {
        // Up to 5 degrees of shake on top of a steady 9 degrees, so the raw angle reaches 14 but never the threshold.
        samples.push_back(pose(HOLD, 9.0F + jitter(), -9.0F + jitter(), time_us));
        time_us += INTERVAL_US;
    }
    for (const auto& direction : directions_after_each(tilt, samples))
    {
        CHECK_FALSE(direction);
    }
}

TEST_CASE("One jolted sample does not steer", "[tilt][filter]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    // One sample of 40 degrees, as a knock would give, then back to level.
    CHECK_FALSE(tilt.update(pose(HOLD, 40.0F, 0.0F, time_us)).direction);
    time_us += INTERVAL_US;
    CHECK_FALSE(tilt.update(pose(HOLD, 0.0F, 0.0F, time_us)).direction);
}

TEST_CASE("A real tilt steers within a tenth of a second", "[tilt][filter]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    int samples_taken = 0;
    TiltReading reading;
    while (!reading.direction && samples_taken < 50)
    {
        reading = tilt.update(pose(HOLD, 25.0F, 0.0F, time_us));
        time_us += INTERVAL_US;
        ++samples_taken;
    }
    REQUIRE(reading.direction == Direction::RIGHT);
    CHECK(samples_taken <= 10);
}

TEST_CASE("Without the filter a single sample steers", "[tilt][filter]")
{
    TiltConfig config;
    config.filter_seconds = 0.0F;
    TiltProcessor tilt(config);
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    CHECK(tilt.update(pose(HOLD, 25.0F, 0.0F, time_us)).direction == Direction::RIGHT);
}

TEST_CASE("Without the filter two samples with the same time are both taken in full", "[tilt][filter]")
{
    TiltConfig config;
    config.filter_seconds = 0.0F;
    TiltProcessor tilt(config);
    tilt.update(pose(HOLD, 0.0F, 0.0F, 0));
    CHECK(tilt.update(pose(HOLD, 20.0F, 0.0F, 0)).right_degrees == Approx(20.0F).margin(0.01));
}

TEST_CASE("Two samples with the same time count as one", "[tilt][filter]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    const TiltReading before = tilt.update(pose(HOLD, 0.0F, 0.0F, time_us));
    const TiltReading after = tilt.update(pose(HOLD, 30.0F, 0.0F, time_us));
    CHECK(after.right_degrees == Approx(before.right_degrees).margin(0.01));
}

TEST_CASE("The time counter may wrap around", "[tilt][filter]")
{
    const std::uint32_t start = std::numeric_limits<std::uint32_t>::max() - (50 * INTERVAL_US);
    const auto play = [](std::uint32_t first_time)
    {
        TiltProcessor tilt;
        std::uint32_t time_us = first_time;
        settle(tilt, 0.0F, 0.0F, time_us);
        return settle(tilt, 25.0F, 0.0F, time_us);
    };
    const TiltReading plain = play(0);
    const TiltReading wrapped = play(start);
    CHECK(wrapped.direction == Direction::RIGHT);
    CHECK(wrapped.right_degrees == Approx(plain.right_degrees).margin(0.001));
}

TEST_CASE("Inverted axes steer the other way", "[tilt]")
{
    TiltConfig config;
    config.invert_axes = true;
    TiltProcessor tilt(config);
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    const TiltReading right_edge = settle(tilt, 25.0F, 0.0F, time_us);
    CHECK(right_edge.direction == Direction::LEFT);
    CHECK(right_edge.right_degrees == Approx(-25.0F).margin(0.2));
    CHECK(settle(tilt, 0.0F, 25.0F, time_us).direction == Direction::DOWN);
}

TEST_CASE("The axes can be inverted while playing", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    REQUIRE(settle(tilt, 25.0F, 0.0F, time_us).direction == Direction::RIGHT);
    tilt.set_invert_axes(true);
    CHECK(tilt.config().invert_axes);
    CHECK(settle(tilt, 25.0F, 0.0F, time_us).direction == Direction::LEFT);
}

TEST_CASE("A sample that is not gravity is skipped", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    const TiltReading before = settle(tilt, 25.0F, 0.0F, time_us);
    REQUIRE(before.direction == Direction::RIGHT);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const AccelSample bad[] = {{.x = 0.0F, .y = 0.0F, .z = 0.0F, .time_us = time_us}, // free fall
                               {.x = 0.0F, .y = 3.0F, .z = 0.0F, .time_us = time_us}, // a knock
                               {.x = nan, .y = 0.5F, .z = 0.5F, .time_us = time_us},  // a failed read
                               {.x = 0.0F, .y = nan, .z = 0.0F, .time_us = time_us}};
    for (const AccelSample& sample : bad)
    {
        const TiltReading reading = tilt.update(sample);
        CHECK(reading.direction == before.direction);
        CHECK(reading.right_degrees == before.right_degrees);
        CHECK(reading.up_degrees == before.up_degrees);
    }
}

TEST_CASE("A skipped first sample does not start the filter", "[tilt]")
{
    TiltProcessor tilt;
    CHECK_FALSE(tilt.update({.x = 0.0F, .y = 0.0F, .z = 0.0F, .time_us = 0}).direction);
    const TiltReading reading = tilt.update(pose(HOLD, 30.0F, 0.0F, 10'000));
    // This sample is the neutral pose, as if it had been the first.
    CHECK(reading.right_degrees == Approx(0.0F).margin(0.01));
}

TEST_CASE("Standing the device on its edge keeps the roll it had", "[tilt]")
{
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    settle(tilt, 0.0F, 0.0F, time_us);
    // Lift the device to upright, 90 degrees above the horizontal. Gravity then lies along y and the roll is not
    // known, so it is held, while the pitch still shows.
    TiltReading reading;
    for (int i = 0; i < 100; ++i)
    {
        reading = tilt.update({.x = 0.0F, .y = 1.0F, .z = 0.0F, .time_us = time_us});
        time_us += INTERVAL_US;
    }
    CHECK(reading.right_degrees == Approx(0.0F).margin(0.01));
    // Lifting the top edge towards upright makes the bottom edge go down, so this heads down.
    CHECK(reading.up_degrees == Approx(HOLD - 90.0F).margin(0.5));
    CHECK(reading.direction == Direction::DOWN);

    // Calibrated upright, the held roll becomes the neutral roll.
    tilt.calibrate();
    CHECK(tilt.update({.x = 0.0F, .y = 1.0F, .z = 0.0F, .time_us = time_us}).right_degrees ==
          Approx(0.0F).margin(0.01));
}

TEST_CASE("An angle difference that crosses the back of the circle is the short way round", "[tilt]")
{
    // Held face down, the roll is near 180 degrees and may sit on either side of it.
    TiltProcessor tilt;
    std::uint32_t time_us = 0;
    // Neutral roll about +170 degrees.
    const auto face_down = [](float roll_degrees, std::uint32_t t)
    {
        const float roll = radians(roll_degrees);
        return AccelSample{.x = std::sin(roll) * 0.8F, .y = 0.6F, .z = std::cos(roll) * 0.8F, .time_us = t};
    };
    for (int i = 0; i < 100; ++i)
    {
        tilt.update(face_down(170.0F, time_us));
        time_us += INTERVAL_US;
    }
    TiltReading reading;
    for (int i = 0; i < 100; ++i)
    {
        reading = tilt.update(face_down(-170.0F, time_us));
        time_us += INTERVAL_US;
    }
    // From +170 to -170 is 20 degrees further round, not 340 the other way: the roll fell, so the right edge went down.
    CHECK(reading.right_degrees == Approx(-20.0F).margin(0.5));
    // Calibrated there, going from -170 to +170 is also 20 degrees, the other way round.
    tilt.calibrate();
    for (int i = 0; i < 100; ++i)
    {
        reading = tilt.update(face_down(170.0F, time_us));
        time_us += INTERVAL_US;
    }
    CHECK(reading.right_degrees == Approx(20.0F).margin(0.5));
}

TEST_CASE("A recorded game of tilting gives the directions in order", "[tilt][trace]")
{
    const Recording recording = Recording()
                                    .hold_for(0.5F, 0.0F, 0.0F)    // level
                                    .hold_for(0.6F, 25.0F, 0.0F)   // right
                                    .hold_for(0.4F, 0.0F, 0.0F)    // level
                                    .hold_for(0.6F, 0.0F, 25.0F)   // up
                                    .hold_for(0.6F, 26.0F, 24.0F)  // up and right, up stays in control
                                    .hold_for(0.6F, -30.0F, 22.0F) // left, 8 degrees larger than up, takes over
                                    .hold_for(0.4F, 0.0F, 0.0F)    // level
                                    .hold_for(0.6F, 0.0F, -25.0F); // down

    // Draining the source takes the whole trace and ends on the last reading.
    TiltProcessor drained;
    TraceSource source(recording.samples());
    CHECK(drained.drain(source).direction == Direction::DOWN);
    CHECK(source.finished());

    // Feeding the samples one at a time shows the directions in the order they were set.
    TiltProcessor stepped;
    const auto directions = directions_after_each(stepped, recording.samples());
    CHECK(runs(directions) ==
          std::vector<Direction>{Direction::RIGHT, Direction::UP, Direction::LEFT, Direction::DOWN});
}

TEST_CASE("Draining a source with nothing in it changes nothing", "[tilt][trace]")
{
    TiltProcessor tilt;
    const std::vector<AccelSample> empty;
    TraceSource source{empty};
    CHECK_FALSE(tilt.drain(source).direction);
    CHECK(tilt.drain(source).right_degrees == 0.0F);
}

TEST_CASE("The quick sensitivity steers at 12 degrees and the gentle one waits for 24", "[tilt][sensitivity]")
{
    for (const auto& [selector, below, over] :
         {std::tuple{TILT_SELECTOR_QUICK, 10.0F, 14.0F}, std::tuple{TILT_SELECTOR_GENTLE, 20.0F, 26.0F}})
    {
        for (const bool steers : {false, true})
        {
            TiltProcessor tilt;
            tilt.set_selector(selector);
            CHECK(tilt.config().selector.engage == selector.engage);
            const Recording recording =
                Recording().hold_for(0.5F, 0.0F, 0.0F).hold_for(0.5F, 0.0F, steers ? over : below);
            TraceSource source(recording.samples());
            const TiltReading reading = tilt.drain(source);
            CHECK(reading.direction == (steers ? std::optional<Direction>(Direction::UP) : std::nullopt));
        }
    }
}
