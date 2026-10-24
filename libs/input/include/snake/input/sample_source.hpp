// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <optional>

namespace snake::input
{

/// One reading of the accelerometer, in units of g, in the frame of the screen: x towards the right edge, y towards
/// the top edge, z out of the screen towards the player. The numbers are the force the device feels, so a device lying
/// face up and still reads (0, 0, 1) and tipping its right edge down makes x negative. A platform whose sensor uses
/// other axes, other signs or other units converts before it hands a sample over. `time_us` counts microseconds from
/// any starting point and may wrap around; only the difference between two samples is used.
struct AccelSample
{
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    std::uint32_t time_us = 0;
};

/// Where accelerometer samples come from. The phones implement it over the system sensor API, the ESP32-P4 over the
/// MPU-6050 driver, and tests with a TraceSource that plays back a recording, so everything above it is the same
/// code everywhere. No function of it is defined in the library, which is built without RTTI, so a program that uses
/// it with RTTI on still gets its type information.
class SampleSource
{
public:
    SampleSource() = default;
    SampleSource(const SampleSource&) = delete;
    SampleSource& operator=(const SampleSource&) = delete;
    SampleSource(SampleSource&&) = delete;
    SampleSource& operator=(SampleSource&&) = delete;
    virtual ~SampleSource() = default;

    /// The oldest sample not yet returned, or nothing when there is none for now.
    virtual std::optional<AccelSample> next() noexcept = 0;
};

} // namespace snake::input
