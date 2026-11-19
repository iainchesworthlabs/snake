// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/screen_frame.hpp"
#include "snake/input/tilt_processor.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <utility>

using namespace snake::input;
using Catch::Approx;

namespace
{

class QueueSource final : public SampleSource
{
public:
    explicit QueueSource(bool device_frame) : m_DeviceFrame(device_frame) {}

    void push(float x, float y, float z, std::uint32_t time_us = 0)
    {
        m_Queue.push_back({.x = x, .y = y, .z = z, .time_us = time_us});
    }

    std::optional<AccelSample> next() noexcept override
    {
        if (m_Queue.empty())
        {
            return std::nullopt;
        }
        const AccelSample sample = m_Queue.front();
        m_Queue.pop_front();
        return sample;
    }

    [[nodiscard]] bool in_device_frame() const noexcept override
    {
        return m_DeviceFrame;
    }

private:
    std::deque<AccelSample> m_Queue;
    bool m_DeviceFrame;
};

} // namespace

TEST_CASE("A sample is turned into the frame of the screen by the way its top points", "[screen-frame]")
{
    // Tipped so that the right edge of the screen is down a little (x negative) and the top is up (y positive).
    const AccelSample upright{.x = -0.2F, .y = 0.9F, .z = 0.3F, .time_us = 7};
    // The same pose seen by a device turned a quarter anticlockwise, the screen drawn with its top along +x: what the
    // sensor reads is the pose turned with the device.
    const AccelSample quarter{.x = 0.9F, .y = 0.2F, .z = 0.3F, .time_us = 7};
    const AccelSample half{.x = 0.2F, .y = -0.9F, .z = 0.3F, .time_us = 7};
    const AccelSample three_quarters{.x = -0.9F, .y = -0.2F, .z = 0.3F, .time_us = 7};

    for (const auto& [reading, top] : {std::pair{upright, ScreenTop::PLUS_Y}, std::pair{quarter, ScreenTop::PLUS_X},
                                       std::pair{half, ScreenTop::MINUS_Y},
                                       std::pair{three_quarters, ScreenTop::MINUS_X}})
    {
        const AccelSample turned = to_screen_frame(reading, top);
        CHECK(turned.x == Approx(-0.2F));
        CHECK(turned.y == Approx(0.9F));
        CHECK(turned.z == Approx(0.3F));
        CHECK(turned.time_us == 7);
    }
}

TEST_CASE("A screen in portrait has its top along y", "[screen-frame]")
{
    CHECK(choose_screen_top({.x = 0.9F}, false, ScreenTop::MINUS_X) == ScreenTop::PLUS_Y);
    CHECK(choose_screen_top({.y = -0.9F}, false, ScreenTop::PLUS_X) == ScreenTop::PLUS_Y);
}

TEST_CASE("A screen in landscape has its top at the end of x that points up", "[screen-frame]")
{
    CHECK(choose_screen_top({.x = 0.9F, .y = -0.1F}, true, ScreenTop::PLUS_Y) == ScreenTop::PLUS_X);
    CHECK(choose_screen_top({.x = -0.9F, .y = 0.1F}, true, ScreenTop::PLUS_Y) == ScreenTop::MINUS_X);
    CHECK(choose_screen_top({.x = 0.31F}, true, ScreenTop::MINUS_X) == ScreenTop::PLUS_X);
    CHECK(choose_screen_top({.x = -0.31F}, true, ScreenTop::PLUS_X) == ScreenTop::MINUS_X);
}

TEST_CASE("A device held flat says nothing, and the screen's top stays", "[screen-frame]")
{
    CHECK(choose_screen_top({.x = 0.1F, .z = 1.0F}, true, ScreenTop::MINUS_X) == ScreenTop::MINUS_X);
    CHECK(choose_screen_top({.x = -0.1F, .z = 1.0F}, true, ScreenTop::PLUS_X) == ScreenTop::PLUS_X);
    // From portrait there is nothing to keep, so it is +x.
    CHECK(choose_screen_top({.x = 0.0F, .z = 1.0F}, true, ScreenTop::PLUS_Y) == ScreenTop::PLUS_X);
}

TEST_CASE("A source that turned its own samples is passed through", "[screen-frame]")
{
    QueueSource inner(false);
    ScreenFrameSource source(inner);
    inner.push(0.2F, 0.9F, 0.3F);
    source.latch(true);
    const auto sample = source.next();
    REQUIRE(sample);
    CHECK(sample->x == Approx(0.2F));
    CHECK(sample->y == Approx(0.9F));
    CHECK(source.top() == ScreenTop::PLUS_Y);
}

TEST_CASE("A device that turns the screen after the player is calibrated reads the same tilt", "[screen-frame]")
{
    // The player holds the device upright, calibrated, and tips the top edge down: up is positive and the tilt steers.
    QueueSource inner(true);
    ScreenFrameSource source(inner);
    TiltProcessor tilt;
    for (std::uint32_t i = 0; i < 30; ++i)
    {
        inner.push(0.0F, 0.9F, 0.4F, i * 16000);
    }
    tilt.drain(source);
    tilt.calibrate();

    // The player turns the device a quarter anticlockwise. The screen turns with it, and the program is told that it
    // is in landscape, as the sensor's own axes now say that +x points up.
    inner.push(0.9F, 0.0F, 0.4F, 30 * 16000);
    source.next();
    source.latch(true);
    CHECK(source.top() == ScreenTop::PLUS_X);
    // The player calibrates again, holding the device as they like it.
    for (std::uint32_t i = 31; i < 90; ++i)
    {
        inner.push(0.9F, 0.0F, 0.4F, i * 16000);
    }
    tilt.drain(source);
    tilt.calibrate();

    // Tipping the top edge of the screen down is tipping the sensor's +x end down: x loses what it had, z gains.
    for (std::uint32_t i = 90; i < 150; ++i)
    {
        inner.push(0.55F, 0.0F, 0.75F, i * 16000);
    }
    const TiltReading reading = tilt.drain(source);
    CHECK(reading.up_degrees > 18.0F);
    CHECK(std::abs(reading.right_degrees) < 5.0F);
    REQUIRE(reading.direction);
    CHECK(*reading.direction == snake::sim::Direction::UP);
}
