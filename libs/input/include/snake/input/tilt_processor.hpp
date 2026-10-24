// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snake/input/direction_selector.hpp"
#include "snake/input/sample_source.hpp"
#include "snake/sim/direction.hpp"

#include <optional>

namespace snake::input
{

/// Dead zone 12 degrees, threshold 18 degrees and a 5 degree margin before the other axis takes over: the starting
/// values of the requirements, to be tuned in playtests.
inline constexpr SelectorConfig TILT_SELECTOR = {.release = 12.0F, .engage = 18.0F, .switch_margin = 5.0F};

struct TiltConfig
{
    /// The angles in degrees, see DirectionSelector.
    SelectorConfig selector = TILT_SELECTOR;
    /// The time constant of the low-pass filter on the readings. Zero switches the filter off.
    float filter_seconds = 0.05F;
    /// A sample whose size is outside this range, in g, is not a reading of gravity (free fall, a knock, a bad read)
    /// and is skipped.
    float min_magnitude = 0.5F;
    float max_magnitude = 1.5F;
    /// Flips both axes, for the player who wants the snake to go the other way.
    bool invert_axes = false;
};

/// How far the device is tipped from neutral, and the direction that makes. The angles are the ones that steer: right
/// is positive when the right edge is down, up is positive when the top edge is down, and both change sign with
/// inverted axes. A tilt indicator draws these two numbers against the dead zone.
struct TiltReading
{
    float right_degrees = 0.0F;
    float up_degrees = 0.0F;
    std::optional<sim::Direction> direction;
};

/// Turns accelerometer samples into a steering direction, like a marble on a tray: tip to turn, level out to cruise.
///
/// A low-pass filter smooths the readings. The tilt is measured from the filtered gravity as two angles, one about
/// each screen axis, and the angles are taken relative to the neutral pose set by calibrate(), so the player can hold
/// the device at any comfortable angle between lying flat and standing nearly upright. A DirectionSelector then picks
/// the direction. The direction is the one in force now, not an event: the caller offers it to the input buffer on
/// every frame it is set, and the buffer drops the repeats.
///
/// Held within about six degrees of upright, a device cannot tell a roll to the left or right from no roll, so the roll
/// keeps its last value there.
class TiltProcessor
{
public:
    explicit TiltProcessor(TiltConfig config = {}) noexcept : m_Config(config), m_Selector(config.selector) {}

    /// Feeds one sample and returns the reading after it. A sample that is not a reading of gravity changes nothing.
    TiltReading update(const AccelSample& sample) noexcept;

    /// Feeds every sample the source has now and returns the reading after the last one. Call it once a frame.
    TiltReading drain(SampleSource& source) noexcept;

    /// Makes the current orientation neutral, for the start of a game and for Pause > Recalibrate tilt. Before the
    /// first sample it makes that sample neutral. The reading goes back to level and no direction.
    void calibrate() noexcept;

    void set_invert_axes(bool invert) noexcept
    {
        m_Config.invert_axes = invert;
    }

    [[nodiscard]] const TiltReading& reading() const noexcept
    {
        return m_Reading;
    }

    [[nodiscard]] const TiltConfig& config() const noexcept
    {
        return m_Config;
    }

private:
    void filter(const AccelSample& sample) noexcept;
    void measure() noexcept;
    void publish() noexcept;

    TiltConfig m_Config;
    DirectionSelector m_Selector;

    // The filtered gravity, and the angles measured from it: the roll about the y axis and the pitch of the device
    // above the horizontal.
    float m_X = 0.0F;
    float m_Y = 0.0F;
    float m_Z = 0.0F;
    float m_Roll = 0.0F;
    float m_Pitch = 0.0F;
    std::uint32_t m_LastTimeUs = 0;
    bool m_HasSample = false;

    float m_NeutralRoll = 0.0F;
    float m_NeutralPitch = 0.0F;
    bool m_CalibrationPending = true;

    TiltReading m_Reading;
};

} // namespace snake::input
