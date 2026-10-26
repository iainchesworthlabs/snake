// SPDX-License-Identifier: GPL-3.0-or-later
#include "snake/input/tilt_processor.hpp"

#include "snake/input/sample_source.hpp"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace snake::input
{

namespace
{

constexpr float DEGREES_PER_RADIAN = 180.0F / std::numbers::pi_v<float>;
constexpr float MICROSECONDS_PER_SECOND = 1.0e6F;

/// Below this much gravity along the screen's x and z axes the device stands on its edge and the roll is not known.
constexpr float MIN_ROLL_PLANE = 0.1F;

/// An angle difference brought into (-180, 180].
float wrap_degrees(float degrees) noexcept
{
    if (degrees > 180.0F)
    {
        return degrees - 360.0F;
    }
    if (degrees <= -180.0F)
    {
        return degrees + 360.0F;
    }
    return degrees;
}

} // namespace

TiltReading TiltProcessor::update(const AccelSample& sample) noexcept
{
    const float size = std::sqrt((sample.x * sample.x) + (sample.y * sample.y) + (sample.z * sample.z));
    // Written so that a size which is not a number fails it.
    if (!(size >= m_Config.min_magnitude && size <= m_Config.max_magnitude))
    {
        return m_Reading;
    }
    filter(sample);
    measure();
    if (m_CalibrationPending)
    {
        calibrate();
    }
    publish();
    return m_Reading;
}

TiltReading TiltProcessor::drain(SampleSource& source) noexcept
{
    while (const std::optional<AccelSample> sample = source.next())
    {
        update(*sample);
    }
    return m_Reading;
}

void TiltProcessor::calibrate() noexcept
{
    if (!m_HasSample)
    {
        m_CalibrationPending = true;
        return;
    }
    m_NeutralRoll = m_Roll;
    m_NeutralPitch = m_Pitch;
    m_CalibrationPending = false;
    m_Selector.reset();
    m_Reading = {};
}

void TiltProcessor::filter(const AccelSample& sample) noexcept
{
    if (!m_HasSample)
    {
        m_X = sample.x;
        m_Y = sample.y;
        m_Z = sample.z;
        m_HasSample = true;
    }
    else
    {
        // The subtraction wraps with the counter, so the step is right across the wrap.
        const std::uint32_t step_us = sample.time_us - m_LastTimeUs;
        const float step = static_cast<float>(step_us) / MICROSECONDS_PER_SECOND;
        const float span = m_Config.filter_seconds + step;
        const float weight = span > 0.0F ? step / span : 1.0F;
        m_X += weight * (sample.x - m_X);
        m_Y += weight * (sample.y - m_Y);
        m_Z += weight * (sample.z - m_Z);
    }
    m_LastTimeUs = sample.time_us;
}

/// The angle of the point (x, y) from the x axis, in radians; the arguments are y then x, as std::atan2 takes them. It
/// is worked in double: the float function is `atan2f`, which glibc 2.43 gave a new symbol version, so a program that
/// called it would not start on any system older than that. The double one has been in glibc for ever. The result is
/// the same to the precision of a float.
static float angle_between(float y, float x) noexcept
{
    return static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(x)));
}

void TiltProcessor::measure() noexcept
{
    const float plane = std::hypot(m_X, m_Z);
    if (plane >= MIN_ROLL_PLANE)
    {
        m_Roll = angle_between(m_X, m_Z) * DEGREES_PER_RADIAN;
    }
    m_Pitch = angle_between(m_Y, plane) * DEGREES_PER_RADIAN;
}

void TiltProcessor::publish() noexcept
{
    // Tipping the right edge down turns the x reading negative, so the roll falls. Tipping the top edge down lowers
    // the pitch.
    float right = wrap_degrees(m_NeutralRoll - m_Roll);
    float up = m_NeutralPitch - m_Pitch;
    if (m_Config.invert_axes)
    {
        right = -right;
        up = -up;
    }
    m_Reading = {.right_degrees = right, .up_degrees = up, .direction = m_Selector.update(right, up)};
}

} // namespace snake::input
