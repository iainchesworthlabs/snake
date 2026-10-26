// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/sample_source.hpp"

#include <cstdint>

namespace snake::ui::device
{

/// The acceleration of gravity, to turn the metres per second squared that Qt Sensors reports into g.
inline constexpr double STANDARD_GRAVITY = 9.80665;

/// A sample from a reading of Qt Sensors' accelerometer: its three axes in metres per second squared, in the frame of
/// the screen (x to the right edge, y to the top edge, z out of the screen), as the force the device feels, so that
/// a device lying face up reads z = +9.8; and the time in microseconds since some start. The time is cut to 32 bits,
/// as AccelSample's is: only the difference between two samples is used.
[[nodiscard]] constexpr input::AccelSample accel_sample_from(double x, double y, double z,
                                                             std::uint64_t timestamp_us) noexcept
{
    return {.x = static_cast<float>(x / STANDARD_GRAVITY),
            .y = static_cast<float>(y / STANDARD_GRAVITY),
            .z = static_cast<float>(z / STANDARD_GRAVITY),
            .time_us = static_cast<std::uint32_t>(timestamp_us & 0xFFFFFFFFU)};
}

} // namespace snake::ui::device
