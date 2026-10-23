// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>
#include <cstdint>
#include <ratio>

namespace snake
{

/// The frame rate of the game and the time budget of one frame.
class TimingLoop
{
public:
    static constexpr std::uint32_t FRAME_RATE = 60;

    /// One frame at FRAME_RATE, exact.
    using FrameDuration = std::chrono::duration<std::uint32_t, std::ratio<1, FRAME_RATE>>;

    /// The longest time one frame may take: 1/60 s.
    [[nodiscard]] static constexpr FrameDuration max_frame_delta_time() noexcept
    {
        return FrameDuration{1};
    }

    /// The frame time in whole milliseconds, rounded to the nearest (17 ms at 60 Hz). Timers in Qt
    /// and most operating systems count in milliseconds, so a timer built from this runs slightly
    /// slower than FRAME_RATE.
    [[nodiscard]] static constexpr std::chrono::milliseconds frame_interval() noexcept
    {
        return std::chrono::round<std::chrono::milliseconds>(max_frame_delta_time());
    }
};

} // namespace snake
