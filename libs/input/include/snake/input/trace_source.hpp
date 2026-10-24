// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/sample_source.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace snake::input
{

/// A SampleSource that plays back a recording, one sample per call and then nothing. It is the fake sensor for tests
/// and for the ESP32-P4 front end before an IMU is wired. The recording is not copied, so it must outlive the source.
///
/// Every function is defined here, so the class has no source file. The library is built without RTTI and its users
/// may have it on; a class with a function in the library would leave them without its type information.
class TraceSource final : public SampleSource
{
public:
    explicit TraceSource(std::span<const AccelSample> trace) noexcept : m_Trace(trace) {}

    std::optional<AccelSample> next() noexcept override
    {
        if (finished())
        {
            return std::nullopt;
        }
        return m_Trace[m_Position++];
    }

    /// Starts again from the first sample.
    void rewind() noexcept
    {
        m_Position = 0;
    }

    /// True once every sample has been returned.
    [[nodiscard]] bool finished() const noexcept
    {
        return m_Position >= m_Trace.size();
    }

private:
    std::span<const AccelSample> m_Trace;
    std::size_t m_Position = 0;
};

} // namespace snake::input
