// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/sample_source.hpp"

#include <cstdint>
#include <optional>

namespace snake::input
{

/// Which way the top of the screen points, in the device's own axes. A phone held upright, with the screen in portrait,
/// has the top of the screen along the device's +y. Turned a quarter, with the screen turned to match, the top of the
/// screen is along the device's +x or -x; the sensor does not know, and reads in the device's axes whichever way the
/// screen is drawn.
enum class ScreenTop : std::uint8_t
{
    PLUS_Y,
    PLUS_X,
    MINUS_Y,
    MINUS_X,
};

/// A sample in the device's axes, turned into the axes of the screen as it is drawn now: x towards its right edge, y
/// towards its top edge, as AccelSample says. The screen's right edge is a quarter turn clockwise from its top.
[[nodiscard]] constexpr AccelSample to_screen_frame(const AccelSample& sample, ScreenTop top) noexcept
{
    switch (top)
    {
    case ScreenTop::PLUS_X:
        return {.x = -sample.y, .y = sample.x, .z = sample.z, .time_us = sample.time_us};
    case ScreenTop::MINUS_Y:
        return {.x = -sample.x, .y = -sample.y, .z = sample.z, .time_us = sample.time_us};
    case ScreenTop::MINUS_X:
        return {.x = sample.y, .y = -sample.x, .z = sample.z, .time_us = sample.time_us};
    case ScreenTop::PLUS_Y:
        break;
    }
    return sample;
}

/// How the top of the screen lies, from a device reading taken now and whether the screen is drawn in landscape.
///
/// A screen in portrait is taken to have its top along +y: a phone does not draw upside down, and an iPad drawn upside
/// down is not told apart from upright, because a screen that is held still while the device is turned cannot be told
/// from one that turned with it. A screen in landscape follows the pull of gravity, which a phone that turns its screen
/// has just done: the top of the screen is the end of the x axis that points up. A reading that is mostly flat (under
/// 0.3 g along x) says nothing, and the answer stays as it was, or +x when it was not on the x axis.
[[nodiscard]] constexpr ScreenTop choose_screen_top(const AccelSample& latest, bool landscape,
                                                    ScreenTop previous) noexcept
{
    constexpr float MIN_PULL = 0.3F;
    if (!landscape)
    {
        return ScreenTop::PLUS_Y;
    }
    if (latest.x >= MIN_PULL)
    {
        return ScreenTop::PLUS_X;
    }
    if (latest.x <= -MIN_PULL)
    {
        return ScreenTop::MINUS_X;
    }
    return previous == ScreenTop::MINUS_X ? ScreenTop::MINUS_X : ScreenTop::PLUS_X;
}

/// Turns the samples of a source that reads in the device's axes (SampleSource::in_device_frame()) into the axes of the
/// screen, with the quarter turn that latch() last chose. A source that has turned its own samples is passed through
/// as it is. The source is not owned and must outlive this.
class ScreenFrameSource final : public SampleSource
{
public:
    explicit ScreenFrameSource(SampleSource& inner) noexcept : m_Inner(inner) {}

    std::optional<AccelSample> next() noexcept override
    {
        const std::optional<AccelSample> sample = m_Inner.next();
        if (!sample)
        {
            return std::nullopt;
        }
        m_Latest = *sample;
        return m_Inner.in_device_frame() ? std::optional<AccelSample>(to_screen_frame(*sample, m_Top)) : sample;
    }

    /// Takes the way the screen is drawn now: the quarter turn that the latest reading and the screen's shape give.
    void latch(bool landscape) noexcept
    {
        if (m_Inner.in_device_frame())
        {
            m_Top = choose_screen_top(m_Latest, landscape, m_Top);
        }
    }

    [[nodiscard]] ScreenTop top() const noexcept
    {
        return m_Top;
    }

private:
    SampleSource& m_Inner;
    AccelSample m_Latest{};
    ScreenTop m_Top = ScreenTop::PLUS_Y;
};

} // namespace snake::input
